# Offene Fragen

Sortiert nach Blockerwirkung. Stand: 2026-10-24.

## A — Blockiert den Aufbau

### A1 Steckerbelegung des Motors
Beim Scooter-Motor hat der Julet-Stecker 9 Pole. Erwartet: 3 dicke Adern = Phasen,
5 dünne = Hall (Vcc, GND, Ha, Hb, Hc), 1 weiße = PTC. Beim Hoverboard-Rad sind es
3 Flachstecker (gelb/grün/blau) plus ein weißer JST mit 5 dünnen Adern.

In beiden Fällen gilt: **mit dem Messgerät verifizieren**, bevor irgendetwas
angeschlossen wird. Eine vertauschte Hall-Versorgung zerstört die Sensoren.

Außerdem offen: Gegenstecker beschaffen oder abschneiden und eigene Verbinder setzen?

### A9 Bremse, falls das Hoverboard-Rad genommen wird
Dem Hoverboard-Rad fehlt die Trommelbremse. Bei 13 Minuten freiem Auslauf braucht es
dann eine Bandbremse oder einen Bremssattel auf einer Scheibe an der Trommelwelle.
Ebenso fehlt der Temperaturfühler — ein NTC auf der Statorachse ersetzt ihn.

### A5 Kennlinie des Motor-PTC
Der Motor hat einen Temperaturfühler, aber die Kennlinie ist unbekannt. Ohne sie lässt
sich kein Grenzwert festlegen. Ermittlung: Widerstand bei Raumtemperatur messen und mit
den üblichen Typen vergleichen (KTY, PT1000, einfacher Schaltschwellen-PTC). Bis dahin
bleibt die Temperaturabschaltung in [07-sicherheit](07-sicherheit.md) §4 ohne Grenzwert.

### A6 Ist die Motorachse zweiseitig gelagert?
Scooter-Vorderräder sitzen üblicherweise beidseitig in der Gabel. Falls ja, ist die
Pendellagerung unkompliziert: beide Achsenden in Stehlager, Drehmomentarm an einem Ende.
Falls einseitig: wie beim Hoverboard-Rad entsprechend auslegen.

### A8 Bremskonzept und Rückspeisung — **entschieden**
**Kurzschlussbremsung statt Bremswiderstand.** Werden die drei Motorphasen gegeneinander
kurzgeschlossen, bleibt die Energie im Motorkupfer: 1321 J heben die Wicklung um 4 bis
11 K an, also nichts. Der Bremswiderstand mit Chopper entfällt damit ersatzlos.

| Zweck | Mittel | funktioniert ohne |
|---|---|---|
| normales Anhalten | Regler im Kurzschlussmodus | — braucht den Regler |
| **Not-Aus** | dreipoliger Schütz, Öffnerkontakte: fällt bei Spannungsverlust ab und schließt die Phasen kurz | Regler **und** Strom |
| Feststellen | mechanische Trommelbremse | alles |

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
(f ∝ √(k/J), siehe [02-messkonzept](02-messkonzept.md)). Ein 9,5″-Scooter-Nabenmotor ist
deutlich schwerer als das Hoverboard-Rad mit 2,26 kg. Wiegen und prüfen, ob die
geschätzte Eigenfrequenz von 50–150 Hz noch über der Zahneingriffsfrequenz des Riemens
liegt (bei 605 1/min und 20 Zähnen rund 200 Hz — das könnte eng werden).

### A2 Zulässige Höchstdrehzahl der Trommel
Hängt an Holzaufbau, Nabenanbindung und Wuchtgüte. Entscheidet, ob über 60 km/h hinaus
überhaupt etwas geht. **Das ist die härteste Einschränkung am ganzen Konzept** — nicht
die Elektronik. Siehe [07-sicherheit](07-sicherheit.md).

### A10 Trommel in eigenen Lagern oder auf dem Motorrotor?
Zwei baubare Anordnungen, beide kosten rund 1 % am Signal, aber an verschiedenen Stellen —
Gegenüberstellung mit Zahlen in [08-staender](08-staender.md) §1:

- **Trommel auf dem Rotor** (bisher in [04-hardware](04-hardware.md) §2): Trommellager
  fallen aus der Momentenbilanz, aber die Pendellager tragen Trommel, Motor und Auflast
  (≈ 650 N) und bringen 9,8 mNm Hysterese — nicht korrigierbar.
- **Trommel auf eigener, mitdrehender Welle**, abgesetzt Ø15/Ø30, 2 × 6902 auf den
  Zapfen im Rahmen, Motor über Flansch und radial weiche Kupplung am Wellenende:
  Pendellager fast lastfrei (1,2 mNm), dafür stehen die Trommellager im Momentenpfad und
  tragen 4,7 mNm lastabhängig bei — glatt, reproduzierbar und über A11 messbar.

Der Entwurf in [08-staender](08-staender.md) geht von der zweiten Variante aus, weil sie
ohne die Rotormaße baubar ist (A4 blockiert sie nicht). Bedingung ist eine **radial
weiche** Kupplung: eine starre Flanschverbindung macht die Motorlager zum zweiten
Trommellager und hebt den Vorteil auf.

### A11 Referenzlauf mit starrer Rolle
Eine Stahl- oder PA-Rolle anstelle des Reifens, gleiche Auflast: ihre Hysterese ist
vernachlässigbar, gemessen wird praktisch nur der lastabhängige Lageranteil. Damit wird
die Korrektur aus A10 gemessen statt geschätzt. Gehört als Schritt in
[05-prüfablauf](05-pruefablauf.md); Rollendurchmesser und Material noch offen.

### A12 Trommelbreite 250 mm — Leerlauf neu rechnen
[02-messkonzept](02-messkonzept.md) §4.1 rechnet die Mantelfläche mit b = 0,1 m. Mit den
jetzt geplanten 0,25 m steigt die Trommelaerodynamik von 0,29 auf **0,43 Nm** bei 60 km/h
und damit von 39 auf 58 % des Rennreifensignals. Zahlen in
[08-staender](08-staender.md) §10.2. Zu entscheiden: Breite bestätigen und 02-messkonzept,
README und die Momentenbilanz nachziehen — oder die Trommel schmaler bauen.

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

### B2 ESP32-Board
Konkretes Modell bestimmt die endgültige Pinbelegung. GPIO34–39 sind reine Eingänge,
einige Pins sind beim Boot belegt.

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
| Mehrere Marken sinnvoll? | Ja — nicht wegen der Crr-Genauigkeit (die braucht kein v), sondern für Regelung, Schlupferkennung und winkelaufgelöste Diagnose | 2026-10-24 |
| Drehmomentwaage bedämpfen? | Ja, aber zuletzt: erst steif und leicht bauen, Anregung vermeiden, umdrehungssynchron mitteln — dann Wirbelstromdämpfer. Nichts mit Haftreibung im Momentenpfad. | 2026-10-24 |
| Gimbal-Motor direkt auf der Nabe? | Nein — Faktor ~10 zu wenig Moment, dazu Gegen-EMK-Grenze | 2026-10-24 |
| Antrieb | Nabenmotor Bird One / DAAO 23328N36A28G001, 36 V / 350 W, 605 1/min, Zahnriemen i ≈ 1,8 | 2026-10-24 |
| Hoverboard- oder Scooter-Motor? | **Offen geblieben, Preisfrage.** Beide haben Hallsensoren (beim Hoverboard-Rad an den Angebotsfotos verifiziert: 3 dicke Phasen + weißer JST mit 5 dünnen Adern, also 8 Leiter statt der beschriebenen 6). Beide erfüllen 3,53 Nm mit Reserve, das Hoverboard-Rad mit 6,96 Nm sogar mehr. Unterschied ist nur noch die mechanische Bremse. Entscheidungsregel in [03-parameter](03-parameter.md) §4. | 2026-10-24 |
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
