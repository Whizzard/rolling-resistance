#!/usr/bin/env python3
"""Soldering and connection template for the three Hall sensors.

The circuit diagram (hardware/schematic.py) shows WHAT is connected to what, the
connection plan (hardware/wiring_plan.py) HOW the wires run. This plan
shows WHAT GOES ONTO THE BOARD — part by part, hole by hole.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/hall_plan.py

Produces hardware/hall-plan.pdf and hardware/hall-plan.png.

ALL DATA ON THE SENSOR come from the datasheet of the family TLE4905L /
TLE4935L / TLE4945L / TLE4945-2L, Infineon, V1.5 of November 2007:

    Fig. 1, page 2    pin assignment, view onto the PRINTED side:
                      1 = V_S, 2 = GND, 3 = Q
    Page 3            output is OPEN COLLECTOR (Schmitt trigger)
    Page 6            operating range V_S = 3.8 ... 24 V, I_Q up to 100 mA
    Page 7            V_QSat 0.25 V typ. at 40 mA; leakage current <= 10 uA;
                      rise time <= 1 us at R_L = 1.2 kOhm
    Fig. 8, page 11   application circuit: pull-up at the CONTROL UNIT, 4.7 nF each
                      from V_S and from Q to GND AT THE SENSOR
    Page 4 / 5        TLE4905 is the UNIPOLAR type; bipolar (latching) is
                      the TLE4935L from the same family.

WHAT IS NOT IN THE DATASHEET and is therefore not claimed here either: the
pitch of the three legs. The package PG-SSO-3-2 is dimensioned in a separate
document. So: measure on the part, then bend to 2.54 mm.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle, Polygon

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # measurement signal
AUX  = "#4a7a2a"   # supply
PWR  = "#9c3524"   # warning / 5 V
LINE = "#c9d2d8"
SOLDER = "#b8860b"   # wires on the solder side
MONO = {"family": "DejaVu Sans Mono"}

fig, ax = plt.subplots(figsize=(12.0, 24.0))
ax.set_xlim(0, 160)
ax.set_ylim(0, 320)
ax.set_aspect("equal")
ax.axis("off")


# ----------------------------------------------------------------- tools
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
    """Resistor as a rectangle (IEC)."""
    if vert:
        ax.add_patch(Rectangle((x - h / 2, y - w / 2), h, w, ec=color, fc="white", lw=1.3,
                               zorder=3))
        txt(x + h / 2 + 1.5, y, label, 7.5, color, mono=True)
    else:
        ax.add_patch(Rectangle((x - w / 2, y - h / 2), w, h, ec=color, fc="white", lw=1.3,
                               zorder=3))
        txt(x, y + h / 2 + 2.2, label, 7.5, color, ha="center", mono=True)


def cap(x, y, label="", color=INK, w=6.0, gap=1.6):
    """Capacitor, current flowing vertically."""
    line(x - w / 2, y + gap / 2, x + w / 2, y + gap / 2, color, 1.6)
    line(x - w / 2, y - gap / 2, x + w / 2, y - gap / 2, color, 1.6)
    txt(x + w / 2 + 1.6, y, label, 7.5, color, mono=True)


def led(x, y, color=AUX, size=3.2, label=""):
    """LED, current from top to bottom."""
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
    """Zener diode, cathode at the BOTTOM (to the signal), anode at the top (to ground) — as in
    the plan, where the ground rail lies above the signal rail."""
    ax.add_patch(Polygon([(x - size, y + size), (x + size, y + size), (x, y - size * 0.6)],
                         closed=True, ec=color, fc="white", lw=1.3, zorder=3))
    line(x - size, y - size * 0.6, x + size, y - size * 0.6, color, 1.8)
    line(x - size, y - size * 0.6, x - size, y - size * 0.6 - 1.4, color, 1.8)
    line(x + size, y - size * 0.6, x + size, y - size * 0.6 + 1.4, color, 1.8)
    txt(x + size + 1.6, y, label, 7.5, color, mono=True)


# ------------------------------------------------------------------- header
txt(0, 312, "Hall sensors — soldering and connection template", 18, INK, weight="bold")
txt(0, 305, "3 × TLE4905L  ·  drum marks, index mark, wheel  ·  "
            "Source: hardware/hall_plan.py", 9, MUT)
txt(0, 300, "All sensor data from the Infineon datasheet TLE4905L/4935L/4945L, "
            "V1.5 (2007-11) — references in the footnotes.", 9, MUT)
line(0, 295, 160, 295, LINE, 1.2)


# =================================================== 1 pin assignment
txt(0, 287, "1   Sensor terminals", 12, INK, weight="bold")
txt(0, 281, "View onto the PRINTED side, legs pointing down.", 9, PWR, weight="bold")

bx, by, bw, bh = 14, 252, 26, 19
ax.add_patch(FancyBboxPatch((bx, by), bw, bh, boxstyle="round,pad=0,rounding_size=1.6",
                            ec=INK, fc="#f4f6f7", lw=1.6))
txt(bx + bw / 2, by + 13.5, "05 L", 9.5, INK, ha="center", mono=True, weight="bold")
ax.add_patch(Circle((bx + bw / 2, by + 6.0), 2.2, ec=INK, fc="white", lw=1.0, zorder=4))
txt(bx + bw + 3, by + 6.0, "sensitive area", 7.5, MUT)

for i, (nm, fn, col) in enumerate((("1", "V_S", PWR), ("2", "GND", INK), ("3", "Q", SIG))):
    px = bx + 5 + i * 8
    line(px, by, px, by - 10, INK, 2.0)
    txt(px, by - 14, nm, 9, INK, ha="center", weight="bold", mono=True)
    txt(px, by - 19.5, fn, 8.5, col, ha="center", mono=True)

txt(62, 272, "1   V_S    supply  3.8 … 24 V   →   here 5 V", 9, INK, mono=True)
txt(62, 266, "2   GND", 9, INK, mono=True)
txt(62, 260, "3   Q      output, OPEN COLLECTOR", 9, INK, mono=True)
txt(62, 250, "Open collector means: the sensor can only pull to ground.\n"
             "The high level is made solely by the external circuit —\n"
             "and that is exactly where the 3.3 V come from.", 8.5, MUT, va="top")

txt(0, 225, "TLE4905L is the UNIPOLAR type of the family: it switches at ONE "
            "field direction. All eight magnets with", 8.5, PWR)
txt(0, 220, "the same side facing out — the bipolar, latching type would be the "
            "TLE4935L from the same series.", 8.5, PWR)
txt(0, 213, "Package PG-SSO-3-2. The pitch of the legs is NOT in the datasheet: "
            "measure on the part, then bend", 8.5, MUT)
txt(0, 208, "to 2.54 mm. The order 1-2-3 stays the same.", 8.5, MUT)
line(0, 202, 160, 202, LINE, 1.0)


# =================================================== 2 circuit
txt(0, 194, "2   Circuit per sensor", 12, INK, weight="bold")

rbox(0, 120, 86, 62, MUT, 1.0, "--")
txt(3, 185, "in the housing, at the sensor", 8.5, MUT)
rbox(104, 120, 56, 62, MUT, 1.0, "--")
txt(107, 185, "at the ESP32", 8.5, MUT)
txt(95, 185, "Lead", 8.5, MUT, ha="center")

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

cap(42, (y5 + yg) / 2, "4.7 nF", INK)
line(42, y5, 42, (y5 + yg) / 2 + 0.8); line(42, (y5 + yg) / 2 - 0.8, 42, yg)
dot(42, y5); dot(42, yg)

cap(56, (yg + yq) / 2, "4.7 nF", INK)
line(56, yg, 56, (yg + yq) / 2 + 0.8); line(56, (yg + yq) / 2 - 0.8, 56, yq)
dot(56, yg); dot(56, yq)

lx = 74
res(lx, (y5 + yg) / 2, 9.0, 3.4, "1 kΩ", AUX, vert=True)
line(lx, y5, lx, (y5 + yg) / 2 + 4.5, AUX)
line(lx, (y5 + yg) / 2 - 4.5, lx, yq + 7.0, AUX)
led(lx, yq + 4.0, AUX, 3.0, "green")
line(lx, yq + 1.2, lx, yq, AUX)
dot(lx, y5, PWR); dot(lx, yq, SIG)

tx = 118
res(tx, (y5 + yg) / 2, 9.0, 3.4, "470 Ω", SIG, vert=True)
line(tx, y5, tx, (y5 + yg) / 2 + 4.5, SIG); line(tx, (y5 + yg) / 2 - 4.5, tx, yq, SIG)
dot(tx, y5, PWR); dot(tx, yq, SIG)

tx2 = 134
zener(tx2, (yg + yq) / 2, SIG, 3.0, "3.3 V")
line(tx2, yq, tx2, (yg + yq) / 2 - 1.8, SIG); line(tx2, (yg + yq) / 2 + 3.0, tx2, yg, SIG)
dot(tx2, yq, SIG); dot(tx2, yg)

line(150, yq, 150, yq - 8, SIG)
txt(150, yq - 11.5, "GPIO", 8.5, SIG, ha="center", mono=True)

txt(107, 131, "open:         ≈ 3.2 V", 8, SIG, mono=True)
txt(107, 126, "switched:     < 0.1 V", 8, SIG, mono=True)

txt(0, 112, "Pull-up and Zener diode at the ESP32 ARE the level conversion: 470 Ω to 5 V pull "
            "the open collector up,", 8.5, INK)
txt(0, 107, "the 3.3 V Zener diode to ground clamps the node at its breakdown voltage "
            "(≈ 3.6 mA, the knee is soft,", 8.5, INK)
txt(0, 102, "3.1 V on the multimeter is normal). Cathode = ring to the signal, anode to ground "
            "— reversed it clamps as a", 8.5, INK)
txt(0, 97, "normal diode at 0.7 V, the input stays permanently low. A pull-up alone "
           "would be fatal with 5 V at the output.", 8.5, INK)
line(0, 92, 160, 92, LINE, 1.0)


# =================================================== 3 perfboard
txt(0, 88, "3   Perfboard 18 × 13 mm", 12, INK, weight="bold")
txt(0, 82.5, "View from the COMPONENT SIDE", 9, PWR, weight="bold")

P = 10.0
X0, Y0 = 16, 30
COLS = "ABCDEFG"

ax.add_patch(Rectangle((X0 - 6, Y0 - 6), 6 * P + 12, 4 * P + 12, ec=LINE, fc="#fbfcfc",
                       lw=1.2, zorder=0))
txt(X0 - 6, Y0 + 4 * P + 8, "front — towards the drum", 8, PWR)
txt(X0 + 6 * P + 6, Y0 + 4 * P + 8, "rear — cable", 8, MUT, ha="right")

def hole(c, r):
    return X0 + COLS.index(c) * P, Y0 + (r - 1) * P

for r, (nm, col) in ((2, ("+5 V", PWR)), (3, ("GND", INK)), (4, ("Signal", SIG))):
    yy = Y0 + (r - 1) * P
    line(X0, yy, X0 + 6 * P, yy, SOLDER, 3.4)

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
    txt(x1 + 3.8, (y1 + y2) / 2, "4.7 nF", 6.5, INK, mono=True)

x1, y1 = hole("E", 2); x2, y2 = hole("E", 5)
res((x1 + x2) / 2, (y1 + y2) / 2, 12.0, 3.2, "", AUX, vert=True)
line(x1, y1, x1, (y1 + y2) / 2 - 6.0, AUX); line(x2, (y1 + y2) / 2 + 6.0, x2, y2, AUX)
txt(x1 + 3.2, (y1 + y2) / 2, "1 kΩ", 6.5, AUX, mono=True)

xa, ya = hole("E", 5); xb, yb = hole("F", 5)
line(xa, ya, xb, yb, SOLDER, 3.4)
x1, y1 = hole("F", 5); x2, y2 = hole("F", 4)
led(x1, (y1 + y2) / 2, AUX, 2.4, "")
line(x1, y1, x1, (y1 + y2) / 2 + 2.4, AUX); line(x2, (y1 + y2) / 2 - 1.4, x2, y2, AUX)
txt(x1 - 2.0, (y1 + y2) / 2 + 7.5, "LED green", 6.5, AUX, mono=True)

for r, (nm, col) in ((2, ("red", PWR)), (3, ("black", INK)), (4, ("yellow", SIG))):
    xx, yy = hole("G", r)
    line(xx, yy, xx + 11, yy, col, 1.8)
    txt(xx + 12.5, yy, nm, 7, col, mono=True)

txt(X0 - 6, 18, "Thick lines = wire jumpers on the SOLDER SIDE. Lay the three rails "
                "first, then fit the parts.", 8, SOLDER)
txt(X0 - 6, 12.5, "Row 2 = +5 V (red) · row 3 = GND (black) · row 4 = signal "
                  "(yellow).", 8, MUT)
txt(X0 - 6, 7, "LED in column F, i.e. 14.1 mm behind the front edge of the board ≈ 16.3 mm "
               "behind the end wall;", 8, MUT)
txt(X0 - 6, 1.5, "the hole in the lid is at 15.0 mm — so bend the LED legs 1.3 mm "
                 "forward.", 8, MUT)


# =================================================== 4 per sensor
X = 112
txt(X, 88, "4   Per sensor", 12, INK, weight="bold")
rows = [("1 ×", "TLE4905L", "PG-SSO-3-2"),
        ("2 ×", "4.7 nF", "datasheet Fig. 8"),
        ("1 ×", "1 kΩ", "for the LED"),
        ("1 ×", "LED green 3 mm", "see footnote"),
        ("1 ×", "470 Ω", "at the ESP32, pull-up to 5 V"),
        ("1 ×", "Zener 3.3 V", "at the ESP32, cathode to signal"),
        ("1 ×", "3-wire lead", "red/black/yellow")]
for i, (n, b, k) in enumerate(rows):
    yy = 78 - i * 6.0
    txt(X, yy, n, 8, MUT, mono=True)
    txt(X + 8, yy, b, 8, INK, mono=True)
    txt(X, yy - 2.6, k, 7, MUT)

txt(X, 34, "Channels", 10, INK, weight="bold")
for i, (nm, gp) in enumerate((("Drum marks", "GPIO 4"), ("Index mark", "GPIO 5"),
                              ("Wheel", "GPIO 6"))):
    yy = 27 - i * 6.0
    txt(X, yy, nm, 8, INK)
    txt(X + 32, yy, gp, 8, SIG, mono=True)


# =================================================== footnotes
notes = [
    ("5 V, not 3.3 V:", "according to the datasheet the sensor needs at least 3.8 V "
     "(operating range 3.8 … 24 V, page 6). It does not run on 3.3 V."),
    ("Why green:", "with the output open only 5 − 3.2 ≈ 1.8 V lie across the LED branch. "
     "A green LED needs more and reliably stays dark; a red one would glow faintly "
     "and pull the level."),
    ("The 4.7 nF matter:", "they are in the application circuit of the "
     "datasheet (Fig. 8) like this and sit AT THE SENSOR, not at the ESP32 — they catch "
     "what couples into a long lead."),
    ("Current:", "switched, the sensor draws ≈ 10 mA through the pull-up and 2.8 mA through the "
     "LED. 100 mA are allowed; the saturation voltage stays below 0.1 V. Open, "
     "≈ 3.6 mA flow through the Zener diode, about 12 mW — any glass Zener handles that."),
    ("Magnet polarity:", "unipolar means there is only ONE effective field direction. Check "
     "each magnet individually against the sensor before tightening — the LED shows it. "
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

fig.savefig("hardware/hall-plan.pdf", bbox_inches="tight")
fig.savefig("hardware/hall-plan.png", dpi=140, bbox_inches="tight")
print("ok  hardware/hall-plan.pdf + .png")
