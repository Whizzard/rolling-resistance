#include "autozero.h"

#include "config.h"
#include "drivespeed.h"
#include "revsrc.h"
#include "speed.h"
#include "speedctl.h"
#include "state.h"
#include "torque.h"
#include "vesc/vesc.h"

namespace autozero {
namespace {

St       g_st = OFF;
bool     g_setTare = false;
uint8_t  g_k = 0;
float    g_mean[AZ_POSITIONS], g_sig[AZ_POSITIONS], g_deg[AZ_POSITIONS], g_spread[AZ_POSITIONS];
uint32_t g_n[AZ_POSITIONS];
uint32_t g_posT0 = 0, g_steadySince = 0, g_wheel0 = 0, g_rev0 = 0, g_revLast = 0;
bool     g_sampling = false;
double   g_sumLast = 0; uint32_t g_nLast = 0;
uint32_t g_revMs = 0; float g_vFirst = NAN, g_vLast = NAN;   // revolution times: speed of the first and last one
float    g_revLo = NAN, g_revHi = NAN;
float    g_result = NAN, g_range = NAN, g_sigma = NAN, g_plus = NAN, g_minus = NAN;
const char* g_why = "";
static_assert(AZ_POSITIONS % 2 == 0, "autozero alternates the direction: needs an even number of readings");

uint32_t wheelCount() { uint32_t c, us; speed::wheelPulses(c, us); return c; }
int8_t   dirOf(uint8_t k) { return (k % 2 == 0) ? 1 : -1; }

void fail(const char* w) {
  speedctl::stop();
  torque::sampleStop(); g_sampling = false;
  g_why = w; g_st = FAILED;
  Serial.printf("# Auto zero: %s\n", w);
}

void finish() {
  double s = 0, ss = 0, sp = 0, sm = 0; uint32_t n = 0; float rg = 0;
  for (uint8_t i = 0; i < AZ_POSITIONS; i++) {
    s += g_mean[i];
    if (dirOf(i) > 0) sp += g_mean[i]; else sm += g_mean[i];
    if (g_spread[i] > rg) rg = g_spread[i];
    if (!isnan(g_sig[i]) && g_n[i] > 1) { ss += static_cast<double>(g_sig[i]) * g_sig[i] * (g_n[i] - 1); n += g_n[i] - 1; }
  }
  g_result = static_cast<float>(s / AZ_POSITIONS);
  g_plus = static_cast<float>(sp / (AZ_POSITIONS / 2));
  g_minus = static_cast<float>(sm / (AZ_POSITIONS / 2));
  g_range = rg;
  g_sigma = n ? static_cast<float>(sqrt(ss / n)) : NAN;
  if (g_setTare) torque::setTare(lroundf(g_result), g_sigma);
  Serial.printf("# Auto zero: mean %.1f counts; + %.1f, - %.1f (half difference %.1f = drum friction); revolution spread %.1f, sigma %.1f\n",
                g_result, g_plus, g_minus, (g_plus - g_minus) / 2, g_range, g_sigma);
  g_st = DONE;
}

// Next reading (or the end) once the drum is below STANDSTILL_KMH.
void beginReading() { g_posT0 = millis(); g_steadySince = 0; g_st = BRAKE; }

}  // namespace

bool start(bool setTare) {
  if (state::phase() != state::IDLE) { g_why = "only at standstill"; return false; }
  if (!vesc::connected())            { g_why = "no controller"; return false; }
  if (!torque::isResponding())       { g_why = "load cell not responding"; return false; }
  if (speedctl::running())           { g_why = "drive running"; return false; }
  g_setTare = setTare; g_k = 0; g_result = g_range = g_sigma = g_plus = g_minus = NAN; g_why = "";
  for (uint8_t i = 0; i < AZ_POSITIONS; i++) { g_mean[i] = g_sig[i] = g_deg[i] = g_spread[i] = NAN; g_n[i] = 0; }
  g_wheel0 = wheelCount();
  beginReading();
  return true;
}

void stop() {
  if (g_st == SPINUP || g_st == READ || (g_st == BRAKE && g_k > 0)) speedctl::stop();   // also ends the controller brake
  if (g_sampling) { torque::sampleStop(); g_sampling = false; }
  g_st = OFF;
}

void service() {
  if (g_st == OFF || g_st == DONE || g_st == FAILED) return;
  if (state::phase() != state::IDLE && state::phase() != state::TARE) { fail("aborted: stand left standstill mode"); return; }
  if (!vesc::connected()) { fail("controller lost"); return; }
  // The wheel turning along = it rests on the drum. Echo pulses are filtered in speed.cpp.
  if (wheelCount() - g_wheel0 >= AZ_WHEEL_PULSES_MAX) { fail("test wheel turns along: lift it"); return; }
  const uint32_t now = millis();
  if (now - g_posT0 > AZ_POS_TIMEOUT_MS) { fail(g_st == BRAKE ? "drum does not stand still" : "drum does not reach a steady speed"); return; }

  switch (g_st) {
    case BRAKE: {    // controller brakes; below STANDSTILL_KMH reverse at once (builder 2026-11-28)
      const float v = drivespeed::kmh();
      if (isnan(v) || fabsf(v) >= STANDSTILL_KMH) return;
      if (g_k >= AZ_POSITIONS) { speedctl::coast(); finish(); return; }   // after the last reading: release
      if (!speedctl::crawl(dirOf(g_k), AZ_SPIN_KMH)) return;   // refused (relays settling etc.): next time
      g_st = SPINUP;
      return; }
    case SPINUP: {
      if (!speedctl::crawling()) { fail(speedctl::crawlWhy()); return; }
      const float v = speedctl::crawlSpeed();
      if (fabsf(v - AZ_SPIN_KMH) > AZ_SPIN_TOL_KMH) { g_steadySince = 0; return; }
      if (!g_steadySince) { g_steadySince = now; return; }
      if (now - g_steadySince < AZ_SPIN_STEADY_MS) return;
      speedctl::crawlFreeze(true);
      g_rev0 = g_revLast = revsrc::revCount();
      g_st = READ;    // sampling starts at the next whole revolution
      return; }
    case READ: {
      if (!speedctl::crawling()) { fail(speedctl::crawlWhy()); return; }
      const uint32_t r = revsrc::revCount();
      if (r == g_revLast) return;
      const uint32_t prevRev = g_revLast;
      g_revLast = r;
      if (!g_sampling) {   // first revolution boundary: start averaging
        torque::sampleBegin(); g_sampling = true; g_rev0 = r;
        g_revMs = now; g_vFirst = g_vLast = NAN;
        g_sumLast = 0; g_nLast = 0; g_revLo = g_revHi = NAN;
        return;
      }
      // One revolution done: its mean from the running sums.
      const uint32_t n = torque::sampleN(); const double sum = static_cast<double>(torque::sampleMean()) * n;
      if (n > g_nLast) {
        const float m = static_cast<float>((sum - g_sumLast) / (n - g_nLast));
        if (isnan(g_revLo) || m < g_revLo) g_revLo = m;
        if (isnan(g_revHi) || m > g_revHi) g_revHi = m;
      }
      g_sumLast = sum; g_nLast = n;
      if (now > g_revMs) { g_vLast = DRUM_CIRCUMFERENCE_M * (r - (prevRev)) / ((now - g_revMs) / 1000.0f) * 3.6f; if (isnan(g_vFirst)) g_vFirst = g_vLast; }
      g_revMs = now;
      if (r - g_rev0 < AZ_SPIN_REVS) return;
      // Speed check (2026-11-28): the drum's inertia goes straight into the cell, 0.05 km/h
      // over the window are about 0.012 Nm with DRUM_INERTIA_KGM2. Drifted: settle and read again.
      if (fabsf(g_vLast - g_vFirst) > AZ_SPIN_DRIFT_KMH) {
        Serial.printf("# Auto zero: speed drifted %.3f km/h during the reading, again\n", g_vLast - g_vFirst);
        torque::sampleStop(); g_sampling = false;
        speedctl::crawlFreeze(false); g_steadySince = 0; g_st = SPINUP;
        return;
      }
      g_mean[g_k] = torque::sampleMean(); g_sig[g_k] = torque::sampleSigma(); g_n[g_k] = n;
      g_deg[g_k] = (r - g_rev0) * 360.0f; g_spread[g_k] = g_revHi - g_revLo;
      torque::sampleStop(); g_sampling = false;
      speedctl::crawlBrake();   // controller brake; the next reading starts below STANDSTILL_KMH
      if (isnan(g_mean[g_k])) { fail("no load-cell values"); return; }
      Serial.printf("# Auto zero: reading %u (%c) over %u revolutions at %.1f km/h: %.1f counts (sigma %.1f, revolution spread %.1f, drift %+.3f km/h, n %lu)\n",
                    (unsigned)(g_k + 1), dirOf(g_k) > 0 ? '+' : '-', (unsigned)(r - g_rev0), AZ_SPIN_KMH,
                    g_mean[g_k], g_sig[g_k], g_spread[g_k], g_vLast - g_vFirst, (unsigned long)n);
      ++g_k;
      beginReading();
      return; }
    default: return;
  }
}

St state() { return g_st; }
uint8_t position() { return g_k < AZ_POSITIONS ? g_k : AZ_POSITIONS - 1; }
uint8_t done() { return g_st == DONE ? AZ_POSITIONS : g_k; }
float mean(uint8_t i) { return i < AZ_POSITIONS ? g_mean[i] : NAN; }
float degTurned(uint8_t i) { return i < AZ_POSITIONS ? g_deg[i] : NAN; }
float resultCounts() { return g_result; }
float rangeCounts() { return g_range; }
float sigmaCounts() { return g_sigma; }
float plusCounts() { return g_plus; }
float minusCounts() { return g_minus; }
const char* why() { return g_why; }
const char* phaseText() {
  switch (g_st) {
    case BRAKE:  return g_k ? "reversing" : "starting";
    case SPINUP: return dirOf(g_k) > 0 ? "spin-up forwards" : "spin-up backwards";
    case READ:   return dirOf(g_k) > 0 ? "reading forwards" : "reading backwards";
    case DONE:   return "done";
    case FAILED: return g_why;
    default: return "";
  }
}

}  // namespace autozero
