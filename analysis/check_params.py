#!/usr/bin/env python3
"""Querprüfung: jede physikalische Konstante aus config.h ist dokumentiert.

Hintergrund: config.h und docs/03-parameters.md sollen 1:1 übereinstimmen
(siehe CLAUDE.md). Dieses Skript findet Konstanten, die beim Dokumentieren
vergessen wurden.

Aufruf aus dem Projektwurzelverzeichnis:
    python3 analysis/check_params.py
"""

import pathlib
import re
import sys

# Reine Sprachhilfen und Strukturkonstanten ohne physikalische Bedeutung.
EXEMPT = {
    "KMH_TO_MS", "MS_TO_KMH",   # Einheitenumrechnung
    "PLATEAU_COUNT",            # aus PLATEAUS_KMH abgeleitet
}

DOCS = ["docs/03-parameters.md", "docs/04-hardware.md"]


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    cfg = (root / "firmware/src/config.h").read_text()
    docs = "\n".join((root / d).read_text() for d in DOCS)

    names = re.findall(r"constexpr\s+\w+\s+(\w+)\s*(?:\[\])?\s*=", cfg)
    missing = [n for n in names if n not in EXEMPT and n not in docs]

    print(f"Konstanten in config.h: {len(names)}  (ausgenommen: {len(EXEMPT)})")
    if missing:
        print(f"\nNICHT dokumentiert in {' / '.join(DOCS)}:")
        for m in missing:
            print("  -", m)
        return 1

    print("Alle Konstanten sind dokumentiert.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
