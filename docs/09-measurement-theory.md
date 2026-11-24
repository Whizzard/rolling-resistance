# Measurement Theory, Errors and Procedure

The closed account: which formulas apply, which quantities are determined once and which
before every measurement, where the sensors sit, how calibration works and what the
ESP32 needs to compute watts and Crr live.

Replaces the earlier 02-messkonzept (derivation and pitfalls). The checkable step list is
in [05-test-procedure](05-test-procedure.md) — this document gives the reasons behind it.

Numerical values apply to the stand as built (2026-11-24): drum U = 2.842 m
(Ø 905 mm, r = 0.4523 m), J = 1.94 kg·m², cell arm 84 mm, 5 kg cell, calibration arm
330 mm, designed for fast road tires. `config.h` and
[03-parameters](03-parameters.md) are authoritative. The findings of the review of 2026-11-24
([12-methodology-review](12-methodology-review.md)) are incorporated: inertia
correction per point (§1.7, §6), zero with free rotor (§5.3), reference point before and
after the plateaus (§7.1, §8), extended budget (§3.2).

## 1 The Formula Chain

### 1.1 From Raw Value to Power Loss

```
(1)  F_Z   = (raw − offset) · N_PER_COUNT                    [N]   force at the load cell
(2)  M     = F_Z · L                                         [Nm]  torque at the drum shaft
(3)  F_roll= M / r                                           [N]   tangential force at the drum shell
(4)  ω     = 2π · n                                          [rad/s]
(5)  v     = U · n                                           [m/s]  circumferential speed
(6)  P     = M · ω  =  F_roll · v                            [W]   total power loss
```

`n` is the drum speed in 1/s, measured via the marks. `r = U / 2π`.
**Speed does not appear in (1) to (3)** — Crr accuracy does not depend on the speed
measurement, only on force and geometry.

### 1.2 From Total Loss to the Tire

Everything that draws torque **outside** the cradle is measured. Three shares:

```
(7)  M_ges(v) = M_Reifen(v) + M_leer(v) + M_Rad(v) + M_Lager(N)
```

| Share | origin | treatment |
|---|---|---|
| `M_leer` | drum aerodynamics **and drum bearings** (pillow-block bearings on the stand, outside the cradle) | **idle-loss curve**, wheel lifted, before and after the series |
| `M_Rad` | hub bearings + aerodynamics of the test wheel | **wheel spin-down test**, [08-stand](08-stand.md) §6 |
| `M_Lager` | load-dependent **increase** of the drum bearings under wheel load | reference run with a rigid roller (A11), otherwise residual error `UNC_BEARING_REL` |
| `M_Reifen` | **the signal** | what remains |

```
(8)  F_roll,Reifen = (M_ges − M_leer − M_Rad − M_Lager) / r
(9)  Crr  = F_roll,Reifen / N
(10) P_Reifen = Crr · N · v
```

Both correction curves are kept as a model, not as a table:

```
(11) M_leer(ω) = A₀ + B₀ · ω²          fitted from the baseline run
(12) M_Rad(ω)  = (A₁ + B₁ · ω_Rad²) · r / r_Rad    from the wheel spin-down, converted to the drum
```

`ω_Rad = ω · r / r_Rad`, because drum and wheel share the same circumferential speed.

### 1.3 The Wheel Load

Swing arm balanced with a counterweight, pivot at wheel-axle height:

```
(13) N = m_Rad · g + W · b / a            a = 200 mm, W = sliding weight, b = scale reading
```

Because the pivot lies at wheel-axle height, the tangential force has no lever — the
correction term `∓ F_roll · Δz / a` drops out ([08-stand](08-stand.md) §5.2).

### 1.4 Numerical Example and Cross-Check

Continental Contact Urban, 18.4 W at 4 bar, 42.5 kg, 28.8 km/h (published):

```
Crr      = 18.4 W / (8.0 m/s · 416.9 N)   = 0.00552
F_roll   = 0.00552 · 416.9 N              = 2.301 N
M        = 2.301 N · 0.4523 m             = 1.0408 Nm
F_Zelle  = 1.0408 Nm / 0.084 m            = 12.39 N      (25 % of cell full scale)
P_Reifen = 0.00552 · 416.9 · 8.0          = 18.41 W      ✓ matches the source
```

This backwards calculation is the best functional test of the whole chain: whoever runs
a tire with a published value must end up with the same wattage — except for the drum
curvature, see §9.

### 1.5 Speed and Velocity

```
n [rpm] = 60 · v / U          ω = 2π · v / U          v = U · n
```

With **U = 2.842 m** (tape measure around the finished surface, 2026-11-23; Ø 904.6 mm):

| v [km/h] | n [rpm] | ω [rad/s] | pulse period with 8 marks |
|---|---|---|---|
| 10 | 58.7 | 6.14 | 128 ms |
| 28.8 | 168.9 | 17.69 | 44 ms |
| 60 | **351.9** | **36.86** | 21 ms |

`MIN_PULSE_US` = 800 µs safely filters only glitches. `MAX_DRUM_RPM` is **derived** in
`config.h` from `V_MAX_KMH` + `OVERSPEED_MARGIN_KMH` and `DRUM_CIRCUMFERENCE_M` (370 rpm)
and follows the circumference automatically.

### 1.6 What the Cradle Measures — and What It Does Not

Take as the system everything that sits on the cradle. In steady state the sum of the
**external** torques is zero, so `T_Arm = T_ext`. Everything internal cancels pairwise:

| Component | inside the cradle? | measured? |
|---|---|---|
| Cogging torque, iron losses of the motor | yes | **no** |
| Bearing friction rotor ↔ stator | yes | **no** |
| Drum bearings (two pillow-block bearings of the 15 mm shaft on the stand, motor only flanged on) | no | **yes** → in A₀ of the idle-loss curve; load-dependent increase §3.2, reference roller A11 |
| Coupling losses | no | yes → fall into the idle-loss curve |
| Drum air drag | no | yes → idle-loss curve |
| Bearings and aerodynamics of the test wheel | no | yes → wheel spin-down test |
| Tire at the contact patch | no | yes → **the signal** |
| Friction rotor ↔ stator (= the "cradle bearing") | yes | **no** — cancels exactly, [08-stand](08-stand.md) §7.2 |

The whole rest of the procedure follows from this: two correction curves (idle, wheel), one
residual error (drum bearings), and a cradle bearing that must be as low-friction as possible.

The commercial reference test rig instead measures the electrical power draw and
works backwards via an efficiency map of the motor
([description](https://www.bicyclerollingresistance.com/the-test)). That map is
the largest error source there; the cradle does not need it.

### 1.7 Ramp and Spin-Down

During a speed change the cell additionally measures the inertial torque:

```
(14) M_beschl = J · dω/dt
```

With J = 1.94 kg·m² and the configured ramp of 0.5 km/h/s that is **0.60 Nm** — almost
as much as the entire tire signal of a good racing tire (0.70 Nm). **That is why
measurement happens only on plateaus, never during the ramp.**

On the plateau the acceleration is not zero, only small: the control loop oscillates by
±0.2 km/h. What remains in the mean over a window of duration T is the
**net change**: `J·(ω_Ende − ω_Anfang)/T`. At 0.2 km/h net over 10 revolutions
that is 0.035 Nm at 15 km/h (5 % of the signal), 0.067 Nm at 28.8 km/h (10 %) and
0.140 Nm at 60 km/h (20 %). **That is why each point is corrected, not merely gated**
(since 2026-11-24, [12-methodology-review](12-methodology-review.md) K1): the firmware
fits a regression line v(t) over the whole point, takes its slope as the
net acceleration and subtracts `J_eff·dω/dt`, with `J_eff = J + J_Rad·(r/r_Rad)²` in the
measurement run. What remains is `UNC_J_REL` (5 %) of that; a point counts as valid as long as the
net acceleration stays below `POINT_DVDT_NET_MAX_KMH_S` = 0.2 km/h/s, then the
residual is below 1 %.

The same relation can be used in reverse during spin-down:

```
(15) J · dω/dt = −M_verlust(ω)
```

This yields M_verlust **independently of the load cell** and is therefore the sharpest
available test of the whole torque measurement chain — see §9.1.

### 1.8 Where the Drum Aerodynamics Come From

With direct drive, the idle loss consists only of the drum's air drag.
Calculated for 16.67 m/s circumferential speed, Ø 0.933 m, width 0.100 m,
ρ = 1.2 kg/m³, ν = 1.51·10⁻⁵ m²/s:

```
shell:     Re  = v·D/ν   = 1.03·10⁶ ,  Cf = 0.004 ,  A = π·D·b = 0.293 m²
           τ   = ½·ρ·Cf·v² = 0.67 Pa  →  F = 0.196 N  →  M = 0.091 Nm

discs:     Re_ω = ω·R²/ν = 5.15·10⁵ ,  Cm = 0.146·Re_ω^−0.2 = 0.0105
           M    = Cm·½·ρ·ω²·R⁵ = 0.178 Nm

sum ≈ 0.27 Nm ≈ 9.6 W at 60 km/h, growing with v²
```

**Measured on 2026-11-17** from the free spin-down (97 → 29 km/h in 140 s, eight
support points, M = J·dω/dt):

```
M_verlust = 0.472 Nm  +  3.178·10⁻⁴ · ω²          (ω in rad/s)

at 60 km/h:  0.91 Nm total,  of which 0.43 Nm with ω²   (calculation above: 0.27 Nm)
at 100 km/h: 1.68 Nm total,  of which 1.20 Nm with ω²
```

The ω² share is **1.6 times** as large as calculated — and the calculation itself names
the reason one line further on: the drum has **teardrop-pattern sheet**, not a smooth surface. For
an air-drag estimate, a factor of 1.6 is a good result.

The constant share of 0.472 Nm is the **drag torque** (bearings and motor), i.e.
exactly the `a` from §8.9 — there it was taken as unknown.

> What of this enters the measurement result: **only the ω² share.** Drum bearings and
> motor friction lie inside the cradle and cancel out. The 0.43 Nm at
> 60 km/h, however, stand against a useful signal of 1.32 Nm (Crr 0.007 at 417 N) — that is
> **33 %**, at 100 km/h 91 %. Subtractable, because measurable at idle per stage; but to be
> measured, not calculated. Constants in [03-parameters](03-parameters.md) §1.

Applies to a **smooth, closed** drum in still air. Open spokes, a rough surface
or attachments can yield a multiple. **The first idle-loss curve
(§8.4) replaces this calculation** — it is only the expectation against which the measurement result
is checked for plausibility.

## 2 Which Quantity Is Determined When

### 2.1 Once — Stand Constants

Change only on rebuild. Belong in `firmware/src/config.h` **and**
[03-parameters](03-parameters.md).

| Quantity | Symbol | Determination | Target accuracy |
|---|---|---|---|
| Drum circumference | `U` | tape measure **over the finished surface**, average of 3 measurements | ±1 mm (0.03 %) |
| Drum radius | `r = U/2π` | calculated | — |
| Lever arm of the torque balance | `L` | axle center → force application, 84 mm; **cancels out in the torque** (§5.1), only determines cell utilization | ±0.5 mm suffices |
| Calibration radius | `L_cal` | axle → mark on the arm, 330.0 mm; **determines the correctness of every torque** | ±0.1 mm (0.03 %) + plumb direction |
| Cell characteristic | `N_PER_COUNT` | §5.1, at least 3 weights | ±0.2 % |
| Lever of the wheel load | `a` | pivot axis → wheel axle, horizontal | ±0.5 mm (0.25 %) |
| Marks per revolution | `MARKS_PER_REV` | by definition, 6 + index | exact |
| Drum inertia | `J_Trommel` | spin-down test, §9.1 | ±5 % (inertia correction per point, §1.7) |
| Idle-loss model | `A₀, B₀` | baseline run, fitted | see §3 |

### 2.2 Per Wheel — Once per Wheel, Not per Tire

| Quantity | Determination | Target accuracy |
|---|---|---|
| Moment of inertia `J_Rad` | bifilar pendulum, [08-stand](08-stand.md) §6.2 | ±1.5 % |
| Wheel loss model `A₁, B₁` | wheel spin-down test | ±2 % |
| Wheel mass `m_Rad` | scale | ±20 g |
| Reference tire mass `m_Reifen,ref` | scale, belongs to `J_Rad` | ±1 g |

### 2.2a Rolling Radius from the Speed Ratio

`r_Rad` is **not entered but measured**. Drum and wheel roll on each other,
so both surfaces cover the same distance:

```
U_Rad = U_Trommel · n_Trommel / n_Rad        r_Rad = r_Trommel · n_Trommel / n_Rad
```

| Contribution | Size |
|---|---|
| Uncertainty of `U_Trommel` | 0.035 % (1 mm tape measure) |
| Slip in the contact patch, estimated | ≈ 0.01 % |
| Count quantization over 30 s | negligible |
| **total** | **≈ 0.05 %** |

On slip: only the wheel's own loss torque is transmitted, `F_t = M_Rad/r =
0.55 N` against 417 N wheel load, so µ = 0.0013. With µ_max ≈ 0.8 and 5 to 10 % slip at
the maximum, roughly 0.01 % of that remains.

The value so determined is the **loaded rolling radius on the curved drum** —
exactly the quantity that belongs in the conversion. A tape measure around the unloaded tire
gives 2 to 4 % too much.

> The ratio is at the same time a **slip monitor**: if it jumps, the tire is slipping.

### 2.3 Per Tire / per Measurement Series

| Quantity | Determination | Note |
|---|---|---|
| Tire mass `m_Reifen` | scale | corrects `J_Rad`: `ΔJ = Δm · r_Reifen²` |
| Tire radius `r_Rad` | circumference with tape measure, measured loaded | enters (12) |
| Boom height at the mast | set to wheel-axle height, clamp lever | spirit level on the fork leg |
| Tire pressure `p` | gauge, directly before the run | console `p<wert>` |
| Wheel load `N` | from `b` on the scale, formula (13) | console `l<wert>` |
| Static tare `offset` | console `t`, drum stationary | **after every intervention** |
| Idle-loss curve | baseline before **and** after the series | rejection criterion §3.3 |
| Ambient temperature | thermometer, start and end | into the log |

### 2.4 Per Measurement Point

| Quantity | Source |
|---|---|
| `v` or `n` | mark counter, mean over whole revolutions |
| `F_Z` | HX711, mean over the same whole number of revolutions |
| Tire temperature | IR sensor via I²C, logged alongside |
| Timestamp | for the temporal interpolation of the baseline |

## 3 Error Budget and Dynamics

What has to be resolved is in this table. Crr rises with speed, but
between 14.4 and 36 km/h only by **11.6 %**
([measurement](https://www.bicyclerollingresistance.com/specials/crr-speed-test), mean of
6 tires):

| v [km/h] | 14.4 | 21.6 | 28.8 | 36.0 |
|---|---|---|---|---|
| Crr | 0.00346 | 0.00362 | 0.00378 | 0.00386 |

The curve is therefore a flat effect. To resolve it over several support points
requires **1 to 2 % per measurement point**.

### 3.1 Express Everything in F_roll

`F_roll = Crr · N` contains neither r nor L — that makes the contributions comparable.
The reference is the target from [01-requirements](01-requirements.md) M1:
**ΔCrr = 0.0005 ⇒ ΔF_roll = 0.208 N.**

| Signal size | Crr | F_roll | M | F_Zelle |
|---|---|---|---|---|
| best racing tire | 0.0025 | 1.042 N | 0.471 Nm | 5.6 N |
| good racing tire | 0.0037 | 1.543 N | 0.698 Nm | 8.3 N |
| Contact Urban | 0.0055 | 2.301 N | 1.041 Nm | 12.4 N |

### 3.2 The Contributions

| Error source | on F_roll | of target | of signal* | in code |
|---|---|---|---|---|
| Cell noise, single sample | 0.0017 N | 0.8 % | 0.11 % | Type A |
| Cell noise after 10 revolutions | 0.0001 N | 0.1 % | 0.01 % | Type A |
| Cell, FS error 0.02 % (creep, repeatability) | 0.0017 N | 0.8 % | 0.11 % | `UNC_K_REL` |
| Calibration arm L_kal to ±0.1 mm plus plumb (0.1 %) | 0.0015 N | 0.7 % | 0.10 % | `UNC_LCAL_REL` |
| ~~Lever arm L~~ — *cancels out, §5.1* | 0 | 0 | 0 | — |
| Circumference U to ±1 mm | 0.0005 N | 0.3 % | 0.03 % | — |
| Wheel load ±0.5 % | 0.0077 N | 3.7 % | 0.50 % | `UNC_N_REL` |
| **Tire pressure ±1 %** (gauge, AirSpy offset) | 0.0154 N | 7.4 % | 1.00 % | `UNC_P_REL` |
| **J_Rad measured, ±1.5 %** | 0.0080 N | 3.9 % | 0.52 % | `UNC_RAD_REL` |
| J_Rad estimated, ±10 % *(as long as there is no bifilar pendulum)* | 0.0540 N | 26 % | 3.5 % | `UNC_RAD_EST_REL` |
| **Drum bearings, load-dependent (4.7 mNm)** | 0.0104 N | 5.0 % | 0.67 % | `UNC_BEARING_REL` |
| **Baseline drift, criterion 0.02 N** | 0.0200 N | 9.6 % | 1.30 % | drift pre/post, otherwise criterion |
| Inertia correction, residual 5 % of J·Δω/T (0.2 km/h over 10 rev at 28.8) | 0.0074 N | 3.6 % | 0.48 % | `UNC_J_REL`·M_inert |
| Averaging without a true index: ripple 20 %, a/N at N = 10 | 0.0155 N | 7.4 % | 1.0 % | ripple/2/N, only without index |
| Zero point (cogging torque, drift), both checks passed | ≤ 0.0200 N | 9.6 % | 1.3 % | W13/W14 as warning |
| *Baseline drift, old criterion 0.10 N* | *0.1000 N* | *48.0 %* | *6.48 %* | — |

\* relative to a good racing tire (1.543 N)

Summed in quadrature, the bold rows give roughly **0.029 N = 14 % of the target**
(with index mark; without it 0.033 N = 16 %). With only an estimated J_Rad it is
**0.063 N = 30 %**. Thus two tires differing by ΔCrr = 0.0005 are
separable — but only with bifilar pendulum, index mark and both zero-point checks. The
firmware computes the same sum per plateau as u(Crr) with k = 1 into the log
([11-run-review](11-run-review.md) R9).

### 3.3 The Old Baseline Criterion Has Become Too Soft

The earlier criterion rejected a series at more than **0.10 N** deviation between pre-run
and post-run baseline. For MTB tires (F_roll up to 6.3 N) that was a
percentage of 1.6 %. For a good racing tire the same 0.10 N is **6.5 %** of the
signal, for the best tire 9.6 %.

→ **Tighten the criterion to 0.02 N.** That is 1.3 % of the racing-tire signal and 9.6 % of the
resolution target. It is achievable because the idle loss itself is only 0.267 Nm (= 0.57 N on
F_roll): 0.02 N is 3.5 % drift of the idle-loss curve over a measurement series.

### 3.4 The Wheel Load Enters Linearly

`Crr = F_roll / N` — one percent error on N is one percent on Crr.

| Error on N | Error on Crr (at 0.0037) |
|---|---|
| 0.5 % (2.1 N) | 0.000019 |
| 1.0 % (4.2 N) | 0.000037 |
| 2.0 % (8.3 N) | 0.000074 |

Even 2 % stays below ΔCrr = 0.0005. The wheel load is therefore **not** the critical path
— as long as it is the same between the compared tires. For comparison with
published data, by contrast, the absolute value counts: aim for 1 % there.

### 3.5 The Balance as an Oscillator

Lever arm, tension link and load cell form a spring-mass system with the mass of the stator.
Stiffness as seen from the rotation axis:

```
k_rot = k_Zelle · L²  =  327 N/mm · (0.080 m)²  =  2092 Nm/rad   [TBD: k of the 5 kg cell]
```

The tension link (shift cable, 1272 N/mm) contributes only 0.4 % to this and is negligible.
Since the cradle was dropped, the mass is **only the stator**:

| J_Stator | f_n |
|---|---|
| 0.007 kg·m² (2 kg, radius of gyration 60 mm) | **43.5 Hz** |
| 0.012 kg·m² | 33.2 Hz |
| 0.020 kg·m² | 25.7 Hz |
| 0.026 kg·m² (4 kg, 80 mm) | **22.6 Hz** |

For comparison, the excitation at 60 km/h (drum 5.87 rev/s):

| Order | Frequency | Cause |
|---|---|---|
| 1st | 5.9 Hz | imbalance, joint of the shell sheet, tire out-of-roundness |
| 2nd | 11.7 Hz | runout error |
| 3rd | 17.6 Hz | |
| 4th | 23.5 Hz | |
| 8th | 47.0 Hz | eightfold geometry, if present |

**At the light end the natural frequency lies above all orders, at the heavy end it does not:**
at J = 0.026 kg·m² f_n at 22.6 Hz falls between the 3rd and the 4th order. That is why
f_n is **measured** after the build, not calculated (A7) — and nothing soft goes into
the force path. Every elastomer and every leaf spring in series pushes it down:
with a 58 N/mm series spring it would be only 12.9 Hz and thus between 2nd and 3rd order.
That is the reason the tension link must be **axially stiff** and the compliance
should come from the cell alone.

> **The mean value is unaffected by this.** The measurement is torque-driven: the arm is
> free to move, `F = T/L` holds regardless of what in the chain flexes or creeps.
> A resonance only enlarges the amplitude around the mean, and that drops out with
> synchronous averaging. It is dangerous because the arm runs into the stops.

### 3.6 Step Response: Factor 2

A torque step — braking, current limit at start-up, a blocking wheel — builds
up in about one millisecond. An undamped system responds to it with
**twice** the static value. Two decisions follow from this:

- The stop must withstand **twice** the expected torque.
- The eddy-current damper must be mounted **before** braking for the first time.

Damping is applied **in parallel**: aluminum disc on the lever arm between two magnets. An
elastomer in the force path damps too, but lowers f_n in doing so — it buys the damping
with exactly the problem it is supposed to solve.

### 3.7 Sampling: Here Is Still a Gap

The HX711 runs at 80 SPS, so the Nyquist limit is at 40 Hz.

| | 10 km/h | 28.8 km/h | 60 km/h |
|---|---|---|---|
| Samples per revolution | 84 | 29 | **14** |
| 8th order | 7.6 Hz | 21.8 Hz | **45.5 Hz** |

At 60 km/h the 8th order lies **above** the Nyquist limit and folds back to
**34.5 Hz**. There it no longer sits on an order of the rotation and is no longer
cleanly cancelled by synchronous averaging. Two measures:

1. **No eightfold geometry on the drum.** Do not place the eight sheet-metal screws of the shell
   in one plane on a pitch circle, but axially staggered. The eight
   speed marks themselves are uncritical — magnets generate no force on the drum
   as long as the sensor holders are aluminum or plastic (§4).
2. **Do not average samples, integrate over time.** At 14 samples per
   revolution the sampling instants are not synchronous with the rotation; a simple
   mean then weights unevenly. Correct is trapezoidal integration between two
   index pulses, divided by the duration.

### 3.8 Why Synchronous Averaging Rather Than Low-Pass Filtering

A low-pass filter would have settling time and phase shift and would not capture the low orders (5.7
and 11.4 Hz — exactly the disturbing ones) at all. Averaging over a **whole
number of revolutions**, by contrast, cancels *all* harmonics of the rotation exactly, independent
of frequency and amplitude, and costs only the index mark.

Sorted by mark number into **45° bins**, the same raw signal additionally shows *where* on
the circumference the disturbance sits — joint, imbalance and tire out-of-roundness can thus be
told apart.

## 4 Sensors: What Sits Where

| Sensor | Location | Signal | Pin |
|---|---|---|---|
| **Drum marks** | **8 magnets** on the end disc (the existing lines), r as large as possible, **holder of aluminum or plastic** | Hall, interrupt | GPIO4 |
| **Index mark** | 1 magnet, different track or different radius — **must be unipolar (TLE4905L)**, a latch would stay stuck | Hall, interrupt | GPIO5 |
| **Motor Hall** | present in the motor, bring out one phase | interrupt | GPIO7 |
| **Load cell** | lever arm → **shift cable 1.2 mm, 80 mm** → cell, preloaded to 12 N | HX711, 80 SPS | 15 / 16 |
| **Wheel speed** | **1 magnet on the spokes** (installed this way since 2026-11-22; more would only give a faster update, no accuracy), Hall at the dropout | interrupt | GPIO6 |
| **Measuring zone of the balance** | slot-type photo interrupter, vane with window at r = 200 mm on the lever arm | digital | **TBD, free pin** |
| Tire temperature | **IR contactless** onto the tread, just behind the contact patch | I²C | 8 / 9, no dedicated pin |
| Ambient temperature | on the frame, away from motor and power supply | I²C | **TBD** |
| Motor PTC | in the motor | analog | GPIO1 (ADC1) |
| Tire pressure | BLE sensor at the valve | BLE | — |

Two things that decide during assembly whether the measurement is any good:

1. **Marks as far out as possible.** The pulse period is the time base for v; the
   larger the radius, the less sensitive to position errors of the magnets. Uneven
   spacing otherwise produces a spurious modulation at the rotation frequency — exactly where
   the joint of the shell sheet also sits.
2. **Hall sensor for the wheel at the dropout, not on the fork.** It has to move with the wheel
   when the wheel size is adjusted.

## 5 Calibration

### 5.1 Load Cell and Torque Chain

Calibration covers **not the cell alone but the whole chain** up to the torque. That
saves the separate uncertainty of cell and lever arm — more precisely: **the cell arm L
cancels out.** Calibrated is `k = m·g·L_kal / (L·Δcounts)`, computed is
`M = counts·k·L`, so `M = counts · m·g·L_kal / Δcounts`. L_kal has to be correct (the
mark on the arm, 330 mm) and the weight must hang plumb; L only determines how far the cell
is utilized. Because k applies to exactly the arm with which it was calibrated, the
firmware remembers the arm at calibration and the measurement run blocks if it was changed.

1. Balance in measuring position, **wheel lifted**, drum nudged by hand and allowed to settle
   (rotor free in the detent position, §5.3), cables routed as in operation.
2. Static tare (`t`), 200 samples.
3. Hang a known weight at the **mark of the calibration arm** at `L_cal` = 330.0 mm.
   Torque `M_cal = m · g · L_cal`. Average the raw value over 200 samples.
4. Repeat with at least **three** masses over the range. At `L_cal` = 330 mm the
   cell at arm L = 84 mm sees **3.9 times** the force: 0.2 / 0.5 / 1.0 kg give
   0.65 / 1.62 / 3.24 Nm and 16 / 40 / 79 % of the full scale of the 5 kg cell; the measurement signal
   (0.5 to 1.4 Nm) lies between the first two. **Do not hang more than 1.27 kg**
   — that is the full scale. (calculated)
5. Linear fit `M = k · (raw − offset)`. Require **linearity deviation < 0.2 %**.
   So far calibration has been done with only **one** weight (1 kg) ([03-parameters](03-parameters.md)
   §2); the linearity check is pending.
6. Check hysteresis: load ascending and descending. The difference at equal load
   belongs in the log, not in the fit.
7. `N_PER_COUNT = k / L` into `config.h` and [03-parameters](03-parameters.md).
8. The check weight in the measurement run (1 kg at 330 mm) checks **drift**, not correctness:
   it is the same weight on the same arm. Correctness is checked by the cross-check with a
   second mass in the measuring range (0.2 kg) or the same mass on the cell arm (force
   times 84/330).

> Weigh the calibration weights yourself. A kitchen scale to 1 g at 2 kg is 0.05 % — good
> enough. Taking gravitational acceleration as 9.81 m/s² suffices; the local variation in
> Germany is below 0.1 %.

### 5.2 Wheel Load

The wheel load is **weighed, not calculated**. The value of 2026-11-23 (494 N from
7.2 kg + 10.4 kg · 84/20.2) is a calculation from the lever geometry; it now serves only
for plausibility.

**Way a, now: gantry and hanging scale at the wheel axle.** A gantry over the swing arm,
the hanging scale grips the wheel axle and pulls **vertically** upward, with the arm free
(eccentric released, sliding weight in position). Lift until the wheel just
no longer touches the drum — the displayed value is then exactly the force that presses
on the drum in measuring operation, including wheel, tire and arm. Three things decide:

- **pull vertically**, otherwise a cosine enters the reading (5° is 0.4 %);
- **lift as little as possible** (1 to 2 mm clearance), so the arm stays practically in the
  measuring position and the lever ratio does not tip;
- check the hanging scale beforehand with a known weight (the 1 kg check weight).

The value goes into the firmware as `l<kg>` or, in the measurement run, as the mandatory field "Auflast".
Uncertainty `UNC_N_REL` = 0.5 %: scale, pull direction, residual clearance.

**Way b, later:** two load cells directly at the wheel axle, displayed live
(`PIN_HX7112_DOUT` is reserved). That eliminates the manual step, and the wheel load is
in the log per measurement point.

**Setup, once:** measure `a` horizontally (±0.5 mm), tape measure on the beam,
zero **on the pivot axis**, center-punch an index on the sliding weight. The read position of the
sliding weight goes into the CSV. If it differs next time, something has changed on
wheel, tire or mechanics — then weigh again.

### 5.3 Zero, Twice

| | when | captures |
|---|---|---|
| **static tare** | drum stationary, before every series | ADC offset, dead weight of the cradle, drift |
| **idle-loss curve** | drum turning, wheel lifted | everything that draws torque outside the cradle |

Both are needed. The static tare alone leaves the aerodynamics in place, the
idle-loss curve alone leaves the ADC offset in place.

**The tare needs a free rotor** ([12-methodology-review](12-methodology-review.md)
K2). The cogging torque does lie inside the cradle and averages out during the run, but at
standstill `T_arm = T_last` holds: if the rotor stands at an arbitrary angle and static friction
(tire on the drum) holds it there, the arm carries the cogging torque of that
angle, typically 0.05 to 0.2 Nm, and it ends up in the offset. Therefore: **lift the wheel,
nudge the drum by hand and let it settle**, then zero; afterwards turn the drum a
quarter revolution further and read again. If the value deviates by more than
`RUN_ZERO_LIMIT_N` (0.02 N on F_roll), something is holding the rotor (W13). The same
reading after the spin-down, wheel lifted, checks the drift of cell, cable and preload
over the run (W14) — hands-off, the guided run does it in the "post-run" step.

## 6 The Calculation Chain in Operation

Order of evaluation. The equations are in §1, the constants in
[03-parameters](03-parameters.md), inputs and display in
[10-firmware](10-firmware.md).

```
per sample:  F_Z = (raw − offset) · k

per revolution, index-triggered, averaged over AVG_REVS_PER_POINT:
             n   = marks / (k_Marken · Δt)
             ω   = 2π·n              v = U·n
             M_roh = F_Z,mittel · L
             dω/dt = slope of the regression line ω(t) over the point
             J_eff = J_Trommel + J_Rad·(r/r_Rad)²        (measurement run; idle only J_Trommel)
             M   = M_roh − J_eff · dω/dt                  inertia correction, §1.7
             M_leer    = A₀ + B₀·ω²
             ω_Rad     = ω · r / r_Rad
             M_Rad     = (A₁ + B₁·ω_Rad²) · r / r_Rad
             M_Reifen  = M − M_leer − M_Rad
             F_roll    = M_Reifen / r
             Crr       = F_roll / N
             P_Reifen  = M_Reifen · ω        (= Crr · N · v)
```

**The wattage that counts is `P_Reifen`**, not `P_ges = M·ω`. The published values
are tire powers at 28.8 km/h and 42.5 kg; `P_ges` additionally contains drum and
wheel and would be about 2 W too high at 28.8 km/h (1.05 W drum, 0.99 W wheel).

## 7 The Measurement Matrix

### 7.1 Three Axes, Three Purposes

| Axis | Support points | purpose |
|---|---|---|
| **Speed** | 10, 15, 20, 25, **28.8**, 35, 40, 45, 50, 55, 60 km/h | curve Crr(v), comparison point at 28.8 |
| **Tire pressure** | 3, 4, 5, 6 bar (tire-dependent) | Crr(p), the relationship of actual interest |
| **Wheel load** | 30, 42.5, 55 kg | linearity check: Crr should be nearly load-independent |

Full factorial would be 11 × 4 × 3 = 132 points per tire — unrealistic. Practicable:

1. **Standard run per tire:** reference point 28.8 km/h **directly out of the warm-up**, then
   the plateaus ascending, then 28.8 km/h once more. The difference between the two
   reference points is the drift over the measurement run (tire temperature, bearings, air). The
   tire cools at 15 km/h and warms up to 60 km/h beyond the warm-up state;
   the curve Crr(v) therefore contains a temperature trend that only a second
   pass in reverse order separates ([12-methodology-review](12-methodology-review.md)
   W1). About 10 min plus two baselines.
2. **Pressure series:** only at 28.8 km/h, all pressure stages, 42.5 kg. At least
   10 revolutions per stage, **5 min settling time** between stages — with the
   pressure the tire also changes its flexing work and thus its temperature.
3. **Load series:** only at 28.8 km/h and 4 bar, three loads. Shows whether the lever balance
   works linearly — Crr should hardly change in the process.

### 7.2 What May Change Between the Axes

| | Wheel | Tire | J_Rad | Baseline | Warm-up |
|---|---|---|---|---|---|
| different speed | same | same | same | valid | once |
| different pressure | same | same | same | valid | 5 min re-warm |
| different wheel load | same | same | same | valid | 5 min re-warm |
| **different tire** | same | **new** | **correct** | **new** | **full 30 min** |
| **different wheel** | **new** | new | **measure anew** | **new** | **full 30 min** |

A tire change thus entails three things: correct J (§2.3), new baseline,
full warm-up. A wheel change additionally the bifilar and the spin-down test.

## 8 Test Procedure

A complete pass takes about two hours, of which 30 minutes is warm-up.
Shortcuts take their revenge: the differences to be measured are smaller than
the effects a skipped step causes. For perspective — the curve
over speed changes between 14 and 36 km/h by a whole 11.6 %, and two
good racing tires are 0.2 N of tangential force apart.

**The step list itself is in [05-test-procedure](05-test-procedure.md)** — bare and
checkable there. Here only why the order is as it is and what the numbers mean.

| Step (05) | why at this point |
|---|---|
| B0 visual inspection, emergency stop | [07-safety](07-safety.md) |
| B1 insert wheel, pivot at wheel-axle height | determines the geometry of all following steps |
| B2 weigh tire, set pressure, correct `J_Rad` | before the tare, because it does not touch the cradle |
| B3 static tare `t` | after every mechanical intervention |
| B4 baseline pre-run `b`, wheel lifted | cold → lower corner value of the drift |
| B5 warm-up | Crr falls with tire temperature; pressure and tire temperature (AirSpy) are logged alongside |
| B6 measurement run `m` | reference point out of the warm-up, plateaus, reference point; thermally settled only at the reference point |
| B7 baseline post-run `b` | warm → upper corner value, difference is the rejection criterion |
| B8 let spin down, do not brake | §8.9 |
| B9 evaluation `analysis/crr.py` | interpolates the baseline in time between B4 and B7 |

### 8.0 Before Switching On

Checklist in [05-test-procedure](05-test-procedure.md) §B0, safety part in
[07-safety](07-safety.md) §7.

> A dragging brake — should one be retrofitted — produces a
> temperature-dependent drag torque and lets
> pre-run and post-run baseline diverge. That makes the whole series worthless, and you see it
> only at the rejection criterion in §8.8.

### 8.1 Static Tare

Drum stationary, **wheel lifted, rotor free** (§5.3), `t` on the console. The firmware
averages `TARE_SAMPLES` = 200 samples (≈ 2.5 s at 80 SPS) and sets the offset. Captures
ADC offset, dead weight of the weighing part on the lever arm and the drift since the last
adjustment. Then quarter revolution and counter-reading.

**Repeat as soon as anything on the setup has changed** — also after adjusting
the boom height or rerouting a cable.

### 8.2 Insert Wheel and Set Geometry

Checklist in [05-test-procedure](05-test-procedure.md) §B1. The three dimensions behind it:

- **centered**: with 100 mm drum width and a 57 mm tire only 21 mm per side remain.
- **fork leg horizontal** (spirit level): then the pivot lies at wheel-axle height and
  the tangential force has no lever, the correction term from §1.3 drops out.
- **x = 0 ± 2 mm**: wheel axle vertically above the drum axle, otherwise the
  contact-patch normal wanders.

### 8.3 Prepare Tire

Checklist in [05-test-procedure](05-test-procedure.md) §B2. Before the tare, because no step
of it touches the cradle. The standard wheel load `STANDARD_LOAD_N` = **416.9 N
(42.5 kg)** is the load of the published Crr database — only then are the values comparable.

### 8.4 Baseline Pre-Run

**Wheel lifted** (eccentric lever), *Leerlauf* on the measurement page or `m0` on
the console. The firmware runs through all measurement speeds from the settings
(default `PLATEAUS_KMH`), waits `SETTLE_MS` each and averages over
`AVG_REVS_PER_POINT` **whole revolutions**, triggered on the index mark.

Result: `M_leer(ω)`, from it the fit `A₀ + B₀·ω²`. Do not skip — without this curve
the absolute values are worthless.

### 8.5 Warm-Up

**30 minutes at 28.8 km/h.** Not a safety margin, but the time the
commercial reference test rig allows
([source](https://www.bicyclerollingresistance.com/the-test)). It then measures
three times per pressure stage at a room temperature held to ±0.5 °C — this
temperature constancy cannot be achieved in a workshop, and precisely for that reason the
A/B/A protocol in §8.10 is not an extra but load-bearing.

### 8.6 Measurement Run

`m` on the console, **the same plateaus** as in the baseline run — the evaluation
subtracts pointwise, the support points must match. The guided run places
the reference point (28.8 km/h) before and after the plateaus (§7.1). Note the tire pressure after the
warm-up or take it from the AirSpy: the air in the tire warms by 10 to
20 K, that is 3 to 7 % absolute pressure and 1 to 3 % Crr — tire-dependent, hence important
precisely for the A/B comparison. The database sets pressure cold; what applies here goes in the
log.

During the run do not touch the setup and **do not change doors or ventilation** —
the drum aerodynamics react to draft, and they enter the result directly via the
idle-loss curve. Pressure and load series need 5 min settling time per stage (§7.1).

### 8.7 Baseline Post-Run

Lift the wheel, `b`. Second baseline immediately afterwards, in the warm state.

### 8.8 Rejection Criterion

> If the post-run baseline deviates from the pre-run baseline at any support point by more than **0.02 N** (referred to F_roll),
> the measurement series is to be rejected.

0.02 N is 1.3 % of the racing-tire signal and 9.6 % of the resolution target (§3.3). The earlier
value of 0.10 N comes from the MTB design and would be 6.5 % of the signal here.
Usual causes: temperature drift of the bearings, dragging brake, moved cable,
draft.

### 8.9 Let Spin Down

`s`, then let it spin down freely. **Do not brake by hand.** At 60 km/h about 1.3 kJ
are stored. The spin-down duration depends almost entirely on the drag torque of the motor:

With `J·dω/dt = −(a + b·ω²)`, a = drag torque, b = 0.27/35.73² = 2.12·10⁻⁴:

```
t = (J/√(ab)) · arctan(ω₀·√(b/a))
```

| Drag torque a | Spin-down from 60 km/h |
|---|---|
| drum bearings only (0.002 Nm) | 80 min |
| 0.05 Nm | 13 min |
| **hub motor, typical cogging torque (0.15 Nm)** | **5.7 min** |
| strongly cogging stator (0.30 Nm) | 3.3 min |

Plan for it — there is no brake ([07-safety](07-safety.md) §3). **Continue
working only when the drum is visibly stationary.**

### 8.10 A/B/A

For every comparison: reference tire — test tire — reference tire, in **one** session,
at the same wheel load and the same pressure. If the second reference measurement deviates from the
first, the difference in between cannot be interpreted.

At least three repetitions per tire, state the scatter as well.

**Plausibility anchor:** once run a tire with a published value, at 42.5 kg and
28.8 km/h. The absolute values will not match exactly (§10), but the
order of magnitude must fit and the ranking of several tires must be right. If it
is not, something fundamental is wrong.

### 8.11 Evaluation

```sh
python3 analysis/crr.py data/<datei>.csv
```

Reads the run set of the guided measurement run (`/runs/<kennung>.csv`) and computes
**the same way as the firmware**: idle-loss model A₀/B₀ from the pre-run fit, wheel model
A₁/B₁ from the header, Crr per plateau with pooled scatter and u(Crr), plus anchor drift,
pre-/post-run drift and zero points. One run, two computers, one number. The old serial
CSV (`note=baseline|measure`) is still read, there with a fit per baseline run.

## 9 Special Runs

Not part of every measurement, but a prerequisite for the numbers meaning anything.

### 9.1 Spin-Down Test of the Drum — Yields J and Checks the Measurement Chain

Once and after every change to the drum:

1. Wheel lifted, accelerate to 60 km/h.
2. Disconnect the drive to freewheel, record the speed trace down to standstill.
3. From `J · dω/dt = −M_verlust(ω)` follows M_verlust **independently of the load cell**.
4. Compare with the idle-loss curve from §8.4. **Both ways must agree** —
   that is the sharpest available test of the whole system.

J itself follows from the same test with a known added mass, or from the
geometry (2.07 kg·m² calculated).

### 9.2 Bifilar Pendulum — Yields J_Rad

Once per wheel. Hang the wheel horizontally on two threads of equal length, at two
opposite points of the rim:

```
(16) J_Rad = m · g · b² · T² / (4 π² · L)
```

b = half the distance between the suspension points, L = thread length, T = period over
20 oscillations. Deflection < 10°, tire mounted and inflated. Achievable **1.2–1.4 %**
([08-stand](08-stand.md) §6.2).

### 9.3 Wheel Spin-Down Test — Yields A₁, B₁

Once per wheel:

1. Bring the drum with the wheel resting on it to 60 km/h.
2. Lift the wheel **while running** (eccentric lever).
3. Record the wheel speed via the Hall sensor, 60 → 10 km/h takes 4.5 min.
4. Fit `M_Rad(ω) = −J_Rad · dω/dt` as the model `A₁ + B₁·ω²` over the whole spin-down —
   do not differentiate pointwise. The firmware uses the **integral method** for this:
   `ω(t) = ω₀ − (A₁/J)·t − (B₁/J)·∫ω²dt` is linear in ω₀, A₁/J, B₁/J, the fit runs
   over all pulses without forming differences, residual and number of support points are shown in the
   console (`wheel.cpp`, [12-methodology-review](12-methodology-review.md) W8).
5. The assistant stores A₁/B₁ itself; `w<A₁> <B₁>` only for adopting external values.
   Enter **J_Rad measured** (bifilar, §9.2) beforehand with `Y<kgm2>`, otherwise the
   firmware computes with 0.85·m·r² and ±10 %.

Without this run the wheel loss sits unseparated in the result: 0.25 Nm corresponds to
an apparent Crr of 0.00129, i.e. half of the best racing tire.

### 9.4 Reference Run with a Rigid Roller — Quantifies the Bearing Share

Open (A11). A steel or PA roller of the same size instead of the tire, same
wheel load: its hysteresis is negligible, what is measured is practically only the
load-dependent bearing share of 4.7 mNm. That turns the residual error into a correction.

## 10 What This Method Does Not Deliver

| | |
|---|---|
| **Absolute values as on the road** | The drum curvature raises the measured Crr compared to a flat surface. Our drum at 0.93 m is larger than the 0.77 m of the reference test rig, so the surcharge is smaller — the direction is known, the magnitude is not (C1 in [06-open-questions](06-open-questions.md)). |
| **Separation tire ↔ tube ↔ rim tape** | The system is measured. Whoever wants to compare the tube must keep everything else constant. |
| **Wet grip, puncture protection, wear** | Non-goals, [01-requirements](01-requirements.md) §5. |
| **The load-dependent bearing share** | 4.7 mNm = 0.65 % of the signal, until A11 (reference run with a rigid roller) is done. |
| **Tire temperature of the tread** | The AirSpy delivers the air temperature inside the tire, not the tread; the warm-up remains a time specification with a logged trace. IR sensor open (A5). |
| **Pressure offset of the AirSpy** | Relative pressure with an open absolute offset; pressure is set by gauge, the sensor shows the change during the run. |

What it **reliably** delivers in return: the comparison of two tires on the same wheel, at
the same load, the same pressure and the same temperature, with a resolution of
ΔCrr ≈ 0.0005 — that is the difference between a good and a very good
racing tire, and most decisions need no more than that.
