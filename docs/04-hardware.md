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

**Direktantrieb**: Die Trommel sitzt auf dem Rotor des Nabenmotors, die Statorachse ist
die Pendelachse. Keine Übersetzung, kein Riemen, keine Kupplung.

```
            Prüflaufrad (abhebbar)
                   ↓ F_N = 417 N (42,5 kg)
        ┌──────────────────────────┐
        │     ╭────────────╮       │   Trommel U = 3 m, auf dem Motorrotor
        │    (   Trommel    )      │   → Trommellager liegen INNERHALB der
        │     ╰─────┬──────╯       │     Pendelwiege und werden nicht mitgemessen
        └───────────┼──────────────┘
              Statorachse, beidseitig in Stehlagern
              auf dem Wiegerahmen  (Kabel treten axial
              auf der Pendelachse aus)
                    │
               Hebelarm L = 0,07 m
                    │
        ════════════╪════════════  Wägezelle 10 kg
                                   + Wirbelstromdämpfer
                                   + weiche Endanschläge beidseitig
```

Der Wiegerahmen dreht sich um dieselbe Achse wie die Trommel. Seine Lagerreibung ist das
einzige äußere Störmoment, das sich nicht herausrechnen lässt — deshalb Flexgelenk oder
sehr gutes Wälzlager.

### Warum Direktantrieb

| | Direktantrieb | mit Zahnriemen |
|---|---|---|
| Trommellager | innerhalb der Wiege → **nicht gemessen** | außerhalb → gemessen, driftet mit der Temperatur |
| Übersetzungsverluste | keine | η ≈ 0,97 → 2–3 % systematischer Skalenfehler |
| Leerlauf gesamt bei 60 km/h | **0,29 Nm** (nur Aerodynamik) | 0,29 + Lager + Riemen |
| Anregung des Wiegeteils | nur Rastmoment | zusätzlich Zahneingriff |
| Felgenadapter nötig | nein | ja |
| verfügbares Moment | 5,52 Nm | 9,94 Nm bei i = 1,8 |
| Reserve über dem Auslegungsfall (2,51 Nm) | **Faktor 2,2** | Faktor 4,0 |

Das verfügbare Moment ist der einzige Punkt, der für den Riemen spricht. Da der
Auslegungsfall samt Rampe bei 2,51 Nm liegt ([08-staender](08-staender.md) §5),
reichen 5,52 Nm mit **Faktor 2,2**. Der Riemen wäre eingetauschte Messgüte für nicht
benötigtes Moment.

### Voraussetzung

Die Trommel hängt damit an den Motorlagern. Das setzt voraus, dass die **Statorachse
beidseitig** gelagert werden kann (offene Frage A6) und dass die Trommel symmetrisch zur
Motorebene gebaut wird, damit kein Biegemoment auf die Motorlager wirkt. Ist das nicht
gegeben, bekommt die Trommel eigene Lager und der Motor einen Zahnriemen — die
Firmware-Konstante `BELT_RATIO` existiert genau für diesen Fall.

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
| Nabenmotor | Bird One / DAAO 23328N36A28G001, 9,5″, 36 V / 350 W, 605 1/min | ausgewählt |
| Gegenstecker Julet 9-polig | oder Stecker abschneiden und eigene Verbinder setzen | offen |
| Motorregler | sensorierte FOC, UART-Sollwert, Bremsfunktion (VESC-Klasse **oder** umgeflashte Hoverboard-Platine) | offen |
| Netzteil | 36–42 V, ≥ 10 A | offen |
| ESP32-Board | beliebig, ≥ 2 freie Interrupt-Pins | offen |
| Hall-Sensoren | 2 Stück, unipolar oder latching, digital | offen |
| Magnete | 6 + 1 für Marken und Index | offen |
| Netzteil | 36 V, ≥ 10 A (besser 15 A) — Reserve für die Stromgrenze beim Anfahren | offen |
| Bremswiderstand + Chopper | ~100 W, falls rekuperativ gebremst werden soll; ein Netzteil kann Rückspeisung nicht aufnehmen | offen |
| Stehlager für die Statorachse | 2 Stück, auf dem Wiegerahmen | offen |
| Zahnriemen | **entfällt beim Direktantrieb** — nur nötig, falls A6 negativ ausfällt | – |
| Not-Aus | zwangsöffnend, trennt den Leistungspfad | offen |
| Zweiter HX711 | für die spätere Auflastmessung | später |
| BLE-Drucksensor | Modell noch nicht festgelegt | später |

## 4 Pinbelegung

Board: **ESP32-S3-DevKitC-1**, Modul WROOM-1 **N16R8** (16 MB Flash, 8 MB Octal-PSRAM).
Mit `firmware/src/config.h` und `firmware/platformio.ini` abgeglichen.

Maßgeblich ist `hardware/schaltplan.py` — dort ist auch die Peripherie verdrahtet, die
die Firmware noch nicht anfasst.

| GPIO | Konstante | Funktion | Richtung | Anmerkung |
|---|---|---|---|---|
| 1 | `PIN_MOTOR_PTC` | Motor-PTC | in, analog | **ADC1**_CH0 — ADC2 ist bei aktivem WLAN gesperrt. Kennlinie des PTC noch **TBD** |
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

## 5 Verdrahtungshinweise, die messtechnisch zählen

- **HX711 getrennt versorgen.** Eigene saubere 5 V, Sternmasse, nicht am Motorzweig.
- **Zellenkabel verdrillt und geschirmt**, Schirm einseitig an den Zellenkörper.
- **HX711 thermisch vom Motor entkoppeln** — Offsetdrift.
- **Motorzuleitungen über das Pendellager als weiche Schlaufe.** Beim Nabenmotor treten
  sie axial durch die Statorachse aus, also auf der Pendelachse — der Vorteil geht
  verloren, wenn das Kabel straff abgeht.
- **Logik- und Leistungsmasse an genau einem Punkt verbinden.**
- **LCD-Pegel beachten:** PCF8574-Module laufen meist auf 5 V; der ESP32 ist 3,3-V-Logik.
  Ein Modul mit 3,3-V-Betrieb wählen oder Pegelwandler vorsehen.

## 6 Warum der Motor nicht an der Trommeloberfläche angreift

Eine naheliegende Variante wäre, das Hoverboard-Rad mit seinem Vollgummireifen als
Reibrolle gegen die Trommel zu drücken. Das geht nicht:

```
Trommeloberfläche bei 60 km/h   = 16,67 m/s
Raddrehzahl dafür                = 16,67 / 0,718 · 60 = 1393 1/min
Motor-Nenndrehzahl bei 36 V      = 605 1/min
```

Mehr als das Doppelte der Nenndrehzahl — unerreichbar. An der **Welle** sind dagegen nur
333 1/min nötig. Unabhängig davon wäre der Schlupf einer Reibrolle lastabhängig und fiele
damit nicht sauber in die Leerlaufkennlinie.

## 7 Leistungsbedarf und Netzteil

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
sind 140 W, ein 100-W-Widerstand reicht), eine Batterie als Puffer, oder die mechanische
Trommelbremse des Motors benutzen.

## 8 Gebergehäuse für die Hall-Sensoren

Feststehende Seite der Drehzahlerfassung, am Rahmen. Gegenstück ist §9.

CAD-Quelle `hardware/hall_gehaeuse.py` → `hall-gehaeuse.FCStd`, `.step`, `.stl`.
Maßänderungen immer im Skript. 3D-Druck, Außenmaß **37,2 × 20,0 × 12,0 mm**, 5,1 cm³.

### 8.1 Maße

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
| LED-Loch | Ø 3,2 im Deckel, 15 mm von der Stirnwand |

Stirnwand 1,2 mm, weil sie in den Magnet-Luftspalt eingeht (§9.3). Nut 3,4 mm bei
4 mm Kabel, damit das Blech den Mantel klemmt — das ist die Zugentlastung.

### 8.2 Platinenaufnahme

| Merkmal | Maß |
|---|---|
| Auflageleisten | 1,5 mm Vorsprung je Seite, 10 mm lang, mittig; Auflage 1,0 mm |
| Rastnase | 0,8 mm Vorsprung, 6 mm lang, mittig; Überdeckung 0,30 mm |
| Haltefläche der Nase | z = 3,7 mm (0,1 mm Luft über der Platine) |
| Einführschräge | 1,0 mm hoch, unterhalb der Nase |
| Federfeld, von außen freigestellt | 12 mm lang, 0,8 mm Restwand |

Die Platine wird von unten eingeschoben, rastet über die Nasen und liegt auf den
Leisten. Leisten und Nasen sitzen **nur mittig**: an den Enden bleibt die volle
Breite für Bauteile frei, und die 3-mm-Wand federt nur im freigestellten Feld
überhaupt. Deshalb auch die kleine Überdeckung von 0,30 mm — mehr ließe sich nicht
montieren.

### 8.3 Montage

Offene Seite auf das Alublech, eine M6-Schraube. Daraus folgt:

1. Das Blech schließt das Gehäuse und klemmt das Kabel in der Nut.
2. Die Platine hat 2,0 mm Abstand zum Blech. Klebeband als Isolation ist damit
   Rückfallebene, nicht Voraussetzung.
3. Eine Schraube heißt verdrehbar — die Ausrichtung zur Trommelmarke wird beim
   Anziehen mit eingestellt.

## 9 Markenaufnahme an der Trommel

Mitdrehende Seite, Gegenstück zu §8. Kein Druckteil.

**8 Magnete** auf dem Teilkreis der 8 Abstandsklötze der Trommel-Stirnfläche. Dort
ist die 15-mm-Multiplexscheibe von hinten abgestützt, der Radius ist maximal, und
die Teilung liegt durch den vorhandenen Aufbau fest.

### 9.1 Teile

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

### 9.2 Die Schraube muss nichtmagnetisch sein

Keine Korrosions-, sondern eine Messfrage: die Schraube sitzt im Flussweg des axial
magnetisierten Magneten. Stahl schließt Nord und Süd durch den Magneten kurz, das
Feld wird ringförmig statt mittig, der Schaltpunkt wandert, im Grenzfall gibt es zwei
Flanken pro Magnet. **A2 oder A4, niemals verzinkter Stahl.**

Der Kopf der 2,9er misst 5,5 mm und sitzt in einer Senkung für Ø3 eher etwas tiefer
als eine 3,0er — erwünscht, weil der Stahl damit weiter vom Sensor weg ist.

### 9.3 Feldreserve

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

### 9.4 Polung

TLE4905L ist unipolar. Alle acht Magnete mit derselben Seite nach außen; ein
Zylindermagnet passt in beiden Richtungen ins Loch, die Mechanik verhindert den
Dreher nicht. **Jeden Magneten vor dem Festziehen einzeln gegen den Sensor prüfen.**

### 9.5 Winkellage: messen statt bohren

Bohrgenauigkeit ist nicht nötig. Bei hoher Drehzahl ändert sich ω innerhalb einer
Umdrehung um höchstens 0,15 % ([09-messtheorie](09-messtheorie.md) §1.7); die
Abweichungen der acht Intervalle von je 1/8 sind damit direkt die Winkelfehler.
Einmal im Auslauf aufnehmen, als Korrekturtabelle ablegen.

Nötig nur für Drehzahl **unterhalb** einer Umdrehung. Was ins Ergebnis geht, läuft
index-getriggert über ganze Umdrehungen — dort kürzt sich der Winkelfehler exakt
heraus.
