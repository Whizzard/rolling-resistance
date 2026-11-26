#!/usr/bin/env python3
"""Sketch: bifilar pendulum for the moment of inertia of the wheel (J_wheel).

Procedure docs/08-stand.md §6.2 and docs/09-measurement-theory.md §9.2, entered in
the user interface under "Create run" (step 1 of the guided measurement run).

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/bifilar.py

Produces hardware/bifilar.pdf and hardware/bifilar.png.
"""
import math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyArrowPatch, Rectangle

INK = "#1b2328"
MUT = "#6b7a85"
SIG = "#1f6f8b"
PWR = "#9c3524"
AUX = "#4a7a2a"
MONO = {"family": "DejaVu Sans Mono"}

fig, (ax, ay) = plt.subplots(1, 2, figsize=(13.5, 7.6), gridspec_kw={"width_ratios": [1.15, 1]})
for a in (ax, ay):
    a.set_aspect("equal"); a.axis("off")

# ------------------------------------------------------------ side view
ax.set_xlim(-6.2, 6.2); ax.set_ylim(-2.6, 9.3)
ax.text(-6.0, 9.2, "Side view", fontsize=12, weight="bold", color=INK, va="top")
# beam at the top
ax.add_patch(Rectangle((-5.0, 7.0), 10.0, 0.35, color="#c9d2d8"))
ax.text(5.1, 7.17, "Beam / ceiling", fontsize=8, color=MUT, va="center")
b = 3.0
L_top, L_bot = 7.0, 1.0
for x in (-b, b):
    ax.plot([x, x], [L_top, L_bot], color=SIG, linewidth=1.6)
    ax.plot([x], [L_top], marker="o", color=SIG, markersize=4)
# wheel seen from the side: rim as a flat disc, tyre as beads
ax.add_patch(Rectangle((-3.3, L_bot - 0.12), 6.6, 0.24, color=INK))
for x in (-3.55, 3.55):
    ax.add_patch(Circle((x, L_bot), 0.32, color="#3a444c"))
ax.add_patch(Rectangle((-0.35, L_bot - 0.45), 0.7, 0.9, color="#8795a1"))
ax.text(0, L_bot - 0.75, "Hub", fontsize=8, color=MUT, ha="center", va="top")
# dimensions b, L
ax.annotate("", xy=(-b, 7.75), xytext=(b, 7.75), arrowprops=dict(arrowstyle="<->", color=PWR, lw=1.3))
ax.text(0, 7.9, "2·b  = distance of the suspension points, equal at top AND bottom", fontsize=9, color=PWR, ha="center", va="bottom")
ax.annotate("", xy=(-b, L_bot + 0.35), xytext=(b, L_bot + 0.35), arrowprops=dict(arrowstyle="<->", color=PWR, lw=1.3))
ax.text(0, L_bot + 0.5, "2·b", fontsize=9, color=PWR, ha="center", va="bottom")
ax.annotate("", xy=(4.6, L_top), xytext=(4.6, L_bot), arrowprops=dict(arrowstyle="<->", color=SIG, lw=1.3))
ax.text(4.8, (L_top + L_bot) / 2, "L\nthread length\nboth equal", fontsize=9, color=SIG, va="center")
ax.text(-6.0, -0.2,
        "Threads parallel and vertical, wheel horizontal (spirit level on the rim).\n"
        "Tyre mounted and inflated as in the test; magnet on the spoke end in place.\n"
        "Do not suspend near the hub: J grows with b², every mm of error in b counts twice.\n"
        "At the rim (b ≈ rim radius) the method is most forgiving.",
        fontsize=8.6, color=INK, va="top", linespacing=1.5)

# ------------------------------------------------------------ top view
ay.set_xlim(-5.6, 7.4); ay.set_ylim(-5.4, 5.6)
ay.text(-5.4, 5.4, "Top view", fontsize=12, weight="bold", color=INK, va="top")
R_t, R_f = 3.6, 3.1
ay.add_patch(Circle((0, 0), R_t, fill=False, color="#3a444c", linewidth=7))
ay.add_patch(Circle((0, 0), R_f, fill=False, color=INK, linewidth=1.2))
ay.add_patch(Circle((0, 0), 0.4, color="#8795a1"))
for k in range(16):
    a = k * math.pi / 8
    ay.plot([0.4 * math.cos(a), R_f * math.cos(a)], [0.4 * math.sin(a), R_f * math.sin(a)], color="#9aa6b0", linewidth=0.8)
# suspension points opposite each other on the rim
for s in (-1, 1):
    ay.plot([s * R_f], [0], marker="o", color=SIG, markersize=8)
ay.text(R_f - 0.25, 0.35, "Thread", fontsize=8.5, color=SIG, ha="right")
ay.text(-R_f + 0.25, 0.35, "Thread", fontsize=8.5, color=SIG, ha="left")
ay.annotate("", xy=(-R_f, -0.6), xytext=(R_f, -0.6), arrowprops=dict(arrowstyle="<->", color=PWR, lw=1.3))
ay.text(0, -0.85, "2·b  (opposite on the rim, through the axle)", fontsize=8.5, color=PWR, ha="center", va="top")
# twist
phi = math.radians(9)
ay.add_patch(FancyArrowPatch((R_t + 0.55, 0.0), ((R_t + 0.55) * math.cos(phi * 2.2), (R_t + 0.55) * math.sin(phi * 2.2)),
                             connectionstyle="arc3,rad=0.25", arrowstyle="-|>", color=AUX, lw=1.6, mutation_scale=14))
ay.text(R_t + 0.9, 1.6, "φ < 10°\ntwist about the\nvertical axis,\nrelease", fontsize=8.5, color=AUX, va="center")
ay.text(-5.4, -4.0,
        "Torsional oscillation only, no sideways swinging — let it settle first if needed.\n"
        "Start the stopwatch at a turning point, count n = 20 full oscillations,\n"
        "stop at the same turning point. T = t / n.",
        fontsize=8.6, color=INK, va="top", linespacing=1.5)

# ------------------------------------------------------------ formula and inputs
fig.text(0.5, 0.075,
         "J_wheel = m · g · b² · T² / (4 π² · L)        m wheel with tyre [kg]   b = half thread distance [m]   L thread length [m]   T = t/n [s]   g = 9.80665 m/s²",
         fontsize=10, color=INK, ha="center", **MONO)
fig.text(0.5, 0.035,
         "Inputs in the user interface, Create run (step 1): mass [g] · thread distance 2·b [mm] · thread length L [mm] · n · time t [s]  →  J, stored as Y<kgm2>",
         fontsize=9, color=MUT, ha="center")
fig.text(0.02, 0.975, "Bifilar pendulum — moment of inertia of the wheel J_wheel", fontsize=16, color=INK, va="top")
fig.text(0.02, 0.935, "As of 2026-11-25  ·  Source: hardware/bifilar.py  ·  Procedure docs/08-stand.md §6.2, formula docs/09-measurement-theory.md §9.2 (16)",
         fontsize=8, color=MUT, va="top")
fig.subplots_adjust(left=0.02, right=0.98, top=0.88, bottom=0.12, wspace=0.05)
fig.savefig("hardware/bifilar.pdf")
fig.savefig("hardware/bifilar.png", dpi=130)
print("ok")
