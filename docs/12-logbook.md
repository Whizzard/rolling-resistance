# Logbook

What happened at the stand, day by day, newest first. Short entries: what was done,
what went wrong, what was learned. Details live in the documents linked from here;
measurement values only with their file under `data/` or the commit that recorded them.

Entries before 2026-11-25 were written on 2026-11-26 from the builder's notes and photos,
and from commit messages and dated passages in the documents (those name their source).
Days with only design or documentation work are left out.

## 2026-11-28

**Calibration moved to the cell itself** (builder's decision). The arm is lifted off the
Bowden cable, a stirrup on the cable carries 200, 500, 1000 and 2000 g; F = m g, a straight
line over all readings, linearity in % of load with a graphic. The check weight left the
run, the lever calibration over the 330 mm mark and the automatic zero by the motor of
the day before were dropped. The cell arm (84 mm) now enters every torque and still has
to be measured to ±0.1 mm. Source: [09-measurement-theory](09-measurement-theory.md) §5.1
(uncommitted).

## 2026-11-27

**Motor flange made new** (workshop day, builder's report). The shaft bore was no longer
drilled on the mill: only pre-drilled roughly, then bored out on the lathe with the motor
housing chucked and the flange mounted, for better concentricity. On the way a fault of the
mill was found: centring on the bearing and drilling always ended with a marked offset despite
a precise dial gauge. Cause: the head was not trammed square, so every travel in Z shifted
the tool in X, the more the higher chuck, gauge or cutter stood. Trammed since.

## 2026-11-26

**SD card swapped, SD clock found at 400 kHz.** File transfers over USB ran at about
45 kB/s; the SPI clock register showed 400 kHz instead of the configured 10 MHz (the
driver raises the clock only in steps from 20 MHz upward). Set explicitly since then:
about 1 MB/s read (`SD_SPI_KHZ`, [03-parameters](03-parameters.md)). The old card's files
were backed up off the repo before the swap; the new card (64 GB) was formatted FAT32 on
the stand. Load-cell calibration became its own guided procedure with a calibration book
on the SD; the check weight in a run no longer replaces the factor.

**Second complete run with full raw logging** (SX-R 42-406, Vittoria latex, 4.99 bar,
49.8 kg, run `2026-11-26-1542` on the stand's SD). Check weight 0.55 % off the calibration,
the old +24 % riddle gone with L_cal 340 mm on the 85 mm arm. Findings from the raw log:
the wheel model from the lifted-wheel spin-down was about eight times too high (a single
spurious wheel pulse at the lift, and the model form A + B·ω²), which drove the corrected
Crr negative; re-evaluated from the pulses, about 0.005 at 28.8 km/h. The HX711 ran at
10.5 SPS: the RATE jumper on the SparkFun board is closed from the factory; cut by the
builder, since then 83.8 values/s with the same averaged noise
([04-hardware](04-hardware.md) §5.5). Wheel load newly measured: 10.46 kg at 84 cm, arm
20.5 cm, fork 1.7 kg at 84 cm, 49.8 kg.

**Short run with the fixes** (run `2026-11-26-1651`, SX-R, 15 and 28.8 km/h, 80 SPS): Crr
corrected 0.00447 / 0.00480, wheel model from the spin-down clean (A1 0.021 Nm, residual
0.068 rad/s). The check weight read 2.0 % off the calibration and was adopted as the new
factor (W18); the day before it was 0.55 % at 10 SPS. Warm-up flat from minute 7 in the
minute means of the raw log. Relay pulse test at standstill (console `X5`): no edge on
any input, load cell unaffected; the phantom wheel and mark pulses come only when the
relays short the motor while the drum turns (18 and 27 ms after the drop in this run),
and the echo filters caught them.

Work on the desk, not at the stand:

- **Web UI and run sequence** revised: tabs Manual · Measure · Setup, trend arrow next
  to the actual speed, Coast left of the speeds, uniform button labels (Next →,
  ← Back), warning sign at "hands off", live raw-value curve while zeroing, dial for
  the quarter turn, the quarter-turn reading started by a button instead of by itself.
- **Methodology:** step 5 reordered — check weight first, then the end stop with the
  weight on, then the weight read again (W22) and the zero read again (W21), to catch a
  bias from the overload. Linearity check over the measuring range (about 16 to 38 % of
  full scale) still open. [11-review](11-review.md)
- **Raw-data logging prepared:** microSD on the ESP32-S3, binary `.rrl` with µs time
  stamps, CSV zip on download; raw data on the SD only, the flash keeps settings (each
  with its last-changed time). Requirement F8a in [01-requirements](01-requirements.md);
  [10-firmware](10-firmware.md) §11. Built and host-tested, not yet flashed, no card fitted.
- **Wheel profiles:** lists of tires, tubes, rims and hubs in the flash, dropdowns in
  step 1, editing under Setup; each combination keeps its own wheel model, mass and J.
  The field "wheel" became rim + hub.
- **Storage on the SD card in use:** one directory per run or manual recording under
  `/vmrr/<date-time>/` (raw log, protocol, points), file browser in the web UI with a
  raw-file viewer, recording by hand from Manual. Two bugs found at the stand on the way:
  *.zip* downloads answered "not found", and formatting a readable card did nothing.
  [10-firmware](10-firmware.md) §11.
- Repository fully in English: German identifiers and file names removed from firmware, web UI,
  tools (`tools/console.py`) and `hardware/` (generators, exports and FreeCAD object names
  regenerated). Beam rider for the check weight redesigned with snap claws, the M6 screws
  only clamp ([04-hardware](04-hardware.md) §7.11); not printed yet.
- At the stand: drum marks and wheel count about 1200 edges/s at standstill, the same rate
  on both inputs; index and motor Hall quiet. Cause open.
- Firmware of commit f6ccf63 flashed. SD card at the stand: FAT, 3814 MB, "ready";
  write test with the console (`L1`, 5 s, `L0`): `/raw/up23.rrl`, 118562 B, 0 B dropped.
- Requirement F8a ([01-requirements](01-requirements.md)) extended: raw logging of the
  whole run, preparation, tare and check-weight calibration included, for checking
  suspected errors in the procedure later and as a reference.
- Commit c1131e7 pushed; the old history is on GitHub as branch
  `history-until-2026-11-24` (contains the stand's local WLAN password, judged
  uncritical by the builder).
- vmrr logo chosen: mark E3 "Rolling" (wheel with a time sector, speed lines, road).
  Files under `logo/`; favicon, touch icon and top-left logo in the web UI; favicon and
  the mark large above the name on vmrr.de (hosts repo `sites/vmrr.de/`, deployed),
  coming-soon stamp moved up; vmrr.de set in Comfortaa (self-hosted, rounded like the mark).
- Tubes get maker, model and material; rims maker, model, inner width and bead seat
  diameter, checked against the tire's ETRTO.
- Run header: tire state (unknown / new / ridden) and the optional measured tire thickness (mm),
  in the report and the CSV header (`tire_new`, `tire_thickness_mm`).
- Setup: "Reset" (all settings to config.h defaults, without confirmation) and the
  calibration-factor note removed; wheel mass and J_wheel fields removed (in the profile).
- **Drive direction detected instead of chosen:** slow start, the load cell must count
  positive, otherwise the sign is reversed (`DIRDETECT_MIN_N`). Found on the way: STOP
  did not end the slow start (breakaway measurement); fixed, [07-safety](07-safety.md).
- **Wheel profiles in Setup** with mass, bifilar readings and hub details (spokes, axle,
  brake, drive wheel, colour); step 1 only chooses a complete profile.
- Setup: load-cell raw display removed (shown in the run and on the Manual page);
  "learn direction" and "measure breakaway torque" replaced by a direction choice A/B
  and a "Test slowly" button (the same slow start that stops when the drum moves).
- Naming: "check weight" in step 5, "calibration weight" for the calibration; "test
  weight" corrected in [10-firmware](10-firmware.md), the balancing weight in
  [07-safety](07-safety.md) is a "trial weight".
- **Wheel model part of every run:** measured in step 6 with the idle loss; "without
  model" removed. The model now stores the inputs of its run (tire, tube, wheel, ETRTO,
  pressure, run id); a stored one may be taken only for the same wheel (pressure series).
  Fixed: a stored model counted as "from this run" when the spin-down failed.
- **Cup for the check weight** designed: Ø 50 × 50 inside, 1.5 mm wall, one M6 button
  head in a counterbore in the floor. `hardware/weight_cup.py`,
  [04-hardware](04-hardware.md) §7.13. Not yet printed.
- **SD wiring drawing** redrawn: one straight wire per contact to its ESP32 pin,
  pull-ups and decoupling capacitors drawn as parts. `hardware/sd-plan.pdf`,
  [04-hardware](04-hardware.md) §15.
- Run hint starts with the yellow warning sign and "Keep clear." whenever the drum
  turns under power (not in step 4, where it is turned by hand).
- Check weight shown live while it hangs and is read: deviation from the stored
  calibration (reread: from the first reading) as tiles, bar with the mark and a fine
  curve; without a calibration the raw counts with their scale.
- Separate noise measurement in the system check dropped; the noise is now taken from
  the tare values (σ and peak-to-peak).
- Raw log of a run gets step marks and the standstill results (noise, tare, check
  weight) as text; the protocol now lists the tare raw value, the check-weight counts,
  the factor they give and the date of the calibration in use.
- Repository fully in English; GitHub carries the full history again (passwords removed).

## 2026-11-25

**First complete test run** through all 14 steps of the guided run, with only a short
warm-up.

- First value for the **Schwalbe SX-R** — a test value, not a result: warm-up cut short
  and the problems below still open. No run file under `data/` yet; the value goes in
  here once the protocol is copied from the stand.
- **Problems:**
  - The drum sheet still runs in **waves** (out-of-round surface).
  - **Imbalance** of the drum.
  - Slight **runout at the motor flange**.

Also that day (sources: [07-safety](07-safety.md) §3 and §5, [04-hardware](04-hardware.md) §9.4,
`MARK_ECHO_PCT` in `config.h`; the run-down test from the working notes of that session):

- **Relay brake** wired into the e-stop chain and put under firmware control
  (`RELAY_BRAKE_WIRED` = true). Brake comparison from 35.9 km/h: relay short circuit and
  controller at 20 A brake the same, 2.8 km/h/s.
- **Drum Hall sensor mounted**, the 8 marks count; the 9th (index) magnet is still
  missing. Sporadic double pulses at high speed, now rejected by an echo filter.
- Run-down tested 100 → 80 → 60 → 40 → 20 → 0 km/h with 30 s plateaus.
  `V_MAX_KMH` = 100 km/h on the builder's instruction; balance and runout still TBD.

## 2026-11-24

**Stand day with the guided run** (source: [11-review](11-review.md) §0 and §5, commit
`ff57bdc`). Driven from "create run" through zeroing, end stop, check weight, pre-run
baseline and warm-up into the plateaus. Aborted twice and hung once, each time for a
software reason, fixed the same day (N1 to N3). First measurement points recorded, not
evaluated. **The check weight read +23.8 %** (1 kg at 330 mm: 4.007 Nm) — still open (N5).

Domain vmrr.de registered.

## 2026-11-23

**Hall sensors and load cell put into operation**, first calibration, first torques
measured. Details (commits `f5ce3df`, `31cabb4`, `9801500`; [07-safety](07-safety.md) §10):

- Motor drew current but did not turn (ERPM 0 at up to 6.4 A) after rewiring: Hall table of
  the controller re-detected.
- **Drum circumference measured with a tape: 2842 mm.** Load cell **calibrated** with the
  certified 1 kg at 330 mm on the 84 mm cell arm: 0.00002668 N/count.
- Running down by duty cycle fed back into the supply: 43.8 V, fault 1 at 40 → 15 km/h.
  Changed to braking; speed control tuned (0.8 / 1.0 km/h overshoot at 15 / 40 km/h, then
  ±0.2 km/h).

## 2026-11-21

Stand moved to the basement. **Drum bearing blocks replaced.** Aluminium sheet on the
drum re-tensioned to smooth it (washers, tension band).

## 2026-11-18

**First test run, wired on the fly:** ESP32 and VESC at the drum, with the test wheel, and
the first web UI on the iPad. Load cell (HX711) and controller both answer the ESP32; at
8.2 V on the bus the controller answers but cannot drive — "answers" is not "can drive"
(commit `ee99cab`).

- **Problem:** drum bearings too soft — they tilt and creak. New bearing blocks ordered.

## 2026-11-17

**First commissioning of motor and drum on the controller** (commits `1dbed67`,
`2953df5`, `7d6a8a3`, `4ecd041`).

- Measured instead of assumed: 15 pole pairs (900 tachometer steps on 10 turns by hand),
  kt = 0.389 Nm/A, idle loss of the drum from the free spin-down 0.472 Nm + 3.178e-4·ω².
  Air drag at 60 km/h about a third of the useful signal (chequer plate).
- Braking on the power supply ended in overvoltage every time (fault 1, also with 20 A
  from 40 km/h). With a power supply there is no controller brake.
- Relay brake triggered from 101 km/h: the controller lost its supply and vanished from
  USB — this controller is powered only from its power input.
- Controller flashed from firmware 5.02 to 7.01; configuration written back as XML.

## 2026-11-16

Fork for the test wheel mounted.

## 2026-11-15

Commits `08658f4`, `8133c56`. **First flash** of the ESP32-S3 over the CH343 port; the
console comes up as intended. Status LED confirmed on GPIO 48.

## 2026-11-11

Torque balance begun. Drum planked with chequer plate.

## 2026-11-01

**400 rpm, about 70 km/h, with the raw drum.** 120 km/h was reached later (date not
noted). First measurements on the built drum (commit `a4707c0`):

- Drum Ø 900 mm bare, Ø 903 mm with the 1.5 mm chequer plate (circumference then
  estimated at 2.837 m; measured on 2026-11-23: 2.842 m).
- Motor over 400 rpm at 40 V, not at its limit.
- Short-circuit braking stops the drum in about 25 s, a factor 8 to 14 faster than the
  free run-down; mean braking torque 3.25 Nm.

## 2026-10-31

First spin tests with the motor and a manual controller.

## 2026-10-30

First aluminium profiles cut, stand frame assembled. The drum turns in the stand and its
bearing blocks. VESC arrived. Motor flange made.

## 2026-10-29

First raw drum turns on trestles. Made narrower; a second multiplex board instead of
blockboard. Aluminium saw blade arrived. First drum bearing blocks made.

## 2026-10-28

Motor arrived. Raw build of the first drum.
