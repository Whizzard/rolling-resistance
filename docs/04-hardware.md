# Hardware

## 1 Blockbild

```
                 ┌──────────────────────────────────────────┐
                 │        ESP32-S3-DevKitC-1 N16R8          │
                 │                                          │
   Hall-Marken ──┤ GPIO4  (ISR)        I²C ──┬── LCD 4×20   │
   Hall-Index  ──┤ GPIO5  (ISR)              │   (PCF8574)  │
   Hall-Laufrad──┤ GPIO6  (ISR)              │   SDA8/SCL9  │
   Motor-Hall  ──┤ GPIO7  (ISR)              └── (frei)     │
   HX711 DOUT  ──┤ GPIO15                                   │
   HX711 SCK   ──┤ GPIO16          UART1 ───── Motorregler  │
                 │                 (TX18/RX21)              │
   Not-Aus-     ─┤ GPIO2  (in)                              │
   Rückmeldung   │                                          │
                 │                 BLE ────── Drucksensor   │
                 └──────────────────────────────────────────┘

   Leistungspfad (galvanisch getrennt vom Logikteil):

   Netzteil 36 V ──[Not-Aus]──[Sicherung]── Motorregler ── Nabenmotor
```

## 2 Mechanischer Aufbau

Die Trommel sitzt auf einer **eigenen, mitdrehenden Welle**, nicht auf dem Motorrotor
(Entscheidung A10). Der Motor ist über einen Flansch starr mit dem Wellenende verbunden;
gehalten wird nur seine **Statorachse**, über den Hebelarm gegen die Wägezelle.
Maßkette und Lagerrechnung in [08-staender](08-staender.md) §1 und §4.

```
          Prüflaufrad (abhebbar)
                 ↓ F_N = 417 N (42,5 kg)
        ╔════════╧════════╗     Trommel U = 2,931 m, an beiden Stirnscheiben
        ║     Trommel     ║     auf die Welle geklemmt
  ──┬───╫──┬───────────┬──╫───┬──────  Welle mitdrehend, abgesetzt Ø15/Ø30
    │   ╚══╪═══════════╪══╝   │
  6902     Klemmnaben        6902      Flansch auf dem Wellenende
                                        │
                                   Nabenmotor, Rotor starr mitdrehend
                                        │
                              Statorachse, nur am Hebelarm gehalten
                                        │
                              L = 0,040 m → Schaltzug → Wägezelle 10 kg
                                        + Endanschläge bei r = 200 mm
```

Die Rotor-Stator-Reibung kürzt sich exakt heraus, solange die Statorachse **nur** über
den Hebelarm gehalten wird — Herleitung in [08-staender](08-staender.md) §7.2. Deshalb
ist das Zugglied ein Schaltzug und kein Stab: er überträgt kein Biegemoment.

**Keine Übersetzung.** Der Motor dreht mit etwa 480 1/min, die Trommel braucht für
60 km/h 341 1/min; ein Riemen läge außerhalb des Momentenpfads und ginge mit η ≈ 0,97 als
2–3 % Skalenfehler ein. `BELT_RATIO` bleibt in `config.h` für den Fall, dass doch einer
nötig wird.

Als Reibrolle gegen die Trommeloberfläche geht der Motor nicht: dafür bräuchte er
1393 1/min, also das Dreifache seiner Nenndrehzahl.

## 3 Stückliste

### Vorhanden

| Teil | Daten |
|---|---|
| Wägezelle | 10 kg, Biegebalken |
| Messverstärker | HX711-Breakout |
| Display | HD44780 4×20 Zeichen |
| Motortreiber | SimpleFOCShield v2 — nur für die Inbetriebnahme, für den Vollausbau zu schwach |

### Zu beschaffen

| Teil | Anforderung | Status |
|---|---|---|
| Nabenmotor | **Hoverboard-Antriebsrad 6,5″, 36 V / 350 W** | **beschafft** |
| Motorregler | **FSESC 4.20 / 50 A** (VESC-Klasse) | **beschafft** |
| Bremse an der Trommelwelle | Bandbremse oder Bremssattel auf einer Scheibe — der Motor hat keine (A9) | **offen, sicherheitsrelevant** |
| NTC für die Statorachse | nur falls die 6. Ader am Hall-Stecker kein Fühler ist (A5) | offen |
| Gegenstecker Julet 9-polig | oder Stecker abschneiden und eigene Verbinder setzen | offen |

| Netzteil | 36–42 V, ≥ 10 A | offen |
| ESP32-Board | **ESP32-S3-DevKitC-1 N16R8** | beschafft |
| USB-Netzteil | 5 V / ≥ 1 A, für den Messbetrieb (nicht das Notebook) | offen |
| HX711-Modul | **getrennte VCC/VDD** (fünfpolig, SparkFun-Bauform) — vierpolige Module scheiden aus, siehe §5.2 | offen |
| Hall-Sensoren | 3 × TLE4905L (Marken, Index, Laufrad), unipolar | vorhanden |
| Magnete | 8 + 1, Ø10 × 3 mm mit Ø3-Senkloch | vorhanden |
| Netzteil | 36 V, ≥ 10 A (besser 15 A) — Reserve für die Stromgrenze beim Anfahren | offen |
| Bremswiderstand + Chopper | **zurückgestellt** (A8). Achtung: die mechanische Trommelbremse als Rückfall gibt es nicht, siehe oben. | – |
| Stehlager für die Statorachse | 2 Stück, auf dem Wiegerahmen | offen |
| Zahnriemen | **entfällt beim Direktantrieb** — nur nötig, falls A6 negativ ausfällt | – |
| Not-Aus | zwangsöffnend, trennt den Leistungspfad | offen |
| Zweiter HX711 | dieselbe Bauform, für die spätere Auflastmessung | später |
| BLE-Drucksensor | Modell noch nicht festgelegt | später |

## 4 Pinbelegung

Board: **ESP32-S3-DevKitC-1**, Modul WROOM-1 **N16R8** (16 MB Flash, 8 MB Octal-PSRAM).
Mit `firmware/src/config.h` und `firmware/platformio.ini` abgeglichen.

Maßgeblich ist `hardware/schaltplan.py` — dort ist auch die Peripherie verdrahtet, die
die Firmware noch nicht anfasst.

| GPIO | Konstante | Funktion | Richtung | Anmerkung |
|---|---|---|---|---|
| 1 | `PIN_MOTOR_PTC` | Motortemperatur | in, analog | **ADC1**_CH0; ADC2 ist bei aktivem WLAN gesperrt. Fühlertyp **TBD**, siehe A5 |
| 2 | `PIN_ESTOP_FB` | Not-Aus-Rückmeldung | in | nur **Anzeige** des Zustands; die Trennung erfolgt in Hardware |
| 4 | `PIN_MARK` | Drehzahlmarken Trommel | in, Interrupt | **8** Marken/Umdrehung |
| 5 | `PIN_INDEX` | Indexmarke Trommel | in, Interrupt | 1/Umdrehung, triggert die Mittelung |
| 6 | `PIN_WHEEL` | Drehzahl des Prüflaufrads | in, Interrupt | für den Laufrad-Auslaufversuch; **noch nicht in der Firmware** |
| 7 | `PIN_MOTOR_HALL` | Motor-Hall (eine Phase) | in, Interrupt | zugleich die hochauflösende Drehzahlquelle |
| 8 | `PIN_I2C_SDA` | I²C SDA | bidir | LCD; S3-Vorgabebelegung |
| 9 | `PIN_I2C_SCL` | I²C SCL | out | LCD |
| 10 | `PIN_SD_CS` | microSD, Chip Select | out | übrige SPI-Pins noch **TBD** |
| 14 | `PIN_BUTTON` | Bedientaster | in | **noch nicht in der Firmware** |
| 15 | `PIN_HX711_DOUT` | HX711 #1 DOUT (Moment) | in | |
| 16 | `PIN_HX711_SCK` | HX711 SCK | out | **gemeinsamer Takt beider HX711** |
| 17 | `PIN_HX7112_DOUT` | HX711 #2 DOUT (Auflast) | in | zweite Zelle noch nicht bestückt |
| 18 | `PIN_DRIVE_TX` | UART1 TX → Regler | out | |
| 21 | `PIN_DRIVE_RX` | UART1 RX ← Regler | in | |
| 48 | `PIN_STATUS_LED` | Status-LED | out | auf DevKitC-1 **v1.0** ist das die RGB-LED, auf v1.1 ist sie an GPIO 38 |

### Welche GPIOs auf diesem Board gesperrt sind

Die frühere Belegung (25/26/27/32/33/34/35/22) stammte vom klassischen ESP32 und lag
auf dem S3 fast vollständig auf Pins, die es entweder nicht gibt oder die belegt sind:

| GPIO | warum gesperrt |
|---|---|
| 22…25 | existieren auf dem ESP32-S3 nicht |
| 26…32 | SPI-Flash |
| 33…37 | Octal-PSRAM — das **R8** im Modulnamen |
| 19, 20 | USB D−/D+ |
| 0, 3, 45, 46 | Strapping-Pins |
| 43, 44 | UART0, serielle Konsole |
| 38 | RGB-LED des DevKits |

Frei bleiben 1…18 und 21 sowie 39…42 und 47/48. Analog geht nur **ADC1** (GPIO 1…10),
weil ADC2 bei eingeschaltetem WLAN vom Funk belegt ist — deshalb liegt der PTC auf
GPIO 10.

Display: `LCD_I2C_ADDR` = 0x27 (**TBD**, übliche Alternative 0x3F), `LCD_COLS` = 20,
`LCD_ROWS` = 4.

## 5 Stromversorgung und Pegel

### 5.1 Speisung über USB

Der Logikteil wird **ausschließlich über USB-C** gespeist, nicht über den 5V-Pin.
Espressif nennt die drei Wege ausdrücklich *mutually exclusive*
([User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html)) —
eine extern eingespeiste 5 V und ein zum Flashen angestecktes Notebook wären ein
Kurzschluss zweier Quellen. USB schließt den Fehler konstruktiv aus.

Das Board hat zwei USB-C-Buchsen, und **beide dürfen gleichzeitig speisen**
(„either one or both"). Also: Netzteil dauerhaft an der einen, Notebook zum Flashen
an der anderen.

Bei USB-Speisung ist der **5V-Pin ein Ausgang** (VBUS). Beide Schienen kommen damit
aus derselben Quelle:

| Schiene | Herkunft | versorgt |
|---|---|---|
| 5 V | 5V-Pin = VBUS | Hall-Geber, HX711-**VCC**, LCD |
| 3,3 V | 3V3-Pin, Onboard-LDO aus VBUS | HX711-**VDD**, Pull-ups der Geber |

Weil der 3,3-V-Regler aus denselben 5 V läuft, gibt es **kein Sequencing-Problem**:
die Schienen kommen und gehen gemeinsam. Nur bei getrennten Quellen müsste VDD vor
VCC anliegen, sonst treiben die Eingangsschutzdioden des HX711 Strom in den
Digitalteil.

| Verbraucher | Strom |
|---|---|
| ESP32-S3 mit WLAN | ≈ 100 mA, Spitze ≈ 350 mA |
| LCD 2004 mit Beleuchtung | 25…40 mA |
| 3 × TLE4905L | ≈ 21 mA |
| HX711 | 1,4 mA |

Rund 150 mA im Mittel. Ein 1-A-Netzteil genügt dreifach. Für den Messbetrieb ein
eigenes Netzteil nehmen, nicht das Notebook — ein Ruhezustand mitten im Lauf
kostet die Messung.

### 5.2 Pegel: der ESP32-S3 ist nicht 5-V-tolerant

| Baustein | Versorgung | Signal zum ESP32 |
|---|---|---|
| TLE4905L | V_S = 5 V (Minimum 3,8 V) | Open Collector, **Pull-up 2,2…4,7 kΩ nach 3,3 V** |
| HX711 | **VCC = 5 V** (analog), **VDD = 3,3 V** (digital) | DOUT auf 3,3 V |
| LCD / PCF8574 | 5 V | I²C über Pegelwandler |

**Festlegung: HX711-Modul mit getrennten VCC- und VDD-Pins** (SparkFun-Bauform,
fünf Pins: VDD, VCC, DAT, CLK, GND). VCC speist die Brücke, VDD legt den Logikpegel
fest. Am Chip sind das AVDD (Pin 3) und DVDD (Pin 16).

Die billigen vierpoligen Module (VCC, GND, DT, SCK) scheiden damit aus — dort sind
beide verbunden. Die Alternative wäre, den ganzen Baustein auf 3,3 V zu fahren. Das
kostet **keine** Auflösung in Counts, weil die Brücke aus AVDD gespeist wird und AVDD
zugleich die ADC-Referenz ist, die Messung also ratiometrisch läuft. Es kostet aber
**SNR**: ein Drittel weniger Erregung bei gleichem Eingangsrauschen sind rund 3,6 dB.
Bei 12 N Signal des besten Rennreifens auf einer 98-N-Zelle wird das nicht
verschenkt.

### 5.3 Wo der HX711 sitzt

**Am Zellenkabel, nicht am ESP32.** Das Zellensignal ist der empfindliche Teil der
Kette, das digitale Signal der robuste — also wird die lange Strecke auf die
digitale Seite gelegt.

| | |
|---|---|
| Endwert der 10-kg-Zelle bei 1 mV/V und 5 V Erregung | 5 mV |
| Auslegungsfall samt Rampe (62,7 N) | 3,2 mV |
| **ΔCrr = 0,0005 (2,43 N)** | **124 µV** |

Auf einer Leitung, die 124 µV auflösen soll, ist jede eingekoppelte Störung direkt
Messfehler — und der PGA verstärkt sie mit 128.

Der eigentliche Schaden ist nicht Rauschen, sondern **EMV-Gleichrichtung**: die
PWM des Reglers koppelt mit einigen zehn kHz in die hochohmigen PGA-Eingänge und
richtet sich dort zu einem **Gleichanteil** gleich, der mit dem Motorstrom wandert.
Das sieht exakt wie Signal aus und fällt in der Leerlaufkennlinie nicht auf, weil
es dort genauso auftritt.

Der oft genannte Grund — Leitungswiderstand bei der Vierleiterzelle — ist dagegen
**kein** Argument: 2 m Litze sind rund 0,5 Ω in der Erregerschleife, bei 1 kΩ
Brückenwiderstand und 10 K Temperaturhub ergibt das 0,002 % Spannenfehler.

Praktisch:

- HX711 **innerhalb von etwa 10 cm** an der Zelle, in einem kleinen geschirmten
  Gehäuse am Rahmen — nicht auf dem Wiegeteil, sonst zieht sein Kabel am Hebelarm.
- **Nicht** in Motornähe oder in dessen Abluft, trotz der kurzen Strecke zur Zelle:
  Offsetdrift.
- Zellenkabel als **zwei verdrillte Paare** (E+/E−, S+/S−), Gesamtschirm, Schirm
  einseitig an den Zellenkörper.
- Am HX711-Eingang je 100 Ω in Reihe und 100 nF differenziell über INA+/INA−.
- Die digitale Strecke verdrillt oder geschirmt, **getrennt vom Zellenkabel geführt**,
  und 100 Ω in Reihe im SCK direkt am ESP32 — flankenbremsend, damit der Takt nicht
  in die Analogseite zurückkoppelt.

### 5.4 Steckverbinder

**Die Zellenleitung bekommt keinen Steckverbinder.** Sie ist nur 10 cm lang und geht
direkt in die **Schraubklemme auf der HX711-Platine**. Eine Schraubklemme ist eine
gasdichte Klemmverbindung ohne zusätzliche Metallübergänge — elektrisch das Beste,
was es hier gibt, und zum Zellentausch reicht ein Schraubendreher.

Gesteckt wird die **digitale Seite**. Empfehlung: **JST-XH, 6-polig** — gekeyt,
crimpbar, billig. Sechs statt fünf Pole, damit jedes Signal eine Masse neben sich
hat:

| Pin | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| | 5 V | GND | SCK | DOUT | GND | 3,3 V |

Zwei Dinge entscheiden hier, nicht die Signalqualität:

- **Verpolschutz.** 5 V und 3,3 V liegen auf demselben Stecker. Ein verdrehter oder
  um einen Pin versetzter Stecker zerstört entweder den HX711 oder den ESP32. Die
  Bauform muss formschlüssig kodiert sein — deshalb JST und nicht Stiftleiste. Die
  Reihenfolge oben setzt 5 V und 3,3 V zusätzlich an die beiden Enden.
- **Vibration.** Der Stecker sitzt am Prüfstand. Kabelbinder als Zugentlastung, oder
  gleich eine rastende Bauform (Molex Micro-Fit 3.0).

Kabel dazu: **LiYCY 6×0,14 mm²** — geschirmt, sechsadrig, und 0,14 mm² ≈ 26 AWG
liegt mitten im Crimpbereich der XH-Kontakte.

Der Schirm hat bei sechs Adern auf sechs Positionen **keinen eigenen Pin**. Er wird
als kurzer Zopf herausgeführt und **nur am ESP32-Ende** auf Masse gelegt, am HX711
offen gelassen: einseitig geerdet, also keine Masseschleife parallel zur
GND-Ader. Reicht das gegen die Reglerstörungen nicht, kommt das zweite Ende dazu.

Bei über 1 m Länge oder wenn das Kabel am Regler vorbeiläuft: **geschirmtes
RJ45-Patchkabel** (S/FTP). Acht Adern als vier verdrillte Paare, jedes Signal mit
eigener Rückleitung, Schirm auf das Gehäuse, rastend, als Fertigkabel in jeder
Länge zu haben. Elektrisch deutlich besser als jedes Flachbandkabel.

**Nicht** nötig ist Aufwand wegen der Signale selbst: 3,3-V-CMOS bei maximal 1 MHz
über einen Meter ist unkritisch. Der HX711 hat allerdings **keine Prüfsumme** — ein
gekipptes Bit liefert einen wild falschen Wert. Die Firmware sollte Ausreißer
verwerfen, statt sich auf die Leitung zu verlassen.

Grund: jeder Steckverbinder bringt zwei Metallübergänge je Ader in den Signalpfad,
und bei µV-Pegeln zählt die **Thermospannung**. Sie hebt sich nur auf, solange beide
Kontakte eines Paares gleich aufgebaut und gleich warm sind.

| Übergang | Thermospannung |
|---|---|
| Kupfer–Gold | ≈ 0,3 µV/K |
| Kupfer–Lot | 1…3 µV/K |
| Kupfer–Nickel (durchgeriebene Vergoldung) | ≈ 20 µV/K |

Zum Vergleich: 1 % des Signals eines guten Rennreifens sind **16 µV**. Ein
durchgeriebener Kontakt mit 2 K Gradient über das Paar liegt bei 40 µV — also
2,5 % Fehler, und zwar ein systematischer.

Falls doch einer in die Analogstrecke muss:

- **S+ und S− auf benachbarte Pins**, E+ und E− ebenso — dann sind beide Kontakte
  gleich warm und die Thermospannungen heben sich auf.
- Kontakte **vergoldet**, nicht verzinnt.
- Schirm auf das **Metallgehäuse** des Steckers, Metall-Haube verwenden.
- Nicht in einen Temperaturgradienten setzen, also nicht in Motornähe.
- **Niemals Stiftleisten oder Dupont-Kabel** im Analogpfad.
- Analog- und Digitaladern **nicht durch denselben Stecker**; wenn es sich nicht
  vermeiden lässt, Masseadern dazwischen legen.

### 5.5 Messtechnisch

- **HX711 getrennt versorgen.** Nicht am Motorzweig. Ferrit und 100 µF direkt am
  AVDD-Pin, Sternmasse.
- **RATE-Pin prüfen.** `HX711_RATE_SPS` = 80 setzt RATE auf DVDD voraus. Viele
  Module legen Pin 15 fest auf GND, das sind 10 SPS — bei 60 km/h nur **1,8 Abtastwerte
  je Umdrehung** statt 14, womit die umdrehungssynchrone Mittelung
  ([09-messtheorie](09-messtheorie.md) §3.8) zusammenbricht. Gegebenenfalls Leiterbahn
  auftrennen und auf DVDD brücken.
- **Motorzuleitungen über das Pendellager als weiche Schlaufe.** Beim Nabenmotor treten
  sie axial durch die Statorachse aus, also auf der Pendelachse — der Vorteil geht
  verloren, wenn das Kabel straff abgeht.
- **Logik- und Leistungsmasse an genau einem Punkt verbinden.**

## 6 Leistungsbedarf und Netzteil

Bei 60 km/h (ω = 35,73 rad/s), Auflast 42,5 kg. Momente einschließlich Leerlauf und
Laufradverlusten (rund 0,51 Nm, [08-staender](08-staender.md) §5.1):

| Fall | Moment | P mech | Strom bei kt = 0,568 Nm/A |
|---|---|---|---|
| bester Rennreifen (Crr 0,0025) | 1,00 Nm | 36 W | 1,8 A |
| guter Rennreifen (0,0037) | 1,24 Nm | 44 W | 2,2 A |
| Contact Urban (0,0055) | 1,59 Nm | 57 W | 2,8 A |
| **Auslegung (Crr 0,007)** | **1,88 Nm** | **67 W** | **3,3 A** |
| + Anfahren am Rampenende | 2,51 Nm | 90 W | 4,4 A |
| *langsamster MTB (0,0127) — nicht vorgesehen* | *2,98 Nm* | *106 W* | *5,2 A* |

Elektrisch rund **90 W, in der Spitze 120 W**. Der Nennstrom des Motors liegt bei etwa
9,7 A — im Auslegungsfall werden also 34 % davon gebraucht. Mit dem Wechsel von der
MTB- auf die Rennreifenauslegung ist der Antrieb deutlich überdimensioniert.

Netzteil **36 V / 10 A** genügt; 15 A geben Luft, damit die Strombegrenzung beim Anfahren
vom Regler kommt und nicht vom Netzteil.

**Rückspeisung beachten:** Beim rekuperativen Bremsen fließt Energie zurück. Ein
Labornetzteil kann sie nicht aufnehmen, die Zwischenkreisspannung steigt und der Regler
schaltet mit Überspannung ab. Lösungen: Bremswiderstand mit Chopper (1,4 kJ über 10 s
sind 140 W, ein 100-W-Widerstand reicht) oder eine Batterie als Puffer. Eine mechanische
Bremse gibt es nicht ([07-sicherheit](07-sicherheit.md) §3).

## 7 Gebergehäuse für die Hall-Sensoren

Feststehende Seite der Drehzahlerfassung, am Rahmen. Gegenstück ist §8.

3D-Druck, Außenmaß **37,2 × 20,0 × 12,0 mm**, 5,6 cm³.

`hall-gehaeuse.FCStd` ist ein **PartDesign-Body mit Featurebaum** und einem
Spreadsheet `Parameter`; alle Skizzen sind voll bestimmt und über Ausdrücke an die
Tabelle gebunden. Maße ändert man **in der Tabelle**, nicht in den Skizzen.

| | |
|---|---|
| Featurebaum | Grundkoerper · Kavitaet · Bauteilraum · Rastnasen · Federfeld · Schraubloch · Kabelnut (+rund) · Kabelmulde (+rund) · LED_Loch |
| `hardware/hall_gehaeuse.py` | baut den Baum von Null auf, exportiert `.step` und `.stl` |

Wer im GUI arbeitet, pflegt die FCStd — das Skript überschreibt sie. Es ist der
Erzeuger, nicht die Wahrheit.

### 7.1 Maße

| Merkmal | Maß |
|---|---|
| Platine | Lochraster 18 × 13 × 1,6 mm, 2,54er Raster, 7 × 5 Löcher |
| Kavität | 20 × 14 × 10 mm, nach unten offen |
| Lötraum unter der Platine | 2,0 mm |
| Bauteilraum über der Platine | 6,4 mm |
| Stirnwand vor dem Geber | 1,2 mm |
| Seitenwände / Deckel | 3,0 / 2,0 mm |
| M6-Durchgang | Ø 6,6, 4,5 mm aus der Mitte versetzt |
| Kabelkanal | D-Nut 4,4 breit, 3,4 tief (Kabel Ø 4, dreiadrig) |
| Kabelmulde vor der Platine | dieselbe Nut, 5,2 tief, x = 20…26 mm |
| LED-Loch | Ø 3,2 im Deckel, 15 mm von der Stirnwand |

Stirnwand 1,2 mm, weil sie in den Magnet-Luftspalt eingeht (§8.3). Nut 3,4 mm bei
4 mm Kabel, damit das Blech den Mantel klemmt — das ist die Zugentlastung.

Der Mantel passt **nicht** unter die Platine; dort sind nur 2,0 mm. Er endet in der
Mulde, die mit 5,2 mm tiefer ist als das Kabel dick — dort wird nichts geklemmt und
nichts gegen die Platinenkante gedrückt. Weiter laufen nur die drei Adern. Geklemmt
wird erst im massiven Block dahinter.

### 7.2 Platinenaufnahme

| Merkmal | Maß |
|---|---|
| Absatz | 1,8 mm Vorsprung je Seite, **durchgehend 20 mm**; Auflage 1,3 mm |
| Unterkante des Absatzes | z = 3,6 mm |
| Rastnasen | 0,65 mm Vorsprung, 6 mm lang, **nur mittig**; Überdeckung 0,15 mm |
| Form der Nase | symmetrischer Keil, Scheitel z = 2,0 mm |
| Federfeld, von außen freigestellt | 12 mm lang, 0,8 mm Restwand |

Zwei Anschläge mit getrennten Aufgaben:

- Der **Absatz** sitzt tief in der Kavität und hindert die Platine daran, weiter
  hineinzurutschen.
- Die **Rastnasen** sitzen darunter und hindern sie daran, zur offenen Seite
  herauszufallen.

Die Nasen haben **keinen Hinterschnitt** — die Schräge ist oben wie unten gleich,
gehalten wird durch Vorspannung. Die Platine geht ohne Werkzeug wieder heraus.

Nasen nur mittig, weil die 3 mm starke Wand nur im freigestellten Feld überhaupt
federt. Der Absatz dagegen durchgehend; er muss nicht federn, sondern tragen.

> Der äußerste Lochrasterstreifen liegt auf der Leiste und muss frei von Lötstellen
> bleiben: Bauteile innerhalb von ±5,2 mm um die Platinenmitte bestücken.

### 7.3 Montage

Offene Seite auf das Alublech, eine M6-Schraube. Daraus folgt:

1. Das Blech schließt das Gehäuse und klemmt das Kabel in der Nut.
2. Die Platine hat 2,0 mm Abstand zum Blech. Klebeband als Isolation ist damit
   Rückfallebene, nicht Voraussetzung.
3. Eine Schraube heißt verdrehbar — die Ausrichtung zur Trommelmarke wird beim
   Anziehen mit eingestellt.

## 8 Markenaufnahme an der Trommel

Mitdrehende Seite, Gegenstück zu §7. Kein Druckteil.

**8 Magnete** auf dem Teilkreis der 8 Abstandsklötze der Trommel-Stirnfläche. Dort
ist die 15-mm-Multiplexscheibe von hinten abgestützt, der Radius ist maximal, und
die Teilung liegt durch den vorhandenen Aufbau fest.

### 8.1 Teile

| | |
|---|---|
| Magnet | Scheibe Ø10 × 3 mm, Mittelloch Ø3,0 gesenkt, N42 |
| Schraube | **DIN 7982 A2, 2,9 × 13 mm**, Senkkopf, Kreuzschlitz |
| Vorbohrung | Ø2,0 mm, ca. 11 mm tief |
| Einschraubtiefe | 10 mm in 15 mm Multiplex |
| Fliehkraft je Magnet bei 60 km/h | 1,2 N |

Ø2,9 statt 3,0, weil durch ein echtes Ø3,0-Loch keine 3,0er Schraube geht. Kleiner
als 2,9 gibt es in A2 mit Senkkopf und Kreuzschlitz kaum; falls die 2,9 klemmt,
Messing 2,5 × 12 — ebenso unmagnetisch.

### 8.2 Die Schraube muss nichtmagnetisch sein

Keine Korrosions-, sondern eine Messfrage: die Schraube sitzt im Flussweg des axial
magnetisierten Magneten. Stahl schließt Nord und Süd durch den Magneten kurz, das
Feld wird ringförmig statt mittig, der Schaltpunkt wandert, im Grenzfall gibt es zwei
Flanken pro Magnet. **A2 oder A4, niemals verzinkter Stahl.**

Der Kopf der 2,9er misst 5,5 mm und sitzt in einer Senkung für Ø3 eher etwas tiefer
als eine 3,0er — erwünscht, weil der Stahl damit weiter vom Sensor weg ist.

### 8.3 Feldreserve

`B(z) = Br/2 · [(z+t)/√(R²+(z+t)²) − z/√(R²+z²)]`, N42 mit Br ≈ 1,3 T, Loch per
Superposition als Gegenscheibe R = 1,5 mm abgezogen. Abstand bis zum Chip:
`z = Luftspalt + 1,2 mm Gehäusestirnwand + ≈ 0,5 mm Chiplage`.

| z | B |
|---|---|
| 3 mm | 116 mT |
| 4 mm | 96 mT |
| 6 mm | 58 mT |
| 8 mm | 35 mT |

B_OP des TLE4905L im ungünstigsten Fall 18 mT. Das Loch kostet 22 %, bei 8 mm bleibt
Faktor 2. Luftspalt deshalb eher großzügig wählen: die Störung durch Loch und
Schraubenkopf hat nur wenige Millimeter Längenskala.

### 8.4 Polung

TLE4905L ist unipolar. Alle acht Magnete mit derselben Seite nach außen; ein
Zylindermagnet passt in beiden Richtungen ins Loch, die Mechanik verhindert den
Dreher nicht. **Jeden Magneten vor dem Festziehen einzeln gegen den Sensor prüfen.**

### 8.5 Winkellage: messen statt bohren

Bohrgenauigkeit ist nicht nötig. Bei hoher Drehzahl ändert sich ω innerhalb einer
Umdrehung um höchstens 0,15 % ([09-messtheorie](09-messtheorie.md) §1.7); die
Abweichungen der acht Intervalle von je 1/8 sind damit direkt die Winkelfehler.
Einmal im Auslauf aufnehmen, als Korrekturtabelle ablegen.

Nötig nur für Drehzahl **unterhalb** einer Umdrehung. Was ins Ergebnis geht, läuft
index-getriggert über ganze Umdrehungen — dort kürzt sich der Winkelfehler exakt
heraus.
