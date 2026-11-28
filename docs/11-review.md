# Review: setup, run and methodology

As of 2026-11-24, evening. This document replaces the two earlier reviews, the run
review of 2026-11-23 ("does a run come out complete?") and the methodology review of
2026-11-24 ("is the result correct?"). It keeps their finding identifiers, **R** for the
run, **K / W / D** for the methodology (critical / important / documentation), so that
the references in code comments and in the other documents stay valid, and it adds the
findings of the first day at the bench with the new firmware, **N**.

Read for this: [09-measurement-theory](09-measurement-theory.md), [05-test-procedure](05-test-procedure.md),
[03-parameters](03-parameters.md), [08-stand](08-stand.md) §5 to §7, [04-hardware](04-hardware.md) §2
and §14, `config.h`, `run.cpp`, `state.cpp`, `torque.cpp`, `revsrc.cpp`, `speed.cpp`,
`wheel.cpp`, `main.cpp`, `analysis/crr.py`. The clickable prototype of the guided run is
[11-run-prototype.html](11-run-prototype.html).

Figures assume r = 0.4523 m (U = 2.842 m), J = 1.94 kg·m² and a good racing tire with
F_roll = 1.54 N, M = 0.70 Nm, unless stated otherwise.

## 0 Where things stand

The measurement principle, reaction torque at the stator, tangential force without
speed, two zero points, pre-run and post-run baseline, A/B/A, is well chosen and cleanly
derived. Both earlier reviews found the same kind of problem: places where calculation,
code, procedure and report did not do the same thing as the theory, and items missing
from the error budget. Almost all of that is now in the firmware and the documents.

What the bench has shown so far (2026-11-24): the guided run was driven from "create
run" through zeroing, end stop, check weight, pre-run baseline, warm-up and into the
plateaus. It aborted twice and hung once, each time for a software reason that is now
fixed (N1 to N3). First measurement points were recorded; they are not evaluated here
because the wheel-loss model and the check weight (N5) are not settled yet.

| Group | done | partial | open | notes |
|---|---|---|---|---|
| R1 – R12 run | R1 R2 R3 R4 R5 R6 R7 R8 R9 R10 R11 | — | R12 is the list of what stays | run exists as a data set, checks, scatter, report |
| K1 – K4 critical | K1 K2 | K3 K4 | — | K3 needs the pendulum, K4 the mark sensor |
| W1 – W9 important | W1 W2 W3 W5 W6 W7 W8 W9 | W4 | — | W4 needs the linearity check |
| D1 – D5 documentation | D1 D2 D3 D5 | D4 | — | D4 needs the scale |
| N1 – N7 bench day | N1 N2 N3 N4 | N6 | N5 N7 | see §5 |

**Still for the human, in this order:** explain the +24 % check-weight deviation (N5),
linearity with two more masses (W4), weigh the wheel load (D4), wire the Hall sensor to
the eight marks (K4), bifilar pendulum for J_wheel (K3), then the first reference
measurement against a published value (C2 in [06-open-questions](06-open-questions.md)).

## 1 What a run does today

Pressing *Measurement* opens the guided run, fourteen steps ([§3](#3-target-sequence-of-the-guided-run)).
Everything that needs hands comes first; from the warm-up on the stand runs alone.

| | 2026-11-23, before | now |
|---|---|---|
| Start | immediately if load and plateaus exist | system check first: controller, motor, cell, factor, lever arm against the arm at calibration, revolution source, pressure, plateaus, load; only what must be present at this point |
| Zero | tare whenever, wheel maybe on the drum | tare only with the wheel lifted, 80 values, then a quarter turn by hand (watched through the controller tachometer) and a second reading; deviation > 0.25 N on F_roll is W13 |
| Calibration | assumed | end stop checked live (100 to 140 % FS), then the zero after it. Since 2026-11-28 calibration is a Setup procedure directly at the cell (200 to 2000 g, linearity in % of load); the check weight in the run (W10, W18) was dropped |
| Sequence | one point per plateau, 15 s settling by the clock | reference point 28.8 km/h first and last, three points per plateau, settling by state (0.3 km/h for 5 s, at most 60 s) |
| Per point | mean only | mean, scatter of the per-revolution means, net acceleration from the regression line v(t), apparent torque `J_eff·dω/dt` subtracted, valid flag, ripple, pressure and tire temperature |
| Warm-up | not part of the run | 30 min at 28.8 km/h, live P and Crr shown, one point per minute, can be cut short (W11) |
| Baseline | A₀/B₀ in NVS without age | pre-run baseline in the run (or adopted with age, W02), post-run baseline optional (W05), drift per plateau, zero check after the run (W14) |
| Report | CSV on USB, lost without a listener | run record in Flash, Markdown and CSV per run, downloadable, warnings numbered, `crr.py` reads the same record |
| Uncertainty | none | pooled scatter over the plateaus (ν stated) plus Type B items from `config.h`, k = 1 |

## 2 Finding register

One paragraph per finding: what was wrong, what was done, what is left. The full
derivations of the methodology findings are in [09-measurement-theory](09-measurement-theory.md),
where they were incorporated.

### 2.1 Run (R)

**R1 A run is a data set, not a serial stream.** Done. Run identifier, run record with
metadata, checks, calibration state, points, warnings; Markdown and CSV in LittleFS under
`/runs/`, served as downloads with a text view. The SD card is not wired, so Flash it is
(decision E4 revised).

**R2 Check before the start.** Done. Checklist with ok / due / missing per line, all into
the report. The list shows only what must be present at that moment; models and wheel
data are asked for in their own steps. The noise limit at standstill is still TBD and is
shown as "logged" (E6).

**R3 Manual steps first, then the machine alone.** Done. Steps 1 to 8 need hands, 9 to 12
run alone, 13 is the one optional manual step at the end (see R6).

**R4 Three points per plateau.** Done. Scatter of the per-revolution means within a point
and over the three points; `RUN_POINTS_PER_PLATEAU` = 3.

**R5 Validity per point.** Done. Mean speed, net dv/dt, duty, current, voltage,
temperatures, duration; invalid when the net acceleration exceeds 0.2 km/h/s (W03) or the
controller faulted. Invalid points stay in the data, flagged.

**R6 Drift within the run.** Done. Reference point 28.8 km/h directly out of the warm-up
and again after the plateaus (anchor drift W04); post-run baseline as optional last step
(W05 when skipped); zero check after the run without hands (W14).

**R7 Warm-up as a measured state.** Done for recording: live P and Crr every minute,
course in the report. The convergence criterion is still to be derived from data (E3).

**R8 Everything into the report.** Done. Header with build, controller firmware,
U, L, factor and arm at calibration (L_cal until 2026-11-28), tare, models with age and origin of J_wheel,
load, tire, pressure cold and warm, AirSpy temperature at start and end (room temperature entry dropped 2026-11-25).

**R9 Uncertainty per point.** Done. Pooled Type A over all plateaus of the run plus the
Type B items `UNC_K/LCAL/N/P/BEARING/RAD(_EST)/J`, stated with k = 1 and ν.

**R10 Numbered warnings.** Done. W01, W10, W18 and W22 (check weight) dropped 2026-11-28. W01 check weight skipped, W02 idle-loss model older than
the session, W03 point invalid, W04 anchor drift above threshold, W05 post-run baseline
skipped, W06 run aborted (step and reason), W07 room temperature not entered (dropped 2026-11-25: temperature from the AirSpy), W08
baseline drift above limit, W09 without wheel model (no longer possible since 2026-11-26), W10 check weight off by ≥ 0.5 % and
the stored factor kept, W11 warm-up cut short, W12 baseline not fittable, W13 zero
depends on angle, W14 zero drifted during the run, W16 end stop outside 100 to 140 % FS,
W17 end-stop check skipped, W18 calibration factor adopted from the check weight, W20 zeroing skipped, existing zero point taken (since 2026-11-25), W19 wheel
model not fittable from the spin-down in step 6 (too few revolutions; since 2026-11-25); W21 zero shifted after the end-stop press, W22 check weight changed by the end-stop press (both since 2026-11-26).

**R11 Evaluation from the run record.** Done. `analysis/crr.py` reads the run record
(column `kind`) and the legacy CSV, calculates like the ESP; a synthetic data set hits
Crr = 0.00370 exactly.

**R12 What remains even then.** Absolute values stay drum-specific
([09-measurement-theory](09-measurement-theory.md) §10); the A/B/A comparison within a session
is the robust result. The tire temperature comes from the AirSpy, not from the tread
(A5). The load-dependent bearing share (0.65 % of the signal) stays in the result until a
rigid-roller reference run. Angle bins exist only with the mark sensor (K4).

### 2.2 Critical (K)

**K1 Apparent torque from residual acceleration.** Done. The gate stood at 0.3 km/h/s
peak, 51 % of the racing-tire signal, and `DVDT_CONST_KMH_PER_S` was used nowhere. Now the
net acceleration over the point is the slope of the regression line v(t), `J_eff·dω/dt`
is subtracted, and a point is valid below 0.2 km/h/s net (`POINT_DVDT_NET_MAX_KMH_S` =
`DVDT_CONST_KMH_PER_S / UNC_J_REL`), where the residual error stays under 1 %. The
report shows M_raw, M_inert and dv/dt per point. The same logic now also governs the
live value (N4).

**K2 Cogging torque in the tare.** Done. A held rotor puts the cogging torque of its
angle into the tare offset, 7 to 30 % of the signal, different every run. Zeroing is
only possible with the wheel lifted and is re-read after a quarter turn; the limit
`RUN_ZERO_LIMIT_N` = 0.25 N on F_roll allows for the few hundredths that bearing static
friction holds of the cogging torque at standstill (0.05 to 0.10 N seen on the bench,
normal). SOP B3 and A2 updated.

**K3 J_wheel estimated, budget assumed measured.** Partial. A measured J can be entered
(`Y<kgm2>`), the budget distinguishes 0.5 % (measured) from 3.5 % (estimated), the report
names the origin, the spin-down uses the integral method (W8). **Open: run the bifilar
pendulum** ([05-test-procedure](05-test-procedure.md) A5.4), or two spin-downs with and
without a known added mass.

**K4 Pseudo-revolutions.** Partial. Without a drum sensor the firmware integrates the
controller speed and the "revolution" is off by up to a whole one at 60 km/h. `speed.cpp`
now counts every 8th edge of the mark sensor as a revolution, with an optional
intermediate magnet mid-gap (pattern "short, short", detected automatically, fixed zero,
self-correcting); residual error ripple/2/N in the budget as long as no sensor is wired.
**Open: wire the Hall sensor to the eight marks** ([04-hardware](04-hardware.md) §8.6).
The pattern detection is checked against the logic, not against real pulses.

### 2.3 Important (W)

**W1 Plateau order.** Done. 28.8 km/h first and last; the anchor drift measures the
whole run.

**W2 Tire pressure.** Done. SKS AirSpy over BLE in its own task, current value (no
averaging), pressure, temperature and battery on the page, per point in the report, cold
and after the warm-up; `UNC_P_REL` in the budget. Worked on the bench. Open: absolute
offset of the sensor against a reference gauge.

**W3 Temperature.** Done. Room temperature at start and end, tire temperature from the
AirSpy.

**W4 Check weight checks drift, not correctness.** Superseded 2026-11-28: the check weight was dropped; the direct calibration measures linearity over 200 to 2000 g, the cell arm enters every torque and has to be measured to ±0.1 mm. Earlier state: Averaging is real now (2 s
of samples), the lever arm is checked against the arm at calibration, and the end stop is
checked before the weight hangs. The weight is the same as at calibration, so it cannot
find an error in `CALIB_ARM_M` (330 mm, measured) and checks nothing in the range where
measurements are made. **Open: linearity with 0.2 and 0.5 kg**, and the cross-check on
the lever arm. See N5 for what the bench showed.

**W5 Scatter without coverage factor.** Done. Pooled variance over all plateaus, ν in
the report, k = 1 stated.

**W6 Zero point at the end.** Done. Automatic after "wheel lifted" in step 13 (W14).

**W7 Drum bearings.** Done. The builder's decision: drum in two pillow blocks on a
15 mm shaft, motor only flanged. The drum bearings lie outside the cradle and sit in A₀;
the load-dependent part is `UNC_BEARING_REL` = 0.65 % in the budget. README, `config.h`,
03, 04 and 09 corrected.

**W8 Spin-down fit.** Done. Integral method ω(t) = ω₀ − a·t − b·∫ω²dt instead of
pointwise differences; residual and N on the console.

**W9 Evaluation and report calculated differently.** Done. One formula (pre-run fit,
post-run only for the drift criterion), `crr.py` on the run record.

### 2.4 Documentation (D)

**D1 Drum radius.** Done. All documents on r = 0.4523 m, J = 1.94 kg·m²; A14 closed.

**D2 L versus L_cal.** Superseded 2026-11-28. With the lever calibration the torque chain
depended only on L_cal (`UNC_LCAL_REL`). Since the direct calibration the factor is N at
the cell and L enters every torque 1:1: `UNC_ARM_REL` (0.1 mm on 84 mm) replaces
`UNC_LCAL_REL`; `CALIB_ARM_M` is gone.

**D3 Error budget.** Done. All items in `plateauStat` and in
[09-measurement-theory](09-measurement-theory.md) §3.2. Realistic today, with K3, K4 and W4
open: 3 to 5 % of the signal per point, enough for A/B/A within a session, not yet for
the curve Crr(v) at 1 to 2 % per point.

**D4 Wheel load calculated, not weighed.** Partial. Documents say gantry and hanging
scale at the axle, origin `calc` until weighed; the run asks for the load in kg as one
field. **Open: weigh.**

**D5 Minor items.** Done. Settling by state, noise "logged", requirements and firmware
document updated, knowledge-base rule removed from CLAUDE.md.

## 3 Target sequence of the guided run

**H** needs hands, **A** runs alone, **E** is an input. This is what `run.cpp` does.

| # | Step | Type | Result in the report |
|---|---|---|---|
| 1 | Create run: profile (since 2026-11-26, [10-firmware](10-firmware.md) §8.1) or tire, ETRTO size, inner tube (both since 2026-11-25; the tube changes Crr by up to 14 % in Schermer's pendulum data), rim and hub (until 2026-11-26 one field "wheel"), pressure, wheel mass, wheel load in kg, note; bifilar pendulum optional and collapsed (without it J is estimated from the mass, since 2026-11-25); fields prefilled from the last run | E | header data |
| 2 | Visual inspection per SOP B0, emergency stop within reach | H | confirmed |
| 3 | System check: controller, motor, cell, factor, arm, revolutions, pressure, plateaus, load, storage. The separate 5 s noise window was dropped on 2026-11-26: it saw the same noise as the tare and had no limit; the noise is now recorded at zeroing (σ and peak-to-peak of the tare values, step 4) | A | table ok / due / missing |
| 4 | Zeroing with the wheel lifted, 80 values; quarter turn by hand watched through the controller tachometer (bar in degrees), second reading on the *Measure* button, which appears only once the quarter turn is done and the drum still (automatic start removed 2026-11-26: it fired while the drum was still creeping); with a zero point already set since boot the step can be skipped (W20) | H+A, skippable | offset, noise σ, zero after the turn or W13 |
| 5 | Since 2026-11-28 without the check weight: end stop of the load cell (live bar, peak 100 to 140 % FS), then the zero is read again (beyond `RUN_ZERO_LIMIT_N` → W21: the overload left an offset; zero again or continue). Until then the 1 kg check weight came first (W01, W10, W18, W22) | H, skippable | peak % FS or W17; W21 |
| 6 | Idle-loss model: adopt (W02 if of unknown age) or drive new — **since 2026-11-25 together with the wheel spin-down**: wheel resting, drum to 40 km/h, lift the wheel while running (detected from the speed ratio), the wheel coasts in the air (A₁, B₁) while the drum runs the idle plateaus (A₀, B₀); brake, wait for the wheel, lower it | H+A | A₀, B₀, points, drift relative to stored (W08); A₁, B₁ or W19 |
| 7 | Wheel-loss model for this wheel. **Part of every run since 2026-11-26** (it depends on tire, tube and rim): fresh from step 6. Exception, a pressure series on the same wheel: step 6 offers "Use stored" only if the stored model comes from the same tire, tube, wheel and ETRTO (pressure may differ). The model stores the inputs of its run (`wm_org` in NVS). No "without model" any more; without a fresh model (W19) only Back to step 6 | H | A₁, B₁, J_wheel with origin (run, tire, tube, wheel, pressure), previous model of the same wheel for comparison, circumference |
| 8 | Wheel placed, load confirmed | H | confirmed |
| 9 | Warm-up 28.8 km/h, 30 min, live P and Crr, one point per minute, can be cut short (W11) | A | P(t), pressure and tire temperature warm |
| 10 | Reference point 28.8, then plateaus, 3 × 10 revolutions each | A | per plateau M, σ, F_roll, Crr raw and corrected, u, ν, valid |
| 11 | Reference point 28.8 again | A | anchor drift ΔM or W04 |
| 12 | Stop: brake below 45 km/h, coast above | A | duration |
| 13 | Lift wheel: zero check (W14), post-run baseline | H+A, skippable (W05) | zero after run, drift pre/post per plateau |
| 14 | Report: summary, warnings, .md and .csv | A | files |

About 50 min with warm-up, of which 40 without operator.

## 4 Report format

Rendered by the ESP from the run record, reproducible with `crr.py`:

```
# Measurement run <id>
Tire · ETRTO · Tube · Wheel · Pressure cold / warm · Load · AirSpy temperature start / end · Build

## Checks                table: Check | Result | Value   (incl. zero, end stop)
## Calibration state     table: Quantity | Value | Origin | As of
## Warm-up               table P(t) per minute with pressure and tire temperature
## Measurement points    table: v | n | M_raw | M_inert | dv/dt | M ± σ | Crr raw | Crr corrected ± u | valid
## Reference / anchor    ΔM, ΔCrr between first and last 28.8 point
## Baseline pre/post     table per plateau, criterion 0.02 N on F_roll
## Zero after run        deviation on F_roll
## Warnings              W01 … with text
## Raw data              reference to the .csv, 21 columns
```

Every number with unit, SI internally, km/h and bar only in the columns named that way
([../CLAUDE](../CLAUDE.md)). No number without origin.

## 5 Findings of the first bench day, 2026-11-24

**N1 The main loop blocked for 980 ms, the control loop dropped out.** Fixed. At the end
of the first baseline point the point record and the angle bins are over thirty single
`print()` calls; with no computer reading USB each one waited the 20 ms CDC timeout. The
controller values were then older than 1 s and `speedctl` released the drive. Now:
8 kB transmit buffer, timeout 0 (drop instead of wait), and the status line `i` names the
longest loop section, so the next blocker is found by reading, not guessing
([07-safety](07-safety.md) §10).

**N2 The state machine hung in POINT after the dropout.** Fixed. Nothing told the state
machine that the control loop had switched itself off; standstill detection only looks
while the loop runs. A stopped control loop in a driving phase is now a FAULT
("Regelung hat abgeschaltet", control loop switched off), and `abortRun` always stops the state machine
([07-safety](07-safety.md) §4).

**N3 Dead ends in the guided run.** Fixed. Several places offered no way on: a missing
idle-loss model blocked the system check, a check-weight deviation had no continue, a
weight still hanging hid the only button. Rule now: a deviation warns and never blocks,
a physical precondition (weight off, drum still) is detected and named, and every
preparation step has *Back*.

**N4 The warm-up never showed power or Crr.** Fixed. The live value gated on the
instantaneous dv/dt from the controller speed, which jitters by ±0.1 km/h between two
200 ms polls at 30 km/h; the gate tripped constantly and cleared the window. Now the net
acceleration over the window decides, as for a point (K1), and a value appears after
1.5 s of filled window rather than after a fixed number of samples
([03-parameters](03-parameters.md) `STEADY_*`, `LIVE_MIN_MS`).

**N5 The check weight read +23.8 %.** Closed 2026-11-28 with the direct calibration (the lever calibration and the check weight were dropped). 1 kg at the 330 mm mark gave 4.007 Nm against
3.237 Nm expected with the stored factor 2.914e-5 N/count (config.h says 2.668e-5 from an
earlier calibration). A 24 % step is more than drift. Candidates: the weight not at the
mark or not plumb, the hook or cord weighed, the arm against the end stop (the end-stop
check now comes first for this reason), the cell cable, a factor calibrated on a
different arm. Since the check weight **is** the calibration procedure, the run now
offers to adopt the reading (W18); before that is done, the linearity check (W4) should
say whether the chain is linear at all.

**N6 The HX711 is polled, not buffered.** Partial. The loop sees 20 to 40 of the 80
values per second while a browser polls. Averages are fine, but every count-based time
(tare, live window, check-weight sample) became unpredictable. Tare, live value and
progress bars now follow time or whichever end comes first; the loss itself remains.
A data-ready interrupt on DOUT or reading the cell in its own task would restore the
80 SPS; worth doing before angle bins are used (K4), which need every sample.

**N7 The operator's browser is Safari on iOS 12.** Partial. No download manager, no
flex gap, sticky only with the webkit prefix. Downloads are served as an unknown type so
the share sheet (AirDrop) appears instead of an inline view; spacing uses margins;
the header uses the prefixed sticky. Open: verify that the share sheet really appears
for the report files on that device.

## 6 Residual error budget and limits

With K1, K2, W1 to W3, W5 to W9 and D1 to D3 in place, the budget in
[09-measurement-theory](09-measurement-theory.md) §3.2 is complete in its items. What keeps
it from the 1 to 2 % per point that the curve Crr(v) needs:

| Item | today | after the open work |
|---|---|---|
| J_wheel estimated (K3) | 3.5 % | 0.5 % with the pendulum |
| Pseudo-revolutions (K4) | ripple/2/N, measured per point | 0 with the mark sensor |
| Calibration factor (W4, N5) | unverified | 0.2 % after linearity |
| Wheel load (D4) | `calc`, 0.5 % assumed | `meas` |
| Pressure offset (W2) | 1 % assumed | checked against a gauge |

The comparison of two tires within one session (A/B/A, same wheel, same load) is robust
already: everything above cancels or is common to both. Absolute values remain
drum-specific; the curvature correction √(1 + r_tire/r_drum) ≈ 1.32 for 28-622 on the
Ø 0.905 drum is an approximation from car tires, usable only as the expected order
"3 to 4 % below the reference database".

## 7 Order of follow-up

| # | Measure | Effort | Gains |
|---|---|---|---|
| 1 | N5: find the 24 %, then W4 linearity with 0.2 and 0.5 kg | 1 h on the stand | a factor one can trust |
| 2 | D4: weigh the wheel load with the hanging scale | 15 min | `meas` instead of `calc` |
| 3 | K4: wire the Hall sensor to the eight marks, intermediate magnet as preferred | hardware available | true revolutions, angle bins |
| 4 | K3: bifilar pendulum, enter J with `Y` | 1 afternoon | 3.5 % → 0.5 % |
| 5 | N6: HX711 by interrupt or own task | 1 h firmware | all 80 SPS, predictable timing |
| 6 | W2: AirSpy against a reference gauge | 10 min | pressure offset known |
| 7 | E3 / E6: warm-up criterion and noise limit from the first ten runs | data | fewer TBD |
| 8 | C2: first reference measurement against a published value | 1 session | the absolute number means something |

Before item 8 the stand measures reproducibly within a session; the absolute number has
too many open items to interpret a comparison with the database.
