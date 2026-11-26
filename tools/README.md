# Host-side tools

Not part of the firmware. They run on the Mac or laptop and talk to the motor controller
over USB with the same packet protocol as over UART, so the framing here is the same as
in `firmware/src/vesc/packet.cpp`. The Python side of it lives once, in `vescproto.py`
(CRC-16/XMODEM, frame, receiver); `python3 tools/vescproto.py` runs its self-test.

They need **pyserial**. PlatformIO's environment already has it:

```sh
~/.platformio/penv/bin/python tools/vesc_console.py /dev/cu.usbmodem3041
```

## `console.py` — test-stand console over USB

Talks to the **ESP32**, not to the controller: the same command vocabulary as the web
UI and the serial console, logged with timestamps. For debugging and driving from the
computer without a browser.

```sh
~/.platformio/penv/bin/python tools/console.py i                  # state
~/.platformio/penv/bin/python tools/console.py j8 v15 sleep:6 c   # telemetry, start, release
~/.platformio/penv/bin/python tools/console.py ":faults"          # controller terminal, passed through
~/.platformio/penv/bin/python tools/console.py --watch 30         # read only
~/.platformio/penv/bin/python tools/console.py --get /vmrr/2026-11-26-1542 --out raw/   # files from the SD
```

`--get` fetches a file or a session directory from the SD card over USB (firmware
command `Gz`, only at standstill and outside a run): 32 kB chunks, heatshrink-compressed
on the stand (`firmware/lib/heatshrink`, about 3.5x on raw logs), as base64 lines with
CRC32; a damaged chunk is requested again. 5.6 MB in 156 s (2026-11-26). The limit is the
SD card itself: it reads about 48 kB/s over SPI at 10 MHz, whatever the block size; the
compression only shortens the line. Firmware without `Gz` gets the plain `G` transfer. Then `rawlog.py` turns the `.rrl` into CSV.

Arguments are commands in order, `sleep:<s>` waits in between, `--log` writes the
capture to a file as well. `j<s>` is the firmware's live telemetry (one line every
200 ms), `:<text>` passes a text command to the controller's terminal, `><hex>` a raw
packet. Command table in [04-hardware](../docs/04-hardware.md) §14.1.

Found with it on 2026-11-23: the motor drew current but did not turn, ERPM stayed 0
while the current rose to 6.4 A. `:hall_analyze 10` and `:foc_openloop 10 300` showed
sensors and power stage in order; the fault was in the Hall table.

## `rawlog.py` — raw recordings to CSV

Converts a raw recording of the stand (`.rrl`, [10-firmware](../docs/10-firmware.md) §11)
into one CSV per sensor, exactly like the `.zip` download from `/raw` does on the stand.
For files fetched raw or copied straight off the SD card.

```
python3 tools/rawlog.py 2026-11-25-183012.rrl [--out folder/]
```

## `vesc_console.py` — drive the controller by hand

For the first trial run, before the firmware sits in between.

| Key | |
|---|---|
| ↑ / ↓ | setpoint ± 100 ERPM |
| → / ← | ± 1000 ERPM |
| space | **STOP**: drive de-energised, drum coasts |
| `0` | setpoint to zero |
| `q` | quit, stops first |

Displayed: speed, setpoint, bus voltage, motor and input current, duty, FET temperature
and the fault code in plain text.

**What makes it safe:**

- The setpoint is **only sent while the script is alive**. If it crashes or the window
  is closed, nothing more arrives and the controller shuts down after its own
  `timeout_msec`, the second, independent layer.
- **Dead-man**: 30 s without a key press and the setpoint goes to zero by itself. The
  remaining time runs as a countdown.
- On exit, also on Ctrl-C or an error, the drive is released.
- Capped at **± 2400 ERPM**, the same value as `VESC_ERPM_MAX` in `config.h`.
- Refuses to start while the controller reports a fault.

**What it is not:** an emergency stop. That one disconnects in hardware.

> With the controller's `timeout_brake_current` at 0, the factory default, "shut down"
> means **coast**, not brake. The drum then needs 3 to 6 minutes.

`--poles <n>` additionally converts to mechanical rpm. Without it there is **no**
conversion: a guessed pole count would give an invented speed.

## `vesc_backup.py` — save the configuration

Reads `mcconf` and `appconf` raw and stores them with a plain-text dump in
`hardware/vesc-backup/`. Read only. The backup from 2026-11-17 lies there.
