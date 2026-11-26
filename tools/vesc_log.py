#!/usr/bin/env python3
"""
Record the controller's readings — over USB, read-only.

    python3 tools/vesc_log.py /dev/cu.usbmodem3041 --out /tmp/vesc.csv

Polls COMM_GET_VALUES cyclically and writes every reply as a CSV line. Sends
NO setpoints; the motor cannot start from this.

What for: watching during assembly. Turn the motor by hand and see whether the speed
follows. Warm the motor and see whether the temperature rises — that shows whether a
sensor is really connected to the TMP input. Observe voltage and current at idle.

CAUTION, two properties of the protocol:

  * The current and voltage vector fields are AVERAGES SINCE THE LAST POLL, and
    reading resets the accumulator. The poll interval therefore IS the averaging
    window — it is in the file as column `dt_ms`.

  * If the VESC Tool is connected over USB at the same time, the two steal each
    other's samples. Only ever one poller.
"""
import argparse
import signal
import struct

from vescproto import frame, unframe  # noqa: E402
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing:  pip install pyserial")

COMM_GET_VALUES = 4

FAULT = {
    0: "ok", 1: "overvoltage", 2: "undervoltage", 3: "drv", 4: "overcurrent",
    5: "temp_fet", 6: "temp_motor", 7: "drv_uv_high", 8: "drv_uv_low",
    9: "mcu_uv", 10: "watchdog", 19: "brk",
}


# Fixed order of COMM_GET_VALUES, unchanged since firmware 3.x.
def parse(d: bytes):
    def i16(o): return struct.unpack_from(">h", d, o)[0]
    def i32(o): return struct.unpack_from(">i", d, o)[0]
    if len(d) < 53:
        return None
    r = {
        "tfet":   i16(0) / 10,
        "tmot":   i16(2) / 10,
        "imot":   i32(4) / 100,
        "iin":    i32(8) / 100,
        "id":     i32(12) / 100,
        "iq":     i32(16) / 100,
        "duty":   i16(20) / 1000,
        "erpm":   i32(22),
        "vin":    i16(26) / 10,
        "ah":     i32(28) / 1e4,
        "wh":     i32(36) / 1e4,
        "tacho":  i32(44),
        "tachoa": i32(48),
        "fault":  d[52],
    }
    return r


COLS = ["t_s", "dt_ms", "erpm", "vin_V", "imot_A", "iin_A", "id_A", "iq_A",
        "duty", "tfet_C", "tmot_C", "tacho", "tacho_abs", "fault", "fault_txt"]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--out", default="/tmp/vesc.csv")
    ap.add_argument("--hz", type=float, default=5.0, help="poll rate (default 5)")
    ap.add_argument("--seconds", type=float, default=0.0,
                    help="stop after this many seconds; 0 = until Ctrl-C")
    args = ap.parse_args()

    s = serial.Serial(args.port, 115200, timeout=0)
    buf = bytearray()
    period = 1.0 / args.hz
    t0 = time.time()
    last_poll = 0.0
    last_rx = t0
    n = 0

    stop = False
    def on_sig(*_):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGINT, on_sig)
    signal.signal(signal.SIGTERM, on_sig)

    with open(args.out, "w", buffering=1, encoding="utf-8") as f:
        f.write("# Readings of the motor controller, read-only. Currents are averages\n")
        f.write(f"# over dt_ms, because reading resets the accumulator.\n")
        f.write(",".join(COLS) + "\n")
        print(f"writing {args.out} at {args.hz:g} Hz — Ctrl-C stops", flush=True)

        while not stop:
            now = time.time()
            if args.seconds and now - t0 > args.seconds:
                break
            if now - last_poll >= period:
                last_poll = now
                s.write(frame(bytes([COMM_GET_VALUES])))

            data = s.read(4096)
            if data:
                buf += data
                for body in unframe(buf):
                    if not body or body[0] != COMM_GET_VALUES:
                        continue
                    r = parse(body[1:])
                    if r is None:
                        continue
                    dt = (now - last_rx) * 1000
                    last_rx = now
                    n += 1
                    f.write(f"{now - t0:.3f},{dt:.0f},{r['erpm']},{r['vin']:.1f},"
                            f"{r['imot']:.2f},{r['iin']:.2f},{r['id']:.2f},{r['iq']:.2f},"
                            f"{r['duty']:.3f},{r['tfet']:.1f},{r['tmot']:.1f},"
                            f"{r['tacho']},{r['tachoa']},{r['fault']},"
                            f"{FAULT.get(r['fault'], 'unknown')}\n")
                    if n % max(1, int(args.hz)) == 0:
                        print(f"{now - t0:7.1f}s  {r['erpm']:>7d} ERPM  "
                              f"{r['vin']:5.1f} V  {r['imot']:6.2f} A  "
                              f"FET {r['tfet']:5.1f}  Motor {r['tmot']:6.1f}  "
                              f"Tacho {r['tachoa']:>8d}  {FAULT.get(r['fault'], '?')}",
                              flush=True)
            else:
                time.sleep(0.002)

    s.close()
    print(f"\n{n} lines written to {args.out}")


if __name__ == "__main__":
    main()
