#!/usr/bin/env python3
"""microSD on the ESP32-S3 via a microSD-to-SD adapter — wire by wire.

The passive adapter (the sleeve that comes with a microSD) serves as the socket:
wires are soldered to its nine contacts, the microSD sits inside. It only passes
through, without level shifter and without regulator — the card runs directly on 3.3 V,
and the ESP32-S3 speaks 3.3 V. docs/04-hardware.md §15, as of 2026-11-26.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/sdplan.py

Produces hardware/sdplan.pdf and hardware/sdplan.png.

CONTACT ORDER: the SD card originally had the seven contacts of the MMC (1…7);
8 and 9 were added at the edges, 9 at the bevelled corner next to 1. Seen from the
contact side, bevel top left: 9 1 2 3 4 5 6 7 8. Count again on the adapter before
soldering — the bevel is the reference, not the drawing.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle, FancyBboxPatch

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # data
PWR  = "#9c3524"   # 3.3 V
GND  = "#4a7a2a"
NC   = "#9aa5ad"
MONO = {"family": "DejaVu Sans Mono"}

fig, ax = plt.subplots(figsize=(13, 10))

# (contact, SD name, SPI function, ESP32 pin or None, colour, pull-up)
PINS = [
    (9, "DAT2",    "free", None,     NC,  True),
    (1, "DAT3/CD", "CS",   "GPIO10", SIG, True),
    (2, "CMD",     "MOSI", "GPIO11", SIG, False),
    (3, "VSS1",    "GND",  "GND",    GND, False),
    (4, "VDD",     "3V3",  "3V3",    PWR, False),
    (5, "CLK",     "SCK",  "GPIO12", SIG, False),
    (6, "VSS2",    "GND",  "GND",    GND, False),
    (7, "DAT0",    "MISO", "GPIO13", SIG, True),
    (8, "DAT1",    "free", None,     NC,  True),
]

# ------------------------------------------------------------ SD adapter
# Schematic, not to scale: view onto the contacts, bevel top left.
PITCH = 1.3
X0, Y0, CH = 0.6, 0.0, 0.8
BW = PITCH * len(PINS) + 0.6
BH = 4.2
ax.add_patch(Polygon([(X0 + CH, Y0 + BH), (X0 + BW, Y0 + BH), (X0 + BW, Y0),
                      (X0, Y0), (X0, Y0 + BH - CH)], closed=True,
                     linewidth=2, edgecolor=INK, facecolor="#f4f6f8", zorder=1))
ax.text(X0 + BW / 2, Y0 + 0.55, "SD adapter (microSD inside)  ·  view onto the contacts  ·  not to scale",
        ha="center", va="center", fontsize=9, color=MUT)
ax.text(X0 + 0.25, Y0 + BH - 0.25, "Bevel", fontsize=7, color=MUT, rotation=45,
        ha="center", va="center")

top = Y0 + BH
xs = {}
for i, (nr, sdn, spi, pin, col, pu) in enumerate(PINS):
    cx = X0 + 0.6 + PITCH * (i + 0.5)
    xs[nr] = cx
    h = 0.55 if nr != 9 else 0.45
    ax.add_patch(Rectangle((cx - 0.3, top - 0.15 - h), 0.6, h,
                           facecolor="#d9b44a", edgecolor="#8a6d1f", linewidth=0.8, zorder=3))
    ax.text(cx, top - 1.0, str(nr), ha="center", va="center", fontsize=10, weight="bold", color=INK)
    ax.text(cx, top - 1.4, sdn, ha="center", va="top", fontsize=7.5, color=col, **MONO)
    ax.text(cx, top - 1.8, spi, ha="center", va="top", fontsize=7.5, color=col, **MONO)

# ------------------------------------------------------------ wires
# Every contact goes straight up to its pin on the ESP32 — no routing.
Y_PIN = top + 5.6          # bottom edge of the pin labels
R_Y0, R_Y1 = top + 1.6, top + 3.0   # resistor body
for nr, sdn, spi, pin, col, pu in PINS:
    x = xs[nr]
    if not pin:            # DAT1/DAT2: only the pull-up, drawn below
        continue
    ax.plot([x, x], [top - 0.15, Y_PIN], color=col, linewidth=1.8,
            solid_capstyle="round", zorder=2)
    if pin:
        ax.add_patch(FancyBboxPatch((x - 0.55, Y_PIN), 1.1, 0.55,
                                    boxstyle="round,pad=0,rounding_size=0.08",
                                    linewidth=1.4, edgecolor=col, facecolor="white", zorder=3))
        ax.text(x, Y_PIN + 0.275, pin, ha="center", va="center", fontsize=8.5,
                color=col, weight="bold", zorder=4, **MONO)

# Pull-ups 10 kΩ to 3V3: branch to the right of the wire, resistor upright.
for nr, sdn, spi, pin, col, pu in PINS:
    if not pu:
        continue
    x = xs[nr]
    rx = x + 0.45 if pin else x
    yb = top + 0.9
    if pin:
        ax.plot([x, rx, rx], [yb, yb, R_Y0], color=INK, linewidth=1.2, zorder=2)
        ax.plot(x, yb, "o", color=INK, markersize=4, zorder=4)
    else:
        ax.plot([x, x], [top - 0.15, R_Y0], color=INK, linewidth=1.2, zorder=2)
    ax.add_patch(Rectangle((rx - 0.14, R_Y0), 0.28, R_Y1 - R_Y0, linewidth=1.3,
                           edgecolor=INK, facecolor="white", zorder=3))
    ax.text(rx + 0.2, (R_Y0 + R_Y1) / 2, "10k", rotation=90, ha="left", va="center",
            fontsize=7.5, color=INK, **MONO)
    ax.plot([rx, rx], [R_Y1, R_Y1 + 0.4], color=INK, linewidth=1.2, zorder=2)
    ax.plot([rx - 0.22, rx + 0.22], [R_Y1 + 0.4, R_Y1 + 0.4], color=PWR, linewidth=2, zorder=2)
    ax.text(rx, R_Y1 + 0.5, "3V3", ha="center", va="bottom", fontsize=7.5, color=PWR,
            weight="bold", **MONO)

# Decoupling: two capacitors between contact 4 (VDD) and 3 (VSS), bridging the wires.
def cap(y, label):
    xa, xb = xs[3], xs[4]
    xm = (xa + xb) / 2
    ax.plot([xa, xm - 0.08], [y, y], color=INK, linewidth=1.2, zorder=2)
    ax.plot([xm + 0.08, xb], [y, y], color=INK, linewidth=1.2, zorder=2)
    for xp in (xm - 0.08, xm + 0.08):
        ax.plot([xp, xp], [y - 0.22, y + 0.22], color=INK, linewidth=2, zorder=3)
    for xp in (xa, xb):
        ax.plot(xp, y, "o", color=INK, markersize=4, zorder=4)
    ax.text(xm, y + 0.3, label, ha="center", va="bottom", fontsize=7.5, color=INK, **MONO)
cap(top + 1.5, "100n")
cap(top + 2.8, "10µ")

# ESP32 bar across the pin labels
ax.add_patch(FancyBboxPatch((X0, Y_PIN + 0.75), BW, 0.75,
                            boxstyle="round,pad=0,rounding_size=0.12", linewidth=1.8,
                            edgecolor=INK, facecolor="white", zorder=1))
ax.text(X0 + 0.25, Y_PIN + 1.125, "ESP32-S3-DevKitC-1", fontsize=10.5, weight="bold",
        color=INK, va="center")
ax.text(X0 + BW - 0.25, Y_PIN + 1.125, "pin names as printed on the board", fontsize=8,
        color=MUT, va="center", ha="right")

ax.text(0.6, -0.5,
        "FIT ON THE ADAPTER\n"
        "  4 × 10 kΩ to 3V3: contact 1 (CS), 7 (MISO), 8 (DAT1), 9 (DAT2)\n"
        "     CS high = card deselected while the ESP32 boots; leaving DAT1/DAT2 open can upset the card.\n"
        "  10 µF + 100 nF between contact 4 (VDD) and 3 (VSS), as close as possible — the card draws current spikes when writing.\n"
        "\n"
        "WIRES  as short as possible (under 15 cm), run GND next to CLK. Clock 10 MHz (SD_SPI_KHZ); halve it on errors.\n"
        "Contacts 3 and 6 are both ground and both get connected.  GPIO 10…13 = SPI2 via IO-MUX; position on the board per silkscreen.\n"
        "The adapter's write-protect slider has NO effect in SPI mode — it is only a switch for the reader.",
        fontsize=8, color=INK, va="top", linespacing=1.55, **MONO)

ax.text(0.6, 13.4, "microSD via SD adapter on the ESP32-S3", fontsize=16, color=INK, va="top")
ax.text(0.6, 12.85, "As of 2026-11-26  ·  Source: hardware/sdplan.py  ·  Text docs/04-hardware.md §15  ·  "
                    "Pins firmware/src/config.h (PIN_SD_*)", fontsize=8, color=MUT, va="top")

ax.set_xlim(0, 13.4)
ax.set_ylim(-3.9, 13.6)
ax.set_aspect("equal")
ax.axis("off")
fig.savefig("hardware/sdplan.pdf", bbox_inches="tight")
fig.savefig("hardware/sdplan.png", dpi=130, bbox_inches="tight")
print("ok")
