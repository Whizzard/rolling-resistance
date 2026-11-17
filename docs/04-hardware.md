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
                              L = 0,080 m → Schaltzug → Wägezelle 5 kg
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
| Wägezelle | **5 kg**, Biegebalken |
| Messverstärker | HX711-Breakout |
| Display | HD44780 4×20 Zeichen |
| Motortreiber | SimpleFOCShield v2 — nur für die Inbetriebnahme, für den Vollausbau zu schwach |

### Zu beschaffen

| Teil | Anforderung | Status |
|---|---|---|
| Nabenmotor | **Hoverboard-Antriebsrad 6,5″, 36 V / 350 W** | **beschafft** |
| Motorregler | **FSESC 4.20 / 50 A** (VESC-Klasse), Eckdaten und Anschlüsse in §10 | **beschafft** |
| 3 × KFZ-Wechselrelais 12 V/40 A | Kurzschlussbremse, §9 | im Warenkorb |
| 2N5551, 1N4007, Widerstände | Low-Side-Schalter der Relaiskette, §9 | Sortiment beschafft |
| NTC für die Statorachse | nur falls die 6. Ader am Hall-Stecker kein Fühler ist (A5) | offen |
| Gegenstecker Julet 9-polig | oder Stecker abschneiden und eigene Verbinder setzen | offen |

| Netzteil | 36–42 V, ≥ 10 A | offen |
| ESP32-Board | **ESP32-S3-DevKitC-1 N16R8** | beschafft |
| USB-Netzteil | 5 V / ≥ 1 A, für den Messbetrieb (nicht das Notebook) | offen |
| Fünfwege-Tastschalter | Navigationstaster, 5 Kontakte + gemeinsame Masse (z. B. ALPS SKQU); **keine** analogen Joystick-Breakouts | offen |
| HX711-Modul | **getrennte VCC/VDD** (fünfpolig, SparkFun-Bauform) — vierpolige Module scheiden aus, siehe §5.2 | offen |
| Hall-Sensoren | 3 × TLE4905L (Marken, Index, Laufrad), unipolar | vorhanden |
| Magnete | 8 + 1, Ø10 × 3 mm mit Ø3-Senkloch | vorhanden |
| Netzteil | 36 V, ≥ 10 A (besser 15 A) — Reserve für die Stromgrenze beim Anfahren | offen |
| Bremswiderstand + Chopper | **entfällt** — gebremst wird über Kurzschluss, §9 | – |
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
| 2 | `PIN_ESTOP_FB` | Not-Aus-Rückmeldung | in | nur **Anzeige** des Zustands; die Trennung erfolgt in Hardware |
| 4 | `PIN_MARK` | Drehzahlmarken Trommel | in, Interrupt | **8** Marken/Umdrehung |
| 5 | `PIN_INDEX` | Indexmarke Trommel | in, Interrupt | 1/Umdrehung, triggert die Mittelung |
| 6 | `PIN_WHEEL` | Drehzahl des Prüflaufrads | in, Interrupt | für den Laufrad-Auslaufversuch; **noch nicht in der Firmware** |
| 7 | `PIN_MOTOR_HALL` | Motor-Hall (eine Phase) | in, Interrupt | zugleich die hochauflösende Drehzahlquelle |
| 8 | `PIN_I2C_SDA` | I²C SDA | bidir | LCD; S3-Vorgabebelegung |
| 9 | `PIN_I2C_SCL` | I²C SCL | out | LCD |
| 10 | `PIN_SD_CS` | microSD, Chip Select | out | übrige SPI-Pins noch **TBD** |
| 11 | `PIN_BTN_UP` | Steuerkreuz hoch | in, Pull-up | gegen Masse |
| 12 | `PIN_BTN_DOWN` | Steuerkreuz runter | in, Pull-up | |
| 13 | `PIN_BTN_IN` | Steuerkreuz rein | in, Pull-up | |
| 14 | `PIN_BTN_OUT` | Steuerkreuz raus | in, Pull-up | |
| 15 | `PIN_HX711_DOUT` | HX711 #1 DOUT (Moment) | in | |
| 16 | `PIN_HX711_SCK` | HX711 SCK | out | **gemeinsamer Takt beider HX711** |
| 17 | `PIN_HX7112_DOUT` | HX711 #2 DOUT (Auflast) | in | zweite Zelle noch nicht bestückt |
| 18 | `PIN_DRIVE_TX` | UART1 TX → Regler | out | |
| 21 | `PIN_DRIVE_RX` | UART1 RX ← Regler | in | |
| 39 | `PIN_BTN_OK` | Steuerkreuz Mitte, bestätigen | in, Pull-up | |
| 40 | `PIN_BRAKE_RELEASE` | MOSFET in der Spulenmasse der Bremsrelais | out | **high = Relais an = Bremse gelöst.** Pulldown am Gate: beim Reset gebremst |
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

Frei bleiben danach **GPIO 1, 41, 42 und 47**. Die microSD braucht davon noch drei
(SCK/MOSI/MISO, CS liegt auf GPIO 10) — danach ist GPIO 1 der einzige freie Pin. Wenn
später noch etwas dazukommt, muss es über den I²C-Bus gehen, der ohnehin schon liegt.

GPIO 1 ist frei geworden, weil der Motorfühler am Regler hängt und nicht am ESP32
(§11.4). Falls doch einmal etwas analog gemessen werden soll: nur **ADC1** (GPIO 1…10)
ist brauchbar, ADC2 ist bei eingeschaltetem WLAN vom Funk belegt. GPIO 1 ist ADC1_CH0
und damit der richtige Pin dafür.

Dass GPIO 39…42 die JTAG-Pins sind (MTCK/MTDO/MTDI/MTMS), kostet nur einen **externen**
JTAG-Adapter. Das On-Chip-USB-Serial-JTAG des S3 läuft über die native USB-Buchse und
bleibt nutzbar.

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
(„either one or both"). Beide Kabel dürfen also dauerhaft stecken; zum Flashen muss
nichts abgezogen werden.

**Welches Kabel wohin — und zwar nicht beliebig:**

| Buchse | Kabel | warum |
|---|---|---|
| `COM` (UART-Brücke) | **Notebook**, nur bei Bedarf | dort flasht esptool ohne Tastendruck, und **nur dort kommt die serielle Konsole heraus** |
| `USB` (nativ) | **Netzteil**, dauerhaft | speist mit, wird sonst nicht gebraucht |

Andersherum funktioniert es nicht: an der nativen Buchse müsste der Download-Modus von
Hand gestellt werden, und die Konsole bliebe stumm.

Die beiden Buchsen sind aber **nicht gleichwertig**, auch wenn sie es für die Speisung
sind:

| Buchse | Weg in den Chip | wofür |
|---|---|---|
| `USB` | **natives USB** des S3 über GPIO 19/20, direkt am Chip | USB-Serial-JTAG, USB-CDC, Debuggen ohne externen Adapter |
| `COM` | **USB-UART-Brücke** auf UART0 (GPIO 43/44) | Flashen und serielle Konsole, Reset über DTR/RTS |

Praktischer Unterschied beim Flashen: die Brücke an `COM` zieht über DTR und RTS die
Leitungen EN und GPIO 0, kann den Chip also **selbst** in den Download-Modus
zurücksetzen. Die native Buchse kann das nur, solange die laufende Firmware die dafür
nötige Reset-Behandlung mitbringt — tut sie das nicht, bleibt esptool mit *„No serial
data received"* stehen, und der Download-Modus muss von Hand gestellt werden:
**BOOT halten, RESET antippen, BOOT loslassen.**

Zweiter Grund, das Notebook an `COM` zu stecken: **dort liegt die serielle Konsole.**
Die PlatformIO-Boarddefinition setzt `ARDUINO_USB_MODE=1`, aber **nicht**
`ARDUINO_USB_CDC_ON_BOOT`. Damit ist Arduinos `Serial` die UART0 auf GPIO 43/44, also
die `COM`-Buchse — an der nativen Buchse käme nach dem Flashen nichts heraus. Wer die
Konsole trotzdem dort haben will, setzt `-DARDUINO_USB_CDC_ON_BOOT=1` in
`platformio.ini`; dann wandert sie mit, und die Reset-Behandlung kommt gleich mit dazu.

> Nachgesehen am 2026-11-15: an der nativen Buchse meldet sich das Board als
> `0x303A:0x4001` („Espressif Device", Seriennummer `123456`, Composite-Gerät). Die
> Boarddefinition erwartet dort `0x303A:0x1001`, also das USB-Serial-JTAG des Chips.
> Es liegt also eine Firmware auf dem Board, die die native Buchse selbst als USB-CDC
> belegt — und esptool kommt darüber nicht in den Download-Modus.
>
> An `COM` sitzt ein **WCH CH343** (`0x1A86:0x55D3`, „USB Single Serial"), unter macOS
> ohne Zusatztreiber als `/dev/cu.usbmodem*`. Über diese Buchse geflasht am
> 2026-11-15: durchgelaufen ohne jeden Tastendruck, „Hard resetting via RTS pin",
> Konsole meldet sich mit 115200 Bd.

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
| LCD / PCF8574 | 5 V | I²C — drei Wege, §5.6 |

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
| Endwert der 5-kg-Zelle bei 1 mV/V und 5 V Erregung | 5 mV |
| Auslegungsfall samt Rampe (31,4 N) | 3,2 mV |
| **ΔCrr = 0,0005 (1,22 N)** | **124 µV** |

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

### 5.6 Das LCD auf 3,3-V-Pegel bringen

Backpack (PCF8574) und HD44780-Modul laufen auf **5 V**. SDA und SCL sind Open Drain:
den High-Pegel macht nicht der Baustein, sondern der **Pull-up** — und auf dem
Backpack sitzen 4,7 kΩ nach **5 V**. Genau die dürfen nicht an den S3.

| Weg | Eingriff | Bewertung |
|---|---|---|
| **a) Pegelwandler** | zwei MOSFETs (BSS138) oder ein Fertigmodul zwischen Bus und ESP32 | innerhalb der Spezifikation, nichts am Modul zu ändern; kostet ein Bauteil und zwei Steckstellen |
| **b) Pull-ups umhängen** | die 4,7 kΩ auf dem Backpack auslöten, stattdessen 2,2…4,7 kΩ nach **3,3 V** am ESP32 | am wenigsten Aufwand, aber **außerhalb der Spezifikation** |
| **c) LCD auf 3,3 V** | auf dem Backpack ist ein Spannungsregler **nicht bestückt**; dazu kämen einige Kondensatoren und Widerstände (Sichtprüfung der vorhandenen Platine) | sauberster Pegel, aber Bestückungsarbeit — und der Kontrast des HD44780 bei 3,3 V ist zu prüfen |

Warum (b) „nicht ganz sauber" ist: der PCF8574 verlangt als High-Pegel
**0,7 · V_DD**, bei 5 V Versorgung also **3,5 V**. Ein Bus, der nur auf 3,3 V
hochgezogen wird, liegt darunter.

- **Kaputt geht dabei nichts.** Gefährlich wäre die umgekehrte Richtung — 5 V am
  S3 —, und die ist nach dem Auslöten gerade weg.
- Ob der Baustein die 3,3 V **zuverlässig** als High liest, sagt das Datenblatt
  nicht zu. Das entscheidet sich am Exemplar und an der Temperatur. In der Praxis
  läuft es oft; eine Zusicherung ist es nicht.
- **Ein marginaler High-Pegel lässt sich nicht nachbessern.** Kleinere Pull-ups
  machen nur die Flanken steiler, nicht den Pegel höher; 100 kHz statt 400 kHz
  ebenso. Zickt der Bus, bleibt nur (a) oder (c).

**Reihenfolge bei (b): erst auslöten, dann anschließen.** Solange die 4,7 kΩ nach
5 V am Bus hängen, liegen 5 V an GPIO 8 und 9.

Entscheidung offen, siehe [06-offene-fragen](06-offene-fragen.md) B3.

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

## 9 Bremse: Kurzschluss über drei Relais

Der Motor hat keine mechanische Bremse. Gebremst wird, indem die drei Phasen
gegeneinander kurzgeschlossen werden; die Energie bleibt im Kupfer.

**Gemessen am 2026-11-01:** 25 s aus über 400 1/min. Daraus `J·ω/t` = 1,94 · 41,9 / 25 =
**3,25 Nm** mittleres Bremsmoment und 68 W mittlere Verlustleistung — thermisch
belanglos. Gegenüber 3 bis 6 Minuten freiem Auslauf Faktor 8 bis 14.

### 9.1 Schaltung

```
   +36…42 V (Bus)
        │
   ┌────┴────┐   drei 12-V-Spulen in REIHE
   │ K1 K2 K3│   zusammen 140 mA, 1N4007 antiparallel über die ganze Kette
   └────┬────┘
        │
        C
      2N5551        B ──1 kΩ── GPIO40
        E                └─10 kΩ─┐
        │                        │
       GND ──────────────────────┘
```

| Teil | Wert | Warum |
|---|---|---|
| Relais | 3 × KFZ-Wechselrelais 12 V, Kontakt an **87a** (Öffner) | stromlos geschlossen = gebremst |
| Spulen | **in Reihe** am Bus | 3 × 12 V = 36 V, kein DC-DC-Wandler nötig |
| Transistor | **2N5551**, 160 V / 600 mA | Bus plus Diodenabfall; 45-V-Typen sind bei 42 V zu knapp |
| Basiswiderstand | 1 kΩ | 2,6 mA bei hFE ≈ 60 unter Last |
| Basis–Emitter | 10 kΩ | GPIO ist beim Reset hochohmig — ohne ihn floatet die Basis |
| Freilauf | 1N4007 über die Kette | eine genügt |
| Phasenleitungen | ≥ 4 mm² | 60 A Spitze; die mitgelieferten Fassungskabel sind zu dünn |

### 9.2 Warum in Reihe und nicht über einen Wandler

Die Spulen hängen direkt am Bus. Bricht er zusammen, fallen die Relais **sofort** ab;
ein DC-DC-Wandler hielte sie über seinen Ausgangselko noch einige Millisekunden. Der
Strom ist in Reihe nur **einmal** 140 mA, nicht dreimal.

### 9.3 Grenzen

- Der Öffner 87a ist bei KFZ-Wechselrelais meist nur mit **20–30 A** spezifiziert, nicht
  mit den beworbenen 40 A. Gegen 60 A Spitze als Tragstrom für Sekunden vertretbar,
  dauerhaft nicht.
- Beim Schließen in die drehende Maschine fließt sofort Strom (20 V Gegen-EMK). Mit
  Kontaktprellen gibt das einen Einschaltlichtbogen — die Kontakte sind Verschleißteil.
- Die drei Relais sind ein **Satz**: in Reihe teilen sie die Spannung nach ihrem
  Widerstand. Ersatz nur mit demselben Modell.
- Eine **Feststellbremse ersetzt das nicht.** Dafür bleibt der Blockierkeil
  ([07-sicherheit](07-sicherheit.md) §3).

## 10 Motorregler FSESC 4.20

**Flipsky Mini FSESC 4.20 mit Kühlkörper.** VESC-Hardware V4.20, konfiguriert wird
über USB mit dem VESC-/BLDC-Tool. Quelle dieses Abschnitts ist das dem Regler
beiliegende Herstellerhandbuch (Specs und Wiring diagram).

### 10.1 Eckdaten

| Größe | Wert | Anmerkung |
|---|---|---|
| Hardware | V4.20 | VESC-Projekt, Open Source |
| Strom | **50 A dauernd, 150 A Spitze** | Auslegungsfall des Prüfstands: 3,3 A (§6) |
| Zellen | 3–13S LiPo | |
| Spannung | **8–60 V** laut Hersteller, **6–57 V** laut Firmware | *siehe Anmerkung unten* |
| BEC | 5 V / 1,5 A, interner Treiber | **wird nicht benutzt**, siehe §10.3 |
| Timing | per Software | |
| Abschaltspannung | programmierbar | relevant nur im Akkubetrieb |
| Sollwerteingang | PWM (PPM/Servo) | wir fahren über UART, §10.3 |
| Rückwärtslauf | ja | Drehrichtung per Software, nicht durch Phasentausch |
| Programmierkarte | nein | Einstellung ausschließlich über USB |
| Masse | 80 g | |
| Außenmaß | 67 × 39 × 18,3 mm | mit Kühlkörper |
| Lieferumfang | Regler + Kühlkörper, Micro-USB-Kabel, VESC-Sensorkabel, Handbuch | |

> Zwei Zeilen der Vorlage — „Governor" und das reine Platinenmaß — sind im Druck
> nicht lesbar und stehen deshalb nicht in der Tabelle.

> **Die 60 V des Herstellers sind nicht die wirksame Grenze.** Die VESC-Firmware klemmt
> für Hardware 410 auf **57 V** und lässt für `l_max_vin` gar nichts Höheres zu — in der
> vorgefundenen Konfiguration stand dort genau 57,0, also der Anschlag. Maßgeblich ist
> die Firmware, nicht das Datenblatt.
>
> Für den Stand ohne Folgen: ein volles 13S-Paket hat 54,6 V und bleibt darunter. Bei
> uns steht `l_max_vin` ohnehin auf **48 V**, bewusst über der Rückspeise-Abdrosselung
> und weit unter der Hardwaregrenze ([07-sicherheit](07-sicherheit.md) §3.2c).

Zwei Hinweise des Herstellers auf derselben Seite, beide am 2026-11-17 abgerufen
([Produktseite](https://flipsky.net/products/mini-fsesc4-20-50a-base-on-vesc%C2%AE-4-12-with-aluminum-anodized-heat-sink)):

1. *„The current factory-installed firmware is version 7.0 (the latest version)."* —
   siehe §10.5a.
2. *„Please do not remove the shrink to protect the pcb electronics components from
   impurites or metals which result in short circuit or esc burnt damage."* — der
   **Schrumpfschlauch über der Platine bleibt drauf**. In einer Werkstatt mit Spänen,
   Werkzeug und einem Relaisblock daneben ist das keine Floskel: die Platine liegt
   sonst offen unter einem Aufbau, der Metallteile bewegt.

### 10.2 Anschlussschema

```
                        ┌──────── BATTERY ────────┐
                        │   +  rot      −  schwarz│   8…60 V  (hier 36 V)
                        └────────────┬────────────┘
                                     │
   ┌────────┐                 ┌──────┴────────────────────┐              ┌────────┐
   │ 4 SWD  │────────────────▶│                           │◀─────────────│ 1 CAN  │
   └────────┘                 │   Mini FSESC 4.20         │              └────────┘
   ┌────────┐                 │   VESC-HW V4.20           │              ┌────────┐
   │ 3 COMM │────────────────▶│   50 A / 150 A Spitze     │◀─────────────│ 2 SENSE│
   └────────┘                 │                           │              └────────┘
                              │        [ Micro-USB ]      │
                              └────┬────────┬────────┬────┘
                                 C1│      B1│      A1│   3 Phasen, ≥ 4 mm²
                                   └────────┼────────┘    (Bremsrelais dazwischen, §9)
                                            ▼
                                       Nabenmotor
```

Belegung der vier Stiftleisten, in der Reihenfolge des Handbuchs:

| | 1 CAN | 2 SENSE | 3 COMM | 4 SWD |
|---|---|---|---|---|
| | − | − | 5V | NC |
| | CAL | H3 | 3.3V | RST |
| | CAH | H2 | − | DIO |
| | 5V | H1 | ADC | − |
| | | TMP | TX | CLK |
| | | 5V | RX | 3.3V |
| | | | ADC2 | |

> Welches Ende der Leiste Pin 1 ist, geht aus der Zeichnung nicht hervor. Vor dem
> ersten Anschließen 5 V und Masse am Stecker nachmessen.

Der Empfänger ist im Handbuch mit **GND / 5V / SIN** gezeichnet, also der
PWM-Sollwerteingang. Für den Prüfstand ohne Belang — gesteuert wird über UART.

### 10.3 Was davon benutzt wird

| Anschluss | Gegenstelle | Anmerkung |
|---|---|---|
| BATTERY + / − | Netzteil 36 V über Not-Aus und Sicherung | §1, §6 |
| C1 / B1 / A1 | Motorphasen, dazwischen die Bremsrelais | §9. Drehrichtung per Software, nicht durch Phasentausch |
| COMM `TX` | ESP32 `PIN_DRIVE_RX` = GPIO 21 | 3,3-V-Pegel, kein Wandler nötig |
| COMM `RX` | ESP32 `PIN_DRIVE_TX` = GPIO 18 | |
| COMM `−` | gemeinsame Masse, Sternpunkt nach §5.5 | die einzige Verbindung zwischen Logik- und Leistungsmasse |
| COMM `5V` | **nicht anschließen** | der Logikteil hängt am USB (§5.1); zwei speisende Quellen wären ein Kurzschluss |
| SENSE | Hall-Kabel des Motors | Aderfarben und Zuordnung in §11; die weiße Ader erst nach A5 an `TMP` |
| CAN, SWD | frei | CAN erst nötig, wenn ein zweiter Regler dazukommt; SWD nur zum Flashen des Bootloaders |

Der ESP32 greift eine Hall-Phase zusätzlich an `PIN_MOTOR_HALL` = GPIO 7 ab (§4).
**TBD:** Auf welchem Pegel die Hall-Leitungen am SENSE-Eingang liegen, ist nicht
dokumentiert. Liegt dort ein Pull-up nach 5 V, darf die Leitung nicht direkt an den
S3 (§5.2) — am angeschlossenen Regler messen, notfalls Teiler oder Pegelwandler
setzen.

### 10.4 Grenzen

- **Rückspeisung.** Der Regler bremst elektrisch und schiebt die Energie in den Bus
  zurück; ein Labornetzteil nimmt sie nicht auf (§6). Die Abschaltspannung ist
  programmierbar, der Überspannungsfall bleibt trotzdem der Regler-Fehlerfall.
- **Reihenfolge beim Bremsen.** Erst den Treiber sperren, dann die Relais abfallen
  lassen — die Phasen dürfen nicht kurzgeschlossen werden, solange der Regler treibt
  ([10-firmware](10-firmware.md) N2).
- **Die mitgelieferten Phasen- und Batteriekabel sind zu dünn** für die 60 A Spitze
  der Kurzschlussbremse (§9.1).

### 10.4a USB versorgt die Logik NICHT

Am 2026-11-17 nachgewiesen: mit abgeklemmtem Leistungseingang verschwindet dieser
Regler **vollständig vom USB-Bus**. Die Logik hängt am Leistungseingang, USB liefert
nur Daten.

Das ist ein Unterschied zum originalen VESC 4.12, der seine Logik aus den 5 V des USB
mitversorgt. **Nicht verallgemeinern** — es gilt für dieses Flipsky-Exemplar.

Zwei Folgen:

1. **Für die Diagnose:** nur insofern, als ohne Leistungseingang nichts geht. Der
   **Not-Aus trennt den Leistungseingang aber nicht**, nur Regler↔Motor — beim
   Bremsversuch am 2026-11-17 verschwand der Regler trotzdem vom USB-Bus, hat sich also
   zurückgesetzt. Verdacht und Prüfvorschrift in [07-sicherheit](07-sicherheit.md) §4.
2. **Fürs Flashen:** der Leistungseingang muss währenddessen anliegen. Ein kleines
   Paket genügt (2S, 8,4 V wurden benutzt) — ohne Motor gibt es keine Last, also
   keinen Spannungseinbruch. `l_min_vin` sperrt nur den Antrieb, nicht die Logik.

### 10.5 Konfiguration, wie sie am 2026-11-17 eingestellt wurde

Im Regler stand eine **fremde Konfiguration**: Übersetzung 2,77 statt der
Werkseinstellung 3,0, `foc_openloop_rpm` 700 statt 1500, und R/L/Flussverkettung aus
einer Erkennung an einem **anderen Motor**. Es war also einmal ein fremder Antrieb
eingemessen worden.

> *Frühere Fassungen nannten das pauschal „Skateboard-Konfiguration" und behaupteten,
> kein Wert darin gehöre zu diesem Aufbau. Zu weit gegriffen:* `si_motor_poles` = 14,
> `si_wheel_diameter` = 0,083 m und die Drehzahlregler-Gewinne sind
> **Werkseinstellungen** der VESC-Firmware — nachgewiesen am frisch auf 7.01 geflashten
> Regler. Aufstellung in [03-parameter](03-parameter.md) §4.1.

Die vorgefundene Fassung ist byteweise gesichert unter `hardware/vesc-backup/`
(Firmware 5.02, mcconf 458 B, appconf 419 B). **Zum Zurückspielen taugt sie nur auf
5.02** — siehe §10.5a.

**Erkannt** (`foc_detect_apply_all 50`, Motor ohne Trommel):

| | |
|---|---|
| Strangwiderstand | 238,7 mΩ |
| Induktivität | 403,8 µH *(Messung streut, siehe [03-parameter](03-parameter.md) §4.1)* |
| Flussverkettung | 17,287 mWb |
| Hall-Geber | **gefunden**, `foc_sensor_mode` = Hall |
| `foc_hall_table` | 255, **68, 0, 34, 134, 102, 167**, 255 — exakte 60°-Teilung |

**Von Hand gesetzt:**

| Einstellung | vorher | jetzt | Grund |
|---|---|---|---|
| `si_motor_poles` | 14 | **30** | gemessen, [06-offene-fragen](06-offene-fragen.md) |
| `si_gear_ratio` | 2,769 | **1,0** | Direktantrieb |
| `si_wheel_diameter` | 0,083 m | **0,9030 m** | Trommel, aus `DRUM_CIRCUMFERENCE_M` |
| `timeout_msec` | 1000 | **300** | zweite Totmannebene; Firmware wiederholt alle 150 ms, Konsolenwerkzeug alle 100 ms |
| `timeout_brake_current` | 0,0 A | **0,0 A** | *siehe Warnung unten* |
| `l_current_max` | 63,8 A | **20 A** | Antriebsstrom; 0,389 Nm/A → 7,8 Nm |
| `l_current_min` | −63,8 A | **−3 A** | Bremsstrom bleibt klein, weil Bremsen zurückspeist |
| `l_in_current_max` | 99 A | **8 A** | schützt das Netzteil (10 A) **unabhängig** vom Motorstrom |
| `l_in_current_min` | −60 A | **−1 A** | Rückspeisung eng; hilft allerdings nicht, [07-sicherheit](07-sicherheit.md) §3.1 |
| `l_abs_current_max` | 130 A | **30 A** | Panikschwelle, muss über dem Stellwert liegen |

> **`timeout_brake_current` muss 0 bleiben.** Testweise auf 1,0 A gesetzt und wieder
> zurückgenommen, aus zwei Gründen. Erstens **rastet der Zustand ein**: nach dem ersten
> Timeout ruft der Regler die Bremse dauerhaft auf, die Endstufe läuft permanent weiter
> (gemessen 120 mA bei 40 V = 4,8 W, FET-Temperatur stieg auf 32,9 °C) und ein Sollwert
> löst es nicht, weil 300 ms später der nächste Timeout greift. Zweitens, und das wiegt
> schwerer: **ein dauerhaft kurzgeschlossener Motor lässt sich nicht frei drehen.** Der
> Auslaufversuch ist eine Kernmethode dieses Prüfstands, und auch die Polzahlmessung
> wäre damit unmöglich geworden. Die Bremse gehört an die Relais (§9), nicht in den
> Reglertimeout.

**Motorstrom ist nicht Netzteilstrom.** Die beiden stehen im Verhältnis des
Tastverhältnisses, weil die Leistung erhalten bleibt. Bei 3 A im Motor und
Tastverhältnis 0,26 fließen 0,78 A aus dem Netzteil; bei 99 km/h und Tastverhältnis
0,85 sind es aus 4,4 A im Motor 3,2 A am Netzteil (beides gemessen und am Netzteil
gegengeprüft). Wer am Netzteil misst und daraus auf den Motorstrom schließt, unterschätzt
ihn bei kleiner Drehzahl drastisch.

### 10.5a Firmware 7.01 — und was sie kann, was 5.02 nicht konnte

Am **2026-11-17** von 5.02 auf **7.01** gezogen (VESC Tool 7.01, mitgeliefertes Abbild
`40_o_47_o_48_o_410_o_411_o_412_o_DAS_RS.bin`, Hardware 410). Ablauf: erst der
mitgelieferte **Bootloader**, dann die Firmware — der Bootloader ist das Rettungsnetz,
ohne ihn bleibt nach einem fehlgeschlagenen Flash nur SWD.

**Das Update stellt den Auslieferungszustand her, es verlässt ihn nicht.** Flipsky
schreibt auf der Produktseite des Mini FSESC 4.20: *„The current factory-installed
firmware is version 7.0 (the latest version). If you require a different version,
please specify this in a note when placing your order."* Die Hardware ist für 7.x also
vorgesehen — einschließlich der Strommessung, und das ist die Frage, die bei Nachbauten
berechtigt ist.

Dass dieses Exemplar mit **5.02 und einer Erkennung an einem fremden Motor** ankam,
heißt umgekehrt: es ist kein aktuelles Werksstück. Was sonst noch vom Werkszustand
abweicht, ist unbekannt — der Vergleich in §10.5 zeigt nur, was *wir* geändert haben.

> Das Abbild meldet `isTestFw: 1`, die Fassung heißt `7.01-1+f83a3ae1`. Es ist also
> keine als stabil deklarierte Freigabe, sondern die zu diesem VESC Tool passende
> Entwicklungsfassung — das Werkzeug wurde aus dem Quelltext gebaut. Flipsky nennt
> **7.0**, also dieselbe Generation. Wer die ausgezeichnete Freigabe will, spielt das
> Abbild der offiziellen VESC-Tool-Veröffentlichung auf; das Zurückschreiben der
> Konfiguration ist dann unkritisch, weil die XML-Dateien Feldnamen adressieren.

**Zwei Dinge, die den Ausschlag gaben:**

| Feld | 5.02 | 7.01 Werk | gesetzt |
|---|---|---|---|
| `foc_short_ls_on_zero_duty` | **existiert nicht** | 0 | **1** |
| `l_battery_regen_cut_start` / `_end` | **existieren nicht** | 1000 / 1100 V | **42,5 / 44 V** |

Das Erste schaltet bei Tastverhältnis 0 die unteren Schalter dauerhaft ein — der
**echte Brückenkurzschluss**, dissipativ, ohne Beteiligung des Busses. Genau das fehlte
bei allen vier Bremsversuchen in [07-sicherheit](07-sicherheit.md) §3.1 und §3.2c. Das
Zweite verwandelt den Überspannungsfall von einem Abwurf in eine **Abdrosselung**.

**Beides ist noch nicht erprobt** — der Bremsversuch braucht den Motor. [TBD]

### Konfiguration überlebt den Firmwarewechsel nicht — das XML schon

Die Signatur der Konfigurationsstruktur wechselte von 2211848314 auf 471314414, die
`mcconf` von 458 auf 493 Byte. Die gesicherten **Binärdateien sind damit wertlos** für
ein Zurückspielen.

Der Ausweg: VESC Tool liest und schreibt die Konfiguration als **XML über Feldnamen**,
ohne Oberfläche und ohne Byte-Versätze:

```sh
"VESC Tool" --offscreen --getMcConf  mcconf.xml
"VESC Tool" --offscreen --setMcConf  mcconf.xml
"VESC Tool" --offscreen --getAppConf appconf.xml
"VESC Tool" --offscreen --setAppConf appconf.xml
```

So wurden alle am 2026-11-17 gemessenen Werte **ohne Motor** zurückgeschrieben — auch
Hall-Tabelle, R, L und Flussverkettung, die sonst eine neue Erkennung gebraucht hätten.
Der Beobachtergewinn ist dabei nicht geraten, sondern abgeleitet: `g = 1e3/λ²`, an zwei
unabhängigen Datenpunkten der alten Firmware geprüft. **31 Felder gegengelesen, alle
bestätigt.** Ablage in `hardware/vesc-config-fw7.01/`.

> Ab jetzt ist das **XML** die maßgebliche Sicherung, nicht die Binärdatei.

### 10.6 Der Relaisblock muss die Phasenfolge erhalten

Beim ersten Einfügen des Not-Aus-Relaisblocks (§9) **waren zwei Phasen vertauscht**.
Das Bild: 10 A Motorstrom, 68 W Eingangsleistung, Drehzahl pendelt um ±5 1/min, **kein
Fehlercode** — der Regler bestromt die falsche Wicklung, es entsteht ein Drehfeld ohne
Nettomoment. Es brummt und wird warm.

Prüfungen, von harmlos nach aufwendig:

1. **Ohne Messgerät:** Relais abgefallen, Trommel von Hand drehen. Sie muss *deutlich
   schwerer* gehen als frei — sonst schließen die Öffner die Spulen nicht.
2. **Messgerät, Relais abgefallen:** zwischen je zwei **Motor**klemmen ≈ 0,2 Ω
   (Spulen kurz); von jeder **Regler**klemme gegen alles **unendlich**. Durchgang hier
   heißt: der Kurzschlussknoten liegt auch an der Reglerseite, und der Regler treibt in
   einen Kurzschluss.
3. **Messgerät, Relais angezogen:** genau eine Zuordnung eins zu eins, und zwar
   dieselbe wie bei direkter Verdrahtung.
4. **Über den Regler:** `measure_res_ind` muss den direkten Wert reproduzieren —
   251,9 mΩ gegen 237,2 mΩ direkt zeigte die 14,7 mΩ Kontaktwiderstand und belegte,
   dass alle drei Phasen leiten. Ein *Tausch* fällt dabei **nicht** auf, dafür braucht
   es `foc_sensors_detect_apply`: ändert sich die Richtung der Winkelfolge in
   `foc_hall_table`, sind zwei Phasen getauscht.

> **Eine neu erkannte Tabelle heilt den Tausch, ist aber die schlechtere Lösung.** Mit
> montierter Trommel ruckelt die Erkennung über die Rastmomente, weil sie im offenen
> Betrieb langsam dreht: die 60°-Teilung streute danach von 40° bis 74°. Winkelfehler
> machen Momentenwelligkeit, und die landet als Rauschen auf der Wägezelle — dort, wo
> ΔCrr = 0,0005 aufgelöst werden soll. Richtig ist: Adern tauschen, dann die
> Referenztabelle zurückschreiben oder **ohne** Trommel neu erkennen.

## 11 Motoranschluss: Phasen und Hall-Stecker

Aufgenommen am geöffneten Motor (Fotos vom 2026-11-14). **Nichts davon ist
durchgemessen** — die Zuordnung unten ist aus den Aderfarben und dem Verlauf der
Leitungen erschlossen. Vor dem ersten Anschließen gilt §11.3; eine vertauschte
Hall-Versorgung zerstört die Sensoren.

### 11.1 Was am Motor herauskommt

- **Drei dicke Adern mit Flachsteckern: gelb, grün, blau** — die Phasen.
- **Ein weißer JST-Stecker mit sechs dünnen Adern:** weiß, schwarz, grün, blau,
  gelb, rot (Reihenfolge am Stecker, so wie fotografiert).
- Im Stator sitzt eine Platine mit drei Hall-ICs auf dem Teilkreis. Zu ihr führen
  die farbigen Adern und rot/schwarz; die **weiße Ader geht nicht auf die
  Hall-Platine, sondern in einem Glasseidenschlauch ins Wickelpaket** — genau dort,
  wo ein Temperaturfühler sitzen würde.

### 11.2 Erschlossene Belegung

```
   JST 6-polig am Motor            FSESC, Leiste 2 SENSE
   ┌─────────────────────┐
   │ 1  weiß    TMP ?    │ ─────▶ TMP   (erst nach A5 anschließen)
   │ 2  schwarz GND      │ ─────▶ −
   │ 3  grün    Hall     │ ─────▶ H3 ┐
   │ 4  blau    Hall     │ ─────▶ H2 ├ Zuordnung beliebig, das VESC-Tool
   │ 5  gelb    Hall     │ ─────▶ H1 ┘ erkennt die Hall-Tabelle selbst
   │ 6  rot     +5 V     │ ─────▶ 5V
   └─────────────────────┘

   Phasen (Flachstecker):  gelb · grün · blau  →  A1 · B1 · C1 des Reglers,
   Reihenfolge beliebig — die Drehrichtung wird in der Software gesetzt (§10.1).
```

Woher die Zuordnung kommt:

| Ader | Deutung | Begründung |
|---|---|---|
| gelb, grün, blau | die drei Hall-Signale | dieselben drei Farben wie die Phasen; beim Hoverboard-Rad üblich |
| rot, schwarz | Versorgung der Hall-Platine, 5 V und GND | an der Platine, nicht an den Wicklungen |
| weiß | Temperaturfühler | die einzige übrige Ader, und sie führt in die Wicklung, nicht auf die Hall-Platine |

> Die gleichfarbige Hall-Ader gehört **nicht** zwangsläufig zur gleichfarbigen Phase.
> Welches Ende des Steckers Pin 1 ist, liegt ebenfalls nicht fest — maßgeblich ist
> die Farbe, nicht die Position.

### 11.3 Prüfvorschrift vor dem Anschließen

Ohne Versorgung, mit dem Multimeter:

1. **weiß gegen schwarz** messen — das klärt A5:

   | Messwert | Deutung |
   |---|---|
   | ≈ 10 kΩ, **fällt** beim Anwärmen | NTC 10 k, vermutlich B 3950 |
   | ≈ 100 Ω / 1 kΩ, **steigt** beim Anwärmen | PT100 / PT1000 oder KTY |
   | ≈ 0 Ω, öffnet beim Anwärmen | Thermoschalter, kein Fühler |
   | offen | weiß gegen rot und gegen die Hall-Adern nachmessen, bevor „kein Fühler" gilt |

   Anwärmen mit dem Fön auf Handwärme genügt; entscheidend ist das **Vorzeichen**
   der Änderung.
2. **rot gegen schwarz:** niederohmig darf es nicht sein. Erwartet wird der
   Eingangswiderstand der Hall-Platine, nicht ein Kurzschluss.
3. Erst dann **5 V an rot/schwarz** legen, strombegrenzt. Jede der drei Hall-Adern
   gegen schwarz messen und das Rad von Hand langsam drehen: der Pegel muss
   umschalten. Flanken je Umdrehung zählen — das ist `MOTOR_PULSES_PER_REV`
   ([03-parameter](03-parameter.md) §3).
4. **Den High-Pegel notieren.** Liegt er bei 5 V, darf die Ader nicht direkt an
   `PIN_MOTOR_HALL` = GPIO 7; der ESP32-S3 ist nicht 5-V-tolerant (§5.2). Dasselbe
   gilt für den Abgriff am SENSE-Eingang des Reglers (§10.3).

### 11.4 Wohin die weiße Ader geht

**Entschieden: an `TMP` des Reglers.** Es gibt nur eine Auswertestelle, denn ein
resistiver Fühler verträgt keine zwei Pull-ups — bei beiden wären beide Messungen
falsch.

| | **Fühler an `TMP` des Reglers** | Fühler am ESP32 (ADC1) |
|---|---|---|
| Auswertung | der Regler begrenzt selbst, der ESP32 liest den Wert über UART ([10-firmware](10-firmware.md) D25) | ESP32 über ADC1, Kennlinie in der Firmware |
| Voraussetzung | Fühlertyp muss zu den Kennlinien des VESC-Tools passen | freie Kennlinie, aber eigene Beschaltung und Abgleich |
| | **gewählt** | – |

Der Grund ist nicht Bequemlichkeit, sondern wo die Abschaltung sitzt: Übertemperatur
muss den **Treiber** sperren, und der Treiber ist der Regler. Hängt der Fühler am
ESP32, läuft die Schutzfunktion über UART und damit über eine Strecke, die ausfallen
kann. Am Regler begrenzt er auch dann, wenn die Firmware steht.

Preis dafür: der Fühlertyp muss zu einer Kennlinie des VESC-Tools passen. Das ist der
Rest von A5 — **welcher** Fühler es ist, ist weiter offen und wird nach §11.3
ausgemessen. `PIN_MOTOR_PTC` ist aus `config.h` entfallen, GPIO 1 ist frei.

### 11.5 Umsortieren statt löten

Der einfachste Weg, **wenn beide Stecker dasselbe System sind**: die Kontakte aus den
Kammern ziehen und in der richtigen Reihenfolge wieder einsetzen. Kein Löten, keine
zusätzliche Steckstelle, jederzeit rückgängig zu machen.

Zwei Dinge müssen vorher feststehen:

1. **Raster und Kontakttyp beider Stecker.** Messschieber an den weißen Motorstecker
   und an das beiliegende VESC-Sensorkabel. Gleiches System → umstecken. Verschiedene
   Systeme → die Kontakte passen nicht ins andere Gehäuse, dann wird das Sensorkabel
   angespleißt und beim Spleißen umsortiert. **TBD**, beides noch nicht gemessen.
2. **Welches Ende der Leiste `SENSE` Pin 1 ist** (§10.2). Am angeschlossenen Regler
   5 V und Masse nachmessen — sonst wird sauber spiegelverkehrt sortiert.

Zielreihenfolge ist die des Reglers: `−` · `H3` · `H2` · `H1` · `TMP` · `5V`.

| Kammer | heute | künftig |
|---|---|---|
| 1 | weiß | **schwarz** — GND |
| 2 | schwarz | grün — Hall |
| 3 | grün | blau — Hall |
| 4 | blau | gelb — Hall |
| 5 | gelb | **weiß** — TMP |
| 6 | rot | **rot** — 5 V |

> Die drei Hall-Adern dürfen untereinander beliebig liegen, das VESC-Tool erkennt die
> Hall-Tabelle selbst (§11.2). Stimmen müssen nur **schwarz, weiß und rot**. Praktisch
> ist es eine Rotation: alles außer rot rückt eine Kammer vor, weiß wandert ans Ende.

Beim Ziehen:

- **Der gelbe Verguss an der Kabeleinführung muss zuerst weg.** Solange er hält, kommt
  kein Kontakt aus der Kammer. Mit der Klingenspitze aufbrechen oder mit Isopropanol
  anlösen — nicht am Kabel ziehen.
- Rastzunge mit einer feinen Nadel oder einem Entriegelungswerkzeug niederdrücken, dann
  die Ader **nach hinten** herausziehen.
- Eine abgebrochene Rastzunge heißt: der Kontakt hält nicht mehr von allein. Zunge
  wieder aufstellen oder den Kontakt im Gehäuse sichern.
- Vorher fotografieren. Hinterher **jede Ader einzeln auf Durchgang prüfen**, bevor
  Spannung daraufkommt.

## 12 MVP-Verdrahtung

Erster Aufbau zum Inbetriebnehmen: Board, Wägezelle mit HX711, LCD, Steuerkreuz und die
UART-Strecke zum Regler. **Der Motor dreht dabei nicht.** Not-Aus, Sicherung und die
drei Bremsrelais (§9) sind nicht verbaut; ohne sie gibt es nur freien Auslauf von 3 bis
6 Minuten ([07-sicherheit](07-sicherheit.md) §3). Der Regler wird verkabelt und die
UART-Verbindung geprüft, nicht mehr.

Drehzahl kommt im MVP **über UART vom Regler** (ERPM). Die Hall-Geber an GPIO 4/5/6/7
braucht es dafür noch nicht.

### 12.1 Blockbild

```
       USB-C Netzteil 5 V/1 A ──► [USB ]  ┐  beide Buchsen dürfen
       Notebook (Flashen)     ──► [COM ]  ┘  gleichzeitig speisen (§5.1)
                                     │
         ┌───────────────────────────┴──────────────────────────────┐
         │            ESP32-S3-DevKitC-1  N16R8                     │
         │     5V-Pin = VBUS (Ausgang)   ·   3V3-Pin = LDO          │
         │                                                          │
 HX711 ──┤ GPIO15  DOUT ◄───────────────────────  DAT               │
         │ GPIO16  SCK  ──100 Ω─────────────────►  CLK              │
         │ 3V3 ─────────────────────────────────►  VCC + VDD        │
         │ GND ─────────────────────────────────►  GND              │
         │                        Schraubklemme ──► Wägezelle 5 kg  │
         │                                                          │
 LCD   ──┤ GPIO8  SDA ──┬── 3,3 kΩ ──┐                              │
         │ GPIO9  SCL ──┼── 3,3 kΩ ──┤                              │
         │ 3V3 ─────────┴────────────┴──► Backpack VCC  (§12.3)     │
         │ GND ─────────────────────────► Backpack GND              │
         │                                                          │
 Kreuz ──┤ GPIO11 hoch   GPIO12 runter   GPIO13 rein                │
         │ GPIO14 raus   GPIO39 OK    alle gegen GND, interner PU   │
         │                                                          │
 FSESC ──┤ GPIO18  TX ──────────────────►  COMM  RX                 │
         │ GPIO21  RX ◄─────────────────── COMM  TX                 │
         │ GND ──────────────────────────  COMM  −   ← Sternpunkt    │
         │                                 COMM  5V  ← NICHT        │
         └──────────────────────────────────────────────────────────┘
```

### 12.2 Strippenliste

| | von → nach | Kabel | Länge | Steckverbinder |
|---|---|---|---|---|
| 1 | Wägezelle → HX711 | die 4 Adern der Zelle (E+/E−/S+/S−) | **10 cm** | **keiner** — direkt in die Schraubklemme, §5.4 |
| 2 | HX711 → ESP32 | **LiYCY 6×0,14 mm²**, geschirmt | nach Aufbau | **JST-XH 6-polig**, Belegung nach §5.4 |
| 3 | ESP32 → LCD | 4-adrig, beliebig | 30 cm | unkritisch, digital |
| 4 | ESP32 → Steuerkreuz | 6-adrig (5 Signale + gemeinsame Masse) | 50 cm | beliebig |
| 5 | ESP32 → FSESC `COMM` | 3-adrig, verdrillt, **getrennt von den Phasen** | kurz halten | Stiftleiste am Regler |
| 6 | Netzteil → ESP32 | USB-C | – | eine der beiden Buchsen |

Zu #2: die Steckerbelegung **jetzt schon** nach §5.4 festlegen, auch wenn Pin 1 (5 V)
im MVP leer bleibt — dann passt das fünfpolige Modul später unverändert. Schirm als
kurzen Zopf nur am ESP32-Ende auf GND. Die 100 Ω je Zelleneingang und 100 nF
differenziell über INA+/INA− gleich mit einlöten; sie kosten nichts und man muss nicht
nochmal ran, wenn der Regler anfängt zu stören.

Am Steuerkreuz **keine Kondensatoren und keine externen Widerstände** — Entprellung in
Software, kein RC im Signalpfad ([10-firmware](10-firmware.md) B3), der interne Pull-up
genügt.

### 12.3 Drei Festlegungen für den MVP

**HX711 komplett auf 3,3 V**, also VCC und VDD gemeinsam an 3V3. Das ist eine bewusste
Abweichung von §5.2 und gilt nur für die Inbetriebnahme. Gründe: das vorhandene
Breakout ist womöglich das vierpolige, bei dem beide Pins verbunden sind — dann ist 3,3 V
der einzige Weg, der GPIO 15 nicht mit 5 V belegt. Es kostet **3,6 dB SNR**, aber
**keine Counts und keinen Kalibrierfaktor**: die Brücke hängt an AVDD, und AVDD ist
zugleich die ADC-Referenz, die Messung läuft also ratiometrisch (§5.2). Die heute
ermittelte Kalibrierung gilt deshalb auch nach dem Wechsel auf ein fünfpoliges Modul.
Für den Messlauf wird gewechselt — bei 12 N Signal des besten Rennreifens werden die
3,6 dB nicht verschenkt.

**LCD-Pegel**, Reihenfolge zum Ausprobieren (die Bewertung der drei Wege steht in §5.6,
offene Frage B3):

1. **Backpack-VCC auf 3,3 V statt 5 V.** Zwei Minuten, kein Bauteil, umkehrbar — und
   **innerhalb der Spezifikation**, denn die 4k7 auf dem Backpack ziehen dann nach
   3,3 V und der PCF8574 verlangt nur noch 0,7 · 3,3 = 2,31 V als High. Offen ist, ob
   der Kontrast des HD44780 bei 3,3 V reicht und wie dunkel die Beleuchtung wird; ihr
   Vorwiderstand ist für 5 V ausgelegt. Das entscheidet das Exemplar.
2. Reicht das nicht: **die 4k7 auslöten**, 2 × 3,3 kΩ nach 3,3 V am ESP32, LCD bleibt
   an 5 V. **Erst auslöten, dann anschließen** — solange die 4k7 nach 5 V am Bus
   hängen, liegen 5 V an GPIO 8 und 9.
3. **Pegelwandler** (BSS138), wenn einer da ist: spezifikationstreu und guter Kontrast,
   kostet zwei Steckstellen.

Vorher am stromlosen Backpack SDA gegen VCC durchmessen — ob die Pull-ups überhaupt
bestückt sind. Und die I²C-Adresse scannen, `LCD_I2C_ADDR` ist TBD (0x27 oder 0x3F).

**Fünf Taster, nicht vier.** Vier sind das Pflichtminimum
([10-firmware](10-firmware.md) B1), der fünfte ist dort „soll". Mit vier müsste
„bestätigen" auf eine Tastenkombination oder einen langen Druck, und B5 verlangt
ausdrücklich, dass die Schrittweite *nicht* über eine versteckte Kombination geht. Beim
Fünfwege-Tastschalter ist der fünfte Kontakt ohnehin im Bauteil; er kostet eine Ader und
GPIO 39. Die Pinzahl geht trotzdem genau auf (§4).

### 12.4 Vor dem ersten Einschalten

1. Regler an seine Micro-USB-Buchse, dann **5 V und Masse an der Leiste `COMM`
   nachmessen.** Aus der Handbuchzeichnung geht nicht hervor, welches Ende Pin 1 ist
   (§10.2) — sonst wird sauber spiegelverkehrt verdrahtet.
2. `COMM 5V` bleibt frei. Zwei speisende Quellen wären ein Kurzschluss (§10.3).
3. `COMM −` ist die **einzige** Verbindung zwischen Logik- und Leistungsmasse (§5.5).
   Auf der Werkbank gibt es über das Notebook einen zweiten Massepfad, wenn ESP32 und
   Regler am selben Rechner hängen — zum Testen harmlos, für den Messlauf kommt das
   Notebook ab.
4. Steckverbinder #2 **Ader für Ader auf Durchgang prüfen**, bevor Spannung
   draufkommt. 5 V und 3,3 V liegen auf demselben Stecker; ein um einen Pin versetzter
   Stecker zerstört entweder den HX711 oder den ESP32 (§5.4).

## 13 Bedienfeld über WLAN

Der Prüfstand spannt einen **eigenen Accesspoint** auf und liefert die Oberfläche selbst
aus — kein Internet, kein Router, keine App. Bedient wird am Telefon oder am iPad; das
LCD (§5.6) ist damit **optional** und zeigt nur dasselbe noch einmal.

Die Seite ist dieselbe Datei, die auch als Entwurf im Browser läuft:
`firmware/web/index.html`. `firmware/web/embed.py` backt sie gzip-komprimiert nach
`src/webui_html.h` (34 KB → 11 KB) und läuft als `pre:`-Skript bei jedem Build, kann
also nicht veralten. Kein LittleFS, kein zweiter Flashvorgang.

### 13.1 Konstanten

| Konstante | Wert | Anmerkung |
|---|---|---|
| `WEBUI_SSID` | `Pruefstand` | |
| `WEBUI_PASSWORD` | 14 Zeichen | **WPA2**, nicht offen — sonst kann jeder in Reichweite den Motor fahren. Eingetippt wird es nie, es steckt im QR-Code. Unter 8 Zeichen bleibt der AP offen. |
| `WEBUI_AP_IP` | **4.3.2.1** | **nicht** 192.168.x.x, Begründung §13.3 |
| `WEBUI_CHANNEL` | 6 | |
| `WEBUI_MAX_CLIENTS` | 4 | der Vorgabewert von `softAP()` ist ebenfalls 4 |
| `WEBUI_PORT` | 80 | 443 bleibt **zu**: die HTTPS-Prüfungen von iOS und Android lassen sich ohnehin nicht beantworten, und eine abgewiesene Verbindung ist schneller als ein Zeitablauf |
| `WEBUI_HOSTNAME` | `pruefstand` | mDNS → `pruefstand.local`, Bequemlichkeit, kein Verlasspfad |
| `LINK_TIMEOUT_MS` | 2000 ms | **Sicherheitsgrenze**, siehe [07-sicherheit](07-sicherheit.md) §4 |

### 13.2 Kein Captive Portal — und warum

Ein Captive Portal wurde gebaut, erprobt und wieder **entfernt**. Die Messungen vom
2026-11-15 und die Belegstellen dazu:

- Das Portalfenster von iOS (Captive Network Assistant) **ist kein Browser**: kein
  Adressfeld, keine Reiter, es gehört der Einstellungen-App. Eine Seite darin kann
  **nicht** nach Safari übergeben — dafür gibt es keine Schnittstelle. Das Schema
  `x-safari-http://` wurde am Gerät probiert und greift nicht; in Safaris Plist ist
  ohnehin nur `x-safari-https` registriert, undokumentiert und versionsabhängig.
- **iOS trennt das Netz**, sobald das Fenster zugeht, weil ein Netz ohne Internet als
  entbehrlich gilt. Gemessen: Verbindung hielt 15 s, mit Karenzphase 40 s, dann weg.
  Für einen 30-Minuten-Warmlauf, dessen Totmannschaltung an genau dieser Verbindung
  hängt, ist das untauglich.
- Im Portalfenster **läuft** `fetch`, löst aber keine erneute Prüfung aus — eine
  Anmeldung per Javascript lässt den Benutzer vor einem *Abbrechen*-Knopf sitzen, und
  Abbrechen trennt.

**Stattdessen:** Der Stand beantwortet die Verbindungsprüfung so, wie das Betriebssystem
sie erwartet (Apples Erfolgsseite byte-genau mit abschließendem `\n`, Androids nacktes
`204`, Microsofts 22 Zeichen). Dann gilt das Netz als in Ordnung, **nichts springt auf,
und nichts trennt sich**. Aufgerufen wird die Oberfläche über den QR-Code (§13.4) oder
jede beliebige Adresse im Browser — der DNS beantwortet alles mit der eigenen.

> Der Preis steht in RFC 8952 §1: Solange das Telefon verbunden ist, hält es die
> Verbindung für brauchbar und schaltet nicht auf Mobilfunk um. Nachrichten und Mail
> stehen also still. Nach der Messung WLAN aus.

### 13.3 Warum 4.3.2.1 und nicht 192.168.4.1

Androids `NetworkMonitor` verwirft die Verbindungsprüfung, sobald der DNS einen Prüf-Host
auf eine **private** Adresse auflöst, und sendet die HTTP-Anfrage **gar nicht erst ab**:

```java
if (mPrivateIpNoInternetEnabled && probeType == PROBE_HTTP
        && (proxy == null) && hasPrivateIpAddress(resolvedAddr)) {
    return CaptivePortalProbeResult.PRIVATE_IP;
}
```

`hasPrivateIpAddress()` deckt 10/8, 172.16/12, **192.168/16** und 169.254/16 ab. Das
Ergebnis auf Android wäre „Verbunden, kein Internet" — ohne jede Möglichkeit, das zu
heilen. Deshalb setzt auch das arduino-esp32-Beispiel `8.8.4.4` und WLED `4.3.2.1`.

Der Adressraum gehört jemand anderem. Unschön, aber ohne Internet am Stand folgenlos.

### 13.4 QR-Codes

Zwei Aufkleber, erzeugt mit `qrencode`, als PNG zum Drucken und SVG zum Skalieren:

| Datei | Nutzlast | tut |
|---|---|---|
| `hardware/qr-wlan.png` | `WIFI:T:WPA;S:Pruefstand;P:<Kennwort>;;` | Kamera bietet „Mit WLAN verbinden" an |
| `hardware/qr-ui.png` | `http://4.3.2.1/` | öffnet die Bedienung im **echten Browser** |

Beim ersten Mal zweimal scannen. Danach verbindet sich das Telefon von allein, und
**„Zum Home-Bildschirm"** macht aus der Bedienung ein Symbol, das sie im Vollbild ohne
Browserleisten startet (`apple-mobile-web-app-capable` ist gesetzt).

Ab etwa 3 cm Kantenlänge aus Armlänge lesbar, in Werkstattlicht eher 4–5 cm. Die weiße
Ruhezone außen muss bleiben, sonst findet die Kamera die Positionsmarken nicht.

### 13.5 Grenzen

- `WebServer` aus dem Arduino-Core kann laut eigenem Quelltextkommentar **nur einen
  Client gleichzeitig**. Bei einer Prüfungssalve verschluckt er Anfragen; die Oberfläche
  toleriert das, weil sie Aussetzer erst nach `LINK_TIMEOUT_MS` als Abriss wertet.
  Erst wenn Livewerte über WebSocket laufen sollen, lohnt ESPAsyncWebServer.
- **Der Accesspoint ist offen zugänglich**, auch mit WPA2 — wer das Kennwort vom
  Aufkleber liest, kann fahren. Für einen Stand mit Motor ist das eine bewusste
  Abwägung, keine Nachlässigkeit: der Not-Aus ist Hardware, die Bremse stromlos zu.

## 14 Reglervariante

Eigenständige Firmware, die **nur** den Motorregler fährt: keine Wägezelle, keine
Hall-Geber, kein LCD, keine Zustandsmaschine, kein Messbetrieb. Drei Drähte zum
Regler — `COMM` TX, RX und Masse (§12.2) —, und er liefert Drehzahl, Busspannung,
Motorstrom, Reglertemperatur und Fehlercode von selbst.

Zweck: den Antriebsstrang in Betrieb nehmen und kennenlernen, **bevor** ein einziger
Messwert ansteht. Die Messkette ist in wesentlichen Punkten noch TBD — Zelle
unkalibriert, Hebelarm nicht ausgemessen —, der Regler dagegen ist vollständig da.

> **Diese Variante hat ihren Zweck erfüllt.** Mit ihr und den Werkzeugen unter `tools/`
> wurden am 2026-11-17 Polzahl, R, L, Flussverkettung, Hall-Tabelle, Leerlaufkennlinie,
> Leerlaufverluste der Trommel und vier Bremswege vermessen. Die Abschnitte unten sind
> deshalb teils überholt und nachgezogen.

Das ist kein Umweg: [10-firmware](10-firmware.md) §2 (T1…T8) und §5.5 (D23…D28)
fordern genau diese Funktionen bereits.

    pio run -e esp32s3-vesc -t upload      # Reglervariante
    pio run -e esp32s3 -t upload           # zurück zur Hauptfirmware

### 14.1 Was sie kann

Bewusst das Nötigste: **Drehzahl in ERPM setzen** (± 100 und ± 1000), **Start**,
**Stop**. Angezeigt werden fünf Werte — Drehzahl, Sollwert, Busspannung, Motorstrom,
Reglertemperatur — dazu Fehlercode im Klartext und das Statusbyte.

Befehle, identisch über serielle Konsole und Weboberfläche ([10-firmware](10-firmware.md) G5):

| | |
|---|---|
| `r<erpm>` | Sollwert setzen |
| `s` | Stop — Antrieb stromlos, Trommel läuft aus |
| `?` | Hilfe |

### 14.2 ERPM, und warum die Umrechnung jetzt zulässig ist

Der Regler rechnet in **elektrischen** Umdrehungen; mechanisch ist das
`ERPM / Polpaarzahl`.

Ursprünglich wurde hier **nur** ERPM angezeigt, weil die Polpaarzahl nicht gemessen war
und jede Umrechnung eine erfundene Zahl gewesen wäre. Das `si_motor_poles` des Reglers
taugte dafür nicht: es ist, was jemand eingetippt hat — und stand auf dem
**Werkswert 14**, während es in Wirklichkeit 30 Pole sind.

**Seit dem 2026-11-17 ist die Polpaarzahl gemessen: 15** (900 Tachoschritte auf 10
markierte Handumdrehungen, [03-parameter](03-parameter.md) §3). Die Umrechnung ist damit
eine Rechnung und keine Schätzung mehr, und `VESC_ERPM_MAX` ist **abgeleitet** statt
geraten:

```
VESC_ERPM_MAX = MAX_DRUM_RPM · MOTOR_POLE_PAIRS = 353 · 15 = 5295
```

Vorher stand dort 2400, aus der Annahme, 7 Polpaare seien der kleinste plausible Wert.
Die Annahme war konservativ gemeint und lag um Faktor 2 daneben — die Trommel hätte
damit nur 160 1/min erreicht.

### 14.3 Motortemperatur — Fühler bestätigt, Abfrage noch nicht nachgezogen

Ursprünglich wurde das Feld **gar nicht abgefragt**, weil der Regler ohne Fühler am
`TMP`-Eingang eine Phantasiezahl **ohne Fehlermeldung** liefert, typisch einen großen
negativen Wert. Ein Strich ist ehrlicher als eine Zahl, die aussieht wie eine Messung.

**A5 ist seit dem 2026-11-17 beantwortet:** es ist ein 10-k-NTC im Stator, dreifach
belegt ([06-offene-fragen](06-offene-fragen.md) A5), und `m_motor_temp_sens_type` steht
richtig. Der Wert ist damit echt — eben jene Phantasiezahl von −75,5 °C bei offener
Klemme war der *Beweis* dafür.

Die Variante fragt das Feld trotzdem noch nicht ab (Bit 1 fehlt in der Maske von
`vesc/vesc.cpp`). Das ist jetzt ein **Nachtrag**, keine Festlegung mehr. [TBD]

### 14.4 Zwei Totmannschaltungen

| | greift wenn | Wirkung |
|---|---|---|
| **in dieser Firmware** | der Bedienrechner länger als `LINK_TIMEOUT_MS` schweigt | Antrieb wird freigegeben, Trommel läuft aus |
| **im Regler** | länger als `timeout_msec` kein Sollwert ankommt | der Regler stellt selbst ab |

Die zweite greift auch dann, wenn **diese Firmware** stehenbleibt — deshalb wird der
Sollwert nur wiederholt, solange gefahren wird, und sonst gar nichts gesendet.

> **`COMM_GET_VALUES` setzt den Timeout des Reglers NICHT zurück.** Nur Sollwerte und
> `COMM_ALIVE` tun das. Wer meint, mit Messwertabfragen den Antrieb am Leben zu
> halten, irrt sich — und merkt es erst, wenn der Motor mitten im Lauf abstellt.

### 14.5 Einmalige Einrichtung im VESC-Tool über USB

Ohne diese Schritte antwortet der Regler gar nicht oder falsch:

1. `app_to_use` = **UART** (oder PPM+UART / ADC+UART). Ein konfigurierter externer
   IMU verhindert den Start der Schnittstelle ebenfalls — gemeinsame Pins.
2. Baudrate **115200**.
3. Motortyp **FOC**, Sensormodus **Hall**. Sensorlos gibt es unterhalb
   `foc_openloop_rpm` keine Momentenkontrolle.
4. `si_motor_poles` = **30** (gemessen, §10.5).
5. `timeout_msec` auf **300**.
6. Motor- und Hall-Erkennung einmal laufen lassen — oder die gesicherten Werte als XML
   zurückschreiben, das geht **ohne Motor** (§10.5a).

> **Punkt 5 stand hier falsch.** Es hieß: „`timeout_brake_current` ausdrücklich über
> null — die Vorgabe 0,0 A heißt *auslaufen lassen*, nicht bremsen." Das klingt
> vorsichtig und ist für diesen Prüfstand **schädlich**. Testweise auf 1,0 A gesetzt und
> am 2026-11-17 wieder zurückgenommen: der Zustand **rastet ein**, die Endstufe läuft
> dauerhaft weiter (4,8 W, FET auf 32,9 °C), und ein dauerhaft kurzgeschlossener Motor
> **lässt sich nicht frei drehen** — der Auslaufversuch ist aber eine Kernmethode dieses
> Stands. Begründung in §10.5. Der Wert bleibt **0**; gebremst wird über die Relais oder
> über `VESC_BRAKE_CURRENT_STOP`.

Die Firmwareversion meldet die Variante beim Start selbst (`COMM_FW_VERSION`). Sie
entscheidet, ob `foc_short_ls_on_zero_duty` und `l_battery_regen_cut_start/end`
überhaupt existieren — beide erst ab **6.05**. Der Regler läuft seit dem 2026-11-17 auf
**7.01**, beide sind vorhanden und gesetzt (§10.5a).
