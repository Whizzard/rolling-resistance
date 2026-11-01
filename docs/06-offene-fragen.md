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

### A9 Bremse — **akut, sicherheitsrelevant**
Verbaut ist das Hoverboard-Rad, und dem fehlt die Trommelbremse. Zusammen mit dem
zurückgestellten A8 heißt das: **es gibt derzeit überhaupt keinen Bremspfad.** Nach
einem Not-Aus läuft die Trommel 3 bis 6 Minuten aus, und es gibt keine Feststellbremse
für Arbeiten am Aufbau.

Zu beschaffen ist eins von beidem:

- **Bandbremse oder Bremssattel auf einer Scheibe an der Trommelwelle** — wirkt auch
  stromlos, taugt als Feststellbremse. Muss im Messbetrieb vollständig lösbar sein
  (X5), sonst driftet das Schleppmoment mit der Temperatur.
- **Kurzschlussschütz aus A8** — bremst schnell, aber nur im Lauf und nicht stromlos
  haltend.

Beides zusammen wäre richtig: elektrisch bremsen, mechanisch feststellen. Bis dahin
gilt die Zwischenregel in [07-sicherheit](07-sicherheit.md) §3 (Keil zwischen Rahmen
und Speiche vor Arbeiten am Aufbau).

Ebenso fehlt der Temperaturfühler — ein NTC auf der Statorachse ersetzt ihn (A5).

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

### A6 Ist die Motorachse zweiseitig gelagert?
Scooter-Vorderräder sitzen üblicherweise beidseitig in der Gabel. Falls ja, ist die
Pendellagerung unkompliziert: beide Achsenden in Stehlager, Drehmomentarm an einem Ende.
Falls einseitig: wie beim Hoverboard-Rad entsprechend auslegen.

### A8 Bremskonzept und Rückspeisung — **zurückgestellt**
> **Stand 2026-10-31: zurückgestellt.** Der Schütz wird vorerst nicht beschafft.
> Als Rückfall ist eine **mechanische Bremse** vorgesehen — der DAAO-Motor hat eine
> Trommelbremse integriert. Die Analyse unten bleibt gültig und ist die Vorlage, falls
> der elektrische Pfad doch gebaut wird. Bis dahin gilt: **frei auslaufen lassen**
> ([09-messtheorie](09-messtheorie.md) §8.9, 3 bis 80 min je nach Schleppmoment).

**Kurzschlussbremsung statt Bremswiderstand.** Werden die drei Motorphasen gegeneinander
kurzgeschlossen, bleibt die Energie im Motorkupfer: 1321 J heben die Wicklung um 4 bis
11 K an, also nichts. Der Bremswiderstand mit Chopper entfällt damit ersatzlos.

| Zweck | Mittel | funktioniert ohne |
|---|---|---|
| normales Anhalten | Regler im Kurzschlussmodus | — braucht den Regler |
| **Not-Aus** | dreipoliger Schütz, Öffnerkontakte: fällt bei Spannungsverlust ab und schließt die Phasen kurz | Regler **und** Strom |
| Feststellen | mechanische Bremse — **nicht vorhanden**, siehe A9 | alles |

Zwei Öffnerkontakte genügen (U–V und V–W verbinden alle drei). Strom bei 60 km/h rund
**60 A** bei 20,3 V Gegen-EMK — der Kontakt schaltet aber **stromlos** (beim Schließen
fließt noch nichts, geöffnet wird erst im Stillstand), er muss nur tragen. Leistungsschütz
ab 20 A mit 2 Öffnern, Spule 24 V DC aus derselben geschalteten Versorgung wie der Regler,
damit die Reihenfolge stimmt.

Offen bleibt: **R und L des Motors messen** (die 60 A sind damit gerechnet), und der
Kurzschluss muss beim Auslaufversuch offen sein, sonst misst man die Bremskurve statt der
Verlustkurve. Bremsmoment 5…20 Nm ergibt 15…4 s Bremszeit; der Reifen rutscht dabei nicht
(4 N nötige Latschkraft gegen 334 N Haftgrenze).

> **Die Wägezelle muss dabei geschützt sein.** Das Bremsmoment geht über den Stator in den
> Hebelarm. Mit dem Zugglied aus Schaltzug ([08-staender](08-staender.md) §7.3) knickt
> dieser bei 6,6 N und die Last geht in den Anschlag — ohne diese Maßnahme wären es 250 bis
> 500 N an einer Zelle mit 98 N Endwert.

### A7 Masse des Motors
Geht in die Trägheit des Wiegeteils und damit in dessen Eigenfrequenz ein
(f ∝ √(k/J), siehe [09-messtheorie](09-messtheorie.md)). Ein 9,5″-Scooter-Nabenmotor ist
deutlich schwerer als das Hoverboard-Rad mit 2,26 kg. Wiegen und prüfen, ob die
geschätzte Eigenfrequenz von 50–150 Hz noch über der Zahneingriffsfrequenz des Riemens
liegt (bei 605 1/min und 20 Zähnen rund 200 Hz — das könnte eng werden).

### A2 Zulässige Höchstdrehzahl der Trommel
Hängt an Holzaufbau, Nabenanbindung und Wuchtgüte. Entscheidet, ob über 60 km/h hinaus
überhaupt etwas geht. **Das ist die härteste Einschränkung am ganzen Konzept** — nicht
die Elektronik. Siehe [07-sicherheit](07-sicherheit.md).

### ~~A10 Trommel in eigenen Lagern oder auf dem Motorrotor?~~ — **entschieden**
**Eigene, mitdrehende Welle**, abgesetzt Ø15/Ø30, 2 × 6902 im Rahmen, Motor über Flansch
am Wellenende. Die Trommellager stehen damit im Momentenpfad und tragen 4,7 mNm
lastabhängig bei — glatt, reproduzierbar und über A11 messbar. Die Alternative (Trommel
auf dem Rotor) hätte die Pendellager mit 650 N belastet und 9,8 mNm **nicht
korrigierbare** Hysterese gebracht. Zahlen in [08-staender](08-staender.md) §1.

Der Entwurf in [08-staender](08-staender.md) geht von der zweiten Variante aus, weil sie
ohne die Rotormaße baubar ist (A4 blockiert sie nicht). Bedingung ist eine **radial
weiche** Kupplung: eine starre Flanschverbindung macht die Motorlager zum zweiten
Trommellager und hebt den Vorteil auf.

### A11 Referenzlauf mit starrer Rolle
Eine Stahl- oder PA-Rolle anstelle des Reifens, gleiche Auflast: ihre Hysterese ist
vernachlässigbar, gemessen wird praktisch nur der lastabhängige Lageranteil. Damit wird
die Korrektur aus A10 gemessen statt geschätzt. Gehört als Schritt in
[05-prüfablauf](05-pruefablauf.md); Rollendurchmesser und Material noch offen.

### ~~A12 Trommelbreite~~ — **erledigt**
Gebaut sind **100 mm**. Die Trommelaerodynamik bleibt damit bei 0,27 Nm
([09-messtheorie](09-messtheorie.md) §1.8); die 0,43 Nm der verworfenen 250-mm-Variante
sind gegenstandslos.

### A3 Art der Auflast
Totgewicht über Hebel oder Feder plus zweite Wägezelle? Die Auflast geht **linear** in
Crr ein und muss auf 1 % genau bekannt sein.
Der **Nominalwert ist entschieden**: 42,5 kg = 417 N, der De-facto-Standard der
veröffentlichten Crr-Datenbank. Die Mechanik liegt als Entwurf vor:
Hebelwaage mit Laufgewicht 10 kg, a = 200 mm, b = 300…900 mm, Schwingenachse auf
Latschhöhe ([08-staender](08-staender.md) §5). Offen bleibt der Abgleich gegen eine
unabhängige Referenz und die Entscheidung, ob die zweite Wägezelle unter die
Trommellagerkonsolen kommt (dann ist die Auflast gemessen statt gerechnet).

### A4 Befestigung der Trommel auf dem Rotor
Durch den Direktantrieb ist die Riemenscheibe vom Tisch, dafür muss die Trommel auf den
Rotor. Die seitlichen Deckelschrauben sind dafür nicht gedacht — es braucht eine
Adapterplatte, deren Lochkreis zur Felge passt. Maße der Felge noch unbekannt.
Die Trommel muss **symmetrisch zur Motorebene** sitzen, sonst wirkt ein Biegemoment auf
die Motorlager.

## B — Blockiert die Firmware

### B1 Welcher Motorregler?
VESC-Klasse oder umgeflashte Hoverboard-Hauptplatine. Beide sprechen UART, aber mit
unterschiedlichen Frames. Bestimmt die konkrete `IDrive`-Implementierung.
Zu prüfen: Verfügbarkeit, Bremsverhalten, Strombegrenzung, Drehzahlregelung im
unteren Bereich.

### ~~B2 ESP32-Board~~ — **entschieden 2026-11-01**
**ESP32-S3-DevKitC-1, Modul WROOM-1 N16R8** (16 MB Flash, 8 MB Octal-PSRAM), beschafft.
Pinbelegung in [04-hardware](04-hardware.md) §4, verdrahtet in `hardware/schaltplan.py`.
Der Hinweis auf GPIO34–39 galt dem klassischen ESP32; beim S3 gibt es keine reinen
Eingangspins, dafür sperrt das Octal-PSRAM GPIO 33…37 zusätzlich zum Flash auf 26…32.

### B3 LCD-Anschluss und Pegel
I²C-Backpack (PCF8574) ist gesetzt. Offen: Läuft das vorhandene Modul auf 3,3 V, oder
wird ein Pegelwandler gebraucht? I²C-Adresse (0x27 oder 0x3F)?

### B4 Luftdrucksensor
Welches Modell? Die Beschreibung „Airspy" deutet auf einen SKS Airspy hin — zu
verifizieren. Ohne bekanntes BLE-Profil (Service- und Characteristic-UUIDs) bleibt es bei
der Handeingabe. Mögliches Vorgehen: mit einem BLE-Scanner die Characteristics auslesen.

### B5 SimpleFOC-Build braucht eine neuere Plattform
SimpleFOC ab 2.3 verlangt Arduino-esp32 3.x (ESP-IDF 5). Die offizielle
PlatformIO-Plattform `espressif32` liefert nur 2.x; der Build bricht in der Bibliothek
ab. Lösungswege: den `pioarduino`-Fork der Plattform verwenden, eine ältere SimpleFOC-
Version pinnen, oder den Pfad ganz überspringen und direkt mit dem richtigen Regler
anfangen. Entscheidung erst nötig, wenn das Shield tatsächlich verdrahtet wird —
`lib_deps` ist deshalb in `platformio.ini` noch auskommentiert und beide Builds sind grün.

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

### D1 Verzeichnisname
`rolling-reistance` enthält einen Tippfehler (`reistance` → `resistance`). Vor dem
ersten Push korrigieren?

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
