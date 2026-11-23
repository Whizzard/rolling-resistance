#include "vesc.h"
#include "packet.h"
#include "config.h"

namespace vesc {
namespace {

HardwareSerial& port() { return Serial1; }

Reader   g_rx;
Values   g_val;
uint32_t g_lastValueMs = 0;

// Was wiederholt gesendet wird. Nur EINE Stellgroesse ist gleichzeitig aktiv —
// zwei Sollwerte abwechselnd zu senden hiesse, dass der Regler zwischen zwei
// Betriebsarten springt.
enum Mode : uint8_t { MODE_OFF, MODE_RPM, MODE_DUTY, MODE_BRAKE };
Mode     g_mode    = MODE_OFF;
int32_t  g_target  = 0;      // ERPM
float    g_duty    = 0.0f;   // −1 … +1
float    g_brake   = 0.0f;   // A
uint32_t g_lastSend = 0;
uint32_t g_lastPoll = 0;

bool     g_fwKnown = false;
uint8_t  g_fwMajor = 0, g_fwMinor = 0;
char     g_hwName[16] = {0};

// Welche Felder abgefragt werden. Bitnummern wie in commands.c der VESC-Firmware:
//   0 FET-Temperatur · 1 Motortemperatur · 2 Motorstrom · 3 Eingangsstrom
//   6 Aussteuerung   · 7 ERPM            · 8 Busspannung
//   15 Fehlercode    · 21 Statusbyte
// Das sind 22 Nutzbytes statt der 74 einer vollen Abfrage.
constexpr uint32_t MASK = (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 3) |
                          (1UL << 6) | (1UL << 7) | (1UL << 8) |
                          (1UL << 15) | (1UL << 21);

// Breite jedes Feldes in Bytes, Bit 0 bis 21. Wird gebraucht, um nicht abgefragte
// Felder zu überspringen — so bleibt der Parser richtig, wenn die Maske sich ändert.
const uint8_t FIELD_BYTES[22] = {
  2, 2, 4, 4, 4, 4, 2, 4, 2, 4, 4,
  4, 4, 4, 4, 1, 4, 1, 6, 4, 4, 1
};

// ------------------------------------------------------------------ Senden
void sendPayload(const uint8_t* payload, size_t len) {
  uint8_t out[32];
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

// ------------------------------------------------------------------ Lesen
int32_t be32(const uint8_t* p) {
  return (static_cast<int32_t>(p[0]) << 24) | (static_cast<int32_t>(p[1]) << 16) |
         (static_cast<int32_t>(p[2]) << 8)  |  static_cast<int32_t>(p[3]);
}
int16_t be16(const uint8_t* p) {
  return static_cast<int16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

void onValues(const uint8_t* d, size_t len) {
  // Nutzlast: Kennung(1) + gespiegelte Maske(4) + ausgewählte Felder.
  if (len < 5) return;
  const uint32_t mask = static_cast<uint32_t>(be32(d + 1));
  size_t i = 5;

  for (uint8_t bit = 0; bit < 22; bit++) {
    if (!(mask & (1UL << bit))) continue;
    const uint8_t w = FIELD_BYTES[bit];
    // Aelteere Firmwarestaende kennen die hinteren Felder nicht und spiegeln die
    // Maske trotzdem unveraendert zurueck — die Antwort ist dann kuerzer als die
    // Maske verspricht. Am 2026-11-17 an Firmware 5.02 gesehen: 73 statt 74 Byte,
    // weil das Statusbyte (Bit 21) erst ab 5.03 existiert.
    //
    // Abbrechen waere falsch: die bereits gelesenen Felder sind gueltig, die
    // Pruefsumme stimmt ja. Hier auszusteigen hiesse, dass die Oberflaeche "kein
    // Regler" meldet, waehrend der Regler sauber antwortet.
    if (i + w > len) break;

    switch (bit) {
      case 0:  g_val.tempFet      = be16(d + i) / 10.0f;  break;
      case 1:  g_val.tempMotor    = be16(d + i) / 10.0f;  break;
      case 2:  g_val.currentMotor = be32(d + i) / 100.0f; break;
      case 3:  g_val.currentIn    = be32(d + i) / 100.0f; break;
      case 6:  g_val.duty         = be16(d + i) / 1000.0f;break;
      case 7:  g_val.erpm         = be32(d + i);          break;
      case 8:  g_val.vIn          = be16(d + i) / 10.0f;  break;
      case 15: g_val.fault        = d[i];                 break;
      case 21: g_val.status       = d[i];                 break;
      default: break;                                     // übersprungen
    }
    i += w;
  }

  g_val.seen = true;
  g_lastValueMs = millis();
}

void onFwVersion(const uint8_t* d, size_t len) {
  if (len < 3) return;
  g_fwMajor = d[1];
  g_fwMinor = d[2];
  // Danach folgt der Hardwarename als nullterminierte Zeichenkette.
  size_t i = 3, k = 0;
  while (i < len && d[i] != 0 && k < sizeof(g_hwName) - 1) g_hwName[k++] = d[i++];
  g_hwName[k] = '\0';
  g_fwKnown = true;
  Serial.print("# VESC: Firmware "); Serial.print(g_fwMajor);
  Serial.print('.'); Serial.print(g_fwMinor);
  Serial.print("  Hardware \""); Serial.print(g_hwName); Serial.println("\"");
}

void dispatch(const uint8_t* d, size_t len) {
  if (len == 0) return;
  switch (d[0]) {
    case COMM_GET_VALUES_SELECTIVE: onValues(d, len);    break;
    case COMM_FW_VERSION:           onFwVersion(d, len); break;
    default: break;   // alles andere interessiert diese Variante nicht
  }
}

}  // namespace

// ------------------------------------------------------------------ Schnittstelle

void begin() {
  port().begin(VESC_BAUD, SERIAL_8N1, PIN_DRIVE_RX, PIN_DRIVE_TX);
  g_rx.reset();
  sendId(COMM_FW_VERSION);   // kürzeste sinnvolle Transaktion: prüft Rahmen,
                             // Prüfsumme und Verkabelung in einem Zug
}

void service() {
  // Alles lesen, was da ist. Nicht blockierend — der Webserver läuft nebenher.
  while (port().available()) {
    if (g_rx.feed(static_cast<uint8_t>(port().read()))) {
      dispatch(g_rx.payload(), g_rx.payloadLen());
    }
  }

  const uint32_t now = millis();

  // Sollwert wiederholen, solange gefahren wird. Nur das hält den Timeout des
  // Reglers offen; Messwertabfragen tun es nicht.
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
    if (!g_fwKnown) sendId(COMM_FW_VERSION);   // bis er sich gemeldet hat
  }

  g_val.ageMs = g_val.seen ? (now - g_lastValueMs) : 0;
}

const Values& values() { return g_val; }

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
  // Strom null heißt für den Regler: Brücke sperren, Maschine läuft frei aus.
  // NICHT Tastverhältnis null — das wäre im BLDC-Betrieb Kurzschlussbremsung, und
  // im FOC-Betrieb ein geregelter Arbeitspunkt, der zurueckspeist
  // (docs/07-sicherheit.md §3.1).
  sendInt32(COMM_SET_CURRENT, 0);
}

bool fwKnown()   { return g_fwKnown; }
uint8_t fwMajor(){ return g_fwMajor; }
uint8_t fwMinor(){ return g_fwMinor; }
const char* hwName() { return g_hwName; }

const char* faultText(uint8_t code) {
  switch (code) {
    case 0:  return "kein Fehler";
    case 1:  return "Ueberspannung";
    case 2:  return "Unterspannung";
    case 3:  return "Treiberstufe (DRV)";
    case 4:  return "Ueberstrom";
    case 5:  return "Uebertemperatur FET";
    case 6:  return "Uebertemperatur Motor";
    case 7:  return "Treiberversorgung zu hoch";
    case 8:  return "Treiberversorgung zu niedrig";
    case 9:  return "MCU-Unterspannung";
    case 10: return "Neustart durch Watchdog";
    case 11: return "Geber SPI";
    case 12: return "Geber Amplitude zu klein";
    case 13: return "Geber Amplitude zu gross";
    case 14: return "Flash beschaedigt";
    case 15: case 16: case 17: return "Stromsensor-Nullpunkt";
    case 18: return "Straenge unsymmetrisch";
    case 19: return "Abschalteingang (BRK)";
    // Alles darüber gibt es erst in neueren Firmwarestaenden. Lieber ehrlich
    // "unbekannt" melden als auf einen falschen Namen abbilden.
    default: return "unbekannt";
  }
}

}  // namespace vesc
