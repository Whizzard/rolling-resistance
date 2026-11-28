#include "speedctl.h"
#include "config.h"
#include "settings.h"
#include "vesc/vesc.h"
#include "revsrc.h"
#include "drivespeed.h"
#include "brake.h"
#include "logger.h"
#include "calc/loss.h"
#include "torque.h"

namespace speedctl {
namespace {

float    g_target = 0.0f;   // requested km/h
float    g_ramp   = 0.0f;   // ramped inner setpoint
float    g_duty   = 0.0f;   // magnitude of the output, the sign comes from config
bool     g_run    = false;
bool     g_fresh  = true;   // preset from the actual speed on the next pass
uint32_t g_lastMs = 0;
const char* g_refusal = "";
bool     g_learn  = false;  // direction is being learned right now
uint32_t g_learnStart = 0;
bool     g_brk    = false;  // breakaway measurement running
bool     g_dirDet = false;  // direction detection running (on top of g_brk)
float    g_dirBase = NAN, g_dirDelta = NAN;
int      g_dirRes = -2;
float    g_brkDuty = 0.0f;  // result: duty cycle at breakaway
float    g_brkAmps = 0.0f;
bool     g_motorOpen = false;   // finding "no motor on the controller", latching
float    g_out = 0.0f;          // last output sent (I + P + D)
bool     g_coasting = false;    // descent: released, the drum coasts down to the setpoint
bool     g_descBrake = false;   // descent: the relays are braking right now (config.h DESCENT IN THE RUN)
bool     g_descDone  = false;   // already braked for this setpoint: never twice
float    g_descFrom  = NAN;     // actual speed when braking started, for the console
bool     g_stopping = false;    // STOP running: brake to standstill, then release

// Braking (config.h BRAKE_*): above BRAKE_MAX_KMH only release and let it coast --
// entering braking from high speed feeds into the bus for milliseconds, and the
// power supply absorbs nothing (fault 1 at 56...59 km/h, 2026-11-23).
// Below that 20 A braking current; that is above the short-circuit current, so the
// controller ends up in a short and nothing goes into the bus. Duty cycle 0 does
// NOT short directly in FOC (PI ramp with regeneration), docs/07-safety.md §3.2d.
// After a controller fault do not re-apply for BRAKE_FAULT_HOLD_MS: that was the
// tack-tack-tack -- every restart at almost the same speed failed again.
// Only for the STOP (command s, FAULT, run finished): with RELAY_BRAKE_WIRED the
// relay brake, at any speed (decision 2026-11-25, config.h). The controller path
// below only applies without relays. A descent during a run never brakes, it coasts.
void applyBrake(float actualKmh) {
  if (brake::active()) { vesc::release(); return; }          // relays brake, controller stays released
  if (brake::available()) {
    vesc::release();                                          // first disable the driver (N2) ...
    brake::engage();                                          // ... then the relays after RELAY_DRIVER_OFF_MS
    return;
  }
  if (actualKmh > BRAKE_MAX_KMH ||
      vesc::values().fault != 0 || vesc::lastFaultAgeMs() < BRAKE_FAULT_HOLD_MS) {
    vesc::release();
    return;
  }
  vesc::setBrakeCurrent(VESC_BRAKE_CURRENT_STOP);
}
uint32_t g_openSince = 0;

// Mechanical drum speed from the controller speed. Signed:
// in the measuring direction ERPM is negative (VESC_DIRECTION_SIGN).
// NAN without a connection: otherwise the last value stays, and after a
// controller crash (docs/07-safety.md §4) the stand showed a spinning drum that
// had long stopped -- or the other way round. The control loop checks connected()
// itself before reading here.
float rpmSigned() { return drivespeed::rpm(); }

// km/h in the measuring direction, i.e. positive when the drum turns the right way.
float kmhSigned() { return drivespeed::kmh(); }

float rpmFromKmh(float kmh) { return revPerSecFromKmh(kmh) * 60.0f; }

// Every release the control makes ON ITS OWN is reported: on the console and via
// logger::lastNote() in the UI. Before, this happened silently, and two aborted
// starts on 2026-11-23 could not be explained.
void dropout(const char* why) {
  g_run = false;
  g_coasting = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  vesc::release();
  g_refusal = why;
  logger::fail(why);
}
// At standstill without a setpoint the finding expires by itself, together with the message.
void clearMotorOpenIfStopped() {
  if (g_motorOpen && !g_run && fabsf(rpmSigned()) < STANDSTILL_RPM) {
    g_motorOpen = false;
    logger::note("");
  }
}

// No-load speed at full duty and the CURRENT bus voltage.
//
// Including the voltage is not polish: on 2026-11-17 the power supply sagged under
// load from 40.0 to 33.8 V, and a clamp computed for 40 V then left the run-up
// stuck at 94 km/h.
float noLoadRpm() {
  const float v = vesc::values().vIn;
  return MOTOR_NO_LOAD_RPM_PER_V * (v > NOLOAD_VIN_FLOOR_V ? v : NOLOAD_VIN_FLOOR_V);
}

}  // namespace

void finishDirection();   // below, at the breakaway functions

void begin() {
  g_target = g_ramp = g_duty = 0.0f;
  g_run = false;
  g_fresh = true;
  g_lastMs = millis();
  g_refusal = "";
  g_motorOpen = false;
  g_openSince = 0;
  if (ESTOP_FB_WIRED) pinMode(PIN_ESTOP_FB, INPUT_PULLUP);
}

// Coast-down time from vFrom to vTo [km/h] from the loss model: no-load (stored, otherwise
// DRUM_LOSS_*) and wheel, midpoint rule. Only for the decision "is braking worth it".
float coastTimeS(float vFrom, float vTo) {
  const bool base = settings::haveBaseline();
  const float a0 = base ? settings::baselineA0() : DRUM_LOSS_CONST_NM;
  const float b0 = base ? settings::baselineB0() : DRUM_LOSS_OMEGA2_NM;
  const float r  = settings::drumRadiusM();
  const float uW = settings::wheelCircM();
  constexpr int N = 8;
  const float dv = (vFrom - vTo) / N;
  float t = 0.0f;
  for (int i = 0; i < N; i++) {
    const float w = (vFrom - (i + 0.5f) * dv) * KMH_TO_MS / r;
    const float m = calc::lossNm(w, r, a0, b0, true, settings::wheelA1(), settings::wheelB1(),
                                 settings::haveWheelModel(), uW);
    if (!(m > 0.0f)) return INFINITY;
    const float aKmhS = m / DRUM_INERTIA_KGM2 * r * MS_TO_KMH;
    t += dv / aKmhS;
  }
  return t;
}

// Drum speed that can be trusted: the marks, otherwise the controller. The controller
// does not see the drum while the relays short the motor, and after they pull in its
// speed lags behind -- on 2026-11-25 it read 63 instead of 82 km/h 0.4 s after the
// release, the control picked up there, regenerated (45.3 V) and threw overvoltage
// on the next release (fault 1, 46.9 V).
float drumKmh() { return revsrc::angleReference() ? revsrc::kmh() : kmhSigned(); }

// Lead for the release: deceleration times latency plus the intended coast-down.
float descentLeadKmh() {
  const float d = revsrc::dvdtKmhS();
  const float decel = (!isnan(d) && -d > DESCENT_BRAKE_DECEL_KMH_S) ? -d : DESCENT_BRAKE_DECEL_KMH_S;
  return decel * DESCENT_RELEASE_LATENCY_S + DESCENT_COAST_REMAIN_KMH;
}

bool setTargetKmh(float kmh) {
  // New target while the relays brake in a descent: only update the target, the
  // loop releases as soon as it is reached (if it lies above the actual: at once).
  if (g_descBrake && kmh > 0.0f && kmh <= V_MAX_KMH) { g_refusal = ""; g_target = kmh; g_descDone = true; return true; }
  g_refusal = "";
  g_motorOpen = false;
  g_openSince = 0;
  g_coasting = false;
  g_stopping = false;
  g_descDone = false;

  if (kmh <= 0.0f) { coast(); return true; }

  // Relay brake: shorted, nothing drives, and freshly closed contacts bounce.
  if (brake::active()) { g_refusal = "Relay brake active"; return false; }
  if (brake::available() && brake::releasedForMs() < RELAY_SETTLE_MS) { g_refusal = "Relays settling"; return false; }

  if (kmh > V_MAX_KMH) {
    g_refusal = "Above V_MAX_KMH (docs/07-safety.md §5)";
    return false;
  }

  // Reversing direction while running is a brake command (docs/07-safety.md §3.3).
  if (kmhSigned() < -STANDSTILL_KMH) {
    g_refusal = "Drum turning backwards";
    return false;
  }

  if (!g_run) g_fresh = true;   // starting: preset the output from the actual speed
  g_target = kmh;
  g_run    = true;
  return true;
}

void service() {
  const uint32_t now = millis();
  if (now - g_lastMs < SPEEDCTL_PERIOD_MS) return;
  const float dt = (now - g_lastMs) / 1000.0f;
  g_lastMs = now;

  if (g_learn) {
    // Measure sign-free: which sign does the controller speed have when the
    // human turns in the measuring direction? That is exactly directionSign().
    const float raw = kmhFromRevPerSec(rpmSigned() / 60.0f);
    if (fabsf(raw) >= SPEEDCTL_LEARN_MIN_KMH) {
      const int8_t sign = (raw > 0.0f) ? 1 : -1;
      g_learn = false;
      g_refusal = settings::setDirectionSign(sign)
                    ? "" : "Sign not saved";
      return;
    }
    if (now - g_learnStart > SPEEDCTL_LEARN_TIMEOUT_MS) {
      g_learn = false;
      g_refusal = "No rotation, learning aborted";
    }
    return;
  }

  if (g_brk) {
    if (!vesc::connected()) { g_brk = false; dropout("Abort: no controller data"); return; }
    const float actualKmh = kmhSigned();
    // Direction detection: the cell change against the start, judged at breakaway --
    // or already as soon as it exceeds the threshold (the drum need not move at all).
    if (g_dirDet) {
      const float f = torque::newtons();
      if (!isnan(f) && !isnan(g_dirBase)) g_dirDelta = f - g_dirBase;
    }
    const bool dirEnough = g_dirDet && !isnan(g_dirDelta) && fabsf(g_dirDelta) >= DIRDETECT_MIN_N;
    if (fabsf(actualKmh) >= BREAKAWAY_DETECT_KMH || dirEnough) {
      g_brkDuty = g_duty;
      g_brkAmps = vesc::values().currentMotor;
      g_brk = false;
      coast();
      if (g_dirDet) finishDirection();
      return;
    }
    g_duty += BREAKAWAY_RATE_PER_S * dt;
    if (g_duty > BREAKAWAY_DUTY_MAX) {
      g_brk = false;
      g_brkDuty = 0.0f;          // no breakaway -- no invented number
      g_brkAmps = vesc::values().currentMotor;
      coast();
      g_refusal = "No breakaway up to limit";
      if (g_dirDet) finishDirection();
      return;
    }
    vesc::setDuty(g_duty * settings::directionSign());
    return;
  }

  // STOP: track the brake with speed, release at standstill.
  if (g_stopping) {
    const float v = fabsf(kmhSigned());
    if (brake::active()) { vesc::release(); return; }        // relays brake down to standstill
    if (isnan(v) || v < STANDSTILL_KMH) { g_stopping = false; vesc::release(); }
    else                                { applyBrake(v); }
    return;
  }

  if (!g_run) { clearMotorOpenIfStopped(); return; }

  // No output without data from the controller -- blindly raising the duty cycle
  // would be exactly the fault the feed-forward is built against.
  if (!vesc::connected()) {
    dropout("Abort: no controller data for 1 s (loop blocked?)");
    return;
  }

  // Control on the controller speed: it arrives every 20 ms, the mark speed only once
  // per revolution (every 1 s at 10 km/h) -- as the actual value it held back the ramp,
  // and the run-up to 100 km/h took 100 instead of 75 s (2026-11-25). Only after a relay
  // descent, while the controller is not yet in sync again, do the marks count.
  const float actualKmh = kmhSigned();
  const bool  afterRelay = brake::available() && brake::releasedForMs() < DESCENT_HANDOVER_MS;
  const float coastKmh   = afterRelay ? drumKmh() : actualKmh;

  // Descent: relays brake ONCE, release ONCE, coast the rest (config.h DESCENT IN THE RUN).
  // Speed here ONLY from the drum marks: the controller does not see the shorted
  // motor and reports 0 -- that was the chopping of 2026-11-25.
  if (g_descBrake) {
    vesc::release(); g_out = 0.0f;
    const float v = revsrc::angleReference() ? revsrc::kmh() : NAN;
    const float lead = descentLeadKmh();
    if (!brake::active() || isnan(v) || v <= g_target + lead) {
      brake::releaseEarly();
      g_descBrake = false;
      Serial.printf("# Descent: released at %.1f km/h (target %.1f, lead %.1f, braked from %.1f)\n",
                    v, g_target, lead, g_descFrom);
    }
    return;
  }
  // Relays just pulled in: contacts bounce, the controller stays released for now.
  if (brake::available() && !brake::active() && brake::releasedForMs() < RELAY_SETTLE_MS) {
    vesc::release(); g_out = 0.0f; g_fresh = true;
    return;
  }

  // Slowing down: coast, the controller never brakes (config.h SPEEDCTL_COAST_*).
  if (coastKmh > g_target + SPEEDCTL_COAST_ABOVE_KMH) g_coasting = true;
  if (g_coasting) {
    // Keep coasting while the target is not reached OR the controller speed does not
    // yet match the drum -- otherwise the control picks up too low and brakes.
    const bool synced = !afterRelay || fabsf(actualKmh - coastKmh) < DESCENT_SYNC_TOL_KMH;
    if (coastKmh > g_target + SPEEDCTL_COAST_UNTIL_KMH || !synced) {
      vesc::release();
      g_out = 0.0f;
      const float v = revsrc::kmh();
      if (!g_descDone && brake::available() && !brake::active() && revsrc::angleReference() &&
          v > g_target + descentLeadKmh() + SPEEDCTL_COAST_UNTIL_KMH) {
        const float tc = coastTimeS(v, g_target + SPEEDCTL_COAST_UNTIL_KMH);
        g_descDone = true;                 // decided is decided: never a second time
        if (tc >= DESCENT_MIN_COAST_S) {
          brake::engage();                 // controller is released (N2), the relays drop after RELAY_DRIVER_OFF_MS
          g_descBrake = true; g_descFrom = v;
          Serial.printf("# Descent: braking from %.1f km/h to %.1f (coast-down would be %.0f s)\n", v, g_target, tc);
        } else {
          Serial.printf("# Descent: coasting only %.1f -> %.1f km/h (%.1f s)\n", v, g_target, tc);
        }
      }
      return;
    }
    g_coasting = false;
    g_fresh = true;              // pick up again at the setpoint
  }

  if (g_fresh) {
    // Pick up where the drum is turning. See rule 1 in speedctl.h.
    g_fresh = false;
    g_ramp  = actualKmh > 0.0f ? actualKmh : 0.0f;
    g_duty  = rpmFromKmh(g_ramp) / noLoadRpm();
    if (g_duty < 0.0f) g_duty = 0.0f;
  }

  // Ramp the setpoint, flattening before the target (config.h SPEEDCTL_APPROACH_*)
  if (g_ramp < g_target) {
    float k = (g_target - g_ramp) / SPEEDCTL_APPROACH_KMH;
    if (k > 1.0f) k = 1.0f;
    if (k < SPEEDCTL_APPROACH_MIN) k = SPEEDCTL_APPROACH_MIN;
    g_ramp += SPEEDCTL_RAMP_KMH_PER_S * k * dt;
    if (g_ramp > g_target) g_ramp = g_target;
    // ... and never more than SPEEDCTL_LEAD_KMH ahead of the drum (config.h).
    const float lead = (actualKmh > 0.0f ? actualKmh : 0.0f) + SPEEDCTL_LEAD_KMH;
    if (g_ramp > lead && lead < g_target) g_ramp = lead;
  } else if (g_ramp > g_target) {
    g_ramp -= SPEEDCTL_RAMP_KMH_PER_S * dt;
    if (g_ramp < g_target) g_ramp = g_target;
  }

  // Clamp from the no-load characteristic of the setpoint (rule 3)
  float clamp = rpmFromKmh(g_ramp) / noLoadRpm() * SPEEDCTL_CLAMP_FACTOR
                 + SPEEDCTL_CLAMP_OFFSET;
  // Start window: while the drum is still standing, the output may rise up to the
  // breakaway -- otherwise it waits under current until the ramp allows it (config.h).
  if (actualKmh < STANDSTILL_KMH && g_ramp > STANDSTILL_KMH && clamp < SPEEDCTL_START_DUTY)
    clamp = SPEEDCTL_START_DUTY;
  if (clamp > VESC_MAX_DUTY) clamp = VESC_MAX_DUTY;

  // Trim as PI-D (config.h), change per round limited asymmetrically (rule 2).
  // g_duty is the I part including the preset; P and D are added on top each round
  // and are not integrated.
  const float err  = g_ramp - actualKmh;
  const float dvdtNow = revsrc::dvdtKmhS();
  // D only on the acceleration BEYOND the ramp: while the ramp rises, accelerating
  // is intended, and a D against it only increased the lag and with it the
  // overshoot (2.1 km/h on 2026-11-23).
  const float wanted = (g_ramp < g_target - 0.01f) ? SPEEDCTL_RAMP_KMH_PER_S : 0.0f;
  const float dTerm = isnan(dvdtNow) ? 0.0f : -SPEEDCTL_KD * (dvdtNow - wanted);
  const float pTerm = SPEEDCTL_KPP * err;
  float       step = SPEEDCTL_KI * err * dt;
  const float up   = SPEEDCTL_DUTY_UP_PER_S   * dt;
  const float down = SPEEDCTL_DUTY_DOWN_PER_S * dt;
  if (step >  up)   step =  up;
  if (step < -down) step = -down;

  // Anti-windup: if the output sits at the clamp and the error keeps pushing
  // upwards, the I part does not integrate -- otherwise there is a surplus on
  // reaching the setpoint that has to be worked off first.
  const bool saturated = (g_duty >= clamp - 1e-4f) && err > 0.0f;
  if (!saturated) g_duty += step;
  if (g_duty > clamp) g_duty = clamp;
  if (g_duty < 0.0f)   g_duty = 0.0f;
  float out = g_duty + pTerm + dTerm;
  if (out > clamp) out = clamp;
  if (out < 0.0f)   out = 0.0f;

  // Detect the emergency stop (speedctl.h, config.h ESTOP_*): output present, no current,
  // AND the speed is far below the no-load speed for this output.
  // Current only: false alarm at no load (0.2 A at 28 km/h). Standstill only: the
  // Hall sensors still report the coasting drum for 25 s (both 2026-11-23).
  const float imot = fabsf(vesc::values().currentMotor);
  const float rpmNoLoad = g_duty * noLoadRpm();
  const bool  lagging = rpmNoLoad > 0.0f &&
                        fabsf(rpmSigned()) < ESTOP_SPEED_RATIO * rpmNoLoad;
  const float dvdt = revsrc::dvdtKmhS();
  const bool  rampingDown = g_ramp > g_target + 0.1f;
  const bool  decel = !isnan(dvdt) && dvdt < -ESTOP_DECEL_KMH_S && !rampingDown;
  // While the firmware itself has let the relays drop, "no motor on the controller"
  // is the intended state, not an emergency stop (docs/04-hardware.md §9.4).
  if (brake::active()) {
    g_openSince = 0;
  } else if (out > ESTOP_DUTY_MIN && imot < ESTOP_IMOT_MAX_A && (lagging || decel)) {
    const uint32_t now = millis();
    if (g_openSince == 0) {
      g_openSince = now;
    } else if (now - g_openSince >= ESTOP_DETECT_MS) {
      g_motorOpen = true;
      dropout("Abort: no motor current (E-stop or motor disconnected)");
      return;
    }
  } else {
    g_openSince = 0;
  }

  g_out = out;
  vesc::setDuty(out * settings::directionSign());
}

bool motorOpen()  { return g_motorOpen; }
bool estopInput() { return ESTOP_FB_WIRED && digitalRead(PIN_ESTOP_FB) == LOW; }

float targetKmh() { return g_target; }
float rampKmh()   { return g_run ? g_ramp : 0.0f; }
float kmh()       { const float k = kmhSigned(); return k > 0.0f ? k : 0.0f; }
// An output is also applied during the breakaway measurement -- without g_brk here
// the display showed 0 throughout, and that very number is the measurement result.
float duty()      { return g_run ? g_out : (g_brk ? g_duty : 0.0f); }
bool  running()   { return g_run; }
bool  slowingDown() { return (g_run && g_coasting) || g_stopping; }

void stop() {
  // Also ends a breakaway / direction run: until 2026-11-26 STOP did not, and the
  // duty cycle went on rising on the next tick (found while adding the detection).
  if (g_dirDet) { g_dirDet = false; g_dirRes = 0; }
  g_brk = false;
  g_descBrake = false;
  g_run = false;
  g_coasting = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  g_stopping = true;
  applyBrake(fabsf(kmhSigned()));
}

// "Drives and does not turn": output above STALL_DUTY_MIN, ramp running, drum standing.
// ONE finding for FAULT, status line and state output (until 2026-11-25 written out
// three times, each with its own standstill number).
bool stalledUnderDuty() {
  return g_run && g_ramp > STANDSTILL_KMH && g_out > STALL_DUTY_MIN && revsrc::kmh() < STANDSTILL_KMH;
}

void coast() {
  g_brk = false;          // breakaway() sets it again right after its own coast()
  g_descBrake = false;
  g_run = false;
  g_coasting = false;
  g_stopping = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  vesc::release();
}

void learn() {
  // Only at standstill and only with the drive released: whoever turns by hand
  // should not work against a holding torque.
  coast();
  g_learn = true;
  g_learnStart = millis();
  g_refusal = "";
}

bool learning() { return g_learn; }

void finishDirection() {
  g_dirDet = false;
  if (isnan(g_dirDelta) || fabsf(g_dirDelta) < DIRDETECT_MIN_N) { g_dirRes = 0; g_refusal = "Direction unclear, unchanged"; return; }
  if (g_dirDelta > 0.0f) { g_dirRes = 1; return; }
  g_dirRes = -1;
  if (!settings::setDirectionSign(static_cast<int8_t>(-settings::directionSign()))) g_refusal = "Sign not saved";
}
bool detectDirection(const char** why) {
  if (!settings::calibrated()) { *why = "Calibrate first"; return false; }
  const float f = torque::newtons();
  if (isnan(f)) { *why = "No load cell"; return false; }
  breakaway();
  g_dirDet = true; g_dirBase = f; g_dirDelta = NAN; g_dirRes = -2;
  return true;
}
bool  detectingDirection() { return g_dirDet; }
int   directionResult()    { return g_dirRes; }
float directionDeltaN()    { return g_dirDelta; }

void breakaway() {
  coast();
  g_duty = 0.0f;
  g_brkDuty = g_brkAmps = 0.0f;
  g_brk = true;
  g_refusal = "";
}

bool  breakingAway()  { return g_brk; }
float breakawayDuty() { return g_brkDuty; }
float breakawayAmps() { return g_brkAmps; }

const char* lastRefusal() { return g_refusal; }

}  // namespace speedctl
