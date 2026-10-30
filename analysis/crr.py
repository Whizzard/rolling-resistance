#!/usr/bin/env python3
"""Auswertung der Prüfstands-Rohdaten: Crr(v) und P_Reifen(v).

Verfahren (siehe docs/02-messkonzept.md):

1. BASELINE- und MEASURE-Punkte trennen.
2. Die Leerlaufkennlinie wird in zwei Läufen gefahren, vor und nach der Messung.
   Innerhalb eines Laufs wird über die Geschwindigkeit interpoliert, zwischen den
   beiden Läufen linear über die Zeit — die Lagerreibung driftet mit der Temperatur.
3. Punktweise subtrahieren, durch die Auflast teilen.

Abbruch, wenn Vor- und Nachlauf zu weit auseinanderliegen: dann hat sich während
der Messreihe etwas verändert und das Ergebnis ist nicht interpretierbar.
"""

import argparse
import csv
import math
import re
import sys

# Maximal zulässige Abweichung zwischen Vor- und Nachlauf der Leerlaufkennlinie,
# bezogen auf F_roll. Siehe docs/05-pruefablauf.md §7.
BASELINE_DRIFT_LIMIT_N = 0.10


class Meta:
    """Geometrie aus dem Kopf der CSV-Datei."""

    def __init__(self):
        self.U = None
        self.r = None
        self.L = None
        self.n_per_count = None

    @property
    def complete(self):
        return None not in (self.U, self.r, self.L)


def parse_file(path):
    meta = Meta()
    rows = []
    header = None

    with open(path, newline="", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#"):
                for key, attr in (("U", "U"), ("r", "r"), ("L", "L"),
                                  ("N_per_count", "n_per_count")):
                    m = re.search(rf"\b{key}=([0-9.eE+-]+)", line)
                    if m:
                        setattr(meta, attr, float(m.group(1)))
                continue
            if header is None:
                header = next(csv.reader([line]))
                continue
            values = next(csv.reader([line]))
            if len(values) != len(header):
                continue
            rows.append(dict(zip(header, values)))

    return meta, rows


def fnum(row, key):
    """Zahl aus einem CSV-Feld; leere Felder werden zu None, nicht zu 0."""
    value = row.get(key, "")
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def split_baseline_runs(points):
    """Baseline-Punkte in Läufe zerlegen.

    Ein neuer Lauf beginnt, sobald die Geschwindigkeit wieder fällt — die
    Firmware fährt die Plateaus immer aufsteigend ab.
    """
    runs = []
    current = []
    last_v = None
    for p in points:
        if last_v is not None and p["v"] < last_v - 1.0:
            runs.append(current)
            current = []
        current.append(p)
        last_v = p["v"]
    if current:
        runs.append(current)
    return runs


def interp_over_v(run, v):
    """Lineare Interpolation von F_roll über die Geschwindigkeit innerhalb eines Laufs."""
    pts = sorted(run, key=lambda p: p["v"])
    if not pts:
        return None
    if v <= pts[0]["v"]:
        return pts[0]["f_roll"]
    if v >= pts[-1]["v"]:
        return pts[-1]["f_roll"]
    for a, b in zip(pts, pts[1:]):
        if a["v"] <= v <= b["v"]:
            if b["v"] == a["v"]:
                return a["f_roll"]
            w = (v - a["v"]) / (b["v"] - a["v"])
            return a["f_roll"] + w * (b["f_roll"] - a["f_roll"])
    return pts[-1]["f_roll"]


def check_baseline_drift(runs):
    """Vor- und Nachlauf vergleichen. Gibt die größte Abweichung zurück."""
    if len(runs) < 2:
        return None
    first, last = runs[0], runs[-1]
    worst = 0.0
    worst_v = None
    for p in first:
        other = interp_over_v(last, p["v"])
        if other is None:
            continue
        d = abs(other - p["f_roll"])
        if worst_v is None or d > worst:
            worst, worst_v = d, p["v"]
    return worst, worst_v


def baseline_at(runs, v, t):
    """Leerlauf-F_roll bei Geschwindigkeit v und Zeit t.

    Innerhalb eines Laufs über v, zwischen den Läufen linear über die Zeit.
    """
    values = [(sum(p["t"] for p in run) / len(run), interp_over_v(run, v))
              for run in runs if run]
    values = [(tm, val) for tm, val in values if val is not None]
    if not values:
        return None
    if len(values) == 1:
        return values[0][1]

    values.sort(key=lambda x: x[0])
    if t <= values[0][0]:
        return values[0][1]
    if t >= values[-1][0]:
        return values[-1][1]
    for a, b in zip(values, values[1:]):
        if a[0] <= t <= b[0]:
            if b[0] == a[0]:
                return a[1]
            w = (t - a[0]) / (b[0] - a[0])
            return a[1] + w * (b[1] - a[1])
    return values[-1][1]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="Rohdatendatei aus data/")
    ap.add_argument("--plot", action="store_true", help="Diagramm anzeigen (matplotlib)")
    ap.add_argument("--force", action="store_true",
                    help="Auswertung trotz zu großer Baseline-Drift erzwingen")
    args = ap.parse_args()

    meta, rows = parse_file(args.csv)

    if not meta.complete:
        sys.exit("Fehler: Geometrie (U, r, L) steht nicht im Dateikopf. "
                 "Stammt die Datei von dieser Firmware?")
    if not meta.n_per_count:
        sys.exit("Fehler: Die Wägezelle war unkalibriert (N_per_count=0). "
                 "Es liegen nur Rohwerte vor, aus denen sich kein Crr ergibt. "
                 "Kalibrierung: docs/05-pruefablauf.md §2.")

    baseline, measure = [], []
    for row in rows:
        m_nm = fnum(row, "m_Nm")
        v = fnum(row, "v_kmh")
        t = fnum(row, "t_ms")
        if None in (m_nm, v, t):
            continue
        point = {
            "t": t / 1000.0,
            "v": v,
            "f_roll": m_nm / meta.r,
            "p_w": fnum(row, "p_W"),
            "load": fnum(row, "load_N"),
            "bar": fnum(row, "p_tire_bar"),
        }
        note = (row.get("note") or "").strip()
        if note == "baseline":
            baseline.append(point)
        elif note == "measure":
            measure.append(point)

    if not baseline:
        sys.exit("Fehler: keine BASELINE-Punkte in der Datei. Ohne Leerlaufkennlinie "
                 "sind die Werte nicht auswertbar (docs/02-messkonzept.md §3.2).")
    if not measure:
        sys.exit("Fehler: keine MEASURE-Punkte in der Datei.")

    runs = split_baseline_runs(baseline)
    print(f"Baseline-Läufe: {len(runs)}  ({', '.join(str(len(r)) for r in runs)} Punkte)")

    if len(runs) < 2:
        print("WARNUNG: nur ein Baseline-Lauf. Eine Temperaturdrift der Lagerreibung "
              "lässt sich damit nicht erkennen (docs/05-pruefablauf.md §7).")
    else:
        worst, worst_v = check_baseline_drift(runs)
        if worst_v is None:
            print("WARNUNG: Vor- und Nachlauf haben keine vergleichbaren Stützstellen.")
            worst = 0.0
        else:
            print(f"Größte Abweichung Vor-/Nachlauf: {worst:.3f} N bei "
                  f"{worst_v:.1f} km/h (Grenze {BASELINE_DRIFT_LIMIT_N:.2f} N)")
        if worst > BASELINE_DRIFT_LIMIT_N and not args.force:
            sys.exit("Abbruch: Die Leerlaufkennlinie ist zwischen Vor- und Nachlauf zu "
                     "stark gedriftet. Messreihe verwerfen oder mit --force erzwingen.")

    print()
    print(f"{'v':>7} {'F_roll':>9} {'F_leer':>9} {'F_reif':>9} "
          f"{'P_reif':>9} {'Crr':>9}")
    print(f"{'km/h':>7} {'N':>9} {'N':>9} {'N':>9} {'W':>9} {'':>9}")
    print("-" * 58)

    results = []
    for p in sorted(measure, key=lambda x: x["v"]):
        base = baseline_at(runs, p["v"], p["t"])
        if base is None:
            continue
        f_tire = p["f_roll"] - base
        v_ms = p["v"] / 3.6
        p_tire = f_tire * v_ms
        crr = f_tire / p["load"] if p["load"] else float("nan")
        results.append((p["v"], crr, p_tire))
        print(f"{p['v']:7.1f} {p['f_roll']:9.3f} {base:9.3f} {f_tire:9.3f} "
              f"{p_tire:9.2f} {crr:9.5f}")

    if not results:
        sys.exit("Keine auswertbaren Punkte.")

    valid = [c for _, c, _ in results if not math.isnan(c)]
    if valid:
        print()
        print(f"Crr Mittel {sum(valid)/len(valid):.5f}  "
              f"min {min(valid):.5f}  max {max(valid):.5f}")
        print()
        print("Hinweis: Zwei systematische Abweichungen, beide unkorrigiert. Gegenüber der\n"
              "Flachbahn liegen Trommelwerte höher (Krümmung), gegenüber der publizierten\n"
              "Datenbank niedriger (größere Trommel, glatte Holzoberfläche statt Riffelblech).\n"
              "Belastbar sind Vergleiche unter identischen Bedingungen "
              "(docs/02-messkonzept.md §3.6, docs/06-offene-fragen.md C1).")

    if args.plot:
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            sys.exit("matplotlib ist nicht installiert: pip install matplotlib")
        vs = [r[0] for r in results]
        fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(7, 7))
        ax1.plot(vs, [r[1] for r in results], "o-")
        ax1.set_ylabel("Crr")
        ax1.grid(True, alpha=0.3)
        ax2.plot(vs, [r[2] for r in results], "o-")
        ax2.set_ylabel("P_Reifen [W]")
        ax2.set_xlabel("v [km/h]")
        ax2.grid(True, alpha=0.3)
        fig.suptitle(args.csv)
        fig.tight_layout()
        plt.show()


if __name__ == "__main__":
    main()
