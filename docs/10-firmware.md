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
| B2 | S | Fünfter Taster **Mitte = Bestätigen**. |
| B3 | M | Entprellung in Software, kein RC im Signalpfad. |
| B4 | M | Dauerdruck auf Hoch/Runter wiederholt mit Autorepeat. |
| B5 | M | **Schrittweite 1 oder 5 km/h** über Rein/Raus oder einen Modusschalter — nicht über eine versteckte Tastenkombination. |
| B6 | M | Der **Not-Aus ist kein Taster am Steuerkreuz**, sondern der zwangsöffnende Pilzschalter im Leistungspfad ([07-sicherheit](07-sicherheit.md) §3). Die Firmware sieht ihn nur über `PIN_ESTOP_FB`. |
| B7 | S | Dieselbe Navigation über die serielle Konsole, damit ohne Bedienteil gearbeitet werden kann. |

Bauform: **Fünfwege-Tastschalter** (Navigationstaster, z. B. ALPS SKQU) — ein Bauteil,
fünf Kontakte, gemeinsame Masse. Alternativ fünf einzelne Taster.

## 4 Not-Stop

| | P | Anforderung |
|---|---|---|
| N1 | M | Auslösbar jederzeit und aus jeder Betriebsart. |
| N2 | M | **Reihenfolge: erst Treiber sperren, dann Relais abfallen lassen.** Die Phasen dürfen nicht kurzgeschlossen werden, solange der Regler noch treibt. |
| N3 | M | Relais abgefallen = Phasen kurzgeschlossen = gebremst. Gemessen 2026-11-01: rund **25 s** aus über 400 1/min. |
| N4 | M | Zustand ist **rastend**. Quittierung nur im Stillstand. |
| N5 | M | Derselbe Zustand wird von den Abschaltkriterien in [07-sicherheit](07-sicherheit.md) §4 erreicht. |
| N6 | M | Beim Reset und beim Booten sind die Relais abgefallen, der Stand ist also gebremst. |

## 5 Diagnoseseiten

Eine Seite je Sensor, einzeln anwählbar. Zweck: beim Aufbau prüfen, **ob** ein Geber
schaltet, und **wie gut**.

| | P | Anforderung |
|---|---|---|
| D1 | M | Jede Seite funktioniert **im Stillstand bei Drehen von Hand** — Einzelimpulse werden sichtbar, nicht nur ein Mittelwert. |
| D2 | M | Jede Seite zeigt einen **Impulszähler**, der sich einzeln hochzählen lässt. |
| D3 | M | Seite **Trommelmarken**: Zähler, Drehzahl, Impulse je Indexumdrehung, Gleichverteilung (§5.1). |
| D4 | M | Seite **Index**: Zähler, Umdrehungen, Prüfung auf **genau 1 Impuls je Umdrehung**. |
| D5 | M | Seite **Motor-Hall**: Zähler und **Impulse je Trommelumdrehung** — damit wird `MOTOR_PULSES_PER_REV` bestimmt, bisher TBD. |
| D6 | S | Seite **Laufrad**: Zähler, Drehzahl, Impulse je Umdrehung. |
| D7 | M | Seite **Wägezelle**: Rohwert, Mittelwert, **Streuung** über die letzten N Werte, Abtastrate. |
| D8 | S | Seite **Lichtschranke**: Zustand und Flankenzähler. |
| D9 | K | Seite **Temperatur**: Rohwert des Fühlers, bis A5 geklärt ist. |

### 5.1 Drehzahl und Gleichverteilung der Marken

| | P | Anforderung |
|---|---|---|
| D10 | M | Drehzahl in **1/min mit einer Nachkommastelle** und Umfangsgeschwindigkeit in km/h, zum Abgleich mit einem Handdrehzahlmesser. |
| D11 | M | **Pulszahl je Umdrehung** gegen den Sollwert 8. Jede Abweichung heißt: eine Marke schaltet nicht oder prellt. |
| D12 | M | **Winkelabweichung je Marke** gegen die ideale 45°-Teilung, in Grad **und** in mm am Umfang. |
| D13 | M | **Kennzahl**: größter Betrag und Standardabweichung über alle acht Marken. |
| D14 | S | **Impulsbreite je Marke**, umgerechnet in Grad. Eine deutlich schmalere Marke heißt zu großer Luftspalt oder schwacher Magnet — das ist das Maß für die Schaltreserve. Setzt Auswertung **beider Flanken** voraus. |
| D15 | M | Mittelung über ≥ 50 Umdrehungen bei konstanter Drehzahl; der Messwert gilt nur auf einem Plateau, nicht während der Rampe. |

Rechenweg: über eine Umdrehung die acht Intervalle `t_i` messen, daraus

```
φ_i = 360° · (Σ_{k≤i} t_k) / T_U  −  i · 45°
```

Zulässig ist das, weil sich ω innerhalb einer Umdrehung um höchstens 0,15 % ändert
([09-messtheorie](09-messtheorie.md) §1.7). Dieselben φ_i sind zugleich die
Korrekturtabelle aus [04-hardware](04-hardware.md) §8.5 — gemessen statt gebohrt.

## 6 Kalibriermodus Wägezelle

Prüfgewicht an den Hebelarm hängen, daraus `LOADCELL_N_PER_COUNT`.

| | P | Anforderung |
|---|---|---|
| W1 | M | Nullpunkt aufnehmen (Arm unbelastet), Mittelwert über ≥ 200 Werte. |
| W2 | M | Masse des Prüfgewichts und **Anhängeradius** eingeben: 40 mm (direkt) oder 80 mm (doppelter Hebel). |
| W3 | M | Mit Gewicht messen, Mittelwert über ≥ 200 Werte, Faktor berechnen und anzeigen. |
| W4 | M | Faktor wird **angezeigt, nicht automatisch übernommen** — er gehört von Hand in `config.h` und [03-parameter](03-parameter.md). |
| W5 | S | **Mehrere Massen** nacheinander, Linearität über den Bereich prüfen und die Abweichung anzeigen. |
| W6 | S | Dieselbe Masse an beiden Radien als Gegenprobe der Hebelarmmaße. |
| W7 | K | Kriechen: Last 5 min anstehen lassen, Abweichung protokollieren. |

Umrechnung: eine Masse *m* am Radius *b* erzeugt an der Zelle (r = 40 mm)
`F = m · g · b / 0,040 m`. Am 80-mm-Loch also die **doppelte** Kraft.

## 7 Offen

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
