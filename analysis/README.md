# Evaluation

```sh
python3 crr.py ../data/2026-11-24-1530.csv          # table
python3 crr.py ../data/2026-11-24-1530.csv --plot   # plus a chart with u(Crr)
```

Standard library only; `--plot` needs `matplotlib`.

## What the script does

It reads the **run record** of the guided measurement run (firmware `run.cpp`, file
`/runs/<id>.csv`, download it from http://tire.local/runs and store it unchanged under
`data/`) and calculates **the same way the ESP does** (docs/09-measurement-theory.md
§6, docs/11-review.md W9):

1. Idle-loss model A₀/B₀ and wheel-loss model A₁/B₁ from the file header; the header also
   carries r, L, L_cal, wheel load, J_wheel with its origin, the Type B terms and the
   speed source.
2. Per plateau: mean of the valid points (the firmware has already corrected every point
   by `J·dω/dt`), subtract the loss models, divide by the wheel load.
3. u(Crr) with k = 1: Type A from the scatter of the points **pooled** over all plateaus
   (ν degrees of freedom), Type B from the header, baseline from the pre/post drift or
   the criterion, residual of the averaging without a real index, residual of the inertia
   correction.
4. Drifts as warnings, not as abort: reference point after minus before, baseline
   pre/post against 0.02 N on F_roll. The zero checks are in the ESP's report (`.md`).

Against a synthetic run record with Crr = 0.00370 the script returns 0.00370 on every
plateau; firmware and script implement the same formula.

The **old serial CSV** (`t_ms,…,note=baseline|measure`) is still read: there the baseline
is fitted per baseline run and interpolated in time, that format has no wheel model, and
`--force` overrides the drift abort.

## Messages

| Message | Meaning |
|---|---|
| `no idle-loss model in the header` | no baseline run driven or adopted; Crr contains the drum losses |
| `no wheel-loss model in the header` | W09, Crr contains the wheel's own losses |
| `no post-run baseline (W05)` | drift unchecked, the budget assumes the criterion |
| `DISCARD SERIES (W08)` | baseline drift above 0.02 N |
| `N_per_count=0` | old format, load cell was uncalibrated |

The script never writes back into the raw file.

## check_params.py

```sh
python3 check_params.py        # from the project root: analysis/check_params.py
```

Checks that every constant from `firmware/src/config.h` is documented in
`docs/03-parameters.md` or `docs/04-hardware.md`. Both are meant to match 1:1 (see
CLAUDE.md); the script finds what was forgotten while documenting.

## check_links.py

Checks every relative Markdown link in `README.md` and `docs/` for an existing target
and reports link texts that do not use the file name.

## Tests

`.venv/bin/python -m pytest analysis -q` runs `analysis/tests/test_crr.py`: header parsing,
the loss model against a power-balance formulation, plateau statistics with pooled
standard deviation, baseline drift against the limit, and `fit_ab`. The firmware's host
tests for the same arithmetic are `cd firmware && pio test -e native`
([10-firmware](../docs/10-firmware.md) §4.2).
