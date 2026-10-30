# Ständer, Trommellagerung und Auflast-Mechanik

Löst die offenen Punkte A3 (Art der Auflast) und A4 (Trommelanbindung) konstruktiv auf und
legt die Maßkette fest. Alles hier ist **Entwurf zum Bauen**, nicht gemessen: jede Zahl
trägt ihre Herkunft. Maße in mm, Kräfte in N.

> **Trommeldurchmesser ist eine Messgröße, keine Festlegung.** Der Entwurf rechnet mit
> **D = 930 mm (geschätzt, 2026-10-30)** statt der früheren 954,9 mm. Daraus:
> U = 2,922 m, r = 465 mm. Der Wert ist mit dem Maßband um die gebaute Trommel zu messen
> (drei Messungen mitteln, [03-parameter](03-parameter.md) §1) — **erst dann** gehen U und
> r in `config.h`. Welche Maße daran hängen und welche nicht, steht in §2.

## 1 Topologie und ihre Begründung

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
     │     │   ◄────── Lagerabstand 340 ──────►              ══╩══ Zelle 10 kg
     ╰──┬──╯                                                   ╰──┬──╯
 z=40 ══╧══════════════════════════════════════════════════════╧══ Querträger 1000
```

Die Gabelholme liegen bei y = ±100 und laufen **über** die Trommel hinweg — bei der
kleinsten Radgröße mit 180 mm Luft, bei der größten mit 375 mm. Der Pendelbock hängt an
einem Querarm vom rechten Rahmen aus (§6.4), nicht an einem eigenen Pfosten: Pendelachse
und Trommelachse müssen koaxial sein, also gehören sie an dasselbe Bauteil.
