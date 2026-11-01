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

## 3 Not-Stop

| | P | Anforderung |
|---|---|---|
| N1 | M | Auslösbar jederzeit und aus jeder Betriebsart. |
| N2 | M | **Reihenfolge: erst Treiber sperren, dann Relais abfallen lassen.** Die Phasen dürfen nicht kurzgeschlossen werden, solange der Regler noch treibt. |
| N3 | M | Relais abgefallen = Phasen kurzgeschlossen = gebremst. Gemessen 2026-11-01: rund **25 s** aus über 400 1/min. |
| N4 | M | Zustand ist **rastend**. Quittierung nur im Stillstand. |
| N5 | M | Derselbe Zustand wird von den Abschaltkriterien in [07-sicherheit](07-sicherheit.md) §4 erreicht. |
| N6 | M | Beim Reset und beim Booten sind die Relais abgefallen, der Stand ist also gebremst. |

## 4 Kalibriermodus Wägezelle

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

## 5 Offen

| | Frage |
|---|---|
| ? | **Masse des Prüfgewichts.** Vermutet 500 g — das ergibt am 80-mm-Loch nur **10 % vom Zellenendwert**, gemessen wird aber bei 13 bis 64 %. Siehe Hinweis unten. |
| ? | **Bedienteil.** `PIN_BUTTON` = GPIO14 ist **ein** Taster; Hoch/Runter/Schrittweite/Not-Stop brauchen mehr. Festzulegen: Anzahl Taster oder Bedienung nur über die Konsole. |
| ? | **Pin für die Relaisansteuerung** (MOSFET in der Spulenmasse) ist noch nicht vergeben. |

> **Zum Prüfgewicht.** 500 g am 80-mm-Loch sind 9,81 N an der Zelle, also 10 % vom
> Endwert; der Messbetrieb liegt bei 12 bis 63 N. Eine Kalibrierung bei 10 % auf 64 %
> zu extrapolieren verlässt sich ganz auf die Linearität. Besser: mehrere Massen bis
> etwa **4 kg** am 80-mm-Loch (= 90 % FS), mindestens aber drei Punkte über den
> genutzten Bereich.
