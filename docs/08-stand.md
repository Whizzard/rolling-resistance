# Stand, Drum Bearings and Wheel-Load Mechanics

Resolves the open items A3 (type of wheel load) and A4 (drum attachment) by design and
fixes the dimension chain. Everything here is **a design for building**, not measured: every
number carries its origin. Dimensions in mm, forces in N.

> **The drum diameter is a measured quantity, not a specification.** The design uses
> **D = 930 mm (estimated, 2026-10-30)** instead of the earlier 954.9 mm. From this:
> U = 2.922 m, r = 465 mm. The value is to be measured with a tape around the built drum
> (average of three measurements, [03-parameters](03-parameters.md) §1) — **only then** do U
> and r go into `config.h`. Which dimensions depend on it and which do not is in §2.

## 1 Topology

```
          Test wheel
               │ N = 417 N
        ╔══════╧══════╗   Drum, clamped at both end discs
        ║    Drum     ║
  ──┬───╫──┬───────┬──╫───┬──────────  Shaft co-rotating, stepped Ø15/Ø30
    │   ╚══╪═══════╪══╝   │
 6902     Clamping hubs  6902        Ø15 journals (M = 0), Ø30 centre (M = 21.5 Nm)
  y=−170   y=±100       y=+170
                                  └─── Flange on the shaft end
                                        │  + coupling (radially compliant)
                                   Hub motor, stator in the cradle
                                        │
                                   Cradle bearings (2 × 6902)
                                        │
                                   Lever arm L = 70 mm → load cell
```

Compared with [04-hardware](04-hardware.md) §2 (drum directly on the motor rotor) this is a
**different arrangement**, and the difference matters for the measurement:

| | Drum on the rotor (previous) | Drum on its own shaft (here) |
|---|---|---|
| Drum bearings | inside the cradle → drop out | **outside → are measured** (2 × 6902, bore 15) |
| Load on the cradle bearings | drum + motor + wheel load ≈ 650 N | motor + cradle only ≈ 60 N |
| Friction torque of the cradle bearings | 9.8 mNm (1.3 % of the racing-tire signal) | 1.2 mNm (0.16 %) |
| load-dependent share in the signal | 0 | **4.7 mNm (0.63 %)**, see §4.6 |
| Drum attachment | adapter plate onto the rotor, symmetric | flange on the shaft end |
| Buildability | rotor dimensions needed, motor not yet here | drum can be built without knowing the motor |

One variant costs 1.3 % as **hysteresis in the cradle bearing** (not correctable), the
other 0.63 % as **load-dependent bearing friction** (smooth, reproducible, measurable in a
reference run with a rigid roller, §4.6). Hence the second one here. The decision is
recorded as A10 in [06-open-questions](06-open-questions.md).

**A radially compliant coupling is a precondition.** A rigid flange connection turns the
motor bearings into a second drum bearing; the cradle then carries half the wheel load again
and the advantage is gone. The coupling transmits torque only.

## 2 Coordinates and Dimension Chain

Origin on the floor below the drum axis. **x** forward (operator side),
**y** along the drum axis, **z** up. The side frame is a **triangle of vertical mast and
diagonal**: the drum sits on the diagonal, the swing-arm pivot on the mast. The offset `a`
between the two arises by itself.

### 2.1 Independent of the drum diameter

These dimensions are fixed whether the drum measures 930 or 955 mm — the frame is
referenced to the **drum axis**, not to the surface:

| Quantity | Value | Origin |
|---|---|---|
| Drum axis above floor `H` | **550** | chosen; minimum r + 40 (tread) + 30 (clearance) = 535 at D = 930 |
| Mast (vertical), axis | x = −240 | rear edge of the triangle |
| Lever `a` = pivot → drum axis | **200** | from this the load lever balance, §5.3 |
| Diagonal: foot point | x = +232, z = 60 | 472 mm ahead of the mast foot |
| Diagonal: head point | x = −200, z = 1180 | on the front face of the mast |
| Diagonal: length / inclination | **1200 / 21.1°** | from the vertical |
| Drum shaft on the diagonal | centreline (37.3 / 564.4) | bearing block offset 40 mm rearward → shaft at (0 / 550) |
| Mast length | **1160** | z = 40 … 1200 |
| Slider (height adjustment) | **550** | travel 195, overlap 317…512 |
| Floor beams (x), 2 × | **1000** | y = ±230, x = −400 … +600 |
| Cross beams (y) with casters, 2 × | **1000** | x = −360 and +560, y = −500 … +500 |
| Footprint | 1000 × 1000 | §6.3 |
| Fork beams | y = ±100, length 950 | clamped on the pivot axis, §7.1 |

The bearing block puts the shaft centre **20 mm off the profile face**, i.e. 40 mm off the
profile centreline — the diagonal is calculated with this dimension. If your bearing block
differs, the foot point of the diagonal shifts by `Δx_foot ≈ 2.2 · Δ(offset)`.

### 2.2 Dependent on the drum diameter

| Quantity | at D = 930 | at D = 955 | Remark |
|---|---|---|---|
| Drum radius r | 465.0 | 477.5 | |
| Drum lower edge | 85.0 | 72.6 | clearance above the floor beam |
| **Contact plane** (= H + r) | **1015.0** | 1027.5 | |
| Wheel axle (= contact plane + r_Rad) | 1195 … 1390 | 1207.5 … 1402.5 | r_Rad 180 … 375 |
| Pivot (= wheel axle + 30) | 1225 … 1420 | 1237.5 … 1432.5 | §7.1 |

**The 12.4 mm difference disappears in the slider travel** (195 mm). Not a single cut
dimension changes — the pivot is simply clamped 12 mm lower. That is why the drum can be
built and measured *afterwards*.

## 3 Sketch

Side view, not to scale, dimensions in mm (contact plane for D = 930):

```
                        ┌─ Pivot 1225…1420 (wheel axle + 30)
   z=1420 ──────╥───────┴─╮         Wheel (622-57, r = 368)
                ║Slider   │  ╭────╮      Sliding weight 2 × 5 kg,
   z=1225 ──────╫─550─────┼─(      )─────┬──── outboard of the beams
                ║         └──╰────╯      │
   z=1200 ══════╩═══╗ Mast head          │  Fork beam 950, y = ±100
                ║   ╲                    │
                ║    ╲ Diagonal 1200     │
                ║     ╲  21.1°           │
   z=1015 ·······║······╲·········· Contact plane ······│·······
                ║       ╲ Bearing block  │
   z=564 ───────║────────╲── Centreline  ┼──── Shaft 40 mm behind
                ║ Mast    ╲ ╭─────────╮  │
   z=550 ───────║──1160────╲(  Drum   )──┼──── Drum axis (0 / 550)
                ║           ╲╰─────────╯
                ║            ╲
   z=85  ───────║─────────────╲─────────────── Drum bottom
   z=40  ═══════╩══════════════╩══════════════ Floor beam 1000
             x=−400   x=−240   x=+232    x=+600
             Caster    Mast    Diagonal   Caster
```

Front view (looking in +x), not to scale:

```
     y=−230     −170  −100     0    +100  +170  +230      +370    +470
        │         │     │       │      │     │      │         │       │
     ╭──┴──╮   ╭──┴──╮  │       │      │  ╭──┴──╮          ╭──┴──╮
     │Mast │   │6902 │╔═╪═══════╪══════╪═╗│6902 │          │Motor│
     │     │   │ ◘   │║ │ Drum 250      │ ║│  ◘  │   ╭──╮   │ ███ │──┐
z=550├─────┼───┤═════╪╫═╪═══════════════╪═╫╪═════╪═══╡KK╞═══╡ ███ │  │ Ø15
     │     │   │     │║ └─Clamp hub   Clamp hub  │   ╰──╯   │ ███ │──┘ 2×6902
     │     │   ╰─────╯╚═══════════════════╝╰─────╯ Coupling ╰──┬──╯ Spacing 100
     │     │      ◄─ Ø15 ─►◄── Ø30 ──►◄─ Ø15 ─►                │ Lever arm 70 ↓
     │     │   ◄────── Bearing span 340 ──────►              ══╩══ Cell 5 kg
     ╰──┬──╯                                                   ╰──┬──╯
 z=40 ══╧══════════════════════════════════════════════════════╧══ Cross beam 1000
```

The fork beams lie at y = ±100 and run **over** the drum — with 180 mm clearance at the
smallest wheel size, 375 mm at the largest. The cradle bearing block hangs on a cross arm
from the right-hand frame (§6.4), not on a post of its own: cradle axis and drum axis must
be coaxial, so they belong on the same part.

## 4 Drum Shaft and Bearings

### 4.1 Masses and load case

| Item | Mass | Origin |
|---|---|---|
| Drum core without cladding | 16.0 kg | weighed/estimated 2026-10-30 |
| Shell sheet 1.5 mm aluminium tread plate | 1.33 kg | calc: π·0.93·0.10·0.0015·2700, +12 % for the pattern |
| Screws | ≈ 0.25 kg | estimated |
| **Drum total** | **≈ 17.6 kg** | |

```
F = 417 N (wheel load) + 172 N (drum) = 589 N
```

### 4.2 A plain Ø15 shaft is sufficient

With a drum width of 100 mm the bearings move in to **180 mm**, the clamping hubs at the
end discs to **80 mm**. The load thus enters at two points that are only 50 mm from the
respective bearing:

```
M = F/2 · (180 − 80)/2 = 294.5 N · 0.050 m = 14.7 Nm
σ = M / (π·15³/32) = 44.5 MPa
```

Allowable at the clamping-hub edge (β_k = 1.5, rotating bending, C45, S = 2): **75 MPa**.
Safety factor 1.7 — **the stepped Ø15/Ø30 shaft is dropped**. It was only needed while the
drum was 250 mm wide and the bearings 340 mm apart (21.5 Nm).

| | per bearing | against |
|---|---|---|
| Radial load | 295 N | C₀ = 2.19 kN (6902) |
| L10 at 342 rpm | 2558 million revolutions | ≈ 125 000 h |

→ **Shaft Ø15 h6, C45 bright, approx. 350 mm long**, 2 × 6902-2RS, bearing span 180,
clamping hubs Ø15 at both end discs, 80 apart.

### 4.3 The tread plate is a metrological gain

The reference test stand of the published Crr database runs on **diamond plate**, not on
smooth wood. In [06-open-questions](06-open-questions.md) C1 the surface is therefore listed
as a systematic downward deviation. With the tread plate this part of C1 largely
disappears; what remains is the curvature difference (our 0.93 m against 0.77 m there).

### 4.4 What the cladding changes at the circumference

| | D | U | n at 60 km/h |
|---|---|---|---|
| Core alone | 930.0 | 2.9217 m | 342.3 rpm |
| + 2 × 1.5 mm sheet | 933.0 | 2.9311 m | 341.2 rpm |
| + pattern ≈ 1 mm | 935.0 | 2.9374 m | 340.4 rpm |

> **Built and measured (2026-11-23): U = 2.842 m, Ø 904.6 mm**, 352 rpm at 60 km/h.
> The table above is the plan for Ø 933 and stays as a worked example.

**U is measured over the finished surface**, not calculated from the core diameter.
The tape rests on the tears of the tread plate; the tire sinks a little into the pattern, so
the effective radius is marginally below. This uncertainty is small at ≈ 0.1 % and
cancels in A/B comparisons — for absolute values it belongs in the error budget.

> `MAX_DRUM_RPM` must follow: at U = 2.93 m, 60 km/h corresponds to **341 rpm** instead of
> the 333 from [03-parameters](03-parameters.md). This is a safety limit and has to be
> carried over to [07-safety](07-safety.md) §4 — with 333 the stand would otherwise stop
> at 58.4 km/h.

### 4.5 Flywheel mass

```
J = ½·16·0.452²  (core as disc)  +  1.33·0.452²  (shell as ring)
  = 1.63 + 0.27 = 1.94 kg·m²        (calculated for Ø 905 — spin-down test decides)
E = ½·J·ω² = 1320 J at 60 km/h (ω = 36.9 rad/s)
```

The 1.3 kJ from [07-safety](07-safety.md) §1 thus remain the right order of
magnitude. The ramp torque at 0.5 km/h/s is 0.60 Nm — almost as large as the tire
signal, so measurement stays on plateaus; the residual acceleration on the plateau has
been subtracted per point since 2026-11-24 ([09-measurement-theory](09-measurement-theory.md)
§1.7).

### 4.6 The sheet carries itself — the screws only hold its position

```
Circumferential stress in the sheet = ρ·v² = 2700 · 16.67² = 0.75 MPa
```

Against R_p0.2 ≈ 100 MPa that is nothing: a rotating sheet pulls itself taut at speed by
itself; the outward centrifugal pressure is only 2.4 kPa. What has to carry load is the
**seam**: 113 N of tangential force must be transferred there (σ·t·b). Two points:

1. **Screws countersunk and outside the contact-patch width.** A protruding head strikes
   the tire once per revolution. With a drum width of 100 mm and a maximum tire width of
   57 mm (±28.5), a row of screws at **y = ±40** leaves 11.5 mm to the widest tire — and
   is countersunk anyway.
2. **Balancing.** An overlap of 30 × 100 × 1.5 mm is 12 g at r = 0.465:

| Residual imbalance | Centrifugal force at 341 rpm | Share of the static load |
|---|---|---|
| 12 g (unbalanced, seam) | 7.2 N | 1.2 % |
| 2 g (balanced) | 1.2 N | 0.2 % |

The seam also produces a disturbance **once per revolution**. That is exactly the reason
for index-triggered, revolution-synchronous averaging (M2 in
[01-requirements](01-requirements.md)) — and evaluated angle-resolved it becomes a
diagnostic rather than an error ([09-measurement-theory](09-measurement-theory.md) §3.8).

## 5 Torque Budget and Lever Arm of the Torque Balance

### 5.1 New design range: fast road tires only

No more MTB. The slowest tire envisaged is the **Continental Contact Urban**:
18.4 W at 4 bar, 40-622, 42.5 kg, 29 km/h
([source](https://www.bicyclerollingresistance.com/tour-reviews/continental-contact-urban),
retrieved 2026-10-30) — hence `Crr = 18.4 / (8.0 · 416.9) = 0.00552`.

| Tire | Crr | M_Reifen at r = 0.4523 | + idle + wheel |
|---|---|---|---|
| Vittoria Corsa Speed (best) | 0.0025 | 0.471 Nm | 0.98 Nm |
| good racing tire | 0.0037 | 0.698 Nm | 1.21 Nm |
| Continental Grand Prix Urban (16.9 W) | 0.0051 | 0.956 Nm | 1.47 Nm |
| **Continental Contact Urban (18.4 W)** | **0.0055** | **1.041 Nm** | **1.55 Nm** |
| **new design limit** | **0.0070** | 1.320 Nm | **1.83 Nm** |
| *old design limit (MTB)* | *0.0150* | *2.828 Nm* | *3.34 Nm* |

The design limit thus halves from 0.015 to **0.007** — 27 % margin above the
Contact Urban.

### 5.2 Consequence: the lever arm gets shorter, not longer

With L = 0.070 m the worst case including ramp (2.51 Nm) would sit at **37 %** of the
cell's full scale. Two thirds of the measuring range would remain unused, and the
FS-referenced cell errors (creep, non-repeatability, typ. 0.02 % FS) would enter in
full.

Percentages refer to the cell's full scale, and for the installed
**5 kg cell that is 49.03 N** — not 98.07 N, as stated here until 2026-11-16.

| | L = 0.040 m | **L = 0.080 m** |
|---|---|---|
| Design case, steady state (1.88 Nm) | 47.0 N / **96 %** | **23.5 N / 48 %** |
| with ramp (2.51 Nm) | 62.7 N / **128 %** | **31.4 N / 64 %** |
| ΔCrr = 0.0005 | 2.43 N | **1.22 N** |
| Cell error 0.02 % FS, as torque | 0.0004 Nm | **0.0008 Nm** = 0.11 % of the racing-tire signal |

→ **`TORQUE_ARM_M` = 0.080 m.** With 0.040 m the cell would already be loaded to
128 % of its full scale on run-up — in normal operation, not in a fault. The holes in the
lever arm (B4) are at **80.0 mm** (cell) and **160.0 mm** (calibration), both to be
measured to ±0.1 mm; at 80 mm that is 0.13 %, half the earlier budget.
`MAX_FORCE_N` = **45 N** as saturation protection (corresponds to 3.6 Nm, unchanged).

> The utilisations 48 % / 64 % / 92 % are the same as in the earlier calculation with
> a 10 kg cell at 0.040 m: **half the cell, twice the arm.** All voltages at the cell
> input ([04-hardware](04-hardware.md) §5.3) also remain unchanged, because they depend
> on the ratio, not on the absolute value. What halves is the forces.

### 5.3 Run-up time

In the design case 1.88 Nm = 67 W are needed in steady state; the motor delivers 6.96 Nm.
Run-up to 60 km/h with Contact Urban on the wheel, J = 2.38 kg·m²:

| Drive torque | Duration |
|---|---|
| 2.0 Nm | 103 s |
| 3.0 Nm | 46 s |
| **6.96 Nm (continuous torque)** | **15 s** |

### 5.4 Where the problem really lies now

The faster the tires, the larger the share that does **not** come from the tire:

| Tire | Drum aerodynamics | Wheel (bearings + spokes) |
|---|---|---|
| Vittoria Corsa Speed | 55 % | 51 % of the tire signal |
| good racing tire | 37 % | 35 % |
| Contact Urban | 25 % | 23 % |

The aerodynamics drop out via the idle baseline curve. **The wheel does not** — when
lifted off it stands still, so its bearing and spoke losses remain uncorrected in the
result:

```
0.25 Nm (estimated) / (416.9 N · 0.4523 m) = apparent Crr of 0.00133
```

That is **51 % of the best racing tire** and 23 % of the Contact Urban. For A/B comparisons
with the same wheel it cancels ([09-measurement-theory](09-measurement-theory.md) §1.2); for
absolute values it is now the largest single error — larger than anything in §4.2 of the
error budget.

It is fixed by the **wheel spin-down test**
([09-measurement-theory](09-measurement-theory.md) §9.3): bring the drum up to speed, lift
the wheel off while running, record its own spin-down, `J_Rad · dω/dt = M_Rad(ω)`.
Required for this: a Hall sensor with a magnet on the spokes (`PIN_WHEEL`) and J_Rad
from the bifilar pendulum.

## 6 Wheel Spin-Down Test: Where J Comes From

What is needed is not the **mass** but the **moment of inertia** J of the complete
wheel with tire and tube — only with it does the measured deceleration become a
torque: `M_Rad(ω) = −J · dω/dt`.

For a wheel, J is well-behaved because rim, tire and tube account for almost all of it and
all sit at the same radius. Calculated value for a 622 wheel of 2.5 kg:
`J ≈ 0.85 · m · r_Felge² = 0.206 kg·m²`.

### 6.1 What a J error costs

| Error on J | Error on M_Rad | of the racing-tire signal | as Crr |
|---|---|---|---|
| 2 % | 0.0050 Nm | 0.69 % | 0.000026 |
| 5 % | 0.0125 Nm | 1.74 % | 0.000064 |
| 10 % | 0.0250 Nm | 3.47 % | 0.000129 |

Target therefore **≤ 2–3 %**. For comparison: ΔCrr = 0.0005 is the resolution requirement
from [01-requirements](01-requirements.md) M1 — a 10 % error on J eats a quarter of it.

### 6.2 Bifilar pendulum

Chosen because at 1.2–1.4 % it is the only method that gets below the target — a
pendulum on the rim flange is at 2.9 %, spin-down with added mass at 4 %, weighing and
calculating at 3–5 %.

Hang the wheel horizontally on two strings of equal length attached at two opposite
points of the rim. Twist it a few degrees about the vertical axis, release,
time 20 oscillations:

```
J = m · g · b² · T² / (4 π² · L)
```

b = half the distance between the two suspension points (= rim radius), L = string length,
T = period.

| b | L | T | 20 oscillations | Error on J |
|---|---|---|---|---|
| 30 cm | 100 cm | 1.92 s | 38 s | 1.4 % |
| 30 cm | 150 cm | 2.35 s | 47 s | **1.2 %** |
| 15 cm | 100 cm | 3.84 s | 77 s | 1.5 % |

Conditions: strings vertical and of equal length, amplitude < 10°, **tire mounted and
inflated as in the test** — its mass is part of J. Calculate D as a cross-check; if the
two differ by more than 5 %, something is wrong.

### 6.3 Tire change: carry J along, do not remeasure

Every tire puts a different mass at the same radius. This has to be tracked:

| Mass difference | ΔJ | Share of J | Error if ignored |
|---|---|---|---|
| 50 g | 0.0055 kg·m² | 2.7 % | 0.9 % of the racing-tire signal |
| 100 g | 0.0110 kg·m² | 5.3 % | 1.8 % |
| 200 g | 0.0219 kg·m² | 10.6 % | 3.7 % |

So **measure bifilar once**, then correct arithmetically per tire:

```
J = J_ref + (m_Reifen − m_Reifen,ref) · r_Reifen²      r_Reifen = (BSD/2 + tire height/2)
```

Weighing a tire takes 30 seconds and is accurate to 1 g — the correction is thus better
than the base measurement.

### 6.4 The spin-down itself

| | |
|---|---|
| Start speed at 60 km/h circumferential | 469 rpm (ω = 49.2 rad/s) |
| M_Rad at the wheel (from 0.25 Nm at the drum) | 0.182 Nm |
| Initial deceleration | 0.88 rad/s² — in 10 s the speed drops by 84 rpm |
| Spin-down 60 → 10 km/h | **4.5 min** |
| to standstill | 29 min |

Pickup: **4 magnets on the spokes** plus a Hall sensor. With one magnet there would be
only 1.3 pulses/s at the lower end, with four 5.2 — the curve stays resolved even at 10 km/h.
Evaluation is not by pointwise differentiation but by fitting a model
`M_Rad(ω) = A + B·ω²` over the whole spin-down: A is the bearing friction,
B·ω² the aerodynamics of spokes and tire.

> **Since 2026-11-24 the fit uses the integral method:** `ω(t) = ω₀ − (A/J)·t −
> (B/J)·∫ω²dt`, linear in three unknowns, without differencing two periods;
> the firmware prints residual and number of support points ([09-measurement-theory](09-measurement-theory.md)
> §9.3). J comes measured from the bifilar pendulum (`Y<kgm2>`) or estimated as
> 0.85·m·r² — then with ±10 % instead of ±1.5 %.
>
> **Since 2026-11-22 a single magnet is installed** (`WHEEL_MARKS_PER_REV` = 1).
> The firmware computes the speed over a window of one full revolution anyway,
> so the pitch cancels — four magnets would therefore only have sped up the
> update rate, not the accuracy. For the fit over the whole spin-down the roughly
> 1.3 points/s at the lower end are enough; over 4.5 min that is still several
> hundred support points.

> **Two caveats.** First, the free spin-down measures the wheel in still air; in
> measuring operation it runs in the boundary layer of the drum — the aerodynamics are not
> exactly the same. Second, during spin-down the hub bearings carry only the wheel weight
> (25 N) instead of the wheel load (442 N); calculated, that is 0.0003 instead of 0.005 Nm
> at the wheel, negligible both times against the 0.18 Nm of aerodynamics. The dominant
> share is load-independent — which is why the method works at all.

## 7 Torque Pickup Without a Cradle

### 7.1 The arrangement

The rotor of the hub motor is connected to the main shaft by a **rigid flange** and
rotates with it. The stator is supported on the motor's own bearings on the rotor and is
held against rotation **only by the lever arm**. There is no separate cradle bearing.

```
Drum ── main shaft ── rigid flange ── rotor
                                       │  motor bearings
                                     stator ── lever arm ── tension link ── load cell
```

### 7.2 Why the motor bearing friction cancels exactly

Free-body diagrams of stator and rotor separately, steady state:

```
Stator:   −T_em + T_reib + T_arm = 0
Rotor:    +T_em − T_reib − T_last = 0      →  T_em = T_reib + T_last
substitute: T_arm = T_em − T_reib = T_last
```

The rotor–stator friction appears in both equations and cancels **exactly**. The lever
arm measures only what pulls on the drum from outside. This removes the 1.2 mNm of
cradle-bearing hysteresis from the error budget without replacement — and with it two
bearings, a bearing block and an alignment problem.

### 7.3 The tension link: shift cable instead of push rod

A rigid connection between lever arm and cell transmits a parasitic torque into the cell
when the arm rotates. At 7.5 mrad (full load, lever 40 mm, cell 0.3 mm):

| Connection | Parasitic torque | of the Contact Urban signal |
|---|---|---|
| rigid M5 screw, 20 mm | 9664 mNm | **900 %** |
| Ø3 rod, 100 mm | 250 mNm | 23 % |
| **shift cable 1.2 mm, 80 mm free** | **1.6 mNm** | **0.15 %** |

→ **Bicycle shift cable 1.2 mm (1×19), 80 mm free length.** Shift cables are designed for
minimal stretch — shifting precision depends on it; brake cables (especially 7×7) are
deliberately softer.

| | Value | Meaning |
|---|---|---|
| Metal cross-section | 0.85 mm² | |
| k_axial at 80 mm | 1272 N/mm | against 327 N/mm of the cell → no series spring |
| Breaking load | ≈ 1600 N | against 118 N at the stop, factor 13 |
| buckles at | 6.6 N | **when braking the cell sees at most 6.6 N = 6.7 % FS** |

The last row is the real gain: **the braking torque bypasses the sensor entirely.**
At 20 Nm braking torque, 500 N land in the stop and 6.6 N in the cell.
A braided textile cord, at 157 N/mm, would by contrast be soft enough to act
unintentionally as a series spring and halve the natural frequency.

**Attachment:** factory solder nipple at one end, clamp or crimp nipple at the
other. **No knot, no flat clamping screw on the bare cable** — both slip
under load. For the force that would be harmless (the arm is free, the force follows from
the torque), but it shifts the rest position and with it the set thresholds. Route the
cable **straight**: a curve shifts the line of action and thus the lever arm.

### 7.4 Preload

The cable must never go slack in measuring operation, otherwise the signal is clipped on
one side and the mean is falsified. The torque is not constant — imbalance, the seam of
the shell sheet and tire out-of-roundness modulate it once per revolution.

| Torque ripple | Required minimum preload |
|---|---|
| 10 % of the signal | 2.7 N |
| 20 % | 5.4 N |
| 30 % | 8.1 N |

→ **12 N (12 % FS), as a constant force**. **No spring** — a spring has a rate and
diverts an angle-dependent share of the torque; a weight delivers a constant force and
falls entirely into the static tare. Measuring range thus 12 % (tare) to 76 % FS in the
design case.

**Implementation: hanging.** The arm sits **below** the bracket, the load cell above it, the
tension link pulls upward. Gravity thus provides the preload itself — no pulley, no
additional friction, and the cable stays taut even when unpowered. The bracket ends up
higher and is easier to reach.

Two conditions:

- The additional weight is **bolted rigidly to the arm**, not hung on a wire.
  A hanging weight is a pendulum and brings a second natural frequency with it.
- The arm must be oriented so that the motor's reaction torque **tensions** the cable
  rather than unloading it — otherwise the cell sits at the stop at zero and goes slack
  under load.

### 7.4a The preload weight belongs inboard

For a given preload torque, `J = M·r/g` — the moment of inertia grows **linearly with the
radius** at which the weight sits. Far out you need less mass but pay more inertia:

| Radius | Required mass | J increase |
|---|---|---|
| **40 mm** | 1.22 kg | **0.0020 kg·m²** |
| 80 mm | 0.61 kg | 0.0039 kg·m² |
| 200 mm | 0.25 kg | 0.0098 kg·m² |

This feeds straight into the natural frequency. For a stator with J = 0.012 kg·m²:

| Weight at | f_n |
|---|---|
| 40 mm | **30.8 Hz** |
| 200 mm | 24.7 Hz — close to the 4th order (23.5 Hz) |

→ **Preload weight at the same radius as the cell**, i.e. 40 mm. The higher mass is the
right trade.

If the cable does go slack, it shows in the raw data as a lower clipping of the signal.
That belongs in the checklist for the first run.

### 7.5 Stops and measuring-zone detection

The cable only protects against braking. In the **pull direction** the hazard remains the
motor's peak torque: 12.6 Nm at L = 0.080 m is **157 N**, far above the full scale of the
5 kg cell (49 N).

| Radius on the lever arm | Function |
|---|---|
| 40 mm | tension link to the load cell |
| 80 mm | calibration hole |
| **200 mm** | flag for the optical sensor **and** stop screw, both directions |

The large radius multiplies all travels by five and makes adjustment practicable in the
first place:

| | Travel at radius 200 mm |
|---|---|
| Measuring operation (64 % FS) | 0.96 mm |
| Optical sensor switches (90 % FS) | 1.35 mm |
| Stop engages (120 % FS) | 1.80 mm |

**Adjustment is by load, not by gap.** Hang a mass, screw the stop screw in until the
load cell reading stops rising, lock it. This works independently of cell stiffness and
lever ratio:

| Target force | Mass at the calibration hole (80 mm) |
|---|---|
| 90 % FS (optical sensor) | 4.5 kg |
| 120 % FS (stop) | **6.0 kg** |
| *150 % FS — limit of permissible overload* | *7.5 kg, never exceed* |

> 20 kg at the 80 mm hole would be 15.7 Nm = 392 N = **400 % of full scale** and would
> destroy the cell during adjustment.

### 7.6 Why optical

Every alternative exerts a force on the lever arm and thereby falsifies exactly the signal
being measured:

| Method | Force on the arm | at r = 200 mm | of the signal |
|---|---|---|---|
| Microswitch, standard | 1.5 N | 0.30 Nm | 28 % |
| Microswitch, high-sensitivity | 0.1 N | 0.02 Nm | 2 % |
| Magnet + steel shutter | 0.1…1 N (edge force) | 0.02…0.2 Nm | 2…19 % |
| **Slotted optical sensor** | **0** | **0** | **0 %** |

An aluminium shutter is out: aluminium does not shield a static magnetic field. Steel
would shield, but pulls exactly at the switching position.

A flag with a **window** handles both directions with one sensor: light passes →
measuring zone, signal valid. Light blocked → discard, FAULT.

> Holders for the drum-mark sensors in aluminium or plastic, not steel —
> otherwise they attract the passing magnets.
