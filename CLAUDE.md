# Konventionen in diesem Projekt

## Knowledgebase — erste Regel, vor allem anderen

Die Dokumentation wird in der Knowledgebase gespiegelt (MCP-Connector
„claude.ai Knowledgebase", `claudeai-proxy` auf https://machineum.com), als
browsebarer Ast mit Texten, Verweisen und Zeichnungen.

**Gespiegelt werden:** `README.md`, alles unter `docs/`, sowie die gerenderten
Zeichnungen `hardware/schaltplan.pdf`, `.png` und `hardware/hall-gehaeuse.*`.
**Nicht gespiegelt:** `firmware/`, `analysis/`, `data/`, die `.py`-Quellen der
Zeichnungen. Code lebt nur im Repo.

**Ablauf, verbindlich:**

1. **Zu Beginn jeder Sitzung**, bevor irgendetwas lokal geändert wird: den Stand der
   Knowledgebase holen und mit dem Repo vergleichen. Dort geänderte Knoten zuerst ins
   Repo übernehmen und committen.
2. **Vor jedem Schreiben in die Knowledgebase**: prüfen, ob sich der Zielknoten seit
   dem Holen verändert hat. Wenn ja, **nicht überschreiben** — beide Fassungen zeigen
   und nachfragen.
3. **Nach jedem Commit**, der gespiegelte Dateien berührt: die betroffenen Knoten
   zurückschreiben.

Zwischen Schritt 1 und 3 wird nicht erneut abgefragt — ein Abgleich je Datei wäre bei
dutzenden Änderungen je Sitzung unbrauchbar. Schritt 2 fängt den Fall ab, dass
zwischendurch doch jemand in der Knowledgebase gearbeitet hat.

**Bei Konflikt wird nicht automatisch zusammengeführt.** Das Repo ist nicht
automatisch im Recht: wer zuletzt bewusst geschrieben hat, entscheidet — und das weiß
nur der Mensch.

> Der Grund für diese Regel steht in der Projektgeschichte: Dieselbe Information an
> zwei Stellen war die Quelle fast aller Fehler, die wir finden mussten — veraltete
> Trommelmaße, ein Motor, der nie verbaut wurde, eine Bremse, die es nicht gibt.
> Eine Spiegelung ohne Abgleichregel wäre dieselbe Falle noch einmal.


## Sprache
Dokumentation und Kommentare auf Deutsch. Bezeichner im Code auf Englisch.

## Einheiten
**Intern durchgängig SI**: N, Nm, W, m, m/s, rad/s, s, Pa.
km/h, bar und 1/min erscheinen ausschließlich in der Anzeige und in CSV-Spalten, die
explizit so benannt sind (`v_kmh`, `p_tire_bar`). Umrechnung nur an der Ausgabegrenze.

## Parameter
Jede physikalische Konstante steht in `firmware/src/config.h` und **nur dort**.
Jeder Wert dort hat eine Zeile in `docs/03-parameter.md` mit Herkunft
(gemessen / berechnet / Datenblatt / geschätzt) und, falls zutreffend, der
Kalibriervorschrift. Beide Listen werden gemeinsam geändert.

## Keine erfundenen Zahlen
Unbekannte Werte stehen als `TBD` zusammen mit der Methode, wie sie zu ermitteln sind.
Ein Platzhalterwert, der wie eine Messung aussieht, ist schlimmer als eine Lücke.
Das gilt besonders für Kalibrierfaktoren: `LOADCELL_N_PER_COUNT` bleibt TBD, bis
kalibriert wurde, und die Firmware weigert sich, Crr-Werte auszugeben, solange das so ist.

## Messdaten
Versioniert werden die **gemittelten Messpunkte** als CSV unter `data/`, unverändert.
Der rohe 80-SPS-Strom bleibt auf der SD-Karte und kommt **nicht** ins Repo (rund 30 MB
je Sitzung). Abgeleitete Größen werden nie in eine vorhandene Datei zurückgeschrieben —
Auswertung erzeugt immer neue Dateien.

## Firmware
- Kein `delay()` im Hauptpfad; die Zustandsmaschine ist nicht blockierend.
- ISRs sind kurz (Zeitstempel in Ringpuffer, sonst nichts) und mit `IRAM_ATTR` versehen.
- Jede Änderung an Sicherheitsgrenzen (`MAX_*`, Timeouts) wird in
  `docs/07-sicherheit.md` nachgezogen.
