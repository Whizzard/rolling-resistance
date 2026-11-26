# Controller configuration, firmware 7.01 (2026-11-17)

State after the firmware update from 5.02 to **7.01** and after writing back every value
measured on 2026-11-17. **31 fields read back, all confirmed.**

## The XML is the authoritative backup

`mcconf.xml` / `appconf.xml` address **field names**, not byte offsets. They therefore
survive a firmware change, unlike the binaries, whose signature changes with every
version (from 2211848314 to 471314414 on the jump 5.02 → 7.01, mcconf from 458 to
493 bytes).

Read and write with VESC Tool, without the GUI:

    "VESC Tool" --offscreen --getMcConf  <file>
    "VESC Tool" --offscreen --setMcConf  <file>
    "VESC Tool" --offscreen --getAppConf <file>
    "VESC Tool" --offscreen --setAppConf <file>

The `.bin` files lie next to it only as raw evidence.

## What comes from measurements

| Field | Value | Origin |
|---|---|---|
| `si_motor_poles` | 30 | measured, 900 tacho steps over 10 revolutions |
| `foc_sensor_mode` | 2 (Hall) | sensors detected |
| `foc_hall_table__0…7` | 255, 68, 0, 34, 134, 102, 167, 255 | detected, exact 60° spacing |
| `foc_motor_r` | 0.238695 Ω | measured |
| `foc_motor_l` | 403.8 µH | measured, **13 % scatter, TBD** |
| `foc_motor_flux_linkage` | 17.2874 mWb | detected |
| `foc_observer_gain` | 3346112 | derived: g = 1e3/λ², relation checked at two data points |
| `si_gear_ratio` | 1 | direct drive |
| `si_wheel_diameter` | 0.903 m | from `DRUM_CIRCUMFERENCE_M` |

## What this firmware can do that 5.02 could not, the reason for the update

| Field | Factory | Now | Effect |
|---|---|---|---|
| `foc_short_ls_on_zero_duty` | 0 | **1** | real bridge short at duty 0: a dissipative brake **without** the bus, so also on the power supply. 5.02 did not have it, and that was the cause of every braking attempt that ended in overvoltage. |
| `l_battery_regen_cut_start` | 1000 V | **42.5 V** | throttles regeneration instead of dropping the drive |
| `l_battery_regen_cut_end` | 1100 V | **44 V** | |

**Tested on 2026-11-23** (docs/07-safety.md §3.2d): the short engages, but only
**after** the brake entry, which in FOC always crosses the regeneration zone. On the
power supply 20 A brakes cleanly up to about 54 km/h, from 56 km/h fault 1 on entry. The
regen cut is too slow for this millisecond surge (1 kHz, 45 Hz filter) and only helps
against slow regeneration. `l_max_erpm_fbrake(_cc)` acts only in the BLDC code.

## The one value that must follow the energy source

`l_in_current_max` is **4.5 A**, the limit of the power supply. **With the battery it
must go higher**, otherwise it caps the acceleration for no reason (on 2026-11-17 exactly
that stretched the run-up to 100 km/h from 40 to 60 s).

`l_battery_cut_start/end` are 32/30 V, i.e. 3.20/3.00 V per cell at 10S. On the power
supply, which sags to 33.8 V under load, that is tight; there 20/18 V belong. See
`docs/07-safety.md` §3.2c.

## Still factory default, deliberately

`s_pid_kp` 0.004 / `ki` 0.004 / `kd` 0.0001 / `s_pid_min_erpm` 900: the speed
controller is **unusably tuned** and must be adjusted against the mounted drum.
`s_pid_min_erpm` = 900 also blocks the lowest test step (10 km/h is 881 ERPM). These are
factory values of the VESC firmware, not remnants of the foreign configuration, proven on
the freshly flashed controller. See `docs/06-open-questions.md` A15.

## Hall table re-detected, 2026-11-23

After rewiring the stand the motor drew current but did not turn: ERPM stayed exactly 0
while the current on the ramp rose to 6.4 A. `hall_analyze` and `foc_openloop` through
the firmware's pass-through showed sensors and power stage in order; the mapping of Hall
state to angle no longer fitted, so Hall or phase leads were plugged differently than at
the detection on 17 Nov.

`foc_sensors_detect_apply 10` (via `tools/console.py ":foc_sensors_detect_apply 10"`)
re-detected and stored:

| | `foc_hall_table` 0…7 |
|---|---|
| 17 Nov | 255, 68, 0, 34, 134, 102, 167, 255 |
| **23 Nov** | **255, 2, 66, 33, 130, 166, 98, 255** |

`vesc-mcconf-fw7.01-2026-11-23.bin` is the configuration afterwards, raw as read from the
controller (`COMM_GET_MCCONF`); against 17 Nov only the table and the block of current
offsets (bytes 287 to 302), which the controller recalibrates at every start, differ.
`mcconf.xml` is updated in the eight table values. The file from 17 Nov stays as the
state before the rewiring.

The controller's fault log also held a `FAULT_CODE_DRV` from power-up (0 A, 40.5 V,
0 cycles), once, not active. Keep an eye on it.

**Direction of rotation:** with the new table the drum ran backwards on the first start.
The firmware's sign (`VESC_DIRECTION_SIGN`, command `e1`) has been **+1** since
2026-11-23; `m_invert_direction` in the controller stays untouched.
