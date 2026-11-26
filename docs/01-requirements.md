# Requirements

Legend: **M** = must, **S** = should, **K** = can (later).

## 1 Functional

| # | Prio | Requirement |
|---|---|---|
| F1 | M | The drum (circumference U = 2.842 m measured) is driven by a motor and controlled to an adjustable target speed. |
| F2 | M | Speed range 10–60 km/h at the drum surface, controlled to ±0.5 km/h. One support point lies at 28.8 km/h (reference of the published Crr database). |
| F3 | M | A wheel with the test tire is pressed against the drum with a defined, known wheel load. |
| F4 | M | The reaction torque of the drive is measured continuously. |
| F5 | M | Rolling-resistance force, power loss and Crr are computed from torque and geometry. |
| F6 | M | Measurement runs on **speed plateaus** with a hold time, not during the ramp (rationale: [09-measurement-theory](09-measurement-theory.md), pitfall 1). |
| F7 | M | The idle-loss curve P_idle(v) with the wheel lifted off is recorded and subtracted. |
| F8 | M | All measurement points are logged as CSV (serial; SD optional). |
| F8a | M | **Every measurement run records all raw sensor data as completely as possible** and keeps it for later analysis: every edge of the Hall sensors, every load-cell sample, every motor-controller values packet (speed, currents, voltage, temperatures, faults), tire pressure, target and command values, brake and phase changes, and the messages — each with a timestamp in µs. Recording starts and stops with the run on its own. Losses (full buffer, removed card) are recorded in the file itself, never silently. Raw data goes to the SD card, not the internal flash (flash writes block the sensor interrupts); without a ready card the run checklist says so. Format and download: [10-firmware](10-firmware.md) §11 (requirement 2026-11-26). |
| F9 | S | Display of v, P, Crr and state on a 4×20-character LCD. |
| F10 | S | Wheel load is entered manually; measured later. |
| F11 | K | Tire pressure is read via a BLE sensor (SKS AirSpy, since 2026-11-24); manual entry as fallback. |
| F12 | K | Tire and ambient temperature are logged alongside (tire: AirSpy in the rim bed; room: manual entry at start and end). |

## 2 Measurement

| # | Prio | Requirement |
|---|---|---|
| M1 | M | Resolution such that ΔCrr = 0.0005 between two tires is reliably distinguishable (0.21 N tangential force at 42.5 kg). Stricter still: the speed dependence is only 11.6 % between 14 and 36 km/h, which demands 1–2 % accuracy per measurement point. |
| M2 | M | Averaging always over a **whole number of drum revolutions**, triggered on the index mark. Without a wired index these are computed revolutions; the firmware reports the residual error. |
| M2a | M | The apparent torque of the net acceleration over the averaging window, `J_eff·dω/dt`, is subtracted per point ([09-measurement-theory](09-measurement-theory.md) §1.7). |
| M3a | M | The static tare is taken with the **wheel lifted off and the rotor free** and cross-checked by turning the rotor on; after the run the zero point is read again ([09-measurement-theory](09-measurement-theory.md) §5.3). |
| M3 | M | Two separate zero points: static cell zero at standstill **and** speed-dependent idle-loss curve. |
| M4 | M | Idle-loss curve before **and** after every measurement series; the analysis interpolates between the two (bearing friction changes with temperature). |
| M5 | M | No element with stiction or hysteresis in the torque path. |
| M6 | M | **30 minutes warm-up** before the first measurement, as on the commercial reference test rig; without it, comparisons are worthless. |
| M7 | S | Angle-resolved force analysis for diagnosing the drum seam and out-of-roundness. |
| M8 | S | A/B/A protocol: reference tire at the beginning and at the end of every session. |

## 3 Mechanics

These four are expensive to retrofit later and belong in the first design:

1. **Wheel can be lifted off without changing the geometry** (stop, cam or eccentric).
   Otherwise the idle-loss curve cannot be measured under identical conditions.
2. **Pivot of the wheel arm at wheel-axle height**, so that the tangential force does not
   alter the wheel load: the freely spinning wheel passes the contact forces through the
   axle, and a horizontal force at the height of the pivot has no lever arm ([08-stand](08-stand.md)
   §5.2).
3. **Torque pick-up without an additional bearing.** The stator is supported on the
   motor's own bearings and is held only via the lever arm; this cancels the
   rotor-stator friction exactly ([08-stand](08-stand.md) §7.2). Leads exit
   axially on the axle. Calibration hole at 80 mm, flag of the optical sensor
   and **end stops on both sides at 200 mm** — without them, even the peak torque
   of the motor destroys the cell. Allow space for the eddy-current damper.
4. **Route the motor Hall signals to the ESP32.** With direct drive, motor and
   drum speed are identical; the comparison with the drum marks is then a
   plausibility check that detects a failed sensor. If a belt ends up in between
   after all, this becomes the slip monitoring.

Further:

| # | Prio | Requirement |
|---|---|---|
| X1 | M | **Direct drive**: drum on the motor rotor, stator axle in pillow-block bearings **on both sides** on the cradle frame, drum symmetric to the motor plane (no bending moment on the motor bearings). If the axle cannot be supported on both sides, the drum gets its own bearings and a toothed belt. |
| X2 | M | Drum balanced; permissible maximum speed fixed in writing ([07-safety](07-safety.md)). |
| X3 | M | Load cell connected **in tension only** (shift cable, preloaded to 12 N). The sign reverses when braking and during spin-down — the cable then goes slack and the load goes into the stop, not into the cell. Rationale and dimensions: [08-stand](08-stand.md) §7.3. |
| X4 | S | Only if a belt becomes necessary after all: HTD/GT profile, not T profile, because of the excitation of the cradle part. |
| X5 | M | A retrofitted brake must be fully released during measurement operation — a drag torque drifts with temperature and destroys the idle-loss curve. The installed hoverboard motor has no built-in brake. |

## 4 Operation and Safety

| # | Prio | Requirement |
|---|---|---|
| B1 | M | Hardware emergency stop that disconnects the motor power **galvanically** — not by software. |
| B2 | M | Open construction **without a protective hood** (spec 2026-11-01). Instead: speed limit, balance quality, visual inspection, safety glasses, operator position outside the plane of rotation. |
| B3 | M | Automatic shutdown on: missing speed pulses, implausible motor/drum speed comparison, exceeding the force limit, saturated load cell. |
| B4 | M | Controlled braking path via the motor controller; the drum stores around 1.4 kJ at 60 km/h. |
| B5 | S | Operating-hours counter or log of the run-ups performed, for maintenance. |

## 5 Non-Goals

- Not a standards-compliant test rig. The absolute values are shifted systematically
  upwards relative to flat-track values by the drum curvature; what is reliable are
  **comparisons** under identical conditions.
- No measurement of cornering grip, puncture resistance, wear or wet grip.
- No automatic tire recognition or database connection.
- No continuous unattended operation.
