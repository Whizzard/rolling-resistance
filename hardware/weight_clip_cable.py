# -*- coding: utf-8 -*-
"""
Cable clip for the calibration weights: version of weight_clip_dual.py that hangs
straight from the Bowden cable of the load cell, for calibrating the cell directly with
the arm lifted (docs/06-open-questions.md C6; builder 2026-11-28). Carries EITHER a
500 g / 1000 g weight in the lower seat OR a 200 g weight on three claws above it.
Printable standing on the collar, no supports.

CHANGES TO weight_clip_dual.py (builder 2026-11-28):
  - TOP: no rider, no wire pivot. The side posts rise above the 200 g head, bend inwards
    at 45 degrees and meet in a short horizontal ROOF with a vertical hole for the cable
    (hole_d). Under the roof stays clamp_space of free height above the 200 g head for
    the cable end and its clamp; the clamp bears on the flat underside of the roof
    (a short bridge, roof_w_in wide).
  - LOWER COLLAR one millimetre wider (coll_loose): the dual clip held the 1000 g too tightly.
  - CLAWS one millimetre tighter: tip circle neck2_d + fit - claw_tight in diameter (the
    first print held the 200 g weight too loosely).
  - UPPER SEAT LOWER: the claws sit as low as the 1000 g head allows. Criterion: the
    branch underside stays claw_clear above the head top wherever it lies over the head,
    at rest or on its way in from +y (before: where it met the column).

LOWER SEAT, webs and side posts are those of weight_clip.py (1000 g weight measured
2026-11-24; the 500 g head fits the same, builder 2026-11-26).

UPPER SEAT (builder's design 2026-11-26, front noses moved onto the side posts
2026-11-28): two branches leave the side posts and one rises from a column at the back;
they run up at 45 degrees inwards (the front ones slightly forwards, to the opening) over
the 1000 g head and end in three separate CLAWS that reach into the 200 g neck groove.
No ring at the top: the 200 g weight hangs on three points (front right, front left,
back). Each arch: a web on the bed from the collar, a vertical column outside the 1000 g
head, then a straight branch rising inwards at exactly 45 degrees to the claw. The claw
is a nose: tip on r = x_i2 (groove bottom + play), 45 degree flanks. The branch underside
IS the extended lower flank of the nose -- one line from the column to the tip.
Inside corners are filleted (the fillet surfaces lie between 45 degrees and vertical),
the outer corners rounded.

SNAP: the front claws stand apart by gap2 = neck2_d * gap_w / neck_d, the same ratio
as the lower collar (30 / 33). The head and the neck push them apart when the weight is
slid in from +y; the front arches flex outwards and spring back into the groove. The
generator computes the spread each front claw needs to let head, neck and body pass and
the strain at the arch root (PETG).

200 g weight, builder's measurements with calipers 2026-11-26: neck Ø 21 (round groove,
about 8 long), head Ø 27, body Ø 28, overall height 46, neck centre to top edge 8.
ASSUMPTION (not measured): the groove is a semicircle of radius neck2_len / 2 = 4, bottom
at the neck Ø, centre at r = 14.5.

POSITION: z = 0 underside of the collar (print bed), posts and roof in the xz plane,
cable hole on the z axis. Weights go in from +y.

LOAD LINE: the clip hangs from one point on the cable, so clip and weight settle with
their common centre of gravity under it; the cable carries m * g plus the clip, whatever
the tilt. All seats are centred on the hole axis.

The geometry is built with Part solids (booleans), not as a sketch-based PartDesign
tree: the claws lie in inclined planes and are trimmed by revolved envelopes. Change
dimensions in PARAMS below and rerun; the spreadsheet "Parameter" in the FCStd only
documents the values used.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/weight_clip_cable.py
Produces hardware/weight-clip-cable.FCStd, .step.
"""

import math
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
from FreeCAD import Vector as V

P = {}
PARAMS = [
    # 1000 g weight, measured 2026-11-24 (as weight_clip.py); 500 g head fits the same
    ('neck_d',   33.0, 'Neck diameter 1000 g, measured'),
    ('head_d',   45.0, 'Head diameter 1000 g, measured'),
    ('head_h',   11.0, 'Top of collar to top of head, builder 2026-11-28 (was 15, generous)'),
    ('head_round', 11.0, 'Rounding of the head top edge: "completely rounded" (builder 2026-11-28); radius ASSUMED = head_h'),
    # 200 g weight, builder's caliper measurements 2026-11-26
    ('neck2_d',  21.0, '200 g neck (groove bottom), measured'),
    ('neck2_len', 8.0, '200 g groove length, measured "about 8"; semicircle ASSUMED'),
    ('head2_d',  27.0, '200 g head, measured'),
    ('head2_top', 8.0, '200 g neck centre to top edge, measured'),
    ('body2_d',  28.0, '200 g body, measured'),
    ('total2_h', 46.0, '200 g overall height, measured'),
    # Lower seat (weight_clip.py)
    ('fit',       0.6, 'Play at the neck, TOTAL (both seats)'),
    ('coll_loose', 1.0, 'Lower collar this much larger in diameter (builder 2026-11-28: dual clip too tight)'),
    ('claw_tight', 1.0, 'Claw tip circle this much smaller in diameter (builder 2026-11-28: dual clip too loose)'),
    ('ring_t',    4.0, 'Collar radial thickness'),
    ('coll_h',    8.0, 'Collar height'),
    ('gap_w',    30.0, 'Collar opening (smaller than the neck: snaps)'),
    ('head_clear', 1.5, 'Clearance 1000 g head to posts and columns'),
    ('top_clear',  3.0, 'Clearance above the 200 g head to the post bend'),
    ('claw_clear', 2.0, 'Clearance 1000 g head top to the branch underside, builder 2026-11-28'),
    ('arm_w',     8.0, 'Side post width (x)'),
    ('arm_t',     8.0, 'Side post thickness (y)'),
    # Arches and claws
    ('nose_a',    0.5, 'Claw tip half height'),
    ('nose_c',    2.5, 'Claw upper flank run (45 degrees)'),
    ('nose_above', 20.0, 'Nose underside above the top of the lower collar, builder 2026-11-28'),
    ('top_flat',  5.0, 'Claw top flat from the tip; sets the branch thickness'),
    ('col_t',     6.0, 'Arch column and branch width (square column)'),
    ('front_c',  (28.0, 0.5), 'Front branch root (x, y) inside the side post, mirrored for the left (builder 2026-11-28: noses on the hanger)'),
    ('back_c',   (0.0, -27.0), 'Back column centre (x, y)'),
    ('fillet_in', 4.0, 'Fillet radius column inside to branch underside'),
    ('round_out', 3.0, 'Rounding of the outer corners'),
    # Roof and cable (builder 2026-11-28)
    ('clamp_space', 15.0, 'Free height 200 g head top to roof underside; the 45 degree roof adds room beside it (builder 2026-11-28: 1-2 cm less than 30)'),
    ('roof_w_in',  8.0, 'Flat roof underside (bridge, clamp seat), ESTIMATED'),
    ('roof_t',     6.0, 'Roof thickness, ESTIMATED'),
    ('hole_d',     2.0, 'Cable hole, builder 2026-11-28'),
    ('rho_petg',  1.27, 'PETG density g/cm3, typical datasheet value (solid)'),
]


def derive():
    for n, v, _ in PARAMS: P[n] = v
    P['r_in'] = (P['neck_d'] + P['fit'] + P['coll_loose']) / 2
    P['r_out'] = P['r_in'] + P['ring_t']
    P['post_x'] = P['head_d'] / 2 + P['head_clear']
    P['post_xo'] = P['post_x'] + P['arm_w']
    P['z_head'] = P['coll_h'] + P['head_h']
    P['x_i2'] = (P['neck2_d'] + P['fit'] - P['claw_tight']) / 2  # claw tip radius
    P['gap2'] = P['neck2_d'] * P['gap_w'] / P['neck_d']          # snap ratio of the lower collar
    P['th_front'] = math.degrees(math.acos(P['gap2'] / 2 / P['x_i2']))   # front claw tip angle from +x
    # Claw tips and arch lengths; neck centre height from "branch underside stays
    # claw_clear above the 1000 g head wherever it lies over the head or its path in"
    arches = []
    for name, th, c in (('Arch_FR', P['th_front'], P['front_c']),
                        ('Arch_FL', 180 - P['th_front'], (-P['front_c'][0], P['front_c'][1])),
                        ('Arch_B', 270.0, P['back_c'])):
        T = V(P['x_i2'] * math.cos(math.radians(th)), P['x_i2'] * math.sin(math.radians(th)), 0)
        C = V(c[0], c[1], 0)
        L = (C - T).Length
        arches.append({'name': name, 'th': th, 'T': T, 'C': C, 'L': L})
    zc = P['z_head'] + P['claw_clear']
    R = P['head_d'] / 2 + 0.5
    def over_head(p):
        return math.hypot(p.x, p.y) <= R or (abs(p.x) <= R and p.y >= 0)
    zn = 0.0
    for a in arches:
        d = a['C'] - a['T']; d.normalize()
        u_max = max(0.05 * k for k in range(int((a['L'] - P['col_t'] / 2) / 0.05) + 1)
                    if over_head(a['T'] + d * (0.05 * k)))
        zn = max(zn, zc + P['nose_a'] + u_max)
    P['z_n2_head'] = math.ceil(zn * 2) / 2            # lowest that clears a full Ø 45 x head_h head
    P['z_n2'] = P['coll_h'] + P['nose_above'] + P['nose_a']   # builder's choice
    P['arches'] = arches
    P['z_head2'] = P['z_n2'] + P['head2_top']
    # Roof: inner 45 degree lines from the post insides (x = +-post_x, z_v) to the flat
    # underside at z_ru, half width roof_w_in / 2; outer lines parallel, arm_w further out
    P['z_ru'] = P['z_head2'] + P['clamp_space']
    P['z_v'] = P['z_ru'] - (P['post_x'] - P['roof_w_in'] / 2)
    P['z_ro'] = P['z_ru'] + P['roof_t']
    P['x_ro'] = P['post_xo'] + P['z_v'] - P['z_ro']          # half width of the flat top
    P['z_top'] = P['z_ro']


# ------------------------------------------------------------------ geometry helpers

def poly_face_xz(pts):
    w = Part.makePolygon([V(x, 0, z) for x, z in pts] + [V(pts[0][0], 0, pts[0][1])])
    return Part.Face(w)


def revolve(pts):
    return poly_face_xz(pts).revolve(V(0, 0, 0), V(0, 0, 1), 360)


def box(x0, x1, y0, y1, z0, z1):
    return Part.makeBox(x1 - x0, y1 - y0, z1 - z0, V(x0, y0, z0))


def fillet_corner_2d(pts, i, r):
    """Replace polygon corner i by an arc of radius r (2D, (u, z) tuples); returns a list
    of edges-to-be: we approximate the arc with points (fine, 8 segments per arc)."""
    p0, p1, p2 = V(*pts[i - 1], 0), V(*pts[i], 0), V(*pts[(i + 1) % len(pts)], 0)
    a = (p0 - p1); a.normalize(); b = (p2 - p1); b.normalize()
    ang = math.acos(max(-1.0, min(1.0, a.dot(b))))
    t = r / math.tan(ang / 2)
    s, e = p1 + a * t, p1 + b * t
    bis = a + b; bis.normalize()
    ctr = p1 + bis * (r / math.sin(ang / 2))
    a0 = math.atan2(s.y - ctr.y, s.x - ctr.x); a1 = math.atan2(e.y - ctr.y, e.x - ctr.x)
    da = a1 - a0
    while da > math.pi: da -= 2 * math.pi
    while da < -math.pi: da += 2 * math.pi
    n = 10
    return [(ctr.x + r * math.cos(a0 + da * k / n), ctr.y + r * math.sin(a0 + da * k / n)) for k in range(n + 1)]


def rounded(pts, radii):
    """Polygon with selected corners rounded: radii = {index: r}."""
    out = []
    for i in range(len(pts)):
        if i in radii: out += fillet_corner_2d(pts, i, radii[i])
        else: out.append(pts[i])
    return out


def arch_solid(a):
    """One arch: outline in the arch plane (u from the claw tip towards the column, z up),
    extruded col_t wide, placed through the tip T towards the column centre C."""
    zn, na, nc = P['z_n2'], P['nose_a'], P['nose_c']
    ct, L = P['col_t'], a['L']
    u_ci, u_co = L - ct / 2, L + ct / 2
    z_ci = zn - na - u_ci                         # branch underside meets the column inside
    u1 = P['top_flat']
    z_co = zn + 3.0 - (u_co - u1)                 # top line, 45 degrees down outwards
    # web from the collar (r_out - 1) to the column; u of the collar in this plane
    u_w0 = L - (a['C'].Length - (P['r_out'] - 1))
    ch = P['coll_h']
    pts = [(0, zn - na), (0, zn + na), (nc, zn + na + nc), (u1, zn + na + nc), (u_co, z_co),
           (u_co, 0), (u_w0, 0), (u_w0, ch), (u_ci, ch), (u_ci, z_ci)]
    pts = rounded(pts, {3: P['round_out'], 4: P['round_out'], 9: P['fillet_in']})   # no fillet on the web top: the 1000 g head sits there
    f = Part.Face(Part.makePolygon([V(u, 0, z) for u, z in pts] + [V(pts[0][0], 0, pts[0][1])]))
    s = f.extrude(V(0, ct, 0)); s.translate(V(0, -ct / 2, 0))
    d = a['C'] - a['T']; ang = math.degrees(math.atan2(d.y, d.x))
    s.rotate(V(0, 0, 0), V(0, 0, 1), ang)
    s.translate(a['T'])
    a.update({'z_ci': z_ci, 'u_ci': u_ci, 'u_co': u_co, 'z_co': z_co})
    return s


def envelope():
    """Where claw material may be at rest: r >= x_i2, and inside r = 14.5 only within the
    nose envelope (head above, body below)."""
    zn, na = P['z_n2'], P['nose_a']
    r_b = P['body2_d'] / 2 + 0.5
    big = 400.0
    outer = Part.makeCylinder(80, big, V(0, 0, -100)).cut(Part.makeCylinder(r_b, big, V(0, 0, -100)))
    nose = revolve([(P['x_i2'], zn - na), (P['x_i2'], zn + na), (P['x_i2'] + P['nose_c'], zn + na + P['nose_c']),
                    (r_b + 0.01, zn + na + P['nose_c']), (r_b + 0.01, zn - na - (r_b + 0.01 - P['x_i2']))])
    return outer.fuse(nose)


def build(doc):
    zn = P['z_n2']
    r_in, r_out, ch = P['r_in'], P['r_out'], P['coll_h']
    px, pxo, at, aw = P['post_x'], P['post_xo'], P['arm_t'], P['arm_w']
    # Lower seat: collar, side webs (weight_clip.py)
    collar = Part.makeCylinder(r_out, ch)
    webs = box(r_out - 1, pxo, -at / 2, at / 2, 0, ch).fuse(box(-pxo, -(r_out - 1), -at / 2, at / 2, 0, ch))
    # Side posts, 45 degree bends inwards, flat roof (one closed outline in xz)
    z0, zv, zru, zro, xri, xro = ch - 1, P['z_v'], P['z_ru'], P['z_ro'], P['roof_w_in'] / 2, P['x_ro']
    if P['z_v'] + P['post_x'] - P['head2_d'] / 2 < P['z_head2'] + P['top_clear']:
        raise ValueError('inner roof line closer than top_clear to the 200 g head edge')
    pts = [(px, z0), (pxo, z0), (pxo, zv), (xro, zro), (-xro, zro), (-pxo, zv), (-pxo, z0), (-px, z0),
           (-px, zv), (-xri, zru), (xri, zru), (px, zv)]
    posts = poly_face_xz(pts).extrude(V(0, at, 0)); posts.translate(V(0, -at / 2, 0))
    # Arches with claws, trimmed to the envelope; front claws also to |x| >= gap2/2
    env = envelope()
    arches = []
    for a in P['arches']:
        s = arch_solid(a).common(env)
        if a['name'] != 'Arch_B':
            s = s.cut(box(-P['gap2'] / 2, P['gap2'] / 2, 0, 60, 0, 200))
        arches.append((a['name'], s))
    shape = collar.fuse(webs).fuse(posts)
    for _, s in arches: shape = shape.fuse(s)
    # Recut the lower seat: neck space and opening
    shape = shape.cut(Part.makeCylinder(r_in, ch + 2, V(0, 0, -1)))
    shape = shape.cut(box(-P['gap_w'] / 2, P['gap_w'] / 2, 0, r_out + 2, -1, ch + 0.001))
    # Cable hole, vertical through the roof
    shape = shape.cut(Part.makeCylinder(P['hole_d'] / 2, P['roof_t'] + 10, V(0, 0, P['z_ru'] - 5)))
    shape = shape.removeSplitter()
    for name, s in arches:
        o = doc.addObject('Part::Feature', name); o.Shape = s; o.Visibility = False
    o = doc.addObject('Part::Feature', 'WeightClipCable'); o.Shape = shape
    return o


# ------------------------------------------------------------------ checks

def groove(r):
    R = P['neck2_len'] / 2; c = P['neck2_d'] / 2 + R; d = c - r
    return math.sqrt(max(R * R - d * d, 0.0)) if abs(d) <= R else 0.0


def weight200_inside(p, yc):
    """True if point p (clip coordinates) lies inside the 200 g weight centred at (0, yc)."""
    zn = P['z_n2']; r = math.hypot(p.x, p.y - yc); dz = p.z - zn
    rn, rh, rb = P['neck2_d'] / 2, P['head2_d'] / 2, P['body2_d'] / 2
    top, bot = P['head2_top'], P['total2_h'] - P['head2_top']
    if dz > top or dz < -bot: return False
    if r <= rn: return True
    if dz >= 0: return r <= rh and dz >= groove(r)
    return r <= rb and -dz >= groove(r)


def check(obj):
    s = obj.Shape; ok = True; msgs = []
    inside = lambda x, y, z: s.isInside(V(x, y, z), 1e-6, True)

    def bad(m):
        nonlocal ok
        ok = False
        if m not in msgs: msgs.append(m); print('ERROR ' + m)

    ch, r_in = P['coll_h'], P['r_in']
    if inside(0, 0, ch / 2): bad('neck not clear')
    if not inside(r_in + 1, 0, ch / 2) or not inside(0, -(r_in + 1), ch / 2): bad('collar missing')
    if inside(0, r_in + 1, ch / 2): bad('opening missing')
    # 1000 g head at rest and on its way in (+y), with top_clear above
    R1 = P['head_d'] / 2 - 0.2
    def head_top(r):
        rr, r0 = P['head_round'], P['head_d'] / 2 - P['head_round']
        if r <= r0: return P['z_head']
        return P['z_head'] - rr + math.sqrt(max(rr * rr - (r - r0) ** 2, 0.0))
    for yc in (0, 8, 16, 25, 40):
        for ang in range(0, 360, 10):
            for rr in (R1, R1 * 0.85, R1 * 0.7, R1 * 0.5, R1 * 0.35):
                x, y = rr * math.cos(math.radians(ang)), yc + rr * math.sin(math.radians(ang))
                zt = head_top(rr)
                for z in (ch + 0.5, max(ch + 0.5, zt - 0.5), zt + P['claw_clear'] - 0.3):
                    if inside(x, y, z): bad('1000 g head blocked at y_c=%g (%.1f, %.1f, %.1f)' % (yc, x, y, z))
    # 200 g at rest: no material inside the weight (sampled on a grid around the seat)
    zn = P['z_n2']
    pts = []
    for ang in range(0, 360, 5):
        ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))
        for rr in [P['neck2_d'] / 2 + 0.05 + 0.25 * k for k in range(16)]:
            for dz in [-0.1 * k for k in range(0, 45)] + [0.1 * k for k in range(1, 45)]:
                pts.append(V(rr * ca, rr * sa, zn + dz))
    # the claws are claw_tight smaller on purpose: test each point shifted outwards by that
    # preload, i.e. against the claws at nominal size
    def nominal(p):
        r = math.hypot(p.x, p.y); k = (r + P['claw_tight'] / 2) / r
        return V(p.x * k, p.y * k, p.z)
    hits = [p for p in pts if weight200_inside(nominal(p), 0.0) and s.isInside(p, 1e-6, True)]
    if hits: bad('claws cut into the 200 g weight at rest beyond the preload (%d points, e.g. %s)' % (len(hits), hits[0]))
    # body hanging down through the lower collar (Ø 28, down to z_head2 - total2_h)
    rb = P['body2_d'] / 2 + 0.2 - P['claw_tight'] / 2   # claws pushed out by the preload, as above
    zb = P['z_head2'] - P['total2_h']
    for ang in range(0, 360, 10):
        for z in (zb + 0.3, (zb + zn) / 2, zn - groove(P['body2_d'] / 2) - 0.1):
            if inside(rb * math.cos(math.radians(ang)), rb * math.sin(math.radians(ang)), z): bad('200 g body blocked at z=%.1f' % z)
    # Claws hold: three contact points, all angular gaps < 180 degrees
    ths = sorted(a['th'] % 360 for a in P['arches'])
    gaps = [(ths[(i + 1) % 3] - ths[i]) % 360 for i in range(3)]
    if max(gaps) >= 180: bad('claws do not enclose the neck')
    for a in P['arches']:
        T = a['T']; dirv = V(T.x, T.y, 0); dirv.normalize()
        p = T + dirv * 0.3; p.z = zn
        if not s.isInside(p, 1e-6, True): bad('claw tip of %s missing' % a['name'])
    # Insertion from +y: spread each front claw needs (claw points moved outwards in x)
    # sample the whole clip (the trimmed arch alone is not a valid solid); the region
    # below holds only the right front nose
    sreq = 0.0
    if True:
        fs = s
        bb = fs.BoundBox
        samples = []
        for ix in range(0, 41):
            x = P['gap2'] / 2 + 0.25 * ix
            for iy in range(0, 33):
                y = 0.5 * iy
                for iz in range(-16, 17):
                    z = zn + 0.5 * iz
                    p = V(x, y, z)
                    if fs.isInside(p, 1e-6, True): samples.append(p)
        for p in samples:
            # smallest outward shift so that p clears the weight on its whole path yc >= 0
            for k in range(0, 41):
                sh = 0.1 * k
                q = V(p.x + sh, p.y, p.z)
                if all(not weight200_inside(q, yc) for yc in [0.5 * j for j in range(0, 80)]):
                    sreq = max(sreq, sh); break
            else:
                bad('front claw point %s cannot clear the 200 g even at 4 mm spread' % p)
    P['spread'] = sreq
    # Posts, roof, hole, clamp space
    xm = P['post_x'] + P['arm_w'] / 2
    for z in (P['z_head'], P['z_v'] - 1):
        if not inside(xm, 0, z) or not inside(-xm, 0, z): bad('post missing at z=%.1f' % z)
    zr = (P['z_ru'] + P['z_ro']) / 2
    if not inside(P['hole_d'] / 2 + 1, 0, zr) or not inside(-P['hole_d'] / 2 - 1, 0, zr): bad('roof missing')
    if inside(0, 0, zr): bad('cable hole missing')
    for z in (P['z_head2'] + 0.5, P['z_ru'] - 0.5):
        for x in (0, P['roof_w_in'] / 2 - 0.5, -(P['roof_w_in'] / 2 - 0.5)):
            if inside(x, 0, z): bad('material in the clamp space at (%.1f, %.1f)' % (x, z))
    return ok


def overhang_faces(shape, limit_deg=45.0):
    lim = -math.cos(math.radians(limit_deg)) - 2e-3
    found = []
    for i, f in enumerate(shape.Faces):
        if f.BoundBox.ZMax < 0.01: continue
        if hasattr(f.Surface, 'Radius') and f.Surface.Radius < 2.0: continue   # cable hole
        if abs(f.BoundBox.ZMax - P['z_ru']) < 1e-3 and abs(f.BoundBox.ZMin - P['z_ru']) < 1e-3 \
                and f.BoundBox.XLength <= P['roof_w_in'] + 1e-3: continue          # roof bridge
        u0, u1, v0, v1 = f.ParameterRange
        for a in (0.05, 0.3, 0.5, 0.7, 0.95):
            for b in (0.05, 0.3, 0.5, 0.7, 0.95):
                u, v = u0 + a * (u1 - u0), v0 + b * (v1 - v0)
                try:
                    p = f.valueAt(u, v); n = f.normalAt(u, v)
                except Exception:
                    continue
                if not f.isInside(p, 1e-3, True): continue
                if shape.isInside(p + n * 0.05, 1e-6, False): n = -n   # make n point out of the material
                if n.z < lim: found.append((i, p, n.z))
    return found


def overhang_layers(shape, dz=0.3, limit_deg=45.0):
    """Slice the part and look for area in a layer that lies further than
    dz * tan(limit) outside the layer below (edge effects included)."""
    off = dz * math.tan(math.radians(limit_deg)) + 0.05
    zp = P['z_ru']; rw = dz
    bad = []
    prev = None
    z = dz / 2
    while z < shape.BoundBox.ZMax:
        wires = shape.slice(V(0, 0, 1), z)
        try:
            cur = Part.makeFace(wires, 'Part::FaceMakerBullseye') if wires else None
        except Exception:
            cur = None
        if cur is not None and prev is not None and not (zp - rw - 0.5 < z < zp + rw + 0.5):
            try:
                grown = prev.makeOffset2D(off, 0)
                grown.translate(V(0, 0, cur.BoundBox.ZMin - grown.BoundBox.ZMin))
                extra = cur.cut(grown)
                if extra.Area > 0.3:
                    bb = extra.BoundBox
                    bad.append((round(z, 2), round(extra.Area, 2), (round(bb.Center.x, 1), round(bb.Center.y, 1))))
            except Exception as e:
                bad.append((round(z, 2), 'slice error %s' % e, None))
        prev = cur
        z += dz
    return bad


def write_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter'); sh.set('B1', 'Value'); sh.set('C1', 'Meaning')
    r = 2
    for n, v, note in PARAMS:
        sh.set('A%d' % r, n); sh.set('B%d' % r, str(v)); sh.set('C%d' % r, note); r += 1
    for n in ('x_i2', 'gap2', 'th_front', 'z_n2', 'z_head2', 'z_ru', 'z_top'):
        sh.set('A%d' % r, n); sh.set('B%d' % r, '%.3f' % P[n]); sh.set('C%d' % r, 'derived'); r += 1


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    derive()
    doc = App.newDocument('weight-clip-cable')
    write_sheet(doc)
    obj = build(doc)
    doc.recompute()
    fcstd = os.path.join(here, 'weight-clip-cable.FCStd')
    doc.saveAs(fcstd)
    Part.export([obj], os.path.join(here, 'weight-clip-cable.step'))
    s = obj.Shape; bb = s.BoundBox
    vol = s.Volume / 1000
    print('Outer size  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.1f cm3, solid PETG at %.2f g/cm3: %.1f g' % (vol, P['rho_petg'], vol * P['rho_petg']))
    print('Valid       %s, solids %d' % (s.isValid(), len(s.Solids)))
    print('Plausible   %s' % check(obj))
    of = overhang_faces(s)
    print('Overhang faces   %s' % ('none beyond 45 degrees (roof bridge excepted)' if not of else of[:6]))
    # slice with the cable hole filled (offsetting the tiny hole fails)
    ol = overhang_layers(s.fuse(Part.makeCylinder(P['hole_d'] / 2 + 0.2, P['roof_t'] + 1, V(0, 0, P['z_ru']))))
    print('Overhang layers  %s' % ('none (0.3 mm slices, roof bridge excepted)' if not ol else ol[:10]))
    zn = P['z_n2']
    print('Snap        gap2 = %.2f (neck2 %.1f x %.0f / %.0f): travel %.2f mm total at the groove bottom; front tips at %.1f deg'
          % (P['gap2'], P['neck2_d'], P['gap_w'], P['neck_d'], P['neck2_d'] - P['gap2'], P['th_front']))
    sp = P.get('spread', float('nan'))
    for a in P['arches']:
        Lc = zn - P['coll_h']
        print('Arch %-7s tip angle %.1f, length tip-column %.1f, column top inside z %.1f' % (a['name'], a['th'], a['L'], a['z_ci']))
    # the front noses sit on the side posts: the posts bow outwards between collar and
    # roof (both ends held, load near the middle): strain approx. 12 * d * t / L^2
    Lc = P['z_v'] - P['coll_h']
    eps = 12 * P['arm_w'] * sp / (Lc * Lc)
    print('Insertion   each front nose moves out %.1f mm; the side post (%.0f mm in x) bows over %.0f mm between collar and roof:'
          ' strain approx. %.1f %% (PETG: 2 %% for repeated snapping)' % (sp, P['arm_w'], Lc, 100 * eps))
    print('Noses       underside %.1f above the collar (clearing a full-diameter 1000 g head would need %.1f)' % (P['nose_above'], P['z_n2_head'] - P['nose_a'] - P['coll_h']))
    print('Claws       tip circle %.1f (neck %.1f + fit %.1f - %.1f)' % (2 * P['x_i2'], P['neck2_d'], P['fit'], P['claw_tight']))
    print('Heights     1000 g head top z %.1f, 200 g neck centre z %.1f (dual clip 42.5), head top %.1f, body bottom %.1f;'
          ' roof underside %.1f, top %.1f (flat %.1f wide)'
          % (P['z_head'], zn, P['z_head2'], P['z_head2'] - P['total2_h'], P['z_ru'], P['z_ro'], 2 * P['x_ro']))
    c = s.Solids[0].CenterOfMass if len(s.Solids) == 1 else s.CenterOfMass
    m_clip = vol * P['rho_petg']
    for m, yw, zw, name in ((1000.0, 0.0, P['coll_h'] - 20, '1000 g'), (500.0, 0.0, P['coll_h'] - 15, '500 g'), (200.0, 0.0, zn - 18, '200 g')):
        yc = (m_clip * c.y + m * yw) / (m_clip + m); zc = (m_clip * c.z + m * zw) / (m_clip + m)
        print('Load line   %s: seat at x = y = 0, clip tilt about the hole approx. %.2f deg' % (name, math.degrees(math.atan2(yc, P['z_ro'] - zc))))
    print('Clip CoG    x %.2f, y %.2f, z %.1f' % (c.x, c.y, c.z))


if __name__ == '__main__':
    main()
