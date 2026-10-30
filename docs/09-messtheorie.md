# Messtheorie, Fehler und Ablauf

Die geschlossene Darstellung: welche Formeln gelten, welche Größen einmal und welche vor
jeder Messung bestimmt werden, wo die Sensoren sitzen, wie kalibriert wird und was der
ESP32 braucht, um Watt und Crr live zu rechnen.

Ergänzt [02-messkonzept](02-messkonzept.md) (Herleitung und Fallstricke) und
[05-prüfablauf](05-pruefablauf.md) (Checklisten). Zahlenwerte gelten für den Stand vom
2026-10-30: Trommel Ø 933 mm, Hebelarm 40 mm, Auslegung auf schnelle Straßenreifen.

## 1 Die Formelkette

### 1.1 Vom Rohwert zur Verlustleistung

```
(1)  F_Z   = (raw − offset) · N_PER_COUNT                    [N]   Kraft an der Wägezelle
(2)  M     = F_Z · L                                         [Nm]  Moment an der Trommelwelle
(3)  F_roll= M / r                                           [N]   Umfangskraft am Trommelmantel
(4)  ω     = 2π · n                                          [rad/s]
(5)  v     = U · n                                           [m/s]  Umfangsgeschwindigkeit
(6)  P     = M · ω  =  F_roll · v                            [W]   gesamte Verlustleistung
```

`n` ist die Trommeldrehzahl in 1/s, gemessen über die Marken. `r = U / 2π`.
**Die Geschwindigkeit kommt in (1) bis (3) nicht vor** — die Crr-Genauigkeit hängt nicht
an der Drehzahlmessung, sondern nur an Kraft und Geometrie.

### 1.2 Vom Gesamtverlust zum Reifen

Gemessen wird alles, was **außerhalb** der Pendelwiege Moment zieht. Drei Anteile:

```
(7)  M_ges(v) = M_Reifen(v) + M_leer(v) + M_Rad(v) + M_Lager(N)
```

| Anteil | woher | wie behandelt |
|---|---|---|
| `M_leer` | Trommelaerodynamik | **Leerlaufkennlinie**, Rad abgehoben, vor und nach der Reihe |
| `M_Rad` | Nabenlager + Aerodynamik des Prüflaufrads | **Laufrad-Auslaufversuch**, [08-staender](08-staender.md) §6 |
| `M_Lager` | lastabhängiger Anteil der Trommellager | Referenzlauf mit starrer Rolle (A11), sonst Restfehler |
| `M_Reifen` | **das Signal** | bleibt übrig |

```
(8)  F_roll,Reifen = (M_ges − M_leer − M_Rad − M_Lager) / r
(9)  Crr  = F_roll,Reifen / N
(10) P_Reifen = Crr · N · v
```

Beide Korrekturkurven werden als Modell geführt, nicht als Tabelle:

```
(11) M_leer(ω) = A₀ + B₀ · ω²          aus dem Baseline-Lauf gefittet
(12) M_Rad(ω)  = (A₁ + B₁ · ω_Rad²) · r / r_Rad    aus dem Laufrad-Auslauf, auf die Trommel umgerechnet
```

`ω_Rad = ω · r / r_Rad`, weil Trommel und Rad dieselbe Umfangsgeschwindigkeit haben.

### 1.3 Die Auflast

Schwinge mit Gegengewicht ausgewuchtet, Drehpunkt auf Radachshöhe:

```
(13) N = m_Rad · g + W · b / a            a = 200 mm, W = Laufgewicht, b = Skalenwert
```

Weil der Drehpunkt auf Radachshöhe liegt, hat die Umfangskraft keinen Hebel — der
Korrekturterm `∓ F_roll · Δz / a` entfällt ([08-staender](08-staender.md) §5.2).

### 1.4 Zahlenbeispiel und Gegenprobe

Continental Contact Urban, 18,4 W bei 4 bar, 42,5 kg, 28,8 km/h (publiziert):

```
Crr      = 18,4 W / (8,0 m/s · 416,9 N)   = 0,00552
F_roll   = 0,00552 · 416,9 N              = 2,301 N
M        = 2,301 N · 0,4665 m             = 1,0736 Nm
F_Zelle  = 1,0736 Nm / 0,040 m            = 26,84 N      (27 % vom Zellenendwert)
P_Reifen = 0,00552 · 416,9 · 8,0          = 18,41 W      ✓ stimmt mit der Quelle
```

Diese Rechnung rückwärts ist der beste Funktionstest der ganzen Kette: wer einen Reifen
mit publiziertem Wert fährt, muss am Ende dieselbe Wattzahl herausbekommen — bis auf die
Trommelkrümmung, siehe §9.

## 2 Welche Größe wann bestimmt wird

### 2.1 Einmalig — Anlagenkonstanten

Ändern sich nur bei Umbau. Gehören nach `firmware/src/config.h` **und**
[03-parameter](03-parameter.md).

| Größe | Symbol | Ermittlung | Zielgenauigkeit |
|---|---|---|---|
| Trommelumfang | `U` | Maßband **über die fertige Oberfläche**, 3 Messungen mitteln | ±1 mm (0,03 %) |
| Trommelradius | `r = U/2π` | gerechnet | — |
| Hebelarm Drehmomentwaage | `L` | Achsmitte Pendel → Krafteinleitung, Messschieber | ±0,1 mm (0,25 %) |
| Kalibrierradius | `L_cal` | zweite Bohrung, hier 80,0 mm | ±0,1 mm |
| Zellenkennwert | `N_PER_COUNT` | §5.1, mindestens 3 Gewichte | ±0,2 % |
| Hebel der Auflast | `a` | Pivotachse → Radachse, waagerecht | ±0,5 mm (0,25 %) |
| Marken pro Umdrehung | `MARKS_PER_REV` | Festlegung, 6 + Index | exakt |
| Trommelträgheit | `J_Trommel` | Auslaufversuch, [05-prüfablauf](05-pruefablauf.md) | ±5 % (nur für die Rampenkorrektur) |
| Leerlaufmodell | `A₀, B₀` | Baseline-Lauf, gefittet | siehe §3 |

### 2.2 Pro Laufrad — einmal je Laufrad, nicht je Reifen

| Größe | Ermittlung | Zielgenauigkeit |
|---|---|---|
| Trägheitsmoment `J_Rad` | Bifilarpendel, [08-staender](08-staender.md) §6.2 | ±1,5 % |
| Radverlustmodell `A₁, B₁` | Laufrad-Auslaufversuch | ±2 % |
| Laufradmasse `m_Rad` | Waage | ±20 g |
| Referenz-Reifenmasse `m_Reifen,ref` | Waage, gehört zu `J_Rad` | ±1 g |

### 2.3 Pro Reifen / pro Messreihe

| Größe | Ermittlung | Anmerkung |
|---|---|---|
| Reifenmasse `m_Reifen` | Waage | korrigiert `J_Rad`: `ΔJ = Δm · r_Reifen²` |
| Reifenradius `r_Rad` | Umfang mit Maßband, belastet gemessen | geht in (12) ein |
| Auslegerhöhe am Mast | auf Radachshöhe einstellen, Klemmhebel | Wasserwaage am Gabelholm |
| Reifendruck `p` | Manometer, direkt vor dem Lauf | Konsole `p<wert>` |
| Auflast `N` | aus `b` auf der Skala, Formel (13) | Konsole `l<wert>` |
| Statische Tara `offset` | Konsole `t`, Trommel steht | **nach jedem Eingriff** |
| Leerlaufkennlinie | Baseline vor **und** nach der Reihe | Abbruchkriterium §3.3 |
| Umgebungstemperatur | Thermometer, Start und Ende | ins Protokoll |

### 2.4 Pro Messpunkt

| Größe | Quelle |
|---|---|
| `v` bzw. `n` | Markenzähler, Mittel über ganze Umdrehungen |
| `F_Z` | HX711, Mittel über dieselbe ganze Zahl von Umdrehungen |
| Reifentemperatur | Sensor **TBD**, bis dahin Protokollnotiz |
| Zeitstempel | für die zeitliche Interpolation der Baseline |

## 3 Fehlerbudget

### 3.1 Alles in F_roll rechnen

`F_roll = Crr · N` enthält weder r noch L — das macht die Beiträge vergleichbar.
Bezugsgröße ist das Ziel aus [01-requirements](01-requirements.md) M1:
**ΔCrr = 0,0005 ⇒ ΔF_roll = 0,208 N.**

| Signalgröße | Crr | F_roll | M | F_Zelle |
|---|---|---|---|---|
| bester Rennreifen | 0,0025 | 1,042 N | 0,486 Nm | 12,2 N |
| guter Rennreifen | 0,0037 | 1,543 N | 0,720 Nm | 18,0 N |
| Contact Urban | 0,0055 | 2,301 N | 1,074 Nm | 26,8 N |

### 3.2 Die Beiträge

| Fehlerquelle | auf F_roll | vom Ziel | vom Signal* |
|---|---|---|---|
| Zellenrauschen, Einzelsample | 0,0017 N | 0,8 % | 0,11 % |
| Zellenrauschen nach 10 Umdrehungen | 0,0001 N | 0,1 % | 0,01 % |
| Zelle, FS-Fehler 0,02 % (Kriechen, Wiederholbarkeit) | 0,0017 N | 0,8 % | 0,11 % |
| Pendellager-Hysterese (1,2 mNm) | 0,0026 N | 1,2 % | 0,17 % |
| Hebelarm L auf ±0,1 mm | 0,0039 N | 1,8 % | 0,25 % |
| Umfang U auf ±1 mm | 0,0005 N | 0,3 % | 0,03 % |
| **J_Rad auf ±1,5 %** | 0,0080 N | 3,9 % | 0,52 % |
| J_Rad auf ±10 % *(wenn geschätzt statt gemessen)* | 0,0536 N | 25,7 % | 3,47 % |
| **Trommellager, lastabhängig (4,7 mNm)** | 0,0101 N | 4,8 % | 0,65 % |
| **Baseline-Drift, Kriterium 0,02 N** | 0,0200 N | 9,6 % | 1,30 % |
| *Baseline-Drift, altes Kriterium 0,10 N* | *0,1000 N* | *48,0 %* | *6,48 %* |

\* bezogen auf einen guten Rennreifen (1,543 N)

Quadratisch summiert ergeben die fett gesetzten Zeilen rund **0,025 N = 12 % vom Ziel**.
Damit sind zwei Reifen, die sich um ΔCrr = 0,0005 unterscheiden, sauber trennbar — aber
nur, wenn die drei großen Posten wirklich klein gehalten werden.

### 3.3 Das alte Baseline-Kriterium ist zu weich geworden

[05-prüfablauf](05-pruefablauf.md) §7 verwirft eine Reihe bei mehr als **0,10 N**
Abweichung zwischen Vor- und Nachlauf. Das war bei MTB-Reifen (F_roll bis 6,3 N) ein
Prozentwert von 1,6 %. Bei einem guten Rennreifen sind dieselben 0,10 N **6,5 %** des
Signals, beim besten Reifen 9,6 %.

→ **Kriterium auf 0,02 N verschärfen.** Das sind 1,3 % vom Rennreifensignal und 9,6 % vom
Auflösungsziel. Erreichbar ist es, weil der Leerlauf selbst nur 0,267 Nm (= 0,57 N an
F_roll) beträgt: 0,02 N sind 3,5 % Drift der Leerlaufkurve über eine Messreihe.

### 3.4 Die Auflast geht linear ein

`Crr = F_roll / N` — ein Prozent Fehler auf N ist ein Prozent auf Crr.

| Fehler auf N | Fehler auf Crr (bei 0,0037) |
|---|---|
| 0,5 % (2,1 N) | 0,000019 |
| 1,0 % (4,2 N) | 0,000037 |
| 2,0 % (8,3 N) | 0,000074 |

Selbst 2 % bleiben unter ΔCrr = 0,0005. Die Auflast ist also **nicht** der kritische Pfad
— solange sie zwischen den verglichenen Reifen dieselbe ist. Für den Vergleich mit
publizierten Daten dagegen zählt der Absolutwert: dort auf 1 % zielen.

## 4 Sensorik: was wo sitzt

| Sensor | Ort | Signal | Pin |
|---|---|---|---|
| **Trommelmarken** | 6 Magnete auf der Stirnscheibe, gleichmäßig, r so groß wie möglich | Hall, Interrupt | `PIN_MARK` 25 |
| **Indexmarke** | 1 Magnet, andere Spur oder anderer Radius | Hall, Interrupt | `PIN_INDEX` 26 |
| **Motor-Hall** | im Motor vorhanden, eine Phase herausführen | Interrupt | `PIN_MOTOR_HALL` 27 |
| **Wägezelle** | Hebelarm → Zelle, Anbindung über abgesetzten M5-Stab | HX711, 80 SPS | 32 / 33 |
| **Laufraddrehzahl** *(neu)* | **4 Magnete an den Speichen**, Hall am Ausfallende | Interrupt | **TBD, freier Pin** |
| Reifentemperatur | berührungslos auf die Lauffläche, kurz nach dem Latsch | analog oder I²C | **TBD** |
| Umgebungstemperatur | am Rahmen, weg von Motor und Netzteil | I²C | **TBD** |
| Motor-PTC | im Motor | analog | `PIN_MOTOR_PTC` 35 |
| Reifendruck | BLE-Sensor am Ventil | BLE | — |

Zwei Dinge, die bei der Montage entscheiden, ob die Messung taugt:

1. **Marken so weit außen wie möglich.** Die Pulsperiode ist die Zeitbasis für v; je
   größer der Radius, desto unempfindlicher gegen Positionsfehler der Magnete. Ungleiche
   Teilung erzeugt sonst eine Scheinmodulation mit der Drehfrequenz — genau dort, wo auch
   die Stoßstelle des Mantelblechs sitzt.
2. **Hallsensor für das Laufrad am Ausfallende, nicht an der Gabel.** Er muss mit dem Rad
   mitwandern, wenn die Radgröße verstellt wird.

## 5 Kalibrierung

### 5.1 Wägezelle und Momentenkette

Kalibriert wird **nicht die Zelle allein, sondern die ganze Kette** bis zum Moment. Das
erspart die getrennte Unsicherheit von Zelle und Hebelarm.

1. Wiegeteil in Messlage, Trommel steht, Kabel wie im Betrieb verlegt.
2. Statische Tara (`t`), 200 Samples.
3. Bekanntes Gewicht an die **Kalibrierbohrung** bei `L_cal` = 80,0 mm hängen.
   Moment `M_cal = m · g · L_cal`. Rohwert über 200 Samples mitteln.
4. Mit mindestens **drei** Massen über den Bereich wiederholen — bei L = 40 mm deckt der
   Auslegungsfall 2,51 Nm ab, also z. B. 1 / 2 / 3 kg an 80 mm = 0,785 / 1,57 / 2,35 Nm.
5. Geradenfit `M = k · (raw − offset)`. **Linearitätsabweichung < 0,2 %** verlangen.
6. Hysterese prüfen: aufsteigend und absteigend belasten. Die Differenz bei gleicher Last
   ist die Hysterese der Pendellagerung — sie gehört ins Protokoll, nicht in den Fit.
7. `N_PER_COUNT = k / L` nach `config.h` und [03-parameter](03-parameter.md).

> Kalibriergewichte selbst wiegen. Eine Küchenwaage auf 1 g bei 2 kg ist 0,05 % — gut
> genug. Die Fallbeschleunigung mit 9,81 m/s² anzusetzen reicht; der Ortsunterschied in
> Deutschland liegt unter 0,1 %.

### 5.2 Auflast

1. Schwinge **ohne Laufrad und ohne Laufgewicht** mit dem Gegengewicht auswuchten.
   Restkraft an der Radposition mit einer Federwaage prüfen: **< ±2 N**.
2. Laufrad wiegen (`m_Rad`).
3. `a` waagerecht ausmessen, ±0,5 mm.
4. Maßband auf den Holm kleben, Null **auf der Pivotachse**. Index am Laufgewicht ankörnen.
5. Gegenprobe: Personenwaage auf die Trommel, Rad absenken, Anzeige mit (13) vergleichen.
   Abweichung ≤ 2 % erwartet — die Waage hebt das Rad um ihre Bauhöhe an, das ist eine
   Plausibilitätsprüfung, **nicht** die Kalibrierung.

### 5.3 Null, zweimal

| | wann | erfasst |
|---|---|---|
| **statische Tara** | Trommel steht, vor jeder Reihe | ADC-Offset, Eigengewicht der Wiege, Drift |
| **Leerlaufkennlinie** | Trommel dreht, Rad abgehoben | alles, was außerhalb der Wiege Moment zieht |

Beides ist nötig. Die statische Tara allein lässt die Aerodynamik stehen, die
Leerlaufkennlinie allein lässt den ADC-Offset stehen.

## 6 Was der ESP32 rechnet und anzeigt

### 6.1 Konstanten, die fest im Code stehen

```c
DRUM_CIRCUMFERENCE_M   U          gemessen          → r = U/(2π)
TORQUE_ARM_M           L  0,040   gemessen
LOADCELL_N_PER_COUNT   k          kalibriert §5.1
MARKS_PER_REV          6
```

### 6.2 Werte, die vor dem Lauf eingegeben werden

Über die serielle Konsole, einzeilig, und mit ins CSV:

| Befehl | Größe | Beispiel | wofür |
|---|---|---|---|
| `l<N>` | Auflast | `l416.9` | Crr, Gleichung (9) |
| `p<bar>` | Reifendruck | `p4.0` | nur Protokoll |
| `r<m>` | Reifenradius | `r0.339` | Umrechnung (12) |
| `w<A₁> <B₁>` | Radverlustmodell | `w0.012 0.000071` | Gleichung (12) |
| `n<A₀> <B₀>` | Leerlaufmodell | `n0.004 0.000209` | Gleichung (11) |
| `t` | statische Tara | | Offset |

`A₀/B₀` fallen aus dem Baseline-Lauf an; die Firmware kann sie selbst fitten und anbieten.
`A₁/B₁` kommen aus dem Laufrad-Auslauf und bleiben, bis das Laufrad gewechselt wird.

### 6.3 Die Live-Rechnung, Schritt für Schritt

```
je Sample:   F_Z = (raw − offset) · k
je Umdrehung (Index-getriggert, über AVG_REVS_PER_POINT Umdrehungen gemittelt):
             n   = Marken / (k_Marken · Δt)
             ω   = 2π·n              v = U·n
             M   = F_Z,mittel · L
             P_ges     = M · ω
             M_leer    = A₀ + B₀·ω²
             ω_Rad     = ω · r / r_Rad
             M_Rad     = (A₁ + B₁·ω_Rad²) · r / r_Rad
             M_Reifen  = M − M_leer − M_Rad
             F_roll    = M_Reifen / r
             Crr       = F_roll / N
             P_Reifen  = M_Reifen · ω        (= Crr · N · v)
```

**Die Wattzahl, die zählt, ist `P_Reifen`** — nicht `P_ges`. Die publizierten Werte sind
Reifenleistungen bei 28,8 km/h und 42,5 kg; `P_ges` enthält zusätzlich Trommel und
Laufrad und wäre bei 28,8 km/h um rund 2 W zu hoch (1,05 W Trommel + 0,99 W Laufrad).

### 6.4 Anzeige 4 × 20

```
v 28.8 km/h  n 196
P 12.3 W     Crr .00370
N 417 N      p 4.0 bar
MEASURE  rev 7/10
```

Zeile 3 zeigt die Eingaben, damit ein falsch gesetzter Wert auffällt, bevor die Reihe
läuft. Solange `LOADCELL_N_PER_COUNT` oder `A₁/B₁` fehlen, zeigt die Firmware statt Crr
und Watt **`--`** — kein Wert ist besser als ein falscher.

## 7 Die Messmatrix

### 7.1 Drei Achsen, drei Zwecke

| Achse | Stützstellen | wofür |
|---|---|---|
| **Geschwindigkeit** | 10, 15, 20, 25, **28,8**, 35, 40, 45, 50, 55, 60 km/h | Kennlinie Crr(v), Vergleichspunkt bei 28,8 |
| **Reifendruck** | 3, 4, 5, 6 bar (reifenabhängig) | Crr(p), der eigentlich interessante Zusammenhang |
| **Auflast** | 30, 42,5, 55 kg | Linearitätsprüfung: Crr soll nahezu lastunabhängig sein |

Vollfaktoriell wären das 11 × 4 × 3 = 132 Punkte je Reifen — unrealistisch. Praktikabel:

1. **Standardlauf je Reifen:** alle 11 Geschwindigkeiten bei 4 bar und 42,5 kg.
   Rund 15 min plus zwei Baselines.
2. **Druckreihe:** nur bei 28,8 km/h, alle Druckstufen, 42,5 kg. Je Stufe mindestens
   10 Umdrehungen, zwischen den Stufen **5 min Angleichzeit** — der Reifen ändert mit dem
   Druck auch seine Walkarbeit und damit seine Temperatur.
3. **Lastreihe:** nur bei 28,8 km/h und 4 bar, drei Lasten. Zeigt, ob die Hebelwaage
   linear arbeitet — Crr darf sich dabei kaum ändern.

### 7.2 Was sich zwischen den Achsen ändern darf

| | Laufrad | Reifen | J_Rad | Baseline | Warmlauf |
|---|---|---|---|---|---|
| andere Geschwindigkeit | gleich | gleich | gleich | gilt | einmal |
| anderer Druck | gleich | gleich | gleich | gilt | 5 min nachwärmen |
| andere Auflast | gleich | gleich | gleich | gilt | 5 min nachwärmen |
| **anderer Reifen** | gleich | **neu** | **korrigieren** | **neu** | **volle 30 min** |
| **anderes Laufrad** | **neu** | neu | **neu messen** | **neu** | **volle 30 min** |

Ein Reifenwechsel zieht also drei Dinge nach sich: J korrigieren (§2.3), neue Baseline,
voller Warmlauf. Ein Laufradwechsel zusätzlich den Bifilar- und den Auslaufversuch.

## 8 Ablauf einer Messreihe

Checklisten in [05-prüfablauf](05-pruefablauf.md); hier nur die Reihenfolge und warum:

| # | Schritt | Dauer | warum an dieser Stelle |
|---|---|---|---|
| 1 | Sichtprüfung, Haube, Not-Aus | 5 min | [07-sicherheit](07-sicherheit.md) |
| 2 | Laufrad einsetzen, Auslegerhöhe auf Radachshöhe | 10 min | bestimmt die Geometrie aller Folgeschritte |
| 3 | Reifen wiegen, Druck setzen, `J` korrigieren | 5 min | vor der Tara, weil es die Wiege nicht berührt |
| 4 | Statische Tara `t` | 1 min | nach jedem mechanischen Eingriff |
| 5 | **Baseline Vorlauf** `b`, Rad abgehoben | 15 min | kalt → liefert den unteren Eckwert der Drift |
| 6 | Rad absenken, **Warmlauf** 30 min bei 28,8 km/h | 30 min | Crr sinkt mit der Reifentemperatur |
| 7 | **Messlauf** `m` | 15 min | im thermisch eingeschwungenen Zustand |
| 8 | **Baseline Nachlauf** `b` | 15 min | warm → oberer Eckwert; Differenz ist das Abbruchkriterium |
| 9 | Auslaufen lassen, nicht bremsen | 5–30 min | §8 in 05 |
| 10 | Auswertung `analysis/crr.py` | 2 min | interpoliert Baseline zeitlich zwischen 5 und 8 |

Rund **zwei Stunden je Reifen**. Die Druckreihe (§7.1 Punkt 2) hängt sich an Schritt 7 an
und kostet je Druckstufe 10 Minuten.

Für jeden Vergleich gilt **A/B/A**: Referenzreifen — Prüfreifen — Referenzreifen in einer
Sitzung. Weicht die zweite Referenzmessung von der ersten ab, ist der Unterschied
dazwischen nicht interpretierbar.

## 9 Was dieses Verfahren nicht hergibt

| | |
|---|---|
| **Absolutwerte wie auf der Straße** | Die Trommelkrümmung erhöht den gemessenen Crr gegenüber der Ebene. Unsere Trommel ist mit 0,93 m größer als die 0,77 m des Referenzprüfstands, der Zuschlag also kleiner — die Richtung ist bekannt, die Größe nicht (C1 in [06-offene-fragen](06-offene-fragen.md)). |
| **Trennung Reifen ↔ Schlauch ↔ Felgenband** | Gemessen wird das System. Wer den Schlauch vergleichen will, muss alles andere konstant halten. |
| **Nasshaftung, Pannenschutz, Verschleiß** | Nicht-Ziele, [01-requirements](01-requirements.md) §5. |
| **Der lastabhängige Lageranteil** | 4,7 mNm = 0,65 % vom Signal, bis A11 (Referenzlauf mit starrer Rolle) läuft. |
| **Reifentemperatur als Messgröße** | Sensor noch offen; ohne ihn ist der Warmlauf eine Zeitvorgabe, keine Zustandsgröße. |

Was es dafür **sicher** hergibt: den Vergleich zweier Reifen auf demselben Laufrad, bei
derselben Last, demselben Druck und derselben Temperatur, mit einer Auflösung von
ΔCrr ≈ 0,0005 — das ist der Unterschied zwischen einem guten und einem sehr guten
Rennreifen, und mehr als das brauchen die meisten Entscheidungen nicht.
