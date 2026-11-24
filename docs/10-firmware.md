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
| N6 | M | On reset and on boot the relays are dropped, so the stand is braked. |

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
| L1 | M | The DevKit's RGB LED shows **exclusively** the state of the operator connection. No startup animation: the connection indicator is up a few milliseconds after start and says more. |
| L2 | M | **Steady green** = an operator's computer is reporting in, **blinking red** = waiting. Written only on a state change; `neopixelWrite()` occupies the RMT and takes about 30 µs. |
| L3 | M | The animation is **non-blocking** (G1): `statusled::service()` runs from `loop()`, there is no `delay()`. |

Constants in [03-parameters](03-parameters.md) §8; the pin question v1.0/v1.1 is listed
there as TBD.

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

Hang a test weight on the lever arm, derive `LOADCELL_N_PER_COUNT` from it.

| | P | Requirement |
|---|---|---|
| W1 | M | Record the zero point (arm unloaded), mean over ≥ 200 values. |
| W2 | M | Enter the mass of the test weight. The **lever arms** are separate fields on the same page (cell arm 80 mm, calibration arm 160 mm), measured on the assembly, not typed in per weighing. |
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
| L12 | M | Compute and display the **rolling radius `r_Rad` from the speed ratio** `r_Trommel · n_Trommel / n_Rad` instead of having it entered. Averaging over ≥ 30 s on a plateau. |
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
| L9 | M | The apparent torque is **subtracted per point**: `M = M_roh − J_eff·dω/dt` with the net acceleration as the slope of the regression line v(t) over the point (since 2026-11-24, `state.cpp`). Residual: `UNC_J_REL` = 5 % of it. |
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
| `u` | **Spin-down + idle wizard** | `u` | The drum runs with the wheel resting on it to `WIZ_KMH` = 40 km/h; then lift the wheel off. If it turns slower than it would have to via the drum (`WIZ_SLIP_RATIO`), it is lifted off, and at the same time the wheel spin-down (A₁, B₁ from the pulses, J_Rad from mass and circumference) and the drum idle run over all plateaus (A₀, B₀ from the cell) begin. Both are stored. `uq` acknowledges the last step. In the web UI as a wizard under *Setup → Loss models*. Since 2026-11-23 |
| `y<kg>` | Wheel mass including tire | `y2.4` | J_Rad = `WHEEL_J_FACTOR` · m · r_Rad² |
| `w<A₁> <B₁>` | Wheel-loss model by hand | `w0.012 0.000071` | from the wheel spin-down |
| `n<A₀> <B₀>` | Idle-loss model by hand | `n0.004 0.000209` | the idle run `m0` fits and stores it itself since 2026-11-23; from the baseline run |
| `t` | static tare | | offset |
| `R` | **Guided measurement run** | `R`, then `Rk`/`Rs`/`Rn`/`Rx`, `Rm<k>=<v>` | Fourteen steps ([11-run-review](11-run-review.md) §3): create run, visual check, system check, zeroing, control weight, idle-loss model (adopt or pre-run with wheel lifted), wheel model, wheel resting; then on its own warm-up (`WARMUP_MS` at `WARMUP_KMH`, live point every minute), plateaus with `RUN_POINTS_PER_PLATEAU` points, anchor point, stop; optional idle post-run; report `.md` and data `.csv` in LittleFS under `/runs/<id>`, retrievable via `/runs`. Warnings W01…W12 in the report. In the web UI behind *Measurement*. Since 2026-11-23 |

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
| ? | **Mass of the test weight.** Presumed 500 g, which at the 80 mm hole gives only **10 % of the cell's full scale**, while measurement takes place at 13 to 64 %. See note below. |
| ? | **Pin assignment** is in [04-hardware](04-hardware.md) §4 and goes into `config.h` as soon as firmware work resumes. |
| ? | **Bypass switch for the brake relays.** N6 keeps the stand braked when unpowered; for turning by hand (D1, balancing, mark mounting) this must be switchable off. |

> **On the test weight.** 500 g at the 80 mm hole is 9.81 N at the cell, i.e. 10 % of
> full scale; measurement operation lies at 12 to 63 N. Extrapolating a calibration at 10 % to 64 %
> relies entirely on linearity. Better: several masses up to
> about **4 kg** at the 80 mm hole (= 90 % FS), but at least three points over the
> range used.
