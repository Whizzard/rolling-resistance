#include "revsrc.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "drivespeed.h"
#include "calc/linfit.h"

namespace revsrc {
namespace {

Source   g_src = SRC_NONE;
uint32_t g_lastMarkCount = 0;
uint32_t g_lastMarkMs = 0;
uint32_t g_lastIdxCount = 0;
uint32_t g_lastIdxMs = 0;
bool     g_indexLive = false;   // real index pulses arriving

// Computed revolutions from the controller speed. g_frac carries the fraction
// between two passes, otherwise the remainders would be lost on every call.
float    g_frac = 0.0f;
uint32_t g_revs = 0;
bool     g_indexFlag = false;
uint32_t g_lastIntegMs = 0;

// Speed history for dv/dt. Eight samples over DVDT_WINDOW_MS, slope from the
// regression line -- a difference of only two samples is too noisy, and exactly
// this number decides whether a measurement point is valid (L9/L10).
constexpr uint8_t DVDT_N = 8;
constexpr uint8_t DVDT_MIN_N = 3;   // fewer samples are no line
float    g_vHist[DVDT_N];
uint32_t g_tHist[DVDT_N];
uint8_t  g_dvdtCount = 0;
uint8_t  g_dvdtHead = 0;
uint32_t g_lastSampleMs = 0;

void sampleDvdt(uint32_t now, float v) {
  const uint32_t spacing = DVDT_WINDOW_MS / DVDT_N;
  if (g_lastSampleMs != 0 && (now - g_lastSampleMs) < spacing) return;
  g_lastSampleMs = now;
  g_vHist[g_dvdtHead] = v;
  g_tHist[g_dvdtHead] = now;
  g_dvdtHead = (g_dvdtHead + 1) % DVDT_N;
  if (g_dvdtCount < DVDT_N) g_dvdtCount++;
}

}  // namespace

void begin() {
  speed::begin();
  g_src = SRC_NONE;
  g_lastMarkCount = speed::markCount();
  g_lastMarkMs = 0;
  g_lastIdxCount = speed::revCount();
  g_lastIdxMs = 0;
  g_indexLive = false;
  g_frac = 0.0f;
  g_revs = 0;
  g_indexFlag = false;
  g_lastIntegMs = millis();
  g_dvdtCount = 0;
  g_dvdtHead = 0;
  g_lastSampleMs = 0;
}

void service() {
  const uint32_t now = millis();
  speed::service();   // timestamps -> revolutions, mark number, pattern

  // Source selection. Marks apply as long as they count; the same limit with which the
  // state machine detects standstill despite a setpoint.
  const uint32_t mc = speed::markCount();
  if (mc != g_lastMarkCount) {
    g_lastMarkCount = mc;
    g_lastMarkMs = now;
    g_src = SRC_MARKS;
  } else if (g_src == SRC_NONE) {
    // Marks that have counted once remain the source -- even at standstill.
    // Until 2026-11-25 the source fell back to the controller after STALL_TIMEOUT_MS;
    // the run then reported "speed source: controller" at the start although the marks
    // took over with the first pulse, and the revolution counter switched in the middle
    // of the point. If the mark sensor fails during a run, "no speed pulses" applies.
    g_src = SRC_VESC;
  }

  // Real index pulses? Then the revolutions are real, even without the eight marks.
  const uint32_t ic = speed::revCount();
  if (ic != g_lastIdxCount) { g_lastIdxCount = ic; g_lastIdxMs = now; g_indexLive = true; }
  else if (g_indexLive && (now - g_lastIdxMs) > STALL_TIMEOUT_MS * 3) g_indexLive = false;
  if (g_src == SRC_VESC && g_indexLive) g_src = SRC_VESC_INDEX;
  if (g_src == SRC_VESC_INDEX && !g_indexLive) g_src = SRC_VESC;

  if (g_src == SRC_VESC) {
    // Integrate revolutions from the controller speed.
    const uint32_t dt = now - g_lastIntegMs;
    if (dt > 0) {
      g_lastIntegMs = now;
      const float u = settings::drumCircumferenceM(), k = drivespeed::kmh();
      // Do not integrate without the controller (NAN): the counter would stay NAN otherwise.
      const float rps = (u > 0.01f && !isnan(k)) ? (k * KMH_TO_MS / u) : 0.0f;
      g_frac += rps * (static_cast<float>(dt) / 1000.0f);
      while (g_frac >= 1.0f) {
        g_frac -= 1.0f;
        g_revs++;
        g_indexFlag = true;
      }
    }
  } else {
    g_lastIntegMs = now;
    g_frac = 0.0f;
  }

  sampleDvdt(now, kmh());
}

Source source() { return g_src; }

const char* sourceName() {
  switch (g_src) {
    case SRC_MARKS: return "marks";
    case SRC_VESC_INDEX: return "controller+index";
    case SRC_VESC:  return "controller";
    default:        return "none";
  }
}

bool angleReference() { return g_src == SRC_MARKS; }
bool realRevolutions() { return g_src == SRC_MARKS || g_src == SRC_VESC_INDEX; }

// Without marks and without the controller: NAN, no invented zero. The comparisons for
// standstill (< 0.05 1/s) are false with NAN, so a FAULT stays until the controller
// reports again -- more honest than a standstill nobody has measured.
float revPerSec() {
  if (g_src == SRC_MARKS) return speed::revPerSec();
  const float u = settings::drumCircumferenceM(), k = drivespeed::kmh();
  return (u > 0.01f && !isnan(k)) ? (k * KMH_TO_MS / u) : (isnan(k) ? NAN : 0.0f);
}

float kmh() {
  return (g_src == SRC_MARKS) ? speed::kmh() : drivespeed::kmh();
}

float omega() { return 2.0f * PI * revPerSec(); }

uint32_t revCount() {
  return realRevolutions() ? speed::revCount() : g_revs;
}

bool consumeIndexFlag() {
  if (realRevolutions()) return speed::consumeIndexFlag();
  const bool f = g_indexFlag;
  g_indexFlag = false;
  return f;
}

uint8_t markIndexInRev() {
  return (g_src == SRC_MARKS) ? speed::markIndexInRev() : MARKS_PER_REV;
}

float dvdtKmhS() {
  if (g_dvdtCount < DVDT_MIN_N) return NAN;

  // Regression line v = a + b·t over the window, b is dv/dt.
  calc::LinFit fit;
  const uint32_t t0 = g_tHist[(g_dvdtHead + DVDT_N - g_dvdtCount) % DVDT_N];
  for (uint8_t i = 0; i < g_dvdtCount; i++) {
    const uint8_t k = (g_dvdtHead + DVDT_N - g_dvdtCount + i) % DVDT_N;
    fit.add(static_cast<double>(g_tHist[k] - t0) / 1000.0, g_vHist[k]);
  }
  return static_cast<float>(fit.slope(3, 1e-9));
}

}  // namespace revsrc
