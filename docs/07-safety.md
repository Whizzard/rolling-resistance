# Safety

Private equipment, open construction **without a guard**, operated only by the builder
and only under supervision. No hazard assessment under occupational safety law.

## 1 Energy of the drum

J = 1.94 kg·m² (from the geometry of the Ø-905 drum as built, to be confirmed by a
spin-down test), U = 2.842 m (measured 2026-11-23). At 60 km/h (ω = 36.9 rad/s) that is
1.32 kJ.

> Until 2026-11-24 this read J = 2.07 kg·m² and U = 2.931 m from the Ø-933 design
> ([06-open-questions](06-open-questions.md) A14, resolved). The tables below are
> computed with the old values and are therefore about 7 % too high, i.e. on the safe
> side.
>
> The spin-down test of 2026-11-17 does **not** confirm J: it yields the loss torque
> *assuming* J. For J itself a spin-down with a known added mass is needed.

| v at the surface | ω | E = ½Jω² | |
|---|---|---|---|
| 30 km/h | 17.9 rad/s | 330 J | 0.09 Wh |
| **60 km/h** | **35.7 rad/s** | **1321 J** | **0.37 Wh** |
| 100 km/h | 59.5 rad/s | 3670 J | 1.02 Wh |

1321 J corresponds to 17 kg at 45 km/h. Surface speed at 60 km/h: 16.7 m/s.

## 2 Hazards and measures

Without a guard, the speed limit, balance quality and visual inspection carry the load.

| Hazard | Measure |
|---|---|
| Drum bursting or coming loose | Speed limit §5, balancing §6, visual inspection before every run. Do not stand in the plane of rotation. |
| Detached marker magnet | 2 g at 16.7 m/s = 0.28 J. Safety glasses. Check magnet screws before every run. |
| Drawing-in between drum and test wheel | No loose clothing, no long hair left open, no gloves. Do not reach into the gap. |
| Touching the drum surface while running | Defined standing position to the side, outside the plane of rotation. |
| Braking path | §3. Relays installed and measured (3.25 Nm, 368 rpm → standstill in 26 s). Free spin-down from 100 km/h takes **160 s** by comparison. |
| Detached test wheel | Positively secured, not merely clamped. |
| Electrical: 36–42 V | Fuse in the power path, insulated tools, power supply covered. |
| Overheating of motor or controller | Current limit in the controller, run not left unattended. Temperature sensor see A5. |
| Tipping of the stand | Frame anchored to the floor or sufficiently heavy. |

## 3 Emergency stop and brake

The emergency stop disconnects the power path **in hardware**. A software command does
not count.

- positively opening mushroom switch in the supply line to the controller
- reachable without tools, also while backing away
- feedback to the ESP32 for display only
- afterwards the drum coasts freely

> **As of 2026-11-23, this is how it is built:** a **manual** emergency stop via three
> relays that disconnects the motor from the controller and short-circuits it at the
> same time. Controller, ESP32 and controls stay powered. There is no feedback yet; the
> firmware detects the case from the **motor current** (§4): duty cycle present, current
> gone, rotation gone.

> **Firmware since 2026-11-25, hardware rebuilt the same day (`RELAY_BRAKE_WIRED` = true
> since then):** the emergency stop moves into the relay coil
> chain, and from it one wire runs to a transistor (2N5551) at the ESP32 (GPIO40). Hand and firmware
> are then two switches in series; either one drops the relays. The firmware can thus
> brake via the relays **above** `BRAKE_MAX_KMH`, where the controller must not
> (§3.2d), instead of letting the drum coast. Circuit [04-hardware](04-hardware.md) §9.1,
> sequence §9.4. Two conditions: driver off **before** the relays drop (N2, by software),
> and the criterion "no motor on the controller" (§4) is **muted** while the firmware
> itself has dropped the relays — otherwise every programmatic brake ends in a FAULT with
> the wrong reason. The hand emergency stop stays detected as before. The relays are
> energized whenever the firmware runs and drop only for such a stop; details in
> [04-hardware](04-hardware.md) §9.4.

**Braking is done by shorting the three phases** — circuit in
[04-hardware](04-hardware.md) §9. Measured: **25 s** from above 400 rpm. Two paths,
both in the same direction: bus gone → braked, GPIO low → braked, reset → braked.

**There is no parking brake.** A standing drum is not secured:

- After every run, wait until the drum is **visibly** stationary.
- Before working on the stand, locking wedge between frame and spoke.

The relays have been installed and measured since 2026-11-17. Free spin-down remains the
second path: from 100 km/h **160 s**, from 43 km/h about 100 s (`DRUM_LOSS_*` in
[03-parameters](03-parameters.md) §1).

### 3.0 Everything in §3.1 to §3.2c was measured on firmware 5.02

> **The controller has been running firmware 7.01 since 2026-11-17**, and there
> `foc_short_ls_on_zero_duty` = **1** is set — the true bridge short that 5.02 did not
> have. In addition, `l_battery_regen_cut_start/end` = 42.5/44 V throttles regeneration
> instead of dropping the drive.
>
> **The statement "with the power supply there is no controller brake" therefore
> applies only to 5.02.** Re-measured on 7.01 on 2026-11-23: **20 A brake current brakes
> cleanly with the power supply down to about 54 km/h** and ends in the true short (bus
> current ≈ 0). Above that, the controller faults with overvoltage at **brake entry** —
> why is explained in §3.2d from the source code. The firmware therefore brakes only
> below `BRAKE_MAX_KMH`; above that it lets the drum coast.
>
> What still holds, because it is physics and not firmware: a power supply does not
> absorb current, and the DC-link capacitors buffer only milliseconds.

### 3.1 The controller cannot brake — measured on firmware 5.02

**As of 2026-11-17: on firmware 5.02 there is no braking path through the controller.**
Demonstrated three times, with the drum mounted and a 40 V power supply:

| Method | Result |
|---|---|
| Duty cycle 0 from 43 km/h | Bus 40 → **55.5 V**, fault code 1 (overvoltage), drop-out |
| Duty cycle 0 from 30.7 km/h, regeneration limit 1 A | Bus 40 → **55.8 V**, fault code 1, drop-out |
| Controlled speed ramp-down from 41 km/h | Bus 40 → **53.3 V** |

In all three cases the drum hardly slowed down beforehand — at 30.7 km/h from 180 to
177 rpm in 0.6 s, then it was over.

**Why this was misjudged before.** The assumption was: `SET_DUTY(0)` switches all three
half-bridges to the same duty cycle, i.e. a bridge short, energy stays in the copper.
**That holds for BLDC operation, not for FOC.** In field-oriented operation, duty
cycle 0 is a *controlled operating point*: the controller draws brake current to reach
it, and that current goes into the bus. The true permanent short of the low-side
switches is done by `foc_short_ls_on_zero_duty` — and that exists only from
**firmware 6.05**.

A first test **without** the drum showed no voltage rise and was misleading: without the
1.94 kg·m² there is simply too little energy involved. That was not proof, just too
small a test.

**The regeneration limit does not help.** Pulling `l_in_current_min` down from 3 to 1 A
changed nothing: only 0.35 A flowed back, and the bus was still at the limit after
0.6 s. The controller's DC-link capacitors buffer nothing, and a power supply does not
absorb current.

### 3.2 What follows from this

**There are exactly two ways to slow the drum:**

1. **Free spin-down.** Measured 2026-11-17: 97 → 29 km/h in 140 s, from 43 km/h to
   standstill about 100 s. Loss torque `DRUM_LOSS_*` in [03-parameters](03-parameters.md) §1.
2. **Shorting the three phases via the relays** ([04-hardware](04-hardware.md) §9).
   Measured brake torque 3.25 Nm, i.e. three times what the controller could deliver at
   all with 3 A (0.389 Nm/A · 3 A = 1.17 Nm).

**The relays are therefore not an accessory but the only brake.** Until they are
installed and verified: no run beyond the speed whose free spin-down can be waited out.

Two options would be open if a controller brake is wanted: **firmware ≥ 6.05**
(brings `foc_short_ls_on_zero_duty` and `l_battery_regen_cut_start/end`) or a
**battery instead of the power supply** to absorb the energy. Neither decided.

### 3.2a With a battery the controller brakes — measured 2026-11-17

On the same day the 40 V power supply (5 A maximum) was swapped for a **battery pack**,
40.55 V open-circuit, presumably 10S Li-ion. This reverses the picture from §3.1: the
battery **absorbs the energy**, and `SET_CURRENT_BRAKE` works.

| Brake current | from | Duration | Bus rise | Fault |
|---|---|---|---|---|
| 3 A | 25.5 km/h | 30 s | **+0.4 V** | none |
| 20 A | 51.2 km/h | 22.7 s | **+0.3 V** | none |

With the power supply, the bus rose by **+15 V** at comparable speed until drop-out. The
difference is not the controller but what hangs on the bus.

**The controller brakes at 20 A as hard as the relays:** 6.2 Nm at 255 rpm against
5.7 Nm for the short via the relays, and practically the same curve over the whole
range. **This shifts the purpose of the relays**: they are no longer the *stronger*
brake but the **independent** one — the path that also works if firmware, controller
or computer fail. That remains their value.

> **Below 5 km/h, torque values from the controller speed are unusable.** At 0.2 s
> spacing the Hall resolution is no longer sufficient there; the log showed 7.9 Nm at
> 18 rpm, which is arithmetic noise.

### 3.2b Brake current curve — and why a large brake current is friendly to the power supply

Recorded as a series on 2026-11-17: for each step, braked from 50 km/h with
`SET_CURRENT_BRAKE`, time to 5 km/h and energy balance measured. **911 J** were
available.

| Brake current | 50 → 5 km/h | M @ 250 rpm | M @ 150 rpm | Bus rise | Energy |
|---|---|---|---|---|---|
| 3 A | 31.4 s | 2.04 Nm | 2.56 Nm | +0.40 V | **+487 J** (53 %) |
| 6 A | 19.6 s | 3.27 Nm | 2.54 Nm | +0.70 V | +409 J (45 %) |
| 10 A | 14.9 s | 4.90 Nm | 4.03 Nm | +0.50 V | +223 J (24 %) |
| 15 A | 13.9 s | 7.39 Nm | 6.97 Nm | +0.60 V | **+36 J** (4 %) |
| 20 A | 13.1 s | — | 6.53 Nm | +0.40 V | **−140 J** |

**Braking power and recovery are mutually exclusive.** At 20 A the sign is negative:
the controller *drew* energy from the battery in order to brake that hard.

The reason lies in how `SET_CURRENT_BRAKE` works (corrected 2026-11-23 from the source
code, §3.2d): the controller **ends** in the short when the setpoint is above the short-
circuit current — it does not **start** there, but crosses the regeneration zone on
entry.

- Setpoint **below** the short-circuit current → it has to *regulate it down*, and that
  is only possible via the bus → **regeneration**.
- Setpoint **above** the short-circuit current → it has to *drive additional current*,
  and that **costs** bus power → no return flow.

> **From this it was first concluded that braking with the power supply is allowed
> from 15 A upwards. This conclusion was REFUTED by measurement on 2026-11-17.** With
> the power supply and a setpoint of 20 A from 40 km/h: **fault code 1 after 0.61 s.**
> The input current showed just −0.11 A, the bus display 40.2 V — but the polling ran at
> 5 Hz, and a regeneration surge is a matter of milliseconds. Between two samples the
> bus was above `l_max_vin`.
>
> **The error in reasoning:** the energy balance is a **sum over the whole process**. At
> high speed the setpoint is *below* the short-circuit current, there the controller
> **regenerates**; only at low speed does it draw, and net draw remains. The battery
> swallows the initial surge, the power supply does not. A sum says nothing about the
> most dangerous instant of a process.

### 3.2c Status after four tests

| Method | Power supply | Battery |
|---|---|---|
| `SET_DUTY(0)` | **overvoltage** | — |
| Controlled speed ramp-down | **overvoltage** | — |
| `SET_CURRENT_BRAKE` 20 A | **overvoltage** | works |
| `SET_CURRENT_BRAKE` 3 … 20 A | — | works, 13–31 s from 50 km/h |

**With the power supply there is no controller brake.** What remains is free spin-down
or the short-circuit relays. **With a battery** the controller brakes at 15–20 A as hard
as the relays, but controllably and continuously.

**Decision of the builder (2026-11-17): the fastest possible stop is always used**,
never a slow one. Recovery is therefore not a selection criterion — it happens or it
does not. There is therefore only **one** brake current, `VESC_BRAKE_CURRENT_STOP` =
20 A.

It follows for the energy source: **the battery is not an accessory but a prerequisite
for a fast, controllable stop.** With the power supply only the relay path would remain
for that, and that is an all-or-nothing switch without intermediate steps.

> `l_max_vin` = 43.5 V did exactly what it was set for in the test: dropped out early
> instead of at the hardware limit of 57 V. The limit stays.
>
> **The undervoltage cutoff must follow the source.** For the 10S pack, 32/30 V belong
> there; the power supply, however, sags to **33.8 V** under load and would thereby
> throttle itself. When changing the source, `l_battery_cut_start/end` must be carried
> along — for the power supply 20/18 V. [TBD: add to a checklist]

> The energy figures stand on weaker ground than the torques. In braking operation the
> input current is a **computed** quantity of the controller, not a measurement; during
> the 20 A run it stood at exactly 0.00 A throughout. A reliable balance needs a current
> clamp or the battery's own meter. [TBD]

**Protection limits for battery operation**, set on 2026-11-17:

| | on 5.02 | **now (7.01)** | Reason |
|---|---|---|---|
| `l_battery_regen_cut_start` / `_end` | *does not exist* | **42.5 / 44 V** | **The actual protection limit.** Throttles regeneration instead of dropping out. 42.5 V is above the end-of-charge voltage of a 10S pack (42.0 V) and above the open-circuit voltage of the power supply. |
| `l_max_vin` | 43.5 V | **48 V** | hard fault case, deliberately **above** the throttling — throttle first, then drop out. On 5.02, 43.5 V was necessary because there was no throttling and the BMS would otherwise have been the only protection. A BMS that **disconnects** under regeneration lets the controller suddenly feed into an open circuit — the case that destroys power stages. |
| `l_battery_cut_start` / `_end` | 32 / 30 V | **32 / 30 V** | 3.20 / 3.00 V per cell at 10S. Factory settings were 10 / 8 V, i.e. ineffective. |
| `l_in_current_min` | −10 A | **−10 A** | Regeneration current. Higher only if the pack's charge current limit is known. [TBD] |

> **Not yet verified:** cell count, chemistry and permissible **charge current** of the
> pack are not confirmed; the 10S is inferred from 40.55 V. A **full** pack likewise
> absorbs almost nothing — then the overvoltage returns, only with the pack as the
> victim instead of the power supply. Clarify before longer braking series.


### 3.2d What the controller really does when braking — source code FW 7.01, read 2026-11-23

Read in `mcpwm_foc.c` and `mc_interface.c` (vedderb/bldc, as of 7.01), after the
braking test from 60.9 km/h with the power supply, which produced fault 1 four times.

| Command / state | What the controller does |
|---|---|
| Motor released (`SET_CURRENT 0`) | Bridge off, currents 0. The controller nevertheless keeps measuring the phase voltages and derives `vq` and a **duty cycle** from them — at 60 km/h it reads 0.42 although nothing is being commanded (fault log: "Duty 0.419" with the motor released). |
| `SET_CURRENT_BRAKE 20 A` while running | The short-circuit branch (modulation clamped to 0, with `foc_short_ls_on_zero_duty` all low-side switches on, no bus current) is only entered if the tracked duty cycle is already below 0.001 or speed or `vq` changes sign. While running, neither is true → direct current control to −20 A. The current controller starts at `vq` = back-EMF and ramps down; in doing so the current **inevitably** crosses the zone 0 … −I_short, in which power flows into the bus. This surge lasts milliseconds and cannot be disabled by any parameter. |
| … afterwards, setpoint > I_short | Modulation clamped to 0, true short, energy stays in the copper. Measured: from 54 km/h **14–16 A** motor current at 0.05 A bus current — that is the short-circuit current itself, not the setpoint. |
| … afterwards, setpoint < I_short | Controlled regeneration into the bus, continuously. Small brake currents are therefore the dangerous ones with the power supply. |
| `SET_DUTY 0` | PI ramp (`foc_duty_dowmramp_kp/ki` = 20/400) from the current duty cycle to 0 with **up to 20 A regeneration** during the ramp. No safer than the brake current. The firmware does not send this command. |
| `l_max_erpm_fbrake`, `l_max_erpm_fbrake_cc` | Do not appear in `mcpwm_foc.c` and `mc_interface.c`, only in the BLDC code `mcpwm.c`. **No effect in FOC.** |
| Overvoltage (fault 1) | Integrator over (U − `l_max_vin`) per PWM cycle, trips from 2.4 V·cycles — i.e. after **1 to 3 cycles** at 25 kHz. The voltage value in the fault log is read by the fault thread only afterwards and does not show the peak (46.1 V, later 42.7 V for a fault that requires 48 V). |
| `l_battery_regen_cut_start/end` 42.5/44 V | Computed at 1 kHz from a 45 Hz-filtered voltage. **Ineffective** against the entry surge, effective against slow regeneration (duty cycle ramp). |
| After the fault | PWM off, motor released; for **500 ms** all control commands are discarded (`m_fault_stop_time_ms`), then the fault bit is gone. |

**Measurement points 2026-11-23, power supply on the bus, wheel loaded:** brake entry at
59.2 / 57.7 / 56.1 km/h → immediate fault 1; at 54.0 km/h clean, then short until
standstill. The energy of the surge grows roughly with the square of the speed; between
54 and 56 km/h it evidently sits right at the limit of the buffering. How high the peak
goes is **not measured** (would need an oscilloscope on the bus). [TBD]

**Consequences for the firmware:** `BRAKE_MAX_KMH` = 45 km/h — above that, release and
coast; below that, 20 A once. No re-application within `BRAKE_FAULT_HOLD_MS` after a
fault. Spin-down 60 → 45 km/h with the wheel takes about 30 s (0.47 km/h/s from the
log).

**Open options if braking from 60 km/h is wanted:** `l_max_vin` from 48 V to the
firmware default of 57 V (4.x-class hardware: 60 V switches; whether the power supply
and everything else on the bus tolerate such a peak needs to be checked) or a buffer on
the bus (battery, brake resistor with comparator). None of this is decided.

### 3.3 The emergency stop removes the drive, not the rotation

Switching off the power supply removes the **driving torque**. The drum keeps turning
afterwards and coasts freely — minutes, not seconds. As an emergency stop against "motor
keeps driving" it is effective, against "drum is still turning" it is not.

> **Pitfall when restarting, triggered on 2026-11-17.** A setpoint *below* the current
> speed is a **brake command**. If, after a stop with duty cycle 0, the drive is started
> again while the drum is still turning, the controller regenerates and drops out on
> overvoltage. Every tool that starts the drive must preset the duty cycle from the
> **measured** speed (`MOTOR_NO_LOAD_RPM_PER_V`). The same applies to a change of
> direction: wait for standstill first.

## 4 Automatic shutdown criteria (firmware)

| Criterion | Limit | Reaction |
|---|---|---|
| No speed pulses despite setpoint > 0 | `STALL_TIMEOUT_MS` = 2000 ms | FAULT, drive off |
| n_motor vs n_drum implausible | `SLIP_TOLERANCE` = 5 % | FAULT, drive off |
| Force at the load cell | `MAX_FORCE_N` = 45 N | FAULT, drive off |
| Load cell saturated or HX711 not responding | — | FAULT, drive off |
| Drum speed above limit | `MAX_DRUM_RPM` = 370 rpm (63 km/h) | FAULT, drive off |
| Motor temperature | **TBD**, see A5 | FAULT, drive off |
| **Control link lost** | `LINK_TIMEOUT_MS` = 10 s (2 s until 2026-11-23; iOS holds the single-client web server for up to 5 s with empty preconnections while the page's requests wait) | FAULT, drive **released, the drum coasts** — no brake. Until 2026-11-25 the fault was reported as "Regelung hat abgeschaltet" and braked with 20 A, because the main loop had already released the drive before the criterion ran; the link check now comes first |
| **No motor on the controller** (emergency stop triggered or motor disconnected): duty cycle > `ESTOP_DUTY_MIN` = 0.05 (until the evening of 2026-11-23 `STALL_DUTY_MIN` = 0.20, hence blind below 30 km/h), motor current < `ESTOP_IMOT_MAX_A` = 0.3 A **and** (speed < `ESTOP_SPEED_RATIO` = 0.6 × no-load speed for the duty cycle **or** drum decelerates faster than `ESTOP_DECEL_KMH_S` = 0.5 km/h/s without the setpoint decreasing). Current alone gave a false alarm at no load (0.2 A at 28 km/h); standstill alone came 25 s too late because the Hall sensors report the coasting drum (both 2026-11-23). A connected motor cannot lag behind its voltage without drawing current | `ESTOP_DETECT_MS` = 1000 ms | Drive released, message in UI and console; FAULT during a run. Since 2026-11-23. **Must be muted while the firmware itself has dropped the relays** ([04-hardware](04-hardware.md) §9.4, proposal) |
| **Emergency stop auxiliary contact** on `PIN_ESTOP_FB` | only if `ESTOP_FB_WIRED` | FAULT; **not yet wired (TBD)**. Option without an extra wire in [04-hardware](04-hardware.md) §9.1, meaningful only while the relays are dropped |
| **Control loop switched itself off** while the state machine is still in a driving phase (dropout: no controller values for 1 s, breakaway failed) | immediate, debounced by `FAULT_SETTLE_MS` | FAULT, sequence ends, guided run aborts. Since 2026-11-24: before that the state machine stayed in POINT with the drum at rest and the run waited forever |

FAULT holds as long as the drum turns and clears itself two seconds after standstill
(since 2026-11-23; previously latching with acknowledgement, which was only a nuisance at
the stand). The drive stays off until then; the reason is shown in red in the control
panel. `q` on the console still acknowledges, likewise only at standstill.

> **The dedicated drum sensor is mandatory, not redundant.** Measured 2026-11-17:
> `foc_sl_erpm` = 4000 means that above **4000 ERPM = 267 rpm = 45.4 km/h** the
> controller computes with the **flux observer** even in Hall mode, instead of with the
> sensors. The observer needs current and voltage at the motor — if the emergency stop
> relays disconnect the controller, it is blind and its speed report drifts.
>
> Observed: after an emergency stop from 550 rpm (93.7 km/h) the controller subsequently
> reported 708 rpm, i.e. 157 rpm **too much**. Only below 267 rpm did the Hall sensors
> take over again and the values became valid. Precisely in the range where the brake
> matters, the controller is therefore no source of speed.
>
> **Aggravation, confirmed 2026-11-17:** when the relay brake was triggered from
> 101 km/h, the **USB connection** to the controller dropped at the same time — the last
> sample was at the same instant, and the switch-off came only afterwards.
>
> **The emergency stop disconnects only controller↔motor, not power supply↔controller**
> (information from the builder, 2026-11-17). So the controller had power and still
> vanished from the USB bus. It therefore **reset or crashed**.
>
> *This first said it had lost its supply. That was wrong and is withdrawn.* What
> remains correct is the partial observation that suggested it: **this controller is not
> powered via USB** — with the battery disconnected it vanishes completely from the bus;
> the logic hangs on the power input. (The original VESC 4.12 powers its logic from the
> USB 5 V, this Flipsky does not. Do not generalize.)
>
> **Suspected cause**, and it already stands as a warning in
> [04-hardware](04-hardware.md) §10.4: *"Disable the driver first, then let the relays
> drop."* In the test, the phase connection was opened **while the controller was
> driving at duty cycle 0.8** and current was flowing through the windings. Opening a
> current-carrying inductive circuit produces a voltage spike — at the phase terminals
> or back into the supply via the relay coils. One of the two knocked over the processor
> in the controller. **This stresses the controller on every trigger.** [TBD]
>
> **How to check:** after the next emergency stop, query `uptime` and `faults` in the
> controller terminal. A short uptime proves the restart; the fault list names the
> reason. (No longer possible on 2026-11-17 — the firmware update erased the list.)
>
> **Since 2026-11-25 the firmware reports no speed at all (NAN) while the controller is
> gone**, instead of the last value it sent. Without drum marks a FAULT therefore stays
> until the controller reports again — a standstill nobody measured is not declared.
>
> **Regardless of the cause:** after the emergency stop the controller delivers no usable
> information. The dedicated drum sensor on the ESP32 is therefore **not the second
> source but the only one**, and the ESP32 needs its supply independent of the switched
> power path — which it had via USB-C ([04-hardware](04-hardware.md) §5). **Since
> 2026-11-25 it is fed via the 5V pin from an XL7015 buck on the bus, so this
> independence is gone** ([04-hardware](04-hardware.md) §5.1): bus off → logic off → no
> drum sensor after a power cut. The relay brake stays unaffected.
>
> Two ways: evaluate the **8 markers on the drum** on the ESP32 (independent of
> everything that happens in the power path), and/or raise `foc_sl_erpm` far enough that
> the Hall sensors stay in use over the whole range. The second changes the commutation
> at speed and must be tested. [TBD]

**On the control link.** While a run is in progress, the control computer must check in
— if the link drops, the run aborts and the drum coasts. Three decisions on this:

- What counts is a **heartbeat**, not the TCP connection: a phone that has been put
  down or has gone to sleep is noticed by TCP only after minutes. Every request to
  `/api/state` counts as a sign of life; the UI polls 5× per second.
- The criterion is **armed only after a client has been present**
  (`webui::clientSeen()`). Without a control panel, the stand can still be operated via
  the serial console alone — otherwise every console run would immediately be a fault.
- Like every other criterion, it holds until the drum stands still. A run that resumes
  by itself after a link loss as soon as the WLAN returns would be the worse property.

The status LED shows the same state: steady green = link up, blinking red = waiting.

With direct drive there is no slip; the speed comparison is a plausibility check of the
two sensors.

## 5 Clearance for higher speeds

`MAX_DRUM_RPM` = 370 rpm corresponds to 63 km/h (`V_MAX_KMH` + `OVERSPEED_MARGIN_KMH`)
at U = 2.842 m; 60 km/h is 352 rpm. The value in `config.h` is **derived** from
`V_MAX_KMH`, `OVERSPEED_MARGIN_KMH` and `DRUM_CIRCUMFERENCE_M`, not entered as a fixed
number — a change of the circumference therefore shifts a safety limit and must be
tracked here.

Raising `V_MAX_KMH` requires:

1. Drum balanced, residual imbalance documented (§6)
2. Hub connection calculated for torque and centrifugal forces
3. Runout measured
4. Braking path available (§3)
5. Stepwise run-up in 5 km/h steps, observing running and noise, keeping distance
6. Entry here with date and justification

Without these points it stays at 60 km/h.

**Entry 2026-11-25: `V_MAX_KMH` = 100 km/h, on the instruction of the builder.** Status
of the six points at that moment, as recorded by the firmware session, not verified by
measurement:

| Point | Status 2026-11-25 |
|---|---|
| 1 Drum balanced, residual imbalance documented (§6) | TBD, not documented in this repository |
| 2 Hub connection calculated | TBD |
| 3 Runout measured | TBD |
| 4 Braking path (§3) | **Available since 2026-11-25:** relay brake under firmware control for every stop (`RELAY_BRAKE_WIRED`), triggered from 35.9 km/h in the brake comparison: 2.8 km/h/s, same as the controller with 20 A ([04-hardware](04-hardware.md) §9.4). |
| 5 Stepwise run-up in 5 km/h steps | **To be done**, with distance, listening for running noise, watching `vin` |
| 6 Entry | this one |

Consequences in the firmware: `MAX_DRUM_RPM` is now 604 rpm (103 km/h), `VESC_ERPM_MAX`
9065; the default measurement speeds still end at 60 km/h, higher plateaus are added
with `h<kmh>`. At 100 km/h the drum carries 3.7 kJ (§1), 2.8 times the energy at 60 km/h.

## 6 Balancing the drum

L/D = 100/933 = 0.11, so **single-plane balancing** is sufficient.

**Target:** ISO 21940 balance quality grade G 6.3 at m = 17 kg and ω = 35.7 rad/s.

```
e_zul = G / ω     = 6.3 / 35.7       = 0.176 mm
U_zul = e_zul · m = 0.176 mm · 17 kg = 3000 g·mm
```

At radius 466 mm that is **6.4 g at the circumference**.

**Procedure.** Disconnect the drum from the motor, place the shaft on two horizontal
knife edges or bearing pairs. The heavy spot turns downward. Compensate with screws and
washers in the plywood disc, note the radius — **not on the pitch circle of the marker
magnets**, where steel disturbs the Hall sensors.

**Demonstrate the resolution.** That the drum stays in every position only means that
the friction is greater than the residual imbalance. Stick a 5 g trial weight to the
circumference: if it turns, the resolution is below 5 g. If it does not move, the
friction is too high and the result is worthless.

Smallest detectable imbalance at the circumference, `m_min = T_reib / (g · r)`:

| Support | T_reib | m_min |
|---|---|---|
| with hub motor (cogging torque) | 0.15…0.30 Nm | 33…66 g |
| sealed deep-groove ball bearings | ≈ 0.02 Nm | 4.4 g |
| open bearings or knife edges | ≈ 0.002 Nm | 0.44 g |

With the motor flanged on, the test is therefore worthless.

**Cross-check while running.** The 1st order in the revolution-synchronous averaged
torque trace is a direct measure of the residual imbalance. Record during the run-up per
§5. For the measurement itself, imbalance is not critical — 5.7 Hz at 60 km/h is far
below the natural frequency of the cradle, and the averaging cancels all
multiples of the rotation frequency.

## 7 Checklist before every run

- [ ] Emergency stop reachable and tested
- [ ] Drum visually inspected: cracks, hub, loose parts, magnet screws tight
- [ ] Test wheel secured
- [ ] Locking wedge removed
- [ ] Nothing loose in the test area, no loose clothing
- [ ] Safety glasses
- [ ] Time allowed for 3 to 6 minutes of spin-down

## 8 After every run

- [ ] Drive off, drum **visibly** come to a standstill
- [ ] Before working on the stand: locking wedge between frame and spoke

## 9 No command may repeat itself

On 2026-11-18 the drive set itself to 20 km/h by itself during **page load**. No slip of
the hand, no fault in the controller — a property of HTTP:

The UI sent commands as **GET**. A GET is considered repeatable, and iOS resends it by
itself after a connection loss. The `WebServer` of the Arduino core can, according to
its own source comment, serve only **one** client at a time; during page load something
therefore drops. The browser repeated the last command — seconds later, as `v20`.

**Two locks, not one:**

1. Commands go only as **POST** now. A repeated GET lands in `onNotFound` and shows the
   UI instead of driving.
2. Every command carries the **timestamp** the UI last saw from `/api/state`. Anything
   older than `CMD_MAX_AGE_MS` = 2000 ms is rejected with **409** and noted on the
   console as `VERWORFEN`.

> **STOP and coast are exempt from the age check.** A late STOP is harmless; a rejected
> one would be a lock against stopping, and that is exactly the wrong direction.

The general lesson: **an operating action on a machine is not idempotent.** It must not
go through a mechanism that assumes repetition is harmless.

## 10 Operating experience 2026-11-23: what prevents aborts and overvoltage

First day with load on the wheel. Everything below is measured, not assumed, and has
been incorporated into firmware and parameter list. Ordered by frequency of trouble.

| Symptom | Cause | Remedy |
|---|---|---|
| Run aborts after seconds, "control link lost" | The Arduino web server serves **one** client; iOS holds it for up to 5 s with empty preconnections. First this blocked the main loop (2.8 s), then the page's requests. | Web server in its own task; dead-man 10 s instead of 2 s; dead-man applies only to runs started from the browser. |
| Controller reports overvoltage when ramping down | Lowering the duty cycle is **regeneration** into the bus in FOC. The power supply absorbs nothing: 43.8 V, fault 1 at 40 → 15 km/h. | Setpoint below actual value means **braking**, not commanding. |
| Overvoltage when braking from 60 km/h, then tack-tack-tack | **Entry** into the brake current in FOC always crosses the regeneration zone (§3.2d); the surge grows with the square of the speed and was enough for fault 1 from 56 km/h. Afterwards the firmware re-applied every 3 s at almost the same speed — every restart failed again. At 54 km/h the brake engaged and held the true short (14–16 A, bus current ≈ 0). | Above `BRAKE_MAX_KMH` = 45 km/h, release and let coast; below that, 20 A once. Applies to STOP, to FAULT and to every descent. Duty cycle 0 is **no** way out (PI ramp with regeneration); `l_max_erpm_fbrake` only acts in the BLDC code. |
| "Speed limit" on the topmost plateau | The plateau sits at `V_MAX_KMH`; 1 km/h of overshoot was enough. | `OVERSPEED_MARGIN_KMH` = 3 km/h of headroom; an aborted no-load run still saves its model if it has three points. |
| "Drum not turning" when starting under load | Breakaway under 494 N only at duty cycle 0.139 / 8.3 A; standstill threshold 0.05 came from runs without load. | Starting window 0.16 while the drum stands, clamp with load allowance 0.08, standstill threshold 0.20. |
| Motor energized, does not turn, no fault | Hall table no longer matched after re-plugging. | After every re-plugging, re-detect the Hall table **and** check the direction of rotation ([04-hardware](04-hardware.md) §11.2). |
| Emergency stop not detected, setpoint stays | Current alone: false alarm at no load (0.2 A). Standstill alone: sensors report the coasting drum for 25 s. | No current **and** speed far below the no-load speed for the duty cycle, **or** drum slows down without current at constant setpoint. Detection in 1.2 s. Auxiliary contact on `PIN_ESTOP_FB` would be immediate and unambiguous — not yet wired. |
| Controller faults never visible | The controller clears the code after 500 ms. | Last fault stays in the control panel for 30 s, every new one on the console with voltage, current, speed; `:faults` delivers the history. |
| Page freezes, message only after refresh | A JS error in `paint()` was swallowed by the error handler. | Errors are shown in red; the page reloads when the firmware reports a different build id. |
| Overshoot when ramping up | Pure integrator, ramp ran into the target at full slope, drum lagged 3 km/h. | PI-D, anti-windup, D only beyond the ramp slope, ramp at most 1.5 km/h ahead of the drum, flattening before the target. 0.8–1.0 km/h, then ±0.2. |
| Main loop stalled 266 ms without a reader | USB-CDC blocks when the computer is not reading. | `Serial.setTxTimeoutMs(20)`. Open: with telemetry windows over a minute, USB output pauses intermittently; loop and drive keep running. |
| Main loop stalled 980 ms at the end of a baseline point, control loop dropped out ("no controller values for over 1 s"), state machine hung in POINT (2026-11-24) | The point record and the angle bins are over 30 single `print()` calls; with nobody reading USB each one waited the 20 ms timeout. | Transmit buffer 8 kB and timeout 0: with a reader the buffer never fills, without one output is dropped. The status line (`i`) now names the longest loop section. The state machine treats a stopped control loop in a driving phase as a FAULT (§4). |

**The physics behind it:**

- A connected motor always draws current under a duty cycle; only a disconnected one can
  lag behind the voltage without current. The emergency stop detection relies on this.
- Braking with current ≥ short-circuit current keeps the energy in the copper; smaller
  brake currents and any lowering of the duty cycle push it into the bus.
- During acceleration J·dω/dt sits in the measured torque; during settling it
  oscillates: loss power and Crr only at constant speed, averaged (`STEADY_*`,
  `LIVE_WINDOW_MS`).
- Hall table and direction of rotation are properties of the wiring, not of the motor.

## Breakaway and direction detection (2026-11-26)

As of 2026-11-26. The slow start (`x`, and the direction detection `ed` / Setup →
*Detect drive direction*) raises the duty cycle at `BREAKAWAY_RATE_PER_S` up to at most
`BREAKAWAY_DUTY_MAX` and lets go as soon as the drum moves (`BREAKAWAY_DETECT_KMH`) or the
cell changes by `DIRDETECT_MIN_N`. **Found and fixed on 2026-11-26:** STOP did not end
this slow start — the duty cycle went on rising on the next cycle. `speedctl::stop()`
and `coast()` now end it. Not yet tested on the stand.

**Crawl in both directions (2026-11-27 to 2026-11-28), removed.** The automatic zero ran the
drum at 10 km/h forwards and backwards (`speedctl::crawl`, limits `CRAWL_*`). It was dropped
with the direct calibration on 2026-11-28 before it ran on the stand; `crawl()` and the
`CRAWL_*` and `AZ_*` limits are gone. Zeroing and calibration read the cell at rest, the
motor does not turn for them.

