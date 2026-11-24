# Methodology review: setup, calculations, checks, error analysis

As of 2026-11-24, commit 99d8959. Second pass after
[11-run-review](11-run-review.md), this time not "does the run go smoothly"
but **"is the result correct"**: physics, calculation chain, calibration, checks,
error budget. Read: [09-measurement-theory](09-measurement-theory.md),
[05-test-procedure](05-test-procedure.md), [03-parameters](03-parameters.md),
[08-stand](08-stand.md) §5 to §7, [04-hardware](04-hardware.md) §2 and §14,
`config.h`, `run.cpp`, `state.cpp`, `torque.cpp`, `revsrc.cpp`, `wheel.cpp`,
`analysis/crr.py`.

Context: the measurement principle (reaction torque at the stator, tangential force without
speed, two zero points, pre-run and post-run baseline, A/B/A) is well chosen and cleanly
derived. The findings below concern places where calculation, code and report
**do not do the same thing** as the theory, and items **missing** from the error budget.
Numbers **K** = biases the result systematically, **W** = important for the
targeted 1 to 2 % per point, **D** = documentation contradicts itself or the code.
Figures assume r = 0.4523 m (U = 2.842 m), J = 1.94 kg·m², good racing tire
F_roll = 1.54 N, M = 0.70 Nm, unless stated otherwise.

## 0 Status of the follow-up, 2026-11-24

Builder's decisions: drum in two pillow-block bearings (15 mm shaft), motor only
flanged on, not suspended separately (W7 → outside, done). Drum circumference 2.842 m applies
everywhere (D1). Calibration mark at 330 mm is measured (W4 partial). Tire pressure via the
SKS AirSpy (W2). Wheel load initially with gantry and hanging scale at the axle (D4), later two
load cells.

| Finding | Status | where |
|---|---|---|
| K1 Apparent torque | **done**: `J_eff·dω/dt` subtracted per point, gate on net acceleration 0.2 km/h/s, M_raw/M_inertia/dv/dt in the report | `state.cpp`, `config.h` |
| K2 Cogging torque in the tare | **done**: zeroing only with wheel lifted, re-reading after a quarter turn (W13), SOP B3/A2 | `run.cpp`, 05, 09 §5.3 |
| K3 J_Rad estimated | **partial**: measured J configurable (`Y<kgm2>`, setup), budget distinguishes 0.5 % / 3.5 %, report shows origin. **Open: run the bifilar pendulum.** | `settings`, `wheel.cpp`, 03 |
| K4 Pseudo-revolutions | **partial**: every drum sensor delivers true revolutions — the mark Hall sensor via every 8th edge, with optional intermediate magnet (pattern "short, short", detected automatically, fixed zero point, self-correcting) or an index Hall sensor; residual error ripple/2/N in the budget as long as none is present. **Open: wire the Hall sensor to the 8 marks, intermediate magnet as preferred** ([04-hardware](04-hardware.md) §8.6). Pattern detection only checked against the logic, not against real pulses. | `speed.cpp`, `revsrc.cpp` |
| W1 Plateau order | **done**: reference point 28.8 directly from the warm-up and after the plateaus, drift = difference | `run.cpp` |
| W2 Tire pressure | **done**: AirSpy via BLE (own task), pressure/temperature/battery on the measurement page, per point in the report, cold and after warm-up; `UNC_P_REL` in the budget. **Open: test on the stand with a real sensor; absolute offset.** | `tirepressure.cpp`, 03 §9 |
| W3 Temperature | **done**: room temperature at start and end (step 13), tire temperature from the AirSpy | `run.cpp`, UI |
| W4 Check weight | **partial**: averaging over 2 s for real (160 values), lever arm check against arm at calibration (blocking). **Open: linearity with 0.2 / 0.5 kg, cross-check at the lever arm.** | `run.cpp`, `settings` |
| W5 Scatter | **done**: pooled variance over all plateaus, ν in the report, k = 1 stated | `run.cpp`, `crr.py` |
| W6 Zero point at the end | **done**: automatic after "wheel lifted" in the post-run step (W14) | `run.cpp` |
| W7 Drum bearings | **done**: README, config.h, 03, 04, 09 moved to "outside, in A₀"; `UNC_BEARING_REL` in the budget | docs |
| W8 Spin-down fit | **done**: integral method ω(t) = ω₀ − a·t − b·∫ω²dt, residual and N in the console | `wheel.cpp` |
| W9 Evaluation | **done**: `crr.py` reads the run record and calculates like the ESP (synthetic test hits 0.00370 exactly); old format stays readable | `analysis/` |
| D1 Radius | **done**: all documents on 0.4523 m, J = 1.94, A14 closed | docs |
| D2 L vs. L_kal | **done**: `UNC_LCAL_REL` instead of `UNC_L_REL`, arm at calibration stored and checked | `config.h`, `settings` |
| D3 Budget | **done**: all items in `plateauStat` and 09 §3.2 | `run.cpp`, 09 |
| D4 Wheel load | **docs done** (gantry + hanging scale, origin `ber`). **Open: weigh.** | 03, 05, 09 §5.2 |
| D5 Minor items | Settle by state, noise "logged", requirements/10-firmware updated; CLAUDE.md rule on the knowledge base unchanged | — |

Not yet run on the stand. Open for the human: wire the mark Hall sensor, bifilar pendulum,
linearity, weigh the wheel load, AirSpy test, reference measurement (C2).

## 1 Critical

### K1 The apparent torque from the residual acceleration is not subtracted, only gated too coarsely

[09-measurement-theory](09-measurement-theory.md) §1.7 derives correctly: during a speed change
the cell sees `J·dω/dt`. [03-parameters](03-parameters.md) §5 sets `DVDT_CONST_KMH_PER_S` =
0.010 km/h/s as the validity limit for this (1 % of the racing-tire signal). **This constant
is not used anywhere in the code.** `state.cpp` marks a point as valid with
`STEADY_DVDT_KMH_S` = **0.3 km/h/s**, i.e. 30 times that:

| Limit | dω/dt | J·dω/dt | of racing-tire signal (0.70 Nm) |
|---|---|---|---|
| 0.010 km/h/s (docs) | 0.0061 rad/s² | 0.012 Nm | 1.7 % |
| **0.3 km/h/s (code)** | **0.184 rad/s²** | **0.357 Nm** | **51 %** |

The gate is therefore ineffective for validity. What matters anyway is not the
peak value |dv/dt| but the **net change of speed over the averaging window**:
the mean torque over a window of duration T is shifted by `J·(ω_end − ω_start)/T`.
According to [03-parameters](03-parameters.md) §8.2 the control holds "±0.2 km/h". If
0.2 km/h of that is net over a window of 10 revolutions:

| Plateau | T (10 rev.) | Δω at 0.2 km/h | Apparent torque | of signal |
|---|---|---|---|---|
| 15 km/h | 6.8 s | 0.123 rad/s | 0.035 Nm | 5 % |
| 28.8 km/h | 3.6 s | 0.123 rad/s | 0.067 Nm | 10 % |
| 60 km/h | 1.7 s | 0.123 rad/s | 0.140 Nm | 20 % |

This is random in sign per point, so it partly shows up in the scatter of the three
points, but not completely: after the overshoot the drum approaches from
above, the trend over the first points is systematically negative. `SETTLE_MS` = 15 s
is shorter than the 17 to 22 s that [03-parameters](03-parameters.md) §8.2 states until the
target is reached.

**Proposal:** correct instead of gate. Capture ω at the first and last index pulse of the point
(available in `revsrc`) and calculate `M_korr = M_mittel − J·(ω_Ende − ω_Anfang)/T`.
J is known to ±5 %, the remainder is < 0.01 Nm. Additionally put the gate on the
net change, not on the peak value, and separate `STEADY_DVDT_KMH_S` from the
validity decision (it is a control quantity). Report: state Δω per point
and the applied correction.

### K2 The static tare contains the cogging torque when the rotor is held

[09-measurement-theory](09-measurement-theory.md) §1.6 correctly states that cogging torque and iron losses
lie inside the cradle and are **not measured during the run**: over one revolution
they average out. At **standstill** this does not hold. Free-body diagram as in
[08-stand](08-stand.md) §7.2: `T_arm = T_last`. If the rotor stands at an arbitrary
angle, static friction (tire on the drum, drum bearings) holds it at exactly that
position against the cogging torque, and the arm carries the cogging torque of that
angle. It ends up in the tare offset.

Order of magnitude: cogging torques of 350 W hub motors are typically 0.05 to 0.2 Nm
peak, i.e. 7 to 30 % of the racing-tire signal. An error that differs from run to run,
because the drum stops at a different position each run.

The tare is harmless only if the rotor is **free** and can settle into a detent
position. Then only what the static friction of the drum bearings can hold of the
cogging torque remains (calculated: a few mNm). In the guided run the zeroing step
(4) comes **before** lifting the wheel (6, substep 1); whether the wheel rests on the drum during
the tare depends on the operator.

**Proposal:** zeroing only with the **wheel lifted**, nudge the drum by hand beforehand
and let it settle. As an in-run check: after zeroing, turn the drum a quarter
turn further and read again. If the raw value deviates by more than the noise,
something is holding the rotor. Same rule for the check weight and the
calibration. Add to [05-test-procedure](05-test-procedure.md) B3 and A2.

### K3 The wheel-loss model is calculated with an estimated J, the budget assumes a measured one

`wheel.cpp` calculates `J_Rad = 0.85 · m · r_Rad²` (`WHEEL_J_FACTOR`, origin "literature").
The bifilar pendulum ([09-measurement-theory](09-measurement-theory.md) §9.2, SOP A5.4) has not been run, there is
no input path for a measured J. The error budget in [09-measurement-theory](09-measurement-theory.md)
§3.2 and `UNC_RAD_REL` = 0.005 however assume ±1.5 % on J. For a factor from the
literature ±5 to 10 % is realistic, and the same table says what that costs:
at ±10 % **0.054 N = 26 % of the resolution target**, 3.5 % of the signal.

In addition: the spin-down fits `−J·dω/dt = A₁ + B₁·ω²`. J enters **both**
coefficients linearly, the error transfers 1:1 to the entire wheel correction
(at 60 km/h about 0.25 Nm, i.e. 35 % of the racing-tire signal).

**Proposal:** run the bifilar pendulum, store J as a setting (`settings`),
`WHEEL_J_FACTOR` only as a plausibility check. Until then set `UNC_RAD_REL` honestly
to 0.03 to 0.035 and state "J estimated" in the report. A second, independent
path that does not need J at all: spin-down **once with and once without a
known additional mass** on the rim (two runs, J from the difference of the
decelerations); this is the same method that [05-test-procedure](05-test-procedure.md) A5.3 prescribes for the
drum.

### K4 The "whole revolutions" are calculated revolutions without drum marks

Requirement M2 and [09-measurement-theory](09-measurement-theory.md) §3.8 justify revolution-synchronous
averaging by its **exact** cancellation of all rotational orders. Without wired
drum marks (as of 2026-11-23, `revsrc` on `SRC_VESC`) the firmware integrates the
controller speed, which arrives every 200 ms, and generates an index pulse from it. At
60 km/h one revolution takes 170 ms, so the index is off from reality by up to a whole
revolution. Averaging over 10 "revolutions" is then averaging over a
time window of arbitrary phase, and a first-order disturbance
with amplitude a remains in the mean at up to `a/10`. At 20 % ripple
(joint, imbalance, tire out-of-roundness) that is 2 % of the torque, random per point.
The "scatter of the revolution means" (R4) is thus not a scatter of
revolution means either.

This is known ([05-test-procedure](05-test-procedure.md) "status today", [11-run-review](11-run-review.md)
R12), but the consequence for the budget is stated nowhere. The Hall housing exists
(`hardware/hall-gehaeuse.*`).

**Proposal:** wire the marks before publishing absolute values. Until then
measure the ripple (peak-to-peak of the raw values within a point is in the
ring buffer) and carry the residual error `a/N_Umdr` in the report as a Type B item.
Alternatively raise `AVG_REVS_PER_POINT` at high speed (30 revolutions at
60 km/h cost 5 s).

## 2 Important

### W1 The plateau order mixes speed and tire temperature

Warm-up 30 min at 28.8 km/h, then plateaus **ascending from 15 km/h**. The tire
cools during the 15 km/h plateau and warms up to 60 km/h far beyond the
warm-up state. The curve Crr(v) to be resolved changes
between 14 and 36 km/h by 11.6 % ([09-measurement-theory](09-measurement-theory.md) §3); a temperature dependence
of the same order lies on top and is not balanced by 15 s of settling
time (thermal time constant of a tire: minutes). The anchor point (R6)
compares the state at the end with the **second** plateau, not with the state
directly after the warm-up.

**Proposal:** order 28.8 → 15 → 40 → 50 → 60 → 28.8: the reference point is
measured directly out of the warm-up and repeated at the end, the anchor drift then
measures the whole run. For the curve Crr(v) itself two passes, ascending and
descending, and state the hysteresis between them. Log tire temperature per plateau
as soon as the IR sensor is there (A5 open); until then at least
`SETTLE_MS` of 60 s per plateau for the reference point.

### W2 Tire pressure is missing from the error budget, and it changes during the run

For racing tires in the range 5 to 7 bar Crr depends on pressure in the order of 5 to 10 %
per bar (readable from the published pressure series of the reference database,
tire-dependent). From this two items that are not in [09-measurement-theory](09-measurement-theory.md) §3.2:

| Item | Size | Effect on Crr |
|---|---|---|
| Pressure gauge ±0.1 bar | 1.5 to 2 % of pressure | 0.5 to 1 % |
| Pressure rise during warm-up (air warms by 10 to 20 K) | 3 to 7 % of absolute pressure | 1 to 3 % |

The second item is systematic and tire-dependent (thin-walled tires warm up
differently), it distorts precisely the A/B comparison. The SOP measures pressure only
**before** the run (B2).

**Proposal:** measure pressure again after the warm-up and put it in the report; decide
whether to readjust warm (the reference database sets cold). Both values as
mandatory fields in steps 1 and 9 of the run. Pressure uncertainty as a Type B item in
`plateauStat`.

### W3 Ambient and tire temperature: only once and by hand

Room temperature is requested in step 1, not at the end. [09-measurement-theory](09-measurement-theory.md)
§8.5 justifies the A/B/A protocol with the lack of temperature constancy, but whether the
temperature has drifted between A and B is stated nowhere later. An I²C sensor on the
frame costs an afternoon and delivers one value per point. Until then: temperature
at the end as a second mandatory field, and both into the report.

### W4 The check weight checks drift, not correctness

Calibration and check weight use the **same** weight on the **same** arm
(1 kg at 330 mm, 79 % FS). This catches drift, shifted cables and cell defects, but
no error in `CALIB_ARM_M`, and it checks nothing in the range where measurements are made
(0.5 to 1.4 Nm = 12 to 35 % FS). [03-parameters](03-parameters.md) §1 says of the 330 mm
"hole not yet drilled, enter the measured distance after drilling" and §2
at the same time "calibrated 2026-11-23 at 330 mm". Whether 330 is a nominal dimension or a measured value
must be clear from the document; the whole torque chain depends on it (see D2).

**Proposal:** linearity once with three masses (SOP A2.5 has not been done yet),
plus a **second** check with a different lever or different mass that lies in the measurement
range (e.g. 0.2 kg at 330 mm = 0.65 Nm). Cross-check SOP A2.8 (same mass on the
lever arm, half the force) checks the arm ratio independently. Measure `CALIB_ARM_M` with
a caliper from the pivot to the suspension point and enter it as `gem`.

### W5 Scatter from three points without coverage factor

`plateauStat` calculates `u_A = s/√3`. At n = 3 the standard deviation itself is uncertain to
about ±50 %; for 95 % coverage the t-factor would be 4.3 instead of 2. The report
shows "± u" without stating factor or degrees of freedom.

**Proposal:** either state the t-factor for ν = 2, or **pool** the variance of the points over
all plateaus of a run (ν = 10 at 5 plateaus × 3 points) and additionally
treat the scatter over repeated runs (SOP B9.4, three repetitions) as the actual
Type A uncertainty. Add "k = 1" or "k = 2" in the report.

### W6 No automatic zero-point check at the end

The post-run baseline (R6) is optional because it needs hands. Part of what it checks
works without hands: once the drum has stopped after coasting down, the cell must show the tare offset
again (with the caveat from K2: rotor free). Deviation = zero-point drift of the
cell, the cable, the preload. Costs 5 s in the "stop" step, catches half
the causes from [09-measurement-theory](09-measurement-theory.md) §8.8 and belongs in the report as its own line.
Additionally: if the cell sits near the motor, read the cell temperature too (zero and
span temperature coefficients of typical cells ~0.02 % FS/K; at 10 K and 49 N that is
0.1 N at the cell = 0.008 Nm = 1.2 % of the signal).

### W7 Drum bearings: inside or outside? Docs contradict themselves, load dependence remains

[README](../README.md), `config.h` (comment on `DRUM_LOSS_CONST_NM`) and
[03-parameters](03-parameters.md) §1 say the drum bearings lie **inside** the cradle and
cancel out. [04-hardware](04-hardware.md) §2 describes the setup: drum on its
**own shaft** in two 6902 bearings on the stand, motor coupled via flange. Thus the
drum bearings lie **outside** and are measured, as [09-measurement-theory](09-measurement-theory.md) §1.6
also correctly tabulates. What cancels out is only motor bearings and cogging torque.

Consequences: the constant part A₀ of the idle-loss curve contains the drum-bearing friction
under drum weight; the **load-dependent increase** under 494 N (A11, calculated
4.7 mNm, 0.65 % of the signal) is in the result. That is small and accepted as a residual
error, but must appear as a Type B item in `plateauStat`, and the three wrong
passages are to be corrected.

### W8 Spin-down of the wheel: pointwise differentiation, contrary to its own rule

[09-measurement-theory](09-measurement-theory.md) §9.3 and [08-stand](08-stand.md) §6.4: "do not differentiate
pointwise, fit a model over the whole spin-down". `wheel.cpp` forms
`(ω_k − ω_{k−1})/dt` from two consecutive pulse intervals and fits these
differences linearly in ω². With one magnet per revolution dt is 0.13 to 0.8 s, the
difference of two periods is noisy with the timestamp resolution and the magnet position; the
fit is unweighted although the scatter varies strongly with ω. The regression is
also without residual, number of points and uncertainty of the coefficients.

**Proposal:** the equation of motion has a closed-form solution: with
`k = √(A₁B₁)/J` and `ω* = √(A₁/B₁)`, `ω(t) = ω*·tan(arctan(ω₀/ω*) − k·t)`.
Either fit ω(t) directly nonlinearly (on the computer, from the pulse timestamps),
or store the cumulative times per revolution in the ESP and fit `1/ω` over t.
At minimum: log residual RMS and number of points so that a poor fit
is visible.

### W9 Evaluation and report calculate differently; crr.py does not read the run record

- `run.cpp` corrects with the **pre-run fit** A₀/B₀ and uses the post-run baseline only for the
  drift criterion. `crr.py` interpolates **in time** between pre-run and post-run baseline
  ([09-measurement-theory](09-measurement-theory.md) §8.11 describes only the second). With the
  criterion of 0.02 N met, the two differ by at most 0.01 N, but two paths for
  the same number are the error that CLAUDE.md warns against.
- [09-measurement-theory](09-measurement-theory.md) §8.11 claims that `crr.py` subtracts the wheel-loss model.
  It does not; A₁/B₁ do not appear there.
- `crr.py` expects the serial CSV (`t_ms, note=baseline|measure`). The guided
  run writes `kind=baseline_pre|measure|anchor|baseline_post`, `t_s`, without a
  `load_N` column. The evaluation chain for the new report is thus broken
  (R11 open).

**Proposal:** switch `crr.py` to the run record, one correction formula (fit or
interpolation, but then both the same) and a test data set with which ESP report
and Python must arrive at the same number.

## 3 Documentation and numbers

### D1 The drum radius has been 0.4523 m since 2026-11-23, the calculations are based on 0.4665

U = 2.842 m is measured and in `config.h`. [03-parameters](03-parameters.md) §1 additionally lists
`r = 0.4665` as "ber" (that would be U = 2.931), and [09-measurement-theory](09-measurement-theory.md) calculates
all examples, the error budget and the torque tables with 0.4665 (six places in
the docs). All torques are thus stated **3.1 % too high**, e.g. Contact Urban
1.041 instead of 1.074 Nm. A14 in [06-open-questions](06-open-questions.md) is answered by the
measurement and can be closed; the line J = 2.07 (Ø 933) in
[03-parameters](03-parameters.md) §1 is to be deleted.

### D2 The torque chain depends only on L_kal, not on L. The budget counts L anyway

Calibration is `k = m·g·L_kal / (L · Δcounts)`, calculation `M = counts·k·L`. L cancels:
`M = counts · m·g·L_kal / Δcounts`. That is the advantage
[09-measurement-theory](09-measurement-theory.md) §5.1 describes. It follows:

- `UNC_L_REL` = 0.0012 (0.1 mm on 84 mm) is the wrong item. Correct is the
  uncertainty on **L_kal** (0.1 mm on 330 mm = 0.03 %) plus the plumb direction of the
  suspension.
- A `TORQUE_ARM_M` (NVS) changed afterwards without recalibration shifts
  every torque linearly. The report should record L **at the time of calibration**
  and warn if the current value deviates.
- "84 mm, stated, caliper accuracy not confirmed" is thus uncritical for
  M, but critical for `MAX_FORCE_N` and the utilization of the cell.

### D3 Error budget: missing and duplicate items

[09-measurement-theory](09-measurement-theory.md) §3.2 and `plateauStat` read together:

| Item | in budget §3.2 | in `plateauStat` | should be |
|---|---|---|---|
| Residual acceleration J·Δω/T (K1) | no | no | after correction residual ±5 % of J·Δω/T |
| Cogging torque in the tare (K2) | no | no | 0 by procedure, otherwise up to 0.15 Nm |
| Pseudo-revolutions (K4) | no | no | a/N, measured |
| J_Rad estimated (K3) | ±1.5 % | 0.5 % | 3 to 3.5 % until the pendulum |
| Tire pressure (W2) | no | no | 1 to 3 % |
| Temperature tire/room (W3) | no | no | at least log |
| Cell temperature (W6) | no | no | ~1 % per 10 K |
| Drum bearings load-dependent (W7) | yes, 0.65 % | no | 0.65 % |
| Lever arm L (D2) | yes, 0.25 % | yes, 0.12 % | replace with L_kal 0.03 % |
| Wheel load | 0.5 % | 0.5 % | ok, see D4 |
| Calibration factor | 0.2 % | 0.2 % | ok after linearity check, open before |

The bold sum "0.025 N = 12 % of target" in §3.2 cannot be held with the missing
items; realistic today is 3 to 5 % of the signal per point
(0.05 to 0.08 N), i.e. 25 to 40 % of the resolution target ΔCrr = 0.0005. That suffices for
A/B/A within the same session, not for the curve Crr(v) with 1 to 2 % per point. After
K1 to K4 one gets close to the budget.

### D4 Wheel load: SOP says weigh, what was done is calculate

[05-test-procedure](05-test-procedure.md) B1 and [09-measurement-theory](09-measurement-theory.md) §5.2: platform scale on the
drum. [03-parameters](03-parameters.md) §6: `LOAD_DEFAULT_N` = 494.3 N "gem" from
7.2 kg + 10.4 kg · 84/20.2. That is calculated, not weighed, and the lever ratio
4.16 depends on 20.2 cm (±0.5 mm = 0.25 %) and 84 cm (±5 mm = 0.6 %). Run both paths
against each other once; if they differ by more than 1 %, the lever geometry or
the scale is wrong. Origin then honestly `ber` or `gem`.

### D5 Minor inconsistencies

- Check weight: `RUN_CTRL_AVG_MS` = 2000 ms is waited out, then the display ring with
  40 values (0.5 s) is read. Whoever wants to average 2 s must collect 160 values.
- "Noise at standstill" is always `ok` because the limit is TBD. Until it is set, show it as
  "logged" instead of "ok".
- `SETTLE_MS` = 15 s versus 17 to 22 s settling time ([03-parameters](03-parameters.md) §8.2):
  either 25 s, or settle on `|v − setpoint| < 0.2 km/h over 5 s` instead of on the clock.
- [01-requirements](01-requirements.md) §3.2 "pivot at contact-patch height" versus [08-stand](08-stand.md)
  §5.2 "at wheel-axle height". Wheel-axle height is correct because the free wheel transmits the forces through
  the axle; F1 still states U = 3.000 m.
- Curvature correction (C1): common for car tires is
  `Crr_Trommel/Crr_eben ≈ √(1 + r_Reifen/r_Trommel)` (Clark). For 28-622 on
  Ø 0.905 m that is 1.32, on the Ø 0.77 m reference drum 1.37, ratio 1.04.
  That is an approximation from a different tire class, **not a measured value**, but usable as
  the expected order of magnitude "our values 3 to 4 % below the database" for the
  plausibility anchor (SOP B9.5).
- The first rule in CLAUDE.md (knowledge base sync) no longer has a counterpart; the
  branch has been deleted since 2026-11-22.

## 4 Order of follow-up

| # | Measure | Effort | gains |
|---|---|---|---|
| 1 | K1: subtract J·Δω/T per point, gate on net change | 1 h firmware | up to 20 % systematic |
| 2 | K2: zeroing only with free rotor, check by turning the drum further | SOP + 20 lines | up to 30 % per run, random |
| 3 | W6: zero-point check after spin-down, automatic | 20 lines | drift visible without hands |
| 4 | D1/D2/W7: numbers on r = 0.4523, L_kal instead of L, drum bearings outside | docs | consistency |
| 5 | W2/W3: pressure and temperature after warm-up as mandatory fields | UI | 1 to 3 % visible |
| 6 | W1: plateau order 28.8 first and last | settings | anchor measures the whole run |
| 7 | K3: bifilar pendulum, J as a setting; until then `UNC_RAD_REL` 0.03 | 1 afternoon | 3 % honest instead of 0.5 % claimed |
| 8 | W4: linearity with three masses, second check in the measurement range | 1 h on the stand | calibration factor reliable |
| 9 | K4: wire the drum marks | hardware available | M2 met, angle bins |
| 10 | W8/W9: spin-down fit over ω(t), `crr.py` on the run record, one calculation path | Python | reproducible evaluation |

After that the first reference measurement against a published value (C2) is worthwhile. Before that the
stand measures reproducibly within a session, but the absolute number has too many open
items to interpret a comparison with the database.
