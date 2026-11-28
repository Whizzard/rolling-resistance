# -*- coding: utf-8 -*-
"""
Stirrup for the calibration weights, version 2 PRINTED LYING (builder 2026-11-28). Same
job as weight_stirrup.py: hangs from the Bowden cable of the load cell, each weight
(200 g, 500 g, 1 kg, 2 kg) stands on it in turn on centring steps.

JOINT: a 45 degree chamfer (gusset) in the corner between the floor top and the front
face of each post stiffens the joint; in print it rises at 45 degrees from the frame.

FLOOR is a T (builder 2026-11-28): a cross band behind the frame from post to post and a
stem from the centre backwards to the rim of the largest recess; the centring steps
remain on band and stem (arcs on both sides and at the back). The steps have 45 degree
walls (step_h chamfer, then flat): printed lying, they are 45 degree overhangs.
CENTRE OF GRAVITY: cut_e (and with it the lean) is solved so that the empty stirrup, with
Bambu Lab's default PLA settings (2 walls, 15 % infill), hangs with its floor level; the
weights stand on the axis under the hole, so it stays level with any of them.

IDEA: the frame (two posts, 45 degree legs, roof with the cable hole) is one flat plate
that lies on the print bed, so the bending from the roof legs runs along the extrusion
lines instead of across the layers. The round floor is cut off at the back face of the
frame (cut_e in front of the weight axis), so it stands up from the bed. The weights
still stand safely: the floor reaches cut_e past their axis.
Because the frame then sits off the axis, it leans by a small angle alpha against the
floor (floor and frame are at 90 - alpha degrees), so that the roof and its cable hole
come back over the weight axis. Hanging, the floor is level and the hole is above the
centre of every weight.

USE FRAME: z up, weight axis = z axis, floor underside z = 0; frame plate across x,
thickness frame_t in y, its back face through y = -cut_e at z = 0.

PRINT: back face of the frame on the bed; the floor stands up from it (leaning by
alpha). The centring steps become 1 mm overhangs, the cable hole a short horizontal
hole. No support.

WEIGHTS as weight_stirrup.py (2 kg: Ø 60.5 scaled from the 1 kg, height 114 KERN 347-12;
measure the bought one and rerun).

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/weight_stirrup_flat.py
Produces hardware/weight-stirrup-flat.FCStd, .step (in print orientation).
"""

import math
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import FreeCAD as App
import Part
from FreeCAD import Vector as V

from weight_stirrup import W2, D500, head_top   # same weights

PARAMS = [
    ('cut_e',      6.0, 'Start value: floor cut (frame back face) in front of the weight axis; solved so that the empty stirrup hangs level'),
    ('band_w',    14.0, 'T floor: cross band behind the frame back face, carries the posts'),
    ('stem_w',    14.0, 'T floor: stem from the centre backwards, to the rim of the largest recess'),
    ('stem_chamfer', 8.0, 'T floor: 45 degree chamfer in the inside corners band / stem (plan view), builder 2026-11-28'),
    # Bambu Lab default PLA (0.4 nozzle, 0.2 mm layers): 2 walls of 0.42, top 5 layers,
    # bottom 3 layers, 15 % sparse infill. Modelled as a solid skin of skin_t and 15 % inside.
    ('skin_t',     0.9, 'Solid skin thickness for the mass model (walls 0.84, top 1.0, bottom 0.6)'),
    ('infill',     0.15, 'Sparse infill share for the mass model (Bambu default)'),
    ('frame_t',    8.0, 'Frame plate thickness (print height)'),
    ('post_w',    10.0, 'Post width in the frame plane'),
    ('seat_fit',   1.0, 'Smallest recess: 500 g body + this'),
    ('step_dd',    4.0, 'Recess diameter step, builder 2026-11-28'),
    ('step_h',     1.0, 'Recess depth step, builder 2026-11-28'),
    ('floor_min',  3.0, 'Floor at the thinnest point, builder 2026-11-28 (first 1, the stem was too thin)'),
    ('rim',        3.0, 'Floor rim outside the largest recess'),
    ('side_clear', 3.0, 'Post inside to the 2 kg body, per side'),
    ('head_clear', 2.0, 'Roof legs above the 2 kg head'),
    ('clamp_space', 15.0, 'Roof underside above the 2 kg head top'),
    ('roof_w_in',  8.0, 'Flat roof underside (clamp seat)'),
    ('roof_t',     8.0, 'Roof thickness'),
    ('gusset',     8.0, 'Chamfer (45 degrees) floor top to post front, along each post, at most to the back of the cross band, builder 2026-11-28'),
    ('round_out',  6.0, 'Rounding of the outer frame corners'),
    ('round_in',   4.0, 'Rounding of the inner frame corners'),
    ('hole_d',     2.0, 'Cable hole, builder 2026-11-28'),
    ('rho_pla',    1.24, 'PLA density g/cm3, typical datasheet value (solid)'),
]
P = {n: v for n, v, _ in PARAMS}


def fillet_pts(pts, radii, n=10):
    out = []
    for i, p1 in enumerate(pts):
        r = radii.get(i)
        if not r:
            out.append(p1); continue
        p0, p2 = pts[i - 1], pts[(i + 1) % len(pts)]
        a = (p0[0] - p1[0], p0[1] - p1[1]); b = (p2[0] - p1[0], p2[1] - p1[1])
        la, lb = math.hypot(*a), math.hypot(*b)
        a = (a[0] / la, a[1] / la); b = (b[0] / lb, b[1] / lb)
        ang = math.acos(max(-1.0, min(1.0, a[0] * b[0] + a[1] * b[1])))
        t = r / math.tan(ang / 2)
        bis = (a[0] + b[0], a[1] + b[1]); lbis = math.hypot(*bis); bis = (bis[0] / lbis, bis[1] / lbis)
        c = (p1[0] + bis[0] * r / math.sin(ang / 2), p1[1] + bis[1] * r / math.sin(ang / 2))
        s, e = (p1[0] + a[0] * t, p1[1] + a[1] * t), (p1[0] + b[0] * t, p1[1] + b[1] * t)
        a0, a1 = math.atan2(s[1] - c[1], s[0] - c[0]), math.atan2(e[1] - c[1], e[0] - c[0])
        da = a1 - a0
        while da > math.pi: da -= 2 * math.pi
        while da < -math.pi: da += 2 * math.pi
        out += [(c[0] + r * math.cos(a0 + da * k / n), c[1] + r * math.sin(a0 + da * k / n)) for k in range(n + 1)]
    return out


def recess_cut(ds, floor_t, extra=1.0):
    """Stepped recess with 45 degree walls: each step a step_h chamfer, then flat."""
    h = P['step_h']; cut = None
    for i, d in enumerate(reversed(ds)):
        depth = (i + 1) * h
        z0 = floor_t - depth
        c = Part.makeCone(d / 2 - h, d / 2, h, V(0, 0, z0)).fuse(Part.makeCylinder(d / 2, depth - h + extra, V(0, 0, z0 + h)))
        cut = c if cut is None else cut.fuse(c)
    return cut


def build(e=None):
    e = P['cut_e'] if e is None else e
    # floor and recesses (as weight_stirrup.py)
    ds = [D500 + P['seat_fit']]
    while ds[-1] < W2['body'] + 1.0:
        ds.append(ds[-1] + P['step_dd'])
    floor_t = P['floor_min'] + len(ds) * P['step_h']
    xi = W2['body'] / 2 + P['side_clear']
    xo = xi + P['post_w']
    r_floor = max(ds[-1] / 2 + P['rim'], xo)
    z_stand = floor_t - P['step_h']
    z_head0 = z_stand + W2['total'] - W2['head_h']
    z_headtop = z_stand + W2['total']
    z_ru = z_headtop + P['clamp_space']
    z_v = z_ru - (xi - P['roof_w_in'] / 2)
    for _ in range(200):
        if all(z_v + (xi - r) >= head_top(r, z_head0) + P['head_clear'] for r in [0.25 * k for k in range(int(xi * 4))]):
            break
        z_v += 0.5; z_ru += 0.5
    z_ro = z_ru + P['roof_t']
    x_ro = xo + z_v - z_ro
    # lean alpha: the frame mid-plane at the roof passes through y = 0
    t, zr = P['frame_t'], (z_ru + z_ro) / 2
    al = 0.0
    for _ in range(20):
        al = math.asin(max(-1.0, min(1.0, (e - t / 2 * math.cos(al)) / zr)))
    # frame outline in its own plane (x, v), v along the leaning posts, from below the floor
    pts = [(xi, -5), (xo, -5), (xo, z_v), (x_ro, z_ro), (-x_ro, z_ro), (-xo, z_v), (-xo, -5), (-xi, -5),
           (-xi, z_v), (-P['roof_w_in'] / 2, z_ru), (P['roof_w_in'] / 2, z_ru), (xi, z_v)]
    ro, ri = P['round_out'], P['round_in']
    pts = fillet_pts(pts, {2: ro, 3: ro, 4: ro, 5: ro, 8: ri, 9: ri, 10: ri, 11: ri})
    f = Part.Face(Part.makePolygon([V(x, 0, z) for x, z in pts] + [V(pts[0][0], 0, pts[0][1])]))
    frame = f.extrude(V(0, t, 0))
    # lean: rotate about x so the top goes to +y, then shift the back face to y = -cut_e
    def place(s):
        s.rotate(V(0, 0, 0), V(1, 0, 0), -math.degrees(al))
        s.translate(V(0, -e, 0))
        return s
    frame = place(frame)
    half = place(Part.makeBox(400, 400, 600, V(-200, 0, -100)))       # the side of the back face that is kept
    # T floor: a cross band behind the frame (posts to the centre) and a stem backwards to
    # the rim of the largest recess; the rest of the round floor is left out to save mass
    # and to bring the centre of gravity onto the axis
    r_stem = ds[-1] / 2 + P['rim']
    disc = Part.makeCylinder(r_floor, floor_t)
    band = Part.makeBox(2 * r_floor + 2, P['band_w'] + 2, floor_t, V(-r_floor - 1, -e - 2, 0))
    stem = Part.makeBox(P['stem_w'], r_stem + e + 2, floor_t, V(-P['stem_w'] / 2, -e - 2, 0))
    yb, cf, sw = -e + P['band_w'], P['stem_chamfer'], P['stem_w'] / 2
    for sg in (1, -1):   # 45 degree chamfers in the inside corners band / stem
        tri = Part.Face(Part.makePolygon([V(sg * (sw - 1), yb - 1, 0), V(sg * (sw + cf + 1), yb - 1, 0),
                                          V(sg * (sw - 1), yb + cf + 1, 0), V(sg * (sw - 1), yb - 1, 0)]))
        stem = stem.fuse(tri.extrude(V(0, 0, floor_t)))
    floor = disc.common(band.fuse(stem)).common(Part.makeCylinder(r_floor, floor_t)).fuse(
        Part.makeCylinder(r_stem, floor_t).common(stem))
    floor = floor.cut(recess_cut(ds, floor_t))
    # 45 degree chamfer in the corner floor top / post front face, along each post
    yf = -e + t / math.cos(al) + floor_t * math.tan(al)       # front face at the floor top
    c = min(P['gusset'], -e + P['band_w'] - yf)               # only as far as the cross band reaches
    tri = Part.Face(Part.makePolygon([V(0, yf - 1, floor_t - 0.5), V(0, yf + c, floor_t - 0.5), V(0, yf + c, floor_t),
                                      V(0, yf - 1, floor_t + c + 1), V(0, yf - 1, floor_t - 0.5)]))
    for sg in (1, -1):
        g = tri.extrude(V(P['post_w'], 0, 0)); g.translate(V(xi if sg > 0 else -xo, 0, 0))
        frame = frame.fuse(g)
    shape = floor.fuse(frame).common(half).common(Part.makeBox(400, 400, 600, V(-200, -200, 0)))
    shape = shape.cut(recess_cut(ds, floor_t, 0.001))
    shape = shape.cut(Part.makeCylinder(P['hole_d'] / 2, 60, V(0, 0, z_ru - 30)))
    try:
        shape = shape.removeSplitter()
    except Exception:
        pass
    if len(shape.Solids) != 1: raise ValueError('%d solids' % len(shape.Solids))
    info = dict(ds=ds, floor_t=floor_t, r_floor=r_floor, xi=xi, z_ru=z_ru, z_ro=z_ro, z_stand=z_stand,
                z_head0=z_head0, z_headtop=z_headtop, alpha=math.degrees(al), e=e)
    return shape.Solids[0], info


def print_mass(s):
    """Mass and centre of gravity as printed: a solid skin of skin_t over the whole surface
    (walls, top and bottom layers), sparse infill inside. Skin approximated by surface area x
    skin_t with the area centroid; exact enough for these 1-3 mm walls and 8 mm plates."""
    rho = P['rho_pla'] / 1000.0   # g/mm3
    A = sum(f.Area for f in s.Faces)
    ca = V(0, 0, 0)
    for f in s.Faces: ca += f.CenterOfMass * f.Area
    ca = ca * (1.0 / A)
    vs = min(A * P['skin_t'], s.Volume)
    vi = s.Volume - vs
    ci = (s.CenterOfMass * s.Volume - ca * vs) * (1.0 / vi) if vi > 0 else s.CenterOfMass
    m_skin, m_in = rho * vs, rho * P['infill'] * vi
    m = m_skin + m_in
    return m, (ca * m_skin + ci * m_in) * (1.0 / m), True


def solve():
    """cut_e so that the empty stirrup (as printed) hangs with its floor level."""
    e0, e1 = P['cut_e'], P['cut_e'] + 2.0
    s0, I0 = build(e0); y0 = print_mass(s0)[1].y
    s1, I1 = build(e1); y1 = print_mass(s1)[1].y
    for _ in range(8):
        if abs(y1) < 0.05 or y1 == y0: break
        e2 = e1 - y1 * (e1 - e0) / (y1 - y0)
        e2 = max(2.0, min(20.0, e2))
        e0, y0 = e1, y1
        s1, I1 = build(e2); y1 = print_mass(s1)[1].y; e1 = e2
    return s1, I1


def check(s, I):
    bad = []
    inside = lambda p: s.isInside(p, 1e-6, True)
    # 2 kg body and head (and so every smaller weight) clear of the stirrup
    R = W2['body'] / 2 + 0.2
    for k in range(72):
        c, sn = math.cos(2 * math.pi * k / 72), math.sin(2 * math.pi * k / 72)
        for z in [I['z_stand'] + 0.5 + 4 * j for j in range(int((I['z_head0'] - I['z_stand']) / 4))]:
            if inside(V(R * c, R * sn, z)): bad.append(('body', z))
        for rr in [W2['head'] / 2 * f for f in (0.3, 0.6, 0.9, 0.99)]:
            z = head_top(rr, I['z_head0']) + 0.5
            if inside(V(rr * c, rr * sn, z)): bad.append(('head', z))
    # floor under the axis and at the cut, cable hole, roof around it
    if not inside(V(0, 0, P['floor_min'] / 2)): bad.append(('floor centre', 0))
    if not inside(V(0, -I['e'] + 0.5, P['floor_min'] / 2)): bad.append(('floor at the cut', 0))
    zr = (I['z_ru'] + I['z_ro']) / 2
    if inside(V(0, 0, zr)): bad.append(('cable hole', zr))
    if not inside(V(P['hole_d'], 0, zr)): bad.append(('roof at the hole', zr))
    return bad


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    s, I = solve()
    bad = check(s, I)
    # tilt when hanging: stirrup CoG (as printed) off the axis (y), weights on the axis
    m_s, c, shell_ok = print_mass(s)
    z_h = (I['z_ru'] + I['z_ro']) / 2
    tilts = []
    for name, m, zg in (('empty', 0.0, 0.0), ('200 g', 200.0, I['z_stand'] + 18), ('500 g', 500.0, I['z_stand'] + 25),
                        ('1 kg', 1000.0, I['z_stand'] + 32), ('2 kg', 2000.0, I['z_stand'] + 40)):
        M = m_s + m
        yg = m_s * c.y / M; zgg = (m_s * c.z + m * zg) / M
        tilts.append((name, math.degrees(math.atan2(yg, z_h - zgg))))
    # print orientation: undo the lean, back face to z = 0
    pr = s.copy()
    pr.translate(V(0, I['e'], 0))
    pr.rotate(V(0, 0, 0), V(1, 0, 0), I['alpha'])
    pr.rotate(V(0, 0, 0), V(1, 0, 0), 90)
    pr.translate(V(0, 0, -pr.BoundBox.ZMin))
    doc = App.newDocument('weight-stirrup-flat')
    o = doc.addObject('Part::Feature', 'WeightStirrupFlat'); o.Shape = pr
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    for i, (n, v, note) in enumerate(PARAMS):
        sh.set('A%d' % (i + 1), n); sh.set('B%d' % (i + 1), str(v)); sh.set('C%d' % (i + 1), note)
    doc.recompute()
    doc.saveAs(os.path.join(here, 'weight-stirrup-flat.FCStd'))
    Part.export([o], os.path.join(here, 'weight-stirrup-flat.step'))
    bb = pr.BoundBox
    print('Print size  %.1f x %.1f x %.1f mm (frame on the bed), %.1f cm3, valid %s'
          % (bb.XLength, bb.YLength, bb.ZLength, s.Volume / 1000, pr.isValid()))
    print('Mass        %.0f g as printed (Bambu PLA defaults, skin %.1f mm + %.0f %% infill%s), %.0f g solid; CoG y %.2f, z %.1f'
          % (m_s, P['skin_t'], P['infill'] * 100, '', s.Volume / 1000 * P['rho_pla'], c.y, c.z))
    print('Check       %s' % ('ok (weights clear, floor, hole, roof)' if not bad else bad[:6]))
    print('Lean        frame %.2f deg against the floor normal (floor at %.2f deg to the frame); hole over the axis'
          % (I['alpha'], 90 - I['alpha']))
    print('Floor       T in Ø %.1f, cut %.2f in front of the axis (solved: empty stirrup level); recesses Ø %s, 45 degree walls; %.1f thick outside, %.1f in the centre'
          % (2 * I['r_floor'], I['e'], ', '.join('%.1f' % d for d in reversed(I['ds'])), I['floor_t'], P['floor_min']))
    print('Frame       %.0f mm plate, posts %.0f wide, %.1f clear; roof underside z %.1f, %.1f above the 2 kg head'
          % (P['frame_t'], P['post_w'], 2 * I['xi'], I['z_ru'], I['z_ru'] - I['z_headtop']))
    print('Hanging     tilt of the floor (stirrup CoG off the axis): ' + ', '.join('%s %.2f deg' % t for t in tilts))


if __name__ == '__main__':
    main()
