# Rolling-resistance test stand for bicycle and velomobile tires

A drum test stand that measures the rolling resistance of bicycle tires the way the
published databases do, but at home, for the wheels a velomobile or recumbent actually
runs: **349 mm (Brompton), 406 mm (velomobile front, recumbent), 584 mm and 622 mm
(velomobile rear)**, at a selectable wheel load and over a wide speed range, with
metrology that is calibrated and self-checking, and a measurement run that the
firmware carries out on its own.

## Goals

| Goal | How |
|---|---|
| **Reproducible tests of my own** | Fixed procedure (SOP), reference tire before and after each candidate (A/B/A), every run leaves a report with all inputs, calibration state, scatter and an uncertainty per point |
| **Any wheel size** | Swing arm with a vertical slider for the axle height (349 to 622 mm, 12 mm thru-axle with adapter bushings), wheel centered on the drum, lifted and lowered with an eccentric lever |
| **Selectable wheel load** | Lever balance with a sliding weight; the load is weighed at the axle. 417 N (42.5 kg) is the standard of the published Crr database, 494 N is set today |
| **Wide speed range** | Direct-drive hub motor on the drum shaft, 10 to 60 km/h cleared, 100 km/h reachable after balancing and clearance of the drum |
| **Proper metrology** | Reaction torque at the motor stator via lever arm and load cell (no motor efficiency map needed); the speed does not enter the force at all. Idle-loss curve before and after each series, wheel-loss model from a spin-down, inertia correction per point, two zero checks, error budget per plateau |
| **Automated run** | ESP32-S3 (Arduino framework) runs the whole sequence: system checks, tare, check weight, baseline, warm-up, reference point, plateaus, reference point, zero check, post-run baseline, report. The operator lifts and lowers the wheel and hangs a weight; everything else runs alone |
| **Few manual steps** | Tire pressure and tire temperature come from a BLE sensor on the valve, speed from the motor controller, the report and the data go to flash and are downloaded from the web UI |

## Measuring principle in one line

The hub motor's stator is held only by a lever arm against a load cell. Its reaction
torque is the torque the drum needs to turn, and from it the tangential force at the
contact patch follows **without** any knowledge of speed:

```
F_roll = F_cell · L / r_drum
Crr    = (F_roll − F_roll,idle) / F_load
```

Rotor-stator friction and cogging torque cancel exactly. What is measured besides the
tire is the drum's aerodynamic drag and its two shaft bearings (the drum runs on its own
15 mm shaft in two pillow blocks, the motor is only flanged on) and the test wheel's own
losses. The first two are taken out by the idle-loss curve (wheel lifted, before and
after each series), the third by a spin-down model of the wheel. Details:
[docs/09-measurement-theory.md](docs/09-measurement-theory.md).

## Status

**Built, wired and calibrated; the first guided measurement run on the stand is still
to come.** Drum, direct drive, load cell, lever balance for the wheel load and the web
UI are in place; the firmware runs the guided measurement run and writes report and data
to flash. The code paths added in the methodology review of 2026-11-24 (inertia
correction, zero checks, reference point before and after the plateaus, BLE pressure
sensor, intermediate-magnet index) compile but have not been driven on the stand.

Open before the first result: wire the Hall sensor to the drum marks, measure the wheel's
moment of inertia with a bifilar pendulum, check the load-cell linearity with three
masses, weigh the wheel load. The list with status is
[docs/11-review.md](docs/11-review.md) §0.

## Key data

| | |
|---|---|
| Drum | Ø 905 mm, circumference 2.842 m (measured), 100 mm wide, 15 mm plywood with aluminium tread plate, J = 1.94 kg·m² |
| Speed | 10 to 60 km/h (59 to 352 rpm), 28.8 km/h is the reference point of the published database |
| Drive | Hoverboard hub motor 6.5″, 36 V / 350 W, direct drive, FSESC 4.20 controller over UART, phase-short relays on the emergency stop |
| Torque | 5 kg load cell on a 84 mm lever arm, HX711 at 80 SPS, calibrated with a certified 1 kg weight on a 330 mm calibration arm |
| Wheel load | Lever balance, 494 N set (50.4 kg), weighed with a hanging scale at the axle |
| Tire pressure and temperature | SKS AirSpy on the valve, read over BLE, per measurement point |
| Speed and revolutions | Motor controller (ERPM / 15 pole pairs) plus Hall sensor at 8 drum marks, optional intermediate magnet as index |
| Controller | ESP32-S3-DevKitC-1, web UI over its own WLAN `Pruefstand`, http://tire.local |
| Resolution target | ΔCrr = 0.0005 between two tires (0.21 N at 42.5 kg); error budget 0.029 N per point with all checks in place |

## Documents

| File | Content |
|---|---|
| [docs/01-requirements.md](docs/01-requirements.md) | Requirements and non-goals |
| [docs/03-parameters.md](docs/03-parameters.md) | Every constant with origin and calibration rule, mirror of `config.h` |
| [docs/04-hardware.md](docs/04-hardware.md) | Block diagram, parts, wiring, printed parts, controller, WLAN |
| [docs/05-test-procedure.md](docs/05-test-procedure.md) | The SOP: calibration blocks A0 to A5, measurement blocks B0 to B10 |
| [docs/06-open-questions.md](docs/06-open-questions.md) | Decision list |
| [docs/07-safety.md](docs/07-safety.md) | Hazards, protective measures, braking, checklist |
| [docs/08-stand.md](docs/08-stand.md) | Stand: dimension chain, drum bearings, lever balance, torque pickup, wheel inertia |
| [docs/09-measurement-theory.md](docs/09-measurement-theory.md) | Formulas, error budget, sensors, calibration, measurement matrix, procedure |
| [docs/10-firmware.md](docs/10-firmware.md) | Firmware requirements |
| [docs/11-review.md](docs/11-review.md) | Consolidated review of setup, run and methodology: status of every finding, target sequence, report format, bench-day findings; clickable prototype [11-run-prototype.html](docs/11-run-prototype.html) |

There is no 02: the earlier measurement concept was merged into 09. Firmware comments
and the web UI are in German; the documentation is in English.

## Repository layout

| Path | |
|---|---|
| `firmware/` | PlatformIO project for the ESP32-S3: state machine, guided run, VESC driver, load cell, speed sources, BLE pressure sensor, web UI (`web/index.html` is baked into the binary) |
| `analysis/` | `crr.py` evaluates a run record the same way the firmware does; `check_params.py` and `check_links.py` keep docs and code consistent |
| `hardware/` | FreeCAD generators and exports of the printed parts, wiring and schematic generators, VESC configuration backups |
| `tools/` | Host-side tools: test-stand console over USB, manual VESC console, VESC backup |
| `data/` | Averaged measurement points as CSV, unchanged (raw 80 SPS streams stay on the stand) |

## Quickstart

```sh
# WLAN password lives outside the repo
cp firmware/src/secrets.example.h firmware/src/secrets.h   # then set the password

# Build and flash (PlatformIO fetches NimBLE for the pressure sensor)
cd firmware && pio run -e esp32s3
pio run -t upload && pio device monitor -b 115200

# Evaluate a run record downloaded from http://tire.local/runs
python3 analysis/crr.py data/2026-11-24-1530.csv

# Consistency checks: every constant documented, every link valid
python3 analysis/check_params.py
python3 analysis/check_links.py
```

## Conventions

SI units internally (N, Nm, W, m, m/s, rad/s); km/h, bar and rpm only at the output.
Every physical constant lives in `firmware/src/config.h` and has a row in
`docs/03-parameters.md` with its origin. Unknown values are `TBD` with the method to
determine them, never a plausible placeholder. Working rules: [CLAUDE.md](CLAUDE.md).

## Safety

Open build without a guard, operated only by the builder and under supervision. The
drum stores 1.3 kJ at 60 km/h; there is no mechanical brake, braking goes through the
controller or the phase-short relays of the emergency stop. Read
[docs/07-safety.md](docs/07-safety.md) before switching on.

## License

MIT, see [LICENSE](LICENSE).
