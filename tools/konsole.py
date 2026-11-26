#!/usr/bin/env python3
"""Console of the test stand over USB: send commands, record the replies.

The same command vocabulary as the web UI and the serial console (docs/10-firmware.md
G5). Meant for troubleshooting and for driving the stand from the computer, without a
browser:

    ~/.platformio/penv/bin/python tools/konsole.py i
    ~/.platformio/penv/bin/python tools/konsole.py j8 v15 sleep:6 c
    ~/.platformio/penv/bin/python tools/konsole.py --watch 30
    ~/.platformio/penv/bin/python tools/konsole.py --log run.txt j20 m0

Arguments are commands, in order; `sleep:<s>` waits in between, and
`until:<field><op><value>` waits until the live telemetry (command j) meets the
condition, e.g. `until:ist>=14.5` or `until:ist<=0.3` -- the telemetry is requested
again automatically for this, for at most --max seconds (default 300). After the
last command reading continues for --tail seconds (default 2, with `j<s>`
automatically until the telemetry has ended).

    konsole.py j30 v15 until:ist>=14.5 sleep:5 s until:ist<=0.3 c

Every line gets a timestamp in seconds since start; our own commands appear
with `>>>` in the transcript.

The port is searched for (/dev/cu.usbmodem*), --port fixes it. Needs pyserial,
PlatformIO's Python has it.

STOP is always `s`, coast-down `c`. Both also work while a j is running.
"""
import argparse
import glob
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing — use ~/.platformio/penv/bin/python")


def find_port(explicit):
    if explicit:
        return explicit
    ports = sorted(glob.glob('/dev/cu.usbmodem*')) or sorted(glob.glob('/dev/ttyACM*'))
    if not ports:
        sys.exit("no USB port found — is the board connected via USB?")
    return ports[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('befehle', nargs='*', help="commands, sleep:<s> in between")
    ap.add_argument('--port', help="serial port, otherwise /dev/cu.usbmodem*")
    ap.add_argument('--watch', type=float, default=0.0,
                    help="only listen, for this many seconds")
    ap.add_argument('--tail', type=float, default=2.0,
                    help="keep reading this long after the last command")
    ap.add_argument('--log', help="also append the transcript to this file")
    ap.add_argument('--max', type=float, default=300.0,
                    help="longest wait per until:, seconds")
    a = ap.parse_args()

    port = find_port(a.port)
    s = serial.Serial(port, 115200, timeout=0.1)
    t0 = time.time()
    logf = open(a.log, 'a') if a.log else None

    def out(line):
        txt = "%7.2f  %s" % (time.time() - t0, line)
        print(txt)
        sys.stdout.flush()
        if logf:
            logf.write(txt + "\n")
            logf.flush()

    last_j = {}      # most recently seen telemetry fields
    rest = [b""]

    def handle(line):
        out(line)
        if line.startswith("# J "):
            for tok in line.split()[3:]:
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    try:
                        last_j[k] = float(v)
                    except ValueError:
                        last_j[k] = v

    def read_for(seconds):
        end = time.time() + seconds
        while time.time() < end:
            d = s.read(4096)
            if not d:
                continue
            rest[0] += d
            while b"\n" in rest[0]:
                ln, rest[0] = rest[0].split(b"\n", 1)
                handle(ln.decode('utf-8', 'replace').rstrip("\r"))

    OPS = {">=": lambda a, b: a >= b, "<=": lambda a, b: a <= b,
           ">": lambda a, b: a > b, "<": lambda a, b: a < b}

    def wait_until(expr):
        m = re.match(r'^(\w+)(>=|<=|>|<)(-?\d+(?:\.\d+)?)$', expr)
        if not m:
            sys.exit("until: not understood: %s" % expr)
        key, op, val = m.group(1), m.group(2), float(m.group(3))
        t_start = time.time()
        next_j = 0.0
        while time.time() - t_start < a.max:
            if time.time() >= next_j:
                s.write(b"j30\n")
                next_j = time.time() + 25.0
            read_for(0.2)
            if key in last_j and isinstance(last_j[key], float) and OPS[op](last_j[key], val):
                out(">>> until:%s met (%s=%s)" % (expr, key, last_j[key]))
                return
        out(">>> until:%s NOT met after %.0f s — aborting, releasing" % (expr, a.max))
        s.write(b"c\n")
        sys.exit(2)

    out("# Port %s" % port)
    time.sleep(0.3)
    s.reset_input_buffer()

    if a.watch > 0 and not a.befehle:
        read_for(a.watch)
        return

    tail = a.tail
    for b in a.befehle:
        m = re.match(r'^sleep:(\d+(?:\.\d+)?)$', b)
        if m:
            read_for(float(m.group(1)))
            continue
        if b.startswith('until:'):
            wait_until(b[6:])
            continue
        s.write((b + "\n").encode())
        out(">>> " + b)
        mj = re.match(r'^j(\d+(?:\.\d+)?)?$', b)
        if mj:
            tail = max(tail, float(mj.group(1) or 5) + 0.5)
        read_for(0.3)
    read_for(tail)


if __name__ == '__main__':
    main()
