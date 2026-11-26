#!/usr/bin/env python3
"""Connection plan of the rolling-resistance test stand — lead by lead.

The circuit diagram (hardware/schaltplan.py) shows WHAT is connected to what.
This plan shows HOW: every part with its terminals in the order in which they
sit there, and every wire individually with signal names.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/anschlussplan.py

Produces hardware/anschlussplan.pdf and hardware/anschlussplan.png.
Pin assignment see docs/04-hardware.md §12; change here and there together.

PRINCIPLE: pin numbers appear here ONLY where they are confirmed. Where the counting
direction on the part is not certain — COMM header of the controller, type of the
D-pad — there is a question mark and, next to it, how to determine it. An invented
pin number is worse than a gap: people solder by it.

The layout runs with a cursor from top to bottom; every section reports its height
back. There are no fixed y values — otherwise the boxes run into each other as soon
as a pin is added somewhere.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # measurement signals
AUX  = "#4a7a2a"   # logic supply
PWR  = "#9c3524"   # power path / warning
LINE = "#c9d2d8"

PITCH   = 0.62        # line spacing of the pins
HEAD    = 1.25        # header height of a box (title + subtitle)
FOOT    = 0.35        # space below the last pin
W       = 16.0        # drawing width
MONO    = {"family": "DejaVu Sans Mono"}

fig, ax = plt.subplots(figsize=(13.5, 19))


def modul(x, ytop, w, titel, untertitel, links=None, rechts=None, farbe=INK):
    """Part with terminals on the left and/or right.

    links/rechts: list of (nr, name, colour); nr may be None.
    Returns (anker, height). anker: {name: (x, y)}.
    """
    links  = links or []
    rechts = rechts or []
    rows = max(len(links), len(rechts))
    h = HEAD + PITCH * rows + FOOT

    ax.add_patch(FancyBboxPatch((x, ytop - h), w, h,
                                boxstyle="round,pad=0,rounding_size=0.12",
                                linewidth=1.8, edgecolor=farbe, facecolor="white", zorder=2))
    ax.text(x + 0.25, ytop - 0.42, titel, fontsize=10.5, weight="bold", color=farbe,
            va="center", zorder=3)
    if untertitel:
        ax.text(x + 0.25, ytop - 0.85, untertitel, fontsize=7.4, color=MUT,
                va="center", zorder=3)

    anker = {}
    for seite, pins in (("links", links), ("rechts", rechts)):
        for i, (nr, name, pc) in enumerate(pins):
            py = ytop - HEAD - i * PITCH
            kante = x if seite == "links" else x + w
            if nr is not None:
                bx = x + 0.10 if seite == "links" else x + w - 0.50
                ax.add_patch(Rectangle((bx, py - 0.17), 0.40, 0.34, linewidth=0.9,
                                       edgecolor=MUT, facecolor="#eef2f4", zorder=3))
                ax.text(bx + 0.20, py, str(nr), fontsize=7.2, ha="center", va="center",
                        color=INK, zorder=4, **MONO)
            tx = x + 0.64 if seite == "links" else x + w - 0.64
            ax.text(tx, py, name, fontsize=8.2, color=pc, zorder=3, **MONO,
                    ha="left" if seite == "links" else "right", va="center")
            sx = kante - 0.30 if seite == "links" else kante + 0.30
            ax.plot([kante, sx], [py, py], color=pc, linewidth=1.3, zorder=2)
            anker[name] = (sx, py)
    return anker, h


def strang(paare, farbe_default=SIG):
    """Several wires between two parts.

    paare: list of (anchorA, anchorB, label, colour). The vertical segments
    are staggered so that the labels do not pile up.
    """
    n = len(paare)
    xs = [p[0][0] for p in paare] + [p[1][0] for p in paare]
    x_links, x_rechts = min(xs), max(xs)
    spanne = x_rechts - x_links
    for i, (a, b, label, farbe) in enumerate(paare):
        (x1, y1), (x2, y2) = a, b
        xm = x_links + spanne * (i + 1) / (n + 1)
        ax.plot([x1, xm, xm, x2], [y1, y1, y2, y2], color=farbe, linewidth=1.5,
                zorder=1, solid_capstyle="round")
        ax.text(xm + 0.10, (y1 + y2) / 2, label, fontsize=7.4, color=farbe,
                va="center", ha="left", zorder=4, **MONO,
                bbox=dict(boxstyle="round,pad=0.14", fc="white", ec="none"))


# One text line at 7.8 pt with line spacing 1.5 is about 0.40 drawing units
# high (the axis runs at roughly 30 pt per unit). The advances follow from that —
# otherwise footnotes run into the next heading.
ZEILE = 0.40


def ueberschrift(y, nummer, titel, hinweis=None):
    y -= 0.35                                    # space above the rule
    ax.plot([0.2, W - 0.2], [y, y], color=LINE, linewidth=2.4, zorder=0)
    ax.text(0.2, y - 0.45, f"{nummer}  {titel}", fontsize=11.5, weight="bold",
            color=INK, va="center")
    if hinweis:
        ax.text(W - 0.2, y - 0.45, hinweis, fontsize=7.8, color=PWR,
                ha="right", va="center")
    return y - 1.05


def fussnote(y, text, farbe=PWR):
    ax.text(0.4, y, text, fontsize=7.8, color=farbe, va="top", linespacing=1.5)
    return y - ZEILE * (text.count("\n") + 1) - 0.30


# ============================================================ header
y = 0.0
ax.text(0.2, y, "Rolling-resistance test stand — connection plan MVP", fontsize=17, color=INK,
        va="top")
y -= 0.70
ax.text(0.2, y, "ESP32-S3-DevKitC-1 N16R8  ·  As of 2026-11-16  ·  "
                "Source: hardware/anschlussplan.py  ·  Pin assignment docs/04-hardware.md §12",
        fontsize=8, color=MUT, va="top")
y -= 0.45
ax.text(0.2, y, "The numbers at the ESP32 are GPIO numbers from the board's silkscreen, "
                "not connector positions. “?​\" = counting direction on the part not yet determined.",
        fontsize=8, color=MUT, va="top")
y -= 1.1

# ============================================================ 1 load cell
y = ueberschrift(y, "1", "Load cell → HX711 → ESP32",
                 "cell lead without connectors, max. 10 cm")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "DevKitC-1", rechts=[
    (None, "3V3", AUX), (None, "GND", AUX),
    (15, "GPIO15", SIG), (16, "GPIO16", SIG)])

hx, h2 = modul(7.0, y, 4.2, "HX711", "module with separate VCC/VDD",
               links=[(1, "VDD", AUX), (2, "VCC", AUX), (3, "DAT", SIG),
                      (4, "CLK", SIG), (5, "GND", AUX)],
               rechts=[(None, "E+", PWR), (None, "E−", PWR),
                       (None, "A+", SIG), (None, "A−", SIG)])

zelle, h3 = modul(13.0, y, 2.8, "Load cell", "5 kg bending beam", links=[
    (None, "red", PWR), (None, "black", PWR), (None, "green", SIG), (None, "white", SIG)])

strang([(esp["3V3"], hx["VDD"], "3.3 V", AUX),
        (esp["3V3"], hx["VCC"], "3.3 V  MVP", AUX),
        (esp["GND"], hx["GND"], "GND", AUX),
        (esp["GPIO15"], hx["DAT"], "DOUT", SIG),
        (esp["GPIO16"], hx["CLK"], "SCK +100 Ω", SIG)])

strang([(hx["E+"], zelle["red"], "exc. +", PWR),
        (hx["E−"], zelle["black"], "exc. −", PWR),
        (hx["A+"], zelle["green"], "sig. +", SIG),
        (hx["A−"], zelle["white"], "sig. −", SIG)])

y -= max(h1, h2, h3) + 0.35
y = fussnote(y, "The cell's wire colours are the usual assignment — check on the actual part.\n"
                "VCC and VDD are both on 3.3 V in the MVP; for the measurement run VCC goes to 5 V.\n"
                "The four cell wires go straight into the screw terminal, without connectors.")

# ============================================================ 2 LCD
y = ueberschrift(y, "2", "LCD 4×20 via PCF8574 backpack", "optional — the UI replaces it")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "I²C", rechts=[
    (None, "3V3", AUX), (None, "GND", AUX), (8, "GPIO8", SIG), (9, "GPIO9", SIG)])

lcd, h2 = modul(8.4, y, 4.0, "PCF8574", "backpack on the LCD", links=[
    (1, "GND", AUX), (2, "VCC", AUX), (3, "SDA", SIG), (4, "SCL", SIG)])

strang([(esp["GND"], lcd["GND"], "GND", AUX),
        (esp["3V3"], lcd["VCC"], "3.3 V", AUX),
        (esp["GPIO8"], lcd["SDA"], "SDA + 3.3 kΩ→3V3", SIG),
        (esp["GPIO9"], lcd["SCL"], "SCL + 3.3 kΩ→3V3", SIG)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "The order GND·VCC·SDA·SCL is printed on the backpack — read it, do not assume it.\n"
                "Try 3.3 V first: then the 4k7 on the backpack also pull to 3.3 V\n"
                "and stay within the specification. Scan the I²C address (0x27 or 0x3F).")

# ============================================================ 3 D-pad
y = ueberschrift(y, "3", "D-pad, five contacts",
                 "no resistors, no capacitors")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "internal pull-up", rechts=[
    (11, "GPIO11", SIG), (12, "GPIO12", SIG), (13, "GPIO13", SIG),
    (14, "GPIO14", SIG), (39, "GPIO39", SIG), (None, "GND", AUX)])

kr, h2 = modul(8.4, y, 4.4, "Five-way switch", "e.g. ALPS SKQU", links=[
    ("?", "up", SIG), ("?", "down", SIG), ("?", "in", SIG),
    ("?", "out", SIG), ("?", "centre", SIG), ("?", "common", AUX)])

strang([(esp["GPIO11"], kr["up"], "UP", SIG),
        (esp["GPIO12"], kr["down"], "DOWN", SIG),
        (esp["GPIO13"], kr["in"], "IN — step size", SIG),
        (esp["GPIO14"], kr["out"], "OUT", SIG),
        (esp["GPIO39"], kr["centre"], "OK", SIG),
        (esp["GND"], kr["common"], "GND", AUX)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "The contact numbers depend on the type and are not fixed yet. Before soldering, determine them\n"
                "with the continuity tester: the common terminal is the one that switches against ALL\n"
                "others. The three Hall wires on the motor are interchangeable among themselves, these are not.")

# ============================================================ 4 controller
y = ueberschrift(y, "4", "Motor controller FSESC 4.20, COMM header",
                 "do NOT connect COMM 5V")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "UART1", rechts=[
    (18, "GPIO18", SIG), (21, "GPIO21", SIG), (None, "GND", AUX)])

comm, h2 = modul(8.4, y, 4.4, "FSESC 4.20", "COMM pin header, 7 pins", links=[
    ("?", "5V", PWR), ("?", "3.3V", MUT), ("?", "−", AUX), ("?", "ADC", MUT),
    ("?", "TX", SIG), ("?", "RX", SIG), ("?", "ADC2", MUT)])

strang([(esp["GPIO18"], comm["RX"], "TX → RX", SIG),
        (esp["GPIO21"], comm["TX"], "RX ← TX", SIG),
        (esp["GND"], comm["−"], "GND — star point", AUX)])

# cross out 5V
x5, y5 = comm["5V"]
ax.plot([x5 - 0.55, x5 - 0.05], [y5 + 0.20, y5 - 0.20], color=PWR, linewidth=2.2, zorder=5)
ax.plot([x5 - 0.55, x5 - 0.05], [y5 - 0.20, y5 + 0.20], color=PWR, linewidth=2.2, zorder=5)
ax.text(x5 - 0.75, y5, "leave unconnected", fontsize=7.4, color=PWR, va="center", ha="right")

y -= max(h1, h2) + 0.35
y = fussnote(y, "Which end of the header is pin 1 is NOT stated in the manual. Before connecting,\n"
                "measure 5 V and ground on the controller — powered via its micro-USB —\n"
                "otherwise it gets wired neatly mirror-inverted. The order above is the manual's.\n"
                "The ground at the controller is the ONLY connection between logic and power ground.")

# ============================================================ 5 supply
y = ueberschrift(y, "5", "Supply via USB-C", "both sockets may supply at the same time")

usb, h1 = modul(0.4, y, 4.2, "USB-C", "two sockets on the board", rechts=[
    (None, "COM — CH343", AUX), (None, "USB — native", AUX)])

q, h2 = modul(9.4, y, 6.2, "Sources", "", links=[
    (None, "Notebook: flashing + console", AUX),
    (None, "Power supply 5 V / ≥ 1 A", AUX)])

strang([(usb["COM — CH343"], q["Notebook: flashing + console"], "as needed", AUX),
        (usb["USB — native"], q["Power supply 5 V / ≥ 1 A"], "permanent", AUX)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "Only on COM does esptool flash without pressing a button, and only there does the serial console come out\n"
                "(Serial = UART0 on GPIO 43/44, because ARDUINO_USB_CDC_ON_BOOT is not set).\n"
                "Do NOT feed the 5V pin — USB, 5V pin and 3V3 pin are mutually exclusive.\n"
                "With USB supply the 5V pin is an OUTPUT (VBUS), 3V3 comes from the onboard LDO.", MUT)

# ============================================================ trailer
y -= 0.35
ax.plot([0.2, W - 0.2], [y, y], color=LINE, linewidth=2.4)
y -= 0.80
ax.text(0.2, y, "What is NOT wired in the MVP", fontsize=11.5, weight="bold",
        color=INK, va="top")
y -= 0.85
ax.text(0.2, y,
        "E-stop, fuse and the three brake relays (§9) are missing. Without them there is only free coast-down of\n"
        "3 to 6 minutes — THE MOTOR DOES NOT TURN IN THE MVP. The controller is cabled and the UART link\n"
        "checked, nothing more.\n\n"
        "Also not yet: Hall sensors (GPIO 4 marks, 5 index, 6 wheel, 7 motor Hall), microSD\n"
        "(GPIO 10 CS), second load cell (GPIO 17), brake relays (GPIO 40), E-stop feedback (GPIO 2).\n"
        "In the MVP the speed comes from the controller via UART.",
        fontsize=8.2, color=INK, va="top", linespacing=1.5)
y -= ZEILE * 9 + 1.0
ax.text(0.2, y,
        "Leads:   1  cell→HX711, 4 wires, 10 cm, no connector   ·   "
        "2  HX711→ESP32, LiYCY 6×0.14 shielded, JST-XH 6 pins\n"
        "3  ESP32→LCD, 4 wires   ·   4  ESP32→D-pad, 6 wires   ·   "
        "5  ESP32→FSESC, 3 wires twisted, separate from the phases",
        fontsize=7.8, color=MUT, va="top", linespacing=1.6)
y -= ZEILE * 2 + 0.5

ax.set_xlim(0, W)
ax.set_ylim(y, 0.6)
ax.axis("off")
fig.savefig("hardware/anschlussplan.pdf", bbox_inches="tight")
fig.savefig("hardware/anschlussplan.png", dpi=130, bbox_inches="tight")
print(f"ok — drawing height {abs(y):.1f} units")
