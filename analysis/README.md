# Auswertung

```sh
python3 crr.py ../data/messung.csv          # Tabelle
python3 crr.py ../data/messung.csv --plot   # zusätzlich Diagramm
```

Nur Standardbibliothek; `--plot` braucht `matplotlib`.

## Was das Skript tut

1. Liest Geometrie (U, r, L) und Kalibrierfaktor aus dem Dateikopf.
2. Trennt BASELINE- und MEASURE-Punkte an der Spalte `note`.
3. Zerlegt die Baseline in Läufe (neuer Lauf, sobald die Geschwindigkeit wieder fällt).
4. Vergleicht Vor- und Nachlauf. **Abbruch bei mehr als 0,10 N Abweichung** — dann hat
   sich während der Messreihe etwas verändert, meist die Lagertemperatur, und das
   Ergebnis wäre nicht interpretierbar. Mit `--force` überstimmbar.
5. Interpoliert die Leerlaufkennlinie über v (innerhalb eines Laufs) und über die Zeit
   (zwischen den Läufen), subtrahiert punktweise, teilt durch die Auflast.

## Abbruchgründe

| Meldung | Bedeutung |
|---|---|
| `N_per_count=0` | Wägezelle war unkalibriert, es liegen nur Rohwerte vor |
| keine BASELINE-Punkte | Leerlaufkennlinie fehlt — ohne sie keine Absolutwerte |
| Baseline-Drift zu groß | Messreihe verwerfen |

Das Skript schreibt nie in die Rohdatei zurück.

## check_params.py

```sh
python3 check_params.py        # aus dem Projektwurzelverzeichnis: analysis/check_params.py
```

Prüft, dass jede Konstante aus `firmware/src/config.h` in `docs/03-parameter.md` oder
`docs/04-hardware.md` dokumentiert ist. Beide sollen 1:1 übereinstimmen (siehe CLAUDE.md);
das Skript findet, was beim Dokumentieren vergessen wurde.
