# Ständer, Trommellagerung und Auflast-Mechanik

Löst die offenen Punkte A3 (Art der Auflast) und A4 (Trommelanbindung) konstruktiv auf und
legt die Maßkette fest. Alles hier ist **Entwurf zum Bauen**, nicht gemessen: jede Zahl
trägt ihre Herkunft. Maße in mm, Kräfte in N.

> **Trommeldurchmesser ist eine Messgröße, keine Festlegung.** Der Entwurf rechnet mit
> **D = 930 mm (geschätzt, 2026-10-30)** statt der früheren 954,9 mm. Daraus:
> U = 2,922 m, r = 465 mm. Der Wert ist mit dem Maßband um die gebaute Trommel zu messen
> (drei Messungen mitteln, [03-parameter](03-parameter.md) §1) — **erst dann** gehen U und
> r in `config.h`. Welche Maße daran hängen und welche nicht, steht in §2.

## 1 Topologie

```
          Prüflaufrad
               │ N = 417 N
        ╔══════╧══════╗   Trommel, an beiden Stirnscheiben aufgeklemmt
        ║   Trommel   ║
  ──┬───╫──┬───────┬──╫───┬──────────  Welle mitdrehend, abgesetzt Ø15/Ø30
    │   ╚══╪═══════╪══╝   │
 6902      Klemmnaben    6902        Ø15 Zapfen (M = 0), Ø30 Mitte (M = 21,5 Nm)
  y=−170   y=±100       y=+170
                                  └─── Flansch auf dem Wellenende
                                        │  + Kupplung (radial weich)
                                   Nabenmotor, Stator in der Wiege
                                        │
                                   Pendellager (2 × 6902)
                                        │
                                   Hebelarm L = 70 mm → Wägezelle
```

Gegenüber [04-hardware](04-hardware.md) §2 (Trommel direkt auf dem Motorrotor) ist das eine
**andere Anordnung**, und der Unterschied ist messtechnisch relevant:

| | Trommel auf dem Rotor (bisher) | Trommel auf eigener Welle (hier) |
|---|---|---|
| Trommellager | innerhalb der Wiege → fallen heraus | **außerhalb → werden mitgemessen** (2 × 6902, Bohrung 15) |
| Last auf den Pendellagern | Trommel + Motor + Auflast ≈ 650 N | nur Motor + Wiege ≈ 60 N |
| Reibmoment der Pendellager | 9,8 mNm (1,3 % vom Rennreifensignal) | 1,2 mNm (0,16 %) |
| lastabhängiger Anteil im Signal | 0 | **4,7 mNm (0,63 %)**, siehe §4.6 |
| Trommelanbindung | Adapterplatte auf den Rotor, symmetrisch | Flansch auf dem Wellenende |
| Baubarkeit | Rotormaße nötig, Motor noch nicht da | Trommel baubar, ohne den Motor zu kennen |

Die eine Variante kostet 1,3 % als **Hysterese im Pendellager** (nicht korrigierbar), die
andere 0,63 % als **lastabhängige Lagerreibung** (glatt, reproduzierbar, über einen
Referenzlauf mit starrer Rolle messbar, §4.6). Deshalb hier die zweite. Die Entscheidung
steht als A10 in [06-offene-fragen](06-offene-fragen.md).

**Radial weiche Kupplung ist Bedingung.** Eine starre Flanschverbindung macht die
Motorlager zum zweiten Trommellager; dann trägt die Wiege wieder die halbe Auflast und der
Vorteil ist weg. Die Kupplung überträgt nur Moment.

## 2 Koordinaten und Maßkette

Ursprung auf dem Boden unter der Trommelachse. **x** nach vorn (Bedienseite),
**y** entlang der Trommelachse, **z** nach oben. Der Seitenrahmen ist ein **Dreieck aus
senkrechtem Mast und Diagonale**: die Trommel sitzt an der Diagonale, der Schwingen-
drehpunkt am Mast. Der Versatz `a` zwischen beiden entsteht dadurch von selbst.

### 2.1 Vom Trommeldurchmesser unabhängig

Diese Maße stehen fest, egal ob die Trommel 930 oder 955 mm misst — der Rahmen ist auf die
**Trommelachse** bezogen, nicht auf die Oberfläche:

| Größe | Wert | Herkunft |
|---|---|---|
| Trommelachse über Boden `H` | **550** | gewählt; Minimum r + 40 (Profil) + 30 (Luft) = 535 bei D = 930 |
| Mast (senkrecht), Achse | x = −240 | Hinterkante des Dreiecks |
| Hebel `a` = Pivot → Trommelachse | **200** | daraus die Hebelwaage, §5.3 |
| Diagonale: Fußpunkt | x = +232, z = 60 | 472 mm vor dem Mastfuß |
| Diagonale: Kopfpunkt | x = −200, z = 1180 | an der Mastvorderseite |
| Diagonale: Länge / Neigung | **1200 / 21,1°** | gegen die Senkrechte |
| Trommelwelle an der Diagonale | Mittellinie (37,3 / 564,4) | Lagerbock 40 mm nach hinten versetzt → Welle auf (0 / 550) |
| Mastlänge | **1160** | z = 40 … 1200 |
| Schieber (Höhenverstellung) | **550** | Verstellweg 195, Überlappung 317…512 |
| Bodenholme (x), 2 × | **1000** | y = ±230, x = −400 … +600 |
| Querträger (y) mit Lenkrollen, 2 × | **1000** | x = −360 und +560, y = −500 … +500 |
| Standfläche | 1000 × 1000 | §6.3 |
| Gabelholme | y = ±100, Länge 950 | auf der Pendelachse geklemmt, §7.1 |

Der Lagerbock setzt die Wellenmitte **20 mm neben die Profilfläche**, also 40 mm neben die
Profilmittellinie — mit diesem Maß ist die Diagonale gerechnet. Weicht dein Lagerbock ab,
verschiebt sich der Fußpunkt der Diagonale um `Δx_Fuß ≈ 2,2 · Δ(Versatz)`.

### 2.2 Vom Trommeldurchmesser abhängig

| Größe | bei D = 930 | bei D = 955 | Bemerkung |
|---|---|---|---|
| Trommelradius r | 465,0 | 477,5 | |
| Trommelunterkante | 85,0 | 72,6 | Luft über dem Bodenholm |
| **Latschebene** (= H + r) | **1015,0** | 1027,5 | |
| Radachse (= Latsch + r_Rad) | 1195 … 1390 | 1207,5 … 1402,5 | r_Rad 180 … 375 |
| Pivot (= Radachse + 30) | 1225 … 1420 | 1237,5 … 1432,5 | §7.1 |

**Der Unterschied von 12,4 mm verschwindet im Verstellweg des Schiebers** (195 mm). Es
ändert sich kein einziges Zuschnittmaß — der Pivot wird einfach 12 mm tiefer geklemmt.
Deshalb kann die Trommel gebaut und *danach* gemessen werden.

## 3 Skizze

Seitenansicht, nicht maßstäblich, Maße in mm (Latschebene für D = 930):

```
                        ┌─ Pivot 1225…1420 (Radachse + 30)
   z=1420 ──────╥───────┴─╮         Laufrad (622-57, r = 368)
                ║Schieber │  ╭────╮      Laufgewicht 2 × 5 kg,
   z=1225 ──────╫─550─────┼─(      )─────┬──── außen an den Holmen
                ║         └──╰────╯      │
   z=1200 ══════╩═══╗ Mastkopf           │  Gabelholm 950, y = ±100
                ║   ╲                    │
                ║    ╲ Diagonale 1200    │
                ║     ╲  21,1°           │
   z=1015 ·······║······╲··········· Latschebene ·······│·······
                ║       ╲ Lagerbock      │
   z=564 ───────║────────╲── Mittellinie ┼──── Welle 40 mm dahinter
                ║ Mast    ╲ ╭─────────╮  │
   z=550 ───────║──1160────╲( Trommel )──┼──── Trommelachse (0 / 550)
                ║           ╲╰─────────╯
                ║            ╲
   z=85  ───────║─────────────╲─────────────── Trommel unten
   z=40  ═══════╩══════════════╩══════════════ Bodenholm 1000
             x=−400   x=−240   x=+232    x=+600
             Rolle     Mast    Diagonale  Rolle
```

Vorderansicht (Blick in +x), nicht maßstäblich:

```
     y=−230     −170  −100     0    +100  +170  +230      +370    +470
        │         │     │       │      │     │      │         │       │
     ╭──┴──╮   ╭──┴──╮  │       │      │  ╭──┴──╮          ╭──┴──╮
     │Mast │   │6902 │╔═╪═══════╪══════╪═╗│6902 │          │Motor│
     │     │   │ ◘   │║ │ Trommel 250   │ ║│  ◘  │   ╭──╮   │ ███ │──┐
z=550├─────┼───┤═════╪╫═╪═══════════════╪═╫╪═════╪═══╡KK╞═══╡ ███ │  │ Ø15
     │     │   │     │║ └─Klemmnabe   Klemmnabe  │   ╰──╯   │ ███ │──┘ 2×6902
     │     │   ╰─────╯╚═══════════════════╝╰─────╯ Kupplung ╰──┬──╯ Abst. 100
     │     │      ◄─ Ø15 ─►◄── Ø30 ──►◄─ Ø15 ─►                │ Hebelarm 70 ↓
     │     │   ◄────── Lagerabstand 340 ──────►              ══╩══ Zelle 5 kg
     ╰──┬──╯                                                   ╰──┬──╯
 z=40 ══╧══════════════════════════════════════════════════════╧══ Querträger 1000
```

Die Gabelholme liegen bei y = ±100 und laufen **über** die Trommel hinweg — bei der
kleinsten Radgröße mit 180 mm Luft, bei der größten mit 375 mm. Der Pendelbock hängt an
einem Querarm vom rechten Rahmen aus (§6.4), nicht an einem eigenen Pfosten: Pendelachse
und Trommelachse müssen koaxial sein, also gehören sie an dasselbe Bauteil.

## 4 Trommelwelle und Lager

### 4.1 Massen und Lastfall

| Anteil | Masse | Herkunft |
|---|---|---|
| Trommelkern ohne Beplankung | 16,0 kg | gewogen/geschätzt 2026-10-30 |
| Mantelblech 1,5 mm Alu-Tränenblech | 1,33 kg | ber: π·0,93·0,10·0,0015·2700, +12 % fürs Muster |
| Schrauben | ≈ 0,25 kg | geschätzt |
| **Trommel gesamt** | **≈ 17,6 kg** | |

```
F = 417 N (Auflast) + 172 N (Trommel) = 589 N
```

### 4.2 Welle Ø15 durchgehend reicht

Mit 100 mm Trommelbreite rücken die Lager auf **180 mm** zusammen, die Klemmnaben an den
Stirnscheiben auf **80 mm**. Die Last kommt damit an zwei Punkten herein, die nur 50 mm
vom jeweiligen Lager entfernt sind:

```
M = F/2 · (180 − 80)/2 = 294,5 N · 0,050 m = 14,7 Nm
σ = M / (π·15³/32) = 44,5 MPa
```

Zulässig an der Klemmnabenkante (β_k = 1,5, umlaufende Biegung, C45, S = 2): **75 MPa**.
Sicherheit 1,7 — **die abgesetzte Welle Ø15/Ø30 entfällt**. Sie war nur nötig, solange die
Trommel 250 mm breit war und die Lager 340 mm auseinander standen (21,5 Nm).

| | Lager je | gegen |
|---|---|---|
| Radiallast | 295 N | C₀ = 2,19 kN (6902) |
| L10 bei 342 1/min | 2558 Mio. Umdrehungen | ≈ 125 000 h |

→ **Welle Ø15 h6, C45 blank, ca. 350 mm lang**, 2 × 6902-2RS, Lagerabstand 180,
Klemmnaben Ø15 an beiden Stirnscheiben im Abstand 80.

### 4.3 Das Tränenblech ist ein messtechnischer Gewinn

Der Referenzprüfstand der publizierten Crr-Datenbank fährt auf **Riffelblech**, nicht auf
glattem Holz. In [06-offene-fragen](06-offene-fragen.md) C1 steht die Oberfläche deshalb
als systematische Abweichung nach unten. Mit dem Tränenblech fällt dieser Teil von C1
weitgehend weg; übrig bleibt die Krümmungsdifferenz (unsere 0,93 m gegen 0,77 m dort).

### 4.4 Was die Beplankung am Umfang ändert

| | D | U | n bei 60 km/h |
|---|---|---|---|
| Kern allein | 930,0 | 2,9217 m | 342,3 1/min |
| + 2 × 1,5 mm Blech | 933,0 | 2,9311 m | 341,2 1/min |
| + Muster ≈ 1 mm | 935,0 | 2,9374 m | 340,4 1/min |

**U wird über die fertige Oberfläche gemessen**, nicht aus dem Kerndurchmesser gerechnet.
Das Maßband liegt dabei auf den Tränen auf; der Reifen sinkt ein Stück ins Muster ein, der
wirksame Radius liegt also minimal darunter. Diese Unsicherheit ist mit ≈ 0,1 % klein und
fällt beim A/B-Vergleich heraus — bei Absolutwerten gehört sie ins Fehlerbudget.

> `MAX_DRUM_RPM` muss mit: bei U = 2,93 m entsprechen 60 km/h **341 1/min** statt der 333
> aus [03-parameter](03-parameter.md). Das ist eine Sicherheitsgrenze und gehört nach
> [07-sicherheit](07-sicherheit.md) §4 nachgezogen — mit 333 bliebe der Stand sonst bei
> 58,4 km/h stehen.

### 4.5 Schwungmasse

```
J = ½·16·0,465²  (Kern als Scheibe)  +  1,33·0,465²  (Mantel als Ring)
  = 1,73 + 0,34 = 2,07 kg·m²        (geschätzt — Auslaufversuch entscheidet)
E = ½·J·ω² = 1330 J bei 60 km/h (ω = 35,8 rad/s)
```

Die 1,4 kJ aus [07-sicherheit](07-sicherheit.md) §1 bleiben damit die richtige
Größenordnung. Das Rampenmoment bei 0,5 km/h/s beträgt 0,62 Nm — weiterhin fast so groß
wie das Reifensignal, es bleibt also bei Messung auf Plateaus.

### 4.6 Das Blech trägt sich selbst — die Schrauben halten nur die Lage

```
Umfangsspannung im Blech = ρ·v² = 2700 · 16,67² = 0,75 MPa
```

Gegen R_p0,2 ≈ 100 MPa ist das nichts: ein umlaufendes Blech zieht sich bei Drehzahl von
selbst stramm, der nach außen drückende Fliehdruck beträgt nur 2,4 kPa. Zu tragen hat die
**Stoßstelle**: dort müssen 113 N Umfangskraft übergeben werden (σ·t·b). Zwei Punkte:

1. **Schrauben versenkt und außerhalb der Latschbreite.** Ein überstehender Kopf schlägt
   einmal pro Umdrehung in den Reifen. Bei 100 mm Trommelbreite und maximal 57 mm
   Reifenbreite (±28,5) liegt eine Schraubenreihe bei **y = ±40** mit 11,5 mm Abstand
   zum breitesten Reifen — und trotzdem senkkopfversenkt.
2. **Wuchten.** Eine Überlappung von 30 × 100 × 1,5 mm sind 12 g auf r = 0,465:

| Restunwucht | Fliehkraft bei 341 1/min | Anteil an der statischen Last |
|---|---|---|
| 12 g (ungewuchtet, Stoß) | 7,2 N | 1,2 % |
| 2 g (gewuchtet) | 1,2 N | 0,2 % |

Die Stoßstelle erzeugt außerdem eine Störung **einmal pro Umdrehung**. Das ist genau der
Grund für die indexgetriggerte, umdrehungssynchrone Mittelung (M2 in
[01-requirements](01-requirements.md)) — und winkelaufgelöst ausgewertet wird daraus eine
Diagnose statt eines Fehlers ([09-messtheorie](09-messtheorie.md) §3.8).

## 5 Momentenbudget und Hebelarm der Drehmomentwaage

### 5.1 Neuer Auslegungsbereich: nur schnelle Straßenreifen

Nicht mehr MTB. Der langsamste vorgesehene Reifen ist der **Continental Contact Urban**:
18,4 W bei 4 bar, 40-622, 42,5 kg, 29 km/h
([Quelle](https://www.bicyclerollingresistance.com/tour-reviews/continental-contact-urban),
abgerufen 2026-10-30) — daraus `Crr = 18,4 / (8,0 · 416,9) = 0,00552`.

| Reifen | Crr | M_Reifen bei r = 0,4665 | + Leerlauf + Laufrad |
|---|---|---|---|
| Vittoria Corsa Speed (bester) | 0,0025 | 0,486 Nm | 1,00 Nm |
| guter Rennreifen | 0,0037 | 0,720 Nm | 1,24 Nm |
| Continental Grand Prix Urban (16,9 W) | 0,0051 | 0,985 Nm | 1,50 Nm |
| **Continental Contact Urban (18,4 W)** | **0,0055** | **1,074 Nm** | **1,59 Nm** |
| **neue Auslegungsgrenze** | **0,0070** | 1,361 Nm | **1,88 Nm** |
| *alte Auslegungsgrenze (MTB)* | *0,0150* | *2,917 Nm* | *3,43 Nm* |

Die Auslegungsgrenze halbiert sich damit von 0,015 auf **0,007** — 27 % Reserve über dem
Contact Urban.

### 5.2 Folge: der Hebelarm wird kürzer, nicht länger

Mit L = 0,070 m läge der ungünstigste Fall samt Rampe (2,51 Nm) bei **37 %** des
Zellenendwerts. Zwei Drittel des Messbereichs blieben ungenutzt, und die auf den Endwert
bezogenen Zellenfehler (Kriechen, Nichtwiederholbarkeit, typ. 0,02 % FS) gingen voll mit
ein.

Prozentangaben beziehen sich auf den Zellenendwert, und der ist bei der verbauten
**5-kg-Zelle 49,03 N** — nicht 98,07 N, wie hier bis zum 2026-11-16 stand.

| | L = 0,040 m | **L = 0,080 m** |
|---|---|---|
| Auslegungsfall stationär (1,88 Nm) | 47,0 N / **96 %** | **23,5 N / 48 %** |
| mit Rampe (2,51 Nm) | 62,7 N / **128 %** | **31,4 N / 64 %** |
| ΔCrr = 0,0005 | 2,43 N | **1,22 N** |
| Zellenfehler 0,02 % FS, als Moment | 0,0004 Nm | **0,0008 Nm** = 0,11 % vom Rennreifensignal |

→ **`TORQUE_ARM_M` = 0,080 m.** Mit 0,040 m würde die Zelle schon beim Anfahren mit
128 % ihres Endwerts belastet — im Normalbetrieb, nicht im Fehlerfall. Die Bohrungen am
Hebelarm (B4) liegen bei **80,0 mm** (Zelle) und **160,0 mm** (Kalibrierung), beide auf
±0,1 mm auszumessen; bei 80 mm sind das 0,13 %, also die Hälfte des früheren Budgets.
`MAX_FORCE_N` = **45 N** als Sättigungsschutz (entspricht 3,6 Nm, unverändert).

> Die Ausnutzungen 48 % / 64 % / 92 % sind dieselben wie in der früheren Rechnung mit
> einer 10-kg-Zelle bei 0,040 m: **halbe Zelle, doppelter Arm.** Auch alle Spannungen
> am Zelleneingang ([04-hardware](04-hardware.md) §5.3) bleiben unverändert, weil sie
> am Verhältnis hängen und nicht am Absolutwert. Was sich halbiert, sind die Kräfte.

### 5.3 Hochlaufzeit

Stationär werden im Auslegungsfall 1,88 Nm = 67 W gebraucht; der Motor liefert 6,96 Nm.
Hochlauf auf 60 km/h mit Contact Urban am Rad, J = 2,38 kg·m²:

| Antriebsmoment | Dauer |
|---|---|
| 2,0 Nm | 103 s |
| 3,0 Nm | 46 s |
| **6,96 Nm (Dauermoment)** | **15 s** |

### 5.4 Wo das Problem jetzt wirklich liegt

Je schneller die Reifen, desto größer der Anteil, der **nicht** vom Reifen kommt:

| Reifen | Trommelaerodynamik | Laufrad (Lager + Speichen) |
|---|---|---|
| Vittoria Corsa Speed | 55 % | 51 % des Reifensignals |
| guter Rennreifen | 37 % | 35 % |
| Contact Urban | 25 % | 23 % |

Die Aerodynamik fällt über die Leerlaufkennlinie heraus. **Das Laufrad nicht** — beim
Abheben steht es still, seine Lager- und Speichenverluste stecken also unkorrigiert im
Ergebnis:

```
0,25 Nm (geschätzt) / (416,9 N · 0,4665 m) = scheinbares Crr von 0,00129
```

Das sind **51 % des besten Rennreifens** und 23 % des Contact Urban. Für A/B-Vergleiche
mit demselben Laufrad fällt es heraus ([09-messtheorie](09-messtheorie.md) §1.2), für
Absolutwerte ist es jetzt der größte Einzelfehler — größer als alles, was in §4.2 des
Fehlerbudgets steht.

Behoben wird das über den **Laufrad-Auslaufversuch**
([09-messtheorie](09-messtheorie.md) §9.3): Trommel auf Drehzahl bringen, Laufrad im Lauf
abheben, seinen eigenen Auslauf aufzeichnen, `J_Rad · dω/dt = M_Rad(ω)`. Dafür nötig:
ein Hallsensor mit Magnet an den Speichen (`PIN_WHEEL`) und J_Rad aus dem Bifilarpendel.

## 6 Laufrad-Auslaufversuch: woher J kommt

Gebraucht wird nicht die **Masse**, sondern das **Trägheitsmoment** J des kompletten
Laufrads mit Reifen und Schlauch — nur damit wird aus der gemessenen Verzögerung ein
Moment: `M_Rad(ω) = −J · dω/dt`.

Für ein Laufrad ist J gutmütig, weil Felge, Reifen und Schlauch fast alles stellen und
alle auf demselben Radius sitzen. Rechenwert für ein 622er mit 2,5 kg:
`J ≈ 0,85 · m · r_Felge² = 0,206 kg·m²`.

### 6.1 Was ein J-Fehler kostet

| Fehler auf J | Fehler auf M_Rad | vom Rennreifensignal | als Crr |
|---|---|---|---|
| 2 % | 0,0050 Nm | 0,69 % | 0,000026 |
| 5 % | 0,0125 Nm | 1,74 % | 0,000064 |
| 10 % | 0,0250 Nm | 3,47 % | 0,000129 |

Ziel also **≤ 2–3 %**. Zum Vergleich: ΔCrr = 0,0005 ist die Auflösungsforderung aus
[01-requirements](01-requirements.md) M1 — ein 10-%-Fehler auf J frisst davon ein Viertel.

### 6.2 Bifilarpendel

Gewählt, weil es mit 1,2–1,4 % als einziges Verfahren unter die Zielmarke kommt — ein
Pendel am Felgenhorn liegt bei 2,9 %, Auslauf mit Zusatzmasse bei 4 %, Wiegen und Rechnen
bei 3–5 %.

Rad waagerecht an zwei gleich langen Fäden aufhängen, die an zwei gegenüberliegenden
Punkten der Felge befestigt sind. Um die Hochachse um wenige Grad verdrehen, loslassen,
20 Schwingungen stoppen:

```
J = m · g · b² · T² / (4 π² · L)
```

b = halber Abstand der beiden Aufhängepunkte (= Felgenradius), L = Fadenlänge,
T = Schwingungsdauer.

| b | L | T | 20 Schwingungen | Fehler auf J |
|---|---|---|---|---|
| 30 cm | 100 cm | 1,92 s | 38 s | 1,4 % |
| 30 cm | 150 cm | 2,35 s | 47 s | **1,2 %** |
| 15 cm | 100 cm | 3,84 s | 77 s | 1,5 % |

Bedingungen: Fäden senkrecht und gleich lang, Ausschlag < 10°, **Reifen montiert und
aufgepumpt wie im Versuch** — seine Masse ist Teil von J. D als Gegenprobe rechnen; weichen
beide um mehr als 5 % ab, stimmt etwas nicht.

### 6.3 Reifenwechsel: J mitführen, nicht neu messen

Jeder Reifen bringt eine andere Masse auf denselben Radius. Das muss nachgeführt werden:

| Massenunterschied | ΔJ | Anteil an J | Fehler, wenn ignoriert |
|---|---|---|---|
| 50 g | 0,0055 kg·m² | 2,7 % | 0,9 % vom Rennreifensignal |
| 100 g | 0,0110 kg·m² | 5,3 % | 1,8 % |
| 200 g | 0,0219 kg·m² | 10,6 % | 3,7 % |

Also **einmal bifilar messen**, danach pro Reifen rechnerisch korrigieren:

```
J = J_ref + (m_Reifen − m_Reifen,ref) · r_Reifen²      r_Reifen = (BSD/2 + Reifenhöhe/2)
```

Reifen wiegen kostet 30 Sekunden und ist auf 1 g genau — die Korrektur ist damit besser
als die Grundmessung.

### 6.4 Der Auslauf selbst

| | |
|---|---|
| Startdrehzahl bei 60 km/h Umfang | 469 1/min (ω = 49,2 rad/s) |
| M_Rad am Rad (aus 0,25 Nm an der Trommel) | 0,182 Nm |
| Anfangsverzögerung | 0,88 rad/s² — in 10 s fällt die Drehzahl um 84 1/min |
| Auslauf 60 → 10 km/h | **4,5 min** |
| bis Stillstand | 29 min |

Aufnehmer: **4 Magnete an den Speichen** plus ein Hallsensor. Mit einem Magneten wären es
am unteren Ende nur 1,3 Pulse/s, mit vieren 5,2 — die Kurve bleibt auch bei 10 km/h
aufgelöst. Ausgewertet wird nicht punktweise differenziert, sondern ein Modell
`M_Rad(ω) = A + B·ω²` über den ganzen Auslauf gefittet: A ist die Lagerreibung,
B·ω² die Aerodynamik von Speichen und Reifen.

> **Zwei Vorbehalte.** Erstens misst der freie Auslauf das Rad in ruhender Luft, im
> Messbetrieb läuft es in der Grenzschicht der Trommel — die Aerodynamik ist nicht exakt
> dieselbe. Zweitens tragen die Nabenlager im Auslauf nur das Radgewicht (25 N) statt der
> Auflast (442 N); gerechnet sind das 0,0003 statt 0,005 Nm am Rad, also beide Male
> vernachlässigbar gegen die 0,18 Nm Aerodynamik. Der dominierende Anteil ist
> lastunabhängig — deshalb funktioniert das Verfahren überhaupt.

## 7 Momentenabgriff ohne Pendelwiege

### 7.1 Die Anordnung

Der Rotor des Nabenmotors ist über einen **starren Flansch** mit der Hauptwelle verbunden
und läuft mit ihr um. Der Stator trägt sich auf den motoreigenen Lagern am Rotor ab und
ist gegen Verdrehen **nur durch den Hebelarm** gehalten. Es gibt kein separates
Pendellager.

```
Trommel ── Hauptwelle ── starrer Flansch ── Rotor
                                             │  Motorlager
                                           Stator ── Hebelarm ── Zugglied ── Wägezelle
```

### 7.2 Warum die Motorlagerreibung exakt herausfällt

Freischnitt des Stators und des Rotors getrennt, stationär:

```
Stator:   −T_em + T_reib + T_arm = 0
Rotor:    +T_em − T_reib − T_last = 0      →  T_em = T_reib + T_last
einsetzen: T_arm = T_em − T_reib = T_last
```

Die Rotor-Stator-Reibung steht in beiden Gleichungen und kürzt sich **exakt** heraus. Der
Hebelarm misst nur, was von außen an der Trommel zieht. Damit entfällt die
Pendellager-Hysterese von 1,2 mNm aus dem Fehlerbudget ersatzlos — und mit ihr zwei
Lager, ein Lagerbock und ein Ausrichtproblem.

### 7.3 Das Zugglied: Schaltzug statt Druckstab

Eine starre Verbindung zwischen Hebelarm und Zelle überträgt bei der Armdrehung ein
Störmoment in die Zelle. Bei 7,5 mrad (Endlast, Hebel 40 mm, Zelle 0,3 mm):

| Verbindung | Störmoment | vom Contact-Urban-Signal |
|---|---|---|
| starre M5-Schraube, 20 mm | 9664 mNm | **900 %** |
| Ø3-Stab, 100 mm | 250 mNm | 23 % |
| **Schaltzug 1,2 mm, 80 mm frei** | **1,6 mNm** | **0,15 %** |

→ **Fahrrad-Schaltzug 1,2 mm (1×19), 80 mm freie Länge.** Schaltzüge sind auf minimale
Längung konstruiert — die Schaltpräzision hängt daran; Bremszüge (besonders 7×7) sind
bewusst weicher.

| | Wert | Bedeutung |
|---|---|---|
| Metallquerschnitt | 0,85 mm² | |
| k_axial bei 80 mm | 1272 N/mm | gegen 327 N/mm der Zelle → keine Reihenfeder |
| Bruchlast | ≈ 1600 N | gegen 118 N am Anschlag, Faktor 13 |
| knickt bei | 6,6 N | **beim Bremsen sieht die Zelle höchstens 6,6 N = 6,7 % FS** |

Die letzte Zeile ist der eigentliche Gewinn: **das Bremsmoment geht vollständig am Sensor
vorbei.** Bei 20 Nm Bremsmoment landen 500 N im Anschlag und 6,6 N in der Zelle.
Eine geflochtene Textilschnur wäre mit 157 N/mm dagegen weich genug, um unbeabsichtigt
als Reihenfeder zu wirken und die Eigenfrequenz zu halbieren.

**Befestigung:** werksseitiger Lötnippel an einem Ende, Klemm- oder Pressnippel am
anderen. **Kein Knoten, keine flache Klemmschraube auf dem blanken Zug** — beides rutscht
unter Last. Für die Kraft wäre das harmlos (der Arm ist frei, die Kraft folgt aus dem
Moment), es verstellt aber die Ruhelage und damit die eingestellten Schwellen. Zug
**gerade** führen: eine Krümmung verschiebt die Wirkungslinie und damit den Hebelarm.

### 7.4 Vorspannung

Der Zug darf im Messbetrieb nie schlaff werden, sonst wird das Signal einseitig
abgeschnitten und der Mittelwert verfälscht. Das Moment ist nicht konstant — Unwucht,
Stoßstelle des Mantelblechs und Reifenunrundheit modulieren es einmal pro Umdrehung.

| Momentenwelligkeit | nötige Mindestvorspannung |
|---|---|
| 10 % vom Signal | 2,7 N |
| 20 % | 5,4 N |
| 30 % | 8,1 N |

→ **12 N (12 % FS), als konstante Kraft**. **Keine Feder** — eine Feder hat eine Rate und
zweigt einen drehwinkelabhängigen Anteil des Moments ab; ein Gewicht liefert eine
konstante Kraft und fällt vollständig in die statische Tara. Messbereich damit 12 %
(Tara) bis 76 % FS im Auslegungsfall.

**Ausführung: hängend.** Der Arm sitzt **unter** der Konsole, die Wägezelle darüber, das
Zugglied zieht nach oben. Die Schwerkraft liefert die Vorspannung damit selbst — keine
Umlenkrolle, keine zusätzliche Reibung, und der Zug steht auch im stromlosen Zustand.
Die Konsole kommt dadurch höher und ist besser zugänglich.

Zwei Bedingungen:

- Das Zusatzgewicht wird **starr an den Arm geschraubt**, nicht an einen Draht gehängt.
  Ein hängendes Gewicht ist ein Pendel und bringt eine zweite Eigenfrequenz mit.
- Der Arm muss so stehen, dass das Reaktionsmoment des Motors den Zug **spannt** statt
  entlastet — sonst steht die Zelle bei Null am Anschlag und läuft unter Last leer.

### 7.4a Das Vorspanngewicht gehört nach innen

Für ein gegebenes Vorspannmoment gilt `J = M·r/g` — das Trägheitsmoment wächst **linear
mit dem Radius**, an dem das Gewicht sitzt. Weit außen braucht man weniger Masse, zahlt
aber mehr Trägheit:

| Radius | nötige Masse | J-Zuwachs |
|---|---|---|
| **40 mm** | 1,22 kg | **0,0020 kg·m²** |
| 80 mm | 0,61 kg | 0,0039 kg·m² |
| 200 mm | 0,25 kg | 0,0098 kg·m² |

Das schlägt direkt auf die Eigenfrequenz durch. Bei einem Stator mit J = 0,012 kg·m²:

| Gewicht bei | f_n |
|---|---|
| 40 mm | **30,8 Hz** |
| 200 mm | 24,7 Hz — dicht an der 4. Ordnung (23,5 Hz) |

→ **Vorspanngewicht auf denselben Radius wie die Zelle**, also 40 mm. Die höhere Masse
ist der richtige Tausch.

Wird der Zug doch schlaff, sieht man es in den Rohdaten als untere Begrenzung des
Signals. Das gehört in die Prüfliste des ersten Laufs.

### 7.5 Anschläge und Messzonenerkennung

Der Zug schützt nur gegen Bremsen. In **Zugrichtung** liegt die Gefahr weiter beim
Spitzenmoment des Motors: 12,6 Nm sind bei L = 0,080 m **157 N** und damit weit über
dem Endwert der 5-kg-Zelle (49 N).

| Radius am Hebelarm | Funktion |
|---|---|
| 40 mm | Zugglied zur Wägezelle |
| 80 mm | Kalibrierbohrung |
| **200 mm** | Flagge der Lichtschranke **und** Anschlagschraube, beide Richtungen |

Der große Radius verfünffacht alle Wege und macht die Einstellung erst praktikabel:

| | Weg am Radius 200 mm |
|---|---|
| Messbetrieb (64 % FS) | 0,96 mm |
| Lichtschranke schaltet (90 % FS) | 1,35 mm |
| Anschlag greift (120 % FS) | 1,80 mm |

**Eingestellt wird über die Last, nicht über den Spalt.** Masse anhängen, Anschlagschraube
hineindrehen, bis die Anzeige der Wägezelle aufhört zu steigen, kontern. Das funktioniert
unabhängig von Zellensteifigkeit und Hebelverhältnis:

| Zielkraft | Masse am Kalibrierloch (80 mm) |
|---|---|
| 90 % FS (Lichtschranke) | 4,5 kg |
| 120 % FS (Anschlag) | **6,0 kg** |
| *150 % FS — Grenze der zulässigen Überlast* | *7,5 kg, nie überschreiten* |

> 20 kg am 80-mm-Loch wären 15,7 Nm = 392 N = **400 % vom Endwert** und zerstören die
> Zelle beim Einstellen.

### 7.6 Warum optisch

Jede Alternative übt eine Kraft auf den Hebelarm aus und fälscht damit genau das Signal,
das gemessen wird:

| Verfahren | Kraft auf den Arm | bei r = 200 mm | vom Signal |
|---|---|---|---|
| Mikroschalter, Standard | 1,5 N | 0,30 Nm | 28 % |
| Mikroschalter, hochempfindlich | 0,1 N | 0,02 Nm | 2 % |
| Magnet + Stahlblende | 0,1…1 N (Kantenkraft) | 0,02…0,2 Nm | 2…19 % |
| **Gabellichtschranke** | **0** | **0** | **0 %** |

Eine Alublende scheidet aus: Aluminium schirmt ein statisches Magnetfeld nicht ab. Stahl
würde schirmen, zieht aber genau an der Schaltposition.

Eine Fahne mit **Fenster** erledigt beide Richtungen mit einem Sensor: Licht durch →
Messzone, Signal gültig. Licht blockiert → verwerfen, FAULT.

> Halter der Trommelmarken-Sensoren aus Aluminium oder Kunststoff, nicht aus Stahl —
> sonst zieht er die vorbeilaufenden Magnete an.
