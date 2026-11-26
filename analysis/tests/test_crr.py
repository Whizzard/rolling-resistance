"""Tests for analysis/crr.py: header lines, loss model, plateau statistics, drift.

    .venv/bin/python -m pytest analysis -q

The expected values are calculated by hand (see comments), not with the
functions under test.
"""
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import crr  # noqa: E402

R, LOAD = 0.45, 400.0
A0, B0, A1, B1, U_WHEEL = 0.05, 0.0005, 0.01, 0.00005, 1.5

COLS = ("kind,plateau,k,t_s,v_kmh,v_sd,m_Nm,m_raw_Nm,m_inert_Nm,sd_Nm,ripple_Nm,dvdt_net,"
        "i_mot,v_in,t_fet,t_mot,p_bar,t_tire,revs,real_revs,ok")


def row(kind, plateau, k, v, m, ok=1, real=1, ripple=0.1):
    return (f"{kind},{plateau},{k},{k * 10},{v},0.1,{m},{m},0,0.01,{ripple},0.01,"
            f"3,40,30,35,6.5,20,10,{real},{ok}")


def fixture(tmp_path, drift_post=0.31):
    lines = [f"# r={R} load_N={LOAD} A0={A0} B0={B0} A1={A1} B1={B1} U_wheel={U_WHEEL} "
             "unc_k=0.002 unc_lcal=0.001 unc_n=0.005 unc_p=0.01 unc_bearing=0.0065 unc_rad=0.005 unc_j=0.05",
             "# tire=Test wheel=TestWheel", COLS]
    lines += [row("measure", 0, 1, 20.0, 1.00), row("measure", 0, 2, 20.0, 1.02), row("measure", 0, 3, 20.0, 0.98),
              row("measure", 1, 1, 30.0, 1.50), row("measure", 1, 2, 30.0, 1.50), row("measure", 1, 3, 30.0, 1.50),
              row("measure", 1, 4, 30.0, 9.99, ok=0),              # invalid, must not count
              row("baseline_pre", 0, 1, 20.0, 0.30), row("baseline_post", 0, 1, 20.0, drift_post)]
    p = tmp_path / "lauf.csv"
    p.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return p


def loss_by_power_balance(v):
    """Formulated independently: wheel loss as power at the wheel, divided by the drum omega."""
    w = v / 3.6 / R
    r_wheel = U_WHEEL / (2 * math.pi)
    w_wheel = w * R / r_wheel
    return A0 + B0 * w * w + (A1 + B1 * w_wheel ** 2) * w_wheel / w


def test_parse_header_floats_and_strings():
    meta = {}
    crr.parse_header("# r=0.45 load_N=400 tire=Test-28", meta)
    assert meta["r"] == 0.45 and meta["load_N"] == 400.0 and meta["tire"] == "Test-28"


def test_parse_file_reads_meta_and_rows(tmp_path):
    meta, header, rows = crr.parse_file(fixture(tmp_path))
    assert meta["U_wheel"] == U_WHEEL and meta["wheel"] == "TestWheel"
    assert header[0] == "kind" and len(rows) == 9
    assert rows[0]["kind"] == "measure" and rows[0]["m_Nm"] == "1.0"


def test_loss_matches_power_balance(tmp_path):
    meta, _, rows = crr.parse_file(fixture(tmp_path))
    run = crr.Run(meta, rows)
    for v in (10.0, 20.0, 35.0):
        assert math.isclose(run.loss(v), loss_by_power_balance(v), rel_tol=1e-12)


def test_plateau_stat_mean_sd_pooled(tmp_path):
    meta, _, rows = crr.parse_file(fixture(tmp_path))
    run = crr.Run(meta, rows)
    s0 = run.plateau_stat(0)
    assert s0["n"] == 3 and math.isclose(s0["m"], 1.0) and math.isclose(s0["sd"], 0.02)
    # pooled: plateau 0 ssq 0.0008 (dof 2), plateau 1 ssq 0 (dof 2) -> sqrt(0.0008/4)
    s_pool, dof = run.pooled_sd()
    assert dof == 4 and math.isclose(s_pool, math.sqrt(0.0008 / 4))
    assert math.isclose(s0["u_a_rel"], s_pool / math.sqrt(3) / (1.0 - loss_by_power_balance(20.0)))
    # Crr raw and corrected: M / r / N
    assert math.isclose(s0["crr_raw"], 1.0 / R / LOAD)
    assert math.isclose(s0["crr"], (1.0 - loss_by_power_balance(20.0)) / R / LOAD)
    assert 0 < s0["u"] < s0["crr"]
    s1 = run.plateau_stat(1)
    assert s1["n"] == 3 and s1["all"] == 4 and math.isclose(s1["sd"], 0.0, abs_tol=1e-12)   # the invalid point does not count


def test_baseline_drift_flags_limit(tmp_path):
    meta, _, rows = crr.parse_file(fixture(tmp_path, drift_post=0.31))
    drift = crr.Run(meta, rows).baseline_drift()
    assert math.isclose(drift, 0.01 / R) and drift > crr.BASELINE_DRIFT_LIMIT_N
    meta, _, rows = crr.parse_file(fixture(tmp_path, drift_post=0.305))
    assert crr.Run(meta, rows).baseline_drift() < crr.BASELINE_DRIFT_LIMIT_N


def test_fit_ab_recovers_model():
    a, b = 0.04, 0.0006
    pts = [{"v": v, "f_roll": (a + b * (v / 3.6 / R) ** 2) / R} for v in (10.0, 20.0, 30.0, 40.0)]
    got = crr.fit_ab(pts, R)
    assert got is not None and math.isclose(got[0], a, rel_tol=1e-9) and math.isclose(got[1], b, rel_tol=1e-9)
    assert crr.fit_ab(pts[:2], R) is None
