# -*- coding: utf-8 -*-
"""
Prism (V block) for measuring from a round shaft (builder 2026-11-27): set from above on a
shaft of 16 to 20 mm, it marks the shaft's vertical centre plane with a STOP FACE for a steel
ruler, and carries a block vial to set that face plumb. For measuring the distances from the
pendulum axis to the force and load arms -- on a round shaft the centre is hard to see.

PROFILE (x across the shaft, z up), extruded along the shaft (y) by `length`:
    V          90 deg, opening downwards, vertex at z = 0, symmetric about x = 0. Its
               symmetry plane is the shaft's vertical centre plane for EVERY diameter: the
               shaft's axis lies at z = -d/2 * sqrt(2) below the vertex, contacts at
               x = +-d/2 / sqrt(2). A small relief at the vertex keeps the corner free.
    Shelf      left (x < 0), top at z = shelf_z: the ruler lies flat on it and is pushed
               from the left against the
    STOP FACE  x = 0, vertical, facing -x: the ruler's end (its zero) sits exactly above the
               shaft axis. For distances to the right, turn the prism round.
    Vial       right (x > 0): pocket for a block vial (15.1 x 15.1 x 40, as level_clip.py)
               along x, between two end walls; the left end wall IS the stop. The vial
               measures the rotation about the shaft: bubble centred = stop face plumb.
               Open at the top and both long sides (visible), held with double-sided tape.
    Wings      left and right the same length, so the prism balances on the shaft.

PRINT lying on its side (a y face on the bed): the part is one extruded profile, so there is
no overhang and no support, and the V faces and the stop face come out as smooth walls.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/shaft_prism.py
Produces hardware/shaft-prism.FCStd and .step.
"""
import math
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')
import FreeCAD as App
import Part
from FreeCAD import Vector as V

P = dict(
    d_min=16.0, d_max=20.0,       # shaft range
    v_depth=15.0,                 # V faces down to z = -v_depth (half opening = v_depth)
    relief=1.5,                   # vertex relief, square
    body_w=21.0,                  # half width of the body around the V
    wing_z0=-3.0,                 # underside of the wings
    shelf_z=6.0,                  # top of the shelf (ruler) and floor of the vial pocket
    vial_l=40.0, vial_h=15.1, vial_fit=0.4,
    wall_t=3.0,                   # end walls of the vial pocket; the left one is the stop
    length=20.0,                  # along the shaft
)
p = type('P', (), P)

xv1 = p.wall_t + p.vial_l + p.vial_fit          # vial pocket right end
xr = xv1 + p.wall_t                             # right edge
xl = -xr                                        # left edge: same length, balances
zt = p.shelf_z + p.vial_h                       # top of the end walls, flush with the vial

prof = [(xl, p.wing_z0), (-p.body_w, p.wing_z0), (-p.body_w, -p.v_depth), (-p.v_depth, -p.v_depth),
        (-p.relief / 2, -p.relief / 2), (-p.relief / 2, p.relief / 2),       # V faces exactly 45 deg
        (p.relief / 2, p.relief / 2), (p.relief / 2, -p.relief / 2),
        (p.v_depth, -p.v_depth), (p.body_w, -p.v_depth), (p.body_w, p.wing_z0),
        (xr, p.wing_z0), (xr, zt), (xv1, zt), (xv1, p.shelf_z), (p.wall_t, p.shelf_z),
        (p.wall_t, zt), (0, zt), (0, p.shelf_z), (xl, p.shelf_z)]
w = Part.makePolygon([V(x, 0, z) for x, z in prof] + [V(prof[0][0], 0, prof[0][1])])
solid = Part.Face(w).extrude(V(0, p.length, 0))
solid.translate(V(0, -p.length / 2, 0))

# checks: both shaft sizes touch the V faces (not the relief, not beyond the V)
for d in (p.d_min, p.d_max):
    r = d / 2
    zc = -r * math.sqrt(2); xc = r / math.sqrt(2)
    assert p.relief < xc < p.v_depth, d
    print("shaft %.0f: axis %.1f below the vertex, contacts at x +-%.2f, %.1f below the shelf"
          % (d, -zc, xc, p.shelf_z - zc))
assert solid.isValid()
# the two V faces must be exactly 45 deg and mirror images: that is what puts x = 0 on the axis
vf = [f for f in solid.Faces if abs(abs(f.normalAt(0, 0).x) - math.sqrt(0.5)) < 1e-9
      and abs(f.normalAt(0, 0).z + math.sqrt(0.5)) < 1e-9]
assert len(vf) == 2, len(vf)

out = os.path.dirname(os.path.abspath(__file__))
doc = App.newDocument('ShaftPrism')
sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
for i, (k, v) in enumerate(sorted(P.items()), start=1):
    sh.set('A%d' % i, k); sh.set('B%d' % i, str(v))
o = doc.addObject('Part::Feature', 'ShaftPrism'); o.Shape = solid
doc.recompute()
doc.saveAs(os.path.join(out, 'shaft-prism.FCStd'))
solid.exportStep(os.path.join(out, 'shaft-prism.step'))
bb = solid.BoundBox
print("size %.1f x %.1f x %.1f mm, %.1f cm3, about %.0f g solid PETG"
      % (bb.XLength, bb.YLength, bb.ZLength, solid.Volume / 1000, solid.Volume * 1.27e-3))
