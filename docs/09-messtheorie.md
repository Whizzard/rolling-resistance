# Messtheorie, Fehler und Ablauf

Die geschlossene Darstellung: welche Formeln gelten, welche Größen einmal und welche vor
jeder Messung bestimmt werden, wo die Sensoren sitzen, wie kalibriert wird und was der
ESP32 braucht, um Watt und Crr live zu rechnen.

Ersetzt das frühere 02-messkonzept (Herleitung und Fallstricke). Die abhakbare
Schrittliste steht in [05-prüfablauf](05-pruefablauf.md) — hier stehen die Gründe dazu.

Zahlenwerte gelten für den Stand vom 2026-10-30: Trommel Ø 933 mm, Hebelarm 40 mm,
Auslegung auf schnelle Straßenreifen. **Der Hebelarm ist seither 80 mm** (5-kg-Zelle,
Kalibrierarm 160 mm) und das Trommelmaß ist offen — verbindlich sind `config.h` und
[03-parameter](03-parameter.md) §1, offene Punkte in
[06-offene-fragen](06-offene-fragen.md) A14. Wo unten 40 mm steht, ist die Rechnung noch
nicht nachgezogen.

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
F_Zelle  = 1,0736 Nm / 0,080 m            = 13,42 N      (27 % vom Zellenendwert)
P_Reifen = 0,00552 · 416,9 · 8,0          = 18,41 W      ✓ stimmt mit der Quelle
```

Diese Rechnung rückwärts ist der beste Funktionstest der ganzen Kette: wer einen Reifen
mit publiziertem Wert fährt, muss am Ende dieselbe Wattzahl herausbekommen — bis auf die
Trommelkrümmung, siehe §9.

### 1.5 Drehzahl und Geschwindigkeit

```
n [1/min] = 60 · v / U          ω = 2π · v / U          v = U · n
```

Mit **U = 2,837 m** (Ø 900 mm blank gemessen am 2026-11-01, + 1,5 mm Tränenblech →
Ø 903 mm). **Vorläufig**, bis das Blech drauf ist und mit dem Maßband nachgemessen wurde:

| v [km/h] | n [1/min] | ω [rad/s] | Pulsperiode bei 8 Marken |
|---|---|---|---|
| 10 | 58,8 | 6,15 | 128 ms |
| 28,8 | 169,2 | 17,72 | 44 ms |
| 60 | **352,5** | **36,91** | 21 ms |

`MIN_PULSE_US` = 800 µs filtert sicher nur Störer. `MAX_DRUM_RPM` ist in `config.h` aus
`V_MAX_KMH` und `DRUM_CIRCUMFERENCE_M` **abgeleitet** und folgt dem Umfang automatisch.

> Die Momente in diesem Dokument sind noch mit r = 0,4665 m (Ø 933) gerechnet. Mit
> r = 0,4515 m sinken sie um **3,2 %**. Nachgezogen wird gemeinsam, sobald der Umfang
> endgültig gemessen ist.

### 1.6 Was die Pendelwiege misst — und was nicht

Nimm als System alles, was auf der Wiege sitzt. Im stationären Zustand ist die Summe der
**äußeren** Momente null, also `T_Arm = T_außen`. Alles Innere hebt sich paarweise auf:

| Bauteil | innerhalb der Wiege? | wird gemessen? |
|---|---|---|
| Rastmoment, Eisenverluste des Motors | ja | **nein** |
| Lagerreibung Rotor ↔ Stator | ja | **nein** |
| Trommellager (eigene Welle, außerhalb) | nein | **ja** → §3.2, Referenzrolle A11 |
| Kupplungsverluste | nein | ja → fallen in die Leerlaufkennlinie |
| Luftwiderstand der Trommel | nein | ja → Leerlaufkennlinie |
| Lager und Aerodynamik des Prüflaufrads | nein | ja → Laufrad-Auslaufversuch |
| Reifen am Latsch | nein | ja → **das Signal** |
| Reibung Rotor ↔ Stator (= das „Pendellager") | ja | **nein** — kürzt sich exakt heraus, [08-staender](08-staender.md) §7.2 |

Daraus folgt der ganze Rest des Verfahrens: zwei Korrekturkurven (Leerlauf, Laufrad), ein
Restfehler (Trommellager), und ein Pendellager, das so reibungsarm wie möglich sein muss.

Der kommerzielle Referenzprüfstand misst stattdessen die elektrische Leistungsaufnahme und
rechnet über eine Wirkungsgradkennlinie des Motors zurück
([Beschreibung](https://www.bicyclerollingresistance.com/the-test)). Diese Kennlinie ist
dort die größte Fehlerquelle; die Pendelwiege braucht sie nicht.

### 1.7 Rampe und Auslauf

Während einer Drehzahländerung misst die Zelle zusätzlich das Beschleunigungsmoment:

```
(14) M_beschl = J · dω/dt
```

Mit J = 2,07 kg·m² und der eingestellten Rampe von 0,5 km/h/s sind das **0,62 Nm** — fast
so viel wie das ganze Reifensignal eines guten Rennreifens (0,72 Nm). **Deshalb wird nur
auf Plateaus gemessen, nie während der Rampe.**

Umgekehrt nutzbar ist derselbe Zusammenhang im Auslauf:

```
(15) J · dω/dt = −M_verlust(ω)
```

Das liefert M_verlust **unabhängig von der Wägezelle** und ist damit der schärfste
verfügbare Test der ganzen Momentenmesskette — siehe §9.1.

### 1.8 Woher die Trommelaerodynamik kommt

Der Leerlauf besteht beim Direktantrieb nur aus dem Luftwiderstand der Trommel.
Gerechnet für 16,67 m/s Umfangsgeschwindigkeit, Ø 0,933 m, Breite 0,100 m,
ρ = 1,2 kg/m³, ν = 1,51·10⁻⁵ m²/s:

```
Mantel:    Re  = v·D/ν   = 1,03·10⁶ ,  Cf = 0,004 ,  A = π·D·b = 0,293 m²
           τ   = ½·ρ·Cf·v² = 0,67 Pa  →  F = 0,196 N  →  M = 0,091 Nm

Scheiben:  Re_ω = ω·R²/ν = 5,15·10⁵ ,  Cm = 0,146·Re_ω^−0,2 = 0,0105
           M    = Cm·½·ρ·ω²·R⁵ = 0,178 Nm

Summe ≈ 0,27 Nm ≈ 9,6 W bei 60 km/h, wachsend mit v²
```

**Gemessen am 2026-11-17** aus dem freien Auslauf (97 → 29 km/h in 140 s, acht
Stützstellen, M = J·dω/dt):

```
M_verlust = 0,472 Nm  +  3,178·10⁻⁴ · ω²          (ω in rad/s)

bei 60 km/h:  0,91 Nm gesamt,  davon 0,43 Nm mit ω²   (Rechnung oben: 0,27 Nm)
bei 100 km/h: 1,68 Nm gesamt,  davon 1,20 Nm mit ω²
```

Der ω²-Anteil ist **1,6-fach** so groß wie gerechnet — und die Rechnung nennt den Grund
selbst eine Zeile weiter: die Trommel hat **Tränenblech**, keine glatte Oberfläche. Für
eine Luftwiderstandsabschätzung ist ein Faktor 1,6 ein gutes Ergebnis.

Der konstante Anteil von 0,472 Nm ist das **Schleppmoment** (Lager und Motor), also
genau das `a` aus §8.9 — dort wurde es als unbekannt angesetzt.

> Was davon ins Messergebnis eingeht: **nur der ω²-Anteil.** Trommellager und
> Motorreibung liegen innerhalb der Pendelwiege und kürzen sich heraus. Die 0,43 Nm bei
> 60 km/h stehen aber gegen ein Nutzsignal von 1,32 Nm (Crr 0,007 bei 417 N) — das sind
> **33 %**, bei 100 km/h 91 %. Abziehbar, weil leerlaufend je Stufe messbar; aber zu
> messen, nicht zu rechnen. Konstanten in [03-parameter](03-parameter.md) §1.

Gilt für eine **glatte, geschlossene** Trommel in ruhender Luft. Offene Speichen, raue
Oberfläche oder Anbauten können ein Vielfaches ergeben. **Die erste Leerlaufkennlinie
(§8.4) ersetzt diese Rechnung** — sie ist nur die Erwartung, gegen die man das Messergebnis
auf Plausibilität prüft.

## 2 Welche Größe wann bestimmt wird

### 2.1 Einmalig — Anlagenkonstanten

Ändern sich nur bei Umbau. Gehören nach `firmware/src/config.h` **und**
[03-parameter](03-parameter.md).

| Größe | Symbol | Ermittlung | Zielgenauigkeit |
|---|---|---|---|
| Trommelumfang | `U` | Maßband **über die fertige Oberfläche**, 3 Messungen mitteln | ±1 mm (0,03 %) |
| Trommelradius | `r = U/2π` | gerechnet | — |
| Hebelarm Drehmomentwaage | `L` | Achsmitte Pendel → Krafteinleitung, Messschieber | ±0,1 mm (0,25 %) |
| Kalibrierradius | `L_cal` | zweite Bohrung, 160,0 mm | ±0,1 mm |
| Zellenkennwert | `N_PER_COUNT` | §5.1, mindestens 3 Gewichte | ±0,2 % |
| Hebel der Auflast | `a` | Pivotachse → Radachse, waagerecht | ±0,5 mm (0,25 %) |
| Marken pro Umdrehung | `MARKS_PER_REV` | Festlegung, 6 + Index | exakt |
| Trommelträgheit | `J_Trommel` | Auslaufversuch, §9.1 | ±5 % (nur für die Rampenkorrektur) |
| Leerlaufmodell | `A₀, B₀` | Baseline-Lauf, gefittet | siehe §3 |

### 2.2 Pro Laufrad — einmal je Laufrad, nicht je Reifen

| Größe | Ermittlung | Zielgenauigkeit |
|---|---|---|
| Trägheitsmoment `J_Rad` | Bifilarpendel, [08-staender](08-staender.md) §6.2 | ±1,5 % |
| Radverlustmodell `A₁, B₁` | Laufrad-Auslaufversuch | ±2 % |
| Laufradmasse `m_Rad` | Waage | ±20 g |
| Referenz-Reifenmasse `m_Reifen,ref` | Waage, gehört zu `J_Rad` | ±1 g |

### 2.2a Abrollradius aus dem Drehzahlverhältnis

`r_Rad` wird **nicht eingegeben, sondern gemessen**. Trommel und Laufrad rollen
aufeinander ab, also legen beide Oberflächen dieselbe Strecke zurück:

```
U_Rad = U_Trommel · n_Trommel / n_Rad        r_Rad = r_Trommel · n_Trommel / n_Rad
```

| Beitrag | Größe |
|---|---|
| Unsicherheit von `U_Trommel` | 0,035 % (1 mm Maßband) |
| Schlupf im Latsch, geschätzt | ≈ 0,01 % |
| Zählquantisierung über 30 s | vernachlässigbar |
| **gesamt** | **≈ 0,05 %** |

Zum Schlupf: übertragen wird nur das Eigenverlustmoment des Laufrads, `F_t = M_Rad/r =
0,55 N` gegen 417 N Auflast, also µ = 0,0013. Bei µ_max ≈ 0,8 und 5 bis 10 % Schlupf im
Maximum bleibt davon rund 0,01 %.

Der so bestimmte Wert ist der **belastete Abrollradius auf der gekrümmten Trommel** —
genau die Größe, die in die Umrechnung gehört. Ein Maßband um den unbelasteten Reifen
liefert 2 bis 4 % zu viel.

> Das Verhältnis ist zugleich eine **Schlupfüberwachung**: springt es, rutscht der Reifen.

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
| Reifentemperatur | IR-Sensor über I²C, mitgeloggt |
| Zeitstempel | für die zeitliche Interpolation der Baseline |

## 3 Fehlerbudget und Dynamik

Was aufzulösen ist, steht in dieser Tabelle. Crr steigt mit der Geschwindigkeit, aber
zwischen 14,4 und 36 km/h nur um **11,6 %**
([Messung](https://www.bicyclerollingresistance.com/specials/crr-speed-test), Mittel aus
6 Reifen):

| v [km/h] | 14,4 | 21,6 | 28,8 | 36,0 |
|---|---|---|---|---|
| Crr | 0,00346 | 0,00362 | 0,00378 | 0,00386 |

Die Kennlinie ist also ein flacher Effekt. Um sie über mehrere Stützstellen aufzulösen,
braucht es **1 bis 2 % je Messpunkt**.

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
| ~~Pendellager-Hysterese~~ — *entfällt, kein Pendellager mehr ([08-staender](08-staender.md) §7.2)* | 0 | 0 | 0 |
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

Das frühere Kriterium verwarf eine Reihe bei mehr als **0,10 N** Abweichung zwischen Vor-
und Nachlauf. Das war bei MTB-Reifen (F_roll bis 6,3 N) ein
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

### 3.5 Die Waage als Schwinger

Hebelarm, Zugglied und Wägezelle bilden mit der Masse des Stators ein Feder-Masse-System.
Steifigkeit aus Sicht der Drehachse:

```
k_rot = k_Zelle · L²  =  327 N/mm · (0,080 m)²  =  2092 Nm/rad   [TBD: k der 5-kg-Zelle]
```

Das Zugglied (Schaltzug, 1272 N/mm) trägt dazu nur 0,4 % bei und ist vernachlässigbar.
Die Masse ist seit dem Wegfall der Pendelwiege **nur noch der Stator**:

| J_Stator | f_n |
|---|---|
| 0,007 kg·m² (2 kg, Trägheitsradius 60 mm) | **43,5 Hz** |
| 0,012 kg·m² | 33,2 Hz |
| 0,020 kg·m² | 25,7 Hz |
| 0,026 kg·m² (4 kg, 80 mm) | **22,6 Hz** |

Zum Vergleich die Anregung bei 60 km/h (Trommel 5,87 U/s):

| Ordnung | Frequenz | Ursache |
|---|---|---|
| 1. | 5,9 Hz | Unwucht, Stoßstelle des Mantelblechs, Reifenunrundheit |
| 2. | 11,7 Hz | Rundlauffehler |
| 3. | 17,6 Hz | |
| 4. | 23,5 Hz | |
| 8. | 47,0 Hz | achtzählige Geometrie, falls vorhanden |

**Am leichten Ende liegt die Eigenfrequenz über allen Ordnungen, am schweren nicht:**
bei J = 0,026 kg·m² fällt f_n mit 22,6 Hz zwischen die 3. und die 4. Ordnung. Deshalb
wird f_n nach dem Bau **gemessen**, nicht gerechnet (A7) — und nichts Weiches kommt in
den Kraftpfad. Jedes Elastomer und jede Blattfeder in Reihe schiebt sie nach unten:
mit 58 N/mm Reihenfeder wären es nur noch 12,9 Hz und damit zwischen 2. und 3. Ordnung.
Das ist der Grund, warum das Zugglied **axial steif** sein muss und die Nachgiebigkeit
allein aus der Zelle kommen soll.

> **Der Mittelwert ist davon unberührt.** Die Messung ist momentengetrieben: der Arm ist
> frei beweglich, `F = T/L` gilt unabhängig davon, was in der Kette federt oder kriecht.
> Eine Resonanz vergrößert nur die Amplitude um den Mittelwert, und die fällt bei
> synchroner Mittelung heraus. Gefährlich ist sie, weil der Arm in die Anschläge läuft.

### 3.6 Sprungantwort: Faktor 2

Ein Momentensprung — Bremsen, Stromgrenze beim Anfahren, blockierendes Laufrad — baut
sich in rund einer Millisekunde auf. Ein ungedämpftes System antwortet darauf mit dem
**doppelten** statischen Wert. Daraus zwei Festlegungen:

- Der Anschlag muss das **Doppelte** des erwarteten Moments aushalten.
- Der Wirbelstromdämpfer gehört montiert, **bevor** zum ersten Mal gebremst wird.

Gedämpft wird **parallel**: Aluminiumscheibe am Hebelarm zwischen zwei Magneten. Ein
Elastomer im Kraftpfad dämpft zwar auch, senkt dabei aber f_n — es erkauft die Dämpfung
mit genau dem Problem, das es lösen soll.

### 3.7 Abtastung: hier ist noch ein Loch

Der HX711 läuft mit 80 SPS, die Nyquist-Grenze liegt also bei 40 Hz.

| | 10 km/h | 28,8 km/h | 60 km/h |
|---|---|---|---|
| Samples je Umdrehung | 84 | 29 | **14** |
| 8. Ordnung | 7,6 Hz | 21,8 Hz | **45,5 Hz** |

Die 8. Ordnung liegt bei 60 km/h **über** der Nyquist-Grenze und faltet sich auf
**34,5 Hz** zurück. Dort sitzt sie nicht mehr auf einer Ordnung der Drehung und wird von
der synchronen Mittelung nicht mehr sauber gelöscht. Zwei Maßnahmen:

1. **Keine achtzählige Geometrie an der Trommel.** Die acht Blechschrauben des Mantels
   nicht in einer Ebene auf einem Teilkreis setzen, sondern axial versetzt. Die acht
   Drehzahlmarken selbst sind unkritisch — Magnete erzeugen keine Kraft auf die Trommel,
   solange die Sensorhalter aus Alu oder Kunststoff sind (§4).
2. **Nicht Samples mitteln, sondern über die Zeit integrieren.** Bei 14 Samples je
   Umdrehung liegen die Abtastzeitpunkte nicht synchron zur Drehung; eine einfache
   Mittelung gewichtet dann ungleich. Richtig ist die Trapezintegration zwischen zwei
   Indexpulsen, geteilt durch die Dauer.

### 3.8 Warum synchron gemittelt und nicht tiefpassgefiltert wird

Ein Tiefpass hätte Einschwingzeit und Phasendrehung und würde die tiefen Ordnungen (5,7
und 11,4 Hz — genau die störenden) gar nicht erfassen. Die Mittelung über eine **ganze
Zahl von Umdrehungen** löscht dagegen *alle* Harmonischen der Drehung exakt, unabhängig
von Frequenz und Amplitude, und kostet nur die Indexmarke.

Nach Markennummer in **45°-Bins** sortiert zeigt dasselbe Rohsignal zusätzlich, *wo* auf
dem Umfang die Störung sitzt — Stoßstelle, Unwucht und Reifenunrundheit lassen sich so
auseinanderhalten.

## 4 Sensorik: was wo sitzt

| Sensor | Ort | Signal | Pin |
|---|---|---|---|
| **Trommelmarken** | **8 Magnete** auf der Stirnscheibe (die vorhandenen Striche), r so groß wie möglich, **Halter aus Alu oder Kunststoff** | Hall, Interrupt | GPIO4 |
| **Indexmarke** | 1 Magnet, andere Spur oder anderer Radius — **zwingend unipolar (TLE4905L)**, ein Latch bliebe hängen | Hall, Interrupt | GPIO5 |
| **Motor-Hall** | im Motor vorhanden, eine Phase herausführen | Interrupt | GPIO7 |
| **Wägezelle** | Hebelarm → **Schaltzug 1,2 mm, 80 mm** → Zelle, 12 N vorgespannt | HX711, 80 SPS | 15 / 16 |
| **Laufraddrehzahl** | **1 Magnet an den Speichen** (seit 2026-11-22 so verbaut; mehr brächten nur eine schnellere Aktualisierung, keine Genauigkeit), Hall am Ausfallende | Interrupt | GPIO6 |
| **Messzone der Waage** | Gabellichtschranke, Fahne mit Fenster auf r = 200 mm am Hebelarm | digital | **TBD, freier Pin** |
| Reifentemperatur | **IR berührungslos** auf die Lauffläche, kurz hinter dem Latsch | I²C | 8 / 9, kein eigener Pin |
| Umgebungstemperatur | am Rahmen, weg von Motor und Netzteil | I²C | **TBD** |
| Motor-PTC | im Motor | analog | GPIO1 (ADC1) |
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
3. Bekanntes Gewicht an die **Kalibrierbohrung** bei `L_cal` = 160,0 mm hängen.
   Moment `M_cal = m · g · L_cal`. Rohwert über 200 Samples mitteln.
4. Mit mindestens **drei** Massen über den Bereich wiederholen. An `L_cal` = 160 mm sieht
   die Zelle am Arm L = 80 mm die **doppelte** Kraft, also `F = m · g · 2`: 0,5 / 1,0 /
   2,0 kg ergeben 0,785 / 1,57 / 3,14 Nm und 20 / 40 / 80 % vom Endwert der 5-kg-Zelle.
   Der Auslegungsfall 2,51 Nm liegt damit innen. **Mehr als 2,4 kg nicht anhängen** — das
   ist der Endwert. (gerechnet)
5. Geradenfit `M = k · (raw − offset)`. **Linearitätsabweichung < 0,2 %** verlangen.
6. Hysterese prüfen: aufsteigend und absteigend belasten. Die Differenz bei gleicher Last
   ist die Hysterese der Pendellagerung — sie gehört ins Protokoll, nicht in den Fit.
7. `N_PER_COUNT = k / L` nach `config.h` und [03-parameter](03-parameter.md).

> Kalibriergewichte selbst wiegen. Eine Küchenwaage auf 1 g bei 2 kg ist 0,05 % — gut
> genug. Die Fallbeschleunigung mit 9,81 m/s² anzusetzen reicht; der Ortsunterschied in
> Deutschland liegt unter 0,1 %.

### 5.2 Auflast

Die Auflast wird **gewogen, nicht gerechnet**. Vorhandene Plattformwaage: 120 kg,
20 g Auflösung — bei 42,5 kg sind das 0,05 %, weit unter dem 1-%-Ziel. Laufrad- und
Reifenmasse stecken automatisch darin, eine zweite Wägezelle entfällt (A3).

**Einrichtung, einmalig:**

1. Schwinge **ohne Laufrad und ohne Laufgewicht** mit dem Gegengewicht auswuchten.
   Restkraft an der Radposition mit einer Federwaage prüfen: **< ±2 N**.
2. Laufrad wiegen (`m_Rad`).
3. `a` waagerecht ausmessen, ±0,5 mm.
4. Maßband auf den Holm kleben, Null **auf der Pivotachse**. Index am Laufgewicht
   ankörnen. Damit ist die Hebelwaage reproduzierbar einstellbar, auch ohne Wägung.

**Vor jeder Messreihe:**

1. Laufrad mit dem Exzenterhebel abheben.
2. Waage auf die Trommel legen, gegen Kippen sichern — sie steht auf dem Scheitel einer
   nur 100 mm breiten Trommel.
3. **Schieber um die Bauhöhe der Waage anheben.** Damit steht die Radachse relativ zum
   Schwingendrehpunkt wieder dort, wo sie beim Messen steht, und das Hebelverhältnis
   bleibt unverändert. Der Verstellweg von 195 mm
   ([08-staender](08-staender.md) §2.1) deckt jede übliche Waagenhöhe ab.
4. Rad absenken, Laufgewicht auf den Zielwert justieren, Position am Maßband ablesen.
5. Rad abheben, Waage heraus, Schieber zurück auf das Maß der Radgröße, Rad absenken.

Ohne Schritt 3 kippt die Schwinge um die Bauhöhe; bei 40 mm und 400 mm Schwingenlänge
wären das rund 0,5 %.

Die abgelesene Position des Laufgewichts kommt in die CSV. Weicht sie beim nächsten Mal
ab, hat sich etwas an Laufrad, Reifen oder Mechanik geändert.

### 5.3 Null, zweimal

| | wann | erfasst |
|---|---|---|
| **statische Tara** | Trommel steht, vor jeder Reihe | ADC-Offset, Eigengewicht der Wiege, Drift |
| **Leerlaufkennlinie** | Trommel dreht, Rad abgehoben | alles, was außerhalb der Wiege Moment zieht |

Beides ist nötig. Die statische Tara allein lässt die Aerodynamik stehen, die
Leerlaufkennlinie allein lässt den ADC-Offset stehen.

## 6 Die Rechenkette im Betrieb

Reihenfolge der Auswertung. Die Gleichungen stehen in §1, die Konstanten in
[03-parameter](03-parameter.md), Eingaben und Anzeige in
[10-firmware](10-firmware.md).

```
je Sample:   F_Z = (raw − offset) · k

je Umdrehung, index-getriggert, über AVG_REVS_PER_POINT gemittelt:
             n   = Marken / (k_Marken · Δt)
             ω   = 2π·n              v = U·n
             M   = F_Z,mittel · L
             M_leer    = A₀ + B₀·ω²
             ω_Rad     = ω · r / r_Rad
             M_Rad     = (A₁ + B₁·ω_Rad²) · r / r_Rad
             M_Reifen  = M − M_leer − M_Rad
             F_roll    = M_Reifen / r
             Crr       = F_roll / N
             P_Reifen  = M_Reifen · ω        (= Crr · N · v)
```

**Die Wattzahl, die zählt, ist `P_Reifen`**, nicht `P_ges = M·ω`. Die publizierten Werte
sind Reifenleistungen bei 28,8 km/h und 42,5 kg; `P_ges` enthält zusätzlich Trommel und
Laufrad und läge bei 28,8 km/h um rund 2 W zu hoch (1,05 W Trommel, 0,99 W Laufrad).

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

## 8 Prüfablauf

Ein vollständiger Durchlauf dauert rund zwei Stunden, davon 30 Minuten Warmlauf.
Abkürzungen rächen sich: die Unterschiede, die gemessen werden sollen, sind kleiner als
die Effekte, die ein ausgelassener Schritt verursacht. Zur Einordnung — die Kennlinie
über der Geschwindigkeit ändert sich zwischen 14 und 36 km/h um ganze 11,6 %, und zwei
gute Rennreifen trennen 0,2 N Umfangskraft.

**Die Schrittliste selbst steht in [05-prüfablauf](05-pruefablauf.md)** — dort nackt und
abhakbar. Hier steht nur, warum die Reihenfolge so ist und was die Zahlen bedeuten.

| Schritt (05) | warum an dieser Stelle |
|---|---|
| B0 Sichtprüfung, Not-Aus | [07-sicherheit](07-sicherheit.md) |
| B1 Laufrad einsetzen, Drehpunkt auf Radachshöhe | bestimmt die Geometrie aller Folgeschritte |
| B2 Reifen wiegen, Druck setzen, `J_Rad` korrigieren | vor der Tara, weil es die Wiege nicht berührt |
| B3 Statische Tara `t` | nach jedem mechanischen Eingriff |
| B4 Baseline Vorlauf `b`, Rad abgehoben | kalt → unterer Eckwert der Drift |
| B5 Warmlauf | Crr sinkt mit der Reifentemperatur |
| B6 Messlauf `m` | thermisch eingeschwungener Zustand |
| B7 Baseline Nachlauf `b` | warm → oberer Eckwert, Differenz ist das Abbruchkriterium |
| B8 Auslaufen lassen, nicht bremsen | §8.9 |
| B9 Auswertung `analysis/crr.py` | interpoliert die Baseline zeitlich zwischen B4 und B7 |

### 8.0 Vor dem Einschalten

Checkliste in [05-prüfablauf](05-pruefablauf.md) §B0, Sicherheitsteil in
[07-sicherheit](07-sicherheit.md) §7.

> Eine schleifende Bremse — falls eine nachgerüstet wird — erzeugt ein
> temperaturabhängiges Schleppmoment und lässt
> Vor- und Nachlauf auseinanderlaufen. Das macht die ganze Reihe wertlos, und man sieht es
> erst am Abbruchkriterium in §8.8.

### 8.1 Statische Tara

Trommel steht, `t` auf der Konsole. Die Firmware mittelt `TARE_SAMPLES` = 200 Samples
(≈ 2,5 s bei 80 SPS) und setzt den Offset. Erfasst ADC-Offset, Eigengewicht des Wiegeteils
am Hebelarm und die Drift seit dem letzten Abgleich.

**Wiederholen, sobald sich am Aufbau irgendetwas geändert hat** — auch nach dem Umstellen
der Auslegerhöhe oder dem Verlegen eines Kabels.

### 8.2 Laufrad einsetzen und Geometrie einstellen

Checkliste in [05-prüfablauf](05-pruefablauf.md) §B1. Die drei Maße dahinter:

- **mittig**: bei 100 mm Trommelbreite und 57 mm Reifen bleiben nur 21 mm je Seite.
- **Gabelholm waagerecht** (Wasserwaage): dann liegt der Drehpunkt auf Radachshöhe und
  die Umfangskraft hat keinen Hebel, der Korrekturterm aus §1.3 entfällt.
- **x = 0 ± 2 mm**: Radachse senkrecht über der Trommelachse, sonst wandert die
  Latschnormale.

### 8.3 Reifen vorbereiten

Checkliste in [05-prüfablauf](05-pruefablauf.md) §B2. Vor der Tara, weil kein Schritt
davon die Wiege berührt. Die Standardauflast `STANDARD_LOAD_N` = **416,9 N
(42,5 kg)** ist die Last der publizierten Crr-Datenbank — nur so sind die Werte vergleichbar.

### 8.4 Baseline Vorlauf

**Laufrad abgehoben** (Exzenterhebel), `b` auf der Konsole. Die Firmware fährt alle
Plateaus aus `PLATEAUS_KMH`, wartet je `SETTLE_MS` und mittelt über
`AVG_REVS_PER_POINT` **ganze Umdrehungen**, getriggert auf die Indexmarke.

Ergebnis: `M_leer(ω)`, daraus der Fit `A₀ + B₀·ω²`. Nicht überspringen — ohne diese Kurve
sind die Absolutwerte wertlos.

### 8.5 Warmlauf

**30 Minuten bei 28,8 km/h.** Keine Sicherheitsmarge, sondern die Zeit, die der
kommerzielle Referenzprüfstand ansetzt
([Quelle](https://www.bicyclerollingresistance.com/the-test)). Der misst anschließend
dreimal pro Druckstufe bei einer auf ±0,5 °C gehaltenen Raumtemperatur — diese
Temperaturkonstanz ist in einer Werkstatt nicht zu erreichen, und genau deshalb ist das
A/B/A-Protokoll in §8.10 kein Extra, sondern tragend.

### 8.6 Messlauf

`m` auf der Konsole, **dieselben Plateaus** wie im Baseline-Lauf — die Auswertung
subtrahiert punktweise, die Stützstellen müssen übereinstimmen.

Während des Laufs nicht an den Aufbau fassen und **Türen und Lüftung nicht verändern** —
die Trommelaerodynamik reagiert auf Luftzug, und sie steckt über die Leerlaufkennlinie
direkt im Ergebnis. Druck- und Lastreihen brauchen je Stufe 5 min Angleichzeit (§7.1).

### 8.7 Baseline Nachlauf

Laufrad abheben, `b`. Zweite Baseline direkt im Anschluss, im warmen Zustand.

### 8.8 Abbruchkriterium

> Weicht der Nachlauf an einer Stützstelle um mehr als **0,02 N** (bezogen auf F_roll) vom
> Vorlauf ab, ist die Messreihe zu verwerfen.

0,02 N sind 1,3 % vom Rennreifensignal und 9,6 % vom Auflösungsziel (§3.3). Der frühere
Wert von 0,10 N stammt aus der MTB-Auslegung und wäre hier 6,5 % des Signals.
Übliche Ursachen: Temperaturdrift der Lager, schleifende Bremse, verändertes Kabel,
Luftzug.

### 8.9 Auslaufen lassen

`s`, dann frei auslaufen lassen. **Nicht von Hand bremsen.** Bei 60 km/h sind rund 1,3 kJ
gespeichert. Die Auslaufdauer hängt fast ganz am Schleppmoment des Motors:

Mit `J·dω/dt = −(a + b·ω²)`, a = Schleppmoment, b = 0,27/35,73² = 2,12·10⁻⁴:

```
t = (J/√(ab)) · arctan(ω₀·√(b/a))
```

| Schleppmoment a | Auslauf von 60 km/h |
|---|---|
| nur Trommellager (0,002 Nm) | 80 min |
| 0,05 Nm | 13 min |
| **Nabenmotor, typisches Rastmoment (0,15 Nm)** | **5,7 min** |
| stark rastender Stator (0,30 Nm) | 3,3 min |

Einplanen — es gibt keine Bremse ([07-sicherheit](07-sicherheit.md) §3). **Erst
weiterarbeiten, wenn die Trommel sichtbar steht.**

### 8.10 A/B/A

Für jeden Vergleich: Referenzreifen — Prüfreifen — Referenzreifen, in **einer** Sitzung,
bei derselben Auflast und demselben Druck. Weicht die zweite Referenzmessung von der
ersten ab, ist der Unterschied dazwischen nicht interpretierbar.

Mindestens drei Wiederholungen je Reifen, Streuung mit angeben.

**Plausibilitätsanker:** einmal einen Reifen mit publiziertem Wert fahren, bei 42,5 kg und
28,8 km/h. Die Absolutwerte werden nicht exakt übereinstimmen (§10), aber die
Größenordnung muss passen und die Rangfolge mehrerer Reifen muss stimmen. Tut sie das
nicht, stimmt etwas Grundsätzliches nicht.

### 8.11 Auswertung

```sh
python3 analysis/crr.py data/<datei>.csv
```

Trennt BASELINE- und MEASURE-Punkte, interpoliert die Leerlaufkennlinie **zeitlich**
zwischen Vor- und Nachlauf, subtrahiert punktweise, zieht das Radverlustmodell ab und gibt
Crr(v) und P_Reifen(v) aus.

## 9 Sonderläufe

Nicht Teil jeder Messung, aber Voraussetzung dafür, dass die Zahlen etwas bedeuten.

### 9.1 Auslaufversuch der Trommel — liefert J und prüft die Messkette

Einmalig und nach jeder Änderung an der Trommel:

1. Laufrad abgehoben, auf 60 km/h beschleunigen.
2. Antrieb freilaufend trennen, Drehzahlverlauf bis zum Stillstand aufzeichnen.
3. Aus `J · dω/dt = −M_verlust(ω)` folgt M_verlust **unabhängig von der Wägezelle**.
4. Mit der Leerlaufkennlinie aus §8.4 vergleichen. **Beide Wege müssen übereinstimmen** —
   das ist der schärfste verfügbare Test des Gesamtsystems.

J selbst folgt aus demselben Versuch mit einer bekannten Zusatzmasse, oder aus der
Geometrie (2,07 kg·m² gerechnet).

### 9.2 Bifilarpendel — liefert J_Rad

Je Laufrad einmal. Rad waagerecht an zwei gleich langen Fäden aufhängen, an zwei
gegenüberliegenden Punkten der Felge:

```
(16) J_Rad = m · g · b² · T² / (4 π² · L)
```

b = halber Abstand der Aufhängepunkte, L = Fadenlänge, T = Schwingungsdauer über
20 Schwingungen. Ausschlag < 10°, Reifen montiert und aufgepumpt. Erreichbar **1,2–1,4 %**
([08-staender](08-staender.md) §6.2).

### 9.3 Laufrad-Auslaufversuch — liefert A₁, B₁

Je Laufrad einmal:

1. Trommel mit aufliegendem Laufrad auf 60 km/h bringen.
2. Laufrad **im Lauf** abheben (Exzenterhebel).
3. Drehzahl des Laufrads über den Hallsensor aufzeichnen, 60 → 10 km/h dauert 4,5 min.
4. `M_Rad(ω) = −J_Rad · dω/dt` als Modell `A₁ + B₁·ω²` über den ganzen Auslauf fitten —
   nicht punktweise differenzieren.
5. `w<A₁> <B₁>` eingeben.

Ohne diesen Lauf steckt der Laufradverlust ungetrennt im Ergebnis: 0,25 Nm entsprechen
einem scheinbaren Crr von 0,00129, also der Hälfte des besten Rennreifens.

### 9.4 Referenzlauf mit starrer Rolle — quantifiziert den Lageranteil

Offen (A11). Eine Stahl- oder PA-Rolle gleicher Baugröße statt des Reifens, gleiche
Auflast: ihre Hysterese ist vernachlässigbar, gemessen wird praktisch nur der
lastabhängige Lageranteil von 4,7 mNm. Damit wird aus dem Restfehler eine Korrektur.

## 10 Was dieses Verfahren nicht hergibt

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
