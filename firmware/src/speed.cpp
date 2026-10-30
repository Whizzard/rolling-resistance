#include "speed.h"
#include "config.h"

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
}

void IRAM_ATTR onIndex() {
  g_revCount++;
  g_indexFlag = true;
  g_markInRev = 0;  // Index definiert den Winkelnullpunkt
}

void IRAM_ATTR onMotorHall() {
  const uint32_t now = micros();
  const uint32_t dt = now - g_motorLastUs;
  if (dt < 100) return;  // Störfilter; bei 480 1/min und 15 Flanken sind es 8,3 ms
  g_motorPeriodUs = dt;
  g_motorLastUs = now;
}

}  // namespace

void begin() {
  pinMode(PIN_MARK, INPUT_PULLUP);
  pinMode(PIN_INDEX, INPUT_PULLUP);
  pinMode(PIN_MOTOR_HALL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_MARK), onMark, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_INDEX), onIndex, FALLING);
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

}  // namespace speed
