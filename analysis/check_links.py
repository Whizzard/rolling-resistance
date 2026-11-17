#!/usr/bin/env python3
"""Querprüfung: jeder relative Markdown-Verweis in README und docs/ zeigt auf eine
Datei, die es gibt.

Hintergrund: die Dokumentation wird in die Knowledgebase gespiegelt, und dabei werden
die Verweise auf deren Knotennamen umgeschrieben (`04-hardware.md` → `23_hardware.md`).
Die Umschreibung ist einseitig. Gerät ein Knotenname versehentlich ins Repo, zeigt der
Verweis dort ins Nichts, und keine Spiegelung repariert das — sie schreibt nur noch
einmal um. Am 2026-11-17 waren sieben solche Verweise entstanden.

Aufruf aus dem Projektwurzelverzeichnis:
    python3 analysis/check_links.py
"""

import pathlib
import re
import sys

# Knotennamen der Knowledgebase. Tauchen sie im Repo auf, ist etwas verwechselt worden.
KB_KNOTEN = re.compile(r"^\d\d_[a-z_]+\.md$")


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    dateien = [root / "README.md"] + sorted((root / "docs").glob("*.md"))
    fehler = []
    geprueft = 0

    for f in dateien:
        if not f.exists():
            continue
        text = f.read_text()
        basis = f.parent
        for m in re.finditer(r"\[[^\]]*\]\(([^)#]+\.md)(#[^)]*)?\)", text):
            ziel = m.group(1)
            if ziel.startswith(("http://", "https://", "/")):
                continue
            geprueft += 1
            zeile = text[: m.start()].count("\n") + 1
            if not (basis / ziel).exists():
                grund = ("Knowledgebase-Knotenname im Repo"
                         if KB_KNOTEN.match(ziel) else "Datei fehlt")
                fehler.append(f"  {f.relative_to(root)}:{zeile}  {ziel}  ({grund})")

    print(f"Relative Verweise geprüft: {geprueft}")
    if fehler:
        print("\nZIELE EXISTIEREN NICHT:")
        print("\n".join(fehler))
        return 1
    print("Alle Verweise zeigen auf vorhandene Dateien.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
