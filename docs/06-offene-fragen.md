# Offene Fragen

Sortiert nach Blockerwirkung. Stand: 2026-10-24.

## A — Blockiert den Aufbau

### A1 Steckerbelegung des Motors
Verbaut ist das Hoverboard-Rad: **3 Flachstecker (gelb/grün/blau) = Phasen, dazu ein
weißer JST mit 6 dünnen Adern** = Vcc, GND, Ha, Hb, Hc und vermutlich ein
Temperaturfühler (A5). Belegung mit dem Messgerät verifizieren.

In beiden Fällen gilt: **mit dem Messgerät verifizieren**, bevor irgendetwas
angeschlossen wird. Eine vertauschte Hall-Versorgung zerstört die Sensoren.

Außerdem offen: Gegenstecker beschaffen oder abschneiden und eigene Verbinder setzen?

### A13 Drehzahlreserve — **neu, kann den Bus auf 42 V zwingen**
Erster Versuch 2026-11-01 mit BLDC-Tester: **über 400 1/min bei 40 V**, nicht am
Anschlag. Daraus kv ≥ 10 1/min/V statt der angenommenen 13,3.

Für 60 km/h braucht die Trommel **353 1/min**. Bei kv = 10 und 36 V wären das im
Leerlauf 360 1/min — rechnerisch genug, aber **ohne jede Reserve**, und unter Last
sinkt die Drehzahl durch den Spannungsabfall weiter. Bei 40 V sind es 400 1/min
(68 km/h), damit geht 60 km/h auch belastet.

Zu messen: Motor mit der Bohrmaschine auf bekannte Drehzahl bringen, Phase-Phase-
Wechselspannung messen, daraus kv exakt. Danach entscheiden, ob das Netzteil auf
**40–42 V** ausgelegt wird statt auf 36 V.

Hängt mit A14 zusammen: ein 42-V-Bus liegt über der 40-V-Grenze des LM2596, für den
12-V-Zweig ist deshalb der XL7015 vorgesehen (5–80 V).

### A5 Temperaturfühler: Typ bestimmen
Der Hall-Stecker hat **6 Adern**. Vcc, GND und drei Hall-Signale sind fünf — die
sechste ist vermutlich ein NTC gegen GND.

Widerstand 6. Ader gegen GND bei Raumtemperatur messen:

| Messwert | Deutung |
|---|---|
| ≈ 10 kΩ, fällt beim Anwärmen | NTC 10 k, vermutlich B 3950 |
| ≈ 100 Ω / 1 kΩ, steigt beim Anwärmen | PT100 / PT1000 |
| offen oder 0 Ω | kein Fühler, NTC nachrüsten |

Danach Grenzwert festlegen und in [07-sicherheit](07-sicherheit.md) §4 eintragen.
Anschluss an `PIN_MOTOR_PTC` = GPIO1 (ADC1).

### A7 Masse und Trägheitsmoment des Stators
Nur der **Stator** hängt am Hebelarm, nicht das ganze Rad (2,26 kg). Sein
Trägheitsmoment bestimmt die Eigenfrequenz der Waage: 0,007 kg·m² ergeben 43,5 Hz,
0,026 kg·m² nur noch 25,7 Hz ([09-messtheorie](09-messtheorie.md) §3.5). Unter 17 Hz
käme die 3. Ordnung der Trommeldrehung in die Nähe.

Stator wiegen, Trägheitsradius abschätzen, Eigenfrequenz nachrechnen.

### A2 Zulässige Höchstdrehzahl der Trommel
Hängt an Holzaufbau, Nabenanbindung und Wuchtgüte. Entscheidet, ob über 60 km/h hinaus
überhaupt etwas geht. **Das ist die härteste Einschränkung am ganzen Konzept** — nicht
die Elektronik. Siehe [07-sicherheit](07-sicherheit.md).

### A11 Referenzlauf mit starrer Rolle
Eine Stahl- oder PA-Rolle anstelle des Reifens, gleiche Auflast: ihre Hysterese ist
vernachlässigbar, gemessen wird praktisch nur der lastabhängige Lageranteil. Damit wird
die Korrektur aus A10 gemessen statt geschätzt. Gehört als Schritt in
[05-prüfablauf](05-pruefablauf.md); Rollendurchmesser und Material noch offen.

### A3 Auflast: Abgleich und zweite Wägezelle
Nominalwert und Mechanik stehen (417 N, Hebelwaage mit Laufgewicht,
[08-staender](08-staender.md) §5). Offen ist nur noch:

- Abgleich gegen eine unabhängige Referenz — die Auflast geht **linear** in Crr ein und
  muss auf 1 % stimmen.
- Kommt eine zweite Wägezelle unter die Trommellagerkonsolen? Dann wäre die Auflast
  gemessen statt gerechnet.

## B — Blockiert die Firmware

### B3 LCD-Anschluss und Pegel
I²C-Backpack (PCF8574) ist gesetzt. Offen: Läuft das vorhandene Modul auf 3,3 V, oder
wird ein Pegelwandler gebraucht? I²C-Adresse (0x27 oder 0x3F)?

### B4 Luftdrucksensor
Welches Modell? Die Beschreibung „Airspy" deutet auf einen SKS Airspy hin — zu
verifizieren. Ohne bekanntes BLE-Profil (Service- und Characteristic-UUIDs) bleibt es bei
der Handeingabe. Mögliches Vorgehen: mit einem BLE-Scanner die Characteristics auslesen.

## C — Methodisch

### C1 Trommelkrümmungs- und Oberflächenkorrektur
Zwei systematische Abweichungen gegenüber publizierten Werten, beide unquantifiziert:
die **Krümmung** (unsere Trommel ist mit 0,955 m größer als die 0,77 m des
Referenzprüfstands, also weniger Zuschlag) und die **Oberfläche** (glattes Holz statt
Riffelblech, also niedrigerer Crr). Beide wirken in dieselbe Richtung: unsere Werte
liegen unter den publizierten. Gesucht ist ein belastbarer Korrekturansatz aus der
Literatur — **wird nicht geraten**. Bis dahin: nur Vergleiche, keine Absolutwerte.

### C2 Referenzreifen
Ein Reifen mit publiziertem Crr als Plausibilitätsanker. Die Absolutwerte werden wegen
C1 nicht übereinstimmen, aber Größenordnung und Rangfolge mehrerer Reifen müssen passen.
Marathon Plus (0,00764) wäre ein guter Kandidat: weit verbreitet, gut dokumentiert, und
mit 25,5 W deutlich genug über dem Rauschen.

### C3 Temperaturerfassung
Reifentemperatur und Umgebungstemperatur sollen mitgeloggt werden. Sensor und Messstelle
offen — berührungslos auf die Lauffläche wäre richtig, ist aber bei drehender Trommel
unterzubringen.

### C4 Einschwingzeit und Mittelungslänge
`SETTLE_MS` = 15 s und `AVG_REVS_PER_POINT` = 10 sind geschätzt. Nach den ersten echten
Messungen anhand der Streuung nachziehen.

### C5 Luftzug
Die Trommelaerodynamik dominiert den Leerlauf bei hoher Geschwindigkeit. Wie stark
reagiert sie auf Türen, Lüftung, Position im Raum? Einmal gezielt ausprobieren.

## D — Kleinigkeiten

### D2 Datenablage
Rohdaten unter `data/` versionieren — ab welcher Menge wird das unhandlich?

## Erledigt

| Frage | Entscheidung | Datum |
|---|---|---|
| Rechnerplattform | ESP32 (BLE für den Drucksensor, WLAN-Logging, ein Board für alles) | 2026-10-24 |
| Geschwindigkeitsbereich | 10–60 km/h; 100 km/h erst nach Trommelfreigabe | 2026-10-24 |
| Drehzahlaufnehmer | Hall statt Reed, 6 Marken + Index; Geschwindigkeit aus dem Fenster über eine ganze Umdrehung | 2026-10-24 |
| Trommellagerung (A10) | **Eigene mitdrehende Welle** statt Trommel auf dem Motorrotor. 4,7 mNm lastabhängig im Momentenpfad statt 9,8 mNm nicht korrigierbarer Hysterese. | 2026-11-01 |
| Trommelbreite (A12) | **100 mm** gebaut. Leerlauf bleibt bei 0,27 Nm. | 2026-11-01 |
| **Bremse (A8, A9)** | **Kurzschlussbremsung über drei KFZ-Wechselrelais an den Öffnern.** Gemessen 2026-11-01: 25 s aus über 400 1/min, mittleres Bremsmoment 3,25 Nm, 68 W im Kupfer. Schaltung in [04-hardware](04-hardware.md) §9. Kein Bremswiderstand, kein DC-DC-Wandler. | 2026-11-03 |
| Lagerung der Motorachse (A6) | Gegenstandslos: die Trommel hat eine eigene Welle (A10), die Statorachse wird nur vom Hebelarm gehalten. | 2026-11-03 |
| Trommelbefestigung (A4) | Gegenstandslos: Flansch am Wellenende statt Adapterplatte auf dem Rotor. | 2026-11-03 |
| Motorregler (B1) | **FSESC 4.20 / 50 A** beschafft, VESC-Klasse mit UART. | 2026-11-01 |
| SimpleFOC-Plattform (B5) | Gegenstandslos: mit dem FSESC entfällt der SimpleFOC-Pfad. | 2026-11-03 |
| ESP32-Board (B2) | ESP32-S3-DevKitC-1 N16R8 beschafft. | 2026-11-01 |
| Verzeichnisname (D1) | Heißt korrekt `rolling-resistance`. | 2026-11-03 |
| Motor | **Hoverboard-Antriebsrad 6,5″, 36 V / 350 W** beschafft. Damit 6,96 Nm Dauermoment (Faktor 2,8 über dem Auslegungsfall), aber **ohne Bremse und ohne Temperaturfühler** — A9 und A5 werden dadurch akut. | 2026-11-01 |
| Motorregler | **FSESC 4.20 / 50 A** beschafft, VESC-Klasse mit UART. Schließt B1. | 2026-11-01 |
| Speisung der Logik | **Nur über USB-C.** 5V-Pin und USB schließen sich laut Espressif gegenseitig aus; eine vergessene externe 5 V beim Flashen wäre ein Kurzschluss zweier Quellen. Beide USB-Buchsen dürfen gleichzeitig speisen. | 2026-11-01 |
| HX711-Pegel | Modul mit **getrennten VCC/VDD**: VCC = 5 V für die Brücke, VDD = 3,3 V für den Logikpegel. Der ESP32-S3 ist nicht 5-V-tolerant. Alternative (alles auf 3,3 V) kostet 3,6 dB SNR und wurde verworfen. | 2026-11-01 |
| Markenzahl endgültig | **8** statt 6 — auf dem Teilkreis der 8 Abstandsklötze, geschraubte Ø10×3-Magnete mit A2-Schraube. Mehr Marken bringen *keine* Genauigkeit (das Ergebnis läuft index-getriggert über ganze Umdrehungen, dort kürzt sich die Teilung heraus), nur Latenz. [04-hardware](04-hardware.md) §9 | 2026-11-01 |
| Rechnerplattform konkret | ESP32-**S3**-DevKitC-1 N16R8. Alte Pinbelegung (25/26/27/32/33/34/35/22) lag fast vollständig auf Pins, die es am S3 nicht gibt oder die Flash/PSRAM belegen. | 2026-11-01 |
| Auslegungs-Crr und Hebelarm | `CRR_DESIGN_MAX` 0,015 → **0,007** (keine MTB-Reifen), dadurch `TORQUE_ARM_M` 0,070 → **0,040 m**. Der Auslegungsfall nutzt jetzt 48 % statt 27 % des Zellenmessbereichs. [08-staender](08-staender.md) §5 | 2026-11-01 |
| Trommelumfang | Gemessen: Ø 933 mm, U = **2,931 m**. `MAX_DRUM_RPM` wird daraus abgeleitet (341 1/min) statt fest eingetragen — der alte feste Wert 333 hätte bei 58,4 km/h abgeriegelt. | 2026-11-01 |
| Mehrere Marken sinnvoll? | Ja — nicht wegen der Crr-Genauigkeit (die braucht kein v), sondern für Regelung, Schlupferkennung und winkelaufgelöste Diagnose | 2026-10-24 |
| Drehmomentwaage bedämpfen? | Ja, aber zuletzt: erst steif und leicht bauen, Anregung vermeiden, umdrehungssynchron mitteln — dann Wirbelstromdämpfer. Nichts mit Haftreibung im Momentenpfad. | 2026-10-24 |
| Gimbal-Motor direkt auf der Nabe? | Nein — Faktor ~10 zu wenig Moment, dazu Gegen-EMK-Grenze | 2026-10-24 |
| Antrieb | Nabenmotor Bird One / DAAO 23328N36A28G001, 36 V / 350 W, 605 1/min, Zahnriemen i ≈ 1,8 | 2026-10-24 |
| Hoverboard- oder Scooter-Motor? | **Entschieden 2026-11-01: Hoverboard.** Frühere Notiz: **Offen geblieben, Preisfrage.** Beide haben Hallsensoren (beim Hoverboard-Rad an den Angebotsfotos verifiziert: 3 dicke Phasen + weißer JST mit 5 dünnen Adern, also 8 Leiter statt der beschriebenen 6). Beide erfüllen 3,53 Nm mit Reserve, das Hoverboard-Rad mit 6,96 Nm sogar mehr. Unterschied ist nur noch die mechanische Bremse. Entscheidungsregel in [03-parameter](03-parameter.md) §4. | 2026-10-24 |
| Direktantrieb statt Riemen? | Nein. Drehzahlmäßig möglich (605 1/min = 109 km/h an der Trommel) und messtechnisch besser, aber nur 5,52 statt 9,94 Nm. Zu wenig Moment wäre ein Neubau, Riemenverlust nur eine Zahl zum Abziehen. | 2026-10-24 |
| Reserve für 100 km/h | über die Busspannung, nicht über eine Übersetzung | 2026-10-24 |
| Riemen oder Direktantrieb? | **Direktantrieb.** Die Momentenbilanz der Pendelwiege (`T_Arm = T_außen`) zeigt: ein Riemen liegt außerhalb der Wiege und wird mitgemessen, die Trommellager beim Direktantrieb dagegen innerhalb und fallen heraus. Nach der Literaturrecherche reichen 5,52 Nm mit 36 % Reserve. | 2026-10-24 |
| Momentenbedarf | Neu gerechnet statt geschätzt: 3,53 Nm stationär im Auslegungsfall, nicht 3–6 Nm. Der Leerlauf beträgt 0,29 Nm, nicht 1–4 Nm. | 2026-10-24 |
| Standard-Auflast | 42,5 kg (417 N), wie in der publizierten Crr-Datenbank | 2026-10-24 |
| Hebelarm | 0,07 m statt 0,15 m — der ungünstigste Fall landet damit bei 61 % des Zellenendwerts statt bei 29 % | 2026-10-24 |
| Warmlaufzeit | 30 min statt 10 min, nach dem Vorbild des Referenzprüfstands | 2026-10-24 |
| Motorauswahl allgemein | Zwei Kriterien statt Watt: `kv = n_nenn/U_nenn` muss 14–20 1/min/V sein, `T_dauer = P_nenn/ω_nenn` mindestens 3,53 Nm. Praktisch 250 W Minimum, 350 W richtig. Tabelle in [03-parameter](03-parameter.md) §4. | 2026-10-24 |
| LG-Direct-Drive-Waschmaschinenmotor | Verworfen: Hochvoltmaschine (100–150 V Bus nötig), und das gesichtete Angebot enthielt nur den Rotor ohne Stator. | 2026-10-24 |
| Reibantrieb auf der Trommeloberfläche? | Nein — bräuchte 1930 1/min statt 480 | 2026-10-24 |
| Trommel fliegend auf der Nabe? | Nein — einseitiger Achsstummel, Trommel in zwei eigenen Lagern | 2026-10-24 |
