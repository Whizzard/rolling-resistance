# VESC configuration after commissioning (2026-11-17)

State **after** measuring the drive train. The original state lies next to it in
`hardware/vesc-backup/`: a **foreign configuration**, gear ratio and open-loop speed set by
hand, R/L/flux linkage from a detection on a DIFFERENT motor. Pole count (14), wheel
diameter (0.083 m) and the speed-controller gains in it were merely factory defaults,
which could only be told apart after flashing 7.01. See `docs/03-parameters.md` §4.1.

Difference to the original: 59 bytes in nine regions, every one of them a deliberate
change, none accidental.

| Region | Field | Original | Now |
|---|---|---|---|
| @9…26 | current limits | ±63.8 A motor, 99/−60 A input, 130 A absolute | 20/−20 A motor, 4.5/−1 A input, 40 A absolute |
| @54…62 | voltage limits | `l_max_vin` 57 V, cutoff 10.2/9.0 V | 43.5 V, 20/18 V |
| @154…161, @208…227 | R, L, flux linkage | foreign | measured: 238.7 mΩ, 403.8 µH, 17.287 mWb |
| @199 | `foc_sensor_mode` | sensorless | **Hall** |
| @263…268 | `foc_hall_table` | never detected (255) | 68, 0, 34, 134, 102, 167 |
| @434…442 | mechanics | 14 poles, ratio 2.77, wheel 83 mm | **30 poles**, 1.0, 0.9030 m |

In the `appconf`: `timeout_msec` 1000 → **300 ms**, `timeout_brake_current` stays
**0** (reasoning in `docs/04-hardware.md` §10.5: a latched brake state makes spin-down
tests impossible).

## Caution when restoring

The **voltage limits depend on the energy source.** They read 20/18 V here because the
power supply, which sags to 33.8 V under load, was connected last. For the 10S battery
pack they must be **32/30 V**. See `docs/07-safety.md` §3.2c.

## How to restore

Send the raw bytes from `vesc-mcconf-fw5.02.bin` or `vesc-appconf-fw5.02.bin` as payload
with the id at position 0 replaced: 13 for `COMM_SET_MCCONF`, 16 for `COMM_SET_APPCONF`.
Valid **only for firmware 5.02**: the signature changes between versions, after a
firmware change the file is no longer directly usable.
