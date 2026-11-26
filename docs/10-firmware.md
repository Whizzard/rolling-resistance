# Firmware Requirements

What the firmware must do. The *how* is not covered here.

Priority: **M** = must for the first measurement run · **S** = should · **K** = can, later.
As of 2026-11-01, first version as dictated.

## 1 Principles

| | P | Requirement |
|---|---|---|
| G1 | M | No `delay()` in the main loop path; the state machine is non-blocking. |
| G2 | M | ISRs are short: timestamp into ring buffer, nothing else, `IRAM_ATTR`. |
| G3 | M | As long as `LOADCELL_N_PER_COUNT` = 0, **no** forces and no Crr are output, only raw values. |
| G4 | M | All physical constants come from `config.h`, no literals in the code. |
| G5 | S | Every operating mode is reachable via the serial console, even without a control panel. |

## 2 Test Mode

Purpose: commission motor and mechanics before measuring.

| | P | Requirement |
|---|---|---|
| T1 | M | Switch the motor on and off. |
| T2 | M | Adjust the target speed via **Up/Down**. |
| T3 | M | Step size **1 km/h and 5 km/h**, switchable. |
| T4 | M | Setpoint range **0 to 80 km/h**, limited by `MAX_DRUM_RPM`. Above the release per [07-safety](07-safety.md) §5 the firmware refuses the setpoint. |
| T5 | M | Display **setpoint and actual speed** simultaneously. |
| T6 | M | Display **circumferential speed in km/h and rotational speed in rpm**. |
| T7 | S | Display the raw value of the load cell so the torque path can be checked. |
| T8 | S | Setpoint is set to 0 when leaving test mode. |

## 3 Operation

**Five-way switch with five buttons plus separate emergency stop.**

| | P | Requirement |
|---|---|---|
| B1 | M | Buttons **Up / Down / In / Out**, each to ground, internal pull-up. |
| B2 | M | Fifth button **Center = Confirm**. Decided and wired, `PIN_BTN_OK` = GPIO 39 ([04-hardware](04-hardware.md) §12.3). |
| B3 | M | Debounce in software, no RC in the signal path. |
| B4 | M | Holding Up/Down repeats with autorepeat. |
| B5 | M | **Step size 1 or 5 km/h** via In/Out or a mode switch, not via a hidden key combination. |
| B6 | M | The **emergency stop is not a button on the five-way switch** but the positively opening mushroom switch in the power path ([07-safety](07-safety.md) §3). The firmware sees it only via `PIN_ESTOP_FB`. |
| B7 | S | The same navigation via the serial console, so work is possible without a control panel. |

Form factor: **five-way tactile switch** (navigation switch, e.g. ALPS SKQU), one component,
five contacts, common ground. Alternatively five individual buttons. **No** analog
joystick breakouts: they output a voltage, not five contacts.

## 4 Emergency Stop

| | P | Requirement |
|---|---|---|
| N1 | M | Can be triggered at any time and from any operating mode. |
| N2 | M | **Sequence: first disable the driver, then drop the relays.** The phases must not be shorted while the motor controller is still driving. |
| N3 | M | Relays dropped = phases shorted = braked. Measured 2026-11-01: about **25 s** from over 400 rpm. |
| N4 | M | The state holds until the drum stands still and then clears itself (since 2026-11-23; previously latching with acknowledgement). |
| N5 | M | The same state is reached by the shutdown criteria in [07-safety](07-safety.md) §4. |
| N6 | M | On reset the relays are dropped, so the stand is braked (R2 at the gate, hardware). Since 2026-11-25 the firmware energizes them in `setup()` and keeps them energized; it drops them only for a stop, at any speed since the evening of 2026-11-25 ([04-hardware](04-hardware.md) §9.4). |
| N7 | M | **Since 2026-11-25 (`brake.cpp`):** the firmware drops the relays itself for every stop (until the evening of 2026-11-25 only above 40 km/h) (large descents within a run: one relay pulse each, released again before standstill by `releaseEarly`; the first attempt with a full stop made the weighing arm rattle), in the order of N2 (`RELAY_DRIVER_OFF_MS` after releasing the controller), and pulls them in again after `RELAY_RELEASE_STILL_MS` of standstill. While it has dropped them, the criterion "no motor on the controller" is muted and drive commands are refused; the hand emergency stop stays detected. Only with `RELAY_BRAKE_WIRED`. Circuit [04-hardware](04-hardware.md) §9.1, behaviour §9.4. |

## 3.1 Web UI over Wi-Fi

| | P | Requirement |
|---|---|---|
| W1 | M | The stand serves the web UI **itself**, via its own access point. No internet, no router, no app ([04-hardware](04-hardware.md) §13). |
| W2 | M | **The same command vocabulary as the serial console.** `/api/cmd?c=…` passes the string on to the same parser; there is only one list of commands (G5). |
| W3 | M | **Dead-man switch**: without a heartbeat from the operator's computer, a running run aborts ([07-safety](07-safety.md) §4). |
| W4 | S | The same file runs as a draft in the browser (simulated) and on the device (live). If `/api/state` answers, the test stand leads; otherwise the page simulates itself. |
| W5 | S | Refresh display values at most **5 times per second**. The HX711 delivers 80 SPS; the display shows the moving average over 40 values. A number jittering at 80 Hz is neither readable nor meaningful. |
| W6 | K | Designed for an **iPad in landscape** in kiosk mode, but usable down to phone width. |

**Web server in its own task, since 2026-11-23.** The Arduino library blocks for up
to 5 s when a browser opens a connection but sends nothing, which iOS does as a
precaution. In the main loop, control stalled for 2.8 s as a result, the motor
controller's dead-man (300 ms) tripped, and two starts aborted. DNS, web server and
JSON building therefore run in their own task on core 0; commands from the browser
go through a queue to the main loop. Nothing is actuated from the network task. The
longest loop iteration is shown by `i`.

**The LCD is thus optional.** `display::present()` is false if nobody answers at
`LCD_I2C_ADDR`; then everything runs via console and web UI. The LCD shows the same
once more; it replaces nothing and nothing depends on it.

## 4.1 Status LED

| | P | Requirement |
|---|---|---|
| L1 | M | The DevKit's RGB LED shows the operator connection and, since 2026-11-26, the raw-data recording. No startup animation: the indicator is up a few milliseconds after start and says more. The board's red LED is the power LED and cannot be switched. |
| L2 | M | **Blue, blinking slowly** = waiting for an operator computer; **steady green** = one is reporting in; **red, blinking fast** = raw data are recorded; **red/blue alternating** = recording without an operator computer. Written only when the colour changes; `neopixelWrite()` occupies the RMT and takes about 30 µs. |
| L3 | M | The animation is **non-blocking** (G1): `statusled::service()` runs from `loop()`, there is no `delay()`. |

Constants in [03-parameters](03-parameters.md) §8; the pin question v1.0/v1.1 is listed
there as TBD.

## 4.2 Structure and Host Tests

**Modules, as of 2026-11-25.** `main.cpp` holds `setup()` and `loop()` and nothing
else; the console (parser, help, status line, telemetry, the error text for the UI)
is `console.cpp`, the `/api/state` JSON is `statejson.cpp`, the steady-state live
values are `live.cpp`. The drum speed from the controller is computed in exactly one
place, `drivespeed.cpp`; `revsrc` (marks or controller) and `speedctl` both read it,
and everything above `revsrc` asks only `revsrc`. Refusals and faults go through
`logger::fail()`, so the UI colours by that flag and no longer by the first word of the
text. The end of a sequence is delivered to `run.cpp` on the next `state::service()`
tick, never from inside the state machine's own call.

### Host tests

Since 2026-11-25 the pure arithmetic lives in Arduino-free headers under
`firmware/src/calc/` — least-squares line and standard deviation (`linfit.h`), the
loss-model correction (`loss.h`), the spin-down normal equations (`spindown.h`), the
time format (`timefmt.h`) and the JSON string escape (`json.h`). Until then the same
regression stood five times in the firmware with five different degeneracy guards.
The firmware calls these headers; so do the tests:

```sh
cd firmware && pio test -e native          # Unity, 13 cases + the VESC frame test
.venv/bin/python -m pytest analysis -q     # crr.py: header, loss model, plateau statistics, drift
```

The expected values in the tests are computed independently (an exact line, a
spin-down integrated from known A₁/B₁/J, calendar dates, a power balance for the
wheel loss), not with the function under test. `pio run -e esp32s3` remains the
compile check; both belong before a commit.

**Keys, since 2026-11-25.** The JSON of `/api/state` and `/api/run`, the NVS keys and
the header keys of the run CSV are English throughout (`target`, `driving`, `sensors`,
`cal`, `taring`, `mWheel`, `uWheel`, `estop`, `relayBrake`, `ctrlTarget`, `U_wheel`,
`J_wheel_src` = `meas`/`est`, NVS namespace `teststand`). The NVS namespace change
drops the values stored before: calibration factor, circumference, models and wheel
mass have to be entered again once — accepted during development.

**Run files as one archive.** `/fs/<medium>/vmrr/<id>?zip=1` delivers all files of the run's
session directory (protocol, points, raw log) as a stored (uncompressed) ZIP built on the
fly (`calc/zip.h`, `calc/crc32.h`); the report step links to it (§11.7).

## 5 Diagnostic Pages

One page per sensor, individually selectable. Purpose: during assembly, check
**whether** a sensor switches, and **how well**.

| | P | Requirement |
|---|---|---|
| D1 | M | Every page works **at standstill while turning by hand**; individual pulses become visible, not just an average. |
| D2 | M | Every page shows a **pulse counter**, individually resettable. |
| D3 | M | Display is **4 × 20 characters**. If a page does not fit, it is paged, not shortened. |

### 5.1 Raw Values Overview

| | P | Requirement |
|---|---|---|
| D4 | M | **One page with all sensors at once**, each as a raw value: drum marks, index, motor Hall, wheel, optical sensor, load cell. |
| D5 | M | Digital sensors as **level and counter**, the load cell as **raw value in counts**, unfiltered, uncalibrated. |
| D6 | M | Update at least 5 times per second so that turning by hand becomes visible. |

The purpose is the wiring test: one glance to see whether every channel is alive at all.

**As of 2026-11-22:** the digital sensors (D4, D5 without optical sensor) are available in
the web UI under *Setup → Hall sensors* as counter, time since the last pulse and
level, 5 times per second (D6), counters jointly zeroable with `g`
([04-hardware](04-hardware.md) §14.1). The load cell stands next to it as a raw value under
*Calibrate load cell*.

### 5.2 Drum Marks: Rotational Speed and Even Spacing

| | P | Requirement |
|---|---|---|
| D7 | M | Rotational speed in **rpm with one decimal place** and circumferential speed in km/h, for comparison with a handheld tachometer. |
| D8 | M | **Pulse count per revolution** against the target of 8. Any deviation means: a mark is not switching or is bouncing. |
| D9 | M | **Angular deviation per mark** against the ideal 45° pitch, in degrees **and** in mm on the circumference. |
| D10 | M | **Figure of merit**: largest magnitude and standard deviation over all eight marks. |
| D11 | S | **Pulse width per mark** in degrees. A noticeably narrower mark means too large an air gap or a weak magnet; this is the measure of switching margin. Requires evaluation of **both edges**. |
| D12 | M | Averaging over ≥ 50 revolutions at steady rotational speed; valid only on a plateau, not during the ramp. |

```
φ_i = 360° · (Σ_{k≤i} t_k) / T_U  −  i · 45°
```

Admissible because ω changes by at most 0.15 % within one revolution
([09-measurement-theory](09-measurement-theory.md) §1.7). The same φ_i form the correction table from
[04-hardware](04-hardware.md) §8.5, measured instead of drilled.

### 5.3 Further Sensor Pages

| | P | Requirement |
|---|---|---|
| D13 | M | **Index**: counter, revolutions, check for exactly 1 pulse per revolution. |
| D14 | M | **Motor Hall**: counter and **pulses per drum revolution**; this determines `MOTOR_PULSES_PER_REV`, so far TBD. |
| D15 | S | **Wheel**: counter, rotational speed, pulses per revolution. *Counter and rotational speed available since 2026-11-22, plus since 2026-11-23 the effective wheel circumference from the speed ratio, integrated over a window of steady running ([04-hardware](04-hardware.md) §14.1).* |
| D16 | S | **Light barrier**: state and edge counter. |

### 5.4 Load Cell

| | P | Requirement |
|---|---|---|
| D17 | M | **Raw value** in counts, unfiltered. |
| D18 | M | **Mean, scatter and peak-to-peak** over the last N values. |
| D19 | M | **Smoothing N adjustable at runtime**, effect on the scatter immediately visible. This is how `AVG_REVS_PER_POINT` gets substantiated instead of estimated. |
| D20 | M | **Actual sample rate** in SPS, so it becomes apparent when the RATE pin is set to 10 instead of 80 SPS ([04-hardware](04-hardware.md) §5.5). |
| D21 | S | **Drift since the last tare**, in counts and in minutes. |
| D22 | S | After calibration additionally in **N and Nm**. |

### 5.5 Motor

Values from the motor controller via UART. Turns the motor controller into a second measuring instrument.

| | P | Requirement |
|---|---|---|
| D23 | M | **Rotational speed** from the motor controller, converted to mechanical rpm; cross-check against the drum marks. |
| D24 | M | **Motor current and input current**, **bus voltage**, **duty cycle**. |
| D25 | M | **Controller temperature** and, once A5 is settled, **motor temperature**. |
| D26 | M | **Motor controller error code** in plain text. |
| D27 | S | Consumed Ah and Wh since start of the run. |
| D28 | S | Compute **torque from current** (`M = kt · I`) and place it next to the torque from the load cell. |

> D28 is the most valuable line on this page: two mutually independent torque
> measurements. It does not replace the load cell, since kt is only known to 10 to 20 %
> and is temperature-dependent, but a deviation by a factor of 2 shows up immediately.
> Requires kt from A13 and the pole count from A4.

## 6 Load Cell Calibration Mode

Hang the calibration weight on the lever arm, derive `LOADCELL_N_PER_COUNT` from it.

| | P | Requirement |
|---|---|---|
| W1 | M | Record the zero point (arm unloaded), mean over ≥ 200 values. |
| W2 | M | Enter the mass of the calibration weight. The **lever arms** are separate fields on the same page (cell arm 80 mm, calibration arm 160 mm), measured on the assembly, not typed in per weighing. |
| W3 | M | Measure with weight, mean over ≥ 200 values, compute and display the factor. |
| W4 | M | The factor is **displayed, not adopted automatically**; it goes by hand into `config.h` and [03-parameters](03-parameters.md). |
| W5 | S | **Several masses** in sequence, check linearity over the range and display the deviation. |
| W6 | S | The same mass at both radii as a cross-check of the lever arm dimensions. |
| W7 | K | Creep: leave the load on for 5 min, log the deviation. |

Conversion: a mass *m* at radius *b* produces at the cell (arm L = 80 mm)
`F = m · g · b / L`. At the 160 mm hole therefore **twice** the force; full scale of the 5 kg cell
at 2.4 kg, [09-measurement-theory](09-measurement-theory.md) §5.1.

## 7 Measurement Page

The page on which everything that matters is shown during a run.

| | P | Requirement |
|---|---|---|
| L1 | M | **Rotational speed** in rpm and **circumferential speed** in km/h, each as **setpoint and actual value**. |
| L2 | M | **Power in W**. As long as the cell is not calibrated, the **electrical** power from the motor controller, explicitly labelled as such. |
| L3 | M | **Motor current** and **motor temperature**. |
| L4 | M | **Speed state**: steady / accelerating / braking. |
| L5 | M | **dv/dt numerically** in km/h/s, not just as a symbol. |
| L6 | M | Crr, as soon as `LOADCELL_N_PER_COUNT` is set. |
| L7 | S | Remaining hold time on the plateau. |
| L8 | S | Tire pressure and wheel load, as entered before the run. |
| L12 | M | Compute and display the **rolling radius `r_wheel` from the speed ratio** `r_drum · n_drum / n_wheel` instead of having it entered. Averaging over ≥ 30 s on a plateau. |
| L13 | S | The ratio as **slip monitoring**: a deviation from the settled value by more than 1 % reports a slipping tire. |

### 7.1 When the Speed Is "Steady"

Not by eye, but by threshold. A residual acceleration produces, via
`M = J · dω/dt`, an apparent torque that goes directly into the result:

```
1 % of the signal of a good racing tire  = 0.012 Nm
α = 0.012 / 1.94 kg·m²                   = 0.0062 rad/s²
dv/dt                                    = 0.010 km/h/s
```

| | P | Requirement |
|---|---|---|
| L9 | M | The apparent torque is **subtracted per point**: `M = M_raw − J_eff·dω/dt` with the net acceleration as the slope of the regression line v(t) over the point (since 2026-11-24, `state.cpp`). Residual: `UNC_J_REL` = 5 % of it. |
| L10 | M | Measurement points whose net acceleration exceeds `POINT_DVDT_NET_MAX_KMH_S` = 0.2 km/h/s (residual > 1 %) are **marked invalid**, in the display, in the report (W03) and in the CSV row. Peak |dv/dt| and M_inert are listed per point alongside. |
| ~~L11~~ | — | The tighter window has been dropped: the correction replaces it. |

For perspective: over an averaging window of 10 revolutions at 60 km/h (1.7 s),
0.010 km/h/s is just **0.017 km/h of drift**, i.e. 0.03 % of the speed. The plateau
has to stand more precisely than one would assume, which is why the value belongs on the page and not
in the analysis afterwards.

## 8 Inputs Before the Run

Via the serial console, single-line, and included in the CSV header.

| Command | Quantity | Example | Purpose |
|---|---|---|---|
| `l<N>` | Wheel load | `l416.9` | Crr; also under *Setup → Measurement* |
| `h<kmh>` / `h-<kmh>` | Add / remove measurement speed | `h28.8` | List in NVS, always sorted; the Measurement page shows exactly these points, *Measurement*/*Idle* run through them in order |
| `p<bar>` | Tire pressure | `p4.0` | report only |
| `r<m>` | Tire radius | `r0.339` | **fallback only**; normally measured from the speed ratio, L12 |
| `u` | **Spin-down + idle wizard** | `u` | The drum runs with the wheel resting on it to `WIZ_KMH` = 40 km/h; then lift the wheel off. If it turns slower than it would have to via the drum (`WIZ_SLIP_RATIO`), it is lifted off, and at the same time the wheel spin-down (A₁, B₁ from the pulses, J_wheel from mass and circumference) and the drum idle run over all plateaus (A₀, B₀ from the cell) begin. Both are stored. `uq` acknowledges the last step. In the web UI as a wizard under *Setup → Loss models*. Since 2026-11-23 |
| `y<kg>` | Wheel mass including tire | `y2.4` | J_wheel = `WHEEL_J_FACTOR` · m · r_wheel² |
| `w<A₁> <B₁>` | Wheel-loss model by hand | `w0.012 0.000071` | from the wheel spin-down |
| `n<A₀> <B₀>` | Idle-loss model by hand | `n0.004 0.000209` | the idle run `m0` fits and stores it itself since 2026-11-23; from the baseline run |
| `t` | static tare | | offset |
| `R` | **Guided measurement run** | `R`, then `Rk`/`Rs`/`Rn`/`Rx`, `Rm<k>=<v>` | Fourteen steps ([11-review](11-review.md) §3): create run, visual check, system check, zeroing, control weight, idle-loss model (adopt, or since 2026-11-25 the spin-down wizard inside the run: wheel resting to 40 km/h, lift, wheel spin-down and idle pre-run together), wheel model, wheel resting; then on its own warm-up (`WARMUP_MS` at `WARMUP_KMH`, live point every minute), plateaus with `RUN_POINTS_PER_PLATEAU` points, anchor point, stop; optional idle post-run; report `protocol.md` and data `points.csv` in the session directory `/vmrr/<id>/` (SD, else flash), retrievable in the file browser (§11.7). Warnings W01…W12 in the report. In the web UI behind *Measurement*. Since 2026-11-23 |

### 8.1 Wheel profiles (since 2026-11-26)

As of 2026-11-26. `profiles.h`. Four lists — **tires** (with ETRTO), **tubes**, **rims**,
**hubs** — and the **profiles**, each one combination tire + tube + rim + hub. A profile
holds what belongs to exactly that wheel: the wheel model A₁, B₁ with its origin (run id,
pressure, date), the wheel circumference, the mass and the measured J_wheel. The wheel
model depends on all four parts (the hub bearings carry most of it), so it is never
carried over to another combination.

- **Setup → Wheels** (everything is entered here, beforehand): per profile the four
  parts (dropdowns of earlier entries plus "New…"), the ETRTO with the tire, for the tube
  maker, model and material (latex, butyl, TPU, tubeless), for the rim maker, model, inner
  width and bead seat diameter (BSD, must match the tire's ETRTO), for the hub
  spokes, axle (12 / 15 / 19 mm thru-axle, quick release, nutted axle), brake (none, drum,
  drum with cooling fins, disc), drive wheel yes/no and colour (silver, black, free
  text), the wheel mass and the bifilar pendulum readings (J computed and stored with
  them). Required: parts, ETRTO, tube material, rim inner width and BSD, hub details except
  colour, mass; the pendulum is
  optional (J then estimated). Changing a part drops the profile's wheel model. A
  *Measure* button per complete profile starts the run with it.
- **Step 1 of the run:** only the profile (incomplete ones cannot be chosen; a button
  leads to Setup → Wheels), plus pressure, wheel load and note. Mass and J of the profile
  go into use; without a measured J none (estimated), never the J of the wheel before.
- **Step 6:** the wheel model is measured in every run. "Use stored" only if this
  profile has a model (pressure series on the same wheel); see [11-review](11-review.md) §3.
- **Setup → Wheels, lists:** rename and delete entries (refused while a profile uses
  them), delete a profile or only its wheel model.
- **Storage:** `/profiles.tsv` in the flash (LittleFS), text, one record per line;
  written only at standstill (`storage::flashWritable()`), via a temporary file. No fixed
  number of entries. HTTP: `GET /api/profiles`, edits as `POST /api/profiles?op=…`
  (`pset`, `hub`, `ren`, `del`, `pdel`, `pclr`); the run takes a profile with `Rmq=<index>`.
- **Mirror on the SD (since 2026-11-26):** the four lists (tires, tubes, rims, hubs) are
  also kept as Markdown tables in `/vmrr/wheels.md`, editable on any computer. The flash
  stays the master. Every change in Setup is written to both; when a card is mounted the
  stand reads `wheels.md` and takes over edits made elsewhere (a CRC stamp in the last
  line tells an edited file from one the stand wrote): new rows become entries, changed
  rows update them, removed rows delete the entry unless a profile uses it. Profiles and
  wheel models stay in the flash only. Console: `P<T|U|R|H>| name | ... |` adds or
  changes one entry (empty cells keep the old value), `Dwheels.md` prints the file.
- **Speeds per run (since 2026-11-26):** step 1 ticks which Setup speeds this run measures
  (default all, never none; meta key `Rmv=<bitmask>`). Pre-run idle loss, plateaus and
  post-run use the same list, so the pre/post comparison stays index for index.
- **Test-wheel phantom pulses (since 2026-11-26):** an edge after less than
  `WHEEL_ECHO_PCT` of the last wheel interval is rejected (bit 7 in the raw log), and a
  spin-down fit is rejected if its residual exceeds `SPINDOWN_RESID_MAX_RADS`; a fit with
  A1 < 0 or B1 < 0 is repeated with that term at 0.
- **Stand build in every protocol (since 2026-11-26):** section "Stand build" with the firmware
  commit (`src/build_id.h`, generated by `web/embed_pio.py`, `-dirty` with uncommitted
  changes), drum, marks and index, test-wheel pulses, load cell and HX711 with the rate
  measured over the run, cell and calibration arm, wheel-load lever, motor. Plus the stand
  revision, a free text in Setup → Stand (console `B<text>`), dated by its change stamp and
  kept through a settings reset. The raw-log header carries commit, revision, HX711 rate,
  cell capacity and lever.
- **Warm-up minute values (since 2026-11-26):** the mean of all valid live values of that
  minute, no longer the last snapshot; one console line per minute with M, P, Crr, tire
  temperature and the span the settle test sees.
- **Relay pulse test (since 2026-11-26):** console `X<n>` lets the brake relays drop and pull
  in n times at standstill and prints per cycle the edges on all four inputs and the
  load-cell excursion, against a quiet window of the same length (`RELAYTEST_WIN_MS`).
- **Files over USB, compressed (since 2026-11-26):** `Gz<path> <offset> <len>` sends up to
  32 kB as a heatshrink stream (`lib/heatshrink`, window 2^10, lookahead 2^5), lines `=Z`;
  `=E` carries the SD read time and the total in ms. The SD reads about 48 kB/s over SPI
  and limits the transfer.
- **Files over USB (since 2026-11-26):** console `G?<dir>` lists, `G<path> <offset> <len>`
  sends up to 16 kB of a file below `/vmrr` as base64 lines with CRC32 (`=G`, end `=E`,
  refusal `=X`); only at standstill and outside a run, since one request holds the main
  loop for about 0.5 s. Client: `tools/console.py --get`, about 32 kB/s.

| | P | Requirement |
|---|---|---|
| E1 | M | Every input is in the CSV so that a wrongly set value shows up later. |
| E2 | S | The firmware fits `A₀/B₀` itself after the baseline run and offers them. |
| E3 | M | If an input is missing, the dependent quantity stays `--`; it is not estimated. |

## 9 Display

| | P | Requirement |
|---|---|---|
| Z1 | M | **4 × 20 characters**, HD44780 via PCF8574. |
| Z2 | M | If `LOADCELL_N_PER_COUNT` or `A₁/B₁` is missing, **`--`** is shown instead of Crr and watts. No value is better than a wrong one. |
| Z3 | M | One line always shows the **inputs** (wheel load, pressure) so that an error shows up before the run. |
| Z4 | M | Operating mode and progress are always visible. |

Draft of the Measurement page:

```
v 28.8 km/h  n 196
P 12.3 W     Crr .00370
N 417 N      p 4.0 bar
MEASURE  rev 7/10  ok
```

## 10 Open

| | Question |
|---|---|
| ? | **Mass of the calibration weight.** Presumed 500 g, which at the 80 mm hole gives only **10 % of the cell's full scale**, while measurement takes place at 13 to 64 %. See note below. |
| ? | **Pin assignment** is in [04-hardware](04-hardware.md) §4 and goes into `config.h` as soon as firmware work resumes. |
| ? | **Bypass switch for the brake relays.** N6 keeps the stand braked when unpowered; for turning by hand (D1, balancing, mark mounting) this must be switchable off. |

> **On the calibration weight.** 500 g at the 80 mm hole is 9.81 N at the cell, i.e. 10 % of
> full scale; measurement operation lies at 12 to 63 N. Extrapolating a calibration at 10 % to 64 %
> relies entirely on linearity. Better: several masses up to
> about **4 kg** at the 80 mm hole (= 90 % FS), but at least three points over the
> range used.

## 11 Storage and Raw Log

As of 2026-11-26. Code: `storage.cpp`, `rawlog.cpp`, format in `calc/rawlog.h`, PC
decoder `tools/rawlog.py`. Wiring of the card: [04-hardware](04-hardware.md) §15.
Constants: [03-parameters](03-parameters.md) §10.

### 11.1 Two media

| Medium | File system | Size | Holds |
|---|---|---|---|
| microSD | FAT12/16/32, mounted by ESP-IDF (`esp_vfs_fat_sdspi_mount`) at `/sd`; **no exFAT** (FatFs in the Arduino core 2.0.14 is built with `FF_FS_EXFAT 0`), long file names on | card | `/vmrr/<session>/`: raw log (**only here**), run protocol and points; `/vmrr/wheels.md` mirror of the wheel-part lists |
| internal flash | NVS (settings, calibration) and LittleFS, partition `spiffs` | 3.4 MB | `/profiles.tsv`; `/vmrr/<session>/` protocols only when no card is ready at the end of a run |

**Layout (since 2026-11-26), the same on both media:** one directory per run or manual
raw log, `/vmrr/<session>/`, `<session>` = run id = local time `YYYY-MM-DD-HHMMSS`
(`up<seconds since boot>` while the browser has not set the clock). In it `raw_<session>.rrl`
(`raw_<session>-b.rrl` … after a name clash, never overwritten), `protocol.md`, `points.csv`, and
for a manual raw log `info_<session>.md` (what it is, the header constants as a table). Files from
before 2026-11-26 stay in `/raw` and `/runs` and are reachable in the file browser.
The file system type (FAT32 …, LittleFS) is shown next to the medium under Setup >
Storage, in `/api/state` (`sto.sdFs`, `sto.flFs`) and by `D` on the console.

**The flash holds only what is written rarely and at standstill** (decision
2026-11-26): settings and calibration in NVS, and — without a card — the run protocols
written at the end of a run. Running data never goes there (§11.5). Every persisted
setting carries the local time of its last change (NVS key `t_<key>`), shown next to the
value under Setup (`chg` in `/api/state`).

One task on core 0 (`storage`) mounts both, polls the card every `STORAGE_CHECK_MS`
(CMD13 while mounted, a mount attempt otherwise — there is no card-detect contact),
recomputes the usage every `STORAGE_STATS_MS` and writes the raw log. The main loop
never waits for a file system.

State per medium, shown under **Setup > Storage** and by `i` on the console:

| State | SD | Flash |
|---|---|---|
| absent | no card answers | — |
| ready | mounted, write probe succeeded | mounted |
| read-only | mounted, write probe failed | — |
| unreadable | card answers, no FAT (unformatted, exFAT, damaged) | LittleFS does not mount |

Next to it the usage in % as a bar. **Format:** the SD card also when readable (since
2026-11-26: a fresh FAT card on demand), from the file browser after a confirmation
dialog; refused while a run is active or a raw log is written to it. The flash only when
*unreadable* (it holds the wheel profiles), and only at standstill. Console: `Fs` / `Ff`,
without a question.

Until 2026-11-25 `run.cpp` called `LittleFS.begin(true)`, which formats silently when
mounting fails. That is gone: an unreadable flash is reported and waits for the
operator.

### 11.2 Partition table

`default_16MB.csv`, unchanged. A custom table with an 11.9 MB LittleFS was planned on
2026-11-25 for raw data on the flash; it was dropped on 2026-11-26 together with the
flash fallback of the raw log, before it was ever flashed. LittleFS (3.4 MB) is
plenty for protocols.

### 11.3 What is recorded

Everything the sensors deliver, as raw values, with a timestamp in **µs** (`micros()`,
the finest unit the ISRs and the main loop share; the ISR latency is of the same
order):

| Record | Source | Rate | Content |
|---|---|---|---|
| EDGE | Hall ISRs, `speed::rawEdges` | every edge | channel 0 marks, 1 index, 2 wheel, 3 motor Hall; bit 7 = rejected as echo |
| HX711 | `torque::update` | 80 /s | raw count of cell 1 (24 bit) |
| VESC | each values packet | 5 /s (`VESC_POLL_MS`) | ERPM, tachometer, duty, motor and input current, voltage, both temperatures, fault, status — in the controller's own resolution |
| TIRE | AirSpy / manual entry | on each new value | pressure, temperature, battery |
| CTRL | `speedctl`, `brake`, `state` | on change and with each SYNC | target speed, duty command, brake, phase |
| TEXT | `logger` notes | on each note | text |
| SYNC | — | every `RAWLOG_SYNC_MS` | absolute `micros()` and local Unix time |
| GAP | — | after an overflow | bytes dropped |

Derived quantities (speed, force, Crr) are **not** recorded; they come from the
evaluation. The file header carries the constants needed for it (drum circumference,
lever arm, N per count, tare offset, marks per revolution, ...).

Volume, estimated from the rates above (not yet measured): about 2 kB/s while driving,
roughly 7 MB per hour.

### 11.4 Format `.rrl`

```
"RRL1" | u16 text length | header text (key=value lines)
records: u8 type | time delta (zigzag varint, µs, signed) | payload of fixed length
         SYNC: u8 type | u32 abs_us | u32 unix_s   (no delta)
         TEXT: u8 type | delta | u8 length | text
```

Little endian. The delta is signed because the ISR edges reach the main loop only on its
next round and can be older than a record already written. Record sizes: edge 3–7 B,
load cell 5–9 B, controller packet 28–32 B (the delta takes 1–5 bytes). The decoder accumulates the deltas into a
64-bit `t_us` from the start of the recording, so the 71-minute wrap of `micros()`
does not matter (host test `test_rrl_roundtrip`). A file cut off by a power failure is
valid up to the last complete record.

### 11.5 Path of the data and the flash rule

Main loop → encoder → ring in PSRAM (`RAWLOG_BUF_BYTES`) → storage task → file in
`RAWLOG_CHUNK_BYTES` pieces, committed every `RAWLOG_FLUSH_MS`. If the ring is full,
records are dropped and a GAP record notes how many bytes.

**Raw data goes to the SD only.** While the ESP32 erases or writes its flash, cache and
every interrupt not in IRAM are locked on *both* cores — including the Hall ISRs
(`attachInterrupt` does not use `ESP_INTR_FLAG_IRAM`). A sector erase takes tens of
milliseconds; edges would arrive late or merge, the main loop would stand. Until
2026-11-26 the raw log fell back to the flash and wrote it only at standstill; that
fallback is gone. Without a ready card there is no raw log, and the run checklist says
so (row *Storage*, yellow). The SD hangs on SPI2 and locks nothing; it is written
continuously. The ring (1 MB, about 8 minutes) only has to bridge the card's own
pauses. Flash usage and formatting still wait for `RAWLOG_FLASH_IDLE_MS` of standstill.
Requirement: [01-requirements](01-requirements.md) F8a.

Reading from the flash (downloads in the file browser) also briefly locks the cache per access —
shorter, but large downloads belong to standstill as well.

### 11.6 Operation

- **Automatic** while a guided run is active **from step 2 on** (since 2026-11-26; step 1
  only creates the run, and tapping *Measure* alone must not start a recording), on the
  SD. Without a ready card: no raw log, the run still works.
  The whole run is recorded continuously, waiting times between steps included. Every
  step change writes a TEXT mark `run <step>.<sub>` into the file, and the results of
  the standstill readings follow as TEXT next to their samples (`tare raw_counts=… sigma_counts=… pp_counts=…`, `check_weight raw_above_tare=…`,
  `check_weight_after_stop …`, `zero_after_stop …`), so each window can be cut out of
  `_loadcell.csv` by time. The protocol names the raw file.
- **By hand:** the button *Turn recording on* / *Stop recording* under Manual (since 2026-11-26; it was under Setup >
  Storage), console `L1` / `L0` (refused without a ready card). A run's own recording
  cannot be switched off there.
- **Protocols** (`/vmrr/<id>/protocol.md`, `points.csv`) go to the SD if it is ready at
  the end of the run, otherwise to the flash.
- File name: `/vmrr/<session>/raw_<session>.rrl` (§11.1). An existing file is never overwritten.
- **Download:** in the file browser (§11.7); per raw file *Download* and *CSV .zip*.
  The zip is converted while sending: one CSV per record type
  (`_loadcell.csv`, `_edges.csv`, `_vesc.csv`, `_control.csv`, `_tire.csv`,
  `_events.csv`) plus `_info.txt` with the header constants and row counts. Streamed
  zip with data descriptors over HTTP chunked — one read pass per CSV, no intermediate
  file. If the file is not a readable `.rrl`, the zip contains it unchanged.
  While a download streams, the web task serves nothing else.
- **On the PC:** `python3 tools/rawlog.py <file>.rrl` gives the same CSVs (cross-checked
  against the firmware decoder).

### 11.7 File browser

Setup > Storage > *Files on SD card and flash* opens a page of its own (since 2026-11-26):
SD / Flash switch, *↑ Back* and the path as breadcrumbs, directories newest first, per file
size and time. A tap on icon or name opens a directory or views a file; *Download*, *.zip* and
*CSV* sit right-aligned. No Refresh button: the listing reloads by itself while a raw log is
written and when recording starts or stops (nothing else changes it while it is open).
Viewing: (`.md` rendered, `.csv`/`.txt` as text, `.rrl` as an overview: start, duration, size, complete or cut off, dropped bytes, records per type with rate, bytes and field types, edges per channel, header, events), *Download*, for raw files *CSV .zip*,
per directory *.zip*. *Format SD card* small at the very bottom, with a spinner and the elapsed seconds while the stand formats (FatFs reports no progress), then a fresh listing, then *Formatted in n s* or the reason (`sto.sdFmtRes`). The UI waits for the counter of finished formats (`sto.sdFmtN`), not for the short *formatting* phase, which a poll can miss; file requests get 503 while formatting, so they never stall the web task on the SD lock. Formatting a readable card runs FatFs `f_mkfs` (FAT type by size); until 2026-11-26 it only re-mounted with `format_if_mount_failed`, which does nothing on a readable card. *Close* at the top leaves the browser, *↑ Back* next to the path goes one step back. Navigation runs through the URL
hash (`#fb:sd/vmrr/<session>`, `#fv:<url>`), so the browser's back button walks back
through directories and closes the viewer; *Back* in the bar does the same. The Setup
tab itself is kept as `#setup`, so a refresh stays there.

| Endpoint | Delivers |
|---|---|
| `GET /api/files?m=sd\|flash&p=<dir>` | the directory as JSON: `d` = [{n, dir, s, t}], `fs`, `state`, `rec` (file being recorded) |
| `GET /fs/<medium><path>` | the file, `application/octet-stream`, name `<session>_<file>` |
| `…?view=1` | the same as `text/plain` inline (the viewer) |
| `…?csv=1` | a raw file converted to one CSV per record type, as streamed ZIP |
| `…?zip=1` | all files of a directory (not recursive) as streamed ZIP |
| `…?info=1` | overview of a raw file as JSON: time span, records and bytes per type, edges per channel (echo rejected), dropped bytes, header, first 30 and last 10 TEXT events; shown by the viewer |

The flags need a value (`?zip=1`, not `?zip`): the Arduino WebServer drops query
arguments without `=` (`Parsing.cpp`), which made every *.zip* answer 404 until
2026-11-26.

SD access: the storage task writes the raw log under the same SD lock. A plain download
takes the lock per 1.4 kB chunk, so it never stalls the raw log for long. The two ZIP
variants hold it for the whole file and are therefore refused (409) while a run or a raw
log is active on that medium. The old pages `/runs` and `/raw` are gone.

### 11.7 Open

| | Question |
|---|---|
| ? | **Deleting raw files** from the UI. Not yet. |
| ? | **Second load cell** (wheel load) — gets its own record type once it is fitted. |
| ? | **First test with a card.** Firmware builds and the format is host-tested; insert/remove detection, the write probe and formatting have not run on the stand. |
