#!/usr/bin/env python3
"""Split a raw recording of the test stand (raw_<session>.csv) into one CSV per sensor.

The same thing the stand does for the CSV .zip download (/fs/<medium><file>?csv), for
files fetched raw or copied straight from the SD card. Format ("vmrr-raw" version 3):
firmware/src/calc/rawlog.h, docs/10-firmware.md §11.4.

    python3 tools/rawlog.py raw_2026-11-27-101500.csv          # -> raw_2026-11-27-101500_*.csv
    python3 tools/rawlog.py raw_2026-11-27-101500.csv --out evaluation/

The stream has one record per line, `type,t_us,fields` with a single-letter type, no
header line. Each output CSV
gets a column header row; t_us = microseconds from the start of recording; values keep the
resolution in which they were recorded (load cell: raw counts). The metadata
raw_<session>.json next to the stream (version, stand, dimensions, calibration) is read if
present and goes into <base>_info.txt with the row counts. A line cut off at the end
(power loss, file copied while recording) is skipped.
"""
import argparse
import csv
import json
import pathlib
import sys

# type letter -> fields after t_us, in file order (calc/rawlog.h columns()); L (axle load
# cells) is reserved and not written yet
FIELDS = {
    "S": ["unix_s", "micros"],
    "D": ["echo"], "I": ["echo"], "W": ["echo"], "H": ["echo"],
    "T": ["raw_counts"],
    "M": ["erpm", "tacho", "duty", "i_motor_A", "i_in_A", "v_in_V", "t_fet_C", "t_motor_C", "fault", "status"],
    "C": ["target_kmh", "duty_cmd", "brake", "phase"],
    "P": ["p_tire_bar", "t_tire_C", "battery_pct"],
    "G": ["dropped_bytes"],
    "N": ["text"],
}
EDGES = {"D": "drum", "I": "index", "W": "wheel", "H": "motor"}   # edges.csv: t_us,channel,echo
EVENTS = {"S": "sync", "G": "gap", "N": "note"}                    # events.csv: t_us,event,value
FILES = [("T", "loadcell.csv"), ("D", "edges.csv"), ("M", "vesc.csv"),
         ("C", "control.csv"), ("P", "tire.csv"), ("N", "events.csv")]


def decode(path):
    """Returns ([(type, row)], bad line numbers, last line number)."""
    raw = path.read_bytes().decode("utf-8", "replace")
    complete_end = raw.endswith("\n")
    lines = raw.splitlines()
    recs, bad = [], []
    for no, row in enumerate(csv.reader(lines), 1):
        if not row:
            continue
        ok = (row[0] in FIELDS and len(row) == 2 + len(FIELDS[row[0]])
              and row[1].lstrip("-").isdigit()
              and not (no == len(lines) and not complete_end))   # last line without \n: cut off
        if not ok:
            bad.append(no)
            continue
        recs.append((row[0], row))
    if not recs or recs[0][0] != "S":
        raise ValueError("not a raw stream (first line is not a sync record)")
    return recs, bad, len(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", type=pathlib.Path)
    ap.add_argument("--out", type=pathlib.Path, default=None, help="target folder (default: next to the file)")
    a = ap.parse_args()
    recs, bad, last = decode(a.file)
    out = a.out or a.file.parent
    out.mkdir(parents=True, exist_ok=True)
    base = a.file.stem
    counts = {}
    for kind, name in FILES:
        with (out / f"{base}_{name}").open("w", newline="") as f:
            w = csv.writer(f, lineterminator="\n")
            if kind == "N":
                w.writerow(["t_us", "event", "value"])
                rows = [[r[1], EVENTS[k], r[2]] for k, r in recs if k in EVENTS]
            elif kind == "D":
                w.writerow(["t_us", "channel", "echo"])
                rows = [[r[1], EDGES[k], r[2]] for k, r in recs if k in EDGES]
            else:
                w.writerow(["t_us"] + FIELDS[kind])
                rows = [r[1:] for k, r in recs if k == kind]
            w.writerows(rows)
        counts[name] = len(rows)
        print(f"{base}_{name}: {len(rows)} lines")
    end = "clean" if not bad else "truncated" if bad == [last] else "bad"
    meta = a.file.with_suffix(".json")
    info = ""
    if meta.exists():
        info = json.dumps(json.loads(meta.read_text(encoding="utf-8")), ensure_ascii=False, indent=2) + "\n"
    else:
        print(f"{meta.name} not found: no metadata (dimensions, calibration) in the info file", file=sys.stderr)
    info += f"source_file={a.file.name}\nsource_bytes={a.file.stat().st_size}\n"
    info += "".join(f"rows_{n}={c}\n" for n, c in counts.items()) + f"end={end}\n"
    (out / f"{base}_info.txt").write_text(info)
    if bad:
        print(f"{len(bad)} line(s) not readable (line {', '.join(map(str, bad[:5]))}"
              f"{' …' if len(bad) > 5 else ''}) -- skipped, everything else is valid.", file=sys.stderr)


if __name__ == "__main__":
    main()
