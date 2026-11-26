#include "vesc.h"
#include "packet.h"
#include "config.h"

namespace vesc {
namespace {

HardwareSerial& port() { return Serial1; }

Reader   g_rx;
Values   g_val;
uint32_t g_lastValueMs = 0;

// What is sent repeatedly. Only ONE manipulated variable is active at a time --
// sending two setpoints alternately would mean the controller jumps between two
// operating modes.
enum Mode : uint8_t { MODE_OFF, MODE_RPM, MODE_DUTY, MODE_BRAKE };
Mode     g_mode    = MODE_OFF;
int32_t  g_target  = 0;      // ERPM
float    g_duty    = 0.0f;   // −1 … +1
float    g_brake   = 0.0f;   // A
uint32_t g_lastSend = 0;
uint32_t g_lastPoll = 0;

bool     g_fwKnown = false;
uint8_t  g_fwMajor = 0, g_fwMinor = 0;
uint8_t  g_lastFault = 0;
uint32_t g_lastFaultMs = 0;
uint32_t g_faultCount = 0;
bool     g_faultSeen = false;
char     g_hwName[16] = {0};

// Which fields are requested. Bit numbers as in commands.c of the VESC firmware:
//   0 FET temperature · 1 motor temperature · 2 motor current · 3 input current
//   6 duty cycle      · 7 ERPM              · 8 bus voltage
//   15 fault code     · 21 status byte
// That is 22 payload bytes instead of the 74 of a full request.
constexpr uint32_t MASK = (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 3) |
                          (1UL << 6) | (1UL << 7) | (1UL << 8) | (1UL << 13) |
                          (1UL << 15) | (1UL << 21);

// Width of each field in bytes, bit 0 to 21. Needed to skip fields that were not
// requested -- this keeps the parser correct when the mask changes.
const uint8_t FIELD_BYTES[22] = {
  2, 2, 4, 4, 4, 4, 2, 4, 2, 4, 4,
  4, 4, 4, 4, 1, 4, 1, 6, 4, 4, 1
};

// ------------------------------------------------------------------ Sending
void sendPayload(const uint8_t* payload, size_t len) {
  uint8_t out[FRAME_MAX];
  const size_t n = frame(payload, len, out, sizeof(out));
  if (n) port().write(out, n);
}

void sendId(uint8_t id) { sendPayload(&id, 1); }

void sendInt32(uint8_t id, int32_t v) {
  const uint8_t p[5] = {
    id,
    static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16),
    static_cast<uint8_t>(v >> 8),  static_cast<uint8_t>(v)
  };
  sendPayload(p, sizeof(p));
}

void sendPoll() {
  const uint8_t p[5] = {
    COMM_GET_VALUES_SELECTIVE,
    static_cast<uint8_t>(MASK >> 24), static_cast<uint8_t>(MASK >> 16),
    static_cast<uint8_t>(MASK >> 8),  static_cast<uint8_t>(MASK)
  };
  sendPayload(p, sizeof(p));
}

// ------------------------------------------------------------------ Reading
int32_t be32(const uint8_t* p) {
  return (static_cast<int32_t>(p[0]) << 24) | (static_cast<int32_t>(p[1]) << 16) |
         (static_cast<int32_t>(p[2]) << 8)  |  static_cast<int32_t>(p[3]);
}
int16_t be16(const uint8_t* p) {
  return static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void onValues(const uint8_t* d, size_t len) {
  // Payload: id(1) + echoed mask(4) + selected fields.
  if (len < 5) return;
  const uint32_t mask = static_cast<uint32_t>(be32(d + 1));
  size_t i = 5;

  for (uint8_t bit = 0; bit < 22; bit++) {
    if (!(mask & (1UL << bit))) continue;
    const uint8_t w = FIELD_BYTES[bit];
    // Older firmware versions do not know the rear fields and still echo the mask
    // unchanged -- the answer is then shorter than the mask promises. Seen on
    // 2026-11-17 with firmware 5.02: 73 instead of 74 bytes, because the status
    // byte (bit 21) only exists from 5.03.
    //
    // Aborting would be wrong: the fields already read are valid, the checksum
    // matches after all. Bailing out here would mean the UI reports "no
    // controller" while the controller answers cleanly.
    if (i + w > len) break;

    switch (bit) {
      case 0:  g_val.raw.tFet10   = be16(d + i); g_val.tempFet      = g_val.raw.tFet10 / 10.0f;   break;
      case 1:  g_val.raw.tMot10   = be16(d + i); g_val.tempMotor    = g_val.raw.tMot10 / 10.0f;   break;
      case 2:  g_val.raw.iMot100  = be32(d + i); g_val.currentMotor = g_val.raw.iMot100 / 100.0f; break;
      case 3:  g_val.raw.iIn100   = be32(d + i); g_val.currentIn    = g_val.raw.iIn100 / 100.0f;  break;
      case 6:  g_val.raw.duty1000 = be16(d + i); g_val.duty         = g_val.raw.duty1000 / 1000.0f;break;
      case 7:  g_val.erpm         = be32(d + i);          break;
      case 8:  g_val.raw.vIn10    = be16(d + i); g_val.vIn          = g_val.raw.vIn10 / 10.0f;    break;
      case 13: g_val.tacho        = be32(d + i);          break;
      case 15: g_val.fault        = d[i];                 break;
      case 21: g_val.status       = d[i];                 break;
      default: break;                                     // skipped
    }
    i += w;
  }

  g_val.seen = true;
  g_val.count++;
  g_val.rxUs = micros();
  g_lastValueMs = millis();

  // Hold on to the fault code while it is new (see vesc.h).
  if (g_val.fault != 0 && (!g_faultSeen || g_val.fault != g_lastFault
                           || millis() - g_lastFaultMs > 2000)) {
    g_lastFault = g_val.fault;
    g_lastFaultMs = millis();
    g_faultSeen = true;
    g_faultCount++;
    Serial.print("# VESC FAULT "); Serial.print(g_val.fault);
    Serial.print(": "); Serial.print(faultText(g_val.fault));
    Serial.print(" | "); Serial.print(g_val.vIn, 1); Serial.print(" V, ");
    Serial.print(g_val.currentMotor, 1); Serial.print(" A, ");
    Serial.print(g_val.erpm); Serial.print(" ERPM, Duty ");
    Serial.println(g_val.duty, 3);
  } else if (g_val.fault != 0) {
    g_lastFaultMs = millis();
  }
}


void onFwVersion(const uint8_t* d, size_t len) {
  if (len < 3) return;
  g_fwMajor = d[1];
  g_fwMinor = d[2];
  // Followed by the hardware name as a null-terminated string.
  size_t i = 3, k = 0;
  while (i < len && d[i] != 0 && k < sizeof(g_hwName) - 1) g_hwName[k++] = d[i++];
  g_hwName[k] = '\0';
  g_fwKnown = true;
  Serial.print("# VESC: Firmware "); Serial.print(g_fwMajor);
  Serial.print('.'); Serial.print(g_fwMinor);
  Serial.print("  Hardware \""); Serial.print(g_hwName); Serial.println("\"");
}

// Pass-through: text answers of the terminal line by line, everything else as hex.
void onPrint(const uint8_t* d, size_t len) {
  Serial.print("# VESC> ");
  for (size_t i = 1; i < len; i++) {
    const char c = static_cast<char>(d[i]);
    if (c == '\n') { Serial.println(); Serial.print("# VESC> "); }
    else if (c != '\r' && c != '\0') Serial.print(c);
  }
  Serial.println();
}

void onOther(const uint8_t* d, size_t len) {
  static const char HEXCHARS[] = "0123456789abcdef";
  Serial.print("# V ");
  for (size_t i = 0; i < len; i++) {
    Serial.print(HEXCHARS[d[i] >> 4]); Serial.print(HEXCHARS[d[i] & 15]);
  }
  Serial.println();
}

void dispatch(const uint8_t* d, size_t len) {
  if (len == 0) return;
  switch (d[0]) {
    case COMM_GET_VALUES_SELECTIVE: onValues(d, len);    break;
    case COMM_FW_VERSION:           onFwVersion(d, len); break;
    case COMM_PRINT:                onPrint(d, len);     break;
    default:                        onOther(d, len);     break;   // pass-through
  }
}

}  // namespace

// ------------------------------------------------------------------ Interface

void begin() {
  // A configuration (COMM_GET_MCCONF) is just under 500 bytes; the default buffer
  // of 256 bytes would overflow before the main loop reads again.
  port().setRxBufferSize(2048);
  port().begin(VESC_BAUD, SERIAL_8N1, PIN_DRIVE_RX, PIN_DRIVE_TX);
  g_rx.reset();
  sendId(COMM_FW_VERSION);   // shortest meaningful transaction: checks framing,
                             // checksum and wiring in one go
}

void service() {
  // Read everything that is there. Non-blocking -- the web server runs alongside.
  while (port().available()) {
    if (g_rx.feed(static_cast<uint8_t>(port().read()))) {
      dispatch(g_rx.payload(), g_rx.payloadLen());
    }
  }

  const uint32_t now = millis();

  // Repeat the setpoint while driving. Only that keeps the controller's timeout
  // open; value requests do not.
  if (g_mode != MODE_OFF && now - g_lastSend >= VESC_ALIVE_MS) {
    g_lastSend = now;
    switch (g_mode) {
      case MODE_RPM:   sendInt32(COMM_SET_RPM, g_target); break;
      case MODE_DUTY:  sendInt32(COMM_SET_DUTY,
                                 static_cast<int32_t>(g_duty * 100000.0f)); break;
      case MODE_BRAKE: sendInt32(COMM_SET_CURRENT_BRAKE,
                                 static_cast<int32_t>(g_brake * 1000.0f)); break;
      default: break;
    }
  }

  if (now - g_lastPoll >= VESC_POLL_MS) {
    g_lastPoll = now;
    sendPoll();
    if (!g_fwKnown) sendId(COMM_FW_VERSION);   // until it has answered
  }

  g_val.ageMs = g_val.seen ? (now - g_lastValueMs) : 0;
}

const Values& values() { return g_val; }
uint8_t  lastFault()      { return g_lastFault; }
uint32_t lastFaultAgeMs() { return g_faultSeen ? millis() - g_lastFaultMs : 0xFFFFFFFFu; }
uint32_t faultCount()     { return g_faultCount; }

void terminal(const char* text) {
  uint8_t p[PAYLOAD_MAX];
  size_t n = 0;
  p[n++] = COMM_TERMINAL_CMD;
  while (*text && n < sizeof(p)) p[n++] = static_cast<uint8_t>(*text++);
  sendPayload(p, n);
}

void raw(const uint8_t* payload, size_t len) {
  if (len == 0 || len > PAYLOAD_MAX) return;
  sendPayload(payload, len);
}

bool connected() { return g_val.seen && (millis() - g_lastValueMs) < 1000; }

void setErpm(int32_t erpm) {
  if (erpm >  VESC_ERPM_MAX) erpm =  VESC_ERPM_MAX;
  if (erpm < -VESC_ERPM_MAX) erpm = -VESC_ERPM_MAX;
  g_target = erpm;
  if (erpm == 0) { release(); return; }
  g_mode = MODE_RPM;
  g_lastSend = millis();
  sendInt32(COMM_SET_RPM, g_target);
}

int32_t targetErpm() { return g_target; }
bool braking() { return g_mode == MODE_BRAKE; }
bool driving() { return g_mode == MODE_RPM || g_mode == MODE_DUTY; }

void setDuty(float duty) {
  if (duty >  0.95f) duty =  0.95f;
  if (duty < -0.95f) duty = -0.95f;
  g_duty = duty;
  if (duty == 0.0f) { release(); return; }
  g_mode = MODE_DUTY;
  g_lastSend = millis();
  sendInt32(COMM_SET_DUTY, static_cast<int32_t>(g_duty * 100000.0f));
}

float targetDuty() { return g_mode == MODE_DUTY ? g_duty : 0.0f; }

void setBrakeCurrent(float amps) {
  if (amps < 0.0f) amps = -amps;
  g_brake = amps;
  g_mode  = MODE_BRAKE;
  g_duty  = 0.0f;
  g_target = 0;
  g_lastSend = millis();
  sendInt32(COMM_SET_CURRENT_BRAKE, static_cast<int32_t>(g_brake * 1000.0f));
}

void release() {
  g_mode    = MODE_OFF;
  g_target  = 0;
  g_duty    = 0.0f;
  g_brake   = 0.0f;
  // Zero current means for the controller: disable the bridge, the machine coasts freely.
  // NOT duty cycle zero -- in BLDC mode that would be short-circuit braking, and
  // in FOC mode a controlled operating point that regenerates
  // (docs/07-safety.md §3.1).
  sendInt32(COMM_SET_CURRENT, 0);
}

bool fwKnown()   { return g_fwKnown; }
uint8_t fwMajor(){ return g_fwMajor; }
uint8_t fwMinor(){ return g_fwMinor; }
const char* hwName() { return g_hwName; }

const char* faultText(uint8_t code) {
  switch (code) {
    case 0:  return "no fault";
    case 1:  return "Overvoltage";
    case 2:  return "Undervoltage";
    case 3:  return "Driver stage (DRV)";
    case 4:  return "Overcurrent";
    case 5:  return "FET overtemperature";
    case 6:  return "Motor overtemperature";
    case 7:  return "Gate driver supply high";
    case 8:  return "Gate driver supply low";
    case 9:  return "MCU undervoltage";
    case 10: return "Watchdog reset";
    case 11: return "Encoder SPI";
    case 12: return "Encoder amplitude low";
    case 13: return "Encoder amplitude high";
    case 14: return "Flash corrupted";
    case 15: case 16: case 17: return "Current sensor offset";
    case 18: return "Unbalanced phases";
    case 19: return "Shutdown input (BRK)";
    // Everything above exists only in newer firmware versions. Better to honestly
    // report "unknown" than to map it to a wrong name.
    default: return "unknown";
  }
}

}  // namespace vesc
