#!/usr/bin/env python3
"""Loet- und Anschlussvorlage fuer die drei Hall-Geber.

Der Schaltplan (hardware/schaltplan.py) zeigt, WAS womit verbunden ist, der
Anschlussplan (hardware/anschlussplan.py), WIE die Adern laufen. Dieser Plan
zeigt, WAS AUF DIE PLATINE KOMMT — Bauteil fuer Bauteil, Loch fuer Loch.

Quelle ist dieser Text, das PDF wird daraus erzeugt:

    .venv/bin/python hardware/hallplan.py

Erzeugt hardware/hallplan.pdf und hardware/hallplan.png.

ALLE ANGABEN ZUM GEBER stammen aus dem Datenblatt der Familie TLE4905L /
TLE4935L / TLE4945L / TLE4945-2L, Infineon, V1.5 vom November 2007:

    Bild 1, Seite 2   Pinbelegung, Blick auf die BEDRUCKTE Seite:
                      1 = V_S, 2 = GND, 3 = Q
    Seite 3           Ausgang ist OPEN COLLECTOR (Schmitt-Trigger)
    Seite 6           Betriebsbereich V_S = 3,8 ... 24 V, I_Q bis 100 mA
    Seite 7           V_QSat 0,25 V typ. bei 40 mA; Leckstrom <= 10 uA;
                      Anstiegszeit <= 1 us bei R_L = 1,2 kOhm
    Bild 8, Seite 11  Anwendungsschaltung: Pull-up am STEUERGERAET, je 4,7 nF
                      von V_S und von Q nach GND AM GEBER
    Seite 4 / 5       TLE4905 ist der UNIPOLARE Typ; bipolar (latchend) ist
                      der TLE4935L aus derselben Familie.

WAS NICHT IM DATENBLATT STEHT und deshalb hier auch nicht behauptet wird: das
Rastermass der drei Beine. Die Bauform PG-SSO-3-2 ist in einem eigenen Dokument
bemasst. Also: am Bauteil messen, dann auf 2,54 mm aufbiegen.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle, Polygon

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # Messsignal
AUX  = "#4a7a2a"   # Versorgung
PWR  = "#9c3524"   # Warnung / 5 V
LINE = "#c9d2d8"
LOET = "#b8860b"   # Draehte auf der Loetseite
MONO = {"family": "DejaVu Sans Mono"}

fig, ax = plt.subplots(figsize=(12.0, 24.0))
ax.set_xlim(0, 160)
ax.set_ylim(0, 320)
ax.set_aspect("equal")
ax.axis("off")


# ----------------------------------------------------------------- Werkzeug
def txt(x, y, s, size=8, color=INK, ha="left", va="center", weight=None, mono=False,
        zorder=5):
    ax.text(x, y, s, fontsize=size, color=color, ha=ha, va=va, zorder=zorder,
            fontweight=weight, **(MONO if mono else {}))


def rbox(x, y, w, h, color=LINE, lw=1.0, ls="-", fc="none"):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2",
                                ec=color, fc=fc, lw=lw, ls=ls))


def line(x1, y1, x2, y2, color=INK, lw=1.2, ls="-"):
    ax.plot([x1, x2], [y1, y2], color=color, lw=lw, ls=ls, solid_capstyle="round",
            zorder=2)


def dot(x, y, color=INK, r=0.9):
    ax.add_patch(Circle((x, y), r, fc=color, ec="none", zorder=3))


def res(x, y, w=9.0, h=3.4, label="", color=INK, vert=False):
    """Widerstand als Rechteck (IEC)."""
    if vert:
        ax.add_patch(Rectangle((x - h / 2, y - w / 2), h, w, ec=color, fc="white", lw=1.3,
                               zorder=3))
        txt(x + h / 2 + 1.5, y, label, 7.5, color, mono=True)
    else:
        ax.add_patch(Rectangle((x - w / 2, y - h / 2), w, h, ec=color, fc="white", lw=1.3,
                               zorder=3))
        txt(x, y + h / 2 + 2.2, label, 7.5, color, ha="center", mono=True)


def cap(x, y, label="", color=INK, w=6.0, gap=1.6):
    """Kondensator, senkrecht durchflossen."""
    line(x - w / 2, y + gap / 2, x + w / 2, y + gap / 2, color, 1.6)
    line(x - w / 2, y - gap / 2, x + w / 2, y - gap / 2, color, 1.6)
    txt(x + w / 2 + 1.6, y, label, 7.5, color, mono=True)


def led(x, y, color=AUX, size=3.2, label=""):
    """LED, Strom von oben nach unten."""
    ax.add_patch(Polygon([(x - size, y + size), (x + size, y + size), (x, y - size * 0.6)],
                         closed=True, ec=color, fc="white", lw=1.3, zorder=3))
    line(x - size, y - size * 0.6, x + size, y - size * 0.6, color, 1.8)
    for k in (0, 1):
        x0, y0 = x + size * 0.9 + k * 2.2, y + size * 0.5 + k * 1.4
        line(x0, y0, x0 + 2.4, y0 + 2.0, color, 1.0)
        line(x0 + 2.4, y0 + 2.0, x0 + 1.5, y0 + 1.9, color, 1.0)
        line(x0 + 2.4, y0 + 2.0, x0 + 2.2, y0 + 1.1, color, 1.0)
    txt(x + size + 6.5, y, label, 7.5, color, mono=True)


def zener(x, y, color=INK, size=3.0, label=""):
    """Z-Diode, Kathode UNTEN (am Signal), Anode oben (an Masse) — wie im Plan,
    wo die Masseschiene über der Signalschiene liegt."""
    ax.add_patch(Polygon([(x - size, y + size), (x + size, y + size), (x, y - size * 0.6)],
                         closed=True, ec=color, fc="white", lw=1.3, zorder=3))
    line(x - size, y - size * 0.6, x + size, y - size * 0.6, color, 1.8)
    line(x - size, y - size * 0.6, x - size, y - size * 0.6 - 1.4, color, 1.8)
    line(x + size, y - size * 0.6, x + size, y - size * 0.6 + 1.4, color, 1.8)
    txt(x + size + 1.6, y, label, 7.5, color, mono=True)


# ------------------------------------------------------------------- Kopf
txt(0, 312, "Hall-Geber — Löt- und Anschlussvorlage", 18, INK, weight="bold")
txt(0, 305, "3 × TLE4905L  ·  Trommelmarken, Indexmarke, Laufrad  ·  "
            "Quelle: hardware/hallplan.py", 9, MUT)
txt(0, 300, "Alle Geberangaben aus dem Infineon-Datenblatt TLE4905L/4935L/4945L, "
            "V1.5 (2007-11) — Fundstellen in den Fußnoten.", 9, MUT)
line(0, 295, 160, 295, LINE, 1.2)


# =================================================== 1 Pinbelegung
txt(0, 287, "1   Anschlüsse des Gebers", 12, INK, weight="bold")
txt(0, 281, "Blick auf die BEDRUCKTE Seite, Beine nach unten.", 9, PWR, weight="bold")

bx, by, bw, bh = 14, 252, 26, 19
ax.add_patch(FancyBboxPatch((bx, by), bw, bh, boxstyle="round,pad=0,rounding_size=1.6",
                            ec=INK, fc="#f4f6f7", lw=1.6))
txt(bx + bw / 2, by + 13.5, "05 L", 9.5, INK, ha="center", mono=True, weight="bold")
ax.add_patch(Circle((bx + bw / 2, by + 6.0), 2.2, ec=INK, fc="white", lw=1.0, zorder=4))
txt(bx + bw + 3, by + 6.0, "empfindliche Fläche", 7.5, MUT)

for i, (nm, fn, col) in enumerate((("1", "V_S", PWR), ("2", "GND", INK), ("3", "Q", SIG))):
    px = bx + 5 + i * 8
    line(px, by, px, by - 10, INK, 2.0)
    txt(px, by - 14, nm, 9, INK, ha="center", weight="bold", mono=True)
    txt(px, by - 19.5, fn, 8.5, col, ha="center", mono=True)

txt(62, 272, "1   V_S    Versorgung  3,8 … 24 V   →   hier 5 V", 9, INK, mono=True)
txt(62, 266, "2   GND", 9, INK, mono=True)
txt(62, 260, "3   Q      Ausgang, OPEN COLLECTOR", 9, INK, mono=True)
txt(62, 250, "Open Collector heißt: der Geber kann nur nach Masse ziehen.\n"
             "Den High-Pegel macht ausschließlich die Beschaltung —\n"
             "und genau daher kommen die 3,3 V.", 8.5, MUT, va="top")

txt(0, 225, "TLE4905L ist der UNIPOLARE Typ der Familie: er schaltet bei EINER "
            "Feldrichtung. Alle acht Magnete mit", 8.5, PWR)
txt(0, 220, "derselben Seite nach außen — der bipolare, latchende Typ wäre der "
            "TLE4935L aus derselben Reihe.", 8.5, PWR)
txt(0, 213, "Bauform PG-SSO-3-2. Das Rastermaß der Beine steht NICHT im Datenblatt: "
            "am Bauteil messen, dann auf", 8.5, MUT)
txt(0, 208, "2,54 mm aufbiegen. Die Reihenfolge 1-2-3 bleibt dabei.", 8.5, MUT)
line(0, 202, 160, 202, LINE, 1.0)


# =================================================== 2 Schaltung
txt(0, 194, "2   Schaltung je Geber", 12, INK, weight="bold")

rbox(0, 120, 86, 62, MUT, 1.0, "--")
txt(3, 185, "im Gehäuse, am Geber", 8.5, MUT)
rbox(104, 120, 56, 62, MUT, 1.0, "--")
txt(107, 185, "am ESP32", 8.5, MUT)
txt(95, 185, "Leitung", 8.5, MUT, ha="center")

y5, yg, yq = 172, 156, 140
for yy, nm, col in ((y5, "+5 V", PWR), (yg, "GND", INK), (yq, "Signal", SIG)):
    line(6, yy, 154, yy, col, 1.4)
    txt(95, yy + 2.8, nm, 8, col, ha="center", mono=True)

gx, gw = 12, 20
ax.add_patch(Rectangle((gx, yq - 6), gw, (y5 - yq) + 12, ec=INK, fc="white", lw=1.6,
                       zorder=3))
txt(gx + gw / 2, yg, "TLE\n4905L", 9, INK, ha="center")
for yy, nm in ((y5, "1"), (yg, "2"), (yq, "3")):
    txt(gx - 2.5, yy, nm, 8, INK, ha="right", mono=True)
    dot(gx, yy)

cap(42, (y5 + yg) / 2, "4,7 nF", INK)
line(42, y5, 42, (y5 + yg) / 2 + 0.8); line(42, (y5 + yg) / 2 - 0.8, 42, yg)
dot(42, y5); dot(42, yg)

cap(56, (yg + yq) / 2, "4,7 nF", INK)
line(56, yg, 56, (yg + yq) / 2 + 0.8); line(56, (yg + yq) / 2 - 0.8, 56, yq)
dot(56, yg); dot(56, yq)

lx = 74
res(lx, (y5 + yg) / 2, 9.0, 3.4, "1 kΩ", AUX, vert=True)
line(lx, y5, lx, (y5 + yg) / 2 + 4.5, AUX)
line(lx, (y5 + yg) / 2 - 4.5, lx, yq + 7.0, AUX)
led(lx, yq + 4.0, AUX, 3.0, "grün")
line(lx, yq + 1.2, lx, yq, AUX)
dot(lx, y5, PWR); dot(lx, yq, SIG)

tx = 118
res(tx, (y5 + yg) / 2, 9.0, 3.4, "470 Ω", SIG, vert=True)
line(tx, y5, tx, (y5 + yg) / 2 + 4.5, SIG); line(tx, (y5 + yg) / 2 - 4.5, tx, yq, SIG)
dot(tx, y5, PWR); dot(tx, yq, SIG)

tx2 = 134
zener(tx2, (yg + yq) / 2, SIG, 3.0, "3,3 V")
line(tx2, yq, tx2, (yg + yq) / 2 - 1.8, SIG); line(tx2, (yg + yq) / 2 + 3.0, tx2, yg, SIG)
dot(tx2, yq, SIG); dot(tx2, yg)

line(150, yq, 150, yq - 8, SIG)
txt(150, yq - 11.5, "GPIO", 8.5, SIG, ha="center", mono=True)

txt(107, 131, "offen:        ≈ 3,2 V", 8, SIG, mono=True)
txt(107, 126, "geschaltet:   < 0,1 V", 8, SIG, mono=True)

txt(0, 112, "Pull-up und Z-Diode am ESP32 SIND die Pegelwandlung: 470 Ω nach 5 V ziehen "
            "den offenen Kollektor hoch,", 8.5, INK)
txt(0, 107, "die Z-Diode 3,3 V gegen Masse klemmt den Knoten auf ihre Durchbruchspannung "
            "(≈ 3,6 mA, der Knick ist weich,", 8.5, INK)
txt(0, 102, "3,1 V am Multimeter sind normal). Kathode = Ring ans Signal, Anode an Masse "
            "— falsch herum klemmt sie als", 8.5, INK)
txt(0, 97, "normale Diode auf 0,7 V, der Eingang bleibt dauerhaft low. Ein Pull-up allein "
           "wäre mit 5 V am Ausgang tödlich.", 8.5, INK)
line(0, 92, 160, 92, LINE, 1.0)


# =================================================== 3 Lochraster
txt(0, 88, "3   Lochrasterplatine 18 × 13 mm", 12, INK, weight="bold")
txt(0, 81.5, "Blick von der BESTÜCKUNGSSEITE", 9, PWR, weight="bold")

P = 10.0
X0, Y0 = 16, 30
COLS = "ABCDEFG"

ax.add_patch(Rectangle((X0 - 6, Y0 - 6), 6 * P + 12, 4 * P + 12, ec=LINE, fc="#fbfcfc",
                       lw=1.2, zorder=0))
txt(X0 - 6, Y0 + 4 * P + 10, "vorn — zur Trommel", 8, PWR)
txt(X0 + 6 * P + 6, Y0 + 4 * P + 10, "hinten — Kabel", 8, MUT, ha="right")

def hole(c, r):
    return X0 + COLS.index(c) * P, Y0 + (r - 1) * P

for r, (nm, col) in ((2, ("+5 V", PWR)), (3, ("GND", INK)), (4, ("Signal", SIG))):
    yy = Y0 + (r - 1) * P
    line(X0, yy, X0 + 6 * P, yy, LOET, 3.4)

for c in range(7):
    for r in range(5):
        ax.add_patch(Circle((X0 + c * P, Y0 + r * P), 1.7, ec=MUT, fc="white", lw=0.9,
                            zorder=4))
for c in range(7):
    txt(X0 + c * P, Y0 - 10, COLS[c], 8.5, MUT, ha="center", mono=True)
for r in range(5):
    txt(X0 - 10, Y0 + r * P, str(r + 1), 8.5, MUT, ha="center", mono=True)

hx, hy = hole("A", 3)
ax.add_patch(FancyBboxPatch((hx - 16, hy - P - 1), 12, 2 * P + 2,
                            boxstyle="round,pad=0,rounding_size=1.4",
                            ec=INK, fc="#f4f6f7", lw=1.5, zorder=5))
txt(hx - 10, hy, "TLE\n4905", 6.5, INK, ha="center", zorder=6)
for r in (2, 3, 4):
    xx, yy = hole("A", r)
    line(xx - 4, yy, xx, yy, INK, 1.6)
    txt(xx - 5.5, yy, str(r - 1), 6.5, INK, ha="right", mono=True)

for col, r1, r2 in (("B", 2, 3), ("C", 3, 4)):
    x1, y1 = hole(col, r1); x2, y2 = hole(col, r2)
    cap((x1 + x2) / 2, (y1 + y2) / 2, "", INK, 5.5, 1.4)
    line(x1, y1, x1, (y1 + y2) / 2 + 0.7); line(x2, (y1 + y2) / 2 - 0.7, x2, y2)
    txt(x1 + 3.8, (y1 + y2) / 2, "4,7 nF", 6.5, INK, mono=True)

x1, y1 = hole("E", 2); x2, y2 = hole("E", 5)
res((x1 + x2) / 2, (y1 + y2) / 2, 12.0, 3.2, "", AUX, vert=True)
line(x1, y1, x1, (y1 + y2) / 2 - 6.0, AUX); line(x2, (y1 + y2) / 2 + 6.0, x2, y2, AUX)
txt(x1 + 3.2, (y1 + y2) / 2, "1 kΩ", 6.5, AUX, mono=True)

xa, ya = hole("E", 5); xb, yb = hole("F", 5)
line(xa, ya, xb, yb, LOET, 3.4)
x1, y1 = hole("F", 5); x2, y2 = hole("F", 4)
led(x1, (y1 + y2) / 2, AUX, 2.4, "")
line(x1, y1, x1, (y1 + y2) / 2 + 2.4, AUX); line(x2, (y1 + y2) / 2 - 1.4, x2, y2, AUX)
txt(x1 - 2.0, (y1 + y2) / 2 + 7.5, "LED grün", 6.5, AUX, mono=True)

for r, (nm, col) in ((2, ("rot", PWR)), (3, ("schwarz", INK)), (4, ("gelb", SIG))):
    xx, yy = hole("G", r)
    line(xx, yy, xx + 11, yy, col, 1.8)
    txt(xx + 12.5, yy, nm, 7, col, mono=True)

txt(X0 - 6, 18, "Dicke Linien = Drahtbrücken auf der LÖTSEITE. Erst die drei Schienen "
                "legen, dann bestücken.", 8, LOET)
txt(X0 - 6, 12.5, "Reihe 2 = +5 V (rot) · Reihe 3 = GND (schwarz) · Reihe 4 = Signal "
                  "(gelb).", 8, MUT)
txt(X0 - 6, 7, "LED in Spalte F, also 14,1 mm hinter der Platinenvorderkante ≈ 16,3 mm "
               "hinter der Stirnwand;", 8, MUT)
txt(X0 - 6, 1.5, "das Loch im Deckel liegt bei 15,0 mm — die Beine der LED also 1,3 mm "
                 "nach vorn biegen.", 8, MUT)


# =================================================== 4 Je Geber
X = 112
txt(X, 88, "4   Je Geber", 12, INK, weight="bold")
rows = [("1 ×", "TLE4905L", "PG-SSO-3-2"),
        ("2 ×", "4,7 nF", "Datenblatt Bild 8"),
        ("1 ×", "1 kΩ", "für die LED"),
        ("1 ×", "LED grün 3 mm", "siehe Fußnote"),
        ("1 ×", "470 Ω", "am ESP32, Pull-up nach 5 V"),
        ("1 ×", "Z-Diode 3,3 V", "am ESP32, Kathode ans Signal"),
        ("1 ×", "Leitung 3-adrig", "rot/schwarz/gelb")]
for i, (n, b, k) in enumerate(rows):
    yy = 78 - i * 6.0
    txt(X, yy, n, 8, MUT, mono=True)
    txt(X + 8, yy, b, 8, INK, mono=True)
    txt(X, yy - 2.6, k, 7, MUT)

txt(X, 36, "Kanäle", 10, INK, weight="bold")
for i, (nm, gp) in enumerate((("Trommelmarken", "GPIO 4"), ("Indexmarke", "GPIO 5"),
                              ("Laufrad", "GPIO 6"))):
    yy = 29 - i * 6.0
    txt(X, yy, nm, 8, INK)
    txt(X + 32, yy, gp, 8, SIG, mono=True)


# =================================================== Fussnoten
notes = [
    ("5 V, nicht 3,3 V:", "der Geber braucht laut Datenblatt mindestens 3,8 V "
     "(Betriebsbereich 3,8 … 24 V, Seite 6). An 3,3 V läuft er nicht."),
    ("Warum grün:", "bei offenem Ausgang liegen über dem LED-Zweig nur 5 − 3,2 ≈ 1,8 V. "
     "Eine grüne LED braucht mehr und bleibt sicher dunkel; eine rote würde glimmen "
     "und den Pegel verziehen."),
    ("Die 4,7 nF sind kein Zierrat:", "sie stehen so in der Anwendungsschaltung des "
     "Datenblatts (Bild 8) und sitzen AM GEBER, nicht am ESP32 — sie fangen ab, was "
     "sich auf einer langen Leitung einkoppelt."),
    ("Strom:", "geschaltet zieht der Geber ≈ 10 mA durch den Pull-up und 2,8 mA durch die "
     "LED. Erlaubt sind 100 mA; die Sättigungsspannung bleibt dabei unter 0,1 V. Offen "
     "fließen ≈ 3,6 mA durch die Z-Diode, rund 12 mW — jede Glas-Z-Diode hält das aus."),
    ("Magnetpolung:", "unipolar heißt, es gibt nur EINE wirksame Feldrichtung. Jeden "
     "Magneten vor dem Festziehen einzeln gegen den Geber prüfen — die LED zeigt es an. "
     "docs/04-hardware.md §8.4."),
]
line(0, -8, 160, -8, LINE, 1.0)
yy = -14
for head, body in notes:
    txt(0, yy, head, 8.5, INK, weight="bold")
    words, lines_, cur = body.split(), [], ""
    for w in words:
        if len(cur) + len(w) > 88:
            lines_.append(cur); cur = w
        else:
            cur = (cur + " " + w).strip()
    lines_.append(cur)
    for j, ln in enumerate(lines_):
        txt(40, yy - j * 4.6, ln, 8.5, MUT)
    yy -= 4.6 * len(lines_) + 4.0

fig.savefig("hardware/hallplan.pdf", bbox_inches="tight")
fig.savefig("hardware/hallplan.png", dpi=140, bbox_inches="tight")
print("ok  hardware/hallplan.pdf + .png")
