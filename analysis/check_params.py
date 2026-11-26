#!/usr/bin/env python3
"""Cross-check: every physical constant from config.h is documented.

Background: config.h and docs/03-parameters.md are meant to match 1:1
(see CLAUDE.md). This script finds constants that were forgotten while
documenting.

Run from the project root:
    python3 analysis/check_params.py
"""

import pathlib
import re
import sys

# Pure language helpers and structural constants without physical meaning.
EXEMPT = {
    "KMH_TO_MS", "MS_TO_KMH",   # unit conversion
    "PLATEAU_COUNT",            # derived from PLATEAUS_KMH
}

DOCS = ["docs/03-parameters.md", "docs/04-hardware.md"]


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    cfg = (root / "firmware/src/config.h").read_text()
    docs = "\n".join((root / d).read_text() for d in DOCS)

    names = re.findall(r"constexpr\s+\w+\s+(\w+)\s*(?:\[\w*\])?\s*=", cfg)
    missing = [n for n in names if n not in EXEMPT and n not in docs]

    print(f"Constants in config.h: {len(names)}  (exempt: {len(EXEMPT)})")
    if missing:
        print(f"\nNOT documented in {' / '.join(DOCS)}:")
        for m in missing:
            print("  -", m)
        return 1

    print("All constants are documented.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
