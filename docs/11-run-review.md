# Run review: calibrating, checking, recording

As of 2026-11-23. Question: **does a measurement run come out "complete"?** That is: on
every run, is everything calibrated or checked, is every deviation measured instead of
assumed, and is there a report at the end that can be read and evaluated without the
test stand?

> **As of 2026-11-23, evening: implemented** in `firmware/src/run.cpp` (step machine,
> run record, report as Markdown and CSV in LittleFS), `state.cpp` (sequences with
> scatter per point, phase HOLD), `index.html` (overlay behind *Measurement*).
> Decisions E1 to E6 as proposed; E4 Flash instead of SD card, because the SD card is
> not wired. Clicked through against a mock-up, **not yet driven on the stand**.

Basis: [05-test-procedure](05-test-procedure.md) (SOP), [09-measurement-theory](09-measurement-theory.md)
§3 (error budget) and §8, the firmware in `firmware/src/state.cpp`, `logger.cpp`,
`main.cpp` and the web UI `firmware/web/index.html`. The clickable prototype of the
guided run is in [11-run-prototype.html](11-run-prototype.html).

## 1 What happens today when you press "Measurement"

| | today | consequence |
|---|---|---|
| Start | immediately, if wheel load is set and plateaus exist | no check whether tare, idle-loss model, wheel-loss model, circumference are current |
| Sequence | plateaus in order, 15 s settling each, **one** point over 10 revolutions | one value per speed, no scatter within the plateau |
| Warm-up | not part of the run (`WARMUP_MS` exists, is never driven) | tire cold or arbitrarily warm, comparability depends on the operator |
| Baseline | separate run or assistant; A₀/B₀ in NVS, **without timestamp** | nobody knows at measurement time how old the curve is |
| Post-run baseline | only in the SOP, not in the firmware | drift criterion 0.02 N ([09-measurement-theory](09-measurement-theory.md) §8.8) is never checked |
| Report | CSV line per point on USB; header with U, r, L, N_per_count | lost if no computer is listening; no metadata (tire, pressure, temperature, models, build) |
| Scatter | not recorded | uncertainty per point unknown, outliers invisible |
| Validity | point is taken as soon as the ramp is settled | no note whether the drive was constant during the point |

In short: the physics is clean ([09-measurement-theory](09-measurement-theory.md) §1 to §6), the
measurement chain is calibratable, but the **run** is not yet a measurement procedure. It
measures, it does not check itself, and it leaves nothing behind that is still readable a
month later.

## 2 Findings and recommendations

Numbered **R1 … R12**, sorted by impact. "Effort" means firmware plus web UI.

### R1 A run is a data set, not a serial stream

Every run gets an identifier (`2026-11-23-03`) and a **run record** (JSON) on the ESP
(SD card, otherwise LittleFS): metadata, check results, calibration state with
timestamps, all measurement points with scatter, warnings. The serial CSV remains as a
log capture. The web UI offers **"Report .md"** and **"Data .csv"** for download per run;
the ESP renders the Markdown from the run record (plain text templating, no library).

Effort medium. Without R1 everything else stays volatile.

### R2 Before the start, check instead of assume

Checklist when pressing "Measurement", each line **ok / skipped / missing**, all results
into the report:

| Check | automatic | Criterion |
|---|---|---|
| Motor controller connected, no fault, bus 40–44 V | yes | from telemetry |
| Motor on the controller (emergency stop not triggered) | yes | disconnect detection quiet |
| Load cell responding, not saturated | yes | `torque::isResponding`, `isSaturated` |
| Load-cell noise at standstill | yes, 5 s | standard deviation of raw values < 3 counts [TBD, from first runs] |
| Calibration factor present | yes | `LOADCELL_N_PER_COUNT` not TBD |
| Tare age | yes | newer than the last mechanical intervention; otherwise re-zero |
| **Check weight** | semi-automatic, skippable | 1000 g at 330 mm → target 3.237 Nm; deviation < 0.5 % ok, < 1 % warning, otherwise stop |
| Idle-loss model A₀/B₀ | yes | present, age, number of points; older than the session → drive pre-run baseline |
| Wheel-loss model A₁/B₁ and circumference for **this** wheel | yes | wheel identifier matches; otherwise spin-down assistant |
| Wheel load set, sliding-weight position entered | input | mandatory field |
| Tire, pressure, room temperature | input | mandatory fields (temperature by hand until sensor A5 arrives) |
| Visual inspection, emergency stop within reach | confirmation | SOP B0 |

The check weight is the cheapest sharp test of the whole chain ([09-measurement-theory](09-measurement-theory.md)
§5.1): 20 seconds, and it catches slipped cables, misadjusted arms and cell drift all
at once. Whoever has just done the calibration skips it; the report notes that.

### R3 Manual steps first, then the machine alone

Order of the guided run:

1. **Preparation, manual:** create run, visual inspection, system check, zeroing,
   check weight, idle-loss model (if needed: lift wheel, drive pre-run baseline, lower
   wheel), wheel-loss model (if needed: spin-down assistant), check wheel load.
2. **Measurement drive, automatic:** warm-up, plateaus, anchor point, spin-down.
3. **Finish:** post-run baseline (optional, see R6), report.

Everything that needs hands comes before the warm-up. After that the stand runs on its
own for three quarters of an hour, and the report is produced at the end without
intervention.

### R4 Measure scatter per point: three points per plateau

Today: one point over 10 revolutions. Proposal: **three consecutive points** over 10
revolutions each on every plateau. From that, per plateau, the mean and standard
deviation of the three points, plus the standard deviation of the ten per-revolution
means within each point. Only then does a measured value have a Type A uncertainty that
does not come from an assumption.

Cost: at 15 km/h 20 s more per plateau, at 60 km/h 4 s. The whole measurement run gets
about one minute longer.

Samples within one revolution are correlated (drum orders). The scatter of the
**per-revolution means** is therefore the honest measure, not that of the 80-SPS samples
([09-measurement-theory](09-measurement-theory.md) §3.8).

### R5 Record validity per point

Per point into the report: mean speed and its standard deviation, |dv/dt|, duty and
motor current on average, bus voltage, FET and motor temperature, duration. A point
counts as **invalid** if `STEADY_*` was violated during acquisition or the motor
controller reported a fault; it stays in the data, flagged
([10-firmware](10-firmware.md) L10). The evaluation ignores it, the human sees it.

### R6 Measure drift within the run: anchor point and post-run baseline

Two mechanisms, both cheap compared to what they save:

- **Anchor point:** after the last plateau, back to 28.8 km/h and repeat the point.
  Difference to the first 28.8 point = drift of the total torque during the measurement
  run (tire temperature, bearings, air draft). Fully automatic, 40 s.
- **Post-run baseline:** the SOP requires it (B7); the drift criterion 0.02 N on F_roll
  depends on it. But it needs **lifting the wheel at the end**, that is, a manual step
  after the measurement drive. Proposal: last step of the assistant, **optional** with a
  "Skip" button. If skipped, the report carries "drift of idle-loss curve unchecked" as
  a warning, and the result holds only in the A/B/A comparison of the same session
  ([09-measurement-theory](09-measurement-theory.md) §8.10), not absolutely.

This is the one point where "all manual work at the start" and "scientifically clean"
do not go together. The anchor point mitigates it, it does not replace it: it sees the
drift of drum **plus** tire, the post-run baseline separates them.

### R7 Warm-up as a measured state, not as a clock

30 min at 28.8 km/h is the specification of the reference test stand ([09-measurement-theory](09-measurement-theory.md)
§8.5). During the warm-up the live measurement runs anyway; it is written **every
minute** as a point into the run record. The report shows the course P(t), and one can
see whether the tire has run in after 30 min. Second step, later: abort criterion
"change < 1 % over 5 min, at most 30 min" instead of fixed time. First only record,
then derive the criterion from the data.

### R8 All inputs and the device state into the report

Header of the run record: run identifier, date, firmware build, controller firmware and
Hall table (`vesc-config` state), `U`, `L`, `N_per_count`, tare offset with time,
A₀/B₀ with time and point count, A₁/B₁ with wheel identifier, J_Rad, wheel
circumference, wheel load, sliding-weight position, tire, pressure, room temperature,
operator, free text. Every number that enters Crr thereby stands next to the result
([10-firmware](10-firmware.md) E1).

### R9 Compute uncertainty per point and include it

With the quantities from R4 and R8 this is one line:

```
u(Crr)/Crr = sqrt( (s_M/√n / M_R)²        Type A, scatter of per-revolution means
                 + (u_k/k)² + (u_L/L)²     calibration factor, lever arm
                 + (u_N/N)²                wheel load
                 + (u_leer / M_R)²         idle-loss model (drift pre-/post-run baseline, otherwise assumption 0.02 N·r)
                 + (u_Rad / M_R)² )      wheel-loss model (J_Rad ±1.5 % → 0.5 % of M_R)
```

The Type B contributions are constants in `config.h` with provenance
([03-parameters](03-parameters.md)); the Type A contribution comes from the run. The
report shows Crr ± u(Crr) per plateau. With that, the difference between two tires is a
statement with a margin, not a number.

### R10 Numbered warnings instead of prose

Everything that can go wrong during the run without aborting is a warning with an
identifier that appears in the report: W01 check weight skipped, W02 idle-loss model
older than the session, W03 point invalid (which one, why), W04 anchor drift above
threshold, W05 post-run baseline skipped, W06 controller fault during the run, W07
temperature not entered. An evaluation can filter by them.

### R11 Evaluation from the run record, not from the console

`analysis/crr.py` today reads the serial CSV. It should additionally be able to read
the run record (JSON) and produce from it the same Markdown as the ESP, plus the plots.
Two paths, one format. The ESP's Markdown is the report, the Python is the evaluation
of several runs (A/B/A, repeatability scatter over days).

### R12 What remains, even when all of the above is in place

- Absolute values remain drum-specific ([09-measurement-theory](09-measurement-theory.md) §10); the
  comparison of two tires within one session is the robust result.
- The tire temperature is not a measured quantity as long as A5 is open.
- The load-dependent bearing share (0.65 % of the signal) is contained in the result
  until a reference run with a rigid roller has been driven.
- The Hall sensors on the drum are missing: speed comes from the motor controller,
  angle bins ([09-measurement-theory](09-measurement-theory.md) §3.8) do not exist. For averaging over
  whole revolutions the controller speed is sufficient, for diagnosing disturbance
  sources it is not.

## 3 Target sequence of the guided measurement run

This is the template for the prototype and later for `state.cpp`. Steps marked **H**
need hands, **A** run alone, **E** are inputs.

| # | Step | Type | Result in the report |
|---|---|---|---|
| 1 | Create run: tire, wheel, pressure, room temperature, wheel load, sliding-weight position, note | E | header data |
| 2 | Visual inspection per SOP B0, emergency stop within reach | H | confirmed / time |
| 3 | System check: controller, cell, noise, bus, models, age | A | table ok/missing |
| 4 | Zeroing, drum at rest | A | offset, noise σ |
| 5 | Check weight 1000 g at 330 mm | H, skippable | deviation % or W01 |
| 6 | Idle-loss model: adopt or new (lift wheel → pre-run baseline → lower wheel) | H+A | A₀, B₀, 5 points, drift relative to stored |
| 7 | Wheel-loss model for this wheel: adopt or spin-down assistant | H+A | A₁, B₁, J_Rad, circumference |
| 8 | Wheel placed, wheel load and position confirmed | H | confirmed |
| 9 | Warm-up 28.8 km/h, 30 min, live point per minute | A | P(t), final value, convergence |
| 10 | Plateaus 15 / 28.8 / 40 / 50 / 60 km/h, 3 × 10 revolutions each | A | per plateau M, σ, F_roll, Crr raw, Crr corrected, u(Crr), valid |
| 11 | Repeat anchor point 28.8 km/h | A | drift ΔM, ΔCrr |
| 12 | Spin down, brake below 45 km/h | A | duration |
| 13 | Post-run baseline (lift wheel) | H+A, skippable | drift pre/post per plateau or W05 |
| 14 | Report: summary, warnings, .md and .csv | A | files |

Time required without warm-up: preparation 5 to 10 min, pre-run baseline 4 min,
measurement drive 6 min, anchor 1 min, spin-down 1 min, post-run baseline 4 min.
**With warm-up about 50 min**, of which 40 without operator.

## 4 Report format (Markdown)

Rendered by the ESP, reproducible by `crr.py`. Structure:

```
# Measurement run 2026-11-23-03
Tire · Wheel · Pressure · Load · Room temperature · Operator · Build

## Checks                table: Check | Result | Value
## Calibration state     table: Quantity | Value | Provenance | As of (time)
## Warm-up               table P(t) per minute, final value, change over last 5 min
## Measurement points    table: v | n | M ± σ | F_roll | Crr raw | Crr corrected ± u | valid
## Anchor                ΔM, ΔCrr between first and last 28.8 point
## Baseline pre/post     table per plateau, criterion 0.02 N on F_roll
## Warnings              W01 … with text
## Raw data              reference to the .csv, row count, checksum
```

Every number with unit, SI internally, km/h and bar only in the columns named that way
([CLAUDE.md](../CLAUDE.md)). No number without provenance: measured, calculated, entered,
adopted (with time).

## 5 Open decisions

| | Question | Proposal |
|---|---|---|
| E1 | Post-run baseline as optional last step with a manual step, or drop it? | optional, warning W05 if omitted (R6) |
| E2 | Three points per plateau instead of one? | yes (R4) |
| E3 | Warm-up fixed 30 min or convergence criterion? | first fixed and recorded, criterion later from data (R7) |
| E4 | Run record on SD card or in the ESP's Flash? | SD, because the raw data is supposed to live there anyway |
| E5 | Check weight as a fixed step or only after modifications? | fixed, skippable (R2) |
| E6 | Noise limit of the cell at standstill | TBD, derive from the first ten runs |
