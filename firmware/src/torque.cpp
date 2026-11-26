#include "torque.h"
#include "calc/linfit.h"
#include "config.h"
#include "settings.h"

namespace torque {
namespace {

long g_raw = 0;
bool g_haveValue = false;
uint32_t g_lastValueMs = 0;
long g_offset = 0;

// Ring buffer for the DISPLAY. It lives here and not in the web server, because it
// belongs to the measurements and not to the network layer.
long     g_ring[DISPLAY_AVG_N] = {0};
long     g_trace[TRACE_N] = {0};
uint16_t g_ti = 0, g_tn = 0;
uint8_t  g_ri = 0, g_rn = 0;

// Static tare, non-blocking: collected in update().
double   g_tareSum = 0.0, g_tareSumSq = 0.0;
float    g_tareSigma = NAN;
uint16_t g_tareN = 0;
bool     g_tareBusy = false;
uint32_t g_tareStartMs = 0;

// Accumulator for the revolution-synchronous averaging
double g_sum = 0.0;
uint32_t g_count = 0;
long g_accMin = 0, g_accMax = 0;

// Sample at standstill (sampleBegin/sampleStop)
bool     g_smpOn = false;
double   g_smpSum = 0.0, g_smpSumSq = 0.0;
uint32_t g_smpN = 0;
double g_binSum[MARKS_PER_REV] = {0.0};
uint32_t g_binCount[MARKS_PER_REV] = {0};

inline bool ready() { return digitalRead(PIN_HX711_DOUT) == LOW; }

// Bit-bang read of the HX711, in a critical section (since 2026-11-25).
//
// If SCK stays high for more than 60 us, the HX711 goes into power-down and the word is
// wrong -- and a wrong word is NOT reliably caught by the saturation check.
// A task interruption on core 1 can cause that. The section takes about 55 us
// (27 clocks of 2 us each); the mark ISR is shifted by at most that much, which at
// a 20 ms mark interval is a quarter per mille and goes unnoticed in the revolution
// window.
portMUX_TYPE g_hxMux = portMUX_INITIALIZER_UNLOCKED;
long readRaw() {
  long value = 0;
  portENTER_CRITICAL(&g_hxMux);
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(PIN_HX711_SCK, HIGH);
    delayMicroseconds(1);
    value = (value << 1) | (digitalRead(PIN_HX711_DOUT) ? 1 : 0);
    digitalWrite(PIN_HX711_SCK, LOW);
    delayMicroseconds(1);
  }
  // Extra clocks select channel and gain for the NEXT conversion.
  for (uint8_t i = 0; i < HX711_GAIN_PULSES; i++) {
    digitalWrite(PIN_HX711_SCK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_HX711_SCK, LOW);
    delayMicroseconds(1);
  }
  portEXIT_CRITICAL(&g_hxMux);
  // Sign-extend 24-bit two's complement to 32 bits
  if (value & 0x800000L) value |= ~0xFFFFFFL;
  return value;
}

}  // namespace

void begin() {
  pinMode(PIN_HX711_SCK, OUTPUT);
  pinMode(PIN_HX711_DOUT, INPUT);
  digitalWrite(PIN_HX711_SCK, LOW);
  g_offset = settings::tareOffset();   // last tare, survives a restart
  resetAccumulator();
}

// Finish the tare: offset from the mean, plus the spread. With zero values the old
// offset stays -- better than an invented one.
void finishTare() {
  if (g_tareN > 0) {
    g_offset = static_cast<long>(g_tareSum / g_tareN);
    g_tareSigma = calc::sdFromSums(g_tareSum, g_tareSumSq, g_tareN);
    if (isnan(g_tareSigma)) g_tareSigma = 0.0f;   // a single value: no measurable spread
    settings::setTareOffset(g_offset);
  }
  g_tareBusy = false;
  resetAccumulator();
}

bool update() {
  // Emergency exit for a silent HX711: BEFORE waiting for ready(). Until 2026-11-25
  // the check came after the read and was only reachable if a value did arrive --
  // a loose DOUT wire made the tare (and step 4 of the run) wait forever.
  if (g_tareBusy && (millis() - g_tareStartMs) > TARE_TIMEOUT_MS) finishTare();
  if (!ready()) return false;
  g_raw = readRaw();
  g_haveValue = true;
  g_lastValueMs = millis();

  g_ring[g_ri] = g_raw;
  g_ri = (g_ri + 1) % DISPLAY_AVG_N;
  if (g_rn < DISPLAY_AVG_N) g_rn++;
  g_trace[g_ti] = g_raw;
  g_ti = (g_ti + 1) % TRACE_N;
  if (g_tn < TRACE_N) g_tn++;

  if (g_smpOn) {
    g_smpSum += static_cast<double>(g_raw);
    g_smpSumSq += static_cast<double>(g_raw) * static_cast<double>(g_raw);
    g_smpN++;
  }

  // Feed the running tare. Abort after TARE_TIMEOUT_MS so that a silent
  // HX711 does not occupy the state forever.
  if (g_tareBusy) {
    g_tareSum += static_cast<double>(g_raw);
    g_tareSumSq += static_cast<double>(g_raw) * static_cast<double>(g_raw);
    g_tareN++;
    if (g_tareN >= TARE_SAMPLES || (millis() - g_tareStartMs) > TARE_TIMEOUT_MS) finishTare();
  }
  return true;
}

uint16_t trace(long* out, uint16_t max) {
  const uint16_t n = g_tn < max ? g_tn : max;
  for (uint16_t i = 0; i < n; i++) out[i] = g_trace[(g_ti + TRACE_N - n + i) % TRACE_N];
  return n;
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
  mean   = static_cast<long>(sum / g_rn);   // RAW, without tare -- that is in tareOffset()
  ripple = hi - lo;
}

long lastRaw() { return g_raw; }

bool isSaturated() {
  // Full 24-bit deflections mean overload, broken cable or power-down.
  return g_raw >= 0x7FFFF0L || g_raw <= -0x7FFFF0L;
}

bool isResponding() {
  return g_haveValue && (millis() - g_lastValueMs) < HX711_FRESH_MS;
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
float sampleSigma() { return calc::sdFromSums(g_smpSum, g_smpSumSq, g_smpN); }

float binAverageRaw(uint8_t bin) {
  if (bin >= MARKS_PER_REV || g_binCount[bin] == 0) return NAN;
  return static_cast<float>(g_binSum[bin] / g_binCount[bin]);
}

uint8_t binCount() { return MARKS_PER_REV; }

}  // namespace torque
