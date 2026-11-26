#include "state.h"
#include "calc/linfit.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "revsrc.h"
#include "torque.h"
#include "speedctl.h"
#include "vesc/vesc.h"
#include "logger.h"
#include "tirepressure.h"
#include "webui.h"
#include "wheel.h"

namespace state {
namespace {

Phase g_phase = IDLE;
RunMode g_run = RUN_NONE;
uint8_t g_plateau = 0;
uint8_t g_revs = 0;

// Sequence (state.h): speeds, points per plateau, callbacks.
float     g_seq[PLATEAU_MAX + 1];
uint8_t   g_seqN = 0, g_pointsPer = 1, g_k = 0;
bool      g_coastAtEnd = true;
PointFn   g_onPoint = nullptr;
SeqDoneFn g_onDone = nullptr;

// Spread within a point: mean per revolution from the partial sums of the
// accumulator (torque::sumRaw) and the speed per revolution.
struct RevStats {
  double   prevSum = 0; uint32_t prevN = 0;
  double   s = 0, ss = 0, sv = 0, svv = 0; uint8_t n = 0;
  float    dvdtMax = 0; bool fault = false;
  double   sImot = 0, sVin = 0; uint16_t nTele = 0;
  // Regression line v(t) over the whole point: its slope is the NET acceleration
  // that the inertia torque J*dw/dt leaves in the mean (docs/11-review.md K1).
  // Time relative to the start of the point, seconds.
  uint32_t t0 = 0; calc::LinFit vt;   // v over t
  void reset() { *this = RevStats(); t0 = millis(); }
  void rev() {
    const uint32_t cnt = torque::sampleCount();
    const double sum = torque::sumRaw();
    if (cnt > prevN) {
      const double m = (sum - prevSum) / (cnt - prevN);
      const double v = revsrc::kmh();
      s += m; ss += m * m; sv += v; svv += v * v; n++;
    }
    prevSum = sum; prevN = cnt;
  }
  void tick() {
    const float a = revsrc::dvdtKmhS();
    if (!isnan(a) && fabsf(a) > dvdtMax) dvdtMax = fabsf(a);
    if (vesc::values().fault != 0) fault = true;
    sImot += vesc::values().currentMotor; sVin += vesc::values().vIn; nTele++;
    const double t = (millis() - t0) / 1000.0, v = revsrc::kmh();
    vt.add(t, v);
  }
  // Slope of the regression line, km/h/s. NAN below 0.5 s of data.
  float dvdtNet() const {
    // Time span below ~0.5 s (variance T^2/12) -> NAN, that is what the degeneracy limit is for.
    return static_cast<float>(vt.slope(10, 0.02 * vt.n * vt.n));
  }
};
RevStats g_rs;
uint32_t g_settleStartMs = 0;
uint32_t g_settleSteadyMs = 0;   // since when |v - target| < SETTLE_TOL_KMH
uint32_t g_faultSinceMs = 0;
uint32_t g_faultStandstillMs = 0;   // since when the drum has been still in FAULT
float g_loadN = NAN;
const char* g_faultReason = "";

constexpr float SPEED_TOLERANCE_KMH = 0.5f;
// Idle-loss model M_idle(w) = A0 + B0 w^2: during an idle-loss run the measurement
// points (w^2, M) are collected as regression sums, at the end they are fitted and
// stored (settings::setBaseline). No point list needed.
using Fit = calc::LinFit;   // solve() requires three points, calc/linfit.h
Fit g_fit;


bool     g_wizard = false;      // wizard active (across all phases)
bool     g_wizardDone = false;  // the wizard's idle-loss run has finished
bool     g_wizardAborted = false;
uint32_t g_wizSteadyMs = 0, g_wizSlipMs = 0;
float    g_wizRatioRef = 0.0f;  // wheel/drum ratio with the wheel resting
PointFn   g_wizOnPoint = nullptr;  // callbacks of the guided run, otherwise null
SeqDoneFn g_wizOnDone  = nullptr;
float     g_wizKmh[PLATEAU_MAX];   // plateaus of the guided run
uint8_t   g_wizKmhN = 0;

void storeBaselineFit(const char* why) {
  float a0, b0;
  if (g_fit.solve(a0, b0) && settings::setBaseline(a0, b0)) {
    Serial.print("# Idle-loss model stored ("); Serial.print(why);
    Serial.print(", "); Serial.print(static_cast<int>(g_fit.n));
    Serial.print(" points): A0="); Serial.print(a0, 4);
    Serial.print(" Nm  B0="); Serial.print(b0, 7); Serial.println(" Nm/(rad/s)^2");
    Serial.println("# Enter in config.h and docs/03-parameters.md.");
  } else {
    logger::fail("Idle-loss model not stored: too few points or implausible");
  }
}

// The end of a sequence is reported only in the NEXT service() pass: the callback
// (run.cpp) starts sequences there and stops the drive, and that must not happen
// while finishPoint() is still on the stack (since 2026-11-25).
SeqDoneFn g_pendingDone = nullptr; bool g_pendingCompleted = false, g_pendingSet = false;
void endSequence(bool completed) {
  const SeqDoneFn fn = g_onDone;
  g_onPoint = nullptr; g_onDone = nullptr; g_seqN = 0;
  if (fn) { g_pendingDone = fn; g_pendingCompleted = completed; g_pendingSet = true; }
}
void deliverPending() {
  if (!g_pendingSet) return;
  g_pendingSet = false;
  const SeqDoneFn fn = g_pendingDone; g_pendingDone = nullptr;
  if (fn) fn(g_pendingCompleted);
}

// coast: only release the drive instead of braking. This applies to "operator gone"
// (docs/07-safety.md §4, docs/10-firmware.md W3: the run aborts, the drum coasts
// down) -- until 2026-11-25 this case braked with 20 A and also reported
// "control loop switched off", because loop() had already released.
void enterFault(const char* reason, bool coast = false) {
  // An idle-loss run with at least three points is a result even when aborted.
  if (g_run == RUN_BASELINE && g_fit.n >= 3) storeBaselineFit("abort");
  g_faultReason = reason;
  g_phase = FAULT;
  g_run = RUN_NONE;
  endSequence(false);
  if (g_wizard) g_wizardAborted = true;
  if (coast) speedctl::coast(); else speedctl::stop();
  logger::fail(reason);
}

// Shutdown criteria. See docs/07-safety.md §4.
void checkFaults() {
  const char* reason = nullptr;

  // Standstill despite a setpoint. Without drum marks speed::stalled() cannot see
  // this, and a revolution calculated from the controller speed never "stalls" —
  // hence here via duty and speed. The finding of 2026-11-18: 0.045 duty,
  // 2.5 A motor current, drum still. That is commutation.
  const bool standstill = speedctl::stalledUnderDuty();

  // Short; the user interface shows the text as it is.
  if (torque::isSaturated())        reason = "FAULT: load cell saturated";
  else if (!torque::isResponding()) reason = "FAULT: HX711 not responding";
  else if (isCalibrated() && fabsf(torque::newtons()) > MAX_FORCE_N)
                                    reason = "FAULT: force limit";
  else if (revsrc::revPerSec() * 60.0f > MAX_DRUM_RPM)
                                    reason = "FAULT: speed limit";
  // Operator gone FIRST: loop() has then already released the drive, and the
  // dropout criterion below would otherwise name the wrong cause.
  else if (!webui::linkAlive())     reason = "FAULT: operator gone";
  else if (!vesc::connected())      reason = "FAULT: no controller";
  else if (vesc::values().fault != 0) {
    static char rbuf[64];
    snprintf(rbuf, sizeof(rbuf), "FAULT: controller: %s", vesc::faultText(vesc::values().fault));
    reason = rbuf;
  }
  // The control loop switched itself off (dropout: no controller values, emergency-stop
  // path) while this side is still driving: without this case the machine stayed in
  // POINT on 2026-11-24, and the guided run waited forever for its point.
  else if (!speedctl::running() && (g_phase == SPINUP || g_phase == SETTLE || g_phase == POINT ||
                                    g_phase == HOLD || g_phase == WIZ_SPINUP))
                                    reason = "FAULT: control loop switched off";
  else if (speedctl::estopInput())  reason = "FAULT: emergency stop";
  else if (speedctl::motorOpen())   reason = "FAULT: disconnect detected";
  else if (standstill)              reason = "FAULT: drum not turning";
  // Check belt slip and missing speed pulses only when the drum marks really count.
  // Against their own calculated source both would be tautological.
  // And only from STALL_DUTY_MIN: when starting, the ramp is already running while
  // the drum still stands -- without this threshold the FAULT tripped after 2 s,
  // before it broke away (2026-11-25, first run with counting marks).
  else if (revsrc::angleReference() && speedctl::rampKmh() > STANDSTILL_KMH &&
           speedctl::duty() > STALL_DUTY_MIN && speed::stalled())
                                    reason = "FAULT: no speed pulses";
  else if (revsrc::angleReference() && speed::slipDetected())
                                    reason = "FAULT: belt slip";

  if (reason == nullptr) {
    g_faultSinceMs = 0;
    return;
  }
  // Debounce: short outliers should not shut down.
  const uint32_t now = millis();
  if (g_faultSinceMs == 0) { g_faultSinceMs = now; return; }
  if (now - g_faultSinceMs >= FAULT_SETTLE_MS) enterFault(reason, !webui::linkAlive());
}

void gotoPlateau(uint8_t idx) {
  g_plateau = idx;
  g_k = 0;
  if (!speedctl::setTargetKmh(g_seq[idx])) {
    enterFault(speedctl::lastRefusal());
    return;
  }
  g_phase = SPINUP;
}

// Standard runs (m0/m1): all plateaus from the settings, one point per plateau,
// then coast down.
void startRun(RunMode mode) {
  float kmh[PLATEAU_MAX];
  const uint8_t n = settings::plateauCount();
  for (uint8_t i = 0; i < n; i++) kmh[i] = settings::plateauKmh(i);
  startSequence(mode, kmh, n, 1, true, nullptr, nullptr);
}

// Effective moment of inertia at the drum shaft: drum, and in the measurement run
// the resting wheel, converted with the square of the radius ratio.
float effectiveInertia() {
  float j = DRUM_INERTIA_KGM2;
  if (g_run == RUN_MEASURE) {
    const float jr = wheel::jWheelKgm2(), uWheel = settings::wheelCircM();
    if (!isnan(jr) && !isnan(uWheel) && uWheel > 0.1f) {
      const float q = settings::drumRadiusM() / (uWheel / (2.0f * PI));
      j += jr * q * q;
    }
  }
  return j;
}

void finishPoint() {
  const float v = revsrc::kmh();
  const float raw = torque::averageRaw();
  const float fN = torque::averageN();
  const float mRawNm = isnan(fN) ? NAN : fN * settings::torqueArmM();
  // Inertia correction (docs/09-measurement-theory.md eq. 14, docs/12 K1): the net
  // acceleration over the point produces J*dw/dt in the mean. Subtract it, and discard
  // the point only if the residual (UNC_J_REL of it) would be above 1 %.
  const float dvdtNet = g_rs.dvdtNet();
  const float mInert = isnan(dvdtNet) ? 0.0f
      : effectiveInertia() * (dvdtNet / 3.6f / settings::drumRadiusM());
  const float mNm = isnan(mRawNm) ? NAN : mRawNm - mInert;
  const float pW = isnan(mNm) ? NAN : mNm * revsrc::omega();
  const float fRoll = isnan(mNm) ? NAN : mNm / settings::drumRadiusM();

  // Crr only in the measurement run and only with a known wheel load. In the BASELINE
  // run it is meaningless; the subtraction is done by the evaluation, not the firmware.
  float crr = NAN;
  if (g_run == RUN_MEASURE && !isnan(fRoll) && !isnan(g_loadN) && g_loadN > 0.0f) {
    crr = fRoll / g_loadN;
  }

  logger::point(name(), revsrc::revCount(), v, raw, fN, mNm, pW, crr,
                g_loadN, tirepressure::bar(),
                g_run == RUN_BASELINE ? "baseline" : "measure");
  if (revsrc::angleReference()) logger::angleBins();
  if (g_run == RUN_BASELINE && !isnan(mNm)) {
    const double w = revsrc::omega();
    g_fit.add(w * w, mNm);
  }

  // Spread and validity to the caller of the sequence (run.cpp).
  if (g_onPoint) {
    PointStats ps;
    const float sdRaw = calc::sdFromSums(g_rs.s, g_rs.ss, g_rs.n);
    ps.v   = g_rs.n ? static_cast<float>(g_rs.sv / g_rs.n) : v;
    ps.vSd = calc::sdFromSums(g_rs.sv, g_rs.svv, g_rs.n);
    ps.raw = raw;
    ps.mNm = mNm; ps.mRawNm = mRawNm; ps.mInertNm = mInert; ps.dvdtNet = dvdtNet;
    const float nmPerCount = settings::calibrated() ? settings::loadcellNPerCount() * settings::torqueArmM() : NAN;
    ps.sdNm = isnan(sdRaw) ? NAN : sdRaw * nmPerCount;
    ps.rippleNm = torque::rippleRaw() * nmPerCount;
    ps.realRevs = revsrc::realRevolutions();
    // Without a real index the window is off by up to one revolution; of a first-order
    // disturbance with amplitude a, up to a/N then remains.
    ps.uSyncNm = (ps.realRevs || isnan(ps.rippleNm) || g_revs == 0) ? 0.0f : ps.rippleNm / 2.0f / g_revs;
    ps.dvdtMax = g_rs.dvdtMax;
    ps.imot = g_rs.nTele ? static_cast<float>(g_rs.sImot / g_rs.nTele) : NAN;
    ps.vin  = g_rs.nTele ? static_cast<float>(g_rs.sVin / g_rs.nTele) : NAN;
    ps.tfet = vesc::values().tempFet;
    ps.tmot = vesc::values().tempMotor;
    ps.revs = g_revs; ps.plateau = g_plateau; ps.k = g_k;
    ps.ok = !g_rs.fault && !isnan(dvdtNet) && fabsf(dvdtNet) < POINT_DVDT_NET_MAX_KMH_S
            && fabsf(ps.v - g_seq[g_plateau]) < STEADY_TOL_KMH;
    ps.tMs = millis();
    g_onPoint(ps);
  }

  if (g_k + 1 < g_pointsPer) {
    // Next point on the same plateau, without settling again.
    g_k++;
    torque::resetAccumulator();
    g_rs.reset();
    g_revs = 0;
    return;
  }
  if (g_plateau + 1 < g_seqN) {
    gotoPlateau(g_plateau + 1);
  } else {
    if (g_run == RUN_BASELINE) storeBaselineFit("complete");
    if (g_wizard) g_wizardDone = true;
    g_run = RUN_NONE;
    if (g_coastAtEnd) {
      logger::note("Run finished — drum coasting down");
      speedctl::coast();   // coast down freely, do not brake (docs/05-test-procedure.md §B8)
      g_phase = COAST;
    } else {
      g_phase = HOLD;      // stays at the last speed, run.cpp decides
    }
    endSequence(true);
  }
}

}  // namespace

void begin() {
  g_phase = IDLE;
  g_run = RUN_NONE;
  g_loadN = settings::loadN();   // default LOAD_DEFAULT_N or the stored value
}

void service() {
  // The main loop reads and accumulates the force values — it calls
  // torque::update() and is thus the only one that knows when a new raw value is there.
  //
  // The index mark counts the revolutions of the current measurement point.
  const bool indexSeen = revsrc::consumeIndexFlag();
  deliverPending();   // sequence end from the last pass

  if (g_phase != IDLE && g_phase != FAULT && g_phase != TARE) checkFaults();   // also WIZ_*

  switch (g_phase) {
    case IDLE:
      break;

    case FAULT:
      // Holds until the drum stands still; then back to IDLE by itself. A latching
      // fault that someone must acknowledge was only a nuisance at the stand (2026-11-23).
      // While the drum is still turning, it stays and remains visible.
      if (revsrc::revPerSec() < STANDSTILL_RPS) {
        if (g_faultStandstillMs == 0) g_faultStandstillMs = millis();
        else if (millis() - g_faultStandstillMs >= FAULT_CLEAR_STILL_MS) {
          g_faultReason = "";
          g_faultSinceMs = 0;
          g_faultStandstillMs = 0;
          g_phase = IDLE;
          logger::note("");
        }
      } else {
        g_faultStandstillMs = 0;
      }
      break;

    case TARE:
      if (!torque::tareBusy()) {
        logger::note("Static tare set");
        g_phase = IDLE;
      }
      break;

    case SPINUP:
      if (fabsf(revsrc::kmh() - speedctl::rampKmh()) < SPEED_TOLERANCE_KMH) {
        g_settleStartMs = millis();
        g_settleSteadyMs = 0;
        g_phase = SETTLE;
      }
      break;

    case SETTLE: {
      // Let it settle after the plateau change: residual acceleration of the flywheel
      // mass and oscillations of the weighing part decay here. It is done not by the
      // clock, but when the actual value was within SETTLE_TOL_KMH of the target for
      // SETTLE_STEADY_MS (at least SETTLE_MS, at most SETTLE_MAX_MS).
      const uint32_t now = millis();
      const float dev = fabsf(revsrc::kmh() - speedctl::rampKmh());
      if (dev < SETTLE_TOL_KMH) { if (g_settleSteadyMs == 0) g_settleSteadyMs = now; }
      else g_settleSteadyMs = 0;
      const bool steady = g_settleSteadyMs && now - g_settleSteadyMs >= SETTLE_STEADY_MS;
      if ((now - g_settleStartMs >= SETTLE_MS && steady) || now - g_settleStartMs >= SETTLE_MAX_MS) {
        if (!steady) logger::note("Settling: target not reached, point judged by dv/dt");
        torque::resetAccumulator();
        g_rs.reset();
        g_revs = 0;
        g_phase = POINT;
      } else if (dev > SPEED_TOLERANCE_KMH * 2) {
        g_phase = SPINUP;   // drifted off again
      }
      break;
    }

    case POINT:
      g_rs.tick();
      if (indexSeen) {
        g_rs.rev();
        g_revs++;
        if (g_revs >= AVG_REVS_PER_POINT) finishPoint();
      }
      break;

    case HOLD:
      break;   // driving, shutdown criteria run above

    case COAST:
      if (revsrc::revPerSec() < STANDSTILL_RPS) {
        logger::note("Drum still");
        g_phase = IDLE;
      }
      break;

    case WIZ_SPINUP:
      // With the wheel resting to WIZ_KMH, then 3 s steady -> lifting allowed.
      if (fabsf(revsrc::kmh() - WIZ_KMH) < WIZ_TOL_KMH) {
        if (g_wizSteadyMs == 0) g_wizSteadyMs = millis();
        // ... and only once the circumference from this drive is known: the spin-down needs J_wheel.
        else if (millis() - g_wizSteadyMs > WIZ_STEADY_MS && speed::wheelRevPerSec() > WIZ_WHEEL_MIN_RPS &&
                 !isnan(wheel::circumferenceM())) {
          // Remember the circumference from THIS drive: the window (WHEEL_CIRC_WINDOW_REVS, at
          // 40 km/h 1.4 s) lies entirely within the WIZ_STEADY_MS of steady driving. J_wheel in the spin-down uses it.
          settings::setWheelCircM(wheel::circumferenceM());
          g_wizRatioRef = speed::wheelRevPerSec() / revsrc::revPerSec();
          g_wizSlipMs = 0;
          g_phase = WIZ_LIFT;
          logger::note("Lift wheel now");
        }
      } else {
        g_wizSteadyMs = 0;
      }
      break;

    case WIZ_LIFT: {
      // Wheel turns slower than it would via the drum -> lifted.
      const float ratio = (revsrc::revPerSec() > 0.5f)
          ? speed::wheelRevPerSec() / revsrc::revPerSec() / g_wizRatioRef : 1.0f;
      if (ratio < WIZ_SLIP_RATIO) {
        if (g_wizSlipMs == 0) g_wizSlipMs = millis();
        else if (millis() - g_wizSlipMs > WIZ_LIFT_CONFIRM_MS) {
          logger::note("Wheel lifted: spin-down and idle loss running");
          wheel::spindownBegin();
          if (g_wizOnPoint || g_wizOnDone) {
            // Guided run: points and end to run.cpp, HOLD at the end instead of coasting.
            float kmh[PLATEAU_MAX];
            uint8_t n = g_wizKmhN;
            for (uint8_t i = 0; i < n; i++) kmh[i] = g_wizKmh[i];
            if (!n) { n = settings::plateauCount(); for (uint8_t i = 0; i < n; i++) kmh[i] = settings::plateauKmh(i); }
            if (!startSequence(RUN_BASELINE, kmh, n, RUN_BASELINE_POINTS, false, g_wizOnPoint, g_wizOnDone)) {
              g_wizardAborted = true;
              stopRun();
            }
          } else {
            startRun(RUN_BASELINE);     // idle-loss run, ramp to the first plateau
          }
        }
      } else {
        g_wizSlipMs = 0;
      }
      break;
    }
  }
}

Phase phase() { return g_phase; }
RunMode runMode() { return g_run; }

const char* name() {
  switch (g_phase) {
    case IDLE:   return "IDLE";
    case TARE:   return "TARE";
    case SPINUP: return "SPINUP";
    case SETTLE: return "SETTLE";
    case POINT:  return "POINT";
    case COAST:  return "COAST";
    case WIZ_SPINUP: return "WIZ_SPINUP";
    case WIZ_LIFT:   return "WIZ_LIFT";
    case HOLD:   return "HOLD";
    case FAULT:  return "FAULT";
  }
  return "?";
}

void startBaseline() { startRun(RUN_BASELINE); }

bool startSequence(RunMode mode, const float* kmh, uint8_t n, uint8_t pointsPer,
                   bool coastAtEnd, PointFn onPoint, SeqDoneFn onDone) {
  if (g_phase != IDLE && g_phase != WIZ_LIFT && g_phase != HOLD) {
    logger::fail("Start refused: not IDLE");
    return false;
  }
  if (mode == RUN_MEASURE && isnan(g_loadN)) {
    logger::fail("Start refused: wheel load not set (command 'l<N>')");
    return false;
  }
  if (n == 0 || n > PLATEAU_MAX + 1) {
    logger::fail("Start refused: no measurement speeds (command 'h<kmh>')");
    return false;
  }
  for (uint8_t i = 0; i < n; i++) g_seq[i] = kmh[i];
  g_seqN = n;
  g_pointsPer = pointsPer ? pointsPer : 1;
  g_coastAtEnd = coastAtEnd;
  g_onPoint = onPoint;
  g_onDone = onDone;
  g_run = mode;
  g_fit = Fit();
  logger::note(mode == RUN_BASELINE ? "Idle loss — wheel lifted"
                                    : "Measurement run");
  // Where the speed comes from is logged ONCE per run — not per point.
  if (!revsrc::angleReference()) {
    logger::note(revsrc::realRevolutions()
                 ? "Speed source: controller, revolutions from the index mark. No angle bins."
                 : "Speed source: controller (ERPM/pole pairs). Revolutions CALCULATED, no angle reference.");
  }
  gotoPlateau(0);
  return g_phase == SPINUP;
}

bool startHold(float kmh) {
  if (g_phase != IDLE && g_phase != HOLD) {
    logger::fail("Start refused: not IDLE");
    return false;
  }
  if (!speedctl::setTargetKmh(kmh)) { logger::fail(speedctl::lastRefusal()); return false; }
  g_phase = HOLD;
  return true;
}

void endHold() { if (g_phase == HOLD) g_phase = IDLE; }

void startWizard(PointFn onPoint, SeqDoneFn onDone, const float* kmh, uint8_t n) {
  g_wizOnPoint = onPoint; g_wizOnDone = onDone;
  g_wizKmhN = 0;
  for (uint8_t i = 0; kmh && i < n && i < PLATEAU_MAX; i++) g_wizKmh[g_wizKmhN++] = kmh[i];
  if (g_phase != IDLE) { logger::fail("Start refused: not IDLE"); return; }
  // The circumference is only measured during spin-up (WIZ_SPINUP); here only require
  // the mass or a measured J. Until 2026-11-25 the start refused without a stored circumference.
  if (!wheel::jWheelMeasured() && isnan(settings::wheelMassKg())) {
    logger::fail("Start refused: wheel mass missing"); return;
  }
  if (settings::plateauCount() == 0) { logger::fail("Start refused: no measurement speeds"); return; }
  if (!speedctl::setTargetKmh(WIZ_KMH)) { logger::fail(speedctl::lastRefusal()); return; }
  g_wizard = true;
  g_wizardDone = false;
  g_wizardAborted = false;
  g_wizSteadyMs = 0;
  g_phase = WIZ_SPINUP;
  logger::note("Wizard: drum spinning up, wheel stays down");
}

uint8_t wizardStep() {
  if (!g_wizard) return 0;
  if (g_wizardAborted) return 9;
  if (g_phase == WIZ_SPINUP) return 1;
  if (g_phase == WIZ_LIFT) return 2;
  if (g_wizardDone && !wheel::spindownActive()) return 4;
  return 3;
}

void wizardAck() { g_wizard = false; g_wizardDone = false; g_wizardAborted = false; g_wizOnPoint = nullptr; g_wizOnDone = nullptr; }
void startMeasure()  { startRun(RUN_MEASURE); }

void startTare() {
  if (g_phase != IDLE) { logger::fail("Tare only at standstill"); return; }
  if (revsrc::revPerSec() > STANDSTILL_RPS) { logger::fail("Tare only at standstill"); return; }
  torque::tareBegin();
  g_phase = TARE;
}

void stopRun() {
  speedctl::stop();
  g_run = RUN_NONE;
  if (g_wizard && !g_wizardDone) g_wizardAborted = true;
  if (g_phase != FAULT) g_phase = COAST;
  endSequence(false);
}

void coastRun() {
  speedctl::coast();
  g_run = RUN_NONE;
  if (g_wizard && !g_wizardDone) g_wizardAborted = true;
  if (g_phase != FAULT) g_phase = COAST;
  endSequence(false);
}

void clearFault() {
  if (g_phase != FAULT) return;
  if (revsrc::revPerSec() > STANDSTILL_RPS) {
    logger::fail("Acknowledge only at standstill");
    return;
  }
  g_faultReason = "";
  g_faultSinceMs = 0;
  g_phase = IDLE;
  logger::note("Fault acknowledged");
}

void setLoadN(float n) { g_loadN = n; settings::setLoadN(n); }
float loadN() { return g_loadN; }

uint8_t plateauIndex() { return g_plateau; }
uint8_t pointIndex() { return g_k; }
uint8_t seqLength() { return g_seqN; }
uint8_t pointsPerPlateau() { return g_pointsPer; }
uint32_t settleElapsedMs() { return g_phase == SETTLE ? millis() - g_settleStartMs : 0; }
uint8_t revsInPoint() { return g_revs; }
const char* faultReason() { return g_faultReason; }

}  // namespace state
