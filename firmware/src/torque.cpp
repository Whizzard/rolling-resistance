#include "torque.h"
#include "config.h"

namespace torque {
namespace {

long g_raw = 0;
bool g_haveValue = false;
uint32_t g_lastValueMs = 0;
long g_offset = 0;

// Akkumulator für die umdrehungssynchrone Mittelung
double g_sum = 0.0;
uint32_t g_count = 0;
double g_binSum[MARKS_PER_REV] = {0.0};
uint32_t g_binCount[MARKS_PER_REV] = {0};

inline bool ready() { return digitalRead(PIN_HX711_DOUT) == LOW; }

// Bit-Bang-Lesung des HX711.
//
// Bewusst OHNE noInterrupts(): die Marken-ISR muss weiterlaufen, sonst reißt die
// Drehzahlmessung Lücken. Die ISR ist nur wenige µs lang; der HX711 verträgt das,
// solange SCK nicht länger als 60 µs high bleibt (sonst Power-Down). Ein dadurch
// verfälschter Wert fällt durch die Sättigungsprüfung bzw. über die Mittelung
// nicht ins Gewicht.
long readRaw() {
  long value = 0;
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(PIN_HX711_SCK, HIGH);
    delayMicroseconds(1);
    value = (value << 1) | (digitalRead(PIN_HX711_DOUT) ? 1 : 0);
    digitalWrite(PIN_HX711_SCK, LOW);
    delayMicroseconds(1);
  }
  // Zusätzliche Takte wählen Kanal und Verstärkung für die NÄCHSTE Wandlung.
  for (uint8_t i = 0; i < HX711_GAIN_PULSES; i++) {
    digitalWrite(PIN_HX711_SCK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_HX711_SCK, LOW);
    delayMicroseconds(1);
  }
  // 24 Bit Zweierkomplement auf 32 Bit erweitern
  if (value & 0x800000L) value |= ~0xFFFFFFL;
  return value;
}

}  // namespace

void begin() {
  pinMode(PIN_HX711_SCK, OUTPUT);
  pinMode(PIN_HX711_DOUT, INPUT);
  digitalWrite(PIN_HX711_SCK, LOW);
  resetAccumulator();
}

bool update() {
  if (!ready()) return false;
  g_raw = readRaw();
  g_haveValue = true;
  g_lastValueMs = millis();
  return true;
}

long lastRaw() { return g_raw; }

bool isSaturated() {
  // Volle 24-Bit-Ausschläge bedeuten Überlast, Kabelbruch oder Power-Down.
  return g_raw >= 0x7FFFF0L || g_raw <= -0x7FFFF0L;
}

bool isResponding() {
  return g_haveValue && (millis() - g_lastValueMs) < 1000;
}

float newtons() {
  if (!isCalibrated()) return NAN;
  return static_cast<float>(g_raw - g_offset) * LOADCELL_N_PER_COUNT;
}

float momentNm() {
  const float f = newtons();
  return isnan(f) ? NAN : f * TORQUE_ARM_M;
}

float rollForceN() {
  const float m = momentNm();
  return isnan(m) ? NAN : m / DRUM_RADIUS_M;
}

float powerW(float omega) {
  const float m = momentNm();
  return isnan(m) ? NAN : m * omega;
}

void tareStatic() {
  double sum = 0.0;
  uint16_t n = 0;
  const uint32_t deadline = millis() + 5000;
  while (n < TARE_SAMPLES && millis() < deadline) {
    if (update()) {
      sum += static_cast<double>(g_raw);
      n++;
    }
  }
  if (n > 0) g_offset = static_cast<long>(sum / n);
  resetAccumulator();
}

long tareOffset() { return g_offset; }

void accumulate(uint8_t markIndex) {
  g_sum += static_cast<double>(g_raw);
  g_count++;
  if (markIndex < MARKS_PER_REV) {
    g_binSum[markIndex] += static_cast<double>(g_raw);
    g_binCount[markIndex]++;
  }
}

void resetAccumulator() {
  g_sum = 0.0;
  g_count = 0;
  for (uint8_t i = 0; i < MARKS_PER_REV; i++) {
    g_binSum[i] = 0.0;
    g_binCount[i] = 0;
  }
}

uint32_t sampleCount() { return g_count; }

float averageRaw() {
  if (g_count == 0) return NAN;
  return static_cast<float>(g_sum / g_count);
}

float averageN() {
  if (!isCalibrated() || g_count == 0) return NAN;
  return (averageRaw() - static_cast<float>(g_offset)) * LOADCELL_N_PER_COUNT;
}

float binAverageRaw(uint8_t bin) {
  if (bin >= MARKS_PER_REV || g_binCount[bin] == 0) return NAN;
  return static_cast<float>(g_binSum[bin] / g_binCount[bin]);
}

uint8_t binCount() { return MARKS_PER_REV; }

}  // namespace torque
