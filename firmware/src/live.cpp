#include "live.h"


#include "config.h"
#include "calc/linfit.h"
#include "calc/loss.h"
#include "calc/json.h"
#include "settings.h"
#include "webui.h"
#include "statusled.h"
#include "torque.h"
#include "speedctl.h"
#include "revsrc.h"
#include "speed.h"
#include "state.h"
#include "logger.h"
#include "tirepressure.h"
#include "vesc/vesc.h"
#include "vesc/packet.h"
#include "wheel.h"
#include "run.h"

namespace live {
namespace {

// ---------------------------------------------------------- Live at steady speed
// Power loss and Crr only when the speed is steady (config.h STEADY_*), averaged
// over LIVE_WINDOW_MS: ring of the cell raw values and the angular velocity,
// filled on every new cell value (80 per second).
constexpr uint16_t LIVE_N = LIVE_WINDOW_MS * HX711_RATE_SPS / 1000;
long     g_liveRaw[LIVE_N];
float    g_liveW[LIVE_N];
uint32_t g_liveT[LIVE_N];
uint16_t g_liveHead = 0, g_liveCount = 0;
uint32_t g_steadySince = 0;
bool     g_steady = false;

void liveReset() { g_liveCount = 0; g_liveHead = 0; }


}  // namespace

// Every pass: check steadiness. On every new cell value: store it.
// Steady here only means: drive running, ramp arrived, actual value within the tolerance band.
// liveResult() checks the acceleration NET over the window (config.h, STEADY_*).
void service(bool newSample) {
  const bool steadyNow = speedctl::running()
      && fabsf(revsrc::kmh() - speedctl::targetKmh()) < STEADY_TOL_KMH
      && speedctl::rampKmh() >= speedctl::targetKmh() - 0.01f;   // ramp arrived
  const uint32_t now = millis();
  if (!steadyNow) { g_steadySince = 0; g_steady = false; liveReset(); return; }
  if (g_steadySince == 0) g_steadySince = now;
  g_steady = (now - g_steadySince) >= STEADY_HOLD_MS;
  if (!g_steady) { liveReset(); return; }
  if (newSample) {
    g_liveRaw[g_liveHead] = torque::lastRaw();
    g_liveW[g_liveHead]   = revsrc::omega();
    g_liveT[g_liveHead]   = now;
    g_liveHead = (g_liveHead + 1) % LIVE_N;
    if (g_liveCount < LIVE_N) g_liveCount++;
  }
}


bool steady() { return g_steady; }

// Result: all NAN while not steady, not calibrated or window empty.
// Corrected for idle and wheel losses as far as the models exist.
Result result() {
  Result r = {NAN, NAN, NAN, false, g_liveCount, NAN};
  if (!g_steady || g_liveCount < LIVE_MIN_N || !settings::calibrated()) return r;
  // Only the values of the last LIVE_WINDOW_MS; with a slow cell the ring can be older.
  const uint32_t now = millis();
  double sraw = 0, sw = 0; calc::LinFit wt; uint16_t n = 0; uint32_t tMin = now, tMax = 0;
  for (uint16_t i = 0; i < g_liveCount; i++) {
    if (now - g_liveT[i] > LIVE_WINDOW_MS) continue;
    sraw += g_liveRaw[i]; sw += g_liveW[i]; wt.add(static_cast<double>(g_liveT[i]) / 1000.0, g_liveW[i]); n++;
    if (g_liveT[i] < tMin) tMin = g_liveT[i];
    if (g_liveT[i] > tMax) tMax = g_liveT[i];
  }
  r.n = n;
  if (n < 3 || tMax - tMin < LIVE_MIN_MS) return r;
  // Net acceleration over the window: slope of the regression line omega(t), in km/h/s.
  const double b = wt.slope(3, 0.0);
  if (!isnan(b)) r.dvdtNet = static_cast<float>(b) * settings::drumRadiusM() * 3.6f;
  if (isnan(r.dvdtNet) || fabsf(r.dvdtNet) >= STEADY_DVDT_KMH_S) return r;
  const float raw = static_cast<float>(sraw / n);
  const float w   = static_cast<float>(sw / n);
  float m = (raw - static_cast<float>(torque::tareOffset())) * settings::loadcellNPerCount()
            * settings::torqueArmM();
  // The STORED circumference comes from steady driving (wheel.cpp); the live value only
  // as a substitute while none is stored yet.
  const float uWheel = !isnan(settings::wheelCircM()) ? settings::wheelCircM() : wheel::circumferenceM();
  m -= calc::lossNm(w, settings::drumRadiusM(), settings::baselineA0(), settings::baselineB0(), settings::haveBaseline(),
                    settings::wheelA1(), settings::wheelB1(), settings::haveWheelModel(), uWheel);
  r.corrected = settings::haveBaseline();
  r.mNm = m;
  r.pW  = m * w;
  const float load = state::loadN();
  if (!isnan(load) && load > 0.0f) r.crr = m / settings::drumRadiusM() / load;
  return r;
}

// Live values for the guided measurement run (run.h): warm-up course per minute.
bool forRun(run::LiveVals& out) {
  const Result lv = result();
  out.mNm = lv.mNm; out.pW = lv.pW; out.crr = lv.crr; out.corr = lv.corrected;
  return !isnan(lv.pW);
}

}  // namespace live
