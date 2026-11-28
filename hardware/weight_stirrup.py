# -*- coding: utf-8 -*-
"""
Stirrup for the calibration weights (builder 2026-11-28): a small round platform that
hangs from the Bowden cable of the load cell (direct calibration,
docs/06-open-questions.md C6). Each weight -- 200 g, 500 g, 1 kg, 2 kg -- stands on it in
turn. Replaces the quiver (weight_ladder.py) once the 2 kg weight is there.

STRUCTURE (z = 0 underside of the floor = print bed, cable hole on the z axis):
    Floor    round. From the top, concentric recesses step down by step_h every
             step_dd in diameter, from the 2 kg seat to the 500 g seat; a weight stands in
             the smallest recess its body fits and is centred by the step edge. Floor at
             the thinnest point (the centre) floor_min.
    Bails    two side posts outside the 2 kg weight (x = +-), open to the front and back
             for putting the weights in.
    Roof     the bails bend inwards at 45 degrees and meet in a short flat roof with the
             vertical cable hole; the clamp bears on the flat underside (a short bridge).

WEIGHTS: all of one shape, scaled (builder 2026-11-28). 1 kg: body Ø 48, overall 77
(vendor drawing), head Ø 45, head 11 long (builder). 2 kg ESTIMATED: scaling the 1 kg
with 2^(1/3) = 1.26 gives Ø 60.5 x 97, the KERN 347-12 datasheet (2 kg M1 knob weight)
Ø 60 x 114; the larger of each is used. 500 g scaled with 0.794. Head top modelled fully rounded (radius =
head length, ASSUMED). Measure the 2 kg weight when it arrives and rerun.

PRINT: floor on the bed, no support (the roof underside is a short bridge).

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/weight_stirrup.py
Produces hardware/weight-stirrup.FCStd, .step.
"""

import math
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
from FreeCAD import Vector as V

S2, S5 = 2 ** (1 / 3), 0.5 ** (1 / 3)
W1 = dict(body=48.0, total=77.0, head=45.0, head_h=11.0)
W2 = {k: v * S2 for k, v in W1.items()}                      # 2 kg, ESTIMATED by scaling: Ø 60.5 x 97
# KERN 347-12 (2 kg M1 knob, stainless) datasheet: Ø 60 x 114. Take the larger of both
# per dimension until the bought weight is measured.
W2['total'] = max(W2['total'], 114.0)
D500 = W1['body'] * S5                                       # 500 g body, ESTIMATED

PARAMS = [
    ('seat_fit',   1.0, 'Smallest recess: 500 g body + this'),
    ('step_dd',    4.0, 'Recess diameter step, builder 2026-11-28'),
    ('step_h',     1.0, 'Recess depth step, builder 2026-11-28'),
    ('floor_min',  1.0, 'Floor at the thinnest point, builder 2026-11-28'),
    ('rim',        3.0, 'Floor rim outside the largest recess'),
    ('side_clear', 3.0, 'Bail inside to the 2 kg body, per side'),
    ('bail_t',     6.0, 'Bail thickness (x)'),
    ('bail_w',    12.0, 'Bail width (y)'),
    ('head_clear', 2.0, 'Roof lines above the 2 kg head'),
    ('clamp_space', 15.0, 'Roof underside above the 2 kg head top (cable end and clamp), as the cable clip'),
    ('roof_w_in',  8.0, 'Flat roof underside (bridge, clamp seat)'),
    ('roof_t',     6.0, 'Roof thickness'),
    ('hole_d',     2.0, 'Cable hole, builder 2026-11-28'),
    ('rho_petg',   1.27, 'PETG density g/cm3, typical datasheet value (solid)'),
]
P = {n: v for n, v, _ in PARAMS}


def head_top(r, z0):
    """2 kg head profile: flat in the middle, edge rounded with radius head_h."""
    R, rr = W2['head'] / 2, W2['head_h']
    if r >= R: return -1e9
    r0 = R - rr
    return z0 + W2['head_h'] if r <= r0 else z0 + math.sqrt(max(rr * rr - (r - r0) ** 2, 0.0))


def build():
    # recesses: smallest = 500 g + fit, then step_dd larger up to the first that takes the 2 kg
    ds = [D500 + P['seat_fit']]
    while ds[-1] < W2['body'] + 1.0:
        ds.append(ds[-1] + P['step_dd'])
    n = len(ds)
    floor_t = P['floor_min'] + n * P['step_h']
    r_floor = ds[-1] / 2 + P['rim']
    xi = W2['body'] / 2 + P['side_clear']                    # bail inside
    xo = xi + P['bail_t']
    r_floor = max(r_floor, xo)
    # the 2 kg stands on the first step (depth step_h); its head starts at total - head_h
    z_stand = floor_t - P['step_h']
    z_head0 = z_stand + W2['total'] - W2['head_h']
    z_headtop = z_stand + W2['total']
    # roof: inner 45 degree line from (xi, z_v) up to (roof_w_in/2, z_ru)
    z_ru = z_headtop + P['clamp_space']
    z_v = z_ru - (xi - P['roof_w_in'] / 2)
    # lift the roof until the 45 degree lines clear the head by head_clear
    for _ in range(200):
        ok = all(z_v + (xi - r) >= head_top(r, z_head0) + P['head_clear'] for r in [0.25 * k for k in range(int(xi * 4))])
        if ok: break
        z_v += 0.5; z_ru += 0.5
    z_ro = z_ru + P['roof_t']
    x_ro = xo + z_v - z_ro
    floor = Part.makeCylinder(r_floor, floor_t)
    for i, d in enumerate(reversed(ds)):                     # largest first, 1 step deep
        depth = (i + 1) * P['step_h']
        floor = floor.cut(Part.makeCylinder(d / 2, depth + 1, V(0, 0, floor_t - depth)))
    pts = [(xi, 0), (xo, 0), (xo, z_v), (x_ro, z_ro), (-x_ro, z_ro), (-xo, z_v), (-xo, 0), (-xi, 0),
           (-xi, z_v), (-P['roof_w_in'] / 2, z_ru), (P['roof_w_in'] / 2, z_ru), (xi, z_v)]
    f = Part.Face(Part.makePolygon([V(x, 0, z) for x, z in pts] + [V(pts[0][0], 0, pts[0][1])]))
    bails = f.extrude(V(0, P['bail_w'], 0)); bails.translate(V(0, -P['bail_w'] / 2, 0))
    # bails stand on the floor rim; cut the recesses again so the bails do not fill them
    shape = floor.fuse(bails)
    for i, d in enumerate(reversed(ds)):
        depth = (i + 1) * P['step_h']
        shape = shape.cut(Part.makeCylinder(d / 2, depth + 0.001, V(0, 0, floor_t - depth)))
    shape = shape.cut(Part.makeCylinder(P['hole_d'] / 2, P['roof_t'] + 2, V(0, 0, z_ru - 1)))
    shape = shape.removeSplitter()
    info = dict(ds=ds, floor_t=floor_t, r_floor=r_floor, xi=xi, z_ru=z_ru, z_ro=z_ro, z_v=z_v,
                z_headtop=z_headtop, z_stand=z_stand)
    return shape, info


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    shape, I = build()
    doc = App.newDocument('weight-stirrup')
    o = doc.addObject('Part::Feature', 'WeightStirrup'); o.Shape = shape
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    for i, (n, v, note) in enumerate(PARAMS):
        sh.set('A%d' % (i + 1), n); sh.set('B%d' % (i + 1), str(v)); sh.set('C%d' % (i + 1), note)
    doc.recompute()
    doc.saveAs(os.path.join(here, 'weight-stirrup.FCStd'))
    Part.export([o], os.path.join(here, 'weight-stirrup.step'))
    bb = shape.BoundBox
    vol = shape.Volume / 1000
    inside = lambda x, y, z: shape.isInside(V(x, y, z), 1e-6, True)
    ok = (inside(0, 0, P['floor_min'] / 2) and not inside(0, 0, P['floor_min'] + 0.5)
          and not inside(0, 0, (I['z_ru'] + I['z_ro']) / 2) and inside(3, 0, (I['z_ru'] + I['z_ro']) / 2)
          and not inside(0, 0, I['z_headtop'] + 1) and not inside(I['xi'] - 0.5, 0, I['z_stand'] + 40)
          and inside(I['xi'] + 1, 0, I['z_stand'] + 40))
    print('Size        %.1f x %.1f x %.1f mm, %.1f cm3, solid PETG %.0f g, valid %s, solids %d'
          % (bb.XLength, bb.YLength, bb.ZLength, vol, vol * P['rho_petg'], shape.isValid(), len(shape.Solids)))
    print('Check       %s (floor, recess, hole, roof, head and body clear)' % ok)
    print('2 kg        body Ø %.1f, overall %.1f, head Ø %.1f (ESTIMATED: scaled Ø, KERN height)' % (W2['body'], W2['total'], W2['head']))
    ds_ = I['ds']
    print('Recesses    Ø ' + ', '.join('%.1f' % d for d in reversed(ds_)) + ' mm, each %.0f mm deeper; floor %.1f outside, %.1f in the centre'
          % (P['step_h'], I['floor_t'], P['floor_min']))
    for name, d in (('200 g', 28.0), ('500 g', D500), ('1 kg', W1['body']), ('2 kg', W2['body'])):
        seat = min((x for x in ds_ if x >= d), default=None)
        print('            %-6s body Ø %.1f -> %s' % (name, d, 'recess Ø %.1f (%.1f play)' % (seat, seat - d) if seat else 'stands on the smallest floor'))
    print('Bails       %.1f clear between them; roof underside z %.1f (%.1f above the 2 kg head), top z %.1f'
          % (2 * I['xi'], I['z_ru'], I['z_ru'] - I['z_headtop'], I['z_ro']))


if __name__ == '__main__':
    main()
