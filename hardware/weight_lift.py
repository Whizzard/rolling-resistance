# -*- coding: utf-8 -*-
"""
Automatic hanging of the 1000 g calibration weight (builder's idea 2026-11-27): three parts.
Concept drawing: weight_lift_concept.py (first sketch, superseded in the details).

HANGER -- the measuring part. Swings on the beam rider's wire like weight_clip.py.
    The weight stands with its flat bottom on a CROSS (arms along x and y, z = -cross_h .. 0).
    Four CENTRING FENCES at the arm ends (0/90/180/270 deg), `play` radial play, the inner
    edge a 45 deg lead-in over the full fence thickness (a ridge, no ledge to sit on). The x
    arms carry the posts: vertical outside the body, 45 deg inwards, vertical to the wire
    hole (as weight_clip.py, but taller -- the lifted head must pass under the rider legs).
CRADLE -- the lifting part, hangs on two strings, guided by two 3 mm steel wires.
    Four STUBS turned by 45 deg (45/135/225/315) pass between the hanger's arms and lift the
    weight by its bottom. On each stub a RAMP: from the seat at r = body/2 + play it rises at
    45 deg all the way out to the column (builder 2026-11-27: the weight must never sit on a
    ramp, it slides down into the seat). A flat RING ties the four stub ends together; the
    four COLUMNS rise from the stub ends (builder: "where the crosses end"). Per side the two
    columns run up, then inwards at 45 deg to a HEAD at x = +-guide_x: a vertical hole for
    the guide wire and, beside it at x = +-str_x (50 mm apart, in line with the pendulum
    wire), a horizontal hole for the string.
BASE -- hangs under the 40 x 40 profile (slot 8) of the traverse, which runs parallel above
    the arm (builder 2026-11-27): 2 x M6 button head along y in flat counterbores from below
    as the cups (weight_cup.py); slides along the arm to the rider's position. The guide wires
    pass the arm in front and behind (x = +-guide_x). Carries the two guide wires (hanging; vertical THROUGH holes, parallel to the M6
    screws, orthogonal to the servo shaft; outside the profile, so the wire goes in from
    above; M3 set screw across, horizontal) and per side a turning pin (3 mm steel, across y) over which the string
    turns from horizontal to vertical-down at x = +-str_x. The servo lies FLAT: shaft
    horizontal along y (parallel to the arm), body long side along x; its double horn in the
    x-z plane at y = 0, level along x at rest, centred between the wires. Shaft, horn and
    guide wires are three different axes (y, x, z). Turning the horn pulls both strings outwards by the same length (they leave the
    horn in opposite directions): no balance lever needed.

STATES (z = 0 = weight bottom on the hanger, model built in the measuring state):
    lowered  cradle gap_low below: the weight touches only the hanger, the cradle nothing.
    raised   cradle up by `stroke`: the weight sits in the cradle's ramps, its bottom
             stroke - gap_low above the hanger's cross. Either way the receiving ramps
             catch and centre the weight; it cannot wander sideways.
Clearances (distToShape) and overhangs (every planar face) are CHECKED and printed.

ASSUMED, check on the parts:
    1000 g weight: neck 33, head 45, head top 15 above neck centre measured 2026-11-24.
        BODY DIAMETER AND HEIGHT SCALED from the 200 g weight by (1000/200)^(1/3) -- TBD.
    Servo: MG996R class, typical listing dimensions (body 40.7 x 19.7, tabs 54.5, hole
        pitch 49.5 x 10, shaft 10 from the body end); horn plane to tab face TBD.
    Horn hole radius horn_r; string diameter str_d.
    Profile underside height above the arm: TBD; the script prints the minimum.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/weight_lift.py
Produces hardware/weight-lift.FCStd (assembly, raised and lowered cradle) and
weight-lift-hanger.step, weight-lift-cradle.step, weight-lift-base.step (STEP only).

Printing, all without supports: hanger standing on its cross, cradle standing on its ring,
base upside down (profile face on the bed). Only short bridges remain (listed by the check).
"""
import math
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')
import FreeCAD as App
import Part
from FreeCAD import Vector as V

K = (1000 / 200) ** (1 / 3)
P = dict(
    # weight
    body_d=round(28 * K, 1),          # SCALED, TBD
    body_h=round(34 * K, 1),          # SCALED, TBD
    neck_d=33.0, head_d=45.0,         # measured
    neck_l=round(8 * K, 1),           # SCALED
    head_above_neck=15.0,             # measured
    # centring
    play=0.4,          # radial play body to fence / ramp foot
    fence_t=3.0, fence_h=5.0,         # hanger fences
    ramp_v=1.0,        # vertical foot of the ramp before the 45 deg starts
    # hanger
    cross_w=8.0, cross_h=6.0,
    post_w=8.0, post_t=8.0,           # x, y (weight_clip.py arm_w, arm_t)
    clr=3.0,                          # minimum clearance anywhere
    rider_w=18.3, rider_below=12.0, tube_h=25.0, hole_h=15.0,   # beam_rider.py
    side_clear=1.0, wire_d=2.0, wire_fit=0.3,
    # cradle
    stroke=20.0, gap_low=9.0,
    stub_w=6.0, stub_h=6.0, stub_tip=16.0, ramp_w=8.0,
    ring_w=6.0, col=6.0,
    head_h=16.0, head_y=6.0,          # head height, half width in y
    guide_d=3.0, guide_fit=0.4,       # steel wire, sliding fit in the cradle
    str_x=25.0, str_d=1.0, str_hole=2.5,
    # base
    plate_t=8.0, prof_w=40.0,
    m6_d=6.6, m6_head_d=13.0, m6_head_h=4.0,             # as weight_cup.py
    m6_y=(10.0, 40.0),                # along the profile = along the arm (builder 2026-11-27),
                                      # both on the horn side: the servo body is not in the way
    pin_d=3.0, pin_fit=0.15,          # turning pin, steel 3 mm
    guide_press=0.0,                  # guide wire in the base: hole = wire, M3 set screw
    m3_tap=2.5,
    srv_L=40.7, srv_W=19.7, srv_tabs=54.5, srv_pitch_L=49.5, srv_pitch_W=10.0,
    srv_shaft=10.0,                   # shaft centre from the near body end
    srv_horn_tab=16.0,                # horn mid plane to tab face, TBD measure
    srv_top=10.0,                     # tab face to the servo's top face, TBD measure
    srv_below=27.0,                   # body below the tab face, TBD measure
    cheek_t_servo=2.0,                # pin cheek on the servo side
    srv_screw=2.4,                    # pilot for the servo's self-tapping screws
    srv_fit=0.4, wall_t=4.0,
    horn_r=14.0,                      # string hole radius on the horn, TBD
    base_gap=5.0,                     # base bottom above the raised cradle head
)
p = type('P', (), P)

# ---------------------------------------------------------------- derived
body_r = p.body_d / 2
z_neck0 = p.body_h; z_neck1 = p.body_h + p.neck_l
z_head = z_neck0 + p.neck_l / 2 + p.head_above_neck
r_fi = body_r + p.play
lift = p.stroke - p.gap_low
m_in = body_r + p.clr
x_top = p.rider_w / 2 + p.side_clear
z_v = z_head + lift + p.clr
z_bend = z_v + (m_in - x_top)
z_vo = z_v + p.post_w * (math.sqrt(2) - 1)
z_wire = z_v + p.rider_below + p.tube_h + p.hole_h
z_ptop = z_wire + 8
ring_ri = m_in + p.post_w + p.clr + 1.0
ring_rc = ring_ri + p.ring_w / 2
col_x = ring_rc / math.sqrt(2)               # columns on the stub ends, at 45 deg
guide_x = col_x
zs1 = -p.gap_low; zs0 = zs1 - p.stub_h
S2 = math.sqrt(2) - 1


def prism_xz(pts, y0, y1):
    w = Part.makePolygon([V(x, y0, z) for x, z in pts] + [V(pts[0][0], y0, pts[0][1])])
    return Part.Face(w).extrude(V(0, y1 - y0, 0))


def prism_yz(pts, x0, x1):
    w = Part.makePolygon([V(x0, y, z) for y, z in pts] + [V(x0, pts[0][0], pts[0][1])])
    return Part.Face(w).extrude(V(x1 - x0, 0, 0))


def box(x0, x1, y0, y1, z0, z1):
    return Part.makeBox(x1 - x0, y1 - y0, z1 - z0, V(x0, y0, z0))


def cyl(r, h, base, d):
    return Part.makeCylinder(r, h, base, d)


def radial(pts, w, ang):
    s = prism_xz(pts, -w / 2, w / 2)
    s.rotate(V(0, 0, 0), V(0, 0, 1), ang)
    return s


def weight(dz=0.0):
    s = Part.makeCylinder(body_r, p.body_h)
    s = s.fuse(Part.makeCylinder(p.neck_d / 2, p.neck_l, V(0, 0, z_neck0)))
    s = s.fuse(Part.makeCylinder(p.head_d / 2, z_head - z_neck1, V(0, 0, z_neck1)))
    s.translate(V(0, 0, dz))
    return s


# ---------------------------------------------------------------- hanger
def hanger():
    a = r_fi + p.fence_t
    s = box(-(m_in + p.post_w), m_in + p.post_w, -p.cross_w / 2, p.cross_w / 2, -p.cross_h, 0)
    s = s.fuse(box(-p.cross_w / 2, p.cross_w / 2, -a, a, -p.cross_h, 0))
    ri, ro, h = r_fi, r_fi + p.fence_t, p.fence_h
    for ang in (0, 90, 180, 270):    # ridge: lead-in over the whole thickness
        s = s.fuse(radial([(ri, 0), (ro, 0), (ro, h), (ri, h - p.fence_t)], p.cross_w, ang))
    post = [(m_in, -p.cross_h), (m_in + p.post_w, -p.cross_h), (m_in + p.post_w, z_vo),
            (x_top + p.post_w, z_vo + (m_in - x_top)), (x_top + p.post_w, z_ptop),
            (x_top, z_ptop), (x_top, z_bend), (m_in, z_v)]
    for sx in (1, -1):
        s = s.fuse(prism_xz([(sx * x, z) for x, z in post], -p.post_t / 2, p.post_t / 2))
    hole = cyl((p.wire_d + p.wire_fit) / 2, 2 * (x_top + p.post_w) + 10,
               V(-(x_top + p.post_w) - 5, 0, z_wire), V(1, 0, 0))
    return s.cut(hole).removeSplitter()


# ---------------------------------------------------------------- cradle
def cradle_side(sx, z_r):
    """Columns at (sx*col_x, +-col_x) up to z_r, legs at 45 deg in y-z to the head at y = 0."""
    c = p.col; yo = col_x + c / 2; yi = col_x - c / 2
    z_mo = z_r + (yo - p.head_y)              # outer leg edge reaches the head width
    z_top = z_mo + p.head_h
    outer = [(-yo, zs0), (-yo, z_r), (-p.head_y, z_mo), (-p.head_y, z_top), (p.head_y, z_top),
             (p.head_y, z_mo), (yo, z_r), (yo, zs0)]
    zi = z_r - c * S2                          # inner corner, leg c thick
    inner = [(-yi, zs0 - 1), (-yi, zi), (0, zi + yi), (yi, zi), (yi, zs0 - 1)]   # open at the bottom: no bar under the posts
    x0, x1 = sx * col_x - c / 2, sx * col_x + c / 2
    fr = prism_yz(outer, x0, x1).cut(prism_yz(inner, x0 - 1, x1 + 1))
    # head: from the frame outwards (guide side) and inwards (string side), 45 deg undersides
    xin = sx * (p.str_x - p.str_hole / 2 - 3.0)      # 3 mm wall inside the string hole
    xout = sx * (guide_x + (p.guide_d + p.guide_fit) / 2 + 3.0)
    xa, xb = (x0, x1) if sx > 0 else (x1, x0)        # frame inner / outer edge (|x|)
    prof = [(xin, z_mo + abs(xa - xin)), (xa, z_mo), (xb, z_mo), (xout, z_mo + abs(xout - xb)),
            (xout, z_top), (xin, z_top)]
    fr = fr.fuse(prism_xz(prof, -p.head_y, p.head_y))
    gh = cyl((p.guide_d + p.guide_fit) / 2, p.head_h + 40, V(sx * guide_x, 0, z_mo - 20), V(0, 0, 1))
    sh = cyl(p.str_hole / 2, 2 * p.head_y + 2, V(sx * p.str_x, -p.head_y - 1, z_top - 5), V(0, 1, 0))
    return fr.cut(gh).cut(sh), z_mo, z_top


def cradle(z_r):
    s = Part.makeCylinder(ring_ri + p.ring_w, p.stub_h, V(0, 0, zs0)).cut(
        Part.makeCylinder(ring_ri, p.stub_h, V(0, 0, zs0)))
    for ang in (45, 135, 225, 315):
        st = box(p.stub_tip, r_fi, -p.stub_w / 2, p.stub_w / 2, zs0, zs1)
        st = st.fuse(box(r_fi - 0.01, ring_ri + 1, -p.ramp_w / 2, p.ramp_w / 2, zs0, zs1))
        st.rotate(V(0, 0, 0), V(0, 0, 1), ang)
        re = ring_rc
        ramp = radial([(r_fi, zs1 - 0.01), (re, zs1 - 0.01), (re, zs1 + p.ramp_v + re - r_fi),
                       (r_fi, zs1 + p.ramp_v)], p.ramp_w, ang)
        s = s.fuse(st).fuse(ramp)
    tops = []
    for sx in (1, -1):
        side, z_mo, z_top = cradle_side(sx, z_r)
        s = s.fuse(side); tops.append((z_mo, z_top))
    return s.removeSplitter(), tops[0]


# ---------------------------------------------------------------- base
# Own frame: z = 0 profile underside, base hangs below. Horn plane y = 0.
def base_layout():
    # Servo FLAT (builder 2026-11-27): shaft horizontal along y (parallel to the arm), body
    # long side along x, short side W vertical; the horn lies level along x at rest. Three
    # different axes: shaft y, horn x, guide wires z.
    z_arm = -p.plate_t - max(p.horn_r + 3, p.srv_W / 2) - 3.0   # horn sweep clears the plate
    z_pin = z_arm - (p.pin_d / 2 + p.str_d / 2)         # string leaves the horn level, tangent on the pin top
    x_pin = p.str_x - (p.pin_d / 2 + p.str_d / 2)        # string drops at x = str_x
    z_bot = z_pin - 6.0
    y_tab = -p.srv_horn_tab
    xb0 = -p.srv_shaft; xb1 = xb0 + p.srv_L             # body ends along x, shaft at x = 0
    xt = (p.srv_tabs - p.srv_L) / 2
    return dict(z_arm=z_arm, z_pin=z_pin, x_pin=x_pin, z_bot=z_bot, y_tab=y_tab,
                xb0=xb0, xb1=xb1, xc=(xb0 + xb1) / 2, xt0=xb0 - xt, xt1=xb1 + xt,
                y_top=y_tab + p.srv_top)


def base():
    L = base_layout()
    z_arm, z_pin, x_pin, z_bot, y_tab = L['z_arm'], L['z_pin'], L['x_pin'], L['z_bot'], L['y_tab']
    # The profile runs along the arm (y). The plate covers the guide blocks and the servo
    # frame in x (so nothing overhangs when printed upside down) and reaches along y from
    # the servo frame to beyond the second M6.
    xw0, xw1 = L['xt0'] - 3.0, L['xt1'] + 3.0           # servo frame in x
    xg = guide_x + p.guide_d / 2 + 3.0
    xe0, xe1 = min(-xg, xw0), max(xg, xw1)
    y0 = y_tab - p.wall_t
    y1 = max(p.m6_y) + p.m6_head_d / 2 + 3
    s = box(xe0, xe1, y0, y1, -p.plate_t, 0)
    for my in p.m6_y:                                    # M6 in the slot, flat counterbore from below
        s = s.cut(cyl(p.m6_d / 2, p.plate_t + 2, V(0, my, -p.plate_t - 1), V(0, 0, 1)))
        s = s.cut(cyl(p.m6_head_d / 2, p.m6_head_h + 0.01, V(0, my, -p.plate_t - 0.01), V(0, 0, 1)))
    # side blocks: guide wire + turning pin. On -y only a thin cheek: the servo's top face
    # is close behind the horn plane.
    yb0 = -(2 + p.cheek_t_servo)
    for sx in (1, -1):
        xa = x_pin - p.pin_d / 2 - 2.5
        x0, x1 = sorted((sx * xa, sx * xg))
        blk = box(x0, x1, yb0, 6, z_bot, -p.plate_t + 0.01)
        sx0, sx1 = sorted((sx * xa - sx * 1, sx * (p.str_x + p.str_d / 2 + 2.5)))
        blk = blk.cut(box(sx0, sx1, -2, 2, z_bot - 1, z_arm + 8))          # string slot
        blk = blk.cut(cyl((p.pin_d + p.pin_fit) / 2, 14, V(sx * x_pin, -7, z_pin), V(0, 1, 0)))
        blk = blk.cut(cyl((p.guide_d + p.guide_press) / 2, -z_bot + 2,
                          V(sx * guide_x, 0, z_bot - 1), V(0, 0, 1)))
        blk = blk.cut(cyl(p.m3_tap / 2, 7, V(sx * guide_x, 0, z_bot + 6), V(0, 1, 0)))   # set screw from +y
        s = s.fuse(blk)
        s = s.cut(cyl((p.guide_d + p.guide_press) / 2, -z_bot + 2, V(sx * guide_x, 0, z_bot - 1), V(0, 0, 1)))   # guide wire: through, vertical
    # servo frame: wall in x-z at the tab plane, open at the bottom (no bridge when printed,
    # the servo goes in from below); the tabs rest on its +y face, 4 screws.
    wz = p.srv_W / 2 + p.srv_fit
    zw_bot = z_arm - p.srv_pitch_W / 2 - 5.0
    wall = box(xw0, xw1, y0, y_tab, zw_bot, -p.plate_t + 0.01)
    wall = wall.cut(box(L['xb0'] - p.srv_fit, L['xb1'] + p.srv_fit, y0 - 1, y_tab + 1, zw_bot - 1, z_arm + wz))
    for xh in (L['xc'] - p.srv_pitch_L / 2, L['xc'] + p.srv_pitch_L / 2):
        for zh in (z_arm - p.srv_pitch_W / 2, z_arm + p.srv_pitch_W / 2):
            wall = wall.cut(cyl(p.srv_screw / 2, p.wall_t + 2, V(xh, y0 - 1, zh), V(0, 1, 0)))
    s = s.fuse(wall)
    return s.removeSplitter(), L


def servo_dummy(L):
    """Envelope of the flat servo: body, tabs, and the horn disc swept by the arm (for the checks)."""
    y_tab, z = L['y_tab'], L['z_arm']
    b = box(L['xb0'], L['xb1'], y_tab - p.srv_below, L['y_top'], z - p.srv_W / 2, z + p.srv_W / 2)
    t = box(L['xt0'], L['xt1'], y_tab, y_tab + 2.5, z - p.srv_W / 2, z + p.srv_W / 2)
    horn = cyl(p.horn_r + 3, 4, V(0, -2, z), V(0, 1, 0))
    return b.fuse(t), horn


# ---------------------------------------------------------------- checks
def dist(a, b):
    c = a.common(b).Volume
    return -c if c > 1e-6 else a.distToShape(b)[0]


def overhangs(sh, flip=False, bed=None, limit=45.0):
    """Planar faces facing downwards steeper than `limit` from vertical, not on the bed.
    Returns (area, z, normal z) list. Bridges show up here too (normal z = -1)."""
    out = []
    zmin = sh.BoundBox.ZMin if not flip else -sh.BoundBox.ZMax
    for f in sh.Faces:
        if f.Surface.TypeId != 'Part::GeomPlane':
            continue
        u0, u1, v0, v1 = f.ParameterRange
        n = f.normalAt((u0 + u1) / 2, (v0 + v1) / 2)
        nz = -n.z if flip else n.z
        zc = -f.CenterOfMass.z if flip else f.CenterOfMass.z
        if nz < -math.cos(math.radians(limit)) - 1e-3 and abs(zc - zmin) > 0.05:
            out.append((f.Area, zc - zmin, nz))
    return out


H = hanger()
# Lowest column height where the lowered cradle clears the hanger by clr.
# Lowest column height where the lowered cradle's sides clear the hanger by clr (bisection on
# one side only; the sides are the only parts of the cradle that reach the posts).
lo, hi = z_v - 40, z_v + 40
while hi - lo > 0.25:
    mid = (lo + hi) / 2
    if cradle_side(1, mid)[0].distToShape(H)[0] >= p.clr:
        hi = mid
    else:
        lo = mid
z_r = hi
L, (z_mo, z_ctop) = cradle(z_r)
B, BL = base()
srv, horn = servo_dummy(BL)
z_base = z_ctop + p.stroke + p.base_gap - BL['z_bot']      # profile underside in the stand frame
for o in (B, srv, horn):
    o.translate(V(0, 0, z_base))
# guide wires: from the base down to 3 mm above the lowered head's hole bottom
wires = []
for sx in (1, -1):
    w0 = z_mo + 3
    wires.append(cyl(p.guide_d / 2, z_base - p.plate_t - 4 - w0, V(sx * guide_x, 0, w0), V(0, 0, 1)))
Lr = L.copy(); Lr.translate(V(0, 0, p.stroke))
W0, W1 = weight(0), weight(lift)
cutw = lambda w: w  # wires slide in the cradle with guide_fit: checked separately below

checks = [
    ("lowered: cradle - hanger", dist(L, H)),
    ("lowered: cradle - weight", dist(L, W0)),
    ("raised:  cradle - hanger", dist(Lr, H)),
    ("raised:  weight - hanger", dist(W1, H)),
    ("raised:  cradle - base", dist(Lr, B)),
    ("raised:  cradle - servo/horn", min(dist(Lr, srv), dist(Lr, horn))),
    ("guide wires - hanger", min(dist(w, H) for w in wires)),
    ("horn sweep - base", dist(horn, B)),
    ("servo body - base (0 = seated)", dist(srv, B)),
    ("guide wires - servo", min(dist(w, srv) for w in wires)),
    ("servo body - pin cheeks", dist(srv, B.common(
        box(-200, 200, -(2 + p.cheek_t_servo) - 0.01, 6, z_base - 500, z_base + 500)))),
]
sw = 0.0
while sw < 10:
    Hs = H.copy(); Hs.rotate(V(0, 0, z_wire), V(1, 0, 0), sw + 0.1)
    if Hs.distToShape(L)[0] <= 0.2:
        break
    sw += 0.1
checks.append(("hanger swing allowed (deg)", sw))

# string pull over the horn angle (horn level at 0 deg)
def pull(theta):
    r = p.horn_r; t = math.radians(theta)
    d0 = BL['x_pin'] - r
    return math.hypot(BL['x_pin'] - r * math.cos(t), r * math.sin(t)) - d0

out = os.path.dirname(os.path.abspath(__file__))
doc = App.newDocument('WeightLift')
sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
for i, (k, v) in enumerate(sorted(P.items()), start=1):
    sh.set('A%d' % i, k); sh.set('B%d' % i, str(v))
for name, shape in (('Hanger', H), ('Cradle_lowered', L), ('Cradle_raised', Lr), ('Base', B),
                    ('Servo_envelope', srv), ('Horn_disc', horn), ('Weight', W0),
                    ('Guide_wire_R', wires[0]), ('Guide_wire_L', wires[1])):
    o = doc.addObject('Part::Feature', name); o.Shape = shape
doc.recompute()
for n in ('Cradle_raised', 'Servo_envelope', 'Horn_disc'):
    try:
        doc.getObject(n).ViewObject.Visibility = False
    except Exception:
        pass
doc.saveAs(os.path.join(out, 'weight-lift.FCStd'))
H.exportStep(os.path.join(out, 'weight-lift-hanger.step'))
L.exportStep(os.path.join(out, 'weight-lift-cradle.step'))
Bp = B.copy(); Bp.translate(V(0, 0, -z_base))
Bp.exportStep(os.path.join(out, 'weight-lift-base.step'))

print("weight: body d %.1f h %.1f, head top %.1f (scaled, TBD)" % (p.body_d, p.body_h, z_head))
print("pendulum wire z %.1f; cradle columns at +-%.1f, head z %.1f..%.1f (lowered)"
      % (z_wire, col_x, z_mo, z_ctop))
print("profile underside at least %.0f mm above the pendulum wire" % (z_base - z_wire))
print("guide wires 3 mm at x +-%.1f, length %.0f below the base plate (+ %.0f in the hole)"
      % (guide_x, z_base - p.plate_t - (z_mo + 3) - 0, -p.plate_t - BL['z_bot'] - 4))
print("string pull at horn r %.0f: 90 deg %.1f mm, 120 deg %.1f mm (stroke %.0f)"
      % (p.horn_r, pull(90), pull(120), p.stroke))
for n, v in checks:
    print("%-34s %7.2f" % (n, v))
for name, shp, flip in (('hanger', H, False), ('cradle', L, False), ('base (upside down)', Bp, True)):
    oh = overhangs(shp, flip)
    print("overhangs %-20s %s" % (name, "none" if not oh else
          "; ".join("%.0f mm2 at z %.1f (nz %.2f)" % o for o in sorted(oh, reverse=True)[:6])))
print("solid PETG: hanger %.0f g, cradle %.0f g, base %.0f g"
      % (H.Volume * 1.27e-3, L.Volume * 1.27e-3, B.Volume * 1.27e-3))
