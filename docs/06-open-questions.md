# Open Questions

Sorted by blocking effect. As of **2026-11-17**.

## A — Blocks the build

### A1 Motor connector pinout
The installed motor is the hoverboard wheel: **3 blade connectors (yellow/green/blue) =
phases, plus a white JST with 6 thin wires** (white, black, green, blue, yellow, red).

Wire colors, the inferred pinout and the continuation to the motor controller are in
[04-hardware](04-hardware.md) §11. What remains open is the **verification with the
meter** — procedure in §11.3. A swapped Hall supply destroys the sensors.

Also open: how the connection to the motor controller is made — the order of the two
6-pin connectors does not match (§11.2):

1. **Pull the contacts out of the cavities and re-sort them** ([04-hardware](04-hardware.md)
   §11.5) — the shortest route, but it requires both connectors to be the same system.
2. Procure a mating connector.
3. Cut off and fit our own connectors.

Which of these works is decided by measuring pitch and contact type on both connectors.

### A5 Temperature sensor: determine type
The sixth wire of the Hall connector is **white** and, on the opened motor, does not run to
the Hall board but through a fiberglass sleeve into the winding pack
([04-hardware](04-hardware.md) §11.1). That is where a sensor would sit, but it proves
neither type nor characteristic.

**To do:** measure resistance white against black and watch it while warming — measurement
procedure with interpretation table in [04-hardware](04-hardware.md) §11.3.

**Where the wire goes is decided:** to `TMP` of the **motor controller**, not to the
ESP32 — overtemperature must lock out the driver, and the driver is the motor controller
([04-hardware](04-hardware.md) §11.4). `PIN_MOTOR_PTC` has been removed from `config.h`.

**Sensor type answered, 2026-11-17: 10 k NTC in the winding pack.** Confirmed three
ways, each logged via the motor controller's telemetry:

1. **Open terminal** reports −75.5 °C, connected **19.4 °C** — an NTC on a pull-up to 3V3
   behaves exactly like this. A short circuit or a PTC would give the opposite sign.
2. **Plausibility against the on-board FET sensor**, which is known good: 1.6 K apart at
   room temperature. A wrong characteristic would be off by double digits.
3. **Hot air on the rotor**: motor temperature rose monotonically 19.4 → 19.6 °C while
   the FET reference stayed flat at 17.7 °C. An open input is noisy; it does not follow.

In the motor controller, `m_motor_temp_sens_type` was already set to **NTC 10K at 25 °C** —
so the setting happened to be right and stays.

> The sensor sits in the **stator**. Warming from outside heats the **rotor**, and the
> motor's mass delays everything. A 0.2 K rise over minutes is therefore not a weak
> signal but the expected one.

What remains open is only:

1. **Set the limit** and enter it in [07-safety](07-safety.md) §4.
   Proposal: warning 80 °C, shutdown 100 °C — the hoverboard motor is built for continuous
   operation at low load, not for 20 A over minutes.

### A7 Natural frequency of the balance — **measure, don't weigh**
What is needed is not the mass of the stator but the **moment of inertia** — and even that
only as an intermediate step to the natural frequency:

```
f_n = (1/2π)·√(k_rot/J)      k_rot = k_cell · L² = 523 Nm/rad
```

| J_stator | f_n | Position relative to the orders at 60 km/h |
|---|---|---|
| 0.007 kg·m² | 43.5 Hz | above all |
| 0.026 kg·m² | **22.6 Hz** | between 3rd (17.6) and 4th (23.5) |

Weighing helps little here: mass alone gives no J, the radius of gyration would be a guess.
**Better to measure directly:** tap the lever arm and record the ring-down frequency. That
captures everything — cell, tension link, clamping — instead of modelling it.

Not with the HX711: 80 SPS only reach up to 40 Hz. Instead, stick a phone with an
accelerometer and FFT app on the arm, tap it, read the peak.

**What depends on it:** not the measured value. The measurement is torque-driven; a
resonance only enlarges the amplitude around the mean and drops out under synchronous
averaging ([09-measurement-theory](09-measurement-theory.md) §3.5). It is only dangerous
if the arm runs into the stops or if it is excited above 40 Hz and then aliases into the
sampling (§3.7).

### A2 Permissible maximum drum speed
Depends on the wooden construction, hub attachment and balance quality. Decides whether
anything beyond 60 km/h is possible at all. **This is the hardest constraint in the whole
concept** — not the electronics. See [07-safety](07-safety.md).

### A11 Reference run with a rigid roller — **deferred, probably unnecessary**
The load-dependent bearing friction is **not an estimate** but computed from the friction
model for deep-groove ball bearings:

```
M = 0.5 · µ · F · d = 0.5 · 0.0015 · 417 N · 0.015 m = 4.7 mNm
```

That is **0.39 %** of the signal of a good racing tire. Even ±50 % uncertainty on µ leaves
±0.2 % of that — smaller than anything else in the budget. For A/B comparisons the share
drops out completely anyway (same load, same bearings).

If it is to be measured after all: **wheel diameter is mandatory**, not optional. A small
roller amplifies its own bearing friction by `r_drum/r_roller` — for a 30 mm roller by a
factor of 30. A bare aluminum rim directly on the tread plate is ruled out: line contact at
417 N dents both. What would be needed is a rim with a **POM or PU running surface** in
tire size, in the existing fork. That much effort and money against 0.2 % — not worth it.

### A14 Which drum applies? — **done 2026-11-23/24**

Measured with a tape around the finished surface: **U = 2.842 m** (Ø 904.6 mm),
J = 1.94 kg·m² computed. On 2026-11-24 all documents were brought to r = 0.4523 m;
the values for Ø 933 (2.931 m, 2.07 kg·m²) are struck. History:

The drum circumference stood in four places, and they did not agree:

| Source | Circumference | Diameter | J |
|---|---|---|---|
| `firmware/src/config.h` | **2.837 m** | Ø 903 | 1.94 kg·m² |
| Knowledge base `20_pruefstand` | **2.837 m** | Ø 900 bare, Ø 903 with 1.5 mm tread plate | 1.94 kg·m² |
| `docs/03-parameters.md` §1 | **2.931 m** | Ø 933 | 2.07 kg·m² |
| `README.md` | **2.93 m** | Ø 933 | — |

This retroactively explains the deviation in `MAX_DRUM_RPM` noted for weeks:
**353** in `config.h` follows from 2.837, **341** in [07-safety](07-safety.md) §4
from 2.931. It was never an arithmetic error but **two drums in one project**.

Code and knowledge base agreed on 2.837; the two repo documents were stuck on the old
value. The measurement decided.

### A15 The VESC speed controller is uselessly tuned

Measured 2026-11-17: setpoint 900 ERPM → 27 ERPM, 1500 → 150, 2400 → 780. The controller
stalls at a third and draws 0.3 A, i.e. just the bearing friction.

> **Correction of origin, same day.** This first said the gains were inherited from the
> skateboard configuration found on the device. That is wrong: after flashing firmware
> 7.01 the same four values were back. They are **factory defaults of the VESC
> firmware**. It changes nothing about the matter — they are useless for this motor — but
> it means there is nothing to "clean up" here; they have to be **set for the first
> time**.

| | Value |
|---|---|
| `s_pid_kp` | 0.004 |
| `s_pid_ki` | 0.004 |
| `s_pid_kd` | 0.0001 |
| `s_pid_min_erpm` | 900 |

The P term only reaches as far as the friction, and the I term is practically ineffective
at VESC's internal scaling of 10⁻⁴.

**`s_pid_min_erpm` = 900 weighs heavier.** The lowest test step from `config.h` is
10 km/h = 58.8 rpm = **881 ERPM** — just *below* the threshold from which the controller
regulates at all. Below it, it forces duty cycle 0. The lowest step of the test procedure
would thus not be drivable.

To do: tune the gains **against the mounted drum** (values tuned unloaded would be thrown
away; the loop has to work against 1.94 kg·m² and its friction) and bring
`s_pid_min_erpm` down far enough that 881 ERPM is regulated reliably. Working range is
**881 ERPM (10 km/h) to 5287 ERPM (60 km/h)**.

As a fallback, the stand runs via **duty cycle** — tested on 2026-11-17 up to 99.2 km/h,
following the idle-loss curve strictly linearly. For a measurement at steady speed that is
not sufficient, though; that needs the closed loop.

## B — Blocks the firmware

### B3 LCD connection and logic level
I²C backpack (PCF8574) is set; it runs at 5 V. Three routes to the 3.3 V level stand side
by side in [04-hardware](04-hardware.md) §5.6:

| | |
|---|---|
| a) level shifter | in the specification, one more component |
| b) unsolder the 4.7 kΩ pull-ups from the backpack and pull to 3.3 V | least effort, but below the 0.7 · V_DD = 3.5 V the PCF8574 requires as high |
| c) the LCD itself at 3.3 V | the voltage regulator on the backpack is not populated; contrast at 3.3 V to be checked |

Also open: I²C address (0x27 or 0x3F)?

### B4 Tire-pressure sensor — **done 2026-11-24: SKS AirSpy**
The BLE protocol is decoded in the sister project `~/git/airspy` (service 0xFFF0,
characteristic 0xFFF4, float32 kPa at offset 8, marker 0x2E). `tirepressure.cpp`
connects to the first `AIRSPY-*` found in its own task, shows pressure, temperature and
battery on the measurement page and writes both per measurement point into the log.
Constants in [03-parameters](03-parameters.md) §9. What remains open is the sensor's
absolute pressure offset (there §3.4); pressure is still set by gauge, the sensor delivers
the change during the run. **Not yet tested at the stand with a real sensor.**

## C — Methodological

### C1 Drum curvature — **halved, surface done**
> **Surface: no longer an issue.** The drum gets aluminum **tread plate**, i.e. the same
> kind of surface as the reference test rig. The previously feared penalty from smooth
> wood is gone.
>
> **2026-11-25:** the tread plate now waves between the screws, see [E2](#e2-tread-plate-on-the-drum-waves--surface-no-longer-round).
> The surface is open again.

What remains is the **curvature**. The tire is deformed more on a drum than on a flat
surface; what matters is the effective contact radius `1/(1/r_tire + 1/r_drum)`:

| | r_eff at r_tire = 0.34 m | Share of flat value |
|---|---|---|
| Reference test rig, Ø 0.77 m | 0.181 m | 53 % |
| **our drum, Ø 0.90 m** | **0.194 m** | **57 %** |
| Flat | 0.340 m | 100 % |

So the two drums differ by **7 %** in effective radius — our values are consequently
somewhat below the published ones, but in the same order of magnitude, not off by a factor
of 2. How the 7 % translate into Crr is the unknown part; there is no robust approach from
the literature for it. **Will not be guessed.**

Until then: comparisons between tires are valid, absolute values only with this caveat.

### C2 Reference tire — **deferred**
First establish repeatability, then procure a tire with a published Crr as an anchor.
Candidate remains Marathon Plus (0.00764, 25.5 W) — far enough above the noise that a
factor-2 error in the measurement chain would show.

### C4 Settling time and averaging length
`SETTLE_MS` = 15 s and `AVG_REVS_PER_POINT` = 10 are estimates. Adjust after the first
real measurements based on the scatter.

### C5 Draft
Drum aerodynamics dominate the idle losses at high speed. How strongly do they react to
doors, ventilation, position in the room? Try it out deliberately once.

## E — Mechanical defects of the stand

As of **2026-11-25**, reported by the operator after the first complete run
(2026-11-25-1830). None of them blocks a run, but each one feeds into the measured torque,
and none is quantified yet.

### E1 Motor wobbles — flange has to be replaced
The motor runs out of true; the cause is the flange, which has to be **made new**. The
wobble is passed on to the **weighing beam**, which wobbles with it. As far as can be
seen it does not hit anything. It shows up as torque ripple once per revolution (column
`ripple_Nm`, 0.05 to 0.11 Nm in run 2026-11-25-1830, i.e. 3 to 8 % of M). The mean over
whole revolutions cancels most of it, but not a contact that only happens on one side.
After the new flange: compare the ripple at the same speed.

### E2 Tread plate on the drum waves — surface no longer round
The aluminum tread plate on the drum ([C1](#c1-drum-curvature--halved-surface-done))
**waves between the screws** by now and makes noise. A wave is being pushed around the
drum by the tire, so the surface is neither round nor fixed. That is an extra loss that
belongs to the stand, not the tire, and it may change during a run (anchor drift W04).
Options:

| Option | For | Against |
|---|---|---|
| Re-tension the plate | quick, keeps the reference-like surface | the cause (plate longer than the circumference) stays |
| Remove it, sheathe the drum with thin plywood, stiffen with epoxy | round and smooth, no moving surface | smooth surface: Crr no longer comparable to tread-plate rigs |
| Several **tracks** side by side: smooth wood, tread plate, perforated plate | surface becomes a measurable variable; same tire on each track | more build work, wheel has to be moved sideways, each track needs its own baseline |

Decision open. Whatever is chosen, the surface goes into the run report.

### E3 Vibrations
There are vibrations on the stand, source not yet identified (E1, E2, motor cogging,
resonance of the balance, see [A7](#a7-natural-frequency-of-the-balance--measure-dont-weigh)).
**To do:** record the raw load cell at 80 SPS at several speeds and look for the
frequencies (once per drum revolution, per motor revolution, per screw spacing of the
plate).

### E4 Wheel load not measured cleanly
The wheel load is currently entered (calculated from the position of the sliding weight
or a hanging scale) and is **not measured during the run** (run 2026-11-25-1830:
"50.4 kg, eingegeben"). Crr is inversely proportional to it, so every error in the load
goes 1:1 into Crr. **Plan:** load cells at the wheel axle with their own calibration,
read during the run, the measured load per point into the CSV. Until then the load stays
an assumption without a measured uncertainty.

## D — Minor items

### ~~D2 Data storage~~ — **decided**
**No raw data in Git.** The raw 80-SPS stream is around 30 MB per two-hour session; after
twenty sessions the repo would be at 600 MB. The raw stream stays on the SD card.

What is versioned are the **averaged measurement points** — a few kilobytes per run. That
is the file the analysis runs on.

### B6 Web UI: portal or not — **decided, no portal**
A Captive Portal was built, tested on the device and removed again. Measurements and
evidence in [04-hardware](04-hardware.md) §13.2: the portal window is not a browser and
cannot be left, iOS disconnects the network afterwards (measured 15 s, with grace period
40 s), and Android shows no portal at all for private addresses. Instead the stand answers
the connectivity check, stays connected, and is opened via QR code.

**What remains open:** whether the WLAN should run open or with WPA2. Currently WPA2,
because otherwise anyone in range can drive the motor — the password is in the QR code; it
is never typed.

### A12 Measure lever arm and cell full-scale value
According to the statement of 2026-11-16 the cell is a **5 kg cell**, not 10 kg as
documented until then. Together with the old 40 mm arm, that would have loaded the cell to
128 % of its full-scale value on starting. The arm is now at 80 mm, derivation in
[08-stand](08-stand.md) §5.2.

**To do:** read the cell's nameplate (then set origin of `LOADCELL_CAPACITY_N` to DB) and
measure the bore on the lever arm to ±0.1 mm. According to the statement, about 80 mm is
installed; the arm enters **linearly** into every torque.

## Done

| Question | Decision | Date |
|---|---|---|
| **Calibrating the cell directly (C6)** | **Built 2026-11-28 (builder's decision).** Arm lifted, stirrup on the Bowden cable, 200 / 500 / 1000 / 2000 g, least-squares line, linearity in % of load with graphic ([09-measurement-theory](09-measurement-theory.md) §5.1, `calib.h`). The lever calibration over `L_cal` and the check weight in every run were dropped, so was the automatic zero by the motor of 2026-11-27. Price: the cell arm (84 mm) enters every torque 1:1 and still has to be measured to ±0.1 mm (A12 below). | 2026-11-28 |
| Computing platform | ESP32 (BLE for the pressure sensor, WLAN logging, one board for everything) | 2026-10-24 |
| Speed range | 10–100 km/h since 2026-11-25 (`V_MAX_KMH`, [07-safety](07-safety.md) §5 entry); 10–60 km/h before | 2026-11-25 |
| Speed pickup | Hall instead of reed, 6 marks + index; speed from the window over a whole revolution | 2026-10-24 |
| Drum bearing arrangement (A10) | **Own co-rotating shaft** instead of drum on the motor rotor. 4.7 mNm load-dependent in the torque path instead of 9.8 mNm of uncorrectable hysteresis. | 2026-11-01 |
| Drum width (A12) | **100 mm** built. Idle losses stay at 0.27 Nm. | 2026-11-01 |
| **Brake, triggered by the ESP32** | **Firmware 2026-11-25 (`brake.cpp`, `RELAY_BRAKE_WIRED`), hardware rebuilt the same day.** Proposal 2026-11-24: emergency stop in the relay coil chain, one wire to a transistor at the ESP32 (2N5551, GPIO40); hand and firmware in series. Lets the firmware brake above `BRAKE_MAX_KMH` via the relays. Circuit [04-hardware](04-hardware.md) §9.1, sequence §9.4, drawings `hardware/brake-plan.pdf` and `hardware/brake-wiring.pdf`. | 2026-11-24 |
| **Brake (A8, A9)** | **Short-circuit braking via three automotive changeover relays on the NC contacts.** Measured 2026-11-01: 25 s from over 400 rpm, mean braking torque 3.25 Nm, 68 W in the copper. Circuit in [04-hardware](04-hardware.md) §9. No braking resistor, no DC-DC converter. | 2026-11-03 |
| Bearing of the motor axle (A6) | Moot: the drum has its own shaft (A10), the stator axle is held only by the lever arm. | 2026-11-03 |
| Drum mounting (A4) | Moot: flange at the shaft end instead of adapter plate on the rotor. | 2026-11-03 |
| Motor controller (B1) | **FSESC 4.20 / 50 A** procured, VESC class with UART. | 2026-11-01 |
| SimpleFOC platform (B5) | Moot: with the FSESC the SimpleFOC path is dropped. | 2026-11-03 |
| ESP32 board (B2) | ESP32-S3-DevKitC-1 N16R8 procured. | 2026-11-01 |
| Directory name (D1) | Correctly named `rolling-resistance`. | 2026-11-03 |
| **Speed reserve (A13)** | **Done.** Bare drum with sensorless controller over **100 km/h at 42 V** = 589 rpm → **kv ≈ 14** (not 10; the 400 rpm on the BLDC tester were not the limit). At 36 V that is 505 rpm = 86 km/h, i.e. **43 % reserve** over the 353 rpm needed for 60 km/h. A 36 V power supply suffices. | 2026-11-03 |
| **Wheel load (A3)** | **No second load cell.** Before the measurement, slide a platform scale between drum and wheel, adjust wheel load to the target value. The existing 120 kg scale with 20 g resolution is 0.05 % at 42.5 kg. Wheel and tire mass are thereby included automatically. | 2026-11-03 |
| **Pole-pair count (A13b)** | **15 pole pairs, 30 poles.** Counted via the Hall sensors: 10 marked hand revolutions = exactly **900** tachometer steps, no backward step. Sensorless before: 16.5 and not reproducible. The motor controller was at `si_motor_poles` = 14 (the **factory default** of the VESC firmware, not inherited from the foreign configuration) and has been corrected to 30. `VESC_ERPM_MAX` has since been **derived** instead of guessed. | 2026-11-17 |
| **Hall sensors at the motor controller (A1b)** | **Present and detected.** `foc_detect_apply_all` reported "Found hall sensors, using them", `foc_sensor_mode` is set to Hall, the table is populated with exact 60° spacing. The tachometer counter is thus a measurement rather than an estimate. | 2026-11-17 |
| **Temperature sensor (A5, type)** | **10 k NTC in the stator**, confirmed three ways (open terminal −75.5 °C, 1.6 K against the FET sensor, monotonic rise under hot air against flat reference). `m_motor_temp_sens_type` = NTC 10K was already correct. Only the limit remains open. | 2026-11-17 |
| **Dissipative braking via the motor controller** | **Does not work** on firmware 5.02. Measured three times: bus to 53.3 to 55.8 V, fault code 1. Duty cycle 0 in **FOC** mode is not a bridge short but a regulated operating point — the real short of the low-side switches needs `foc_short_ls_on_zero_duty` from FW 6.05. The **short-circuit relays are thus the only brake**, not optional equipment. Reasoning and numbers in [07-safety](07-safety.md) §3.1. | 2026-11-17 |
| **Idle losses of the drum** | **Measured** from the free spin-down (97 → 29 km/h in 140 s, eight support points): M = 0.472 Nm + 3.178·10⁻⁴·ω². Air drag at 60 km/h **0.43 Nm** instead of the estimated 0.27 — factor 1.6, and thus 33 % of the useful signal. Constants in [03-parameters](03-parameters.md) §1. | 2026-11-17 |
| **Speed reserve, re-measured (A13)** | On the **free** motor 763 rpm at full duty and 40 V, linear over ten support points → 130 km/h at the drum. **With** drum, 99.2 km/h reached, at only 31.9 V effective and 120 W. A 36 V power supply is thus confirmed sufficient. The previously noted "kv = 14" is not a motor constant but the system top speed with drum. | 2026-11-17 |
| **Tire temperature (C3)** | **IR sensor** contactless onto the tread, available. Hangs on the existing I²C bus (GPIO8/9), no additional pin. Turns the warm-up into a criterion instead of a time requirement. | 2026-11-03 |
| Motor | **Hoverboard drive wheel 6.5″, 36 V / 350 W** procured. Thus 6.96 Nm continuous torque (factor 2.8 above the design case), but **without brake and without temperature sensor** — A9 and A5 become acute as a result. | 2026-11-01 |
| Motor controller | **FSESC 4.20 / 50 A** procured, VESC class with UART. Closes B1. | 2026-11-01 |
| Logic supply | **Via USB-C only.** According to Espressif, the 5V pin and USB are mutually exclusive; a forgotten external 5 V while flashing would short two sources. Both USB sockets may supply simultaneously. | 2026-11-01 |
| HX711 level | Module with **separate VCC/VDD**: VCC = 5 V for the bridge, VDD = 3.3 V for the logic level. The ESP32-S3 is not 5 V tolerant. Alternative (everything at 3.3 V) costs 3.6 dB SNR and was rejected. | 2026-11-01 |
| Final number of marks | **8** instead of 6 — on the pitch circle of the 8 spacer blocks, screwed Ø10×3 magnets with A2 screw. More marks bring *no* accuracy (the result runs index-triggered over whole revolutions, where the division cancels out), only latency. [04-hardware](04-hardware.md) §9 | 2026-11-01 |
| Computing platform, specific | ESP32-**S3**-DevKitC-1 N16R8. Old pin assignment (25/26/27/32/33/34/35/22) lay almost entirely on pins that do not exist on the S3 or that are occupied by flash/PSRAM. | 2026-11-01 |
| Design Crr and lever arm | `CRR_DESIGN_MAX` 0.015 → **0.007** (no MTB tires), hence `TORQUE_ARM_M` 0.070 → **0.040 m**. The design case now uses 48 % instead of 27 % of the cell's measuring range. [08-stand](08-stand.md) §5 | 2026-11-01 |
| Drum circumference | Measured: Ø 933 mm, U = **2.931 m**. `MAX_DRUM_RPM` is derived from it (341 rpm) instead of entered as a fixed value — the old fixed value 333 would have cut off at 58.4 km/h. | 2026-11-01 |
| Several marks useful? | Yes — not for Crr accuracy (which needs no v), but for control, slip detection and angle-resolved diagnostics | 2026-10-24 |
| Damp the torque balance? | Yes, but last: first build stiff and light, avoid excitation, average synchronously with revolutions — then an eddy-current damper. Nothing with stiction in the torque path. | 2026-10-24 |
| Gimbal motor directly on the hub? | No — factor ~10 too little torque, plus back-EMF limit | 2026-10-24 |
| Drive | Hub motor Bird One / DAAO 23328N36A28G001, 36 V / 350 W, 605 rpm, toothed belt i ≈ 1.8 | 2026-10-24 |
| Hoverboard or scooter motor? | **Decided 2026-11-01: hoverboard.** Earlier note: **Left open, a question of price.** Both have Hall sensors (verified on the hoverboard wheel from the listing photos: 3 thick phases + white JST with 5 thin wires, i.e. 8 conductors instead of the 6 described). Both meet 3.53 Nm with reserve, the hoverboard wheel with 6.96 Nm even more so. The only remaining difference is the mechanical brake. Decision rule in [03-parameters](03-parameters.md) §4. | 2026-10-24 |
| Direct drive instead of belt? | No. Possible in terms of speed (605 rpm = 109 km/h at the drum) and better metrologically, but only 5.52 instead of 9.94 Nm. Too little torque would mean a rebuild, belt loss is just a number to subtract. | 2026-10-24 |
| Reserve for 100 km/h | via the bus voltage, not via a gear ratio | 2026-10-24 |
| Belt or direct drive? | **Direct drive.** The torque balance of the cradle (`T_Arm = T_ext`) shows: a belt lies outside the cradle and is measured along, whereas the drum bearings with direct drive lie inside and drop out. After the literature review, 5.52 Nm with 36 % reserve suffice. | 2026-10-24 |
| Torque demand | Recomputed instead of estimated: 3.53 Nm steady state in the design case, not 3–6 Nm. Idle losses are 0.29 Nm, not 1–4 Nm. | 2026-10-24 |
| Standard wheel load | 42.5 kg (417 N), as in the published Crr database | 2026-10-24 |
| Lever arm | 0.07 m instead of 0.15 m — the worst case thus lands at 61 % of the cell's full-scale value instead of 29 % | 2026-10-24 |
| Warm-up time | 30 min instead of 10 min, following the reference test rig | 2026-10-24 |
| Motor selection in general | Two criteria instead of watts: `kv = n_rated/U_rated` must be 14–20 rpm/V, `T_cont = P_rated/ω_rated` at least 3.53 Nm. In practice 250 W minimum, 350 W right. Table in [03-parameters](03-parameters.md) §4. | 2026-10-24 |
| LG direct-drive washing-machine motor | Rejected: high-voltage machine (100–150 V bus needed), and the listing seen contained only the rotor without stator. | 2026-10-24 |
| Friction drive on the drum surface? | No — would need 1930 rpm instead of 480 | 2026-10-24 |
| Drum cantilevered on the hub? | No — one-sided stub axle, drum in two of its own bearings | 2026-10-24 |
