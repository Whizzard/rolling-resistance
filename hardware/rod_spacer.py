# -*- coding: utf-8 -*-
"""
Spacer clip for the M6 stop rod (builder 2026-11-28): a 15 mm high spacer, flat top and
bottom, clipped sideways onto the M6 threaded rod between the knurled nuts of the lower
stop and the flat bar. It holds the weighing arm lifted for the direct calibration of the
load cell. Deliberately loose: the slot is as wide as the bore except for a 2 mm high clip
band in the middle.

SHAPE (use orientation, z = 0 bottom face, rod axis = z axis, opening towards +x):
    lower half  cylinder Ø d_bottom
    upper half  cone from Ø d_bottom at mid height to Ø d_top at the top
    clip band   clip_h high in the middle, the slot narrows to clip_w (45 degree flanks)
    grip        round ear Ø grip_d, grip_t thick, flush with the top face, opposite the
                opening, joined to the Ø d_top top by straight tangents (no waist), with
                a Ø lanyard_d hole for a lanyard

PRINT upside down (top face with the ear on the bed), no support: the cone narrows
upwards and the clip band has 45 degree flanks.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/rod_spacer.py
Produces hardware/rod-spacer.FCStd, .step.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
from FreeCAD import Vector as V

PARAMS = [
    ('height',    15.0, 'Spacer height, builder 2026-11-28 (first 20)'),
    ('d_bottom',  10.0, 'Diameter of the lower half, builder 2026-11-28'),
    ('d_top',     20.0, 'Diameter at the top, cone from mid height, builder 2026-11-28'),
    ('bore_d',     6.6, 'Bore and slot width for the M6 rod (loose)'),
    ('clip_h',     2.0, 'Clip band height in the middle, builder 2026-11-28'),
    ('clip_w',     5.4, 'Slot width in the clip band (rod 6.0: clips on)'),
    ('grip_d',    30.0, 'Grip ear outside diameter, builder 2026-11-28'),
    ('grip_t',     5.0, 'Grip ear thickness, builder 2026-11-28'),
    ('grip_overlap', 5.0, 'Ear reaches this far into the top of the cone'),
    ('lanyard_d',  5.0, 'Lanyard hole, builder 2026-11-28'),
    ('lanyard_rim', 4.0, 'Material outside the lanyard hole'),
]
P = {n: v for n, v, _ in PARAMS}


def build():
    h, rb, rt = P['height'], P['d_bottom'] / 2, P['d_top'] / 2
    zm = h / 2
    body = Part.makeCylinder(rb, zm).fuse(Part.makeCone(rb, rt, h - zm, V(0, 0, zm)))
    # grip ear, flush with the top, opposite the opening (-x)
    rg = P['grip_d'] / 2
    xg = -(rt + rg - P['grip_overlap'])
    # outline = convex hull of the top circle and the ear circle: straight tangents, no waist
    import math
    pts = [(rt * math.cos(t), rt * math.sin(t)) for t in [2 * math.pi * k / 180 for k in range(180)]]
    pts += [(xg + rg * math.cos(t), rg * math.sin(t)) for t in [2 * math.pi * k / 180 for k in range(180)]]
    pts = sorted(set(pts))
    def cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lo, up = [], []
    for q in pts:
        while len(lo) >= 2 and cross(lo[-2], lo[-1], q) <= 0: lo.pop()
        lo.append(q)
    for q in reversed(pts):
        while len(up) >= 2 and cross(up[-2], up[-1], q) <= 0: up.pop()
        up.append(q)
    hull = lo[:-1] + up[:-1]
    f = Part.Face(Part.makePolygon([V(x, y, h - P['grip_t']) for x, y in hull] + [V(hull[0][0], hull[0][1], h - P['grip_t'])]))
    ear = f.extrude(V(0, 0, P['grip_t']))
    xl = xg - rg + P['lanyard_rim'] + P['lanyard_d'] / 2
    ear = ear.cut(Part.makeCylinder(P['lanyard_d'] / 2, P['grip_t'] + 2, V(xl, 0, h - P['grip_t'] - 1)))
    shape = body.fuse(ear)
    # bore and slot (bore width) to +x
    bw = P['bore_d']
    shape = shape.cut(Part.makeCylinder(bw / 2, h + 2, V(0, 0, -1)))
    shape = shape.cut(Part.makeBox(rt + 2, bw, h + 2, V(0, -bw / 2, -1)))
    # clip band: ridges on both slot walls, clip_h high at the narrowest, 45 degree flanks
    d = (bw - P['clip_w']) / 2
    zc0, zc1 = zm - P['clip_h'] / 2, zm + P['clip_h'] / 2
    ridges = None
    for sg in (1, -1):
        yo, yi = sg * bw / 2, sg * (bw / 2 - d)
        f = Part.Face(Part.makePolygon([V(0, yo, zc0 - d), V(0, yi, zc0), V(0, yi, zc1), V(0, yo, zc1 + d), V(0, yo, zc0 - d)]))
        r = f.extrude(V(rt + 2, 0, 0))
        r = r.common(Part.makeCylinder(rt + 1, h).cut(Part.makeCylinder(P['clip_w'] / 2 + 0.3, h + 2, V(0, 0, -1))))
        ridges = r if ridges is None else ridges.fuse(r)
    # ridges only where there is wall to carry them (inside the body outline)
    ridges = ridges.common(body)
    shape = shape.fuse(ridges).removeSplitter()
    return shape, xl, xg


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    shape, xl, xg = build()
    doc = App.newDocument('rod-spacer')
    o = doc.addObject('Part::Feature', 'RodSpacer'); o.Shape = shape
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    for i, (n, v, note) in enumerate(PARAMS):
        sh.set('A%d' % (i + 1), n); sh.set('B%d' % (i + 1), str(v)); sh.set('C%d' % (i + 1), note)
    doc.recompute()
    doc.saveAs(os.path.join(here, 'rod-spacer.FCStd'))
    Part.export([o], os.path.join(here, 'rod-spacer.step'))
    bb = shape.BoundBox
    h = P['height']
    inside = lambda x, y, z: shape.isInside(V(x, y, z), 1e-6, True)
    y_clip = (P['clip_w'] / 2 + P['bore_d'] / 2) / 2
    ok = (not inside(0, 0, h / 2) and not inside(P['d_bottom'] / 2 - 0.5, 0, 3)            # bore, slot
          and inside(-P['bore_d'] / 2 - 0.8, 0, 3) and inside(-P['d_top'] / 2 + 0.5, 0, h - 0.5)  # wall, cone top
          and inside(2.0, y_clip, h / 2) and not inside(2.0, y_clip, h / 2 + 3)              # clip band only in the middle
          and not inside(xl, 0, h - 2.5) and inside(xg, 0, h - 2.5))                         # lanyard hole, ear
    print('Size   %.1f x %.1f x %.1f mm, %.1f cm3, valid %s, solids %d'
          % (bb.XLength, bb.YLength, bb.ZLength, shape.Volume / 1000, shape.isValid(), len(shape.Solids)))
    print('Check  %s (bore, slot, wall, cone, clip band, ear, lanyard hole)' % ok)
    print('Wall   %.1f mm at the bottom, %.1f at the top; slot %.1f, clip band %.1f wide x %.1f high'
          % ((P['d_bottom'] - P['bore_d']) / 2, (P['d_top'] - P['bore_d']) / 2, P['bore_d'], P['clip_w'], P['clip_h']))


if __name__ == '__main__':
    main()
