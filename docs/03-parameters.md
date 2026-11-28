# Parameters

Mirrors `firmware/src/config.h` 1:1. Every change here **and** there.

Origin: `meas` = measured · `calc` = calculated · `DS` = datasheet · `est` = estimated ·
`TBD` = to be determined.

## 1 Geometry

| Symbol | Constant | Value | Unit | Origin | Determination |
|---|---|---|---|---|---|
| U | `DRUM_CIRCUMFERENCE_M` | **2.842** | m | meas | **2026-11-23 with the tape measure around the finished surface: 2842 mm**, Ø 904.6 mm. For a while this said 2.931 (Ø 933) while `config.h` carried 2.837 — both replaced ([09-measurement-theory](09-measurement-theory.md) §1.5). The earlier value 3.000 applied to the planned Ø-955 drum and was 2.4 % too large. Re-measure after every change to the surface: tape measure around the drum, average 3 measurements. 1 mm error = 0.03 %. |
| r | `DRUM_RADIUS_M` | **0.4523** | m | calc | U / 2π with U = 2.842 m. **Since 2026-11-24 this value applies to all calculations in the documents**; the earlier 0.4665 (Ø 933) has been replaced everywhere. The radius enters every torque and every tangential force; an error here is 1:1 an error on Crr. |
| L | `TORQUE_ARM_M` | **0.084** | m | **meas** | **On the build 2026-11-23: 84 mm** drum axis to scale (stated, caliper accuracy not yet confirmed). Previously planned 0.080. Applies to the **5-kg cell**. Design case steady-state 1.83 Nm → 21.8 N = 44 % FS; including ramp 2.43 Nm → 28.9 N = 59 %. Derivation [08-stand](08-stand.md) §5.2. **Since 2026-11-28 L enters every torque 1:1:** the cell is calibrated directly (weights on the Bowden cable, arm lifted, `k = m·g / Δcounts`), the torque is `M = counts·k·L`. 1 mm on 84 mm = 1.2 %, so L has to be measured to ±0.1 mm before the first measurement (method: caliper, drum axis to the cable axis at the cell); budget item `UNC_ARM_REL`. Until then the lever calibration over `CALIB_ARM_M` (dropped) made L cancel out ([11-review](11-review.md) D2). |
| J | `DRUM_INERTIA_KGM2` | **1.94** | kg·m² | calc | From the geometry of the **Ø-905 drum** as built (core as a disc plus shell as a ring, [08-stand](08-stand.md) §4.5). The earlier value 2.07 applied to the Ø-933 plan and is struck (A14 done). **Enters the result since 2026-11-24:** per measurement point the firmware subtracts the apparent torque `J_eff·dω/dt` of the net acceleration over the averaging window (`state.cpp`, [11-review](11-review.md) K1); in the measurement run `J_eff = J + J_wheel·(r/r_wheel)²`. Verify by spin-down test with a known added mass ([05-test-procedure](05-test-procedure.md) A5.3). |
| — | `UNC_J_REL` | 0.05 | – | est | Relative uncertainty of J as long as it is only calculated. Together with `DVDT_CONST_KMH_PER_S` it determines the permissible net acceleration of a point (§5) and the residual of the inertia correction in the budget. |
| — | `DRUM_LOSS_CONST_NM` | **0.472** | Nm | **meas** | Constant part of the idle loss torque from the spin-down: drum bearings **and** motor friction, not separable in the spin-down. In the cradle measurement only the **motor part** cancels out (motor bearings, cogging torque, iron losses act between stator and rotor). The **drum bearings** are two pillow-block bearings for the 15-mm shaft on the stand; the motor is only flanged on and has no suspension apart from its two inner bearings — so they lie **outside** and are measured along. Their share sits in the A₀ of the idle-loss curve and is subtracted with it; the load-dependent increase under wheel load remains as `UNC_BEARING_REL` ([06-open-questions](06-open-questions.md) A11). |
| — | `DRUM_LOSS_OMEGA2_NM` | **3.178·10⁻⁴** | Nm·s² | **meas** | Factor of the ω² part: **aerodynamic drag of the drum**, the part that does *not* cancel out. Both values determined on 2026-11-17 from the **free spin-down**: 97 → 29 km/h in 140 s, eight support points, M = J·dω/dt, fit M = a + b·ω². At 60 km/h this gives 0.43 Nm of aerodynamic drag — [09-measurement-theory](09-measurement-theory.md) quotes 0.27 Nm for it, the measured value is **1.6 times larger**. Against a useful signal of 1.32 Nm (Crr 0.007 at 417 N) that is 33 % instead of 20 %, at 100 km/h even 91 %. Re-record as soon as anything changes on the drum surface or enclosure. |
| i | `BELT_RATIO` | 1.00 | – | spec | **Direct drive**: drum on the motor rotor, no transmission. Constant stays for the case of a later belt. |

## 2 Torque measurement

| Symbol | Constant | Value | Unit | Origin | Determination |
|---|---|---|---|---|---|
| — | `LOADCELL_CAPACITY_N` | 49.03 | N | stated 2026-11-16 | **5 kg**. The documents said 10 kg until then — read the nameplate, then set to DS. |
| `LOADCELL_TYPE` | "5 kg (nameplate not read)" | – | stated | Load-cell type for the protocol's "Stand build" section. Nameplate still to be read. |
| `LOADCELL_AMP` | "HX711, SparkFun SEN-13879, RATE trace cut 2026-11-26" | – | stated | Amplifier board as fitted, for the protocol; [04-hardware](04-hardware.md) §5.5. |
| `STAND_REV_LEN` | 160 | characters | spec | Longest stand revision text (Setup → Stand, console `B`). |
| — | `LOADCELL_N_PER_COUNT` | **0.00002668** | N/count | **meas** | **Calibrated 2026-11-23** with the lever calibration (dropped 2026-11-28): certified 1-kg weight at 330 mm, cell arm 84 mm → 38.53 N at the cell. One weight, linearity not checked. **Since 2026-11-28 determined directly at the cell:** arm lifted, stirrup on the Bowden cable, 200 / 500 / 1000 / 2000 g, least-squares line, factor = 1/slope ([05-test-procedure](05-test-procedure.md) A2, [09-measurement-theory](09-measurement-theory.md) §5.1). Enter the value of the first direct calibration here and in `config.h`. |
| — | `HX711_GAIN_PULSES` | 1 | – | DS | 1 = channel A gain 128, 3 = channel B gain 32 |
| — | `HX711_RATE_SPS` | 80 | 1/s | DS | RATE pin to VCC. At 60 km/h ≈ 14 samples per revolution. |
| — | `TARE_SAMPLES` | 80 | – | est | static tare, 1 s at 80 SPS (200 until 2026-11-24: the noise does not need it, 143 counts peak-to-peak are 0.001 N on F_roll; in practice the main loop sets the pace, it polls the HX711 and with a browser attached only 20 to 40 values per second get through) |
| — | `VESC_TACHO_PER_REV` | 90 | – | meas | 6 tachometer steps per electrical revolution × 15 pole pairs; 900 steps over 10 revolutions ([../hardware/vesc-config-fw7.01/README](../hardware/vesc-config-fw7.01/README.md)). The controller counts them even when the drum is turned by hand, which is how the quarter turn of the zero check is monitored |
| `RUN_TURN_DEG` | 90 | ° | est | zero check: how far the drum is turned by hand between the two zero readings |
| `RUN_TURN_TOL_DEG` | 5 | ° | est | Quarter turn accepted from 85°; the tachometer counts 4° per step, so 88° passes and 84° does not (builder, 2026-11-26). |
| `RUN_TURN_STANDSTILL_MS` | 1500 | ms | est | zero check: still for this long after the turn before the *Measure* button appears; the operator starts the second reading (no automatic start since 2026-11-26) |
| `TARE_TIMEOUT_MS` | 15000 | ms | spec | Abort the tare if not enough values arrive (5000 until 2026-11-24: with a browser polling, the main loop only picks up 20 to 40 cell values per second, and the tare ended by the clock after about 50 of 200 values). The progress bar follows whichever end comes first. Since 2026-11-20 the tare runs **non-blocking** (`tareBegin`/`tareBusy`/`tareCount`); without an abort a silent HX711 would occupy the `TARE` state forever. |
| — | `TRACE_N` | 160 | – | spec | Live curve of the raw values in the guided run while zeroing (step 4) and at the zero point after the run: the last 160 values, 2 s at 80 SPS. Display only. |
| — | `DISPLAY_AVG_N` | 40 | – | spec | Moving average for the **display**, 0.5 s at 80 SPS. A number jittering at 80 Hz is neither readable nor meaningful ([10-firmware](10-firmware.md) W5, D6). Has nothing to do with the revolution-synchronous averaging of the **measurement** (`AVG_REVS_PER_POINT`) — the two run independently. |

> Since 2026-11-23 the factor is set. As long as `LOADCELL_N_PER_COUNT` were TBD, the firmware would output no Crr values,
> only raw values. That is intentional.

## 3 Speed sensors

| Symbol | Constant | Value | Unit | Origin | Determination |
|---|---|---|---|---|---|
| k | `MARKS_PER_REV` | 8 | – | spec | screwed-on magnets on the pitch circle of the 8 spacer blocks, [04-hardware](04-hardware.md) §9 (mounted). More marks bring no accuracy, only latency. **Since 2026-11-24 the firmware counts every 8th edge as a full revolution without a separate index sensor** — exact, independent of the pitch. A single Hall at the 8 marks is therefore enough for the measurement; the second Hall (`PIN_INDEX`) now only provides the fixed angular zero for diagnostics. |
| — | `MARK_PATTERN_INDEX` | true | – | spec | Detection of the optional **intermediate magnet** ([04-hardware](04-hardware.md) §8.6): a ninth magnet centred in a 45° gap gives the single mark Hall a fixed angular zero and lets the revolution count re-lock every revolution. Automatic; without the magnet the firmware counts every 8th edge. |
| — | `MARK_SHORT_RATIO` | 0.7 | – | est | An interval below this fraction of the reference (`MARK_REF_N`, median) counts as a half (22.5°). The ramp changes a 20-ms interval by 0.1 %, pitch errors of the marks are a few percent — 0.7 separates safely. |
| — | `MARK_MISSED_RATIO` | 1.6 | – | est | Above this an edge counts as swallowed (counter *swallowed* on the sensor side). |
| — | `MARK_PATTERN_CONFIRM` | 2 | – | est | This many times "short, short" must come at a spacing of exactly 9 pulses before the pattern is accepted — a single spurious pulse produces it too, but not periodically. |
| — | `WHEEL_MARKS_PER_REV` | **1** | – | spec | Magnets on the spokes of the test wheel, sensor at the dropout, [09-measurement-theory](09-measurement-theory.md) §4. Since 2026-11-22 **one** magnet is fitted. Speed over a window of one full revolution; more magnets would bring no accuracy, only a faster update. The value must match the wheel, otherwise speed and wheel circumference are wrong by this factor. |
| — | `WHEEL_CIRC_WINDOW_REVS` | 10 | – | spec | moving window for the wheel circumference, in wheel revolutions; recalculated on every pulse ([04-hardware](04-hardware.md) §14.1). At 15 km/h that is about 4 s. |
| — | `WHEEL_CIRC_GAP_MS` | 3000 | ms | est | without a wheel pulse the wheel counts as stationary, the window expires |
| — | `WHEEL_CIRC_SLIP_REL` | 0.03 | — | est | Slip / wheel lifted: the circumference from the last single wheel revolution may deviate from the window mean by at most this much, otherwise the window is discarded and the display goes blank (2026-11-26). Check on the stand: steady running must stay well below. |
| — | `MIN_PULSE_US` | 800 | µs | calc | Shortest spacing: 60 km/h, 8 marks → 22.5 ms. At 100 km/h → 13.5 ms. 800 µs safely filters only interference. |
| — | `MARK_REF_N` | 9 | – | spec | Reference "regular mark spacing" for the intermediate-magnet pattern = median of the last 9 spacings (at most 2 of 9 are halves). Until 2026-11-25 a moving mean of regular spacings only: if it started with a slow spacing at start-up (817 ms), every later spacing counted as short, the mean was never updated again, and the intermediate magnet was never detected. |
| — | `MARK_PATTERN_MIN_KMH` | 6 | km/h | est | Below this no spacing is classified (short/missed): at start-up the speed changes too much within one revolution. Below it the pattern is neither confirmed nor checked; the speed window keeps the confirmed 9 pulses. |
| — | `MARK_ECHO_PCT` | 35 | % | est | **Double pulse filter** on the drum marks: an edge after less than 35 % of the last mark spacing is an echo of the same mark and is discarded (counted, shown in `i`). Found on 2026-11-25: three jumps of the mark speed by exactly one extra pulse, e.g. 99.4 → 113.8 km/h = 8/7 at 100 km/h. `MIN_PULSE_US` does not catch it. 35 % lets the intermediate magnet (half spacing) through. Active from the second spacing after a standstill. |
| `WHEEL_ECHO_PCT` | 50 | % | est | A test-wheel edge after less than 50 % of the last wheel interval is a phantom and is rejected (bit 7 in the raw log). Found 2026-11-26: the brake relays dropping put a second pulse 5 and 17 ms after a real one; in the spin-down that made the wheel model about eight times too high. |
| — | `MOTOR_HALL_MIN_US` | 100 | µs | est | Glitch filter on the motor Hall edge; at 480 rpm and 15 edges per revolution the spacing is 8.3 ms. |
| — | `SPEED_STALE_GRACE_US` | 100000 | µs | est | The last pulse may be this much older than two **mark spacings**; beyond that the drum counts as standing and the speed is 0. Until 2026-11-25 the yardstick was two revolutions: in the brake comparison the display hung at 1.7 km/h for 12 s after the drum had stopped, and the relay brake waited that long for "standstill". |
| — | `STALL_TIMEOUT_MS` | 2000 | ms | calc | The limiting case is not 10 km/h (135 ms at 8 marks) but the run-up: at 1 km/h it is 1.35 s. 2000 ms means *slower than 0.68 km/h counts as standstill*; the ramp is above that after 1.4 s. |
| — | `SLIP_TOLERANCE` | 0.05 | – | est | permissible relative deviation of n_motor/i against n_drum |
| p | `MOTOR_POLE_PAIRS` | **15** | – | **meas** | **Measured 2026-11-17** via the Hall sensors: over 10 marked hand turns the controller's tachometer counter rose by exactly **900** steps, without a single backward step. 6 steps per electrical revolution → 900/10/6 = 15 pole pairs (30 poles). The scaling "6 per electrical revolution" was checked on the same data set against the ERPM integral (6.31; the excess is jitter). A first attempt **sensorless** gave 16.5 and was not reproducible — the position observer wobbles at hand speed; only with Hall sensors is the counter a measurement. Until then the controller was set to `si_motor_poles` = 14 — that is the **factory default** of the VESC firmware, proven on the controller freshly flashed to 7.01 (it shows 14 again there). The firmware does **not** determine the pole count, it is typed in; without an entry it stays at 14, and every mechanical speed derived from it is wrong. |
| — | `MOTOR_PULSES_PER_REV` | **15** | – | calc | = `MOTOR_POLE_PAIRS`. Hall edges of one phase per motor revolution. With direct drive at the same time a high-resolution drum speed source (90 edges per revolution over all three phases). |

## 4 Drive

**Installed is the 6.5″ hoverboard drive wheel, 36 V / 350 W** (procured 2026-10-28),
plus an **FSESC 4.20 / 50 A** as controller ([04-hardware](04-hardware.md) §10).

| Quantity | Value | Unit | Origin | Note |
|---|---|---|---|---|
| Type | 6.5″ hoverboard hub motor | – | listing | |
| Rated voltage / power | 36 / 350 | V / W | listing | |
| No-load speed at 42 V | **589** | rpm | meas | bare drum above 100 km/h, 2026-11-03 |
| Continuous torque `P/ω` | **6.96** | Nm | calc | factor 2.8 above the design case of 2.51 Nm |
| n → drum at 36 V | 505 rpm → **86 km/h** | – | calc | 60 km/h needs 353 rpm, so 43 % reserve |
| kv | **14.0** | rpm/V | meas | 589 rpm at 42 V — **this is a system quantity**, measured with the drum mounted and hence against its aerodynamic drag, not a motor constant. The motor constant is in §4.1: 19.1 rpm per bus volt on the free motor. |
| kt | 0.68 | Nm/A | calc | 9.55 / kv — **superseded**, because calculated from the system quantity kv. Measured is **0.389 Nm/A**, see §4.1. |
| Mass | 2.26 | kg | listing | enters the natural frequency of the cradle |
| Hall sensors | yes, 3 on a board in the stator | – | photo of the opened motor | 3 blade connectors (yellow/green/blue) + white JST with 6 wires, pinout in [04-hardware](04-hardware.md) §11 |
| Temperature sensor | **10-k NTC** | – | **meas** | White wire on the Hall connector, runs into the winding pack. Confirmed three ways on 2026-11-17 (A5 done): open terminal −75.5 °C, 1.6 K offset to the on-board FET sensor, monotonic rise under hot air against a flat reference. `m_motor_temp_sens_type` = NTC 10K. Only the **limit value** remains open. |
| **Mechanical brake** | **no** | – | – | Braking is done by **shorting the three phases** via relay (installed and measured: 3.25 Nm) or via the controller with `VESC_BRAKE_CURRENT_STOP`. The relays act when de-energised, see [07-safety](07-safety.md) §3. |
| Pole count | **30** (15 pole pairs) | – | **meas** | counted 2026-11-17 via the Hall sensors, see `MOTOR_POLE_PAIRS` in §3 |
| Phase resistance | **0.2387** | Ω | **meas** | see `MOTOR_R_OHM` in §4.1 |

### Not installed: scooter motor (DAAO)

The following table was the alternative and is now only of interest as a comparison.
Data from the nameplate.

| Quantity | Value | Unit | Origin | Note |
|---|---|---|---|---|
| Manufacturer | DAAO Electric Co. Ltd | – | nameplate | |
| Type | 23328N36A28G001 | – | nameplate | |
| Rated voltage | 36 | V | nameplate | |
| Rated power | 350 | W | nameplate | |
| Peak power | 800 | W | seller | starting reserve for the flywheel mass |
| **Rated speed** | **605** | **rpm** | **nameplate** | the design figure |
| Tire | 9.0 × 2.125 | inch | tire sidewall | solid rubber |
| Wheel circumference | 0.718 | m | calc | Ø 9.0″ = 228.6 mm |
| kv at the wheel | 16.8 | rpm/V | calc | 605 / 36 |
| kt | 0.568 | Nm/A | calc | 9.55 / kv |
| Rated torque | 5.52 | Nm | calc | 350 W / 63.4 rad/s |
| Peak torque | ≈ 12.6 | Nm | calc | 800 W at rated speed |
| Torque at the drum | 5.52 | Nm | calc | 5.52 · i, **i = 1.00** (direct drive) |
| Mechanical brake | drum brake | – | description | **the installed hoverboard motor does not have one** |
| Connector | Julet 9-pin | – | photo | 3 phases + 5 Hall + PTC expected — **verify with the meter** |
| Hall sensors | yes | – | connector pinout | 9-pin Julet, expected 3 phases + 5 Hall + PTC |
| Temperature sensor | PTC | – | description | white cable; characteristic **TBD** |
| Brake | drum brake integrated | – | description | mechanical braking path, see [07-safety](07-safety.md) |
| Pole count | **TBD** | – | TBD | turn the motor one revolution by hand, count Hall edges |
| Phase resistance | **TBD** | Ω | TBD | four-wire measurement between each pair of phases |
| Mass | **TBD** | kg | TBD | weigh — enters the natural frequency of the cradle |

### Selection criterion for a hub motor

Should the motor ever have to be replaced: **the rated power alone says nothing.**
Two motors with the same nameplate deliver different torque depending on rated
speed. Two quantities have to be checked:

```
T_cont = P_rated / (2π · n_rated / 60)   ≥ 2.51 Nm   (design case including the ramp, §6)
kv      = n_rated / U_rated               = 14…20 1/min/V
```

The kv window follows from the drum speed: 341 rpm at 60 km/h, divided by
36 V bus voltage, gives 9.25 rpm/V as the absolute lower limit — in practice 14 to 20 with
voltage reserve for the current. Listings with 100 kv or more are motors meant to be
geared down, not direct drive.

From this the minimum power:

The following table is still calculated with the **old** MTB design case (3.53 Nm) and
is therefore conservative by a factor of 1.4. For today's design case of 2.51 Nm, 71 % of the
quoted values suffice — the recommendation still stands, because nameplate data of
cheap motors are optimistic.

| n_rated at 36 V | ω | Minimum (old, 3.53 Nm) | with 50 % reserve |
|---|---|---|---|
| 400 rpm | 41.9 rad/s | 148 W | 222 W |
| 500 rpm | 52.4 rad/s | 185 W | 277 W |
| **605 rpm** (installed motor) | 63.4 rad/s | **224 W** | **336 W** |
| 700 rpm | 73.3 rad/s | 259 W | 388 W |

In practice: **250 W minimum, 350 W right.** Below 400 rpm rated speed it gets
tight regardless of power, because hardly any voltage reserve remains above the 341 rpm
needed for 60 km/h.

The reason for the apparently large gap between the 123 W of mechanical demand (§4 in
[09-measurement-theory](09-measurement-theory.md)) and the required 224 W rated power: the test stand
is **torque-limited, not power-limited**. At 341 rpm the motor runs at 55 %
of its rated speed and can deliver at most 55 % of its rated power.

> Nameplate data of cheap motors are optimistic; the thermal continuous rating
> is frequently lower. The table applies to 42.5 kg wheel load and the old Crr limit
> of 0.015. Only
> racing tires (Crr ≤ 0.008) would get by with 2.06 Nm and thus 200 W; 100 kg wheel load
> doubles everything.

### Speed reserve

With **direct drive** the speed is not voltage-limited. Drum and
motor speed are identical; 605 rpm rated speed at 36 V already gives
**109 km/h** at the drum — almost twice the permitted 60 km/h.

| | n_motor = n_drum | v at the drum |
|---|---|---|
| Rated speed at 36 V | 605 rpm | 109 km/h |
| **permitted** | **341 rpm** | **60 km/h** |

So the limit is **mechanical**, not electrical: balance quality and strength of the
wooden structure, see [07-safety](07-safety.md) §5 and A2 in
[06-open-questions](06-open-questions.md). A higher bus voltage gains nothing as long as
this release is not in place.

Conversely this means: at 341 rpm the motor runs at 55 % of its rated speed, and the
usable power is capped accordingly — the test stand is torque-limited, not
speed-limited.

### Alternative: 6.5″ hoverboard hub motor

TY36VH25B350W, 36 V / 350 W, 2.26 kg. Rated speed ≈ 480 rpm (**derived** from the
usual hoverboard top speed, not on the nameplate — the 605 rpm
of the DAAO motor, by contrast, are printed on it).

| | Hoverboard 6.5″ | Scooter 9.5″ (DAAO) |
|---|---|---|
| Continuous torque `P/ω_rated` | **6.96 Nm** | 5.52 Nm |
| Hall sensors | yes — 3 thick phases (yellow/green/blue) + white JST with 5 thin wires | yes |
| n_rated → drum max | 480 rpm → 86 km/h | 605 rpm → 109 km/h |
| Temperature sensor | probably yes — white wire into the winding pack, type **TBD** (A5) | PTC |
| Mechanical brake | **no** | drum brake |
| Peak load | unknown | 800 W |
| Mass | 2.26 kg | heavier, **TBD** |
| Bolt circle for the drum adapter | 6.5″ | 9.5″, stiffer |

Both meet even the old requirement of 3.53 Nm with reserve, today's of
2.51 Nm all the more. The earlier assessment that the
"6-wire cable whip" made Hall sensors doubtful has been settled by the listing photos:
there are 8 conductors, the description only counted the Hall strand.

What remains as a difference is the **mechanical brake**, and with 13 minutes of
free spin-down (see [09-measurement-theory](09-measurement-theory.md) §8.9) that is not a comfort feature.
It can be retrofitted, though: band brake or brake caliper on a disc on the
drum shaft.

**Decision rule:** If the hoverboard wheel is clearly cheaper, take it and plan a
brake on the drum. If the prices are close, take the scooter motor and
save yourself the two side projects brake and temperature sensor.

The axle question A6 — bearings on both sides — applies equally to both and decides
about the direct drive, not about the motor choice.

## 4.1 Motor, electrically measured

Everything determined on **2026-11-17** on the installed motor, controller FSESC 4.20 (at that time still
firmware 5.02).

The controller held a **foreign configuration**. Which values in it were really foreign
could only be separated after flashing to 7.01 — the factory state has been known since:

| | found | factory | |
|---|---|---|---|
| `si_motor_poles` | 14 | **14** | factory default |
| `si_wheel_diameter` | 0.083 m | **0.083 m** | factory default |
| `s_pid_kp` / `ki` / `kd` / `min_erpm` | 0.004 / 0.004 / 0.0001 / 900 | **same** | factory default |
| `si_gear_ratio` | 2.769 | 3.0 | set by hand |
| R / L / λ | 0.0147 Ω / 2.02 µH / 1.65 mWb | 0.015 / 7 µH / 2.45 mWb | **detection on a different motor** |
| `foc_openloop_rpm` | 700 | 1500 | set by hand |

So a motor detection had been run on a foreign motor, and two values were
typed in. *Earlier versions of this document called everything a
"skateboard configuration" across the board — that was overreaching.*

| Symbol | Constant | Value | Unit | Origin | Determination |
|---|---|---|---|---|---|
| λ | `MOTOR_FLUX_LINKAGE_WB` | **0.017287** | Wb | **meas** | Detected by the controller (`foc_detect_apply_all` with 50 W loss limit). Peak value of the phase flux linkage. Cross-check: 763 rpm give 20.7 V phase peak voltage with it, and 40 V bus delivers at most 40/√3 = 23.1 V via space-vector modulation — ratio 0.90, consistent with full modulation. |
| kt | `MOTOR_KT_NM_PER_A` | **0.389** | Nm/A | calc | kt = 3/2 · p · λ for a three-phase PMSM under field-oriented control. Cross-check on the run-up: 3 A gave 1.17 Nm, and the drum accelerated as expected from `DRUM_INERTIA_KGM2`. |
| R | `MOTOR_R_OHM` | **0.2387** | Ω | **meas** | `measure_res_ind` with **direct** wiring: 237.2 mΩ; the detection entered 238.7 mΩ. **With** the short-circuit relay block in the path 251.9 mΩ (mean of four runs, scatter ±1.5 mΩ), i.e. 14.7 mΩ contact resistance. That is an efficiency item and does **not** appear at the load cell — electrical losses are no mechanical reaction torque. |
| — | `MOTOR_NO_LOAD_RPM_PER_V` | **19.1** | rpm/V | **meas** | No-load speed per bus volt, on the **free** motor over ten support points from duty cycle 0.50 to 0.95 at 40 V, strictly linear (11384 to 11450 ERPM per unit of duty cycle). Extrapolated 11450 ERPM = **763 rpm** at full modulation, 130 km/h at the drum. **Not** to be confused with the "kv = 14" from [06-open-questions](06-open-questions.md) A13: that is the top speed of the system **with** drum, i.e. against its aerodynamic drag — a system quantity, not a motor constant. |
| L | — | *(open)* | H | **TBD** | `measure_res_ind` scattered over four runs from **305 to 377 µH**; the detection entered 403.8 µH. With ±13 % scatter no number may stand here. Needs a different method (LCR bridge at 1 kHz, rotor in a defined position). |

## 5 Measurement operation

| Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|
| `V_MIN_KMH` | 10 | km/h | spec | |
| `V_MAX_KMH` | **100** | km/h | spec | **100 since 2026-11-25**, decision of the builder, entry in [07-safety](07-safety.md) §5; 60 until then. `MAX_DRUM_RPM` and `VESC_ERPM_MAX` follow it. |
| `PLATEAUS_KMH` | 15, **28.8**, 40, 50, 60 | km/h | spec | **Default** of the measurement speeds; since 2026-11-23 the valid list lives in the settings (NVS) and is maintained via *Setup → Measurement*, sorted, with `h<kmh>` / `h-<kmh>`. 28.8 km/h is the reference speed of the published Crr database — together with 42.5 kg load a directly comparable point; 60 km/h was `V_MAX_KMH` until 2026-11-25, the default list still ends there. Baseline run and measurement run drive the same list. |
| `PLATEAU_MAX` | 16 | – | spec | Room for measurement speeds in the settings |
| `RAMP_KMH_PER_S` | 0.5 | km/h/s | est | 2 min from 0 to 60 |
| `SETTLE_MS` | 15000 | ms | est | Settling after plateau change, **minimum time**. Since 2026-11-24 settling does not end by the clock but by state (next three rows): the control loop needs 17 to 22 s to reach the target (§8.2), 15 s alone were too short ([11-review](11-review.md) D5). |
| `SETTLE_TOL_KMH` | 0.3 | km/h | est | Band around the setpoint within which the actual value must lie for settling to count as finished. |
| `RUN_TARE_SETTLE_MS` | 300 | ms | est | Guided run: wait this long after the tare before the zero check starts. |
| `RUN_WARM_LIVE_MAX_MS` | 60000 | ms | est | Guided run, warm-up table: a live value older than this is not entered. |
| `LIVE_MIN_N` | 3 | – | est | Fewer values in the live window are no mean. |
| `WIZ_TOL_KMH` | 1.0 | km/h | est | Wizard: this close to `WIZ_KMH` counts as "on it". |
| `WIZ_WHEEL_MIN_RPS` | 0.5 | 1/s | est | Wizard: below this the wheel is not turning along, no wheel/drum ratio is formed. |
| `FAULT_SHOW_MS` | 30000 | ms | est | Console and UI: a controller fault stays in the status line this long (the controller itself clears it after 500 ms). |
| `NOTE_SHOW_MS` | 5000 | ms | est | Console and UI: an error message stays this long. |
| `HTTP_SUMMARY_MS` | 5000 | ms | est | Period of the HTTP summary line on the console. |
| `CONSOLE_LIVE_PERIOD_MS` | 200 | ms | est | Period of the live telemetry `j<s>`. |
| `CONSOLE_LIVE_DEFAULT_S` | 5 | s | est | `j` without a number. |
| `HX711_FRESH_MS` | 1000 | ms | est | No cell value for this long = "HX711 not responding" (80 values at 80 SPS). |
| `NOLOAD_VIN_FLOOR_V` | 10 | V | est | The no-load speed is computed with the momentary bus voltage; below this floor it is not divided (a controller without voltage reports 0 V, not "no value"). |
| `SETTLE_STEADY_MS` | 5000 | ms | est | It must have stayed within the band continuously for this long. |
| `SETTLE_MAX_MS` | 60000 | ms | est | After this the measurement proceeds anyway; the point is then judged by net dv/dt and marked invalid if necessary. |
| `AVG_REVS_PER_POINT` | 10 | – | est | at 60 km/h ≈ 1.8 s, at 10 km/h ≈ 10.8 s |
| `WARMUP_MS` | 1800000 | ms | lit | **30 min**, as on the reference test rig. Crr drops markedly with tire temperature. |
| `WARMUP_MIN_MS` | 600000 | ms | est | The warm-up may end early only after 10 min. Since 2026-11-26. |
| `WARMUP_STABLE_MIN` | 5 | min | est | Number of last minute values of M compared. |
| `WARMUP_STABLE_NM` | 0.025 | Nm | est | Settled when max − min of those minute values of the corrected torque M ≤ 0.025 Nm (0.055 N on F_roll, Crr 0.0001 at 49.8 kg). Absolute, since M is the small remainder after the losses. Run 2026-11-26-1542: minutes 8–12 and 13–17 spanned 0.014 Nm, minutes 1–5 0.06 Nm. To be checked over further runs.
| `WARMUP_TTIRE_K_PER_MIN` | 0.2 | K/min | est | And the AIRSPY tire temperature rises by at most this per minute over the window (skipped without sensor). |
| `WARMUP_KMH` | 28.8 | km/h | spec | Speed at which the warm-up time counts. Identical to `PLATEAUS_KMH[1]` and for the same reason: it is the support point of the published database. Previously the number only appeared in running text. |
| `WARMUP_TOL_KMH` | 1.0 | km/h | est | Band around `WARMUP_KMH` within which time is credited. Wider than `SPEED_TOLERANCE_KMH`, because here we are warming, not measuring. |
| `DVDT_WINDOW_MS` | 1000 | ms | spec | Window for dv/dt. `revsrc.cpp` stores eight samples in it and fits a straight line — a difference of two samples is too noisy for a validity decision. |
| `DVDT_CONST_KMH_PER_S` | 0.010 | km/h/s | **calc** | Acceleration whose apparent torque `J·dω/dt` amounts to 1 % of the signal of a good racing tire: 0.012 Nm over `J` = 1.94 kg·m² is 0.0062 rad/s² ([10-firmware](10-firmware.md) §7.1, L9). **Since 2026-11-24 the apparent torque is subtracted per point, not just gated** ([11-review](11-review.md) K1): what counts is the net acceleration over the averaging window, as the slope of the regression line v(t) over the point. Until then the gate in the code stood at `STEADY_DVDT_KMH_S` = 0.3 km/h/s peak value, which would have been 51 % of the signal; that constant is now only a display criterion. |
| `POINT_DVDT_NET_MAX_KMH_S` | **derived** → 0.2 | km/h/s | calc | `DVDT_CONST_KMH_PER_S / UNC_J_REL`: after the correction, `UNC_J_REL` of the apparent torque remains, so the net acceleration may be 20 times as large before the residual reaches 1 %. Points above this are marked invalid, not discarded (W03). The earlier `DVDT_STRICT_KMH_PER_S` (0.002) has been dropped: it was never used. |

## 6 Test specimen and load

| Quantity | Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|---|
| Lever balance, empty weight | `LOAD_EMPTY_KG` | 6.97 | kg | meas | Wheel, arm and fork without the sliding weight, referred to the axle: the builder measured 1.7 kg preload at 84 cm (2026-11-26), × 0.84 / 0.205. Until then 7.2 kg weighed at the axle. Runtime value under Setup → Load (`Ae<kg>`). |
| Lever balance, sliding weight | `LOAD_WEIGHT_KG` | 10.46 | kg | meas | `Aw<kg>`. 10.4 kg until 2026-11-26. |
| Lever balance, wheel arm a | `LOAD_ARM_A_M` | 0.205 | m | meas | Pivot to wheel axle, horizontal. `Aa<m>`. 0.202 m until 2026-11-26. |
| Sliding-weight position, default | `LOAD_POS_DEFAULT_M` | 0.84 | m | meas | Position b of the sliding weight as of 2026-11-23. In the guided run the operator enters b; the firmware computes N = (m_empty + m_weight · b / a) · g (`settings::loadFromPosN`, `Ap<m>`). A hanging-scale measurement entered as well overrides the calculation and its deviation goes into the report as a check of the lever model. |
| Wheel load, default | `LOAD_DEFAULT_N` | **488.7** | N | **calc** | From the four rows above. **49.8 kg, as of 2026-11-26:** (10.46 kg + 1.7 kg fork preload) at 84 cm over the wheel arm of 20.5 cm, lever ratio 4.10. Until 2026-11-26: 494.3 N = 50.4 kg from 7.2 kg empty plus 10.4 kg at 84 cm over 20.2 cm, lever ratio 4.16 — **calculated from the lever geometry**, not weighed ([11-review](11-review.md) D4). Default for the setting, changeable at the stand; for 416.9 N (42.5 kg) the weight would have to hang at 69.7 cm. **Since 2026-11-24 the load is weighed with gantry and hanging scale at the wheel axle** (next row); the weighed value replaces the calculation. |
| Standard wheel load | `STANDARD_LOAD_N` | 416.9 | N | lit | 42.5 kg — the de-facto standard of the published Crr database. Suggested value only; the actual load is entered via `l<value>`. |
| Wheel-load mechanism | — | lever balance | – | built | Sliding weight 10.4 kg, `N = m_empty·g + W·b/a` with a = 0.202 m, b up to 0.84 m. Design: [08-stand](08-stand.md) §5. Enters Crr **linearly** → needs to be accurate to 1 %. **Weighing (path a, since 2026-11-24):** gantry above the wheel axle, hanging scale pulls the axle vertically upward with the arm free until the wheel just lifts off the drum — the value read is the wheel load including wheel and arm. The formula then serves only as a plausibility check. **Path b, later:** two load cells directly at the axle (`PIN_HX7112_DOUT` is reserved for this), live display. |
| Design Crr | `CRR_DESIGN_MAX` | **0.007** | – | spec | Fast road tires are measured, not MTB tires. The slowest tire envisaged is the Continental Contact Urban at 0.00552, leaving 27 % headroom. See table below. |
| Tire pressure | — | **SKS AirSpy, BLE** | bar | meas | Since 2026-11-24 `tirepressure.cpp` reads the AirSpy at the valve (protocol §9), 2 Hz, moving average over 5 s; manual entry `p<bar>` remains as fallback. Appears per measurement point in the report, plus the value cold and after the warm-up. **[INFERRED]** Gauge pressure; absolute offset open in the sister project → `UNC_P_REL`. |
| Tire temperature | — | **AirSpy** | °C | meas | The same sensor delivers its temperature, at the valve in the rim bed: air in the tire, not the tread. Sufficient for the warm-up curve and the pressure rise; an IR sensor on the tread remains open (A5). |
| Motor temperature | — | **present** | °C | **meas** | 10-k NTC in the stator, A5 answered on 2026-11-17. Sensor is connected to the `TMP` input of the **controller**, read out via UART — [04-hardware](04-hardware.md) §11.4. Only the **shutdown limit** is still open, see [07-safety](07-safety.md) §4. |

### Measured Crr values as design basis

Uniformly taken at 42.5 kg and 28.8 km/h; torques calculated with r = 0.4523 m and
**without** baseline and wheel losses (those add about 0.51 Nm, see
[08-stand](08-stand.md) §5.1)
([source](https://www.bicyclerollingresistance.com/the-test)):

| Tire | Power | Crr | Torque at our drum |
|---|---|---|---|
| Vittoria Corsa Speed (best racing tire) | ~11 W | ~0.0025 | 0.47 Nm |
| typical good racing tire | 13–17 W | 0.0035–0.0040 | 0.66–0.75 Nm |
| [Schwalbe Marathon Plus](https://www.bicyclerollingresistance.com/tour-reviews/schwalbe-marathon-plus-2015) | 25.5 W | 0.00764 | 1.44 Nm |
| **Continental Contact Urban — slowest tire envisaged** | **18.4 W** | **0.00552** | **1.04 Nm** |
| **Design limit `CRR_DESIGN_MAX`** | | **0.007** | **1.32 Nm** |
| [Schwalbe Hans Dampf TrailStar, slowest MTB](https://www.bicyclerollingresistance.com/mtb-reviews) — *not envisaged* | 42.2 W | ≈ 0.0127 | 2.40 Nm |

The MTB Crr is calculated by us, because the source only quotes watts there:
`42.2 W / 8.0 m/s / (42.5 · 9.81 N) = 0.0127`.

> Caution at higher loads: at 100 kg wheel load (cargo bike, e-bike) Crr 0.0127
> gives 5.64 Nm for the tire alone and thereby approaches the continuous torque of the motor.
> The design applies to 42.5–50 kg.

> **Anyone who does want to measure MTB tires** must not only raise `CRR_DESIGN_MAX`
> again but also lengthen `TORQUE_ARM_M` — otherwise the load cell runs past its full scale
> at Crr 0.0127 including starting torque. With the 5-kg cell the
> slowest MTB tire (2.91 Nm including baseline and wheel) at 0.084 m would already be at
> 71 %. The two values belong together.

## 7 Safety limits

| Constant | Value | Unit | Rationale |
|---|---|---|---|
| `MAX_FORCE_N` | 45 | N | 92 % of the full scale of the 5-kg cell → shutdown before saturation |
| `MAX_DRUM_RPM` | **derived** → 604 at U = 2.842 m (370 until 2026-11-25) | rpm | `(V_MAX_KMH + OVERSPEED_MARGIN_KMH) / 3.6 / U · 60`, i.e. 103 km/h; 100 km/h is 586 rpm, 60 km/h 352 rpm. Not hard-coded: the limit means *maximum speed at the drum surface* and follows the circumference — a fixed value would have tripped in the middle of the top plateau after a drum change. Raising `V_MAX_KMH` only after release per [07-safety](07-safety.md) §5. |
| `STALL_TIMEOUT_MS` | 2000 | ms | see §3. Until 2026-11-25 also the threshold at which `revsrc` fell back from the drum marks to the controller speed; since then marks that have counted once stay the source, and a failed mark sensor is caught by "no speed pulses". |
| `STALL_DUTY_MIN` | **0.20** | – | meas | **0.20 since 2026-11-23:** under 494 N wheel load the drum only breaks away at 0.139 with 8.3 A; 0.05 (determined without load) aborted every measurement run before. Duty cycle from which "driving, drum not turning" is a finding and not noise. On 2026-11-18, 0.045 duty cycle and 2.5 A motor current were present while the drum stood still — that is commutation, not load ([04-hardware](04-hardware.md) §11). Without separate drum marks this is the only way to detect standstill despite a setpoint. |
| `FAULT_SETTLE_MS` | 500 | ms | debouncing of the fault criteria |
| `STANDSTILL_KMH` | **1.0** | km/h | **Standstill, one number for everything** (decision of the builder, 2026-11-25): a FAULT clears itself, tare and acknowledgement are allowed, the relay brake pulls in again, a STOP ends, the start window opens, "driving but not turning", turning backwards. Until 2026-11-25 this was 0.05 1/s (0.5 km/h) here and five separate literals between 0.3 and 1 km/h in `speedctl`, `state`, `console` and `run`. |
| `STANDSTILL_RPS` | **derived** → 0.098 | 1/s | `STANDSTILL_KMH / 3.6 / U`, for `revsrc::revPerSec()` |
| `STANDSTILL_RPM` | **derived** → 5.9 | rpm | `STANDSTILL_RPS · 60`, for comparisons with the controller speed; times `MOTOR_POLE_PAIRS` for ERPM |
| `FAULT_CLEAR_STILL_MS` | 2000 | ms | standstill for this long, then a FAULT clears itself ([07-safety](07-safety.md) §4) |
| `ESTOP_DUTY_MIN` | 0.05 | – | est | Duty cycle from which the emergency-stop detection checks. Until the evening of 2026-11-23 it was tied to `STALL_DUTY_MIN`, and since that has stood at 0.20 it was blind below about 30 km/h (15 km/h: duty cycle 0.10; 28.8 km/h: 0.19). An emergency stop at low speed went undetected. A motor on the controller draws more than `ESTOP_IMOT_MAX_A` from 0.05 upward as soon as it delivers torque; idling is caught by the other two conditions. |
| `ESTOP_IMOT_MAX_A` | 0.3 | A | est | Motor current below which, with a duty cycle applied, **no motor is connected to the controller** (emergency stop tripped or disconnected). With the motor, the stationary drive drew 6.6 A at 0.11 duty cycle on 2026-11-23; after a tripped emergency stop from 60 km/h, Imot was at −0.2…+0.1 A while the sensors still reported speed for 25 s — hence no speed condition. |
| `ESTOP_SPEED_RATIO` | 0.6 | – | est | Emergency stop is only recognised when the speed lies below this fraction of the no-load speed for the duty cycle — together with the missing current. Idling at 28 km/h the current was 0.2 A but the speed at 100 % ([07-safety](07-safety.md) §4). |
| `ESTOP_DECEL_KMH_S` | 0.5 | km/h/s | est | second path: without current the drum decelerates faster than this and the setpoint is not falling — a connected motor would draw current immediately. After the emergency stop from 28 km/h about 3 km/h/s, the control loop oscillates with 0.1…0.2. |
| `ESTOP_DETECT_MS` | 1000 | ms | est | the finding must persist for this long |
| `ESTOP_FB_WIRED` | false | – | TBD | Auxiliary contact of the emergency stop on `PIN_ESTOP_FB` — not yet wired. Contact should pull to ground when tripped. |
| `LINK_TIMEOUT_MS` | **10000** | ms | **Dead-man switch.** 10 s since 2026-11-23, previously 2 s: the single-client web server hangs for up to 5 s on empty preconnections from iOS, and the page's polls wait that long — two runs aborted because of it. While a run is in progress the operator computer must check in; if it stays away longer, the run aborts. Same order of magnitude as `STALL_TIMEOUT_MS`. The web UI polls 5×/s, so ten polls may be missed. Does **not** apply as long as no client has ever been present — then the stand is driven via the console. [07-safety](07-safety.md) §4 |

The WiFi constants `WEBUI_*` are in [04-hardware](04-hardware.md) §13.1, because they
belong to the build and not to the physics.

## 8 Motor controller via UART

Only for the controller variant (`env:esp32s3-vesc`), see
[04-hardware](04-hardware.md) §14. The main firmware does not use them yet.

| Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|
| `VESC_BAUD` | 115200 | Bd | DS | Default of the VESC firmware |
| `VESC_POLL_MS` | 200 | ms | spec | Polling of the measured values. The interval **is** at the same time the averaging window: the controller delivers currents as the mean since the last poll and resets the accumulator on read. |
| `VESC_ALIVE_MS` | 150 | ms | spec | Repetition of the setpoint. The controller shuts off if nothing arrives for longer than `timeout_msec` (set there to 300 ms); 150 ms allows two dropouts. **Measured-value polls do not reset the timeout** — only setpoints and `COMM_ALIVE`. |
| `VESC_ERPM_MAX` | **derived** → 9065 (5295 until 2026-11-25) | ERPM | calc | `MAX_DRUM_RPM · MOTOR_POLE_PAIRS`. Since the pole-pair count is **measured** (§3), the cap follows directly from `V_MAX_KMH` and tracks it automatically. Previously this said 2400, derived from the controller's `si_motor_poles` = 14 on the assumption that this was the smallest plausible value — it was 30 poles. The drive is **not** the limiting factor: on the free motor 763 rpm at full modulation, 130 km/h at the drum; for 60 km/h a duty cycle of 0.46 suffices. No substitute for `MAX_DRUM_RPM`, but a cap ahead of it. |
| `VESC_ERPM_STEP_SMALL` | 100 | ERPM | spec | fine adjustment via ± |
| `VESC_ERPM_STEP_LARGE` | 1000 | ERPM | spec | coarse approach |
| `VESC_DIRECTION_SIGN` | **+1** | – | **meas** | Sign with which setpoints go to the controller so that the drum turns in the **measurement direction**. **+1 since 2026-11-23:** after rewiring, Hall or phase leads were placed differently, the Hall table was re-detected, and with the old −1 the drum ran backwards — seen at the stand. From 2026-11-17 until then −1, with the phase assignment checked at that time (relay block 1:1). **After every re-detection of the Hall table check the sign** (`e` turn by hand, or `e1` / `e-1` directly). Deliberately lives **here** and not as `m_invert_direction` in the controller: there the decision would not be versioned and would be lost on every re-detection. Two pitfalls: two swapped phases in the relay block reverse the direction **and** invalidate the Hall table (the motor then hums with current but without torque and reports **no** fault, [04-hardware](04-hardware.md) §10.6); and a change of direction while running is a **brake command** ([07-safety](07-safety.md) §3.3). |
| `VESC_BRAKE_CURRENT_STOP` | **20.0** | A | **meas** / spec | The **only** brake current. Decision of the builder: the fastest possible stop is always driven, never a slow one — regeneration is therefore not a selection criterion. Measured 50 → 5 km/h in **13.1 s**; 15 A would be hardly slower at 13.9 s and energy-neutral, but the requirement is fastest possible. Above 20 A the brake is saturated. **Only with the battery on the bus** — on the power supply, trip after 0.61 s with fault code 1, because the controller initially **regenerates** at high speed. Full curve in [07-safety](07-safety.md) §3.2b, the refuted counter-assumption in §3.2c. |
| `VESC_MAX_DUTY` | 0.95 | – | ctrl | `l_max_duty` of the controller (`hardware/vesc-config-fw7.01/mcconf.xml`); the firmware clamps its duty cycle there. |

## 8.2 Drum speed control

Applies to the **test-run variant** (`env:esp32s3-vesc`). The stand is driven via the
**duty cycle**, not via the VESC's speed controller: that one is tuned unusably for this motor,
and `s_pid_min_erpm` = 900 locks out the lowest test step
(10 km/h is 881 ERPM) — [06-open-questions](06-open-questions.md) A15.

The feedforward comes from `MOTOR_NO_LOAD_RPM_PER_V` (§4.1) times the **measured**
bus voltage, on top of which sits a slow trim. This is how the stand was driven to
99.2 km/h on 2026-11-17.

| Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|
| `SPEEDCTL_RAMP_KMH_PER_S` | 2.0 | km/h/s | spec | Ramp of the inner setpoint: 30 s from 0 to 60. Slow enough that a mistake is noticed before it gets fast. |
| `SPEEDCTL_KI` | 0.008 | 1/(km/h·s) | est | I term of the trim; does not integrate while the duty cycle is at the clamp (anti-windup). Until 2026-11-23 the trim was a pure integrator with 0.02: under 494 N wheel load the drum oscillated by ±0.8 km/h with a period of about 10 s. |
| `SPEEDCTL_KPP` | 0.008 | 1/(km/h) | est | P term on the deviation. |
| `SPEEDCTL_KD` | 0.015 | 1/(km/h/s) | est | D term on the acceleration **beyond the ramp** (`revsrc::dvdtKmhS` minus ramp slope). A D term against the intended acceleration only increased the lag: 2.1 km/h overshoot at 0.03. |
| `SPEEDCTL_LEAD_KMH` | 1.5 | km/h | est | The ramp runs ahead of the drum by at most this much. Otherwise the trim integrates against a large deviation during the whole run-up and arrives at the target with a surplus. Tuning 2026-11-23: 2.0 → 1.2–1.5 km/h overshoot; 1.0 → none, but 40 km/h only after 42 s; **1.5 → 0.8–1.0 km/h, target after 17–22 s, then ±0.2.** |
| `SPEEDCTL_APPROACH_KMH` | 4.0 | km/h | est | Ramp flattens over the last 4 km/h … |
| `SPEEDCTL_APPROACH_MIN` | 0.25 | – | est | … down to a quarter of the slope. |
| `BRAKE_MAX_KMH` | 45 | km/h | est | **With the power supply on the bus:** above this there is no braking but release until the drum has coasted below it. Entering braking from motion in FOC always crosses the regeneration zone (source code FW 7.01, [07-safety](07-safety.md) §3.2d); the surge grows roughly with the square of the speed. Measured 2026-11-23: entry at 54 km/h clean, at 56/58/59 km/h fault 1 (overvoltage). 45 leaves margin. Raise only in 2-km/h steps with an eye on `vin`. |
| `RUN_POINTS_PER_PLATEAU` | 3 | – | spec | Guided measurement run (`run.h`, [11-review](11-review.md) R4): three points of `AVG_REVS_PER_POINT` revolutions each per plateau, from which mean and scatter. Costs about a minute per run. |
| `CAL_AVG_MS` | 5000 | ms | est | Calibration procedure (calib.h, direct at the cell since 2026-11-28): averaging time per reading at rest. |
| `CAL_MAX_POINTS` | 16 | – | est | Calibration procedure: most readings (zero and loads) per calibration. |
| `CAL_MASSES_G` | 200, 500, 1000, 2000 | g | spec | Calibration procedure: the masses offered in the UI, hung one after the other in the stirrup on the Bowden cable with the arm lifted (builder 2026-11-28). 1000 g certified, 2000, 500 and 200 g nominal until weighed. Console `Cm<kg>` takes any mass. 2000 g = 19.6 N = 40 % FS. |
| `RUN_STOP_MIN_PCT` | 100 | % | spec | End-stop check in the guided run (step 5; the check weight before it was dropped 2026-11-28): the arm is pushed by hand against the stop; the peak of the cell force in % of full scale must lie above this value, otherwise the stop limits the measuring range ([08-stand](08-stand.md) §7.5). |
| `MEAS_TORQUE_MIN_NM` | 0.5 | Nm | calc | Lower end of the measurement signal (tire plus idle losses), [09-measurement-theory](09-measurement-theory.md) §5. Green band of the utilization scale under Manual, in % via torque arm and full scale. |
| `MEAS_TORQUE_MAX_NM` | 1.4 | Nm | calc | Upper end of the same signal range. |
| `RUN_STOP_MAX_PCT` | 140 | % | spec | … and below this value, otherwise the stop does not protect the cell (150 % is the cell's overload limit). Outside: W16, skipped: W17. |
| `RUN_STOP_BAR_PCT` | 160 | % | spec | End of the live bar scale in the web UI. |
| `RUN_BASELINE_POINTS` | 2 | – | est | Idle-loss points per speed in pre- and post-run; W08 compares their means, the protocol shows the spread within each. Run 2026-11-26-1542 had one each, so W08 compared single values within the noise of one point. |
| `RUN_DRIFT_LIMIT_N` | 0.02 | N | calc | Pre-run and post-run baseline of the idle-loss curve, on F_roll, [09-measurement-theory](09-measurement-theory.md) §8.8. Above this W08 "discard series". |
| `RUN_ANCHOR_WARN_N` | 0.03 | N | est | Reference point (R6): since 2026-11-24 `WARMUP_KMH` is driven **right after the warm-up and after the plateaus**; the difference is the drift of the total torque over the whole measurement run, on F_roll. Above this W04. Somewhat wider than the baseline criterion, because the tire drifts along. |
| `RUN_ZERO_LIMIT_N` | 0.25 | N | est | Two zero-point checks on F_roll ([11-review](11-review.md) K2, W6): after zeroing, turn the drum a quarter revolution further and read again (W13 if above: something is holding the rotor against the cogging torque); after the spin-down with the wheel lifted the cell must show the offset again (W14: drift of cell, cable, preload). Equal to the drift criterion. Deliberately wider than the drift criterion: at standstill the static friction of the pillow blocks holds the rotor against the cogging torque, and that much cogging (0.01 to 0.02 Nm, 0.02 to 0.05 N on F_roll) stays as an angle-dependent offset; it cancels in Crr (the tare is in baseline and measurement alike) and averages out while turning. The standstill checks catch gross faults (cable touching, arm at the stop); the fine check is pre-run versus post-run baseline. On 2026-11-24 the quarter-turn check gave +0.05 and +0.10 N: motor bearings (between rotor and stator) and pillow blocks together hold about 0.05 Nm of cogging torque, i.e. 0.1 N on F_roll, the natural blur of a standstill zero on this machine; above 0.25 N something is holding the arm. |
| `RUN_ZERO_AVG_MS` | 2000 | ms | est | Averaging time of the two zero-point checks, 160 cell values. |
| `UNC_K_REL` | 0.002 | – | est | Type B share of the calibration factor for u(Crr) (R9): weights, averaging, linearity (the calibration reports linearity in % of load; replace the estimate once a few calibrations exist). |
| `UNC_ARM_REL` | 0.0012 | – | est | Cell arm `TORQUE_ARM_M`: 0.1 mm on 84 mm. Since the direct calibration (2026-11-28) the arm enters every torque; replaces `UNC_LCAL_REL` (0.001 on the calibration arm, dropped with it). The run CSV writes it as `unc_arm=` (before: `unc_lcal=`). |
| `UNC_N_REL` | 0.005 | – | est | Wheel load: hanging scale on the gantry, position of the arm while weighing. |
| `UNC_P_REL` | 0.010 | – | est | Tire pressure: gauge ±0.1 bar, or the open offset of the AirSpy, at about 8 %/bar Crr sensitivity around 6 bar. The pressure rise during warm-up is measured and recorded in the report and is no longer a budget item. |
| `UNC_BEARING_REL` | 0.0065 | – | calc | Load-dependent share of the drum bearings, 4.7 mNm at 417 N ([06-open-questions](06-open-questions.md) A11), not corrected. |
| `UNC_WHEEL_REL` | 0.005 | – | calc | Wheel model with **measured** J_wheel (bifilar ±1.5 %) → 0.52 % of M_tire ([09-measurement-theory](09-measurement-theory.md) §3.2). Applies when `Y<kgm2>` is set. |
| `UNC_WHEEL_EST_REL` | 0.035 | – | calc | Wheel model with **estimated** J_wheel (`WHEEL_J_FACTOR`, ±10 %): 10 % of 0.25 Nm wheel loss is 3.5 % of the racing-tire signal. Applies as long as no measured J is entered; the report states it. |
| `RELAY_BRAKE_WIRED` | **true** | – | spec | Relay brake driven by the firmware ([04-hardware](04-hardware.md) §9.4). true since the rebuild of 2026-11-25 (coil chain, emergency stop, Q1 per §9.1). **Every stop** goes via the relays, at any speed (decision of the builder, evening of 2026-11-25, after the brake comparison: controller 20 A and relay short decelerate identically at 2.8 km/h/s). Until then a threshold `RELAY_BRAKE_ABOVE_KMH` = 40 km/h existed, below which the controller braked. With false a stop brakes via the controller below `BRAKE_MAX_KMH` and coasts above. |
| `RELAY_DRIVER_OFF_MS` | 100 | ms | est | controller released → relays drop (N2: driver first, then relays; the controller reset on 2026-11-17 when the phases opened under current). One UART command and one control cycle are milliseconds; 100 is margin. |
| `RELAY_SETTLE_MS` | 100 | ms | est | after the relays pull in, no drive command for this long (contact bounce) |
| `RELAYTEST_WIN_MS` | 2000 | ms | est | Relay pulse test (console `X<n>`): quiet window and window after the brake command; covers drop and pull-in of one cycle. |
| `RELAY_RELEASE_STILL_MS` | 1000 | ms | est | drum at standstill (`STANDSTILL_KMH`) for this long, then the relays pull in again and the stand is ready; the spin-down test and the hand checks need the motor free |
| `BRAKE_FAULT_HOLD_MS` | 3000 | ms | est | After a controller fault, do not brake again for this long but release. The controller itself discards all commands for 500 ms (`m_fault_stop_time_ms`); without its own hold time the firmware immediately retried afterwards, and every restart at almost the same speed faulted again (tack-tack-tack). |
| `OVERSPEED_MARGIN_KMH` | 3 | km/h | spec | Headroom of the speed limit above `V_MAX_KMH`: the top plateau lies at V_MAX, 1 km/h overshoot aborted the baseline run there. |
| `SPEEDCTL_COAST_ABOVE_KMH` | 2.0 | km/h | est | **Slowing down never brakes via the controller** (decision of the builder, 2026-11-25). If the actual value is more than 2 km/h above the setpoint, the controller is released; then either the relays brake once (`DESCENT_*`) or the drum only coasts … Until 2026-11-25 named `SPEEDCTL_BRAKE_ABOVE_KMH`, and the controller braked with `VESC_BRAKE_CURRENT_STOP` below `BRAKE_MAX_KMH`. |
| `SPEEDCTL_COAST_UNTIL_KMH` | 0.5 | km/h | est | … until 0.5 km/h above the setpoint, then the controller takes over at the actual speed. Lowering the duty cycle in FOC is regeneration into the bus: 43.8 V, fault 1 on the step 40 → 15 km/h on 2026-11-23. A relay descent was tried and dropped on 2026-11-25 ([04-hardware](04-hardware.md) §9.4). |
| `DESCENT_MIN_COAST_S` | 5 | s | est | **Descent within a run, step 1: is braking worth it?** Coasting time to the setpoint from the loss model (stored idle model, otherwise `DRUM_LOSS_*`, plus the wheel model) and `DRUM_INERTIA_KGM2`. Shorter: coast only. At most **one** relay braking per setpoint, and only with counting drum marks: the controller reports 0 rpm with the motor shorted (cause of the 26-fold chopping on 2026-11-25). |
| `DESCENT_RELEASE_LATENCY_S` | 0.15 | s | est | Step 3: release the relays at setpoint + deceleration × this latency + `DESCENT_COAST_REMAIN_KMH`. Loop 20 ms, contact travel, lag of the deceleration measurement. |
| `DESCENT_COAST_REMAIN_KMH` | 1.5 | km/h | est | Step 4: coasted on purpose after releasing, about 3 s, so the target is approached without braking past it. |
| `DESCENT_HANDOVER_MS` | 2000 | ms | est | Only this long after the relays pulled in, coasting and handover use the drum marks; otherwise the control runs on the controller speed. The mark speed is updated once per revolution: used as the actual value it held the ramp back, and the run-up to 100 km/h took 100 instead of 75 s (2026-11-25). The controller lagged 0.5 s after release. |
| `DESCENT_SYNC_TOL_KMH` | 1.0 | km/h | est | Handover after a descent only when the controller speed agrees with the drum marks this closely. On 2026-11-25 the controller speed lagged after the relays pulled in (63 instead of 82 km/h), the control anchored there, regenerated (45.3 V) and threw overvoltage (fault 1, 46.9 V) at the next release. Coasting and handover use the drum marks since, for `DESCENT_HANDOVER_MS`. |
| `DESCENT_BRAKE_DECEL_KMH_S` | 2.8 | km/h/s | meas | Relay brake deceleration, 30 → 10 km/h from 35.9 km/h on 2026-11-25. Used as long as the own measurement shows less (the 1 s window lags at the start of braking). |
| `SPINDOWN_MIN_KMH` | 5 | km/h | est | Wheel spin-down ends below this wheel speed; to standstill it took minutes, and the slow revolutions contribute hardly anything. |
| `SPINDOWN_RESID_MAX_RADS` | 0.5 | rad/s | est | A spin-down fit with a larger residual is rejected (W19). Run 2026-11-26-1542 replayed: 0.043 rad/s clean, 32.8 rad/s with one phantom pulse. |
| `STEADY_DVDT_KMH_S` | 0.3 | km/h/s | est | **Live measurement only at constancy:** actual value within `STEADY_TOL_KMH` = 1 km/h of the setpoint, ramp arrived, and that for `STEADY_HOLD_MS` = 2000 ms; then the **net** acceleration over the averaging window (slope of the regression line, as for a measurement point) must stay below this. Until 2026-11-24 the instantaneous dv/dt was gated and every excursion cleared the window: the speed computed from the controller jitters by ±0.1 km/h between two polls at 30 km/h, so the gate tripped all the time and the warm-up never showed power or Crr. While accelerating J·dω/dt sits in the torque, while settling it oscillates. |
| `LIVE_WINDOW_MS` | 5000 | ms | est | moving average of the live measurement, up to 400 cell values; only values from the last 5 s count. |
| `LIVE_MIN_MS` | 1500 | ms | est | the window must span at least this before a live value appears. A fixed count would be chance: with a browser polling, the main loop sees only 20 to 40 of the 80 cell values per second. |
| `WHEEL_J_FACTOR` | 0.85 | – | lit | **Estimate** J_wheel = 0.85 · m · r_wheel², [08-stand](08-stand.md) §6: rim, tire and tube almost all sit at the same radius; ±10 %. Mass via `y<kg>`, r_wheel from the measured circumference. An error on J goes 1:1 into the wheel-loss model (A₁ and B₁ both), which is up to 35 % of the racing-tire signal at 60 km/h. Enter **measured J** (bifilar pendulum, [09-measurement-theory](09-measurement-theory.md) §9.2) via `Y<kgm2>` or *Setup → J_wheel measured*; it takes precedence, and the uncertainty drops from `UNC_WHEEL_EST_REL` to `UNC_WHEEL_REL`. |
| `SPINDOWN_MIN_REVS` | 30 | – | est | fewer wheel revolutions in the spin-down are no fit. |
| `WHEEL_MASS_DEFAULT_KG` | 1.5 | kg | spec | Default of the wheel mass with tire; weigh at the stand and enter under *Setup*. |
| `WIZ_KMH` | 40 | km/h | spec | Wizard spin-down + baseline: speed at which the wheel is lifted off — high enough for a long spin-down, still liftable by hand. |
| `WIZ_STEADY_MS` | 3000 | ms | est | steady at `WIZ_KMH` for this long before lifting is allowed; the wheel-circumference window (10 wheel revolutions) fills in that time |
| `WIZ_LIFT_CONFIRM_MS` | 500 | ms | est | wheel/drum ratio below `WIZ_SLIP_RATIO` for this long = lifted; shorter than a bounce of the eccentric lever, longer than one wheel revolution |
| `WIZ_SLIP_RATIO` | 0.90 | – | est | if the ratio of wheel to drum speed falls below 90 % of the value with the wheel resting, it counts as lifted off. |
| `SPEEDCTL_DUTY_UP_PER_S` | 0.30 | 1/s | spec | Maximum change of the duty cycle **upward**. |
| `SPEEDCTL_DUTY_DOWN_PER_S` | 0.06 | 1/s | spec | …and **downward**, a fifth of that. **The asymmetry is the core:** stepping down is a brake command. A duty cycle below what the current speed needs makes the controller regenerate — on the power supply that ends in overvoltage ([07-safety](07-safety.md) §3.3). |
| `SPEEDCTL_START_DUTY` | 0.16 | – | meas | Starting window: while the drum is stationary the duty cycle may go up to here, independent of the ramp. Breakaway under 494 N at 0.139 / 8.3 A (2026-11-23); without the window it stood under current for 10 s. |
| `SPEEDCTL_CLAMP_FACTOR` | 1.08 | – | spec | Clamp against runaway of the duty cycle, as a multiple of the idle-loss curve of the **setpoint**. |
| `SPEEDCTL_CLAMP_OFFSET` | **0.08** | – | meas | **0.08 since 2026-11-23:** with 494 N wheel load the clamp at setpoint 15 stood at 0.139, the drum stayed at 11.9 km/h — the voltage drop under load was missing. Overshoot at 60 km/h not yet checked with this. Load allowance of the same clamp. Without this clamp the trim integrates up to the stop as long as the current limit determines the acceleration — and the drum shoots past the setpoint as soon as the current is freed. Happened exactly like that on 2026-11-17: duty cycle 0.95 at an actual 0.26. |
| `SPEEDCTL_PERIOD_MS` | 20 | ms | spec | Period of the control loop. Faster gains nothing — the speed arrives with `VESC_POLL_MS` = 200 ms. |
| `CMD_MAX_AGE_MS` | 2000 | ms | **safety limit** | Maximum age of a command from the web UI. Every command carries the timestamp the web UI last saw; anything older is rejected with **409**. **STOP and coast are exempt** — a late STOP is harmless, a discarded one is not. Reason: on 2026-11-18, during page load, an old `v20` arrived again as a browser retry and set the drive in motion ([07-safety](07-safety.md) §6). |
| `BREAKAWAY_RATE_PER_S` | 0.01 | 1/s | spec | Rise of the duty cycle in the breakaway measurement. Slow enough that the moment of breakaway is readable. |
| `BREAKAWAY_DUTY_MAX` | 0.15 | – | spec | Upper limit of the measurement. If it has not broken away by then, the measurement is aborted and **no** value is reported — rather than inventing one. |
| `BREAKAWAY_DETECT_KMH` | 0.5 | km/h | spec | From here the drum counts as moving, the measurement ends immediately. |
| `DIRDETECT_MIN_N` | 1.0 | N | est | Direction detection (Setup → Drive direction, since 2026-11-26): change at the load cell during the slow start that counts as a clear sign; positive keeps the sign, negative reverses it, smaller leaves it unchanged. Far above the tare noise (about 0.0003 N), well below the 5.6 N at the cell that the measured idle constant 0.472 Nm means. |
| `SPEEDCTL_LEARN_MIN_KMH` | 2.0 | km/h | spec | From this speed the learning of the direction of rotation counts as unambiguous. Below it the sign could come from noise. |
| `SPEEDCTL_LEARN_TIMEOUT_MS` | 20000 | ms | spec | If nothing turns within this time, learning aborts instead of silently waiting. |
| `MOTOR_TEMP_OPEN_C` | **−20** | °C | **meas** | Below this the motor sensor is **open**, not cold. Measured 2026-11-17: open terminal −75.5 °C, connected 19.4 °C ([06-open-questions](06-open-questions.md) A5). On 2026-11-18 an unplugged sensor lead showed −82 °C. Because TMP and the three Hall wires are in the **same** connector, the open sensor is at the same time the usable indicator for the Hall sensors — the controller does not deliver the raw Hall states. |
| `SPEED_STEP_KMH` | 5 | km/h | spec | Step size of the control buttons. The grid runs from `V_MIN_KMH` to 100; steps above `V_MAX_KMH` are shown **locked** in the web UI, because the release per [07-safety](07-safety.md) §5 is missing. |

> **Three of these values are not there by taste.** Presetting the duty cycle
> from the measured speed, asymmetric rate of change and the clamp are each
> the answer to a fault that happened on 2026-11-17 on the running build.
> The rationales are given in full in `firmware/src/speedctl.h`.

## 9 Tire pressure sensor SKS AirSpy (BLE)

Protocol from the sister project `~/git/airspy`, there `docs/20-protocol.md`, measured against the
sensor (CONFIRMED, 2026-10-13). At the stand **exactly one** sensor hangs on the valve;
the first `AIRSPY-*` found is connected (`firmware/src/tirepressure.cpp`, separate
task on core 0, because the NimBLE calls block and the main loop has to serve the
controller every 150 ms).

| Constant | Value | Origin | Note |
|---|---|---|---|
| `AIRSPY_NAME_PREFIX` | `AIRSPY` | meas | Local name `AIRSPY-<Seriennummer>`; is in the scan response, so scan **actively**. No pressure data in the advertisement. |
| `AIRSPY_SVC_UUID16` | 0xFFF0 | meas | Service of the data stream. |
| `AIRSPY_CHR_UUID16` | 0xFFF4 | meas | Characteristic, notify; streams unsolicited at 2 Hz, no handshake, no pairing. |
| `AIRSPY_TAG_PRESSURE` | 0x2E | meas | Byte 14 of the first 15-byte notification; before it float32 (little endian, offset 8) in **kPa**. |
| `AIRSPY_TAG_TEMP` | 0x22 | meas | Second notification, byte 6; float32 at offset 0 in °C. |
| `AIRSPY_TAG_BATTERY` | 0x1E | meas | Second notification, byte 13; float32 at offset 7 in %. |
| `AIRSPY_AVG_N` | 1 | spec | 1 = the current value, no averaging (decision 2026-11-24: the pressure hardly varies, a fresh value is worth more than a smoothed one). Larger values give a moving average over that many samples; individual values scatter by 4.7 kPa (quantisation 16.7 hPa). |
| `AIRSPY_STALE_MS` | 10000 | est | Older counts as "stale": the sensor sleeps about 10 min after the last movement and then sends nothing. Turn the wheel once before the run. |
| `AIRSPY_CONN_ITVL_MIN` | 320 | spec | Connection interval, lower limit, in 1.25-ms units (400 ms). With the NimBLE defaults (30 to 50 ms) the coin cell in the sister project was empty in a day. A request to the sensor, not a mandate. |
| `AIRSPY_CONN_ITVL_MAX` | 400 | spec | … upper limit (500 ms). |
| `AIRSPY_MUTE_MS` | 6 · `AIRSPY_STALE_MS` = 60000 | est | Connected but silent this long: disconnect and scan again (the sensor sleeps, the link is dead). |
| `AIRSPY_SEEN_MS` | 3000 | est | Connect only if the sensor was heard this recently; then it answers at once. |
| `AIRSPY_RETRY_MS` | 2000 | est | Pause after a failed connection attempt. |
| `AIRSPY_POLL_MS` | 500 | est | Period of the BLE task while connected. |
| `AIRSPY_CONNECT_TIMEOUT_MS` | 5000 | est | Connection attempt only if the sensor was heard in the last 3 s — then it answers in under a second. |

**[INFERRED]** The pressure is gauge pressure (0.2 to 0.8 bar on a tube that was let bleed),
the absolute offset is open in the sister project (§3.4 there). That is why
`UNC_P_REL` stays in the budget and the gauge on the pump head remains the reference when
setting the pressure. The temperature is that of the sensor in the rim bed, i.e. the air in the tire.

## 8.1 Status LED

The RGB LED of the DevKit is an addressable **WS2812**, not a simple output; it is
driven via `neopixelWrite()`, which internally uses RMT.

| Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|
| `PIN_STATUS_LED` | 48 | – | **meas** | Confirmed on the installed board (2026-11-15). On DevKitC-1 v1.1 the LED would sit at GPIO 38 — this board has it at 48, as the Arduino core for the S3 also assumes (`RGB_BUILTIN`). |
| `STATUS_LED_DIV` | 6 | – | spec | Brightness divider. At full drive the LED is blinding on the desk. |
| `STATUS_BLINK_MS` | 1200 | ms | spec | Half period of the slow **blue** blinking while waiting for the operator connection (until 2026-11-26: 700 ms red). **Steady green** = an operator computer is checking in. That makes the dead-man switch readable from three metres. |
| `STATUS_REC_BLINK_MS` | 300 | ms | spec | Half period of the fast **red** blinking while raw data are recorded; without an operator computer red alternates with blue. Red means nothing else. |

Driving both pins one after the other does **not** work: `neopixelWrite()` initialises RMT
once for the pin of the first call and does not switch afterwards. Hence one
pin, and when in doubt switch over instead of trying both.

## 10 Storage and raw data

As of 2026-11-27. Firmware `storage.cpp`, `rawlog.cpp`; reasoning in
[10-firmware](10-firmware.md) §11, wiring in [04-hardware](04-hardware.md) §15.

| Constant | Value | Unit | Origin | Note |
|---|---|---|---|---|
| `SD_SPI_KHZ` | 10000 | kHz | spec | SPI clock of the microSD. Flying wires to the adapter; 20 MHz (IDF default) is not attempted. Halve on read errors. Set explicitly after the card init: the driver alone fell back to 400 kHz below 20 MHz (until 2026-11-26, about 45 kB/s; at 10 MHz about 1 MB/s read). |
| `STORAGE_CHECK_MS` | 2000 | ms | spec | Poll for card inserted/removed (no card-detect contact). |
| `STORAGE_STATS_MS` | 10000 | ms | spec | Recompute the usage. LittleFS walks all blocks for it, so only at standstill (§11 of 10-firmware). |
| `RAWLOG_BUF_BYTES` | 1048576 | B | spec | Ring in PSRAM (1 of 8 MB) between main loop and storage task. At the estimated 2.5–3.5 kB/s of the CSV stream (since 2026-11-27, [10-firmware](10-firmware.md) §11.3): roughly 5 min. Only bridges pauses of the SD card; raw data goes to the SD only since 2026-11-26. |
| `RAWLOG_CHUNK_BYTES` | 4096 | B | spec | Write unit: eight SD sectors. |
| `RAWLOG_SYNC_MS` | 1000 | ms | spec | Interval of the `S` (sync) line, wall clock against `t_us`; the `C` (control) line is repeated with it. |
| `RAWLOG_FLUSH_MS` | 2000 | ms | spec | File is committed this often; at most this much is lost on power failure. |
| `RAWLOG_STOP_PCT` | 95 | % | spec | Recording stops at this usage of the medium. |
| `RAWLOG_EDGE_RING` | 256 | edges | spec | Ring between the Hall ISRs and the main loop. Power of two. At the highest edge rate (motor Hall plus drum marks, well below 1000/s) it bridges more than 0.2 s of main-loop stall. |
| `RAWLOG_FLASH_IDLE_MS` | 3000 | ms | spec | Flash usage is computed and the flash formatted only after no sensor edge for this long and drive off. A flash erase or write locks cache and all non-IRAM interrupts on both cores; raw data therefore never goes to the flash. |

