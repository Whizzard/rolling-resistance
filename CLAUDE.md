# Conventions in this project

## Language
**Everything in the repository is English** — code, identifiers, comments, firmware
console messages, web UI texts, documentation, drawing labels, commit messages. This
holds even when the conversation is in German. Translated on 2026-11-26; until then
firmware comments and UI texts were German. The UI keeps its terse style (one to three
words per message).

A knowledge-base mirror of the documentation existed until 2026-11-22 and was removed.
The repository is the only source; there is nothing to synchronise.

## Logbook
`docs/12-logbook.md` is the project's diary: milestones and events at the stand
(measurement runs, rebuilds, findings, what the builder names), newest first, one short
high-level entry each, with the source (commit, file under `data/`, document section);
no value without a source. **Not** every UI tweak or bug fix: that is what commit
messages are for (builder, 2026-11-26).

## Units
**SI throughout internally**: N, Nm, W, m, m/s, rad/s, s, Pa.
km/h, bar and rpm appear only in the display and in CSV columns that are explicitly
named so (`v_kmh`, `p_tire_bar`). Conversion only at the output boundary.

## Parameters
Every physical constant lives in `firmware/src/config.h` and **only there**. Every
value there has a row in `docs/03-parameters.md` with its origin (measured /
calculated / datasheet / estimated) and, where applicable, the calibration rule. Both
lists change together; `analysis/check_params.py` finds what was forgotten.

## No invented numbers
Unknown values are written as `TBD` together with the method to determine them. A
placeholder that looks like a measurement is worse than a gap. This applies above all
to calibration factors: `LOADCELL_N_PER_COUNT` stays TBD until calibrated, and the
firmware refuses to output Crr values as long as that is the case. Hardware dimensions,
part numbers and prices come from a source or not at all.

## Secrets
The WLAN password lives in `firmware/src/secrets.h`, which is git-ignored; the template
is `secrets.example.h`. Anything that contains it (the WLAN QR code `hardware/qr-wlan.*`)
is git-ignored too. Server names and IP addresses of the hosting are fine in the repo;
credentials, e-mail addresses and keys are not.

## Measurement data
The **averaged measurement points** are versioned as CSV under `data/`, unchanged. The
raw 80 SPS stream stays on the stand (about 4–5 MB per run, estimate 2026-11-27) and does **not** go into
the repo. Derived quantities are never written back into an existing file; evaluation
always produces new files.

## Firmware
- No `delay()` in the main path; the state machine is non-blocking.
- ISRs are short (timestamp into a ring buffer, nothing else) and marked `IRAM_ATTR`.
- Every change to a safety limit (`MAX_*`, timeouts) is carried into
  `docs/07-safety.md`.
- Build with `cd firmware && pio run -e esp32s3` before claiming a change compiles.

## Documentation
- Every doc states its date ("As of YYYY-MM-DD") where the content is a snapshot.
- Numbers use a decimal point; units with a space (28.8 km/h, 0.4523 m).
- Cross-references use the file name as link text: `[09-measurement-theory](09-measurement-theory.md) §3.2`.
  `analysis/check_links.py` checks every relative link.
