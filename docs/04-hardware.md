# Hardware

## 1 Blockbild

```
                 ┌──────────────────────────────────────────┐
                 │                 ESP32                    │
                 │                                          │
   Hall-Marken ──┤ GPIO25 (ISR)        I²C ──┬── LCD 4×20   │
   Hall-Index  ──┤ GPIO26 (ISR)              │   (PCF8574)  │
   Motor-Hall  ──┤ GPIO27 (ISR)              │              │
                 │                           └── (frei)     │
   HX711 DOUT  ──┤ GPIO32                                   │
   HX711 SCK   ──┤ GPIO33          UART2 ───── Motorregler  │
                 │                 (TX17/RX16)              │
   Not-Aus-     ─┤ GPIO34 (in)                              │
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

Das verfügbare Moment ist der einzige Punkt, der für den Riemen spricht. Da der
Auslegungsfall bei 3,53 Nm stationär liegt (siehe [02-messkonzept](02-messkonzept.md) §4),
reichen 5,52 Nm mit 36 % Reserve. Der Riemen wäre eingetauschte Messgüte für nicht
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

Vorschlag, mit `firmware/src/config.h` abgeglichen. Anpassen, sobald das konkrete
ESP32-Board feststeht — nicht alle GPIOs sind auf allen Boards frei oder als Eingang
nutzbar.

| GPIO | Konstante | Funktion | Richtung | Anmerkung |
|---|---|---|---|---|
| 25 | `PIN_MARK` | Drehzahlmarken Trommel | in, Interrupt | 6 Marken/Umdrehung |
| 26 | `PIN_INDEX` | Indexmarke Trommel | in, Interrupt | 1/Umdrehung, triggert die Mittelung |
| 27 | `PIN_MOTOR_HALL` | Motor-Hall (eine Phase) | in, Interrupt | Riemenschlupf-Überwachung |
| 32 | `PIN_HX711_DOUT` | HX711 DOUT | in | |
| 33 | `PIN_HX711_SCK` | HX711 SCK | out | |
| 21 | `PIN_I2C_SDA` | I²C SDA | bidir | LCD |
| 22 | `PIN_I2C_SCL` | I²C SCL | out | LCD |
| 17 | `PIN_DRIVE_TX` | UART2 TX → Regler | out | |
| 16 | `PIN_DRIVE_RX` | UART2 RX ← Regler | in | |
| 34 | `PIN_ESTOP_FB` | Not-Aus-Rückmeldung | in | nur **Anzeige** des Zustands; die Trennung erfolgt in Hardware |
| 35 | `PIN_MOTOR_PTC` | Motor-PTC | in, analog | Temperaturüberwachung; Kennlinie des PTC noch unbekannt |

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

Bei 60 km/h (ω = 34,9 rad/s), Auflast 42,5 kg:

| Fall | Moment | P mech | Strom bei kt = 0,568 Nm/A |
|---|---|---|---|
| guter Rennreifen | 1,28 Nm | 45 W | 2,3 A |
| Marathon Plus | 2,06 Nm | 72 W | 3,6 A |
| langsamster MTB | 3,07 Nm | 107 W | 5,4 A |
| Auslegung (Crr 0,015) | 3,53 Nm | 123 W | 6,2 A |
| + Anfahren am Rampenende | 4,20 Nm | 146 W | 7,4 A |

Elektrisch rund **160 W, in der Spitze 190 W**. Der Nennstrom des Motors liegt bei etwa
9,7 A — im Auslegungsfall werden also 64 % davon gebraucht.

Netzteil **36 V / 10 A** genügt; 15 A geben Luft, damit die Strombegrenzung beim Anfahren
vom Regler kommt und nicht vom Netzteil.

**Rückspeisung beachten:** Beim rekuperativen Bremsen fließt Energie zurück. Ein
Labornetzteil kann sie nicht aufnehmen, die Zwischenkreisspannung steigt und der Regler
schaltet mit Überspannung ab. Lösungen: Bremswiderstand mit Chopper (1,4 kJ über 10 s
sind 140 W, ein 100-W-Widerstand reicht), eine Batterie als Puffer, oder die mechanische
Trommelbremse des Motors benutzen.

## 8 Gebergehäuse für die Hall-Sensoren

CAD-Quelle: `hardware/hall_gehaeuse.py` (FreeCAD-Skript, parametrisch).
Erzeugt `hardware/hall-gehaeuse.FCStd`, `.step` und `.stl`. Die Datei wird aus dem
Skript erzeugt, nicht umgekehrt — Maßänderungen immer im Skript.

Außenmaß **37,2 × 20,0 × 10,0 mm**, 3D-Druck, Materialvolumen 4,6 cm³.

### 8.1 Aufbau

| Merkmal | Maß | Begründung |
|---|---|---|
| Kavität (Platine) | 20 × 14 × 8 mm | handgesägte Lochrasterplatine 18 × 13 mm, 2,54er Raster, 7 × 5 Löcher |
| Stirnwand vor dem Geber | 1,2 mm | geht direkt in den Magnet-Luftspalt ein, daher so dünn wie druckbar |
| Seitenwände / Deckel | 3,0 / 2,0 mm | |
| M6-Durchgang | Ø 6,6 mm, 4,5 mm aus der Mitte versetzt | Platz für den Kabelkanal daneben |
| Kabelkanal | D-Nut 4,4 mm breit, **3,4 mm tief** | Kabel Ø 4 mm, dreiadrig |
| LED-Loch | Ø 3,2 mm im Deckel | 3-mm-LED von innen durchgesteckt |

Die Unterseite ist offen. Der Hall-Geber (TLE4905L, PG-SSO-3-2) steht aufrecht — also
**parallel zur Schraubenachse** — am Kopfende direkt hinter der 1,2-mm-Stirnwand, die
beschriftete, magnetempfindliche Seite nach außen.

### 8.2 Montage und Zugentlastung

Das Gehäuse wird mit der offenen Seite auf das Alublech gesetzt und mit **einer**
M6-Schraube festgezogen. Daraus folgt dreierlei:

1. Das Blech deckt die Platine ab (Isolation durch Klebeband oder Kleber auf der
   Blechseite — das Gehäuse hat dafür keine eigene Wand).
2. Die Nut ist mit 3,4 mm flacher als das Kabel dick ist. Das Blech klemmt den Mantel
   um 0,6 mm ein, das ist die **Zugentlastung** — ohne zusätzliche Schelle.
3. Nur eine Schraube heißt: das Gehäuse kann sich um sie drehen. Die Ausrichtung des
   Gebers zur Trommelmarke muss beim Anziehen mit eingestellt werden.

Es ist bewusst nichts verrastet und nichts verschraubt außer dieser einen M6 — das Teil
soll ohne Nacharbeit druckbar und mit einer Hand montierbar sein.
