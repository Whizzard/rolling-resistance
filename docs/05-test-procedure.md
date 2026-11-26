# Test procedure (SOP)

Bare list. Not a word of justification here — that lives in the **Notes** at the end,
linked from each step by the marker `(n)`, and in full in
[09-measurement-theory](09-measurement-theory.md). Numerical values appear here only
where they are needed to act; `firmware/src/config.h` and
[03-parameters](03-parameters.md) are authoritative.

| Block | when |
|---|---|
| **A1** Geometry | after every intervention on the lever arm or the drum surface |
| **A2** Load cell | after every intervention on the cell, the cradle or their cables |
| **A3** Drive | once per motor/controller configuration |
| **A4** Set up wheel load | once, then only when the swing arm is rebuilt |
| **A5** Inertia and loss models | drum: after every change. Wheel: per wheel |
| **B** Measurement | every measurement series |

---

## A Calibrate

Firmware: variant `esp32s3-vesc` (motor controller + load cell + Setup page). Commands
are the same on the serial console and in the web UI.

### A0 Connect

1. Flash the test variant: `cd firmware && pio run -e esp32s3-vesc -t upload -t monitor`.
2. Join Wi-Fi `Pruefstand` — QR `hardware/qr-wlan.png`, password in `firmware/src/secrets.h` (both outside the repo).
3. Open the web UI: `http://tire.local/` or `http://4.3.2.1/` — QR
   `hardware/qr-ui.png`. Tab **Setup**.
4. Enter `i`. Motor controller, load cell and control connection must **all three** be
   present. (1)

### A1 Geometry

1. Tape measure around the drum, average three measurements → `d<m>`. Current value
   2.842 m. (2)
2. Measure the lever arm, ±0.5 mm → `a<m>`. Current value 0.084 m. It cancels out of the
   torque; **recalibrate after any change**, otherwise the firmware blocks.
3. Calibration arm, axis to mark, ±0.1 mm → `b<m>`. Current value 0.330 m. **This
   dimension determines every torque.** (4)

### A2 Load cell and torque chain

1. Drum stationary, **wheel lifted**, drum nudged by hand and allowed to settle (rotor
   free in its detent position). Weighing part in measuring position, cables routed as in
   operation, arm free. (3)
2. Zeroing: `t`. 80 samples. Turn the drum a quarter turn further; the raw value must
   stay the same.
3. **Weigh the calibration weight yourself** (the 1 kg weight is certified), hang it on the
   mark of the calibration arm, plumb. **At most 1.27 kg** — on the 330 mm arm that is the
   full-scale value of the 5 kg cell. (4)
4. `k<kg>`. The console prints force, counts and factor.
5. Repeat with **at least three** masses across the range, e.g. 0.2 / 0.5 / 1.0 kg
   = 16 / 40 / 79 % of full scale. Note the raw value ("Raw") for each mass. (5)
   **As of 2026-11-24: done with 1.0 kg only, linearity open.**
6. Straight-line fit by hand: `M = k · (raw − offset)`. Require **linearity deviation
   < 0.2 %**.
7. Hysteresis: load ascending and descending. Difference at equal load goes into the
   report, **not** into the fit.
8. Cross-check: same mass on the lever arm → force times 84/330. Checks the arm ratio
   from A1 independently of the factor.
9. Creep: leave the load on for 5 min, record the deviation.
10. Enter `LOADCELL_N_PER_COUNT` by hand in `firmware/src/config.h` **and**
    [03-parameters](03-parameters.md) §2. The console prints the line. (6)

### A3 Characterize the drive

1. Drum free, nothing in contact.
2. Direction of rotation: `l`, then turn the drum **by hand in the measuring direction**,
   ≥ 2 km/h.
3. Breakaway torque: `x`. Do not touch the drum. Duty cycle rises at 0.01/s, abort at
   0.15.
4. Compare the measured breakaway torque against `DRUM_LOSS_CONST_NM`. (7)

### A4 Set up wheel load

1. Measure distance `a` horizontally, ±0.5 mm (current value 0.202 m).
2. Stick a tape measure onto the beam, zero **on the pivot axis**. Punch an index mark on
   the sliding weight. (8)
3. Place the gantry over the wheel axle, check the hanging scale with the 1 kg check
   weight.
4. **Weigh** the wheel → `m_wheel`, weigh the tire → `m_tire` (for J_wheel, A5.4).

### A5 Inertia and loss models

1. **Drum spin-down:** wheel lifted, up to 60 km/h, `c`, record the speed trace down to
   standstill → `M_verlust(ω) = −J · dω/dt`.
2. Compare the result against the idle-loss curve from B5. Both routes must agree. (9)
3. `J` from the same experiment with a known added mass. **Open** — until then the
   calculated 1.94 kg·m² with ±5 % applies, and since 2026-11-24 it enters the result via
   the per-point inertia correction.
4. **Bifilar pendulum** per wheel → `J_wheel`: two threads of equal length at opposite
   points of the rim, 20 oscillations, amplitude < 10°, tire mounted and inflated.
   Procedure and sketch in [08-stand](08-stand.md) §6.2. Enter mass, string spacing,
   string length, n and time under *Create run* (step 1 of the guided run), which
   computes J and stores it; or the result directly with `Y<kgm2>` or *Setup → J_wheel
   measured*. **Open** — without it the firmware uses 0.85·m·r² with ±10 % and states
   so in the report.
5. **Wheel spin-down** per wheel → `A₁`, `B₁`: step 6 of the guided run (*Measure new*), or
   standalone as *Wizard coast-down + no-load* under Setup — same mechanics, same result;
   the setup entry stays for a wheel check without a full run.
   Drum with the wheel resting on it up to 40 km/h, lift the wheel **while running**; the
   firmware fits `A₁ + B₁·ω²` over the **whole** spin-down (integral method) and prints
   residual and support points. (10)
6. Reference run with a rigid roller: open, [06-open-questions](06-open-questions.md) A11.

---

## B Measurement

Firmware: main variant `esp32s3` (state machine, plateaus, report) — see **Status
today** at the end. About two hours, of which 30 min warm-up. (11)

> **Since 2026-11-23 the firmware guides through B3 to B9:** *Measurement* in the web UI
> opens the guided measurement run (`run.h`, [11-review](11-review.md) §3).
> Before starting it checks motor controller, cell, noise, calibration status, lever arm
> and models, tares with the wheel lifted and checks the zero by turning the drum further,
> checks the load-cell end stop (live bar, peak 100 to 140 % of full scale) and then
> prompts for the check weight (continue only once the live torque shows the weight is
> off), runs the idle pre-run baseline if needed, then on its own
> warm-up, reference point, plateaus with three points each, reference point and stop,
> optionally zero check and post-run baseline, and stores report and data under
> `/vmrr/<id>/`. Every point is corrected by `J·dω/dt`; pressure and tire temperature come
> from the AirSpy ([11-review](11-review.md)). The steps below
> remain the reference for what the human does meanwhile. **Not yet run on the stand.**

| # | Step | Duration |
|---|---|---|
| B0 | Before switching on | 5 min |
| B1 | Insert wheel, set geometry | 10 min |
| B2 | Prepare tire | 5 min |
| B3 | Static tare `t`, wheel lifted, cross-reading after a quarter turn | 2 min |
| B4 | Pre-run baseline `b`, wheel lifted | 15 min |
| B5 | Warm-up, wheel lowered | 30 min |
| B6 | Measurement run: reference 28.8 → plateaus → reference 28.8 | 10 min |
| B7 | Check zero, post-run baseline `b`, wheel lifted | 5 min |
| B8 | Let spin down | 5–30 min |
| B9 | Evaluation | 2 min |

### B0 Before switching on

- [ ] Emergency stop reachable and function-tested
- [ ] Drum visually inspected: cracks, joint of the shell sheet, loose screws, magnet
      screws tight
- [ ] Weighing part moves freely, end stops not touching, nothing snagging on the cable
- [ ] Coupling free of play
- [ ] Test wheel secured, locking wedge removed
- [ ] Nothing loose in the test area, no loose clothing, safety glasses
- [ ] Time budgeted for the spin-down (12)
- [ ] Ambient temperature noted (start; once more at the end, step 13 asks)
- [ ] AirSpy awake: turn the wheel once, pressure appears on the Measurement page

### B1 Insert wheel and set geometry

- [ ] Thru-axle Ø12 with matching bushings, wheel secured **positively**
- [ ] Wheel **centered on the drum**
- [ ] Clamp the outrigger so that the fork blade is **horizontal** (spirit level) (13)
- [ ] Wheel axle vertically above the drum axle, x = 0 ± 2 mm
- [ ] Turn the wheel by hand: runs freely, no wobble, tire does not run over the drum edge
- [ ] Weigh the wheel load: sliding weight in position, eccentric released, hanging scale
      on the gantry **vertical** at the wheel axle, lift until the wheel just no longer
      touches the drum, read → `l<kg>`; note the position of the sliding weight on the
      tape measure (14)

### B2 Prepare tire

- [ ] Weigh the tire → `J_wheel = J_ref + (m_tire − m_ref) · r_tire²`, `Y<kgm2>`
- [ ] Set pressure cold by gauge; the AirSpy shows it on the Measurement page, otherwise
      enter `p<bar>`
- [ ] The firmware measures the tire circumference from the speed ratio (Setup → Wheel)
- [ ] Enter the wheel load from B1, `l<N>` — default `STANDARD_LOAD_N` = **416.9 N**
      (42.5 kg), currently set to 494 N (15)

### B3 Static tare

1. **Lift the wheel.** Nudge the drum by hand, let it settle, stationary. (20)
2. `t`. `TARE_SAMPLES` = 80 samples, 1 s at full rate; with a browser polling, 2 to 4 s.
3. Turn the drum a **quarter turn** further, let it settle: the display must stay at zero
   (< 0.25 N on F_roll; up to 0.1 N of standstill cogging is normal and cancels in Crr). Otherwise something is holding the rotor.
4. Repeat after **every** mechanical intervention, including after changing the outrigger
   height or rerouting a cable.

### B4 Pre-run baseline

1. **Lift** the wheel with the eccentric lever.
2. *Idle* on the Measurement page (console `m0`). The firmware runs through all
   measurement speeds from the settings in order (default `PLATEAUS_KMH`: 15, 28.8, 40,
   50, 60 km/h), waits `SETTLE_MS` at each and averages over `AVG_REVS_PER_POINT`
   **whole** revolutions.
3. Result `M_idle(ω)` → fit `A₀ + B₀·ω²`. Do not skip. (16)

### B5 Warm-up

1. Lower the wheel.
2. **30 min at 28.8 km/h** (`WARMUP_MS`). (17)

### B6 Measurement run

1. *Measurement* on the Measurement page (console `m1`), enter the wheel load first.
   **The same list** as in B4 — there is only one. The guided run first drives the
   reference point 28.8 km/h straight out of the warm-up, then the list, then 28.8 km/h
   once more; the difference is the drift of the run. (21)
2. Pressure after the warm-up is in the report (AirSpy); without the sensor, read it now.
3. During the run:
   - [ ] do not touch the rig, do not lean on it
   - [ ] do not change doors or ventilation
4. Pressure series only at 28.8 km/h, **5 min settling time** per step, then measure.
5. Load series only at 28.8 km/h and 4 bar, three loads.
   Support points of both series: [09-measurement-theory](09-measurement-theory.md) §7.1.

### B7 Zero and post-run baseline

1. Lift the wheel. (Since 2026-11-25 there is no room temperature entry; start and end temperature come from the AirSpy, which sits in the room at the valve.)
2. The firmware reads the zero (drum stationary, wheel free): deviation from the offset
   > 0.25 N on F_roll is drift of the cell, cable or preload (W14); the fine check is the post-run baseline.
3. `b`, immediately afterwards, in the warm state.

### B8 Spin down and strip down

1. `s`, then **let it spin down freely. Do not brake by hand.**
2. Continue working only when the drum is **visibly stationary**.
3. Before working on the rig: locking wedge between frame and spoke.

### B9 Evaluation and acceptance

1. `python3 analysis/crr.py data/<file>.csv`
2. **Reject** if the post-run baseline deviates from the pre-run baseline at any support
   point by more than **0.02 N** (referred to `F_roll`) (W08), or if zero (W13, W14) or
   reference point (W04) exceed the limit. `crr.py` shows the warnings. (18)
3. Per comparison **A/B/A**: reference tire — test tire — reference tire, in **one**
   session, same load, same pressure.
4. At least three repetitions per tire, state the scatter.
5. Once, run a tire with a published value, 42.5 kg and 28.8 km/h. (19)

### B10 What a change entails

| changed | to be redone |
|---|---|
| Speed, pressure, load | nothing; for pressure and load, 5 min re-warming |
| **Tire** | correct `J_wheel` (B2), **new baseline**, **full 30 min** warm-up |
| **Wheel** | additionally A5.4 (bifilar) and A5.5 (wheel spin-down) |

---

## As of 2026-11-24: what is still missing before the first measurement run

- **Wire the Hall sensor to the 8 drum marks** (`PIN_MARK`; the magnets are mounted).
  Without the sensor the "10 revolutions" are computed from the controller speed and can
  be off by up to one revolution; with the mark Hall sensor every 8th edge counts as a
  real revolution, plus speed, standstill check and angle bins. For a fixed angular zero
  and a self-correcting count, optionally an **intermediate magnet** centered in one 45°
  gap plus counterweight ([04-hardware](04-hardware.md) §8.6); the firmware detects it on
  its own. The second Hall sensor as index (`PIN_INDEX`) would be the alternative.
- **Bifilar pendulum** (A5.4) → `Y<kgm2>`; until then ±3.5 % on the wheel model.
- **Linearity of the cell** (A2.5) with 0.2 and 0.5 kg.
- **Weigh the wheel load** (B1) instead of calculating it; the default 494 N is
  calculated.
- There is no mechanical brake — [07-safety](07-safety.md) §3.

## Notes

1. If the cell is missing, `i` reports the HX711 with pin numbers; if the motor controller
   is missing, it is the UART. Resolve both before the first weight.
2. 1 mm error on the circumference is 0.03 %. Measured 2026-11-23: 2.842 m (A14 done);
   redo after every change to the surface.
3. It is not the cell alone that is calibrated but the whole chain up to the torque — this
   spares the separate uncertainties of cell and lever arm.
   [09-measurement-theory](09-measurement-theory.md) §5.1.
4. On the arm L = 84 mm the cell sees 3.9 times the force of the weight on the
   calibration arm L_cal = 330 mm. In the torque L cancels out, L_cal does not:
   `M = counts · m·g·L_cal / Δcounts`. Calculation and support points:
   [09-measurement-theory](09-measurement-theory.md) §5.1.
5. `k<kg>` sets the factor from **one** mass. The firmware does not check linearity over
   several masses ([10-firmware](10-firmware.md) W5, priority S) — note the raw values and
   fit by hand.
6. The factor is displayed, **not** automatically written to `config.h`
   ([10-firmware](10-firmware.md) W4). It is stored in NVS nonetheless; `r` resets it.
7. The breakaway torque is static friction plus cogging torque, `DRUM_LOSS_CONST_NM` the
   running friction. If breakaway is far above it, something is binding.
8. This makes the lever balance reproducibly adjustable, even without weighing. The
   position read goes into the CSV; if it deviates, something has changed on the wheel,
   the tire or the mechanics.
9. The spin-down yields `M_verlust` **without** the load cell. Its agreement with the
   idle-loss curve is the sharpest available test of the whole system —
   [09-measurement-theory](09-measurement-theory.md) §9.1.
10. Without this run the wheel loss sits unseparated in the result: 0.25 Nm is an apparent
    Crr of 0.00129, half of the best racing tire.
    [09-measurement-theory](09-measurement-theory.md) §9.3.
11. Shortcuts backfire: the differences to be measured are smaller than the effects of a
    skipped step. Two good racing tires are separated by 0.2 N of tangential force.
12. No brake. At 60 km/h about 1.3 kJ is stored; depending on the drag torque the
    spin-down takes 3 to 80 min — [09-measurement-theory](09-measurement-theory.md) §8.9.
13. A horizontal blade means: pivot at wheel-axle height, the tangential force has no
    lever, the correction term vanishes. [08-stand](08-stand.md) §5.2.
14. Pull vertically (5° of tilt is 0.4 %) and lift only 1 to 2 mm so that the arm stays in
    the measuring position. Route b, later: two load cells at the axle.
    [09-measurement-theory](09-measurement-theory.md) §5.2.
15. 416.9 N (42.5 kg) is the load of the published Crr database, so that the values are
    comparable.
16. Static tare and idle-loss curve capture different things and do not replace each
    other: the tare the ADC offset and the dead weight, the curve the aerodynamics.
    [09-measurement-theory](09-measurement-theory.md) §5.3.
17. 30 min is what the commercial reference test rig allows. Its temperature stability of
    ±0.5 °C cannot be achieved in the workshop — which is why the A/B/A protocol carries
    the weight.
18. 0.02 N is 1.3 % of the racing-tire signal. Usual causes for a divergence: temperature
    drift of the bearings, a rubbing brake, a changed cable, draught.
    [09-measurement-theory](09-measurement-theory.md) §8.8.
19. The absolute values will not match exactly
    ([09-measurement-theory](09-measurement-theory.md) §10), but the order of magnitude
    must fit and the ranking of several tires must be right. Expectation from the
    curvature approximation: 3 to 4 % below the database
    ([11-review](11-review.md) D5).
20. At standstill the arm carries the motor's cogging torque if the tire's static friction
    holds the rotor at an arbitrary angle — up to 0.2 Nm in the offset. Left free, the
    rotor drops into a detent position, where it is zero.
    [09-measurement-theory](09-measurement-theory.md) §5.3.
21. The tire cools at 15 km/h and warms beyond the warm-up state up to 60 km/h. The
    reference point immediately after the warm-up is the comparison value against the
    database; the second one measures the drift of the whole run.
    [09-measurement-theory](09-measurement-theory.md) §7.1.
