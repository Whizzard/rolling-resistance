#!/usr/bin/env python3
"""Concept sketch: automatic hanging of the 1000 g calibration weight (builder's idea 2026-11-27).

BASIC DESIGN ONLY -- no printable part yet. Two parts that never touch in the measuring state:

  M  measuring hanger: swings on the beam rider's wire as before (pendulum, L_cal unchanged).
     The weight stands on FOUR STUBS under its flat bottom (0/90/180/270 deg), not on the neck.
     Four rods outside the body, joined by a ring around the body; the two x-rods continue up
     as the posts to the wire (as weight_clip.py, but taller: the lifted weight must clear the
     rider legs).
  L  lift cradle: four stubs turned by 45 deg (45/135/225/315 deg) that pass between M's
     stubs and lift the weight off them. Rods outside M, two cross bars along y at x = +-25,
     one Bowden cable at the middle of each (50 mm apart, in line with the pendulum axis = the
     wire, which runs along x). Cables up to the traverse, servo with a balance lever there.

Lowered: L hangs gap_low below the weight and clear of M everywhere -> the weight hangs only
on M. Raised by `stroke`: the weight sits on L, lifted off M by stroke - gap_low.

1000 g weight: neck 33 and head 45 measured 2026-11-24; head top 15 above the neck centre
measured. BODY DIAMETER AND HEIGHTS ARE SCALED from the 200 g weight (calipers 2026-11-26)
by (1000/200)^(1/3) = 1.71 -- TBD, builder measures. Bottom flat (builder 2026-11-27).

    .venv/bin/python hardware/weight_lift_concept.py

Produces hardware/weight-lift-concept.pdf and .png.
"""
import math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle, Polygon

K = (1000 / 200) ** (1 / 3)
# weight (z = 0 its bottom in the measuring state)
body_r = 28 * K / 2            # scaled, TBD
body_h = 34 * K                # scaled (200 g: 46 total - 8 head top - 4 half groove), TBD
neck_r, head_r = 33 / 2, 45 / 2        # measured
neck_l = 8 * K                 # scaled
z_neck = body_h + neck_l / 2
z_head = z_neck + 15           # measured 15 above neck centre
# parts
clr = 3.0                      # clearance everywhere, also when M swings
t = 6.0                        # rod / stub section
stub_tip = 16.0                # stubs reach in to this radius under the bottom
stroke = 20.0                  # lift travel of L
gap_low = 3.0                  # L stubs below the weight, lowered
m_in = body_r + clr            # M rods inside radius
ring_z = (body_h * 0.35, body_h * 0.35 + 8)
rider_w, tube_h, rider_below, hole_h = 18.3, 25.0, 12.0, 15.0   # beam_rider.py
x_top = rider_w / 2 + 1.0      # M posts inside at the top (weight_clip.py)
z_v = z_head + stroke + clr    # M posts vertical up to here (lifted head must pass)
z_bend = z_v + (m_in - x_top)  # 45 deg inwards
z_rider = z_head + stroke + clr            # rider legs bottom
z_tube = z_rider + rider_below
z_wire = z_tube + tube_h + hole_h
cab_x = 25.0                   # cables at x = +-25 (50 mm apart, builder)
l_y = 34.0                     # L rods at y = +-l_y, x = +-cab_x
z_cb = z_bend + t + clr        # L cross bar bottom, lowered: above M's bend
z_trav = z_wire + 45           # traverse, position TBD

BLUE, ORANGE, GREY = "#2f6f99", "#d9822b", "#8a8f94"

def rect(ax, x0, z0, w, h, c, ls="-", fill=True, a=0.85):
    ax.add_patch(Rectangle((x0, z0), w, h, fc=c if fill else "none", ec=c, ls=ls, lw=1.2, alpha=a))

def weight_side(ax, dz=0.0, ls="-", fill=True):
    fc = "#c9ced3" if fill else "none"
    pts = [(-body_r, 0), (body_r, 0), (body_r, body_h), (neck_r, body_h), (neck_r, body_h + neck_l),
           (head_r, body_h + neck_l), (head_r, z_head), (-head_r, z_head), (-head_r, body_h + neck_l),
           (-neck_r, body_h + neck_l), (-neck_r, body_h), (-body_r, body_h)]
    ax.add_patch(Polygon([(x, z + dz) for x, z in pts], fc=fc, ec="#555", lw=1, ls=ls))

def m_front(ax):   # x-z, looking along y
    for s in (-1, 1):
        x0 = s * m_in if s > 0 else -m_in - t
        rect(ax, x0, -t, t, z_v + t, BLUE)                          # rod + post, vertical
        xi = s * m_in; xo = s * (m_in + t)
        ax.add_patch(Polygon([(xi, z_v), (xo, z_v + t * (math.sqrt(2) - 1)),
                              (s * (x_top + t), z_bend + t * (math.sqrt(2) - 1)), (s * x_top, z_bend)],
                             fc=BLUE, ec=BLUE, alpha=0.85))
        rect(ax, s * x_top if s > 0 else -x_top - t, z_bend, t, z_wire + 6 - z_bend, BLUE)
        rect(ax, s * stub_tip if s > 0 else -m_in, -t, m_in - stub_tip, t, BLUE)   # stub 0/180
    rect(ax, -m_in - t, ring_z[0], 2 * (m_in + t), ring_z[1] - ring_z[0], BLUE, a=0.35)
    ax.add_patch(Circle((0, z_wire), 1.2, color="k"))

def l_front(ax, dz, ls="-", fill=True):   # rods at x = +-cab_x are behind/ahead of M (y = +-l_y)
    for s in (-1, 1):
        rect(ax, s * cab_x - t / 2, -gap_low - t + dz, t, z_cb + t - (-gap_low - t), ORANGE, ls, fill, 0.55)
        tip = stub_tip / math.sqrt(2)
        x0, x1 = sorted((s * tip, s * (cab_x - t / 2)))
        rect(ax, x0, -gap_low - t + dz, x1 - x0, t, ORANGE, ls, fill, 0.55)
        rect(ax, s * cab_x - t / 2, z_cb + dz, t, t, ORANGE, ls, fill)       # cross bar section
        if ls == "-":
            ax.plot([s * cab_x] * 2, [z_cb + t + dz, z_trav], color="k", lw=1)

def frame(ax, x_lo, x_hi):
    ax.plot([x_lo, x_hi], [z_trav, z_trav], color=GREY, lw=6, solid_capstyle="butt")
    ax.text(x_lo + 2, z_trav + 5, "traverse (cable housings fixed here, never on the arm)", fontsize=7)

fig = plt.figure(figsize=(15, 8.6))
af = fig.add_axes([0.03, 0.06, 0.33, 0.86]); asd = fig.add_axes([0.37, 0.06, 0.33, 0.86])
ap = fig.add_axes([0.72, 0.36, 0.27, 0.56]); at = fig.add_axes([0.72, 0.03, 0.27, 0.30]); at.axis("off")

# FRONT (x-z)
weight_side(af); weight_side(af, stroke - gap_low, "--", False)
m_front(af); l_front(af, 0); l_front(af, stroke, "--", False)
rect(af, -rider_w / 2, z_rider, rider_w, z_wire + 4 - z_rider, GREY, a=0.35)
rect(af, -7.5, z_tube, 15, tube_h, GREY, a=0.6)
af.text(0, z_tube + tube_h / 2, "arm\n(section)", ha="center", va="center", fontsize=7)
frame(af, -45, 45)
af.annotate("", (cab_x, z_trav - 8), (-cab_x, z_trav - 8), arrowprops=dict(arrowstyle="<->", lw=0.8))
af.text(0, z_trav - 14, "50", ha="center", fontsize=8)
af.set_title("Front: looking along the arm (y). Wire along x.", fontsize=10)

# SIDE (y-z): M's y rods stop at the ring; L rods at y = +-l_y, cross bar along y
ax = asd
weight_side(ax); weight_side(ax, stroke - gap_low, "--", False)
for s in (-1, 1):
    rect(ax, s * m_in if s > 0 else -m_in - t, -t, t, ring_z[1] + t, BLUE)
    rect(ax, s * stub_tip if s > 0 else -m_in, -t, m_in - stub_tip, t, BLUE)
rect(ax, -m_in - t, ring_z[0], 2 * (m_in + t), ring_z[1] - ring_z[0], BLUE, a=0.35)
rect(ax, -4, z_v - 10, 8, z_wire + 6 - z_v + 10, BLUE, a=0.5)              # x-posts seen end-on
for dz, ls, fl in ((0, "-", True), (stroke, "--", False)):
    for s in (-1, 1):
        rect(ax, s * l_y - t / 2, -gap_low - t + dz, t, z_cb + t + gap_low + t, ORANGE, ls, fl, 0.55)
    rect(ax, -l_y - t / 2, z_cb + dz, 2 * l_y + t, t, ORANGE, ls, fl)
ax.plot([0, 0], [z_cb + t, z_trav], color="k", lw=1)
rect(ax, -80, z_tube, 160, tube_h, GREY, a=0.6)
rect(ax, -28, z_rider, 56, z_tube - z_rider + tube_h + hole_h + 4, GREY, a=0.25)
ax.text(-78, z_tube + tube_h / 2, "arm -> cell", va="center", fontsize=7)
frame(ax, -80, 80)
ax.set_title("Side: across the arm (x). Dashed = cradle raised.", fontsize=10)

for a in (af, asd):
    a.set_aspect("equal"); a.set_ylim(-20, z_trav + 15); a.grid(alpha=0.2)
af.set_xlim(-60, 60); asd.set_xlim(-85, 85)

# PLAN from below
ap.add_patch(Circle((0, 0), body_r, fc="#e4e7ea", ec="#555", ls="--"))
ap.add_patch(Circle((0, 0), m_in + t, fc="none", ec=BLUE, lw=6, alpha=0.3))
for k in range(4):
    a = math.radians(90 * k)
    ap.plot([stub_tip * math.cos(a), (m_in + t) * math.cos(a)], [stub_tip * math.sin(a), (m_in + t) * math.sin(a)],
            color=BLUE, lw=8, solid_capstyle="butt")
for sx in (-1, 1):
    for sy in (-1, 1):
        tip = stub_tip / math.sqrt(2)
        ap.plot([sx * tip, sx * cab_x], [sy * tip, sy * l_y], color=ORANGE, lw=8, solid_capstyle="butt", alpha=0.8)
        ap.add_patch(Rectangle((sx * cab_x - t / 2, sy * l_y - t / 2), t, t, color=ORANGE))
    ap.plot([sx * cab_x] * 2, [-l_y, l_y], color=ORANGE, lw=3, ls=":")
    ap.add_patch(Circle((sx * cab_x, 0), 2, color="k"))
ap.plot([-60, 60], [0, 0], color="k", lw=0.6, ls="-.")
ap.text(48, 2, "wire", fontsize=7)
ap.set_aspect("equal"); ap.set_xlim(-60, 60); ap.set_ylim(-50, 50); ap.grid(alpha=0.2)
ap.set_title("Plan from below: blue M stubs 0/90/180/270,\norange L stubs 45 deg turned; dots = cables", fontsize=9)

at.text(0, 1, "\n".join([
    "Concept 2026-11-27 -- basic design, not a print part.",
    f"1000 g: neck 33, head 45 measured; body d {2*body_r:.0f}, body h {body_h:.0f},",
    f"   top {z_head:.0f} SCALED from 200 g x{K:.2f} -- TBD measure.",
    f"Stroke {stroke:.0f}: weight {stroke-gap_low:.0f} above M's stubs; lowered L {gap_low:.0f} below,",
    f"   clearance {clr:.0f} to M everywhere.",
    f"M posts {stroke+clr:.0f} taller than weight_clip.py (lifted head passes the rider);",
    "   L_cal unchanged (set by the wire hole).",
    "Load ~1.1 kg (cradle mass estimated): 2.8 kg cm at a 25 mm servo",
    "   horn; a standard 20 kg cm metal-gear servo (angle, not 360 deg).",
    "Open: rider screw heads vs. L cross bars; guide pin for L.",
]), va="top", fontsize=8, family="monospace")

fig.savefig("hardware/weight-lift-concept.pdf")
fig.savefig("hardware/weight-lift-concept.png", dpi=130)
print(f"z_head {z_head:.1f}  z_wire {z_wire:.1f}  z_cb {z_cb:.1f}  ring-to-L-rod {math.hypot(cab_x - t/2, l_y - t/2) - (m_in + t):.1f}")
