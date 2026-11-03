# Konventionen in diesem Projekt

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
