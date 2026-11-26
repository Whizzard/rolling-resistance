#!/usr/bin/env python3
"""Console of the test stand over USB: send commands, record the replies.

The same command vocabulary as the web UI and the serial console (docs/10-firmware.md
G5). Meant for troubleshooting and for driving the stand from the computer, without a
browser:

    ~/.platformio/penv/bin/python tools/console.py i
    ~/.platformio/penv/bin/python tools/console.py j8 v15 sleep:6 c
    ~/.platformio/penv/bin/python tools/console.py --watch 30
    ~/.platformio/penv/bin/python tools/console.py --log run.txt j20 m0

Arguments are commands, in order; `sleep:<s>` waits in between, and
`until:<field><op><value>` waits until the live telemetry (command j) meets the
condition, e.g. `until:actual>=14.5` or `until:actual<=0.3` -- the telemetry is requested
again automatically for this, for at most --max seconds (default 300). After the
last command reading continues for --tail seconds (default 2, with `j<s>`
automatically until the telemetry has ended).

    console.py j30 v15 until:actual>=14.5 sleep:5 s until:actual<=0.3 c

Every line gets a timestamp in seconds since start; our own commands appear
with `>>>` in the transcript.

The port is searched for (/dev/cu.usbmodem*), --port fixes it. Needs pyserial,
PlatformIO's Python has it.

STOP is always `s`, coast-down `c`. Both also work while a j is running.

Fetch files from the stand's SD card (command G, only at standstill and outside a run):

    console.py --get /vmrr/2026-11-26-1542 --out data-raw/      # a session directory
    console.py --get /vmrr/wheels.md                             # one file

Chunks of 32 kB, heatshrink-compressed on the stand (command Gz, about 3.5x on raw
logs), as base64 lines with CRC32; a damaged chunk is requested again. Older firmware
without Gz gets the plain transfer (G, 16 kB chunks, about 32 kB/s).
"""
import base64
import os
import zlib
import argparse
import glob
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing — use ~/.platformio/penv/bin/python")


def hs_decode(buf, W=10, L=5):   # W, L as in firmware/lib/heatshrink/src/heatshrink_config.h
    """heatshrink decoder (LZSS, MSB-first bit stream): tag 1 = literal byte,
    tag 0 = back-reference of W bits offset-1 and L bits length-1. Trailing pad bits end it."""
    pre = 1 << W                 # the window starts as zeros: early back-references read them
    out = bytearray(pre)
    nbits = len(buf) * 8
    pos = 0
    def bits(k):
        nonlocal pos
        v = 0
        for _ in range(k):
            v = (v << 1) | ((buf[pos >> 3] >> (7 - (pos & 7))) & 1)
            pos += 1
        return v
    while pos < nbits:
        if bits(1):
            if pos + 8 > nbits: break
            out.append(bits(8))
        else:
            if pos + W + L > nbits: break
            off = bits(W) + 1; cnt = bits(L) + 1
            s = len(out) - off
            for i in range(cnt): out.append(out[s + i])
    return bytes(out[pre:])


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
    ap.add_argument('--get', action='append', default=[],
                    help="fetch a file or directory below /vmrr from the SD (repeatable)")
    ap.add_argument('--out', default='.', help="target directory for --get")
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
            d = s.read(max(1, s.in_waiting))   # what is there; read(4096) waited out the 0.1 s timeout per chunk
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

    def request(cmd, timeout=10.0):
        """Send one G command; return its =L/=G lines up to =E (or raise on =X)."""
        s.write((cmd + "\n").encode())
        got, end = [], time.time() + timeout
        while time.time() < end:
            d = s.read(max(1, s.in_waiting))   # what is there; read(4096) waited out the 0.1 s timeout per chunk
            if not d:
                continue
            rest[0] += d
            while b"\n" in rest[0]:
                ln, rest[0] = rest[0].split(b"\n", 1)
                ln = ln.decode('ascii', 'replace').rstrip("\r")
                if ln.startswith("=X"):
                    raise RuntimeError(ln[3:])
                if ln.startswith("=E"):
                    return got, ln
                if ln.startswith("=G ") or ln.startswith("=L ") or ln.startswith("=Z "):
                    got.append(ln)
                    end = time.time() + timeout
        raise RuntimeError("no answer to " + cmd)

    zmode = [True]   # heatshrink transfer (Gz); falls back to plain G on older firmware

    def unpack(lines, off):
        """Plain =G lines -> bytes, or None if a line is damaged or out of order."""
        chunk = bytearray()
        for ln in lines:
            p = ln.split(" ")
            if len(p) != 4 or int(p[1]) != off + len(chunk):
                return None
            b = base64.b64decode(p[2])
            if "%08x" % (zlib.crc32(b) & 0xffffffff) != p[3]:
                return None
            chunk += b
        return chunk

    def unpack_z(lines):
        """=Z lines (heatshrink stream in pieces) -> decompressed bytes, or None."""
        z = bytearray()
        for i, ln in enumerate(lines):
            p = ln.split(" ")
            if len(p) != 4 or int(p[1]) != i:
                return None
            b = base64.b64decode(p[2])
            if "%08x" % (zlib.crc32(b) & 0xffffffff) != p[3]:
                return None
            z += b
        return bytearray(hs_decode(bytes(z)))

    def get_file(path, size, target):
        data, off, t1 = bytearray(), 0, time.time()
        while off < size or size == 0:
            for attempt in range(5):
                try:
                    if zmode[0]:
                        try:
                            lines, eline = request("Gz%s %d 32768" % (path, off))
                        except RuntimeError as e:
                            if "below /vmrr" not in str(e):
                                raise
                            zmode[0] = False   # firmware without Gz: the path came out as "z/vmrr..."
                            out("# firmware without compression, plain transfer")
                    if not zmode[0]:
                        lines, eline = request("G%s %d 16384" % (path, off))
                except RuntimeError as e:
                    if attempt == 4:
                        raise
                    out("# retry %s @%d: %s" % (path, off, e))
                    continue
                chunk = unpack_z(lines) if zmode[0] else unpack(lines, off)
                nxt, total = (int(x) for x in eline.split()[1:3])
                if chunk is not None and nxt == off + len(chunk):
                    break
                out("# retry %s @%d (damaged chunk)" % (path, off))
            else:
                raise RuntimeError("chunk at %d failed 5 times" % off)
            data += chunk; off += len(chunk); size = total
            if not chunk:
                break
            print("\r%s  %d / %d B" % (os.path.basename(path), off, size), end="")
            sys.stdout.flush()
        print()
        os.makedirs(os.path.dirname(target) or ".", exist_ok=True)
        with open(target, "wb") as f:
            f.write(data)
        out("# saved %s (%d B, %.0f s)" % (target, len(data), time.time() - t1))

    if a.get:
        s.write(b"j0.1\n"); read_for(0.5); rest[0] = b""
        for path in a.get:
            path = "/" + path.strip("/")
            lines, _ = request("G?" + path)
            entries = [ln.split(" ", 2)[1:] for ln in lines]
            single = len(entries) == 1 and not entries[0][1].endswith("/") and \
                os.path.basename(path) == entries[0][1].rsplit("/", 1)[-1]
            if single:
                get_file(path, int(entries[0][0]), os.path.join(a.out, os.path.basename(path)))
                continue
            for size, name in entries:
                if name.endswith("/"):
                    continue
                name = name.rsplit("/", 1)[-1]
                get_file(path + "/" + name, int(size),
                         os.path.join(a.out, os.path.basename(path), name))
        return

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
