# Firmware-Anforderungen

Was die Firmware können muss. Das *Wie* steht hier nicht.

Priorität: **M** = muss für den ersten Messlauf · **S** = soll · **K** = kann später.
Stand 2026-11-01, erste Fassung nach Ansage.

## 1 Grundsätze

| | P | Anforderung |
|---|---|---|
| G1 | M | Kein `delay()` im Hauptpfad, Zustandsmaschine nicht blockierend. |
| G2 | M | ISRs kurz: Zeitstempel in Ringpuffer, sonst nichts, `IRAM_ATTR`. |
| G3 | M | Solange `LOADCELL_N_PER_COUNT` = 0 ist, werden **keine** Kräfte und kein Crr ausgegeben, nur Rohwerte. |
| G4 | M | Alle physikalischen Konstanten aus `config.h`, keine Literale im Code. |
| G5 | S | Jede Betriebsart ist über die serielle Konsole erreichbar, auch ohne Bedienteil. |

## 2 Testmodus

Zweck: Motor und Mechanik in Betrieb nehmen, bevor gemessen wird.

| | P | Anforderung |
|---|---|---|
| T1 | M | Motor ein- und ausschalten. |
| T2 | M | Sollgeschwindigkeit über **Hoch/Runter** verstellen. |
| T3 | M | Schrittweite **1 km/h und 5 km/h**, umschaltbar. |
| T4 | M | Sollwertbereich **0 bis 80 km/h**, begrenzt durch `MAX_DRUM_RPM`. Oberhalb der Freigabe nach [07-sicherheit](07-sicherheit.md) §5 verweigert die Firmware den Sollwert. |
| T5 | M | Anzeige von **Soll- und Istgeschwindigkeit** gleichzeitig. |
| T6 | M | Anzeige von **Umfangsgeschwindigkeit in km/h und Drehzahl in 1/min**. |
| T7 | S | Anzeige des Rohwerts der Wägezelle, damit der Momentenpfad geprüft werden kann. |
| T8 | S | Sollwert wird beim Verlassen des Testmodus auf 0 gesetzt. |

## 3 Bedienung

**Steuerkreuz mit fünf Tastern plus separatem Not-Aus.**

| | P | Anforderung |
|---|---|---|
| B1 | M | Taster **Hoch / Runter / Rein / Raus**, jeweils gegen Masse, interner Pull-up. |
| B2 | M | Fünfter Taster **Mitte = Bestätigen**. Entschieden und verdrahtet, `PIN_BTN_OK` = GPIO 39 ([04-hardware](04-hardware.md) §12.3). |
| B3 | M | Entprellung in Software, kein RC im Signalpfad. |
| B4 | M | Dauerdruck auf Hoch/Runter wiederholt mit Autorepeat. |
| B5 | M | **Schrittweite 1 oder 5 km/h** über Rein/Raus oder einen Modusschalter — nicht über eine versteckte Tastenkombination. |
| B6 | M | Der **Not-Aus ist kein Taster am Steuerkreuz**, sondern der zwangsöffnende Pilzschalter im Leistungspfad ([07-sicherheit](07-sicherheit.md) §3). Die Firmware sieht ihn nur über `PIN_ESTOP_FB`. |
| B7 | S | Dieselbe Navigation über die serielle Konsole, damit ohne Bedienteil gearbeitet werden kann. |

Bauform: **Fünfwege-Tastschalter** (Navigationstaster, z. B. ALPS SKQU) — ein Bauteil,
fünf Kontakte, gemeinsame Masse. Alternativ fünf einzelne Taster. **Keine** analogen
Joystick-Breakouts: die geben eine Spannung, keine fünf Kontakte.

## 4 Not-Stop

| | P | Anforderung |
|---|---|---|
| N1 | M | Auslösbar jederzeit und aus jeder Betriebsart. |
| N2 | M | **Reihenfolge: erst Treiber sperren, dann Relais abfallen lassen.** Die Phasen dürfen nicht kurzgeschlossen werden, solange der Regler noch treibt. |
| N3 | M | Relais abgefallen = Phasen kurzgeschlossen = gebremst. Gemessen 2026-11-01: rund **25 s** aus über 400 1/min. |
| N4 | M | Zustand ist **rastend**. Quittierung nur im Stillstand. |
| N5 | M | Derselbe Zustand wird von den Abschaltkriterien in [07-sicherheit](07-sicherheit.md) §4 erreicht. |
| N6 | M | Beim Reset und beim Booten sind die Relais abgefallen, der Stand ist also gebremst. |

## 3.1 Bedienfeld über WLAN

| | P | Anforderung |
|---|---|---|
| W1 | M | Der Stand liefert die Oberfläche **selbst** aus, über den eigenen Accesspoint. Kein Internet, kein Router, keine App ([04-hardware](04-hardware.md) §13). |
| W2 | M | **Dasselbe Befehlsvokabular wie die serielle Konsole.** `/api/cmd?c=…` reicht die Zeichenkette an denselben Parser weiter — es gibt nur eine Liste von Befehlen (G5). |
| W3 | M | **Totmannschaltung**: ohne Herzschlag des Bedienrechners bricht ein laufender Lauf ab ([07-sicherheit](07-sicherheit.md) §4). |
| W4 | S | Dieselbe Datei läuft als Entwurf im Browser (simuliert) und auf dem Gerät (live). Antwortet `/api/state`, führt der Prüfstand; sonst simuliert die Seite sich selbst. |
| W5 | S | Anzeigewerte höchstens **5× je Sekunde** auffrischen. Der HX711 liefert 80 SPS; angezeigt wird der gleitende Mittelwert über 40 Werte. Ein mit 80 Hz zappelnder Zahlenwert ist weder lesbar noch aussagekräftig. |
| W6 | K | Ausgelegt für ein **iPad im Querformat** im Kioskmodus, aber bis auf Handybreite benutzbar. |

**Das LCD ist damit optional.** `display::present()` ist false, wenn unter
`LCD_I2C_ADDR` niemand antwortet; dann läuft alles über Konsole und Bedienfeld. Das LCD
zeigt dasselbe noch einmal, es ersetzt nichts und wird von nichts vorausgesetzt.

## 4.1 Status-LED

| | P | Anforderung |
|---|---|---|
| L1 | M | Die RGB-LED des DevKits zeigt **ausschließlich** den Zustand der Bedienverbindung. Kein Startanimation: die Verbindungsanzeige steht schon wenige Millisekunden nach dem Start und sagt mehr aus. |
| L2 | M | **Grün stetig** = ein Bedienrechner meldet sich, **rot blinkend** = es wird gewartet. Geschrieben wird nur bei Zustandswechsel — `neopixelWrite()` belegt die RMT und dauert rund 30 µs. |
| L3 | M | Die Animation ist **nicht blockierend** (G1): `statusled::service()` läuft aus `loop()`, es gibt kein `delay()`. |

Konstanten in [03-parameter](03-parameter.md) §8; die Pin-Frage v1.0/v1.1 steht dort
als TBD.

## 5 Diagnoseseiten

Eine Seite je Sensor, einzeln anwählbar. Zweck: beim Aufbau prüfen, **ob** ein Geber
schaltet, und **wie gut**.

| | P | Anforderung |
|---|---|---|
| D1 | M | Jede Seite funktioniert **im Stillstand bei Drehen von Hand** — Einzelimpulse werden sichtbar, nicht nur ein Mittelwert. |
| D2 | M | Jede Seite zeigt einen **Impulszähler**, einzeln rücksetzbar. |
| D3 | M | Anzeige ist **4 × 20 Zeichen**. Passt eine Seite nicht, wird sie geblättert, nicht gekürzt. |

### 5.1 Übersicht Rohwerte

| | P | Anforderung |
|---|---|---|
| D4 | M | **Eine Seite mit allen Gebern gleichzeitig**, jeder als Rohwert: Trommelmarken, Index, Motor-Hall, Laufrad, Lichtschranke, Wägezelle. |
| D5 | M | Digitale Geber als **Pegel und Zähler**, die Wägezelle als **Rohwert in Counts** — ungefiltert, unkalibriert. |
| D6 | M | Aktualisierung mindestens 5× je Sekunde, damit Handdrehen sichtbar wird. |

Zweck ist der Verdrahtungstest: ein Blick, ob überhaupt jeder Kanal lebt.

**Stand 2026-11-22:** die digitalen Geber (D4, D5 ohne Lichtschranke) gibt es im
Bedienfeld unter *Einrichtung → Hall-Geber* als Zähler, Zeit seit dem letzten Impuls
und Pegel, 5× je Sekunde (D6), Zähler gemeinsam nullbar mit `g`
([04-hardware](04-hardware.md) §14.1). Die Wägezelle steht daneben als Rohwert unter
*Wägezelle kalibrieren*.

### 5.2 Trommelmarken: Drehzahl und Gleichverteilung

| | P | Anforderung |
|---|---|---|
| D7 | M | Drehzahl in **1/min mit einer Nachkommastelle** und Umfangsgeschwindigkeit in km/h, zum Abgleich mit einem Handdrehzahlmesser. |
| D8 | M | **Pulszahl je Umdrehung** gegen den Sollwert 8. Jede Abweichung heißt: eine Marke schaltet nicht oder prellt. |
| D9 | M | **Winkelabweichung je Marke** gegen die ideale 45°-Teilung, in Grad **und** in mm am Umfang. |
| D10 | M | **Kennzahl**: größter Betrag und Standardabweichung über alle acht Marken. |
| D11 | S | **Impulsbreite je Marke** in Grad. Eine deutlich schmalere Marke heißt zu großer Luftspalt oder schwacher Magnet — das ist das Maß für die Schaltreserve. Setzt Auswertung **beider Flanken** voraus. |
| D12 | M | Mittelung über ≥ 50 Umdrehungen bei konstanter Drehzahl; gilt nur auf einem Plateau, nicht während der Rampe. |

```
φ_i = 360° · (Σ_{k≤i} t_k) / T_U  −  i · 45°
```

Zulässig, weil sich ω innerhalb einer Umdrehung um höchstens 0,15 % ändert
([09-messtheorie](09-messtheorie.md) §1.7). Dieselben φ_i sind die Korrekturtabelle aus
[04-hardware](04-hardware.md) §8.5 — gemessen statt gebohrt.

### 5.3 Weitere Geberseiten

| | P | Anforderung |
|---|---|---|
| D13 | M | **Index**: Zähler, Umdrehungen, Prüfung auf genau 1 Impuls je Umdrehung. |
| D14 | M | **Motor-Hall**: Zähler und **Impulse je Trommelumdrehung** — damit wird `MOTOR_PULSES_PER_REV` bestimmt, bisher TBD. |
| D15 | S | **Laufrad**: Zähler, Drehzahl, Impulse je Umdrehung. *Zähler und Drehzahl seit 2026-11-22 vorhanden, dazu der wirksame Reifendurchmesser aus dem Drehzahlverhältnis ([04-hardware](04-hardware.md) §14.1).* |
| D16 | S | **Lichtschranke**: Zustand und Flankenzähler. |

### 5.4 Wägezelle

| | P | Anforderung |
|---|---|---|
| D17 | M | **Rohwert** in Counts, ungefiltert. |
| D18 | M | **Mittelwert, Streuung und Spitze-Spitze** über die letzten N Werte. |
| D19 | M | **Glättung N zur Laufzeit verstellbar**, Wirkung auf die Streuung sofort sichtbar. So wird `AVG_REVS_PER_POINT` belegt statt geschätzt. |
| D20 | M | **Tatsächliche Abtastrate** in SPS — damit fällt auf, wenn der RATE-Pin auf 10 statt 80 SPS steht ([04-hardware](04-hardware.md) §5.5). |
| D21 | S | **Drift seit der letzten Tara**, in Counts und in Minuten. |
| D22 | S | Nach Kalibrierung zusätzlich in **N und Nm**. |

### 5.5 Motor

Werte vom Regler über UART. Macht aus dem Regler ein zweites Messgerät.

| | P | Anforderung |
|---|---|---|
| D23 | M | **Drehzahl** vom Regler, umgerechnet auf mechanische 1/min — Gegenprobe zu den Trommelmarken. |
| D24 | M | **Motorstrom und Eingangsstrom**, **Busspannung**, **Aussteuerung**. |
| D25 | M | **Reglertemperatur** und, sobald A5 geklärt ist, **Motortemperatur**. |
| D26 | M | **Fehlercode des Reglers** im Klartext. |
| D27 | S | Verbrauchte Ah und Wh seit Lauf-Beginn. |
| D28 | S | **Moment aus dem Strom** rechnen (`M = kt · I`) und neben das Moment aus der Wägezelle stellen. |

> D28 ist die wertvollste Zeile dieser Seite: zwei voneinander unabhängige
> Momentenmessungen. Sie ersetzt die Wägezelle nicht — kt ist nur auf 10 bis 20 %
> bekannt und temperaturabhängig —, aber eine Abweichung um Faktor 2 fällt sofort auf.
> Setzt kt aus A13 und die Polzahl aus A4 voraus.

## 6 Kalibriermodus Wägezelle

Prüfgewicht an den Hebelarm hängen, daraus `LOADCELL_N_PER_COUNT`.

| | P | Anforderung |
|---|---|---|
| W1 | M | Nullpunkt aufnehmen (Arm unbelastet), Mittelwert über ≥ 200 Werte. |
| W2 | M | Masse des Prüfgewichts eingeben. Die **Hebelarme** stehen als eigene Felder auf derselben Seite (Zellenarm 80 mm, Kalibrierarm 160 mm) und werden am Aufbau ausgemessen, nicht je Wägung eingetippt. |
| W3 | M | Mit Gewicht messen, Mittelwert über ≥ 200 Werte, Faktor berechnen und anzeigen. |
| W4 | M | Faktor wird **angezeigt, nicht automatisch übernommen** — er gehört von Hand in `config.h` und [03-parameter](03-parameter.md). |
| W5 | S | **Mehrere Massen** nacheinander, Linearität über den Bereich prüfen und die Abweichung anzeigen. |
| W6 | S | Dieselbe Masse an beiden Radien als Gegenprobe der Hebelarmmaße. |
| W7 | K | Kriechen: Last 5 min anstehen lassen, Abweichung protokollieren. |

Umrechnung: eine Masse *m* am Radius *b* erzeugt an der Zelle (Arm L = 80 mm)
`F = m · g · b / L`. Am 160-mm-Loch also die **doppelte** Kraft — Endwert der 5-kg-Zelle
bei 2,4 kg, [09-messtheorie](09-messtheorie.md) §5.1.

## 7 Messseite

Die Seite, auf der während eines Laufs alles steht, was zählt.

| | P | Anforderung |
|---|---|---|
| L1 | M | **Drehzahl** in 1/min und **Umfangsgeschwindigkeit** in km/h, jeweils **Soll und Ist**. |
| L2 | M | **Leistung in W**. Solange die Zelle nicht kalibriert ist, die **elektrische** Leistung vom Regler, ausdrücklich so beschriftet. |
| L3 | M | **Motorstrom** und **Motortemperatur**. |
| L4 | M | **Drehzahlzustand**: konstant / beschleunigt / bremst. |
| L5 | M | **dv/dt numerisch** in km/h/s, nicht nur als Symbol. |
| L6 | M | Crr, sobald `LOADCELL_N_PER_COUNT` gesetzt ist. |
| L7 | S | Restliche Haltezeit auf dem Plateau. |
| L8 | S | Reifendruck und Auflast, wie vor dem Lauf eingegeben. |
| L12 | M | **Abrollradius `r_Rad` aus dem Drehzahlverhältnis** `r_Trommel · n_Trommel / n_Rad` berechnen und anzeigen, statt ihn eingeben zu lassen. Mittelung über ≥ 30 s auf einem Plateau. |
| L13 | S | Das Verhältnis als **Schlupfüberwachung**: Abweichung vom eingeschwungenen Wert um mehr als 1 % meldet rutschenden Reifen. |

### 7.1 Wann die Drehzahl „konstant" ist

Nicht Augenmaß, sondern Grenzwert. Ein Rest an Beschleunigung erzeugt über
`M = J · dω/dt` ein Scheinmoment, das direkt ins Ergebnis geht:

```
1 % vom Signal eines guten Rennreifens  = 0,012 Nm
α = 0,012 / 1,94 kg·m²                  = 0,0062 rad/s²
dv/dt                                    = 0,010 km/h/s
```

| | P | Anforderung |
|---|---|---|
| L9 | M | **konstant** gilt bei `\|dv/dt\| < 0,010 km/h/s`. Das entspricht 1 % Fehler. |
| L10 | M | Messpunkte, die außerhalb dieses Fensters aufgenommen wurden, werden **als ungültig markiert** — in der Anzeige und in der CSV-Zeile. |
| L11 | S | Schärferes Fenster `< 0,002 km/h/s` für 0,2 % wählbar. |

Zur Einordnung: über ein Mittelungsfenster von 10 Umdrehungen bei 60 km/h (1,7 s) sind
0,010 km/h/s gerade **0,017 km/h Drift**, also 0,03 % der Geschwindigkeit. Das Plateau
muss schärfer stehen, als man vermutet — deshalb gehört der Wert auf die Seite und nicht
in die Auswertung hinterher.

## 8 Eingaben vor dem Lauf

Über die serielle Konsole, einzeilig, und mit in die CSV-Kopfzeile.

| Befehl | Größe | Beispiel | wofür |
|---|---|---|---|
| `l<N>` | Auflast | `l416.9` | Crr |
| `p<bar>` | Reifendruck | `p4.0` | nur Protokoll |
| `r<m>` | Reifenradius | `r0.339` | **nur als Rückfall** — normalerweise aus dem Drehzahlverhältnis gemessen, L12 |
| `w<A₁> <B₁>` | Radverlustmodell | `w0.012 0.000071` | aus dem Laufrad-Auslauf |
| `n<A₀> <B₀>` | Leerlaufmodell | `n0.004 0.000209` | aus dem Baseline-Lauf |
| `t` | statische Tara | | Offset |

| | P | Anforderung |
|---|---|---|
| E1 | M | Jede Eingabe steht in der CSV, damit ein falsch gesetzter Wert später auffällt. |
| E2 | S | `A₀/B₀` fittet die Firmware nach dem Baseline-Lauf selbst und bietet sie an. |
| E3 | M | Fehlt eine Eingabe, bleibt die abhängige Größe `--`, sie wird nicht geschätzt. |

## 9 Anzeige

| | P | Anforderung |
|---|---|---|
| Z1 | M | **4 × 20 Zeichen**, HD44780 über PCF8574. |
| Z2 | M | Fehlt `LOADCELL_N_PER_COUNT` oder `A₁/B₁`, steht statt Crr und Watt **`--`**. Kein Wert ist besser als ein falscher. |
| Z3 | M | Eine Zeile zeigt immer die **Eingaben** (Auflast, Druck), damit ein Fehler vor dem Lauf auffällt. |
| Z4 | M | Betriebsart und Fortschritt sind immer sichtbar. |

Entwurf der Messseite:

```
v 28.8 km/h  n 196
P 12.3 W     Crr .00370
N 417 N      p 4.0 bar
MEASURE  rev 7/10  ok
```

## 10 Offen

| | Frage |
|---|---|
| ? | **Masse des Prüfgewichts.** Vermutet 500 g — das ergibt am 80-mm-Loch nur **10 % vom Zellenendwert**, gemessen wird aber bei 13 bis 64 %. Siehe Hinweis unten. |
| ? | **Pinbelegung** steht in [04-hardware](04-hardware.md) §4 und geht in `config.h`, sobald die Firmware-Arbeit wieder aufgenommen wird. |
| ? | **Überbrückungsschalter für die Bremsrelais.** N6 hält den Stand im stromlosen Zustand gebremst — zum Drehen von Hand (D1, Wuchten, Markenmontage) muss sich das abschalten lassen. |

> **Zum Prüfgewicht.** 500 g am 80-mm-Loch sind 9,81 N an der Zelle, also 10 % vom
> Endwert; der Messbetrieb liegt bei 12 bis 63 N. Eine Kalibrierung bei 10 % auf 64 %
> zu extrapolieren verlässt sich ganz auf die Linearität. Besser: mehrere Massen bis
> etwa **4 kg** am 80-mm-Loch (= 90 % FS), mindestens aber drei Punkte über den
> genutzten Bereich.
