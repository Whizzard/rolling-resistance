#include "wheel.h"
#include "calc/spindown.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "revsrc.h"

namespace wheel {
namespace {

float    g_circRaw = NAN;    // last computed circumference before the plausibility check (diagnostics)
double   g_drumInt = 0.0;    // drum revolutions, integrated since begin()
uint32_t g_lastUs = 0;

// Sliding window: the drum integral at the last WHEEL_CIRC_WINDOW_REVS + 1
// wheel pulses (with one magnet per revolution one pulse is one revolution).
constexpr uint8_t N = WHEEL_CIRC_WINDOW_REVS * WHEEL_MARKS_PER_REV;
double   g_hist[N + 1];
uint8_t  g_head = 0;         // newest entry
uint32_t g_filled = 0;       // entries since the last standstill
uint32_t g_lastPulse = 0;    // last seen pulse counter
uint32_t g_lastPulseMs = 0;
float    g_circ = NAN;

void clear() {
  g_filled = 0;
  g_circ = NAN;
}

}  // namespace

// ---- Spin-down
// INTEGRAL METHOD instead of a point-wise difference (docs/11-review.md W8):
// from J dw/dt = -(A1 + B1 w^2), integrating over time gives
//     w(t) = w0 - (A1/J) t - (B1/J) * Int_0^t w^2 dt .
// That is LINEAR in the three unknowns w0, a = A1/J, b = B1/J, with the regressors
// t and I(t) = Int w^2 dt (trapezoid over the pulse intervals). No difference of two
// periods, so no noise from the timestamp resolution; the fit uses every pulse
// equally. The 3x3 normal equations are solved (Cramer).
bool     g_sdActive = false;
uint32_t g_sdRevs = 0;
uint32_t g_sdPrevUs = 0;       // timestamp of the last pulse
uint32_t g_sdStartUs = 0;
double   g_sdPrevW = 0.0;      // w from the last interval
double   g_sdI = 0.0;          // Int w^2 dt so far
// Normal equations: sums over x = (1, t, I), y = w
calc::SpindownSums g_sd;       // normal equations, solved in calc/spindown.h
uint32_t g_sdLastPulseMs = 0;
float    g_sdJ = NAN;
float    g_sdResidRms = NAN;   // residual of the last fit, rad/s
uint32_t g_sdFitN = 0;

void spindownFinish() {
  g_sdActive = false;
  if (g_sdRevs < SPINDOWN_MIN_REVS || g_sd.n < 4) {
    Serial.print("# Spin-down rejected: "); Serial.print(g_sdRevs);
    Serial.println(" revolutions, too few for a fit");
    return;
  }
  float a1, b1; double w0;
  int forced = 0;
  if (!g_sd.solvePositive(g_sdJ, a1, b1, w0, g_sdResidRms, forced)) { Serial.println("# Spin-down rejected: no physical fit (singular or negative losses)"); return; }
  if (forced) Serial.println(forced == 1 ? "# Spin-down: free fit gave A1 < 0, refitted with A1 = 0"
                                         : "# Spin-down: free fit gave B1 < 0, refitted with B1 = 0");
  g_sdFitN = static_cast<uint32_t>(g_sd.n);
  if (g_sdResidRms > SPINDOWN_RESID_MAX_RADS) {
    Serial.print("# Spin-down rejected: residual "); Serial.print(g_sdResidRms, 2);
    Serial.println(" rad/s, a disturbed pulse train (phantom or missing pulses)");
    return;
  }
  if (settings::setWheelModel(a1, b1)) {
    Serial.print("# Wheel loss model saved: A1="); Serial.print(a1, 5);
    Serial.print(" Nm  B1="); Serial.print(b1, 8); Serial.print(" Nm/(rad/s)^2  from ");
    Serial.print(g_sdRevs); Serial.print(" revolutions, residual ");
    Serial.print(g_sdResidRms, 3); Serial.print(" rad/s, w0="); Serial.print(w0, 2);
    Serial.print(", J_wheel="); Serial.print(g_sdJ, 4);
    Serial.println(isnan(settings::wheelJKgm2()) ? " kg m^2 (ESTIMATED, WHEEL_J_FACTOR)" : " kg m^2 (measured)");
  } else {
    Serial.println("# Wheel loss model implausible, not saved");
  }
}

void begin() {
  g_drumInt = 0.0;
  g_lastUs = micros();
  uint32_t last;
  speed::wheelPulses(g_lastPulse, last);
  clear();
}

void service() {
  // Integrate the drum, every round.
  const uint32_t nowUs = micros();
  // Unknown (NAN, controller not answering yet) counts as 0: a single NAN made the
  // integral NAN until the next restart, and with it every circumference (2026-11-25).
  const float rps = revsrc::revPerSec();
  if (isfinite(rps)) g_drumInt += static_cast<double>(rps) * static_cast<double>(nowUs - g_lastUs) * 1e-6;
  g_lastUs = nowUs;

  uint32_t pulses, lastUs;
  speed::wheelPulses(pulses, lastUs);

  if (pulses == g_lastPulse) {
    // No new pulse. If the wheel stands, the window expires -- and a running
    // spin-down is over.
    if (g_filled && millis() - g_lastPulseMs > WHEEL_CIRC_GAP_MS) clear();
    if (g_sdActive && millis() - g_sdLastPulseMs > WHEEL_CIRC_GAP_MS) spindownFinish();
    return;
  }
  // Several pulses since the last round (does not happen with a 1 kHz loop,
  // but could not be separated) -- then only the newest counts.
  const uint32_t prevPulseUs = g_sdPrevUs;
  g_lastPulse = pulses;
  g_lastPulseMs = millis();

  // Spin-down: w from the pulse interval; one point (t, I, w) per pulse for the fit.
  if (g_sdActive) {
    g_sdLastPulseMs = millis();
    if (prevPulseUs) {
      const double dt = (lastUs - prevPulseUs) * 1e-6;
      if (dt > 1e-3) {
        const double w = 2.0 * PI / WHEEL_MARKS_PER_REV / dt;   // mean w of this interval
        // Below SPINDOWN_MIN_KMH at the wheel it ends -- reaching standstill takes
        // minutes, and the slow revolutions hardly contribute.
        const double uWheel = isnan(g_circ) ? settings::wheelCircM() : g_circ;
        if (!isnan(uWheel) && w * uWheel / (2.0 * PI) * 3.6 < SPINDOWN_MIN_KMH) {
          spindownFinish();
          g_sdPrevUs = lastUs;
          g_head = (g_head + 1) % (N + 1); g_hist[g_head] = g_drumInt;
          if (g_filled < N + 1) g_filled++;
          return;
        }
        if (g_sdStartUs == 0) g_sdStartUs = prevPulseUs;
        // w applies at the interval centre; I = Int w^2 dt by trapezoid up to there.
        if (g_sdPrevW > 0.0) g_sdI += 0.5 * (g_sdPrevW * g_sdPrevW + w * w) * dt;
        const double t = (static_cast<double>(lastUs - g_sdStartUs) - 0.5e6 * dt) * 1e-6;
        g_sd.add(t, g_sdI, w);
        g_sdPrevW = w;
        g_sdRevs++;
      }
    }
  }
  g_sdPrevUs = lastUs;

  g_head = (g_head + 1) % (N + 1);
  g_hist[g_head] = g_drumInt;
  if (g_filled < N + 1) g_filled++;

  if (g_filled < N + 1) { g_circ = NAN; return; }
  const double drumRevs = g_hist[g_head] - g_hist[(g_head + 1) % (N + 1)];   // oldest
  // Slip check: the last single wheel revolution against the window (config.h).
  const double lastRev = g_hist[g_head] - g_hist[(g_head + N + 1 - WHEEL_MARKS_PER_REV) % (N + 1)];
  const double meanRev = drumRevs / static_cast<double>(WHEEL_CIRC_WINDOW_REVS);
  if (meanRev > 0.0 && fabs(lastRev / meanRev - 1.0) > WHEEL_CIRC_SLIP_REL) {
    g_circRaw = settings::drumCircumferenceM() * static_cast<float>(lastRev);
    clear();   // refills over WHEEL_CIRC_WINDOW_REVS revolutions once the wheel rolls again
    return;
  }
  g_circ = settings::drumCircumferenceM()
         * static_cast<float>(drumRevs / static_cast<double>(WHEEL_CIRC_WINDOW_REVS));
  // Plausible as in settings::setWheelCircM: 0.5 to 3 m. Turning the wheel by hand with
  // the drum standing gives 0 m -- and 0 as radius divides by zero later (2026-11-25).
  g_circRaw = g_circ;
  if (!(g_circ > 0.5f && g_circ < 3.0f)) { g_circ = NAN; return; }
  // Store, at most every 10 s and only on a change of more than 1 mm -- the NVS
  // should not be written on every revolution.
  // ... and only at steady speed: during the ramp the drum integral lags the
  // pulses, and 1428 instead of 1540 mm ended up in storage that way (2026-11-23).
  static uint32_t lastStoreMs = 0;
  const float dvdt = revsrc::dvdtKmhS();
  if (!isnan(dvdt) && fabsf(dvdt) < STEADY_DVDT_KMH_S && millis() - lastStoreMs > 10000 &&
      (isnan(settings::wheelCircM()) || fabsf(settings::wheelCircM() - g_circ) > 0.001f)) {
    settings::setWheelCircM(g_circ);
    lastStoreMs = millis();
  }
}

float jWheelKgm2() {
  // Measured (bifilar pendulum, settings) beats estimated.
  if (!isnan(settings::wheelJKgm2())) return settings::wheelJKgm2();
  const float m = settings::wheelMassKg();
  // The STORED circumference comes from steady speed; the live value is distorted by
  // slip when lifting off (1.70 instead of 1.54 m on 2026-11-23).
  const float u = isnan(settings::wheelCircM()) ? g_circ : settings::wheelCircM();
  if (isnan(m) || isnan(u)) return NAN;
  const float r = u / (2.0f * PI);
  return WHEEL_J_FACTOR * m * r * r;
}
bool jWheelMeasured() { return !isnan(settings::wheelJKgm2()); }
float spindownResidRadS() { return g_sdResidRms; }
uint32_t spindownFitN() { return g_sdFitN; }

bool spindownBegin() {
  g_sdJ = jWheelKgm2();
  if (isnan(g_sdJ)) return false;
  g_sdActive = true;
  g_sdRevs = 0;
  g_sdPrevW = 0.0;
  g_sdPrevUs = 0;
  g_sdStartUs = 0;
  g_sdI = 0.0;
  g_sd.reset();
  g_sdResidRms = NAN; g_sdFitN = 0;
  g_sdLastPulseMs = millis();
  return true;
}
bool     spindownActive() { return g_sdActive; }
uint32_t spindownRevs()   { return g_sdRevs; }

bool     valid()          { return !isnan(g_circ); }
float    circumferenceM() { return g_circ; }
float    circumferenceRawM() { return g_circRaw; }
uint32_t revsUsed()       { return g_filled ? (g_filled - 1) / WHEEL_MARKS_PER_REV : 0; }

}  // namespace wheel
