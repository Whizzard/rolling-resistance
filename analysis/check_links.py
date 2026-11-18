#!/usr/bin/env python3
"""Querprüfung: jeder relative Markdown-Verweis in README und docs/ zeigt auf eine
Datei, die es gibt.

Geprüft wird zweierlei:

1. **Das Ziel existiert.** Gerät ein Knowledgebase-Knotenname ins Repo, zeigt der
   Verweis ins Nichts, und keine Spiegelung repariert das — sie schreibt nur noch
   einmal um. Am 2026-11-17 waren sieben solche Verweise entstanden.
2. **Das Label ist aus dem Dateinamen abgeleitet**, also `[04-hardware]` oder
   `[docs/04-hardware.md]` oder `[05-prüfablauf]` — aber nicht `[Sicherheit]`.
   Beim Spiegeln wird BEIDES umgeschrieben, Label und Ziel. Ein Verweis mit
   Knowledgebase-Label und Repo-Ziel zeigt deshalb auf die richtige Datei und
   fällt bei Prüfung 1 nicht auf — am 2026-11-18 ist mir genau das passiert. Er
   verrät sich erst, wenn jemand die Spiegelung Zeichen für Zeichen vergleicht.

   Verglichen wird nach Kleinschreibung, ohne `.md` und mit aufgelösten Umlauten.
   Damit sind die beiden vorhandenen Schreibweisen zulässig: README führt den
   ganzen Pfad als Label, und `09-messtheorie` schreibt `05-prüfablauf` mit
   Umlaut, während die Datei `05-pruefablauf.md` heißt.

Hintergrund: die Dokumentation wird in die Knowledgebase gespiegelt, und dabei werden
die Verweise auf deren Knotennamen umgeschrieben (`04-hardware.md` → `23_hardware.md`).
Die Umschreibung ist einseitig.

Aufruf aus dem Projektwurzelverzeichnis:
    python3 analysis/check_links.py
"""

import pathlib
import re
import sys

# Knotennamen der Knowledgebase. Tauchen sie im Repo auf, ist etwas verwechselt worden.
KB_KNOTEN = re.compile(r"^\d\d_[a-z_]+\.md$")

UMLAUTE = (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("ß", "ss"))


def norm(t):
    """Label und Ziel vergleichbar machen: klein, ohne .md, ohne Umlaute."""
    t = t.lower()
    if t.endswith(".md"):
        t = t[:-3]
    for a, b in UMLAUTE:
        t = t.replace(a, b)
    return t


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    dateien = [root / "README.md"] + sorted((root / "docs").glob("*.md"))
    fehler = []
    geprueft = 0

    schief = []

    for f in dateien:
        if not f.exists():
            continue
        text = f.read_text()
        basis = f.parent
        for m in re.finditer(r"\[([^\]]*)\]\(([^)#]+\.md)(#[^)]*)?\)", text):
            label, ziel = m.group(1), m.group(2)
            if ziel.startswith(("http://", "https://", "/")):
                continue
            geprueft += 1
            zeile = text[: m.start()].count("\n") + 1
            if not (basis / ziel).exists():
                grund = ("Knowledgebase-Knotenname im Repo"
                         if KB_KNOTEN.match(ziel) else "Datei fehlt")
                fehler.append(f"  {f.relative_to(root)}:{zeile}  {ziel}  ({grund})")
                continue
            # Prüfung 2: das Label muss aus dem Dateinamen kommen.
            if norm(label) != norm(ziel):
                schief.append(f"  {f.relative_to(root)}:{zeile}  "
                              f"[{label}]({ziel})  →  [{ziel[:-3]}]({ziel})")

    print(f"Relative Verweise geprüft: {geprueft}")
    if fehler:
        print("\nZIELE EXISTIEREN NICHT:")
        print("\n".join(fehler))
    if schief:
        print("\nLABEL PASST NICHT ZUM ZIEL (Knowledgebase-Form im Repo):")
        print("\n".join(schief))
    if fehler or schief:
        return 1
    print("Alle Verweise zeigen auf vorhandene Dateien, alle Label passen.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
