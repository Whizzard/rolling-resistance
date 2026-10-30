# Fahrrad-Reifenprüfstand (Rollwiderstand)

Prüfstand zur Messung des Rollwiderstands von Fahrradreifen auf einer Trommel mit 3 m
Umfang. Der Antrieb ist als Pendelmaschine aufgebaut; aus dem Reaktionsmoment und der
Trommeldrehzahl werden Verlustleistung und Rollwiderstandsbeiwert Crr berechnet.

**Status: Projekt-Rumpf.** Dokumente, Messkonzept und Firmware-Gerüst stehen.
Mechanik ist nicht gebaut, Regler nicht beschafft. Siehe
[docs/06-offene-fragen.md](docs/06-offene-fragen.md).

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
Luftwiderstand der Trommel übrig, rund 0,29 Nm bei 60 km/h — etwa 30 % des
Reifensignals.

## Dokumente

| Datei | Inhalt |
|---|---|
| [docs/01-requirements.md](docs/01-requirements.md) | Anforderungen, Nicht-Ziele |
| [docs/02-messkonzept.md](docs/02-messkonzept.md) | Formeln, Fehlerbudget, Fallstricke |
| [docs/03-parameter.md](docs/03-parameter.md) | Alle Parameter mit Quelle und Kalibrierung |
| [docs/04-hardware.md](docs/04-hardware.md) | Blockbild, Stückliste, Pinbelegung |
| [docs/05-pruefablauf.md](docs/05-pruefablauf.md) | Mess-SOP |
| [docs/06-offene-fragen.md](docs/06-offene-fragen.md) | Entscheidungsliste |
| [docs/07-sicherheit.md](docs/07-sicherheit.md) | Gefährdungen, Schutzmaßnahmen, Checkliste |
| [docs/08-staender.md](docs/08-staender.md) | Ständer: Maßkette, Trommellagerung, Auflast-Hebelwaage, Teileliste |
| [docs/09-messtheorie.md](docs/09-messtheorie.md) | Formeln, Fehlerbudget, Sensorik, Kalibrierung, Messmatrix, Ablauf |

## Quickstart

```sh
# Firmware bauen (Trockentest mit Stub-Antrieb, keine externen Bibliotheken nötig)
cd firmware && pio run

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
| Trommelumfang | 3,000 m (Ø 0,955 m) |
| Geschwindigkeitsbereich | 10–60 km/h (55,6–333 1/min) |
| Antrieb | Nabenmotor 36 V / 350 W / 605 1/min (Bird One), **Direktantrieb** |
| Momentenmessung | Pendelmaschine, Hebelarm 0,07 m, Wägezelle 10 kg, HX711 @ 80 SPS |
| Auflast | 417 N (42,5 kg), Standard der publizierten Crr-Datenbank |
| Leistungsbedarf | 160 W elektrisch im Auslegungsfall, Netzteil 36 V / 10–15 A |
| Drehzahlmessung | Hall-Sensor, 6 Marken + Indexmarke |
| Rechner | ESP32 |
| Anzeige | HD44780 4×20 über PCF8574 (I²C) |
