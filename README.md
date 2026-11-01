# Fahrrad-Reifenprüfstand (Rollwiderstand)

Prüfstand zur Messung des Rollwiderstands von Fahrradreifen auf einer Trommel mit 2,93 m
Umfang. Der Antrieb ist als Pendelmaschine aufgebaut; aus dem Reaktionsmoment und der
Trommeldrehzahl werden Verlustleistung und Rollwiderstandsbeiwert Crr berechnet.

**Status: im Aufbau.** Trommel gebaut, Motor und Regler beschafft, Dokumente und
Firmware-Gerüst stehen. Offen sind Kalibrierung der Wägezelle, Bremse und
Temperaturfühler. Siehe [docs/06-offene-fragen.md](docs/06-offene-fragen.md).

## Messprinzip in einer Zeile

Der Motor sitzt auf einem Pendellager und stützt sich über einen Hebelarm L gegen eine
Wägezelle ab. Daraus folgt die Rollwiderstandskraft am Latsch **ohne** Kenntnis der
Geschwindigkeit:

```
F_roll = F_Zelle · L / r_Trommel
Crr    = (F_roll − F_roll,leer) / F_Auflast
```

Die Geschwindigkeit wird nur für die Leistungsanzeige und für die
geschwindigkeitsabhängige Leerlaufkorrektur gebraucht.

Weil die Trommel direkt auf dem Motorrotor sitzt, liegen die Trommellager **innerhalb**
der Pendelwiege und fallen aus der Momentenbilanz heraus. Im Leerlauf bleibt nur der
Luftwiderstand der Trommel übrig, rund 0,27 Nm bei 60 km/h.

## Dokumente

| Datei | Inhalt |
|---|---|
| [docs/01-requirements.md](docs/01-requirements.md) | Anforderungen, Nicht-Ziele |
| [docs/03-parameter.md](docs/03-parameter.md) | Alle Parameter mit Quelle und Kalibrierung |
| [docs/04-hardware.md](docs/04-hardware.md) | Blockbild, Stückliste, Pinbelegung |
| [docs/05-pruefablauf.md](docs/05-pruefablauf.md) | Mess-SOP |
| [docs/06-offene-fragen.md](docs/06-offene-fragen.md) | Entscheidungsliste |
| [docs/07-sicherheit.md](docs/07-sicherheit.md) | Gefährdungen, Schutzmaßnahmen, Checkliste |
| [docs/08-staender.md](docs/08-staender.md) | Ständer: Maßkette, Trommellagerung, Auflast-Hebelwaage, Teileliste |
| [docs/09-messtheorie.md](docs/09-messtheorie.md) | Formeln, Fehlerbudget, Sensorik, Kalibrierung, Messmatrix, Ablauf |
| [docs/10-firmware.md](docs/10-firmware.md) | Firmware-Anforderungen |

Lücke bei 02: das frühere Messkonzept ist in 09 aufgegangen.

## Quickstart

```sh
# Firmware bauen (Trockentest mit Stub-Antrieb, keine externen Bibliotheken nötig)
cd firmware && pio run -e esp32s3

# Auf ESP32 flashen und serielle Konsole öffnen
pio run -t upload && pio device monitor -b 115200

# Messung auswerten
python3 analysis/crr.py data/messung.csv

# Querprüfung Konstanten gegen Dokumentation
python3 analysis/check_params.py
```

## Eckdaten

| Größe | Wert |
|---|---|
| Trommel | Ø 933 mm, U = 2,931 m, Breite 100 mm, 15-mm-Multiplex, ~17 kg |
| Geschwindigkeitsbereich | 10–60 km/h (57–341 1/min) |
| Antrieb | Hoverboard-Nabenmotor 6,5″, 36 V / 350 W, **Direktantrieb**, Regler FSESC 4.20 |
| Momentenmessung | Pendelmaschine, Hebelarm 0,040 m, Wägezelle 10 kg, HX711 @ 80 SPS |
| Auflast | 417 N (42,5 kg), Standard der publizierten Crr-Datenbank |
| Auslegungs-Crr | 0,007 (schnelle Straßenreifen, kein MTB) |
| Leistungsbedarf | 90 W elektrisch im Auslegungsfall, Netzteil 36 V / 10 A |
| Drehzahlmessung | Hall TLE4905L, 8 Marken + Index + Laufrad |
| Rechner | ESP32-S3-DevKitC-1 N16R8, Speisung über USB-C |
| Anzeige | HD44780 4×20 über PCF8574 (I²C) |
