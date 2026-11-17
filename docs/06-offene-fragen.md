# Offene Fragen

Sortiert nach Blockerwirkung. Stand: **2026-11-17**.

## A — Blockiert den Aufbau

### A1 Steckerbelegung des Motors
Verbaut ist das Hoverboard-Rad: **3 Flachstecker (gelb/grün/blau) = Phasen, dazu ein
weißer JST mit 6 dünnen Adern** (weiß, schwarz, grün, blau, gelb, rot).

Aderfarben, erschlossene Belegung und die Weiterführung an den Regler stehen in
[04-hardware](04-hardware.md) §11. Offen bleibt die **Verifikation mit dem
Messgerät** — Vorschrift in §11.3. Eine vertauschte Hall-Versorgung zerstört die
Sensoren.

Außerdem offen, wie die Verbindung zum Regler zustande kommt — die Reihenfolge der
beiden Sechser stimmt nicht überein (§11.2):

1. **Kontakte aus den Kammern ziehen und umsortieren** ([04-hardware](04-hardware.md)
   §11.5) — der kürzeste Weg, setzt aber voraus, dass beide Stecker dasselbe System
   sind.
2. Gegenstecker beschaffen.
3. Abschneiden und eigene Verbinder setzen.

Was davon geht, entscheidet die Messung von Raster und Kontakttyp an beiden Steckern.

### A5 Temperaturfühler: Typ bestimmen
Die sechste Ader des Hall-Steckers ist **weiß** und läuft am geöffneten Motor nicht
auf die Hall-Platine, sondern in einem Glasseidenschlauch ins Wickelpaket
([04-hardware](04-hardware.md) §11.1). Das ist der Platz eines Fühlers, beweist aber
weder Typ noch Kennlinie.

**Zu tun:** Widerstand weiß gegen schwarz messen und beim Anwärmen beobachten —
Messvorschrift samt Deutungstabelle in [04-hardware](04-hardware.md) §11.3.

**Wohin die Ader geht, ist entschieden:** an `TMP` des **Reglers**, nicht an den
ESP32 — Übertemperatur muss den Treiber sperren, und der Treiber ist der Regler
([04-hardware](04-hardware.md) §11.4). `PIN_MOTOR_PTC` ist aus `config.h` entfallen.

**Fühlertyp beantwortet, 2026-11-17: 10-k-NTC im Wickelpaket.** Dreifach belegt,
jeweils über die Telemetrie des Reglers mitgeschrieben:

1. **Offene Klemme** meldet −75,5 °C, angeschlossen **19,4 °C** — ein NTC an einem
   Pull-up gegen 3V3 verhält sich genau so. Ein Kurzschluss oder ein PTC gäbe das
   andere Vorzeichen.
2. **Plausibilität gegen den bordeigenen FET-Fühler**, der bekannt gut ist: 1,6 K
   Abstand bei Raumtemperatur. Eine falsche Kennlinie läge zweistellig daneben.
3. **Heißluft auf den Rotor**: die Motortemperatur stieg monoton 19,4 → 19,6 °C,
   während die FET-Referenz bei 17,7 °C flach blieb. Ein offener Eingang rauscht,
   er folgt nicht.

Im Regler stand `m_motor_temp_sens_type` bereits auf **NTC 10K at 25 °C** — die
Einstellung war also zufällig richtig und bleibt.

> Der Fühler sitzt im **Stator**. Beim Anwärmen von außen wird der **Rotor** warm, und
> die Masse des Motors verzögert alles. 0,2 K Anstieg in Minuten sind deshalb kein
> schwaches Signal, sondern das erwartete.

Offen bleibt damit nur noch:

1. **Grenzwert festlegen** und in [07-sicherheit](07-sicherheit.md) §4 eintragen.
   Vorschlag: Warnung 80 °C, Abschaltung 100 °C — der Hoverboard-Motor ist für
   Dauerbetrieb bei kleiner Last gebaut, nicht für 20 A über Minuten.

### A7 Eigenfrequenz der Waage — **messen, nicht wiegen**
Gebraucht wird nicht die Masse des Stators, sondern das **Trägheitsmoment** — und selbst
das nur als Zwischenschritt zur Eigenfrequenz:

```
f_n = (1/2π)·√(k_rot/J)      k_rot = k_Zelle · L² = 523 Nm/rad
```

| J_Stator | f_n | Lage zu den Ordnungen bei 60 km/h |
|---|---|---|
| 0,007 kg·m² | 43,5 Hz | über allen |
| 0,026 kg·m² | **22,6 Hz** | zwischen 3. (17,6) und 4. (23,5) |

Wiegen hilft dabei wenig: aus der Masse allein folgt kein J, der Trägheitsradius wäre
geschätzt. **Besser direkt messen:** Hebelarm anschlagen und die Ausschwingfrequenz
aufnehmen. Das erfasst alles — Zelle, Zugglied, Einspannung — statt es zu modellieren.

Nicht mit dem HX711: 80 SPS reichen nur bis 40 Hz. Stattdessen ein Handy mit
Beschleunigungs- und FFT-App auf den Arm kleben, anschlagen, Spitze ablesen.

**Was davon abhängt:** nicht der Messwert. Die Messung ist momentengetrieben, eine
Resonanz vergrößert nur die Amplitude um den Mittelwert und fällt bei synchroner
Mittelung heraus ([09-messtheorie](09-messtheorie.md) §3.5). Gefährlich ist sie nur,
wenn der Arm in die Anschläge läuft oder wenn sie über 40 Hz angeregt wird und sich
dann in die Abtastung faltet (§3.7).

### A2 Zulässige Höchstdrehzahl der Trommel
Hängt an Holzaufbau, Nabenanbindung und Wuchtgüte. Entscheidet, ob über 60 km/h hinaus
überhaupt etwas geht. **Das ist die härteste Einschränkung am ganzen Konzept** — nicht
die Elektronik. Siehe [07-sicherheit](07-sicherheit.md).

### A11 Referenzlauf mit starrer Rolle — **zurückgestellt, vermutlich entbehrlich**
Die lastabhängige Lagerreibung ist **keine Schätzung**, sondern aus dem Reibmodell für
Rillenkugellager gerechnet:

```
M = 0,5 · µ · F · d = 0,5 · 0,0015 · 417 N · 0,015 m = 4,7 mNm
```

Das sind **0,39 %** vom Signal eines guten Rennreifens. Selbst ±50 % Unsicherheit auf µ
lassen davon ±0,2 % übrig — kleiner als alles andere im Budget. Für A/B-Vergleiche fällt
der Anteil ohnehin vollständig heraus (gleiche Last, gleiche Lager).

Falls doch gemessen werden soll: **Laufraddurchmesser ist Pflicht**, nicht Kür. Eine
kleine Rolle verstärkt ihre eigene Lagerreibung mit `r_Trommel/r_Rolle` — bei einer
30-mm-Rolle um Faktor 30. Eine blanke Alufelge direkt auf dem Tränenblech scheidet aus:
Linienkontakt mit 417 N drückt beides ein. Nötig wäre also eine Felge mit **POM- oder
PU-Lauffläche** in Reifengröße, in der vorhandenen Gabel. Dafür Aufwand und Geld gegen
0,2 % — lohnt nicht.

### A14 Welche Trommel gilt? — **Widerspruch, muss der Mensch entscheiden**

Der Trommelumfang steht an vier Stellen, und sie stimmen nicht überein:

| Quelle | Umfang | Durchmesser | J |
|---|---|---|---|
| `firmware/src/config.h` | **2,837 m** | Ø 903 | 1,94 kg·m² |
| Knowledgebase `20_pruefstand` | **2,837 m** | Ø 900 blank, Ø 903 mit 1,5 mm Tränenblech | 1,94 kg·m² |
| `docs/03-parameter.md` §1 | **2,931 m** | Ø 933 | 2,07 kg·m² |
| `README.md` | **2,93 m** | Ø 933 | — |

Das erklärt rückwirkend die seit Wochen notierte Abweichung bei `MAX_DRUM_RPM`:
**353** in `config.h` folgt aus 2,837, **341** in [07-sicherheit](07-sicherheit.md) §4
aus 2,931. Es war nie ein Rechenfehler, sondern **zwei Trommeln in einem Projekt**.

Code und Knowledgebase sind sich auf 2,837 einig, nur die beiden Repo-Dokumente hängen
auf dem alten Wert. **Nicht automatisch zusammengeführt** (CLAUDE.md: bei Konflikt
entscheidet der Mensch). Zu tun: Maßband um die Trommel, drei Messungen mitteln, und
dann alle vier Stellen auf das Ergebnis ziehen.

### A15 Drehzahlregler des VESC ist unbrauchbar eingestellt

Am 2026-11-17 gemessen: Sollwert 900 ERPM → 27 ERPM, 1500 → 150, 2400 → 780. Der
Regler bleibt bei einem Drittel stehen und zieht 0,3 A, also gerade die Lagerreibung.

> **Korrektur der Herkunft, am selben Tag.** Hier stand zuerst, die Gewinne seien ein
> Erbe der vorgefundenen Skateboard-Konfiguration. Das ist falsch: nach dem Flashen auf
> Firmware 7.01 standen dieselben vier Werte wieder da. Es sind **Werkseinstellungen
> der VESC-Firmware**. An der Sache ändert das nichts — sie sind für diesen Motor
> unbrauchbar — aber es heißt, dass hier nichts „aufzuräumen" ist, sondern **erstmals
> einzustellen**.

| | Wert |
|---|---|
| `s_pid_kp` | 0,004 |
| `s_pid_ki` | 0,004 |
| `s_pid_kd` | 0,0001 |
| `s_pid_min_erpm` | 900 |

Der P-Anteil reicht nur bis zur Reibung, und der I-Anteil ist bei VESCs interner
Skalierung von 10⁻⁴ praktisch wirkungslos.

**Schwerer wiegt `s_pid_min_erpm` = 900.** Die unterste Prüfstufe aus `config.h` ist
10 km/h = 58,8 1/min = **881 ERPM** — knapp *unter* der Schwelle, ab der der Regler
überhaupt regelt. Darunter erzwingt er Tastverhältnis 0. Die niedrigste Stufe des
Prüfablaufs wäre so nicht fahrbar.

Zu tun: Gewinne **gegen die montierte Trommel** einstellen (frei eingestellte Werte
wären weggeworfen, der Kreis muss gegen 1,94 kg·m² und deren Reibung arbeiten) und
`s_pid_min_erpm` so weit herunter, dass 881 ERPM sicher geregelt werden. Arbeitsbereich
ist **881 ERPM (10 km/h) bis 5287 ERPM (60 km/h)**.

Ersatzweise fährt der Stand über das **Tastverhältnis** — das ist am 2026-11-17 bis
99,2 km/h erprobt und folgt der Leerlaufkennlinie streng linear. Für eine Messung mit
stehender Geschwindigkeit genügt das aber nicht; dafür braucht es den geschlossenen
Kreis.

## B — Blockiert die Firmware

### B3 LCD-Anschluss und Pegel
I²C-Backpack (PCF8574) ist gesetzt, er läuft auf 5 V. Drei Wege zum 3,3-V-Pegel stehen
nebeneinander in [04-hardware](04-hardware.md) §5.6:

| | |
|---|---|
| a) Pegelwandler | in der Spezifikation, ein Bauteil mehr |
| b) die 4,7-kΩ-Pull-ups vom Backpack auslöten und gegen 3,3 V ziehen | am wenigsten Aufwand, aber unter den 0,7 · V_DD = 3,5 V, die der PCF8574 als High verlangt |
| c) das LCD selbst auf 3,3 V | der Spannungsregler auf dem Backpack ist nicht bestückt; Kontrast bei 3,3 V zu prüfen |

Offen außerdem: I²C-Adresse (0x27 oder 0x3F)?

### B4 Luftdrucksensor
Welches Modell? Die Beschreibung „Airspy" deutet auf einen SKS Airspy hin — zu
verifizieren. Ohne bekanntes BLE-Profil (Service- und Characteristic-UUIDs) bleibt es bei
der Handeingabe. Mögliches Vorgehen: mit einem BLE-Scanner die Characteristics auslesen.

## C — Methodisch

### C1 Trommelkrümmung — **halbiert, Oberfläche erledigt**
> **Oberfläche: kein Thema mehr.** Die Trommel bekommt Alu-**Tränenblech**, also
> dieselbe Art Oberfläche wie der Referenzprüfstand. Der früher befürchtete Zuschlag
> durch glattes Holz entfällt.

Übrig bleibt die **Krümmung**. Der Reifen wird auf einer Trommel stärker verformt als
auf der Ebene; maßgeblich ist der effektive Kontaktradius `1/(1/r_Reifen + 1/r_Trommel)`:

| | r_eff bei r_Reifen = 0,34 m | Anteil am Flachwert |
|---|---|---|
| Referenzprüfstand, Ø 0,77 m | 0,181 m | 53 % |
| **unsere Trommel, Ø 0,90 m** | **0,194 m** | **57 %** |
| Ebene | 0,340 m | 100 % |

Die beiden Trommeln unterscheiden sich also um **7 %** im effektiven Radius — unsere
Werte liegen folglich etwas unter den publizierten, aber in derselben Größenordnung, nicht
um Faktor 2. Wie sich die 7 % in Crr übersetzen, ist der unbekannte Teil; dafür fehlt ein
belastbarer Ansatz aus der Literatur. **Wird nicht geraten.**

Bis dahin: Vergleiche zwischen Reifen sind gültig, Absolutwerte nur mit diesem Vorbehalt.

### C2 Referenzreifen — **zurückgestellt**
Erst Wiederholbarkeit herstellen, dann einen Reifen mit publiziertem Crr als Anker
beschaffen. Kandidat bleibt Marathon Plus (0,00764, 25,5 W) — weit genug über dem
Rauschen, dass ein Faktor-2-Fehler in der Messkette auffiele.

### C4 Einschwingzeit und Mittelungslänge
`SETTLE_MS` = 15 s und `AVG_REVS_PER_POINT` = 10 sind geschätzt. Nach den ersten echten
Messungen anhand der Streuung nachziehen.

### C5 Luftzug
Die Trommelaerodynamik dominiert den Leerlauf bei hoher Geschwindigkeit. Wie stark
reagiert sie auf Türen, Lüftung, Position im Raum? Einmal gezielt ausprobieren.

## D — Kleinigkeiten

### ~~D2 Datenablage~~ — **entschieden**
**Keine Rohdaten in Git.** Der rohe 80-SPS-Strom sind rund 30 MB je Zweistundensitzung;
nach zwanzig Sitzungen wäre das Repo bei 600 MB. Rohstrom bleibt auf der SD-Karte.

Versioniert werden die **gemittelten Messpunkte** — wenige Kilobyte je Lauf. Das ist die
Datei, auf der die Auswertung läuft.

### B6 Bedienfeld: Portal oder nicht — **entschieden, kein Portal**
Ein Captive Portal wurde gebaut, am Gerät erprobt und wieder entfernt. Messungen und
Belege in [04-hardware](04-hardware.md) §13.2: das Portalfenster ist kein Browser und
lässt sich nicht verlassen, iOS trennt danach das Netz (gemessen 15 s, mit Karenzphase
40 s), und Android zeigt bei privaten Adressen überhaupt kein Portal. Stattdessen
beantwortet der Stand die Verbindungsprüfung, bleibt verbunden, und aufgerufen wird
über QR-Code.

**Offen bleibt:** ob das WLAN offen oder mit WPA2 laufen soll. Zurzeit WPA2, weil sonst
jeder in Reichweite den Motor fahren kann — das Kennwort steckt im QR-Code, getippt
wird es nie.

### A12 Hebelarm und Zellenendwert ausmessen
Die Zelle ist laut Ansage vom 2026-11-16 eine **5-kg-Zelle**, nicht 10 kg wie bis dahin
dokumentiert. Zusammen mit dem alten 40-mm-Arm hätte das die Zelle beim Anfahren mit
128 % ihres Endwerts belastet. Arm steht jetzt auf 80 mm, Herleitung in
[08-staender](08-staender.md) §5.2.

**Zu tun:** Typenschild der Zelle ablesen (dann Herkunft von `LOADCELL_CAPACITY_N` auf
DB setzen) und die Bohrung am Hebelarm auf ±0,1 mm ausmessen. Verbaut sind laut Ansage
etwa 80 mm; der Arm geht **linear** in jedes Moment ein.

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
| **Drehzahlreserve (A13)** | **Erledigt.** Nackte Trommel mit sensorlosem Regler über **100 km/h bei 42 V** = 589 1/min → **kv ≈ 14** (nicht 10, die 400 1/min am BLDC-Tester waren nicht der Anschlag). Bei 36 V sind das 505 1/min = 86 km/h, also **43 % Reserve** über den für 60 km/h nötigen 353 1/min. Netzteil 36 V genügt. | 2026-11-03 |
| **Auflast (A3)** | **Keine zweite Wägezelle.** Vor der Messung eine Plattformwaage zwischen Trommel und Laufrad schieben, Auflast auf den Zielwert justieren. Die vorhandene 120-kg-Waage mit 20 g Auflösung sind 0,05 % bei 42,5 kg. Laufrad- und Reifenmasse sind damit automatisch enthalten. | 2026-11-03 |
| **Polpaarzahl (A13b)** | **15 Polpaare, 30 Pole.** Über die Hall-Geber ausgezählt: 10 markierte Handumdrehungen = genau **900** Tachoschritte, kein Rückwärtsschritt. Sensorlos vorher 16,5 und nicht reproduzierbar. Der Regler stand auf `si_motor_poles` = 14 (der **Werkseinstellung** der VESC-Firmware, nicht einem Erbe der Fremdkonfiguration) und ist auf 30 korrigiert. `VESC_ERPM_MAX` ist seither **abgeleitet** statt geraten. | 2026-11-17 |
| **Hall-Geber am Regler (A1b)** | **Vorhanden und erkannt.** `foc_detect_apply_all` meldete „Found hall sensors, using them", `foc_sensor_mode` steht auf Hall, die Tabelle ist mit exakter 60°-Teilung belegt. Damit ist der Tachozähler eine Messung statt einer Schätzung. | 2026-11-17 |
| **Temperaturfühler (A5, Typ)** | **10-k-NTC im Stator**, dreifach belegt (offene Klemme −75,5 °C, 1,6 K gegen den FET-Fühler, monotoner Anstieg unter Heißluft gegen flache Referenz). `m_motor_temp_sens_type` = NTC 10K war bereits richtig. Offen bleibt nur der Grenzwert. | 2026-11-17 |
| **Dissipatives Bremsen über den Regler** | **Geht nicht** auf Firmware 5.02. Dreimal gemessen: Bus auf 53,3 bis 55,8 V, Fehlercode 1. Tastverhältnis 0 ist im **FOC**-Betrieb kein Brückenkurzschluss, sondern ein geregelter Arbeitspunkt — der echte Kurzschluss der unteren Schalter braucht `foc_short_ls_on_zero_duty` ab FW 6.05. Die **Kurzschlussrelais sind damit die einzige Bremse**, nicht Zusatzausstattung. Begründung und Zahlen in [07-sicherheit](07-sicherheit.md) §3.1. | 2026-11-17 |
| **Leerlaufverluste der Trommel** | **Gemessen** aus dem freien Auslauf (97 → 29 km/h in 140 s, acht Stützstellen): M = 0,472 Nm + 3,178·10⁻⁴·ω². Luftwiderstand bei 60 km/h **0,43 Nm** statt der geschätzten 0,27 — Faktor 1,6, und damit 33 % des Nutzsignals. Konstanten in [03-parameter](03-parameter.md) §1. | 2026-11-17 |
| **Drehzahlreserve, nachgemessen (A13)** | Am **freien** Motor 763 1/min bei Vollaussteuerung und 40 V, über zehn Stützstellen linear → an der Trommel 130 km/h. **Mit** Trommel 99,2 km/h erreicht, bei nur 31,9 V wirksam und 120 W. Ein 36-V-Netzteil genügt also bestätigt. Die früher notierten „kv = 14" sind keine Motorkonstante, sondern die Systemendgeschwindigkeit mit Trommel. | 2026-11-17 |
| **Reifentemperatur (C3)** | **IR-Sensor** berührungslos auf die Lauffläche, vorhanden. Hängt am bestehenden I²C-Bus (GPIO8/9), kein zusätzlicher Pin. Macht aus dem Warmlauf ein Kriterium statt einer Zeitvorgabe. | 2026-11-03 |
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
