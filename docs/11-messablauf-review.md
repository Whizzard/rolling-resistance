# Review Messablauf: Kalibrieren, Prüfen, Protokollieren

Stand 2026-11-23. Frage: **Wird ein Messlauf „rund"?** Also: ist bei jedem Lauf alles
kalibriert oder geprüft, wird jede Abweichung gemessen statt angenommen, und steht am
Ende ein Protokoll, das man ohne den Prüfstand lesen und auswerten kann?

> **Stand 2026-11-23 abends: umgesetzt** in `firmware/src/run.cpp` (Schrittmaschine,
> Laufsatz, Protokoll als Markdown und CSV im LittleFS), `state.cpp` (Sequenzen mit
> Streuung je Punkt, Phase HOLD), `index.html` (Überlagerung hinter *Messung*).
> Entscheidungen E1 bis E6 wie vorgeschlagen; E4 Flash statt SD-Karte, weil die
> SD-Karte nicht verdrahtet ist. Gegen eine Attrappe durchgeklickt, **am Stand noch
> nicht gefahren**.

Grundlage: [05-pruefablauf](05-pruefablauf.md) (SOP), [09-messtheorie](09-messtheorie.md)
§3 (Fehlerbudget) und §8, die Firmware in `firmware/src/state.cpp`, `logger.cpp`,
`main.cpp` und die Oberfläche `firmware/web/index.html`. Der klickbare Prototyp des
geführten Ablaufs liegt in [11-messablauf-prototyp.html](11-messablauf-prototyp.html).

## 1 Was heute passiert, wenn man „Messung" drückt

| | heute | Folge |
|---|---|---|
| Start | sofort, wenn Auflast gesetzt und Plateaus vorhanden | keine Prüfung, ob Tara, Leerlaufmodell, Radmodell, Umfang aktuell sind |
| Ablauf | Plateaus der Reihe nach, je 15 s Einschwingen, **ein** Punkt über 10 Umdrehungen | ein Wert je Geschwindigkeit, keine Streuung innerhalb des Plateaus |
| Warmlauf | nicht Teil des Laufs (`WARMUP_MS` existiert, wird nicht gefahren) | Reifen kalt oder beliebig warm, Vergleichbarkeit hängt am Bediener |
| Leerlauf | eigener Lauf oder Assistent; A₀/B₀ im NVS, **ohne Zeitstempel** | niemand weiß beim Messen, wie alt die Kennlinie ist |
| Nachlauf | nur in der SOP, nicht in der Firmware | Driftkriterium 0,02 N ([09](09-messtheorie.md) §8.8) wird nie geprüft |
| Protokoll | CSV-Zeile je Punkt auf USB; Kopf mit U, r, L, N_per_count | geht verloren, wenn kein Rechner mithört; keine Metadaten (Reifen, Druck, Temperatur, Modelle, Build) |
| Streuung | nicht erfasst | Unsicherheit je Punkt unbekannt, Ausreißer unsichtbar |
| Gültigkeit | Punkt wird aufgenommen, sobald die Rampe steht | kein Vermerk, ob die Fahrt während des Punkts konstant war |

Kurz: die Physik ist sauber ([09](09-messtheorie.md) §1 bis §6), die Messkette ist
kalibrierbar, aber der **Lauf** ist noch kein Messverfahren. Er misst, er prüft sich
nicht, und er hinterlässt nichts, was einen Monat später noch lesbar ist.

## 2 Befunde und Empfehlungen

Nummern **R1 … R12**, nach Wirkung sortiert. „Aufwand" meint Firmware plus Oberfläche.

### R1 Ein Lauf ist ein Datensatz, nicht ein Serienstrom

Jeder Lauf bekommt eine Kennung (`2026-11-23-03`) und einen **Laufsatz** (JSON) auf dem
ESP (SD-Karte, sonst LittleFS): Metadaten, Prüfergebnisse, Kalibrierstand mit
Zeitstempeln, alle Messpunkte mit Streuung, Warnungen. Die serielle CSV bleibt als
Mitschnitt. Die Oberfläche bietet je Lauf **„Protokoll .md"** und **„Daten .csv"** zum
Herunterladen an; das Markdown rendert der ESP aus dem Laufsatz (reines Text-Templating,
keine Bibliothek).

Aufwand mittel. Ohne R1 bleibt alles andere flüchtig.

### R2 Vor dem Start wird geprüft, nicht angenommen

Prüfliste beim Drücken von „Messung", jede Zeile **ok / übersprungen / fehlt**, alle
Ergebnisse ins Protokoll:

| Prüfung | automatisch | Kriterium |
|---|---|---|
| Regler verbunden, kein Fehler, Bus 40–44 V | ja | aus Telemetrie |
| Motor am Regler (Not-Aus nicht ausgelöst) | ja | Trennungserkennung ruhig |
| Wägezelle antwortet, nicht am Anschlag | ja | `torque::isResponding`, `isSaturated` |
| Zellenrauschen im Stillstand | ja, 5 s | Standardabweichung der Rohwerte < 3 Counts [TBD, aus ersten Läufen] |
| Kalibrierfaktor vorhanden | ja | `LOADCELL_N_PER_COUNT` nicht TBD |
| Tara-Alter | ja | jünger als der letzte mechanische Eingriff; sonst neu nullen |
| **Kontrollgewicht** | halbautomatisch, überspringbar | 1000 g bei 330 mm → Soll 3,237 Nm; Abweichung < 0,5 % ok, < 1 % Warnung, sonst stopp |
| Leerlaufmodell A₀/B₀ | ja | vorhanden, Alter, Anzahl Punkte; älter als die Sitzung → Vorlauf fahren |
| Radmodell A₁/B₁ und Umfang für **dieses** Laufrad | ja | Laufradkennung passt; sonst Assistent Auslauf |
| Auflast gesetzt, Position Laufgewicht eingetragen | Eingabe | Pflichtfeld |
| Reifen, Druck, Raumtemperatur | Eingabe | Pflichtfelder (Temperatur bis Sensor A5 kommt von Hand) |
| Sichtprüfung, Not-Aus griffbereit | Bestätigung | SOP B0 |

Das Kontrollgewicht ist der billigste scharfe Test der ganzen Kette ([09](09-messtheorie.md)
§5.1): 20 Sekunden, und er fängt verrutschte Kabel, verstellte Arme und Drift der Zelle
auf einmal. Wer die Kalibrierung gerade gemacht hat, überspringt ihn, das Protokoll
vermerkt es.

### R3 Manuelle Eingriffe zuerst, dann die Maschine allein

Reihenfolge des geführten Ablaufs:

1. **Vorbereitung, manuell:** Lauf anlegen, Sichtprüfung, Systemprüfung, Nullen,
   Kontrollgewicht, Leerlaufmodell (falls nötig: Rad abheben, Vorlauf fahren, Rad
   absenken), Radmodell (falls nötig: Assistent Auslauf), Auflast prüfen.
2. **Messfahrt, automatisch:** Warmlauf, Plateaus, Ankerpunkt, Auslauf.
3. **Abschluss:** Nachlauf (optional, siehe R6), Protokoll.

Alles, was Hände braucht, liegt vor dem Warmlauf. Danach läuft der Stand eine
Dreiviertelstunde allein, und das Protokoll entsteht am Ende ohne Zutun.

### R4 Streuung je Punkt messen: drei Punkte je Plateau

Heute: ein Punkt über 10 Umdrehungen. Vorschlag: **drei aufeinanderfolgende Punkte** über
je 10 Umdrehungen auf jedem Plateau. Daraus je Plateau Mittelwert und
Standardabweichung der drei Punkte, dazu die Standardabweichung der zehn
Umdrehungsmittel innerhalb jedes Punkts. Erst damit hat ein Messwert eine Unsicherheit
vom Typ A, die nicht aus einer Annahme stammt.

Kosten: bei 15 km/h 20 s mehr je Plateau, bei 60 km/h 4 s. Der ganze Messlauf wird um
rund eine Minute länger.

Samples innerhalb einer Umdrehung sind korreliert (Trommelordnungen). Die Streuung der
**Umdrehungsmittel** ist deshalb das ehrliche Maß, nicht die der 80-SPS-Samples
([09](09-messtheorie.md) §3.8).

### R5 Gültigkeit je Punkt festhalten

Je Punkt ins Protokoll: mittlere Geschwindigkeit und deren Standardabweichung,
|dv/dt|, Stellwert und Motorstrom im Mittel, Busspannung, FET- und Motortemperatur,
Dauer. Ein Punkt gilt als **ungültig**, wenn während der Aufnahme `STEADY_*` verletzt
war oder der Regler einen Fehler meldete; er bleibt in den Daten, markiert
([10-firmware](10-firmware.md) L10). Die Auswertung ignoriert ihn, der Mensch sieht
ihn.

### R6 Drift innerhalb des Laufs messen: Ankerpunkt und Nachlauf

Zwei Mechanismen, beide billig im Vergleich zu dem, was sie retten:

- **Ankerpunkt:** nach dem letzten Plateau zurück auf 28,8 km/h und den Punkt
  wiederholen. Differenz zum ersten 28,8-Punkt = Drift des Gesamtmoments während des
  Messlaufs (Reifentemperatur, Lager, Luftzug). Vollautomatisch, 40 s.
- **Nachlauf Leerlauf:** die SOP verlangt ihn (B7), das Driftkriterium 0,02 N auf
  F_roll hängt daran. Er braucht aber ein **Abheben des Rads am Ende**, also einen
  Handgriff nach der Messfahrt. Vorschlag: letzter Schritt des Assistenten, **optional**
  mit Taste „Überspringen". Wird übersprungen, trägt das Protokoll „Drift der
  Leerlaufkennlinie ungeprüft" als Warnung, und das Ergebnis gilt nur im
  A/B/A-Vergleich derselben Sitzung ([09](09-messtheorie.md) §8.10), nicht absolut.

Das ist der eine Punkt, an dem „alles Manuelle am Anfang" und „wissenschaftlich sauber"
nicht zusammengehen. Der Ankerpunkt mildert es, ersetzt es nicht: er sieht die Drift
von Trommel **plus** Reifen, der Nachlauf trennt sie.

### R7 Warmlauf als gemessener Zustand, nicht als Uhr

30 min bei 28,8 km/h ist die Vorgabe des Referenzprüfstands ([09](09-messtheorie.md)
§8.5). Während des Warmlaufs läuft die Live-Messung ohnehin; sie wird **jede Minute**
als Punkt in den Laufsatz geschrieben. Das Protokoll zeigt den Verlauf P(t), und man
sieht, ob der Reifen nach 30 min eingelaufen ist. Zweiter Schritt, später:
Abbruchkriterium „Änderung < 1 % über 5 min, höchstens 30 min" statt fester Zeit.
Zuerst nur protokollieren, dann aus den Daten das Kriterium ableiten.

### R8 Alle Eingaben und der Gerätestand ins Protokoll

Kopf des Laufsatzes: Laufkennung, Datum, Firmware-Build, Reglerfirmware und
Hall-Tabelle (`vesc-config` Stand), `U`, `L`, `N_per_count`, Tara-Offset mit
Zeitpunkt, A₀/B₀ mit Zeitpunkt und Punktzahl, A₁/B₁ mit Laufradkennung, J_Rad,
Radumfang, Auflast, Position Laufgewicht, Reifen, Druck, Raumtemperatur, Bediener,
Freitext. Jede Zahl, die in Crr eingeht, steht damit neben dem Ergebnis
([10-firmware](10-firmware.md) E1).

### R9 Unsicherheit je Punkt ausrechnen und mitgeben

Mit den Größen aus R4 und R8 ist das eine Zeile:

```
u(Crr)/Crr = sqrt( (s_M/√n / M_R)²        Typ A, Streuung der Umdrehungsmittel
                 + (u_k/k)² + (u_L/L)²     Kalibrierfaktor, Hebelarm
                 + (u_N/N)²                Auflast
                 + (u_leer / M_R)²         Leerlaufmodell (Drift Vor-/Nachlauf, sonst Annahme 0,02 N·r)
                 + (u_Rad / M_R)² )        Radmodell (J_Rad ±1,5 % → 0,5 % von M_R)
```

Die Typ-B-Anteile stehen als Konstanten in `config.h` mit Herkunft
([03-parameter](03-parameter.md)); der Typ-A-Anteil kommt aus dem Lauf. Das Protokoll
zeigt je Plateau Crr ± u(Crr). Damit ist der Unterschied zweier Reifen eine Aussage mit
Rand, keine Zahl.

### R10 Nummerierte Warnungen statt Prosa

Alles, was während des Laufs schiefgehen kann und nicht abbricht, ist eine Warnung mit
Kennung, die im Protokoll steht: W01 Kontrollgewicht übersprungen, W02 Leerlaufmodell
älter als die Sitzung, W03 Punkt ungültig (welcher, warum), W04 Ankerdrift über
Schwelle, W05 Nachlauf übersprungen, W06 Reglerfehler während des Laufs, W07
Temperatur nicht eingetragen. Eine Auswertung kann danach filtern.

### R11 Auswertung aus dem Laufsatz, nicht aus der Konsole

`analysis/crr.py` liest heute die serielle CSV. Es soll zusätzlich den Laufsatz (JSON)
lesen und daraus dasselbe Markdown erzeugen können wie der ESP, plus die Grafiken.
Zwei Wege, ein Format. Das Markdown des ESP ist das Protokoll, das Python ist die
Auswertung mehrerer Läufe (A/B/A, Wiederholstreuung über Tage).

### R12 Was bleibt, auch wenn alles obige steht

- Absolutwerte bleiben trommelbezogen ([09](09-messtheorie.md) §10); der Vergleich zweier
  Reifen in einer Sitzung ist das belastbare Ergebnis.
- Die Reifentemperatur ist keine Messgröße, solange A5 offen ist.
- Der lastabhängige Lageranteil (0,65 % vom Signal) steckt im Ergebnis, bis ein
  Referenzlauf mit starrer Rolle gefahren ist.
- Die Hall-Geber an der Trommel fehlen: Drehzahl kommt vom Regler, Winkelbins
  ([09](09-messtheorie.md) §3.8) gibt es nicht. Für die Mittelung über ganze
  Umdrehungen reicht die Reglerdrehzahl, für die Diagnose der Störquellen nicht.

## 3 Sollablauf des geführten Messlaufs

Das ist die Vorlage für den Prototyp und später für `state.cpp`. Schritte mit **H**
brauchen Hände, **A** laufen allein, **E** sind Eingaben.

| # | Schritt | Art | Ergebnis im Protokoll |
|---|---|---|---|
| 1 | Lauf anlegen: Reifen, Laufrad, Druck, Raumtemperatur, Auflast, Position Laufgewicht, Notiz | E | Kopfdaten |
| 2 | Sichtprüfung nach SOP B0, Not-Aus griffbereit | H | bestätigt / Zeit |
| 3 | Systemprüfung: Regler, Zelle, Rauschen, Bus, Modelle, Alter | A | Tabelle ok/fehlt |
| 4 | Nullen, Trommel steht | A | Offset, Rauschen σ |
| 5 | Kontrollgewicht 1000 g bei 330 mm | H, überspringbar | Abweichung % oder W01 |
| 6 | Leerlaufmodell: übernehmen oder neu (Rad abheben → Vorlauf → Rad absenken) | H+A | A₀, B₀, 5 Punkte, Drift zum gespeicherten |
| 7 | Radmodell für dieses Laufrad: übernehmen oder Assistent Auslauf | H+A | A₁, B₁, J_Rad, Umfang |
| 8 | Rad aufgelegt, Auflast und Position bestätigt | H | bestätigt |
| 9 | Warmlauf 28,8 km/h, 30 min, Live-Punkt je Minute | A | P(t), Endwert, Konvergenz |
| 10 | Plateaus 15 / 28,8 / 40 / 50 / 60 km/h, je 3 × 10 Umdrehungen | A | je Plateau M, σ, F_roll, Crr roh, Crr korr, u(Crr), gültig |
| 11 | Ankerpunkt 28,8 km/h wiederholen | A | Drift ΔM, ΔCrr |
| 12 | Auslaufen, unter 45 km/h bremsen | A | Dauer |
| 13 | Nachlauf Leerlauf (Rad abheben) | H+A, überspringbar | Drift Vor/Nach je Plateau oder W05 |
| 14 | Protokoll: Zusammenfassung, Warnungen, .md und .csv | A | Dateien |

Zeitbedarf ohne Warmlauf: Vorbereitung 5 bis 10 min, Vorlauf 4 min, Messfahrt 6 min,
Anker 1 min, Auslauf 1 min, Nachlauf 4 min. **Mit Warmlauf rund 50 min**, davon 40 ohne
Bediener.

## 4 Protokollformat (Markdown)

Vom ESP gerendert, von `crr.py` reproduzierbar. Aufbau:

```
# Messlauf 2026-11-23-03
Reifen · Laufrad · Druck · Last · Raumtemperatur · Bediener · Build

## Prüfungen            Tabelle: Prüfung | Ergebnis | Wert
## Kalibrierstand       Tabelle: Größe | Wert | Herkunft | Stand (Zeit)
## Warmlauf             Tabelle P(t) je Minute, Endwert, Änderung letzte 5 min
## Messpunkte           Tabelle: v | n | M ± σ | F_roll | Crr roh | Crr korr ± u | gültig
## Anker                ΔM, ΔCrr zwischen erstem und letztem 28,8-Punkt
## Leerlauf Vor/Nach    Tabelle je Plateau, Kriterium 0,02 N auf F_roll
## Warnungen            W01 … mit Text
## Rohdaten             Verweis auf die .csv, Zeilenzahl, Prüfsumme
```

Jede Zahl mit Einheit, SI intern, km/h und bar nur in den Spalten, die so heißen
([CLAUDE.md](../CLAUDE.md)). Keine Zahl ohne Herkunft: gemessen, gerechnet, eingegeben,
übernommen (mit Zeit).

## 5 Offene Entscheidungen

| | Frage | Vorschlag |
|---|---|---|
| E1 | Nachlauf als optionaler letzter Schritt mit Handgriff, oder weglassen? | optional, Warnung W05 bei Verzicht (R6) |
| E2 | Drei Punkte je Plateau statt einem? | ja (R4) |
| E3 | Warmlauf fest 30 min oder Konvergenzkriterium? | zuerst fest und protokollieren, Kriterium später aus Daten (R7) |
| E4 | Laufsatz auf SD-Karte oder im Flash des ESP? | SD, weil die Rohdaten ohnehin dort liegen sollen |
| E5 | Kontrollgewicht als fester Schritt oder nur nach Umbauten? | fest, überspringbar (R2) |
| E6 | Rauschgrenze der Zelle im Stillstand | TBD, aus den ersten zehn Läufen ableiten |
