#include "speed.h"
#include "config.h"
#include "settings.h"

namespace speed {
namespace {

// Ringpuffer der Marken-Zeitstempel. Eine Stelle mehr als Marken pro Umdrehung,
// damit t[n] und t[n-MARKS_PER_REV] gleichzeitig verfügbar sind.
constexpr uint8_t BUF = MARKS_PER_REV + 1;

volatile uint32_t g_markUs[BUF] = {0};
volatile uint8_t  g_markHead = 0;
volatile uint32_t g_markCount = 0;
volatile uint32_t g_lastMarkUs = 0;
volatile uint8_t  g_markInRev = 0;

volatile uint32_t g_revCount = 0;
volatile bool     g_indexFlag = false;

volatile uint32_t g_motorLastUs = 0;
volatile uint32_t g_motorPeriodUs = 0;

// Laufrad: Ringpuffer wie an der Trommel.
constexpr uint8_t WBUF = WHEEL_MARKS_PER_REV + 1;
volatile uint32_t g_wheelUs[WBUF] = {0};
volatile uint8_t  g_wheelHead = 0;
volatile uint32_t g_wheelCount = 0;

// Verdrahtungstest: eigene Zähler je Kanal (nullbar) und der letzte Zeitstempel.
// Gezählt wird, was auch die Messung sieht — also hinter dem Störfilter.
volatile uint32_t g_chCount[CH_N]  = {0};
volatile uint32_t g_chLastUs[CH_N] = {0};
volatile uint32_t g_wheelLastUs = 0;
constexpr uint8_t CH_PIN[CH_N] = {PIN_MARK, PIN_INDEX, PIN_WHEEL, PIN_MOTOR_HALL};
const char* const CH_NAME[CH_N] = {"Marken", "Index", "Laufrad", "Motor"};

void IRAM_ATTR tick(Channel ch, uint32_t nowUs) {
  g_chCount[ch]++;
  g_chLastUs[ch] = nowUs;
}

void IRAM_ATTR onMark() {
  const uint32_t now = micros();
  // Störimpulse und Prellen ausfiltern. Der kürzeste echte Abstand liegt bei
  // 60 km/h und 6 Marken bei 30 ms, bei 100 km/h bei 18 ms.
  if (now - g_lastMarkUs < MIN_PULSE_US) return;
  g_lastMarkUs = now;

  g_markHead = (g_markHead + 1) % BUF;
  g_markUs[g_markHead] = now;
  g_markCount++;
  g_markInRev = (g_markInRev + 1) % MARKS_PER_REV;
  tick(CH_MARK, now);
}

void IRAM_ATTR onIndex() {
  g_revCount++;
  g_indexFlag = true;
  g_markInRev = 0;  // Index definiert den Winkelnullpunkt
  tick(CH_INDEX, micros());
}

void IRAM_ATTR onMotorHall() {
  const uint32_t now = micros();
  const uint32_t dt = now - g_motorLastUs;
  if (dt < 100) return;  // Störfilter; bei 480 1/min und 15 Flanken sind es 8,3 ms
  g_motorPeriodUs = dt;
  g_motorLastUs = now;
  tick(CH_MOTOR, now);
}

void IRAM_ATTR onWheel() {
  const uint32_t now = micros();
  if (now - g_wheelLastUs < MIN_PULSE_US) return;
  g_wheelLastUs = now;
  g_wheelHead = (g_wheelHead + 1) % WBUF;
  g_wheelUs[g_wheelHead] = now;
  g_wheelCount++;
  tick(CH_WHEEL, now);
}

}  // namespace

void begin() {
  pinMode(PIN_MARK, INPUT_PULLUP);
  pinMode(PIN_INDEX, INPUT_PULLUP);
  pinMode(PIN_WHEEL, INPUT_PULLUP);
  pinMode(PIN_MOTOR_HALL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_MARK), onMark, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_INDEX), onIndex, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_WHEEL), onWheel, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_HALL), onMotorHall, FALLING);
}

float revPerSec() {
  if (g_markCount <= MARKS_PER_REV) return 0.0f;

  noInterrupts();
  const uint8_t head = g_markHead;
  const uint32_t tNew = g_markUs[head];
  const uint32_t tOld = g_markUs[(head + BUF - MARKS_PER_REV) % BUF];
  const uint32_t lastUs = g_lastMarkUs;
  interrupts();

  const uint32_t span = tNew - tOld;   // eine volle Umdrehung
  if (span == 0) return 0.0f;

  // Steht die Trommel gerade? Dann ist der letzte Zeitstempel alt.
  if (micros() - lastUs > span * 2 + 100000UL) return 0.0f;

  return 1000000.0f / static_cast<float>(span);
}

float kmh() { return kmhFromRevPerSec(revPerSec()); }
float omega() { return omegaFromRevPerSec(revPerSec()); }

uint32_t markCount() { return g_markCount; }
uint32_t revCount() { return g_revCount; }

bool consumeIndexFlag() {
  if (!g_indexFlag) return false;
  g_indexFlag = false;
  return true;
}

float motorRevPerSec() {
  noInterrupts();
  const uint32_t period = g_motorPeriodUs;
  const uint32_t last = g_motorLastUs;
  interrupts();

  if (period == 0) return 0.0f;
  if (micros() - last > 500000UL) return 0.0f;  // steht
  return 1000000.0f / (static_cast<float>(period) * MOTOR_PULSES_PER_REV);
}

float wheelRevPerSec() {
  if (g_wheelCount <= WHEEL_MARKS_PER_REV) return 0.0f;

  noInterrupts();
  const uint8_t head = g_wheelHead;
  const uint32_t tNew = g_wheelUs[head];
  const uint32_t tOld = g_wheelUs[(head + WBUF - WHEEL_MARKS_PER_REV) % WBUF];
  const uint32_t lastUs = g_wheelLastUs;
  interrupts();

  const uint32_t span = tNew - tOld;   // eine volle Radumdrehung
  if (span == 0) return 0.0f;
  if (micros() - lastUs > span * 2 + 100000UL) return 0.0f;   // steht
  return 1000000.0f / static_cast<float>(span);
}

bool slipDetected() {
  const float drum = revPerSec();
  const float motor = motorRevPerSec();
  if (drum < 0.2f || motor < 0.2f) return false;  // zu langsam für eine Aussage

  const float expected = motor / BELT_RATIO;
  return fabsf(expected - drum) / expected > SLIP_TOLERANCE;
}

bool stalled() {
  noInterrupts();
  const uint32_t last = g_lastMarkUs;
  interrupts();
  return (micros() - last) > STALL_TIMEOUT_MS * 1000UL;
}

uint8_t markIndexInRev() { return g_markInRev; }

const char* channelName(Channel ch) { return CH_NAME[ch]; }

ChannelState channel(Channel ch) {
  noInterrupts();
  const uint32_t count = g_chCount[ch];
  const uint32_t last  = g_chLastUs[ch];
  interrupts();
  ChannelState s;
  s.count   = count;
  s.sinceMs = count ? (micros() - last) / 1000UL : 0xFFFFFFFFUL;
  s.low     = digitalRead(CH_PIN[ch]) == LOW;
  return s;
}

void resetCounters() {
  noInterrupts();
  for (uint8_t i = 0; i < CH_N; i++) { g_chCount[i] = 0; g_chLastUs[i] = 0; }
  interrupts();
}

}  // namespace speed
