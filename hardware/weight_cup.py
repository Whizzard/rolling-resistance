# -*- coding: utf-8 -*-
"""
Cup for the check weight, round, screwed down with one M6 button head through
the floor. For printing.

Structure as in ipad_holder.py: one PartDesign body with a feature tree plus a
spreadsheet "Parameter"; all sketches are bound to the table via expressions.
Change dimensions in the GUI in the table, not in the sketches. Dimensions in mm.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/weight_cup.py

Creates three sizes, each as .FCStd, .step and .stl (VARIANTS below):
    hardware/weight-cup     inside 50 x 50 (1 kg check weight)
    hardware/weight-cup-38  inside 38 x 38 (small weights, builder 2026-11-26)
    hardware/weight-cup-28  inside 28 x 28 (small weights, builder 2026-11-26)

POSITION: z = 0 is the underside of the floor = print bed face, the cup axis is
the z axis.

    inner_d x inner_h = 50.0 x 50.0    inside, as specified (or 38 x 38, 28 x 28); no clearance added —
                                       if the weight sticks, raise inner_d
    wall_t            = 1.5            side wall

FLOOR as in the iPad holder: one M6 button head in the centre, in a flat
counterbore from the inside. Head height m6_head_h = 4 mm ASSUMED (measure on
the screw), below it m6_floor = 1.5 mm, so the floor is floor_t = 5.5 thick.
The counterbore m6_head_d = 13 takes DIN 7985 (dk 12) as well as ISO 7380
(dk 10.6). The head lies completely within the floor, the weight stands on the
floor, not on the head. inner_h is measured from the top of the floor.

One screw only: the cup can turn about it. That is harmless for a cup.

PRINTING: floor on the bed, no overhangs, no supports.

Feature tree:
    Cylinder   full cylinder outer_d x (floor_t + inner_h)
    Interior  pocket inner_d, inner_h deep from the rim
    M6         flat counterbore, PartDesign Hole with counterbore
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# -------------------------------------------------------------- Parameters
# (name, value or formula, comment). Formulas start with '='.
PARAMS = [
    ('inner_d',      50.0, 'inside diameter, as specified; no clearance added'),
    ('inner_h',      50.0, 'inside depth, from the top of the floor to the rim'),
    ('wall_t',        1.5, 'side wall'),

    # Screw M6 button head, flat counterbore from the inside
    ('m6_d',          6.6, 'M6 clearance'),
    ('m6_head_d',    13.0, 'flat counterbore; DIN 7985 dk 12, ISO 7380 dk 10.6 + air'),
    ('m6_head_h',     4.0, 'head height ASSUMED -> measure on the screw'),
    ('m6_floor',      1.5, 'floor under the head'),

    # derived
    ('floor_t', '=m6_head_h + m6_floor', 'floor thickness: head plus floor'),
    ('outer_d', '=inner_d + 2 * wall_t', 'outside diameter'),
    ('total_h', '=floor_t + inner_h',    'overall height'),
]


# Variants: export name -> (inner_d, inner_h), as specified by the builder.
VARIANTS = [
    ('weight-cup',    50.0, 50.0),
    ('weight-cup-38', 38.0, 38.0),
    ('weight-cup-28', 28.0, 28.0),
]


def params_for(inner_d, inner_h):
    out = []
    for name, val, note in PARAMS:
        if name == 'inner_d': val = inner_d
        if name == 'inner_h': val = inner_h
        out.append((name, val, note))
    return out


def build_sheet(doc, params=None):
    params = params or PARAMS
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Value')
    sh.set('C1', 'Meaning')
    for i, (name, val, note) in enumerate(params):
        r = i + 2
        sh.set('A%d' % r, name)
        sh.setAlias('B%d' % r, name)
        sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(params):
        sh.set('B%d' % (i + 2), str(val))
    doc.recompute()
    return sh


def P(sh, name):
    """Numeric value of a parameter."""
    return float(sh.get(name))


# ----------------------------------------------------------- Sketch helpers

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def centred_circle(sk, r, tag, expr_r):
    """Circle on the origin, fully constrained, radius bound to an expression."""
    g = sk.addGeometry(Part.Circle(Vector(0, 0, 0), Vector(0, 0, 1), r), False)
    sk.addConstraint(Sketcher.Constraint('Coincident', g, 3, -1, 1))
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    sk.setExpression('Constraints.%s_r' % tag, expr_r)
    return g


def sketch_on(body, plane, name, expr_off=None):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if expr_off:
        sk.setExpression('AttachmentOffset.Base.z', expr_off)
    return sk


# -------------------------------------------------------------------- Model

def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'WeightCup')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

    # 1 Full cylinder, floor plus wall height.
    sk = sketch_on(body, xy, 'Sk_Cylinder')
    centred_circle(sk, P(sh, 'outer_d') / 2, 'out', 'Parameter.outer_d / 2')
    f = body.newObject('PartDesign::Pad', 'Cylinder')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.total_h')
    doc.recompute()

    # 2 Interior: sketch on the rim, pocket inner_h deep downwards.
    sk = sketch_on(body, xy, 'Sk_Interior', expr_off='Parameter.total_h')
    centred_circle(sk, P(sh, 'inner_d') / 2, 'in', 'Parameter.inner_d / 2')
    f = body.newObject('PartDesign::Pocket', 'Interior')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.inner_h')
    doc.recompute()

    # 3 M6 flat counterbore from the inside. Sketch on top of the floor, the
    #   hole goes through downwards against the normal; the counterbore starts
    #   at the sketch plane and is m6_head_h deep.
    sk = sketch_on(body, xy, 'Sk_M6', expr_off='Parameter.floor_t')
    centred_circle(sk, P(sh, 'm6_d') / 2, 'm6', 'Parameter.m6_d / 2')
    h = body.newObject('PartDesign::Hole', 'M6')
    h.Profile = sk
    h.Threaded = False
    h.DepthType = 'ThroughAll'
    h.HoleCutType = 'Counterbore'
    h.setExpression('Diameter', 'Parameter.m6_d')
    h.setExpression('HoleCutDiameter', 'Parameter.m6_head_d')
    h.setExpression('HoleCutDepth', 'Parameter.m6_head_h')
    doc.recompute()

    return body


# FreeCAD does NOT store the GUI visibility in Document.xml but in a
# GuiDocument.xml with one ViewProvider per object. Headless, that file is not
# created -- so a file produced this way opens with default visibilities:
# sketches on, bodies off. We write it into the archive ourselves after saving.

_GUI_HEAD = """<?xml version='1.0' encoding='utf-8'?>
<!--
 FreeCAD Document, see https://www.freecad.org for more information...
-->
<Document SchemaVersion="1">
    <ViewProviderData Count="%d">
"""
_GUI_VP = """        <ViewProvider name="%s" expanded="0">
            <Properties Count="1" TransientCount="0">
                <Property name="Visibility" type="App::PropertyBool" status="1">
                    <Bool value="%s"/>
                </Property>
            </Properties>
        </ViewProvider>
"""
_GUI_TAIL = """    </ViewProviderData>
</Document>
"""


def write_gui_visibility(fcstd, visible):
    """Write GuiDocument.xml into the FCStd archive."""
    import re
    import shutil
    import zipfile
    with zipfile.ZipFile(fcstd) as z:
        names = z.namelist()
        doc_xml = z.read('Document.xml').decode('utf-8')
        data = {n: z.read(n) for n in names}
    order, seen = [], set()
    for o in re.findall(r'<Object name="([^"]+)"', doc_xml):
        if o not in seen:
            seen.add(o)
            order.append(o)
    xml = _GUI_HEAD % len(order)
    for o in order:
        xml += _GUI_VP % (o, 'true' if o in visible else 'false')
    xml += _GUI_TAIL
    tmp = fcstd + '.new'
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as z:
        for n in names:
            if n != 'GuiDocument.xml':
                z.writestr(n, data[n])
        z.writestr('GuiDocument.xml', xml)
    shutil.move(tmp, fcstd)
    return len(order)


def set_visibility(doc, body):
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    """Geometric plausibility — a sign error in the sketch planes should show
    up here, not only in the slicer."""
    s = body.Shape
    bb = s.BoundBox
    ok = True

    def material(p, was):
        nonlocal ok
        if not s.isInside(p, 1e-6, True):
            print('ERROR no material: %s at (%.1f, %.1f, %.1f)' % (was, p.x, p.y, p.z))
            ok = False

    def air(p, was):
        nonlocal ok
        if s.isInside(p, 1e-6, True):
            print('ERROR material instead of air: %s at (%.1f, %.1f, %.1f)' % (was, p.x, p.y, p.z))
            ok = False

    ri, wt = P(sh, 'inner_d') / 2, P(sh, 'wall_t')
    ft, th = P(sh, 'floor_t'), P(sh, 'total_h')
    hh, r_cb = P(sh, 'm6_head_h'), P(sh, 'm6_head_d') / 2

    if abs(bb.ZMin) > 1e-6:
        print('ERROR underside not at z = 0: %.3f' % bb.ZMin); ok = False
    if abs(bb.ZMax - th) > 1e-6:
        print('ERROR height %.3f instead of %.3f' % (bb.ZMax, th)); ok = False
    if abs(bb.XLength - P(sh, 'outer_d')) > 0.01:          # BoundBox of a circle is approximate
        print('ERROR outer diameter %.3f' % bb.XLength); ok = False
    # Wall all round
    for x, y in ((1, 0), (0, 1), (-1, 0), (0, -1)):
        material(Vector(x * (ri + wt / 2), y * (ri + wt / 2), ft + 1.0), 'wall bottom')
        material(Vector(x * (ri + wt / 2), y * (ri + wt / 2), th - 0.1), 'wall at the rim')
        air(Vector(x * (ri - 0.1), y * (ri - 0.1), ft + 0.1), 'interior at the wall')
    air(Vector(0, 0, th - 0.1), 'open at the top')
    air(Vector(r_cb + 0.5, 0, ft + 0.1), 'interior above the floor')
    material(Vector(r_cb + 0.5, 0, ft - 0.1), 'floor top')
    # M6: clearance hole, flat counterbore m6_head_h deep, floor below
    air(Vector(0, 0, ft / 2), 'M6 clearance hole')
    air(Vector(r_cb - 0.3, 0, ft - 0.1), 'counterbore top')
    air(Vector(r_cb - 0.3, 0, ft - hh + 0.1), 'counterbore bottom')
    material(Vector(r_cb - 0.3, 0, ft - hh - 0.1), 'floor under the head')
    material(Vector(r_cb - 0.3, 0, 0.1), 'floor at the bottom')
    print('Floor       %.1f thick, flat counterbore %.1f x %.1f deep, %.1f under the head'
          % (ft, P(sh, 'm6_head_d'), hh, P(sh, 'm6_floor')))
    print('Interior    %.1f x %.1f deep, wall %.1f' % (2 * ri, P(sh, 'inner_h'), wt))
    return ok


def main_one(name, inner_d, inner_h):
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument(name)
    sh = build_sheet(doc, params_for(inner_d, inner_h))
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, name + '.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, name + '.step'))
    body.Shape.exportStl(os.path.join(here, name + '.stl'))

    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.1f cm3' % (body.Shape.Volume / 1000.0))
    print('Valid       %s' % body.Shape.isValid())
    print('Plausible   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None)
           and 'Invalid' in o.State]
    print('Invalid     %s' % (bad if bad else 'none'))
    open_ = [o.Name for o in doc.Objects
             if o.isDerivedFrom('Sketcher::SketchObject')
             and not getattr(o, 'FullyConstrained', True)]
    print('Sketches    %d, not fully constrained: %s'
          % (len([o for o in doc.Objects
                  if o.isDerivedFrom('Sketcher::SketchObject')]),
             open_ if open_ else 'none'))

    App.closeDocument(doc.Name)


def main():
    for name, d, h in VARIANTS:
        print('--- %s' % name)
        main_one(name, d, h)

main()
