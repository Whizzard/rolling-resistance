#!/usr/bin/env python3
"""Convert raw data of the test stand (.rrl) to CSV -- on the computer.

The same thing the stand does when downloading as .zip (/raw/<medium>/<file>.zip),
for files that were fetched raw or copied straight from the SD card. Format:
firmware/src/calc/rawlog.h, docs/10-firmware.md §11.

    python3 tools/rawlog.py 2026-11-25-183012.rrl            # -> 2026-11-25-183012_*.csv
    python3 tools/rawlog.py file.rrl --out evaluation/

One CSV per sensor, column t_us = microseconds from the start of recording. The values
keep the resolution in which they were recorded (load cell: raw count).
"""
import argparse
import pathlib
import struct
import sys

SYNC, EDGE, HX711, VESC, CTRL, TIRE, GAP, TEXT = range(1, 9)
PAYLOAD = {SYNC: 8, EDGE: 1, HX711: 3, VESC: 26, CTRL: 6, TIRE: 5, GAP: 4}

FILES = {
    HX711: ("loadcell.csv", "t_us,raw_counts"),
    EDGE:  ("edges.csv",    "t_us,channel,echo"),
    VESC:  ("vesc.csv",     "t_us,erpm,tacho,duty,i_motor_A,i_in_A,v_in_V,t_fet_C,t_motor_C,fault,status"),
    CTRL:  ("control.csv",  "t_us,target_kmh,duty_cmd,brake,phase"),
    TIRE:  ("tire.csv",     "t_us,p_tire_bar,t_tire_C,battery_pct"),
    TEXT:  ("events.csv",   "t_us,event,value"),
}


def fix(v, dec):
    return f"{v / 10 ** dec:.{dec}f}" if dec else str(v)


def row(ty, t, p):
    if ty == EDGE:
        return f"{t},{p[0] & 0x7F},{p[0] >> 7}"
    if ty == HX711:
        v = int.from_bytes(p, "little", signed=True)
        return f"{t},{v}"
    if ty == VESC:
        erpm, tacho, duty, imot, iin, vin, tfet, tmot, fault, status = struct.unpack("<iihiiHhhBB", p)
        return ",".join([str(t), str(erpm), str(tacho), fix(duty, 3), fix(imot, 2), fix(iin, 2),
                         fix(vin, 1), fix(tfet, 1), fix(tmot, 1), str(fault), str(status)])
    if ty == CTRL:
        target, duty, brake, phase = struct.unpack("<hhBB", p)
        return f"{t},{fix(target, 2)},{fix(duty, 4)},{brake},{phase}"
    if ty == TIRE:
        mbar, temp, batt = struct.unpack("<HhB", p)
        return f"{t},{fix(mbar, 3)},{fix(temp, 1)},{batt}"
    if ty == SYNC:
        return f"{t},sync,{struct.unpack('<I', p[4:8])[0]}"
    if ty == GAP:
        return f"{t},gap_bytes,{struct.unpack('<I', p)[0]}"
    if ty == TEXT:
        s = p.decode("utf-8", "replace").replace('"', '""').replace("\n", " ")
        return f'{t},note,"{s}"'
    return None


def decode(data):
    """Returns (header_text, [(type, t_us, payload)], ended_cleanly)."""
    if data[:4] != b"RRL1":
        raise ValueError("not an .rrl file (magic RRL1 missing)")
    n = struct.unpack("<H", data[4:6])[0]
    head = data[6:6 + n].decode("ascii", "replace")
    i, t, last, started, recs = 6 + n, 0, 0, False, []
    while i < len(data):
        ty = data[i]
        if ty == SYNC:
            if i + 9 > len(data):
                return head, recs, False
            ab = struct.unpack("<I", data[i + 1:i + 5])[0]
            if started:
                t += ((ab - last + 2**31) % 2**32) - 2**31
            started, last = True, ab
            recs.append((SYNC, t, data[i + 1:i + 9]))
            i += 9
            continue
        if ty not in PAYLOAD and ty != TEXT:
            return head, recs, False
        j, z, shift = i + 1, 0, 0
        while True:
            if j >= len(data):
                return head, recs, False
            b = data[j]; j += 1
            z |= (b & 0x7F) << shift
            if not b & 0x80:
                break
            shift += 7
        dt = (z >> 1) ^ -(z & 1)
        if ty == TEXT:
            if j >= len(data):
                return head, recs, False
            ln = data[j]; j += 1
        else:
            ln = PAYLOAD[ty]
        if j + ln > len(data):
            return head, recs, False
        last = (last + dt) % 2**32
        t += dt
        recs.append((ty, t, data[j:j + ln]))
        i = j + ln
    return head, recs, True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rrl", type=pathlib.Path)
    ap.add_argument("--out", type=pathlib.Path, default=None, help="target folder (default: next to the file)")
    a = ap.parse_args()
    head, recs, clean = decode(a.rrl.read_bytes())
    out = a.out or a.rrl.parent
    out.mkdir(parents=True, exist_ok=True)
    base = a.rrl.stem
    for ty, (name, header) in FILES.items():
        want = (TEXT, GAP, SYNC) if ty == TEXT else (ty,)
        lines = [row(r[0], r[1], r[2]) for r in recs if r[0] in want]
        (out / f"{base}_{name}").write_text(header + "\n" + "".join(l + "\n" for l in lines))
        print(f"{base}_{name}: {len(lines)} lines")
    (out / f"{base}_info.txt").write_text(head + ("end=clean\n" if clean else "end=truncated\n"))
    if not clean:
        print("File ends in the middle of a record -- everything before it is valid.", file=sys.stderr)


if __name__ == "__main__":
    main()
