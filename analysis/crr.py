#!/usr/bin/env python3
"""Evaluation of a measurement run: Crr(v), P_tire(v), uncertainty, drifts.

Reads the RUN RECORD of the guided measurement run (firmware run.cpp, /runs/<id>.csv) and
calculates THE SAME WAY as the ESP (docs/09-measurement-theory.md §6, docs/11-review.md W9):

1. Idle-loss model A0/B0 from the header (fit of the pre-run baseline), wheel-loss model
   A1/B1 from the header.
2. Per plateau: mean of the valid points, pooled scatter over all plateaus,
   subtract the loss models, divide by the wheel load.
3. u(Crr) with k = 1 from Type A (pooled) and the Type B terms from the header.
4. Drifts: reference point before/after, baseline pre-/post-run, zero points are in the
   ESP's report (.md); here the baseline drift is recalculated and shown as a warning,
   not as an abort — the human decides, with the report.

The OLD serial CSV (columns t_ms,...,note=baseline|measure) is still read; there the
idle-loss model is fitted per baseline run and interpolated in time between the pre- and
post-run baseline. That format has no wheel-loss model.

The script never writes back into the raw file.
"""

import argparse
import csv
import math
import re
import sys

# Pre-/post-run baseline of the idle-loss curve, on F_roll. docs/09 §8.8, config.h RUN_DRIFT_LIMIT_N.
BASELINE_DRIFT_LIMIT_N = 0.02


# ---------------------------------------------------------------- reading
def parse_header(line, meta):
    for m in re.finditer(r"(\w+)=([^\s]+)", line):
        key, val = m.group(1), m.group(2)
        try:
            meta[key] = float(val)
        except ValueError:
            meta[key] = val


def parse_file(path):
    meta, rows, header = {}, [], None
    with open(path, newline="", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#"):
                parse_header(line, meta)
                continue
            if header is None:
                header = next(csv.reader([line]))
                continue
            values = next(csv.reader([line]))
            if len(values) != len(header):
                continue
            rows.append(dict(zip(header, values)))
    return meta, header or [], rows


def fnum(row, key):
    v = row.get(key, "")
    if v in (None, "", "—"):
        return None
    try:
        return float(v)
    except ValueError:
        return None


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def sd(xs):
    if len(xs) < 2:
        return float("nan")
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


# ---------------------------------------------------------------- run record (new)
class Run:
    def __init__(self, meta, rows):
        self.meta = meta
        self.r = meta.get("r")
        self.load = meta.get("load_N")
        self.a0, self.b0 = meta.get("A0"), meta.get("B0")
        self.a1, self.b1 = meta.get("A1"), meta.get("B1")
        self.u_wheel = meta.get("U_wheel", meta.get("U_rad"))   # U_rad: files before 2026-11-25
        self.pts = {}
        for row in rows:
            kind = row.get("kind", "")
            p = {
                "plateau": int(fnum(row, "plateau") or 0),
                "k": int(fnum(row, "k") or 0),
                "t": fnum(row, "t_s"),
                "v": fnum(row, "v_kmh"),
                "m": fnum(row, "m_Nm"),
                "m_raw": fnum(row, "m_raw_Nm"),
                "m_inert": fnum(row, "m_inert_Nm"),
                "sd": fnum(row, "sd_Nm"),
                "ripple": fnum(row, "ripple_Nm"),
                "dvdt": fnum(row, "dvdt_net"),
                "bar": fnum(row, "p_bar"),
                "t_tire": fnum(row, "t_tire"),
                "revs": int(fnum(row, "revs") or 0),
                "real": (fnum(row, "real_revs") or 0) > 0,
                "ok": (fnum(row, "ok") or 0) > 0,
            }
            self.pts.setdefault(kind, []).append(p)

    def omega(self, kmh):
        return kmh / 3.6 / self.r

    def loss(self, kmh):
        w = self.omega(kmh)
        m = 0.0
        if self.a0 is not None and self.b0 is not None:
            m += self.a0 + self.b0 * w * w
        if None not in (self.a1, self.b1, self.u_wheel):
            r_wheel = self.u_wheel / (2 * math.pi)
            w_wheel = w * self.r / r_wheel
            m += (self.a1 + self.b1 * w_wheel * w_wheel) * self.r / r_wheel
        return m

    def crr(self, m_nm):
        return m_nm / self.r / self.load if self.load else float("nan")

    def plateaus(self):
        meas = self.pts.get("measure", [])
        return sorted({p["plateau"] for p in meas})

    def pooled_sd(self):
        ssq, dof = 0.0, 0
        for pl in self.plateaus():
            ms = [p["m"] for p in self.pts.get("measure", []) if p["plateau"] == pl and p["ok"] and p["m"] is not None]
            if len(ms) > 1:
                mu = mean(ms)
                ssq += sum((x - mu) ** 2 for x in ms)
                dof += len(ms) - 1
        return (math.sqrt(ssq / dof) if dof else float("nan")), dof

    def baseline_drift(self):
        pre, post = self.pts.get("baseline_pre", []), self.pts.get("baseline_post", [])
        if not pre or not post:
            return None
        worst = 0.0
        for a, b in zip(pre, post):
            if a["m"] is None or b["m"] is None:
                continue
            worst = max(worst, abs(b["m"] - a["m"]) / self.r)
        return worst

    def anchor_drift(self):
        pre = [p["m"] for p in self.pts.get("anchor_pre", []) if p["ok"] and p["m"] is not None]
        post = [p["m"] for p in self.pts.get("anchor_post", []) if p["ok"] and p["m"] is not None]
        if not pre or not post:
            return None
        return (mean(post) - mean(pre)) / self.r

    def plateau_stat(self, pl):
        pts = [p for p in self.pts.get("measure", []) if p["plateau"] == pl]
        good = [p for p in pts if p["ok"] and p["m"] is not None]
        if not good:
            return None
        v = mean([p["v"] for p in good])
        m = mean([p["m"] for p in good])
        s = sd([p["m"] for p in good])
        m_r = m - self.loss(v)
        s_pool, dof = self.pooled_sd()
        if not math.isnan(s_pool):
            u_a = s_pool / math.sqrt(len(good))
        elif not math.isnan(s):
            u_a = s / math.sqrt(len(good))
        else:
            u_a = mean([(p["sd"] or 0) / math.sqrt(p["revs"] or 1) for p in good])
        g = self.meta.get
        drift = self.baseline_drift()
        u_idle_nm = (drift if drift is not None else g("drift_limit_N", BASELINE_DRIFT_LIMIT_N)) * self.r
        u_sync = mean([(p["ripple"] or 0) / 2 / (p["revs"] or 1) for p in good if not p["real"]]) if any(not p["real"] for p in good) else 0.0
        m_inert = mean([abs(p["m_inert"] or 0) for p in good])
        rel2 = ((u_a / m_r) ** 2 + g("unc_k", 0) ** 2 + g("unc_lcal", 0) ** 2 + g("unc_n", 0) ** 2
                + g("unc_p", 0) ** 2 + g("unc_bearing", 0) ** 2 + (u_idle_nm / m_r) ** 2
                + g("unc_rad", 0) ** 2 + (u_sync / m_r) ** 2 + (g("unc_j", 0) * m_inert / m_r) ** 2)
        crr_k = self.crr(m_r)
        return {
            "v": v, "n": len(good), "all": len(pts), "m": m, "sd": s, "f_roll": m_r / self.r,
            "crr_raw": self.crr(m), "crr": crr_k, "u": abs(crr_k) * math.sqrt(rel2),
            "p_w": m_r * self.omega(v), "dof": dof, "u_a_rel": u_a / m_r,
            "bar": mean([p["bar"] for p in good if p["bar"] is not None]) if any(p["bar"] is not None for p in good) else float("nan"),
        }


def evaluate_run(meta, rows, plot):
    run = Run(meta, rows)
    if run.r is None or not run.load:
        sys.exit("Error: r or load_N missing in the file header.")
    if run.a0 is None:
        print("WARNING: no idle-loss model in the header (A0/B0) — Crr contains the drum losses.")
    if run.a1 is None:
        print("WARNING: no wheel-loss model in the header (A1/B1) — Crr contains the wheel's own losses (W09).")
    print(f"Run record: r = {run.r:.4f} m, wheel load {run.load:.1f} N, speed source {meta.get('source', '?')}, "
          f"J_wheel {meta.get('J_wheel_src', meta.get('J_rad_src', '—'))}")

    drift = run.baseline_drift()
    if drift is None:
        print("WARNING: no post-run baseline (W05) — drift of the idle-loss curve unchecked, the budget assumes the criterion.")
    else:
        flag = "  <-- DISCARD SERIES (W08)" if drift > BASELINE_DRIFT_LIMIT_N else ""
        print(f"Baseline drift pre/post: {drift:.3f} N on F_roll (limit {BASELINE_DRIFT_LIMIT_N:.2f}){flag}")
    ad = run.anchor_drift()
    if ad is not None:
        print(f"Reference point after minus before: {ad:+.3f} N on F_roll")

    bad = [p for p in run.pts.get("measure", []) if not p["ok"]]
    if bad:
        print(f"{len(bad)} invalid points (net dv/dt above limit or controller fault), ignored.")

    print()
    print(f"{'v':>6} {'n':>4} {'M':>8} {'σ':>7} {'F_tire':>7} {'P_tire':>7} {'Crr raw':>8} {'Crr':>8} {'u(k=1)':>8} {'ν':>3} {'p bar':>6}")
    print("-" * 84)
    results = []
    for pl in run.plateaus():
        st = run.plateau_stat(pl)
        if not st:
            continue
        results.append(st)
        print(f"{st['v']:6.1f} {st['n']:>2}/{st['all']:<1} {st['m']:8.4f} {st['sd']:7.4f} {st['f_roll']:7.3f} "
              f"{st['p_w']:7.2f} {st['crr_raw']:8.5f} {st['crr']:8.5f} {st['u']:8.5f} {st['dof']:>3} {st['bar']:6.2f}")
    if not results:
        sys.exit("No plateaus that can be evaluated.")
    print()
    print("u(Crr) with k = 1: Type A pooled over all plateaus (ν degrees of freedom), Type B from the file header,")
    print("baseline from the pre/post drift or the criterion. For 95 % multiply by t(ν).")
    print_caveat()
    if plot:
        plot_results(results, "Run record")


# ---------------------------------------------------------------- serial CSV (old)
def fit_ab(points, r):
    """M = a + b w^2 over (v, f_roll) points; returns (a, b) or None."""
    xs = [(p["v"] / 3.6 / r) ** 2 for p in points]
    ys = [p["f_roll"] * r for p in points]
    n = len(xs)
    if n < 3:
        return None
    sx, sy = sum(xs), sum(ys)
    sxx, sxy = sum(x * x for x in xs), sum(x * y for x, y in zip(xs, ys))
    den = n * sxx - sx * sx
    if abs(den) < 1e-12:
        return None
    b = (n * sxy - sx * sy) / den
    return (sy - b * sx) / n, b


def split_baseline_runs(points):
    runs, current, last_v = [], [], None
    for p in points:
        if last_v is not None and p["v"] < last_v - 1.0:
            runs.append(current)
            current = []
        current.append(p)
        last_v = p["v"]
    if current:
        runs.append(current)
    return runs


def evaluate_legacy(meta, rows, plot, force):
    r = meta.get("r")
    if r is None or meta.get("U") is None:
        sys.exit("Error: geometry (U, r) is not in the file header.")
    if not meta.get("N_per_count"):
        sys.exit("Error: the load cell was uncalibrated (N_per_count=0). Raw values only.")
    baseline, measure = [], []
    for row in rows:
        m_nm, v, t = fnum(row, "m_Nm"), fnum(row, "v_kmh"), fnum(row, "t_ms")
        if None in (m_nm, v, t):
            continue
        point = {"t": t / 1000.0, "v": v, "f_roll": m_nm / r, "load": fnum(row, "load_N")}
        note = (row.get("note") or "").strip()
        if note == "baseline":
            baseline.append(point)
        elif note == "measure":
            measure.append(point)
    if not baseline:
        sys.exit("Error: no BASELINE points. Without an idle-loss curve there are no absolute values.")
    if not measure:
        sys.exit("Error: no MEASURE points.")

    runs = split_baseline_runs(baseline)
    fits = [(mean([p["t"] for p in run]), fit_ab(run, r)) for run in runs]
    fits = [(t, f) for t, f in fits if f]
    if not fits:
        sys.exit("Error: idle-loss model cannot be fitted (fewer than three points per run).")
    print(f"Baseline runs: {len(runs)}, of which fitted: {len(fits)}")
    for t, (a, b) in fits:
        print(f"  t = {t:7.0f} s: A0 = {a:.4f} Nm, B0 = {b:.3e} Nm/(rad/s)²")

    def loss_at(v, t):
        w2 = (v / 3.6 / r) ** 2
        vals = [(tm, a + b * w2) for tm, (a, b) in fits]
        if len(vals) == 1 or t <= vals[0][0]:
            return vals[0][1]
        if t >= vals[-1][0]:
            return vals[-1][1]
        for (t1, m1), (t2, m2) in zip(vals, vals[1:]):
            if t1 <= t <= t2:
                return m1 + (t - t1) / (t2 - t1) * (m2 - m1)
        return vals[-1][1]

    if len(fits) >= 2:
        worst = max(abs(loss_at(p["v"], fits[-1][0]) - loss_at(p["v"], fits[0][0])) / r for p in measure)
        print(f"Baseline drift pre/post: {worst:.3f} N on F_roll (limit {BASELINE_DRIFT_LIMIT_N:.2f})")
        if worst > BASELINE_DRIFT_LIMIT_N and not force:
            sys.exit("Abort: idle-loss curve drifted too much. --force overrides.")
    else:
        print("WARNING: only one baseline run, drift cannot be checked.")

    print()
    print(f"{'v':>7} {'F_roll':>9} {'F_idle':>9} {'F_tire':>9} {'P_tire':>9} {'Crr':>9}")
    print("-" * 58)
    results = []
    for p in sorted(measure, key=lambda x: x["v"]):
        base = loss_at(p["v"], p["t"]) / r
        f_tire = p["f_roll"] - base
        crr = f_tire / p["load"] if p["load"] else float("nan")
        results.append({"v": p["v"], "crr": crr, "p_w": f_tire * p["v"] / 3.6, "u": float("nan")})
        print(f"{p['v']:7.1f} {p['f_roll']:9.3f} {base:9.3f} {f_tire:9.3f} {f_tire * p['v'] / 3.6:9.2f} {crr:9.5f}")
    print()
    print("Note: old format without wheel-loss model — Crr contains the wheel's own losses.")
    print_caveat()
    if plot:
        plot_results(results, "serial CSV")


# ---------------------------------------------------------------- common
def print_caveat():
    print()
    print("Two systematic deviations, both uncorrected: compared with a flat surface, drum\n"
          "values are higher (curvature); compared with the published database they are,\n"
          "by the curvature approximation, about 3 to 4 % lower (larger drum). Reliable are\n"
          "comparisons under identical conditions (docs/09-measurement-theory.md §10).")


def plot_results(results, title):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        sys.exit("matplotlib is not installed: pip install matplotlib")
    vs = [r["v"] for r in results]
    fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(7, 7))
    us = [r["u"] for r in results]
    if all(not math.isnan(u) for u in us):
        ax1.errorbar(vs, [r["crr"] for r in results], yerr=us, fmt="o-", capsize=3)
    else:
        ax1.plot(vs, [r["crr"] for r in results], "o-")
    ax1.set_ylabel("Crr")
    ax1.grid(True, alpha=0.3)
    ax2.plot(vs, [r["p_w"] for r in results], "o-")
    ax2.set_ylabel("P_tire [W]")
    ax2.set_xlabel("v [km/h]")
    ax2.grid(True, alpha=0.3)
    fig.suptitle(title)
    fig.tight_layout()
    plt.show()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", help="run record from /runs/ or serial raw file")
    ap.add_argument("--plot", action="store_true", help="show a chart (matplotlib)")
    ap.add_argument("--force", action="store_true", help="old format: evaluate despite baseline drift")
    args = ap.parse_args()

    meta, header, rows = parse_file(args.csv)
    if "kind" in header:
        evaluate_run(meta, rows, args.plot)
    else:
        evaluate_legacy(meta, rows, args.plot, args.force)


if __name__ == "__main__":
    main()
