#pragma once
//
// All physical constants of the test stand — and only here.
// Mirrors docs/03-parameters.md. Always change both files.
//
// The origin of every value is given in the comment:
//   [meas]  measured        [calc] calculated
//   [DS]    datasheet       [est]  estimated
//   [spec]  specified (design decision)
//   [TBD]   still to be determined — method in docs/03-parameters.md
//

#include <Arduino.h>

// ---------------------------------------------------------------- Geometry

// [TBD] Built and measured: Ø 900 mm BARE (2026-11-01). With 1.5 mm chequer plate
// on the circumference this becomes Ø 903 mm, i.e. U = 2.837 m — that is the value here.
// It stays TBD until the plate is on and the finished surface has been re-measured
// with the tape measure (average of 3 measurements). 1 mm error = 0.035 %.
// All torques derived from r shift with this value.
// [meas 2026-11-23] 2842 mm with the tape measure around the finished surface.
constexpr float DRUM_CIRCUMFERENCE_M = 2.842f;

// [calc] U / 2π
constexpr float DRUM_RADIUS_M = DRUM_CIRCUMFERENCE_M / (2.0f * PI);

// [calc] Moment of inertia of the drum from the geometry of the Ø 905 drum
// (docs/09-measurement-theory.md). Determines the run-up time, the energy that must
// be dissipated in an emergency (3.7 kJ at 100 km/h) -- and since 2026-11-24 the INERTIA
// CORRECTION per measurement point: M_corr = M - J_eff * dw/dt (state.cpp finishPoint,
// docs/11-review.md K1). It therefore enters the result with UNC_J_REL;
// re-measure by a spin-down test with a known added mass (docs/05 A5.3).
constexpr float DRUM_INERTIA_KGM2 = 1.94f;
// [est] Relative uncertainty of DRUM_INERTIA_KGM2 as long as J is only calculated.
// Determines how much residual acceleration a point tolerates: after the correction
// UNC_J_REL * J * dw/dt remains as error.
constexpr float UNC_J_REL = 0.05f;

// [meas] Idle loss torque of the drum, determined on 2026-11-17 from the FREE SPIN-DOWN:
// 97 -> 29 km/h in 140 s, eight support points, M = I * dw/dt. Fit
//     M_loss = DRUM_LOSS_CONST_NM + DRUM_LOSS_OMEGA2_NM * w^2     (w in rad/s)
//
// The constant part is bearing and motor friction. Of this, only the MOTOR PART
// cancels out in the cradle measurement (motor bearings and cogging torque act
// between stator and rotor). The DRUM BEARINGS -- two pillow-block bearings for the
// 15-mm shaft on the stand, the motor is only flanged on -- lie OUTSIDE and are
// measured along; they sit in the A0 of the idle-loss curve and are subtracted with
// it. Their load-dependent increase under wheel load remains as residual error
// (UNC_BEARING_REL, docs/06-open-questions.md A11). The w^2 part is the
// aerodynamic drag of the drum.
//
// At 60 km/h this is 0.43 Nm. docs/09-measurement-theory.md quotes 0.27 Nm for it — the
// measured value is 1.6 times larger. Against a useful signal of 1.32 Nm (Crr 0.007
// at 417 N wheel load) that is 33 % instead of 20 %, at 100 km/h even 91 %. It can be
// subtracted because it can be measured idle at each step — but it must be
// measured, not estimated.
constexpr float DRUM_LOSS_CONST_NM  = 0.472f;
constexpr float DRUM_LOSS_OMEGA2_NM = 3.178e-4f;

// [TBD] Short arm = larger force = better SNR; the limit is the overload reserve.
// The cell is a 5-kg cell (49.03 N). At L = 0.040 m the design case would be at
// 96 % and the case including ramp at 128 % — the cell would be overloaded in NORMAL
// OPERATION, not only in a fault. With L = 0.080 m the design works out again:
//   design case steady state 1.88 Nm → 23.5 N = 48 % of cell full scale
//   including ramp           2.51 Nm → 31.4 N = 64 %
//   ΔCrr = 0.0005                            →  1.22 N
// These are exactly the same utilisations as in the earlier 10-kg calculation at 0.040 m:
// half the cell, double the arm. The reasoning in docs/08-stand.md §5.2 therefore holds
// unchanged, only the overload limit lies elsewhere.
// According to the statement of 2026-11-16 about 80 mm are installed. The arm enters
// every torque LINEARLY — before the first measurement measure the bore to ±0.1 mm and
// enter the value here; until then it stays TBD. The calibration bore moves outwards
// with it to 160 mm, so that the 2:1 ratio is kept. Stop at 0.200 m.
// [meas 2026-11-23] 84 mm, drum axis to load cell, stated at the rig.
constexpr float TORQUE_ARM_M = 0.084f;

// [spec] Lever arm of the CALIBRATION WEIGHT, the second bore in the arm. The
// 2:1 ratio to the cell arm is intentional: a weight on the longer arm produces
// double the torque and thus double the force at the cell, so calibration up into
// the upper measuring range is possible with handy weights.
//
// Both arms can be overridden at runtime (settings.h) — they are measured AT THE
// RIG, and a calibration run that forces a recompile does not get done.
// [meas 2026-11-23] 330 mm, measured axis to mark on the arm; the certified 1-kg
// weight hangs there: 38.5 N at the cell, 79 % of the 5-kg cell -- covers the
// measuring range (48...64 %) at the top and leaves reserve. The mark may still move
// to a rounder value; then update here AND in NVS.
// IMPORTANT: in the torque chain TORQUE_ARM_M cancels out,
//   M = counts * m*g*CALIB_ARM_M / (counts_cal - offset),
// so the correctness of every torque depends on THIS dimension, not on the cell arm
// (docs/11-review.md D2). Hence UNC_LCAL_REL instead of an L item.
constexpr float CALIB_ARM_M = 0.330f;

// [spec] DIRECT DRIVE: the drum sits on the rotor of the hub motor,
// motor and drum speed are identical. The installed hoverboard motor alone gives
// 84 km/h at the drum at about 480 rpm, so a transmission is
// not needed — and any
// transmission would lie OUTSIDE the cradle and be measured along
// (docs/09-measurement-theory.md §1.6). The constant stays in case a belt
// is added after all.
constexpr float BELT_RATIO = 1.00f;

// ------------------------------------------------------- Speed sensors

// [spec] 8 screwed-on magnets on the pitch circle of the 8 spacer blocks
// (docs/04-hardware.md §9). More marks bring no accuracy — everything that goes into
// the result is measured index-triggered over whole revolutions, where the mark
// pitch cancels out exactly. They only bring latency on the
// drum-side path; the actual resolution comes from the motor Hall.
// WHAT FOR WHAT (docs/11-review.md K4): for the measurement, real revolutions
// count -- they turn "10 revolutions calculated from the controller speed" into
// ten real ones. ANY sensor on the drum provides them: the mark Hall (every 8th edge
// is one revolution, speed.cpp) or a separate index Hall. With only ONE Hall the
// 8 marks are the better choice: they add a speed independent of the controller,
// standstill and slip checks and the angle bins. The index Hall only adds
// the fixed angular zero. Without any sensor: revsrc calculates (source "regler").
constexpr uint8_t MARKS_PER_REV = 8;
// INTERMEDIATE MAGNET, optional (2026-11-24, docs/04-hardware.md §8.6): a ninth magnet
// exactly in the middle of ONE 45° gap. The single mark Hall then sees per revolution
// seven intervals of 45° and two of 22.5°; the pulse between the two short ones
// is the index. This gives the fixed angular zero without a second Hall, and the count
// re-locks every revolution (speed.cpp service()). Detection is automatic; without the
// magnet it stays at "every 8th edge". Opposite it, an equally heavy NON-
// magnetic screw against the imbalance.
constexpr bool    MARK_PATTERN_INDEX   = true;   // [spec] detection enabled
constexpr float   MARK_SHORT_RATIO     = 0.7f;   // [est] an interval below this (relative to the mean) counts as a half one
constexpr float   MARK_MISSED_RATIO    = 1.6f;   // [est] above this an edge counts as missed
constexpr uint8_t MARK_PATTERN_CONFIRM = 2;      // [est] this many times at a spacing of 9 pulses, then confirmed
constexpr uint8_t MARK_REF_N           = 9;      // [spec] reference "regular spacing" = median of this many spacings (one revolution with intermediate magnet)
constexpr float   MARK_PATTERN_MIN_KMH = 6.0f;   // [est] no classification below this: when starting, the speed changes too much within one revolution
// [spec] Magnets on the spokes of the test wheel, sensor at the dropout
// (docs/09-measurement-theory.md §4). Like the drum, the wheel speed is calculated over a
// window of one full revolution; more magnets would bring no
// accuracy, only a faster update. Since 2026-11-22
// ONE magnet is fitted — the value must match the wheel, otherwise speed and
// tire diameter are wrong by exactly this factor.
constexpr uint8_t WHEEL_MARKS_PER_REV = 1;
// Wheel circumference from the speed ratio (wheel.h): moving window of the
// last WHEEL_CIRC_WINDOW_REVS wheel revolutions, recalculated on every pulse.
constexpr uint8_t  WHEEL_CIRC_WINDOW_REVS = 10;   // [spec 2026-11-23]
constexpr uint32_t WHEEL_CIRC_GAP_MS      = 3000; // [est] no pulse: wheel stationary, window discarded
// Slip / wheel lifted (2026-11-26): the circumference from the LAST wheel revolution alone
// against the window mean. Lifted, the wheel coasts on by itself and the ratio drifts
// away; the window then kept showing (and could store) a wrong value until the wheel
// stood for WHEEL_CIRC_GAP_MS. Above this deviation the window is discarded.
constexpr float    WHEEL_CIRC_SLIP_REL    = 0.03f; // [est] 3 %; check on the stand that steady running stays below
constexpr uint32_t MIN_PULSE_US = 800;   // [calc] shortest real spacing is 21 ms
// [est] DOUBLE PULSE: if a mark edge arrives after less than MARK_ECHO_PCT % of the
// last mark spacing, it is an echo of the same mark and is discarded. Found
// on 2026-11-25 (staircase 100-80-60-40-20): three jumps of the mark speed by exactly
// one extra pulse, e.g. 99.4 -> 113.8 km/h = 8/7 at 100 km/h. MIN_PULSE_US alone
// does not catch this: at 100 km/h a real spacing is 21 ms, the echo came later than
// 0.8 ms. 35 % lets the intermediate magnet (half spacing, 50 %) through. Applies only from the
// second spacing after a standstill (STALL_TIMEOUT_MS), otherwise the first one after a
// long pause would see every edge as an echo.
constexpr uint32_t MARK_ECHO_PCT = 35;
// [est] The same for the TEST WHEEL (one magnet per revolution): an edge after less than
// WHEEL_ECHO_PCT % of the last wheel interval is a phantom. Found on 2026-11-26 in the raw
// log of run 2026-11-26-1542: when the brake relays dropped (227.995 s, 1495.8 s) a second
// wheel pulse came 5 and 17 ms after a real one at 150 ms spacing; in the lifted-wheel
// spin-down it made the wheel model about eight times too high.
constexpr uint32_t WHEEL_ECHO_PCT = 50;
// [calc] The limiting case is not 10 km/h but the run-up: with 8 marks the
// pulse spacing at 10 km/h is only 132 ms, but at 1 km/h it is 1.32 s. 2000 ms means
// "slower than 0.64 km/h counts as standstill" — the ramp is above that after 1.3 s.
constexpr uint32_t STALL_TIMEOUT_MS = 2000;
// [est] n_motor/i versus n_drum. With direct drive there is no slip —
// this is a PLAUSIBILITY CHECK of the two sensors, not a slip criterion.
constexpr float SLIP_TOLERANCE = 0.05f;

// [meas] Number of pole pairs, counted via the Hall sensors on 2026-11-17: over 10
// marked hand turns the controller's tachometer counter rose by exactly 900
// steps, without a single backward step. 6 steps per electrical
// revolution give 900 / 10 / 6 = 15 pole pairs, i.e. 30 poles.
//
// The scaling "6 steps per electrical revolution" was cross-checked on the same data set:
// the integral of |ERPM| over time gave 6.31 steps per
// electrical revolution, the excess is jitter.
//
// A first attempt in SENSORLESS mode gave 16.5 and was not reproducible —
// the position observer wobbles at hand speed. Only with Hall sensors is the
// tachometer counter a measurement. Until then the controller was set to si_motor_poles = 14 — that
// is the FACTORY DEFAULT of the VESC firmware, proven on the controller freshly flashed
// to 7.01 (it shows 14 again there). The firmware does NOT DETERMINE the pole count,
// it is typed in; without an entry it stays at 14, and every mechanical speed
// derived from it is wrong.
constexpr uint8_t MOTOR_POLE_PAIRS = 15;

// Motor Hall edges per motor revolution, one evaluated phase = number of pole pairs.
// With direct drive this is at the same time a high-resolution drum speed source;
// the comparison with the drum marks remains useful as a plausibility check
// (detects a failed sensor).
constexpr uint8_t MOTOR_PULSES_PER_REV = MOTOR_POLE_PAIRS;
constexpr uint32_t MOTOR_HALL_MIN_US = 100;      // [est] noise filter on the motor Hall; at 480 rpm and 15 edges the spacing is 8.3 ms
constexpr uint32_t SPEED_STALE_GRACE_US = 100000; // [est] if the last pulse is older than two mark spacings plus this, the drum is stationary (until 2026-11-25: two revolutions)
constexpr uint16_t VESC_TACHO_PER_REV = 6 * MOTOR_POLE_PAIRS;   // [meas] 900 tachometer steps over 10 revolutions (hardware/vesc-config-fw7.01); also counts when turned by hand

// --------------------------------------------------- Motor, electrical
// Measured on 2026-11-17 on the installed motor, controller FSESC 4.20 with firmware 5.02.
// Before that the controller held a foreign configuration: gear ratio and open-loop speed
// set by hand, R/L/flux linkage from a detection on a DIFFERENT motor.
// Pole count, wheel diameter and the speed controller gains in it, on the other hand, were only the
// factory defaults — see docs/03-parameters.md §4.1.

// [meas] Flux linkage, detected by the controller (foc_detect_apply_all with 50 W
// loss limit). Peak value of the phase flux linkage.
constexpr float MOTOR_FLUX_LINKAGE_WB = 0.017287f;

// [calc] Torque constant of a three-phase PMSM under field-oriented control:
//     kt = 3/2 * pole pairs * flux linkage = 0.389 Nm/A
// Cross-check on the run-up: 3 A gave 1.17 Nm, and the drum
// accelerated with it as DRUM_INERTIA leads one to expect.
constexpr float MOTOR_KT_NM_PER_A = 1.5f * MOTOR_POLE_PAIRS * MOTOR_FLUX_LINKAGE_WB;

// [meas] Phase resistance with DIRECT wiring: measure_res_ind gave 237.2 mOhm,
// the detection entered 238.7 mOhm. With the short-circuit relay block in the path it is
// 251.9 mOhm (mean of four runs, scatter +/- 1.5 mOhm), i.e. 14.7 mOhm
// contact resistance. This is an efficiency item and does NOT appear at the
// load cell — electrical losses are not a mechanical reaction torque.
constexpr float MOTOR_R_OHM = 0.2387f;

// [meas] No-load speed per bus volt, on the FREE motor over ten support points from
// duty cycle 0.50 to 0.95 at 40 V, strictly linear (11 384 to 11 450 ERPM per
// unit duty cycle).
//
// NOT to be confused with the "kv = 14" listed in docs/06-open-questions.md under A13:
// that is the top speed of the system WITH drum, i.e. against its
// aerodynamic drag — a system quantity, not a motor constant. The inductance is
// deliberately not listed here: measure_res_ind scattered over four runs from
// 305 to 377 uH, the detection entered 403.8 uH. That needs a different
// method before a number here may claim anything. [TBD]
constexpr float MOTOR_NO_LOAD_RPM_PER_V = 19.1f;

// ------------------------------------------------------------------ HX711

constexpr uint8_t HX711_GAIN_PULSES = 1;   // [DS] 1 = channel A / gain 128
constexpr uint16_t HX711_RATE_SPS   = 80;  // [DS] RATE pin to VCC
constexpr uint16_t TARE_SAMPLES     = 80;  // [est] 1 s at 80 SPS; 200 until 2026-11-24. The noise needs no more: 143 counts peak-to-peak are 0.001 N on F_roll, negligible when averaged over 80 values. In practice the time is set by the main loop (polled HX711, 20..40 values/s with a browser)
// [spec] Abort the tare if not enough values arrive. 200 values are
// about 2.5 s at 80 SPS; 5000 ms leave room and prevent a silent HX711 from
// occupying the TARE state forever.
constexpr uint32_t TARE_TIMEOUT_MS  = 15000; // [est] emergency brake for a silent HX711; with browser load only 20..40 values/s arrive, 200 then take up to 10 s

// [spec] Moving average for the DISPLAY, not for the measurement. The
// HX711 delivers HX711_RATE_SPS values per second; a number jittering at 80 Hz
// is neither readable nor meaningful (docs/10-firmware.md W5, D6). 40 values are
// 0.5 s at 80 SPS. The MEASUREMENT averages independently, revolution-synchronously, over
// AVG_REVS_PER_POINT — the two averagings have nothing to do with each other.
constexpr uint8_t DISPLAY_AVG_N = 40;

// [spec 2026-11-26] Live trace of the raw values in the guided run while zeroing
// (step 4) and at the zero point after the run: the last TRACE_N values go to the UI
// as a curve, so the operator sees fine flutter. 160 values = 2 s at 80 SPS.
constexpr uint16_t TRACE_N = 160;
static_assert(TRACE_N >= TARE_SAMPLES, "run.cpp takes the tare peak-to-peak from the trace");

// [stated 2026-11-16] 5-kg cell, 5 · 9.80665 = 49.03 N. Until then the documents
// said 10 kg; that was wrong and, together with the 40-mm arm, would have destroyed
// the cell when starting. Still read the nameplate and set the origin to [DS].
constexpr float LOADCELL_CAPACITY_N = 49.03f;
// [spec 2026-11-26] Load cell and amplifier as fitted, for the protocol's "Stand build"
// section. Type from the builder's statement above; the board from docs/04-hardware.md §5.5.
constexpr const char* LOADCELL_TYPE = "5 kg (nameplate not read)";
constexpr const char* LOADCELL_AMP  = "HX711, SparkFun SEN-13879, RATE trace cut 2026-11-26";
// [est] Calibration procedure (calib.h): averaging time per reading at standstill, at most
// this many readings, and the masses offered as quick choices in the UI (builder's weights,
// stated 2026-11-26; 1000 g certified, 500 and 200 g nominal until weighed).
constexpr uint32_t CAL_AVG_MS     = 5000;
constexpr uint8_t  CAL_MAX_POINTS = 16;
constexpr uint16_t CAL_MASSES_G[] = {200, 500, 1000};
// [spec] Longest stand revision text (settings::standRev), characters.
constexpr uint16_t STAND_REV_LEN = 160;

// [TBD] CALIBRATION PENDING. As long as this value is 0, the firmware outputs
// no forces and no Crr, only raw values. This is intentional:
// an invented calibration factor would be worse than a gap.
// Determination: docs/05-test-procedure.md §2.
// [meas 2026-11-23] Certified 1-kg weight at 330 mm, cell arm 84 mm, i.e.
// 38.53 N at the cell; factor determined via the web UI (command k). A
// single weight -- linearity over the range is therefore not yet
// checked (docs/03-parameters.md §2).
constexpr float LOADCELL_N_PER_COUNT = 0.00002668f;

// -------------------------------------------------------------- Measurement operation

constexpr float V_MIN_KMH = 10.0f;   // [spec]
constexpr float V_MAX_KMH = 100.0f;  // [spec] builder 2026-11-25, entry docs/07-safety.md §5; 60 until then

// [spec] Support points, DEFAULT. Since 2026-11-23 the valid list lives in
// the settings (settings::plateauKmh, NVS) and is maintained via the web UI;
// these values apply as long as nothing else is stored there, or after
// a reset. Baseline and measurement MUST use the same list,
// otherwise the evaluation cannot subtract point by point — hence it is
// ONE list for both runs.
// 28.8 km/h is included on purpose: it is the reference speed of the
// published Crr database (200 rpm on their 77-cm drum). Together with
// STANDARD_LOAD_N this point allows a plausibility comparison. 60 km/h was V_MAX_KMH until
// 2026-11-25; the default still ends there, higher steps are added via h<kmh>.
constexpr float PLATEAUS_KMH[] = {15, 28.8f, 40, 50, 60};
constexpr uint8_t PLATEAU_COUNT = sizeof(PLATEAUS_KMH) / sizeof(PLATEAUS_KMH[0]);
constexpr uint8_t PLATEAU_MAX = 16;           // room in the settings

constexpr float RAMP_KMH_PER_S = 0.5f;        // [est] 2 min from 0 to 60
constexpr uint32_t SETTLE_MS = 15000;         // [est] after a plateau change, minimum time
// [est 2026-11-24] Settling does not end by the clock, but when the actual value
// has been within SETTLE_TOL_KMH of the setpoint for SETTLE_STEADY_MS -- the control
// needs 17 to 22 s to reach the target (docs/03-parameters.md §8.2), SETTLE_MS alone
// was too short. After SETTLE_MAX_MS it continues anyway; the point is then
// judged by dv/dt.
constexpr float    SETTLE_TOL_KMH   = 0.3f;
constexpr uint32_t SETTLE_STEADY_MS = 5000;
constexpr uint32_t SETTLE_MAX_MS    = 60000;
constexpr uint8_t AVG_REVS_PER_POINT = 10;    // [est] whole revolutions
// [lit] The reference test rig warms up for 30 min before measuring.
// Crr drops markedly with tire temperature; without a defined warm-up,
// comparisons between tires are not reliable.
constexpr uint32_t WARMUP_MS = 1800000;
// [est] The warm-up ends early once it has settled (2026-11-26): at least WARMUP_MIN_MS,
// and the last WARMUP_STABLE_MIN minute values of the corrected torque M span at most
// WARMUP_STABLE_NM, and the tire temperature (AIRSPY, if present) rises by at most
// WARMUP_TTIRE_K_PER_MIN. WARMUP_MS stays the upper limit. Run 2026-11-26-1542 (SX-R,
// 5 bar): minutes 8-12 and 13-17 spanned 0.014 Nm each, minutes 1-5 0.06 Nm; 0.025 Nm
// is 0.055 N on F_roll at r = 0.452 m, Crr 0.0001 at 49.8 kg. To be checked over further runs.
constexpr uint32_t WARMUP_MIN_MS          = 600000;
constexpr uint8_t  WARMUP_STABLE_MIN      = 5;
constexpr float    WARMUP_STABLE_NM       = 0.025f;
constexpr float    WARMUP_TTIRE_K_PER_MIN = 0.2f;
// [spec] Speed at which the warm-up counts, and the band around it.
// WARMUP_KMH is PLATEAUS_KMH[1] — the same support point, for the same reason.
constexpr float WARMUP_KMH = 28.8f;
constexpr float WARMUP_TOL_KMH = 1.0f;        // [est] band in which time counts

// [spec] Window over which dv/dt is calculated. At a 5 Hz display that is
// five support points; revsrc.cpp stores eight samples in it and fits a straight line.
constexpr uint32_t DVDT_WINDOW_MS = 1000;
// [calc] "Constant" is not a matter of judgement by eye but a limit: residual
// acceleration produces an apparent torque via M = J·dω/dt. 1 % of the signal of a
// good racing tire is 0.012 Nm, via J = 1.94 kg·m² that is 0.0062 rad/s² and thus
// 0.010 km/h/s (docs/10-firmware.md §7.1).
// Since 2026-11-24 the apparent torque is SUBTRACTED per point (state.cpp), not only
// gated: what matters is the NET acceleration over the averaging window,
// as a fitted line v(t) over the point. What remains is the fraction UNC_J_REL
// of it; a point is therefore valid as long as
//     |dv/dt net| < DVDT_CONST_KMH_PER_S / UNC_J_REL      (= 0.2 km/h/s)
// -- then the residual error is below 1 % of the signal. Before, the gate was set to
// STEADY_DVDT_KMH_S (0.3 km/h/s peak value), which would have been 51 % of the signal.
constexpr float DVDT_CONST_KMH_PER_S = 0.010f;
constexpr float POINT_DVDT_NET_MAX_KMH_S = DVDT_CONST_KMH_PER_S / UNC_J_REL;

// [spec] Standard wheel load: 42.5 kg, as in the published Crr database.
// Only a suggested value for operation — the actual load is measured
// or entered via the command 'l<value>' and enters Crr LINEARLY.
constexpr float STANDARD_LOAD_N = 416.9f;
// WHEEL LOAD FROM THE LEVER BALANCE (docs/08-stand.md §5, docs/09 §5.2). The swing arm carries
// wheel and arm (empty weight, weighed at the axle) and a sliding weight hanging at b from the
// pivot; the wheel arm is a. Then
//     N = (m_empty + m_weight * b / a) * g
// The firmware calculates this from the position b that the operator enters in the run
// (tape measure on the beam, zero at the pivot axis). A measurement with the hanging scale at
// the axle may override the value and thereby checks the lever model.
// [meas 2026-11-23] 7.2 kg empty, 10.4 kg sliding weight, a = 20.2 cm, b = 84 cm -> 50.4 kg.
// [meas 2026-11-26, builder] 10.46 kg sliding weight, a = 20.5 cm, b = 84 cm, fork preload
// 1.7 kg at 84 cm (= 6.97 kg at the axle) -> 49.8 kg.
// All three can be overridden at runtime (settings.h, Setup -> Wheel load).
constexpr float LOAD_EMPTY_KG     = 6.97f;   // [meas] wheel, arm and fork at the axle, without sliding weight: 1.7 kg at 84 cm x 0.84 / 0.205 (2026-11-26; 7.2 weighed until then)
constexpr float LOAD_WEIGHT_KG    = 10.46f;  // [meas] sliding weight (2026-11-26; 10.4 until then)
constexpr float LOAD_ARM_A_M      = 0.205f;  // [meas] pivot -> wheel axle, horizontal (2026-11-26; 0.202 until then)
constexpr float LOAD_POS_DEFAULT_M = 0.84f;  // [meas] position of the sliding weight as of 2026-11-23
// [calc] Default for settings::loadN, from the four values above: 488.7 N = 49.8 kg.
constexpr float LOAD_DEFAULT_N = (LOAD_EMPTY_KG + LOAD_WEIGHT_KG * LOAD_POS_DEFAULT_M / LOAD_ARM_A_M) * 9.80665f;

// [spec] Design upper limit of the rolling resistance coefficient. The test stand is
// intended for fast road tires, not for MTB: the slowest tire envisaged
// is the Continental Contact Urban at 18.4 W, i.e. Crr 0.00552. 0.007 gives
// 27 % headroom above it. The earlier 0.015 was the MTB limit and made the lever arm unnecessarily long
// and thus the signal unnecessarily small.
constexpr float CRR_DESIGN_MAX = 0.007f;

// ------------------------------------------------------ Safety limits

constexpr float MAX_FORCE_N = 45.0f;          // 92 % of cell full scale (5 kg)
// [calc] DERIVED, not hard-coded: the limit means "60 km/h at the
// drum surface" and must follow the circumference. The old fixed value 333 applied to the
// planned Ø 955 drum; on the built Ø 933 drum it would already have tripped at 58.4 km/h,
// i.e. in the middle of the top measurement plateau.
// At U = 2.837 m it is 353 rpm.
// Raise V_MAX_KMH only per docs/07-safety.md §5.
// [spec 2026-11-23] 3 km/h margin above V_MAX_KMH: the top plateau lies AT
// V_MAX, and 1 km/h overshoot aborted the idle run there as "speed limit".
constexpr float OVERSPEED_MARGIN_KMH = 3.0f;
constexpr float MAX_DRUM_RPM = (V_MAX_KMH + OVERSPEED_MARGIN_KMH) / 3.6f / DRUM_CIRCUMFERENCE_M * 60.0f;
constexpr uint32_t FAULT_SETTLE_MS = 500;     // debouncing of the fault criteria
// STANDSTILL. [spec, builder 2026-11-25] Below STANDSTILL_KMH the drum is stationary.
// ONE number for everything that needs standstill: FAULT clears, tare and acknowledgement
// allowed, relay brake pulls in again, STOP ends, start window, "driving but not
// turning", turning backwards. Until 2026-11-25 this was 0.05 1/s (0.5 km/h) here, and
// five files had their own numbers between 0.3 and 1 km/h.
constexpr float    STANDSTILL_KMH  = 1.0f;
constexpr float    STANDSTILL_RPS  = STANDSTILL_KMH / 3.6f / DRUM_CIRCUMFERENCE_M;  // [calc] 0.098 1/s
constexpr float    STANDSTILL_RPM  = STANDSTILL_RPS * 60.0f;                       // [calc] 5.9 rpm, for controller speeds
constexpr uint32_t FAULT_CLEAR_STILL_MS = 2000; // [est] after this long at standstill a FAULT clears by itself
// [est] Duty cycle from which "driving but not turning" is a finding and not
// noise. On 2026-11-18 the drum stood still at duty 0.045 and 2.5 A motor current
// — that is commutation, not load (docs/04-hardware.md §11). Without dedicated
// drum marks this is the only way to detect standstill despite a setpoint.
// [meas 2026-11-23] Under 494 N wheel load the drum only breaks away at duty 0.139
// with 8.3 A. 0.05 was the value without load and aborted every measurement run before
// the drive even had a chance. 0.20 lies above it with reserve; the
// commutation fault (6.6 A at 0.114, no rotation) is thus detected a few
// seconds later, but detected.
constexpr float STALL_DUTY_MIN = 0.20f;
// DETECTING THE EMERGENCY STOP. The manual emergency stop disconnects the motor from the
// controller via three relays and shorts it; controller, ESP32 and operation stay powered. There
// is no feedback — the case is recognisable by the CURRENT: with the motor, a
// stationary drive at an applied duty cycle draws several amperes (6.6 A at 0.11 on
// 2026-11-23), without the motor only the measurement noise of the controller.
// [est] Check: trigger the emergency stop, v15, read Imot in the live telemetry (command j);
// it must stay below this limit.
// Duty cycle from which the emergency stop detection checks at all. NOT STALL_DUTY_MIN:
// that rose to 0.20 on 2026-11-23 (breakaway under load), and with it the
// detection was blind below about 30 km/h -- at 15 km/h the duty cycle is around 0.10,
// at 28.8 around 0.19. A motor on the controller draws more than ESTOP_IMOT_MAX_A at
// any duty cycle above 0.05 as soon as it delivers torque; the speed and
// deceleration conditions catch the idle case. [est]
constexpr float    ESTOP_DUTY_MIN   = 0.05f;
constexpr float    ESTOP_IMOT_MAX_A = 0.3f;
// Current alone is NOT enough: at a constant 28 km/h the motor drew only 0.2 A
// (2026-11-23), that is idling, not an emergency stop. In addition the speed must be clearly
// below the one that the duty cycle implies via voltage (no-load curve
// MOTOR_NO_LOAD_RPM_PER_V): a connected motor that turns slower than
// its voltage demands necessarily draws current. After the emergency stop the
// drum coasts down, the duty cycle remains, the gap grows -- from 28 km/h within
// a few seconds. [est] 0.6: below 60 % of the no-load speed for the duty cycle.
constexpr float    ESTOP_SPEED_RATIO = 0.6f;
// Second, faster path: the drum gets SLOWER without current flowing,
// and the setpoint is not currently being lowered. A connected motor immediately draws current
// when the speed drops under a constant duty cycle. Measured after the
// emergency stop from 28 km/h: about 3 km/h/s; the control oscillates with 0.1...0.2 km/h/s.
constexpr float    ESTOP_DECEL_KMH_S = 0.5f;   // [est]

// ------------------------------------------------- Live measurement only when steady
// Loss power and Crr are ONLY displayed when the run is steady: during
// acceleration J*dw/dt is in the torque, during settling it oscillates. Steady
// means: drive running, actual value close to setpoint, acceleration small, and that
// for STEADY_HOLD_MS. Then it is averaged over LIVE_WINDOW_MS (moving, like the
// wheel circumference). [est] 2026-11-23
// Since the evening of 2026-11-24 the NET acceleration over the averaging window counts
// (fitted line, as for the measurement point), not the instantaneous value: the speed
// calculated from the controller jitters by +-0.1 km/h between two polls at 30 km/h,
// the instantaneous value constantly broke the 0.3 km/h/s gate and cleared the window --
// which is why the warm-up never showed power and Crr.
constexpr float    STEADY_DVDT_KMH_S = 0.3f;    // |dv/dt| NET over the window below this counts as steady
constexpr float    STEADY_TOL_KMH    = 1.0f;    // |actual - setpoint| below this
constexpr uint32_t STEADY_HOLD_MS    = 2000;    // must be steady this long before the window fills
constexpr uint32_t LIVE_WINDOW_MS    = 5000;    // averaging window, up to 400 cell values
constexpr uint8_t  LIVE_MIN_N        = 3;       // [est] fewer values in the window are not an average
constexpr uint32_t LIVE_MIN_MS       = 1500;    // [est] this much of the window must be filled before a value appears (with a browser the main loop sees only 20..40 cell values/s, a fixed count would be random)

// ------------------------------------------------- Loss models (settings.h)
// M_idle(w) = A0 + B0 w^2 from the idle run (wheel lifted, command m0): the firmware
// fits by itself at the end of the run and stores. M_wheel(w_wheel) = A1 + B1 w_wheel^2 from
// the wheel spin-down (command u): J_wheel from the wheel mass, J = WHEEL_J_FACTOR m r^2.
// [lit] 0.85 for a 622 wheel, docs/08-stand.md §6.
constexpr float   WHEEL_J_FACTOR = 0.85f;
constexpr float   WHEEL_MASS_DEFAULT_KG = 1.5f;  // [spec 2026-11-23] default, weigh at the stand
// Assistant spin-down + idle (state.h): drum to this speed, lift the
// wheel; the slip reveals the lifting, after that the wheel spins down freely and
// the drum drives the idle plateaus. [spec] 40 km/h: high enough for
// a long spin-down, still liftable by hand.
constexpr float   WIZ_KMH = 40.0f;
constexpr float   WIZ_SLIP_RATIO = 0.90f;        // [est] below this the wheel counts as lifted
constexpr float   WIZ_TOL_KMH = 1.0f;           // [est] this close to WIZ_KMH counts as "there"
constexpr float   WIZ_WHEEL_MIN_RPS = 0.5f;     // [est] below this the wheel is not turning along, do not form a ratio
constexpr uint32_t WIZ_STEADY_MS = 3000;        // [est] this long steady at WIZ_KMH, then lifting is allowed
constexpr uint32_t WIZ_LIFT_CONFIRM_MS = 500;   // [est] this long below WIZ_SLIP_RATIO, then the wheel counts as lifted
constexpr uint8_t SPINDOWN_MIN_REVS = 30;       // [est] fewer is not a fit
constexpr float   SPINDOWN_MIN_KMH  = 5.0f;     // [est] the spin-down ends below this; standstill takes forever
// [est] A fit whose residual exceeds this is rejected: the clean spin-down of 2026-11-26
// (run 2026-11-26-1542, replayed with the wheel echo filter) left 0.043 rad/s, the same
// data with one phantom pulse 32.8 rad/s.
constexpr float   SPINDOWN_RESID_MAX_RADS = 0.5f;
// SLOWING DOWN = BRAKING. If the setpoint is below the actual value, the duty cycle is
// not lowered -- in FOC that is regeneration into the bus, and the
// power supply absorbs nothing: 43.8 V, fault 1 on the step 40 -> 15 km/h on
// 2026-11-23 (docs/07-safety.md §3.1). Instead brake current
// VESC_BRAKE_CURRENT_STOP until just above the setpoint, then drive again.
// BRAKING WITH THE POWER SUPPLY ON THE BUS (source code finding FW 7.01, docs/07-safety.md
// §3.2d). Every brake entry from motion passes through the regeneration zone
// 0 < |I| < I_short: with the motor enabled, the controller tracks the back EMF as
// duty cycle and therefore never jumps directly into the short circuit. The surge
// lasts milliseconds, the overvoltage fault trips after 1 to 3 PWM cycles via
// l_max_vin, and its energy grows roughly with the square of the speed.
// Measured 2026-11-23: entry at 54 km/h clean, at 56/58/59 km/h fault 1.
// Above BRAKE_MAX_KMH there is therefore NO braking; instead the controller is released until the
// drum has coasted below it; then once VESC_BRAKE_CURRENT_STOP, which lies above
// the short-circuit current and therefore ends in the short circuit (full_brake_hw).
// Duty cycle 0 is NOT a way out: PI ramp with up to 20 A regeneration.
// l_max_erpm_fbrake(_cc) only acts in the BLDC code, not in FOC.
constexpr float   BRAKE_MAX_KMH       = 45.0f;    // [est] 54 ok, 56 not; margin downwards. Raise only in 2-km/h steps while watching vin
constexpr uint32_t BRAKE_FAULT_HOLD_MS = 3000;    // [est] after a controller fault do not brake again for this long (the controller itself discards 500 ms of it)
// RELAY BRAKE (brake.h, docs/04-hardware.md §9.4): EVERY STOP (command s, FAULT,
// end of run) goes via the relays, at any speed: release the controller,
// wait RELAY_DRIVER_OFF_MS (driver off first, then relays -- N2), GPIO40 low ->
// motor shorted until the drum has stood still for RELAY_RELEASE_STILL_MS; then the
// relays pull in again. [spec, builder 2026-11-25] after the brake comparison from
// 35.9 km/h: controller at 20 A and relay short circuit decelerate the same (2.8 km/h/s),
// because 20 A lies above the short-circuit current and the controller ends in the short circuit. The
// relays can do the same without regeneration and without controller faults, at any speed.
// Until then a threshold of 40 km/h applied (RELAY_BRAKE_ABOVE_KMH), below which
// the controller braked. The controller now only brakes DESCENTS IN THE RUN (setpoint lowered), below
// BRAKE_MAX_KMH. Without RELAY_BRAKE_WIRED a stop brakes via the controller as before.
constexpr bool     RELAY_BRAKE_WIRED      = true;   // [spec] coil chain, emergency stop and Q1 wired per §9.1 on 2026-11-25
constexpr uint32_t RELAY_DRIVER_OFF_MS    = 100;    // [est] controller released -> relays drop; one UART command and one controller cycle are milliseconds, 100 is reserve
constexpr uint32_t RELAY_SETTLE_MS        = 100;    // [est] after pulling in, do not drive for this long, contacts finish bouncing
// [est] Relay pulse test (relaytest.h, console X<n>): length of the quiet window and of the
// window after the brake command. 2000 ms covers drop (RELAY_DRIVER_OFF_MS) and pull-in
// (RELAY_RELEASE_STILL_MS after standstill) of one cycle.
constexpr uint32_t RELAYTEST_WIN_MS       = 2000;
constexpr uint32_t RELAY_RELEASE_STILL_MS = 1000;   // [est] this long at standstill, then the relays pull in again
// REJECTED 2026-11-25: descents in the run via the relays, pulling in before the ramp target.
// The weighing arm rattled considerably on opening and closing, mechanically unusable
// (builder). Rule since then: braking is always FULL -- a stop via the relays down
// to standstill, a descent via the controller at 20 A below BRAKE_MAX_KMH, above that
// coasting. Nothing in between.

// GUIDED MEASUREMENT RUN (run.h, docs/11-review.md §2 and §3).
constexpr uint8_t  RUN_POINTS_PER_PLATEAU = 3;    // [spec] R4: scatter per plateau from three points of AVG_REVS_PER_POINT each
constexpr float    RUN_CTRL_MASS_KG  = 1.0f;      // [meas] certified check weight, hangs at CALIB_ARM_M
constexpr float    RUN_CTRL_OK_PCT   = 0.5f;      // [est] check weight: deviation below this is ok
constexpr float    RUN_CTRL_WARN_PCT = 1.0f;      // [est] below this warning W10, above it only Retry or Abort
constexpr uint32_t RUN_CTRL_AVG_MS   = 2000;      // [est] averaging time for the check weight (160 cell values, torque::sample*)
// STOP of the load cell (docs/08-stand.md §7.5): the arm is pressed by hand against the
// stop; the peak must lie ABOVE full scale (otherwise the stop limits within the
// measuring range) and BELOW the permissible overload of the cell (150 % FS).
constexpr float    RUN_STOP_MIN_PCT  = 100.0f;    // [spec] below this the stop engages too early
constexpr uint32_t RUN_TARE_SETTLE_MS = 300;      // [est] wait this long after the tare before the zero check starts
constexpr uint32_t RUN_WARM_LIVE_MAX_MS = 60000;  // [est] an older live value does not go into the warm-up table
constexpr float    RUN_STOP_MAX_PCT  = 140.0f;    // [spec] above this it does not protect the cell (cell: 150 % permissible)
constexpr float    RUN_STOP_BAR_PCT  = 160.0f;    // [spec] scale end of the bar in the web UI
// Measuring range of the torque signal (tire + idle losses) for the green band of the
// utilization scale under Manual (2026-11-26); in % via the torque arm and full scale.
constexpr float    MEAS_TORQUE_MIN_NM = 0.5f;     // [calc] docs/09-measurement-theory.md §5: signal 0.5 to 1.4 Nm
constexpr float    MEAS_TORQUE_MAX_NM = 1.4f;     // [calc] same source, upper end
// [est] Idle-loss points per speed in the pre- and post-run (2026-11-26): W08 compares
// their means. With one point each, run 2026-11-26-1542 compared single values whose
// difference lay within the noise of one point.
constexpr uint8_t  RUN_BASELINE_POINTS = 2;
constexpr float    RUN_DRIFT_LIMIT_N = 0.02f;     // [calc] docs/09-measurement-theory.md §8.8: pre-/post-run on F_roll, above this W08
constexpr float    RUN_ANCHOR_WARN_N = 0.03f;     // [est] anchor drift on F_roll (reference point before and after the plateaus), above this W04
// ZERO POINT, checked twice (docs/11-review.md K2, W6):
//  - after zeroing, turn the drum on by a quarter turn by hand and
//    read again. If the raw value differs, something held the rotor against the cogging torque
//    (wheel on the drum), and the offset contains the cogging torque at that angle.
//  - after the spin-down, wheel lifted, drum stationary: the cell must show the offset
//    again. What is missing is drift of cell, cable or preload over the run.
// Both referred to F_roll. The limit is DELIBERATELY WIDER than the drift criterion
// (0.02 N): at standstill the static friction of the pillow-block bearings holds the rotor against the
// cogging torque, and that much cogging torque (0.01 to 0.02 Nm, on F_roll 0.02 to 0.05 N)
// remains as offset depending on the angle; plus slow noise. Both cancel out
// in Crr (tare is in idle AND measurement) or average out when turning.
// The standstill checks are meant to catch gross errors: cable touching, arm at the stop,
// something pressing. The fine check is pre-run versus post-run (RUN_DRIFT_LIMIT_N).
// On 2026-11-24 the quarter turn gave +0.05 and +0.10 N: motor bearings (between
// rotor and stator) and pillow-block bearings together hold about 0.05 Nm of cogging torque, that
// is 0.1 N on F_roll -- the natural fuzziness of a standstill zero point
// on this machine. Beyond that means: something is holding the arm.
constexpr float    RUN_ZERO_LIMIT_N  = 0.25f;     // [est] above this W13 or W14
constexpr uint32_t RUN_ZERO_AVG_MS   = 2000;      // [est] averaging time of the zero checks
constexpr float    RUN_TURN_DEG       = 90.0f;     // [est] zero check: the drum is turned on by hand this far (monitored via the controller's tachometer)
// [est] Accepted short of RUN_TURN_DEG (builder, 2026-11-26: "88 or so"). The tachometer
// counts 4 degrees per step (VESC_TACHO_PER_REV = 90), so 88 passes, 84 does not.
constexpr float    RUN_TURN_TOL_DEG   = 5.0f;
// Automatic zero with the drum motor (autozero.h, 2026-11-27, builder): the drum runs at
// AZ_SPIN_KMH, first in the measuring direction, then against it; the cell is averaged
// over AZ_SPIN_REVS whole revolutions each. Mean of the two = zero (cogging ripple and
// drum friction cancel), half the difference = drum friction. [spec] speed: builder
// 2026-11-27 ("10 or 15 km/h would be fine"). [est] the rest: revolutions, the band and
// time that count as steady, the spin-up timeout,
// and test-wheel pulses that mean "the wheel is on the drum".
constexpr uint8_t  AZ_POSITIONS        = 2;        // readings: +, - (even, alternating)
constexpr float    AZ_SPIN_KMH         = 10.0f;
constexpr uint8_t  AZ_SPIN_REVS        = 5;
constexpr float    AZ_SPIN_TOL_KMH     = 1.0f;
constexpr uint32_t AZ_SPIN_STEADY_MS   = 3000;
constexpr float    AZ_SPIN_DRIFT_KMH   = 0.05f;    // [est] first vs last revolution of a reading; more = read again
constexpr uint32_t AZ_POS_TIMEOUT_MS   = 60000;
constexpr uint32_t AZ_WHEEL_PULSES_MAX = 1;
// Crawl drive (speedctl::crawl, 2026-11-27). [spec] CRAWL_KMH_MAX: builder 2026-11-27.
// [est] the rest: duty limit (0.136 needed for 15 km/h at the 33.8 V sag of 2026-11-17
// with MOTOR_NO_LOAD_RPM_PER_V), ramp, trim gain, overspeed factor that ends the crawl.
constexpr float    CRAWL_KMH_MAX        = 15.0f;
constexpr float    CRAWL_DUTY_MAX       = 0.15f;
constexpr float    CRAWL_RAMP_PER_S     = 0.02f;
constexpr float    CRAWL_TRIM_PER_KMH_S = 0.002f;
constexpr float    CRAWL_MAX_FACTOR     = 1.5f;
constexpr uint32_t RUN_TURN_STANDSTILL_MS  = 1500;      // [est] then still for this long before the Measure button appears (no automatic start since 2026-11-26)
// Type B components of the uncertainty per point (R9), relative. Origin docs/03-parameters.md.
constexpr float    UNC_K_REL     = 0.002f;        // [est] calibration factor: check weight 0.05 %, averaging, linearity
constexpr float    UNC_LCAL_REL  = 0.001f;        // [est] CALIB_ARM_M: 0.1 mm on 330 mm plus plumb line of the suspension
constexpr float    UNC_N_REL     = 0.005f;        // [est] wheel load: hanging scale on the gantry, arm position
constexpr float    UNC_P_REL     = 0.010f;        // [est] tire pressure: gauge +-0.1 bar or AirSpy offset, at ~8 %/bar Crr sensitivity
constexpr float    UNC_BEARING_REL = 0.0065f;     // [calc] load-dependent drum bearing share 4.7 mNm (docs/06 A11), not corrected
constexpr float    UNC_WHEEL_REL   = 0.005f;        // [calc] J_wheel MEASURED (bifilar +-1.5 %) -> 0.52 % of M_tire (docs/09 §3.2)
constexpr float    UNC_WHEEL_EST_REL = 0.035f;      // [calc] J_wheel only from WHEEL_J_FACTOR (+-10 %): 10 % of 0.25 Nm = 3.5 % of the racing-tire signal
// DESCENT IN THE RUN. [spec, builder 2026-11-25] Brake fully or not at all, the
// controller never brakes (regeneration, fault 1). If the actual value is SPEEDCTL_COAST_ABOVE_KMH
// above the setpoint, the controller is released, and then:
//   1. IS IT WORTH IT? Coast-down time to the setpoint from the loss model (idle, wheel) and
//      DRUM_INERTIA_KGM2. Below DESCENT_MIN_COAST_S it only coasts.
//   2. BRAKE: let the relays drop ONCE, fully.
//   3. RELEASE: when the actual value is at setpoint + lead. Lead = measured deceleration times
//      DESCENT_RELEASE_LATENCY_S plus DESCENT_COAST_REMAIN_KMH, which are deliberately
//      coasted. Pull in the relays ONCE.
//   4. COAST down to setpoint + SPEEDCTL_COAST_UNTIL_KMH, then the controller takes over again.
// At most ONE brake application per setpoint -- every application knocks the weighing arm. On
// the afternoon of 2026-11-25 a bug made the relays switch 26 times: the
// abort condition read the speed from the controller, and with the motor shorted it sees
// 0 rpm. Therefore the speed here is taken ONLY from the drum marks
// (revsrc::angleReference()); without marks there is no braking, only coasting.
constexpr float   DESCENT_MIN_COAST_S        = 5.0f;  // [est] a shorter coast-down is not worth a brake knock
constexpr float   DESCENT_RELEASE_LATENCY_S  = 0.15f; // [est] loop 20 ms + drop-out time of the short-circuit contacts + lag of the deceleration measurement
constexpr float   DESCENT_COAST_REMAIN_KMH   = 1.5f;  // [est] this much is deliberately coasted after release (~3 s)
constexpr uint32_t DESCENT_HANDOVER_MS       = 2000;  // [est] for this long after release the marks apply for coasting and handover; measured, the controller lagged by 0.5 s
constexpr float   DESCENT_SYNC_TOL_KMH       = 1.0f;  // [est] handover only when controller and mark speed are this close
constexpr float   DESCENT_BRAKE_DECEL_KMH_S  = 2.8f;  // [meas] relay brake from 35.9 km/h, 30 -> 10 km/h (2026-11-25); as long as no own measurement exists
constexpr float   SPEEDCTL_COAST_ABOVE_KMH = 2.0f;   // [est] from this much above setpoint it coasts
constexpr float   SPEEDCTL_COAST_UNTIL_KMH = 0.5f;   // [est] down to this much above setpoint
constexpr uint32_t ESTOP_DETECT_MS  = 1000;   // [est] the finding must persist this long
// [TBD] Auxiliary contact of the emergency stop at PIN_ESTOP_FB — not yet wired. As soon as it
// is: set to true. The contact is to pull to ground when triggered (internal
// pull-up; unwired, the pin reads "not triggered").
constexpr bool     ESTOP_FB_WIRED   = false;

// ---------------------------------------------------------------- Pin assignment
// Board: ESP32-S3-DevKitC-1, module WROOM-1 N16R8. Rationale docs/04-hardware.md §4.
//
// Reserved on this board — do NOT use:
//   GPIO 26…32  SPI flash
//   GPIO 33…37  octal PSRAM (the R8 in the name)
//   GPIO 19, 20 USB D−/D+
//   GPIO 0, 3, 45, 46  strapping
//   GPIO 43, 44 UART0 console
//   GPIO 38     RGB LED of the DevKit
//   GPIO 22…25  do not exist on the S3
// The old assignment came from the classic ESP32 and, with 25/26/27/32/33/34/35/22,
// lay almost entirely on reserved or non-existent pins.
// The authority is hardware/schematic.py — all peripherals are wired there,
// including the parts the firmware does not touch yet.

constexpr uint8_t PIN_MARK       = 4;   // drum speed marks (interrupt), 8/rev
constexpr uint8_t PIN_INDEX      = 5;   // drum index mark (interrupt), 1/rev
constexpr uint8_t PIN_WHEEL      = 6;   // speed of the test wheel (interrupt)
constexpr uint8_t PIN_MOTOR_HALL = 7;   // motor Hall, one phase (interrupt)
constexpr uint8_t PIN_HX711_DOUT = 15;  // cell 1, torque
constexpr uint8_t PIN_HX711_SCK  = 16;  // common clock of both HX711
constexpr uint8_t PIN_HX7112_DOUT = 17; // cell 2, wheel load — not yet fitted
constexpr uint8_t PIN_I2C_SDA    = 8;   // S3 default assignment for I²C
constexpr uint8_t PIN_I2C_SCL    = 9;
constexpr uint8_t PIN_SD_CS      = 10;  // microSD, chip select (SD contact 1), 10 kOhm pull-up
constexpr uint8_t PIN_DRIVE_TX   = 18;  // UART1 → motor controller
constexpr uint8_t PIN_DRIVE_RX   = 21;  // UART1 ← motor controller
constexpr uint8_t PIN_ESTOP_FB   = 2;   // display only; disconnection happens in hardware
constexpr uint8_t PIN_STATUS_LED = 48;  // addressable RGB LED of the DevKit, see below

// The D-pad (GPIO 11…14, 39) was removed on 2026-11-22; since 2026-11-25
// 11…13 carry the microSD.

// microSD on SPI2 (FSPI), since 2026-11-25. These are the pins of the IO-MUX direct connection
// of SPI2 (FSPICS0/FSPID/FSPICLK/FSPIQ) — no detour via the GPIO matrix — and
// at the same time the SPI default of the DevKitC-1. Wiring via a microSD-to-SD
// adapter: docs/04-hardware.md §15, hardware/sd_plan.py.
constexpr uint8_t PIN_SD_MOSI    = 11;  // SD contact 2 (CMD/DI)
constexpr uint8_t PIN_SD_SCK     = 12;  // SD contact 5 (CLK)
constexpr uint8_t PIN_SD_MISO    = 13;  // SD contact 7 (DAT0/DO), 10 kOhm pull-up

// Transistor (2N5551) in the coil ground of the brake relays, in series with the emergency stop
// (docs/04-hardware.md §9.1). high = relays pulled in = brake released. 10 kOhm
// base-emitter, so braked during reset.
constexpr uint8_t PIN_BRAKE_RELEASE = 40;

// NO PIN_MOTOR_PTC: the motor's temperature sensor is connected to the TMP input of the
// CONTROLLER, not to the ESP32. The controller limits by itself, the ESP32 reads the value via
// UART (docs/04-hardware.md §11.4). A resistive sensor tolerates only ONE pull-up,
// so there is only one point of evaluation. GPIO 1 thus stays free.
// [TBD] Which sensor type it is remains open (A5) — that only decides which
// curve is set in the VESC Tool, no longer where the wire goes.

// PIN_HX7112_DOUT is reserved and wired in hardware/schematic.py, but not yet used
// by the firmware. It is listed here so that the assignment is maintained in exactly
// one place.
//
// GPIO 1, 14, 39, 41, 42 and 47 remain free.

// ------------------------------------------------------- Storage and raw data
// storage.cpp / rawlog.cpp, since 2026-11-25. Rationale docs/10-firmware.md §11.
//
// SPI clock of the microSD. [spec] 10 MHz: flying leads to the adapter tolerate
// less than the default 20 MHz; an estimated 2.5-3.5 kB/s of raw data are needed. On
// read errors halve this value first. Set explicitly after the card's init
// (storage.cpp sdSetClock): the driver alone knows only 20 MHz and up and fell
// back to 400 kHz until 2026-11-26.
constexpr uint32_t SD_SPI_KHZ          = 10000;
constexpr uint32_t STORAGE_CHECK_MS    = 2000;    // [spec] check SD inserted/removed
constexpr uint32_t STORAGE_STATS_MS    = 10000;   // [spec] recalculate usage (LittleFS walks through all blocks for this)
// Ring buffer in PSRAM between main loop and writer task. [spec] 1 MB: at an estimated
// 2.5-3.5 kB/s (CSV stream since 2026-11-27) about 5 min. It only has to bridge the pauses of the SD (housekeeping of the
// card, removal and insertion), since 2026-11-26 no longer the run down to standstill.
constexpr uint32_t RAWLOG_BUF_BYTES    = 1024u * 1024u;
constexpr uint32_t RAWLOG_CHUNK_BYTES  = 4096;    // [spec] write unit, one flash page / eight SD sectors
constexpr uint32_t RAWLOG_SYNC_MS      = 1000;    // [spec] sync line: wall clock against t
constexpr uint32_t RAWLOG_FLUSH_MS     = 2000;    // [spec] commit the file: at most this much is lost on a power failure
constexpr uint8_t  RAWLOG_STOP_PCT     = 95;      // [spec] recording ends at this usage
constexpr uint16_t RAWLOG_EDGE_RING    = 256;     // [spec] edges between ISR and main loop, power of two
// THE FLASH BLOCKS. During a flash write or erase operation, cache and all interrupts
// outside IRAM are blocked on BOTH cores — including the sensor ISRs
// (attachInterrupt without ESP_INTR_FLAG_IRAM). A sector erase takes tens of ms:
// edges would arrive late or not at all, the main loop would stall. Therefore, since
// 2026-11-26, the flash only takes settings (NVS) and, without SD, the logs at the end
// of a run; raw data go only to the SD (SPI2, blocks nothing). Calculating usage and
// formatting only after this much standstill (no sensor, drive off).
constexpr uint32_t RAWLOG_FLASH_IDLE_MS = 3000;   // [spec]

// ------------------------------------------------------------- WLAN control panel
// The test stand sets up its own access point and serves the web UI
// itself. No internet, no router. Rationale docs/04-hardware.md §13.
constexpr char     WEBUI_SSID[]    = "Pruefstand";  // [spec]
// [spec] WPA2 instead of open. Not for secrecy, but because otherwise anyone in
// range can drive the motor. It is never typed in — it is in the QR code
// (hardware/qr-wlan.png, not in the repo). At least 8 characters, otherwise the AP stays open.
// THE PASSWORD IS NOT HERE, but in firmware/src/secrets.h (git-ignored).
// Template: copy secrets.example.h and enter the password. Without secrets.h
// the firmware builds with the example password from the template.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
constexpr uint8_t  WEBUI_CHANNEL   = 6;             // [spec]
constexpr uint8_t  WEBUI_MAX_CLIENTS = 4;           // [spec]
constexpr uint16_t WEBUI_PORT      = 80;            // [spec] captive portal needs 80
// [spec] mDNS name → http://tire.local/. Short, because it is typed by hand.
//
// WHAT ANSWERS WHAT, so that this does not get mixed up:
//   tire.local   → mDNS (Bonjour). Apple and Windows query ".local"
//                  EXCLUSIVELY via mDNS and NEVER the network's DNS server.
//   any other    → our own DNS server, which answers everything with WEBUI_AP_IP
//   name           (webui.cpp: g_dns.start(53, "*", AP_IP)).
//   4.3.2.1      → directly, without name resolution. The way that always works.
//
// ANDROID cannot do mDNS in the browser — there "tire.local" leads nowhere. Therefore
// the QR code contains the IP and not the name (docs/04-hardware.md §13.4).
constexpr char     WEBUI_HOSTNAME[] = "tire";

// Address of the access point. NOT 192.168.4.1 — and that is not a matter of taste:
// Android's NetworkMonitor discards the connectivity check as soon as the DNS resolves a
// probe host to a PRIVATE address, and does not even send the HTTP request
// (hasPrivateIpAddress(): 10/8, 172.16/12, 192.168/16, 169.254/16 →
// CaptivePortalProbeResult.PRIVATE_IP → failed). Result: "Connected, no
// internet", no portal, no sign-in prompt. That is also why the
// arduino-esp32 example uses 8.8.4.4 and WLED 4.3.2.1.
// [spec] Occupies foreign address space — not nice, but without internet at the stand
// without consequence, and the only way to be recognised on Android at all.
constexpr uint8_t  WEBUI_AP_IP[4] = {4, 3, 2, 1};

// NO captive portal. Rationale in docs/04-hardware.md §13: the portal window is
// not a browser, it closes when swiped away, iOS then disconnects from the network, and Android
// does not even show it for private addresses. Instead the stand answers the
// connectivity check the way the operating system expects — then the network counts as
// fine, the connection stays up, and nothing pops up. The web UI is opened
// via the stuck-on QR code (hardware/qr-ui.png) or any
// address in the browser; the DNS answers everything with its own address.

// SAFETY LIMIT. Dead man's switch: while a run is in progress, the
// operator device must check in. If it stays silent for longer, the run is aborted — a
// shutdown criterion like any other, docs/07-safety.md §4.
// [spec 2026-11-23] 10 s. Before that 2000 ms -- this aborted runs although
// the phone had the page open: the web server of the Arduino library serves
// only ONE client, and an empty pre-connection from iOS holds it for up to 5 s
// (HTTP_MAX_DATA_WAIT); the page's polls wait meanwhile. 10 s lies
// above that and is still a dead man's switch for an operator at the stand.
// Does NOT apply as long as no client has ever been present: then driving is via the console.
constexpr uint32_t LINK_TIMEOUT_MS = 10000;
// Display and message times of console and web UI. Not physics, but also not
// loose numbers in the code.
constexpr uint32_t FAULT_SHOW_MS       = 30000;  // [est] controller fault stays this long in the status line; the controller clears it after 500 ms
constexpr uint32_t NOTE_SHOW_MS        = 5000;   // [est] an error message stays this long
constexpr uint32_t HTTP_SUMMARY_MS     = 5000;   // [est] interval of the HTTP summary on the console
constexpr uint32_t CONSOLE_LIVE_PERIOD_MS = 200; // [est] interval of j<s>
constexpr float    CONSOLE_LIVE_DEFAULT_S = 5.0f;// [est] j without a number
constexpr uint32_t HX711_FRESH_MS      = 1000;   // [est] longer without a value = "not responding" (at 80 SPS that would be 80 values)

// --------------------------------------------------------- Motor controller (VESC)
// Only for the controller variant (env:esp32s3-vesc). The main firmware does not
// use it yet.

constexpr uint32_t VESC_BAUD = 115200;   // [DS] default of the VESC firmware

// [spec] Polling of the measured values. The interval IS at the same time the averaging
// window: the controller delivers currents as the mean since the last poll and
// resets the accumulator on reading. 200 ms = 5 Hz, the same rate as the
// display (docs/10-firmware.md W5).
constexpr uint32_t VESC_POLL_MS = 200;

// [spec] Repetition of the setpoint. The controller shuts off if nothing arrives for longer than
// timeout_msec — default there 1000 ms, set to 300 ms.
// 150 ms allows two dropouts before it takes effect. CAUTION: polls of measured values
// do NOT reset the timeout, only setpoints and COMM_ALIVE do.
constexpr uint32_t VESC_ALIVE_MS = 150;

// SAFETY LIMIT. Derived, since the number of pole pairs has been measured.
//
// The controller calculates in ELECTRICAL revolutions; the mechanical speed is
// ERPM / pole pairs. With MOTOR_POLE_PAIRS = 15 (measured, see above) the
// cap follows directly from MAX_DRUM_RPM and thus automatically follows V_MAX_KMH:
//
//     353 rpm * 15 = 5295 ERPM
//
// The drive is NOT the limiting factor. Measured on 2026-11-17 on the free motor:
// 10 877 ERPM at duty cycle 0.95 and 40 V, linear over ten support points,
// extrapolated 11 450 ERPM = 763 rpm at full modulation. At the drum
// that would be 130 km/h; for 60 km/h duty cycle 0.46 suffices. The limit is a
// specification from docs/07-safety.md, not a property of the machine.
//
// Previously 2400 stood here, derived from the controller's si_motor_poles = 14 under
// the assumption that this was the smallest plausible value. It was 30 poles.
//
// This is NOT a replacement for MAX_DRUM_RPM, but a cap in front of it.
constexpr int32_t VESC_ERPM_MAX =
    static_cast<int32_t>(MAX_DRUM_RPM * MOTOR_POLE_PAIRS);

// DIRECTION OF ROTATION. [meas] Determined at the rig on 2026-11-17, to be confirmed.
//
// With the checked phase assignment (relay block 1:1 as with direct wiring)
// and the measured Hall table, the drum turns in the MEASUREMENT DIRECTION when the controller
// receives NEGATIVE ERPM. Every setpoint is therefore given this sign.
//
// Why not `m_invert_direction` in the controller: then the decision lives in a
// configuration that can be lost on every re-detection or every restore of the
// backup — and it would not be versioned. Here it is in the repo.
//
// CAUTION, two pitfalls, both triggered on 2026-11-17:
//   1. If two phases are swapped in the relay block, the direction reverses, and
//      the Hall table no longer fits — the motor then hums with current but without
//      torque, and reports NO fault (docs/04-hardware.md §10.6).
//   2. A change of direction while running is a BRAKE COMMAND and regenerates. Always
//      wait for standstill first (docs/07-safety.md §3.3).
// [meas 2026-11-23] +1 since the rewiring: pitfall 1 occurred exactly like that,
// the Hall table was re-detected (hardware/vesc-config-fw7.01/README.md), and with
// the old -1 the drum ran BACKWARDS. After every re-detection of the Hall table
// check the sign: command e (turn by hand) or set e1 / e-1 directly.
constexpr int32_t VESC_DIRECTION_SIGN = +1;

// Below this motor temperature the sensor is OPEN, not cold. Measured on
// 2026-11-17: open terminal reports -75.5 C, connected 19.4 C
// (docs/06-open-questions.md A5). On 2026-11-18 an unplugged
// sensor lead showed -82 C -- and because TMP and the three Hall sensors are in the
// SAME connector, that is also a suspicion on the Hall sensors.
constexpr float MOTOR_TEMP_OPEN_C = -20.0f;

// Maximum age of a command from the web UI.
//
// REASON, happened at the stand on 2026-11-18: commands were sent as GET, and a GET
// counts as repeatable -- iOS resends it by itself after a connection
// drop. The connection dropped while the page was loading, and an old
// "v20" arrived again seconds later and set the drive in motion. Every
// command therefore now carries the timestamp the web UI last
// saw; anything older is discarded.
//
// STOP and coasting are EXEMPT from this. A delayed STOP is
// harmless, a discarded one would not be.
constexpr uint32_t CMD_MAX_AGE_MS = 2000;

// Breakaway measurement (speedctl::breakaway). The breakaway torque of the drum is
// NOT measured -- DRUM_LOSS_CONST_NM is the RUNNING friction from the spin-down and
// is not suitable for this. The measurement raises the duty cycle slowly until
// the drum moves, and then aborts immediately.
constexpr float BREAKAWAY_RATE_PER_S  = 0.01f;   // rise of the duty cycle
constexpr float BREAKAWAY_DUTY_MAX    = 0.15f;   // upper limit, then abort
constexpr float BREAKAWAY_DETECT_KMH  = 0.5f;    // from here on it counts as moving
// Direction detection (speedctl::detectDirection, 2026-11-26): change at the load cell
// that counts as a clear sign. [est] Far above the cell noise at standstill (tare σ about
// 12 counts = 0.0003 N) and well below what the drum needs to break away (idle constant
// 0.472 Nm measured 2026-11-17 = 5.6 N at the 84 mm cell arm).
constexpr float DIRDETECT_MIN_N       = 1.0f;

// Learning the direction of rotation by hand (speedctl::learn).
constexpr float    SPEEDCTL_LEARN_MIN_KMH    = 2.0f;
constexpr uint32_t SPEEDCTL_LEARN_TIMEOUT_MS = 20000;

// BRAKE CURRENTS. [meas] Recorded as a curve on 2026-11-17: braked from 50 km/h at each step
// (SET_CURRENT_BRAKE), braking time to 5 km/h and recovered energy
// measured. 911 J were available.
//
//   3 A  31.4 s  +487 J     10 A  14.9 s  +223 J
//   6 A  19.6 s  +409 J     15 A  13.9 s   +36 J
//                           20 A  13.1 s  -140 J
//
// Braking power and regeneration are mutually exclusive, and the reason lies in the
// way SET_CURRENT_BRAKE works: the controller starts SHORT-CIRCUITED and only
// actively regenerates once the short-circuit current EXCEEDS the setpoint.
// Below that it has to regulate the short-circuit current down — which is only possible via
// the bus. Above it, it has to drive additional current, which COSTS bus power.
//
// This gives the most important consequence for operation: a LARGE brake current is
// power-supply-friendly, a small one is not. See docs/07-safety.md §3.2a.

// [spec 2026-11-17] The fastest possible stop is ALWAYS driven, never a
// slow one. Regeneration is therefore not a selection criterion — it occurs or not.
// Hence there is only ONE brake current.
//
// 20 A instead of 15 A, although 15 A would be energy-neutral: the difference in braking time
// is small (13.1 versus 13.9 s), but "fastest possible" is the requirement. More than
// 20 A brings nothing more, the brake is saturated there.
//
// ONLY WITH A BATTERY ON THE BUS. On the power supply the controller trips: on 2026-11-17 with 20 A from
// 40 km/h, fault code 1 (overvoltage) after 0.61 s. At high speed the
// setpoint lies below the short-circuit current, there the controller regenerates — only at
// low speed does it draw. The net balance of -140 J hides this
// initial surge, and a power supply cannot absorb it.
// docs/07-safety.md §3.2b and §3.2c.
constexpr float VESC_BRAKE_CURRENT_STOP = 20.0f;
// Duty cycle limit of the controller, l_max_duty in hardware/vesc-config-fw7.01/mcconf.xml.
constexpr float VESC_MAX_DUTY = 0.95f;            // [ctrl] it accepts no more
// The no-load speed is calculated with the CURRENT bus voltage; below this there is no
// division -- a controller without voltage reports 0 V, not "no value".
constexpr float NOLOAD_VIN_FLOOR_V = 10.0f;       // [est]

// ------------------------------------------- Speed control of the drum
//
// The VESC's speed controller is unusably tuned for this motor, and
// s_pid_min_erpm = 900 blocks the lowest test step (881 ERPM at 10 km/h) —
// docs/06-open-questions.md A15. Driving is therefore done via the DUTY CYCLE,
// with feedforward from the no-load curve and a slow trim on top.
// Tested this way up to 99.2 km/h on 2026-11-17.

// [spec] Ramp of the inner setpoint. 2 km/h per second is 30 s from 0 to 60
// and slow enough that a mistake becomes apparent before it gets fast.
constexpr float SPEEDCTL_RAMP_KMH_PER_S = 2.0f;
// [est 2026-11-23] Over the last SPEEDCTL_APPROACH_KMH before the setpoint the ramp
// flattens down to SPEEDCTL_APPROACH_MIN of the slope. Reason: under load the drum
// lags the full ramp by 3 km/h; if the ramp runs into the target at full
// slope, the drum arrives with momentum -- 1.9 km/h overshoot.
constexpr float SPEEDCTL_APPROACH_KMH = 4.0f;
constexpr float SPEEDCTL_APPROACH_MIN = 0.25f;
// [est 2026-11-23] The ramp runs ahead of the drum by at most this much. Otherwise
// the trim integrates against a large deviation during the whole run-up
// and, on reaching the target, stands 0.04 above the steady-state duty cycle --
// that was the 2 km/h overshoot despite damping and anti-windup.
constexpr float SPEEDCTL_LEAD_KMH = 1.5f;

// Trim as PI-D (since 2026-11-23). Before that a pure integrator with 0.02 per
// (km/h · s): with 494 N wheel load the drum oscillated by +-0.8 km/h with a period of about 10 s,
// because the integrator kept winding up until the zero crossing of the
// deviation. Now: smaller I part, plus a P part on the deviation and a
// D part on the measured acceleration (revsrc::dvdtKmhS), which takes off early
// during run-up. [est] values, tuned on 2026-11-23.
constexpr float SPEEDCTL_KI  = 0.008f;   // duty cycle per (km/h · s)
constexpr float SPEEDCTL_KPP = 0.008f;   // duty cycle per km/h deviation
constexpr float SPEEDCTL_KD  = 0.015f;   // duty cycle per (km/h/s) acceleration beyond the ramp

// [spec] Maximum change of the duty cycle per second.
//
// CAUTION, the asymmetry is the core: turning DOWN is a BRAKE COMMAND. A
// duty cycle below what the current speed needs makes the controller
// regenerate — on the power supply this ends in overvoltage (docs/07-safety.md
// §3.3). Therefore the duty cycle may fall only a fifth as fast as it rises.
constexpr float SPEEDCTL_DUTY_UP_PER_S   = 0.30f;
constexpr float SPEEDCTL_DUTY_DOWN_PER_S = 0.06f;

// [spec] Clamp against runaway of the duty cycle. As long as the current limit
// determines the acceleration, the control deviation stays large and the trim would integrate up to
// the stop — the drum then overshoots the setpoint
// as soon as the current is freed. Happened exactly like that on 2026-11-17.
// The upper limit is the no-load curve of the SETPOINT plus a friction allowance.
constexpr float SPEEDCTL_CLAMP_FACTOR = 1.08f;
// [meas 2026-11-23] 0.02 was only enough without load: with 494 N wheel load the clamp
// was at 0.139 for setpoint 15, and the drum got stuck at 11.9 km/h -- the
// voltage drop under load was missing. 0.08 allowance; whether that leads to
// overshoot at 60 km/h has not been measured yet [est].
constexpr float SPEEDCTL_CLAMP_OFFSET = 0.08f;
// [meas 2026-11-23] Start window: as long as the drum is stationary, the duty cycle may go
// up to here, independent of the ramp. Breakaway under load at 0.139 / 8.3 A;
// without the window the drum stood under current for 10 s until the ramp got that far.
constexpr float SPEEDCTL_START_DUTY = 0.16f;

// [spec] Cycle time of the control loop. Faster brings nothing: the speed comes
// in with VESC_POLL_MS = 200 ms.
constexpr uint32_t SPEEDCTL_PERIOD_MS = 20;

// [spec] Step size of the control buttons.
constexpr float SPEED_STEP_KMH = 5.0f;

constexpr int32_t VESC_ERPM_STEP_SMALL = 100;   // [spec] fine adjustment
constexpr int32_t VESC_ERPM_STEP_LARGE = 1000;  // [spec] coarse approach

// --------------------------------------------------------------- Status LED
// The DevKit's RGB LED is an addressable WS2812, not a simple output —
// driven via neopixelWrite(), which uses RMT internally.
//
// It shows the state of the operator connection: green steady = an operator device
// is checking in, red blinking = waiting. This makes the dead man's switch readable from
// three metres away.
//
// [meas] GPIO 48 confirmed on the installed board (2026-11-15, LED lights up). On
// DevKitC-1 v1.1 the LED would be on GPIO 38 — so this board is v1.0, or at least
// a build with the LED on 48. This settles the earlier TBD.
// Note in case of a board change: driving both pins one after the other does
// NOT work, neopixelWrite() initialises RMT once for the pin of the first call.
constexpr uint8_t  STATUS_LED_DIV       = 6;     // [spec] divide brightness; the
                                                 // LED is dazzling on the table otherwise
// The LED shows the operator connection and the raw-data recording (2026-11-26):
//   blue, blinking slowly   -- waiting for an operator device (dead man's switch open)
//   green, steady           -- an operator device is checking in
//   red, blinking fast      -- raw data are being recorded (connected)
//   red / blue alternating  -- recording, but no operator device
// Visible from three metres away; red is used for nothing else.
constexpr uint32_t STATUS_BLINK_MS      = 1200;  // [spec] half period of the blue waiting blink (700 ms red until 2026-11-26)
constexpr uint32_t STATUS_REC_BLINK_MS  = 300;   // [spec] half period of the red recording blink

// --------------------------------------------------------------------- LCD

constexpr uint8_t LCD_I2C_ADDR = 0x27;  // [TBD] 0x27 or 0x3F
constexpr uint8_t LCD_COLS = 20;
constexpr uint8_t LCD_ROWS = 4;

// ------------------------------------------------------------- Conversions

// ------------------------------------------------- Tire pressure: SKS AIRSPY via BLE
// Protocol from the sister project ~/git/airspy, docs/20-protocol.md (measured there as
// CONFIRMED, 2026-10-13): the sensor advertises as "AIRSPY-<serial number>",
// service 0xFFF0, characteristic 0xFFF4 streams unsolicited at 2 Hz. Each
// 15-byte notification contains a float32 (little endian) at offset 8 and the
// field tag in byte 14: 0x2E pressure in kPa, in the second half 0x22
// temperature °C (offset 0) and 0x1E battery % (offset 7). There is NO pressure data in the
// advertisement; the name is in the scan response, so scan actively.
// [INFERRED] The pressure is a relative pressure (gauge pressure): 0.2 to 0.8 bar on
// a tube that was let bleed. The absolute offset is still open there (§3.4).
// At the stand: EXACTLY ONE sensor; the first one found is taken (tirepressure.cpp).
constexpr char     AIRSPY_NAME_PREFIX[] = "AIRSPY";
constexpr uint16_t AIRSPY_SVC_UUID16    = 0xFFF0;
constexpr uint16_t AIRSPY_CHR_UUID16    = 0xFFF4;
constexpr uint8_t  AIRSPY_TAG_PRESSURE  = 0x2E;
constexpr uint8_t  AIRSPY_TAG_TEMP      = 0x22;
constexpr uint8_t  AIRSPY_TAG_BATTERY   = 0x1E;
constexpr uint8_t  AIRSPY_AVG_N         = 1;      // [spec] 1 = current value, no averaging (2026-11-24: the pressure hardly varies, prefer fresh); >1 = moving average over that many values
constexpr uint32_t AIRSPY_STALE_MS      = 10000;  // [est] after this the value counts as stale (sensor sleeps ~10 min after standstill)
// Connection interval in 1.25-ms units: 400..500 ms instead of 30..50, otherwise the
// coin cell is empty within a day (airspy/docs/20-protocol.md §3.3). A request, not a
// requirement -- the sensor may refuse.
constexpr uint16_t AIRSPY_CONN_ITVL_MIN = 320;
constexpr uint16_t AIRSPY_CONN_ITVL_MAX = 400;
constexpr uint32_t AIRSPY_CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t AIRSPY_MUTE_MS       = 6 * AIRSPY_STALE_MS;  // [est] connected, but silent this long -> disconnect and search again
constexpr uint32_t AIRSPY_SEEN_MS       = 3000;   // [est] only connect if the sensor was heard this recently
constexpr uint32_t AIRSPY_RETRY_MS      = 2000;   // [est] pause after a failed connection attempt
constexpr uint32_t AIRSPY_POLL_MS       = 500;    // [est] cycle of the BLE task in the connected state

constexpr float KMH_TO_MS = 1.0f / 3.6f;
constexpr float MS_TO_KMH = 3.6f;

inline float omegaFromRevPerSec(float rps) { return rps * 2.0f * PI; }

// kmhFromRevPerSec(), revPerSecFromKmh() and isCalibrated() live in settings.h.
// They depend on the drum circumference or the calibration factor, which can be
// overridden at runtime — here they could only see the default value and thus silently
// calculate something different from the rest of the firmware.
