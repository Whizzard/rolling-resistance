#include "torque.h"
#include "config.h"
#include "settings.h"

namespace torque {
namespace {

long g_raw = 0;
bool g_haveValue = false;
uint32_t g_lastValueMs = 0;
long g_offset = 0;

// Ringpuffer für die ANZEIGE. Steht hier und nicht im Webserver, weil er zu den
// Messwerten gehört und nicht zur Netzwerkschicht.
long     g_ring[DISPLAY_AVG_N] = {0};
uint8_t  g_ri = 0, g_rn = 0;

// Statische Tara, nicht blockierend: gesammelt wird in update().
double   g_tareSum = 0.0, g_tareSumSq = 0.0;
float    g_tareSigma = NAN;
uint16_t g_tareN = 0;
bool     g_tareBusy = false;
uint32_t g_tareStartMs = 0;

// Akkumulator für die umdrehungssynchrone Mittelung
double g_sum = 0.0;
uint32_t g_count = 0;
long g_accMin = 0, g_accMax = 0;

// Stichprobe im Stillstand (sampleBegin/sampleStop)
bool     g_smpOn = false;
double   g_smpSum = 0.0, g_smpSumSq = 0.0;
uint32_t g_smpN = 0;
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
  g_offset = settings::tareOffset();   // letzte Tara, ueberlebt den Neustart
  resetAccumulator();
}

bool update() {
  if (!ready()) return false;
  g_raw = readRaw();
  g_haveValue = true;
  g_lastValueMs = millis();

  g_ring[g_ri] = g_raw;
  g_ri = (g_ri + 1) % DISPLAY_AVG_N;
  if (g_rn < DISPLAY_AVG_N) g_rn++;

  if (g_smpOn) {
    g_smpSum += static_cast<double>(g_raw);
    g_smpSumSq += static_cast<double>(g_raw) * static_cast<double>(g_raw);
    g_smpN++;
  }

  // Laufende Tara mitnehmen. Abbruch nach TARE_TIMEOUT_MS, damit ein stummer
  // HX711 den Zustand nicht auf Dauer besetzt.
  if (g_tareBusy) {
    g_tareSum += static_cast<double>(g_raw);
    g_tareSumSq += static_cast<double>(g_raw) * static_cast<double>(g_raw);
    g_tareN++;
    if (g_tareN >= TARE_SAMPLES || (millis() - g_tareStartMs) > TARE_TIMEOUT_MS) {
      if (g_tareN > 0) {
        g_offset = static_cast<long>(g_tareSum / g_tareN);
        const double mean = g_tareSum / g_tareN;
        const double var = g_tareN > 1 ? (g_tareSumSq - g_tareN * mean * mean) / (g_tareN - 1) : 0.0;
        g_tareSigma = var > 0.0 ? static_cast<float>(sqrt(var)) : 0.0f;
        settings::setTareOffset(g_offset);
      }
      g_tareBusy = false;
      resetAccumulator();
    }
  }
  return true;
}

void displayStats(long& mean, long& ripple) {
  if (g_rn == 0) { mean = 0; ripple = 0; return; }
  long long sum = 0;
  long lo = g_ring[0], hi = g_ring[0];
  for (uint8_t i = 0; i < g_rn; i++) {
    const long x = g_ring[i];
    sum += x;
    if (x < lo) lo = x;
    if (x > hi) hi = x;
  }
  mean   = static_cast<long>(sum / g_rn);   // ROH, ohne Tara — die steht in tareOffset()
  ripple = hi - lo;
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
  return static_cast<float>(g_raw - g_offset) * settings::loadcellNPerCount();
}

float momentNm() {
  const float f = newtons();
  return isnan(f) ? NAN : f * settings::torqueArmM();
}

float rollForceN() {
  const float m = momentNm();
  return isnan(m) ? NAN : m / settings::drumRadiusM();
}

float powerW(float omega) {
  const float m = momentNm();
  return isnan(m) ? NAN : m * omega;
}

void tareBegin() {
  g_tareSum = 0.0;
  g_tareSumSq = 0.0;
  g_tareN = 0;
  g_tareBusy = true;
  g_tareStartMs = millis();
}

bool     tareBusy()  { return g_tareBusy; }
uint16_t tareCount() { return g_tareN; }
float    tareSigma() { return g_tareSigma; }

long tareOffset() { return g_offset; }

void accumulate(uint8_t markIndex) {
  if (g_count == 0) { g_accMin = g_accMax = g_raw; }
  else { if (g_raw < g_accMin) g_accMin = g_raw; if (g_raw > g_accMax) g_accMax = g_raw; }
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
  g_accMin = g_accMax = 0;
  for (uint8_t i = 0; i < MARKS_PER_REV; i++) {
    g_binSum[i] = 0.0;
    g_binCount[i] = 0;
  }
}

uint32_t sampleCount() { return g_count; }
double   sumRaw()      { return g_sum; }

float averageRaw() {
  if (g_count == 0) return NAN;
  return static_cast<float>(g_sum / g_count);
}

float averageN() {
  if (!isCalibrated() || g_count == 0) return NAN;
  return (averageRaw() - static_cast<float>(g_offset)) * settings::loadcellNPerCount();
}

long rippleRaw() { return g_count ? g_accMax - g_accMin : 0; }

void sampleBegin() { g_smpOn = true; g_smpSum = g_smpSumSq = 0.0; g_smpN = 0; }
void sampleStop()  { g_smpOn = false; }
uint32_t sampleN() { return g_smpN; }
float sampleMean() { return g_smpN ? static_cast<float>(g_smpSum / g_smpN) : NAN; }
float sampleSigma() {
  if (g_smpN < 2) return NAN;
  const double mean = g_smpSum / g_smpN;
  const double var = (g_smpSumSq - g_smpN * mean * mean) / (g_smpN - 1);
  return var > 0.0 ? static_cast<float>(sqrt(var)) : 0.0f;
}

float binAverageRaw(uint8_t bin) {
  if (bin >= MARKS_PER_REV || g_binCount[bin] == 0) return NAN;
  return static_cast<float>(g_binSum[bin] / g_binCount[bin]);
}

uint8_t binCount() { return MARKS_PER_REV; }

}  // namespace torque
