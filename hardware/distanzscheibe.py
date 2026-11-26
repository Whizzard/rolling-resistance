# -*- coding: utf-8 -*-
"""
Spacer disc with a locating key for the aluminium profile, for printing.

Disc 40 mm outside, 5 mm thick, hole 8.5 mm (M8 clearance), on one face a locating
key 8 mm wide and 3 mm high, as a rib across the whole face -- the disc lies flat on
the aluminium profile, the rib engages the slot and keeps the disc from turning, the
M8 goes through the disc into the slot. Around the bore the rib is cut away to
nose_gap = 9 mm (two rib pieces with straight inner edges, no leftover rounding around
the hole). All vertical edges of the rib pieces, inside at the cut-out and outside at
the circumference, are filleted with nose_r = 1 mm -- this makes the ends oval.

Structure as in libellen_clip.py: one PartDesign body with a feature tree plus a
spreadsheet "Parameter"; all sketches are bound to the table through expressions.
Change dimensions in the GUI in the table, not in the sketches. Dimensions in mm.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/distanzscheibe.py

Produces hardware/distanzscheibe.FCStd, .step and .stl.

POSITION: disc in the XY plane, hole axis = z, z = 0 is the flat side facing away
from the profile. The rib sits on the face z = disc_t, runs along x (= slot
direction), is nose_b wide (y) and nose_h high (z). Its length is chosen so that the
corners lie exactly on the circumference: nose_l = 2 sqrt(r^2 - (nose_b/2)^2),
nothing protrudes beyond the disc. The flanks are parallel (no wedge) so that the rib
sits in the slot without play: width = nose_w - nose_fit.

Feature tree:
    Scheibe     circle Ø disc_d, pad disc_t
    Nase        two rectangles of (nose_l - nose_gap)/2 x nose_b each on the face,
                either side of the cut-out nose_gap around the bore, pad nose_h
    Rundung     fillet nose_r on the eight vertical rib edges
    Loch        Ø hole_d through the disc

Printing: disc with the rib-free side on the bed, the rib on top -- no supports, no
overhangs.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# --------------------------------------------------------------- Parameter
PARAMS = [
    ('disc_d',    40.0, 'Disc outer diameter'),
    ('disc_t',     5.0, 'Disc thickness'),
    ('hole_d',     8.5, 'Hole, M8 clearance'),
    ('nose_w',     8.0, 'Key width, nominal size of the profile slot'),
    ('nose_fit',   0.2, 'Play in the slot width, TOTAL -- print allowance'),
    ('nose_h',     3.0, 'Key height, axially above the face'),
    ('nose_gap',   9.0, 'Cut-out of the rib around the bore, total width'),
    ('nose_r',     1.0, 'Fillet of the vertical rib edges, inside and outside'),
    # Derived
    ('nose_b',  '=nose_w - nose_fit',                           'Rib width as printed'),
    ('nose_l',  '=2 * sqrt((disc_d / 2)^2 - (nose_b / 2)^2)',   'Rib length (total): corners exactly on the circumference'),
    ('nose_seg', '=(nose_l - nose_gap) / 2',                     'length of one rib piece'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Value')
    sh.set('C1', 'Meaning')
    for i, (name, val, note) in enumerate(PARAMS):
        r = i + 2
        sh.set('A%d' % r, name)
        sh.setAlias('B%d' % r, name)
        sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(PARAMS):
        sh.set('B%d' % (i + 2), str(val))
    doc.recompute()
    return sh


def P(sh, name):
    return float(sh.get(name))


# ----------------------------------------------------------- Sketch helpers

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def poly(sk, pts, tag, exprs):
    v = Vector
    n = len(pts)
    g = []
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        g.append(sk.addGeometry(Part.LineSegment(v(a[0], a[1], 0), v(b[0], b[1], 0)), False))
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i in range(n):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, pts[i][0]), '%s_x%d' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, pts[i][1]), '%s_y%d' % (tag, i))
        sk.setExpression('Constraints.%s_x%d' % (tag, i), exprs[i][0])
        sk.setExpression('Constraints.%s_y%d' % (tag, i), exprs[i][1])
    return g


def rect(sk, x0, y0, w, h, tag, ex0, ey0, ew, eh):
    pts = [(x0, y0), (x0 + w, y0), (x0 + w, y0 + h), (x0, y0 + h)]
    ex1 = '(%s) + (%s)' % (ex0, ew)
    ey1 = '(%s) + (%s)' % (ey0, eh)
    return poly(sk, pts, tag, [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)])


def circle(sk, cx, cy, r, tag, expr_x, expr_y, expr_r):
    g = sk.addGeometry(Part.Circle(Vector(cx, cy, 0), Vector(0, 0, 1), r), False)
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g, 3, cx), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g, 3, cy), tag + '_y')
    sk.setExpression('Constraints.%s_r' % tag, expr_r)
    sk.setExpression('Constraints.%s_x' % tag, expr_x)
    sk.setExpression('Constraints.%s_y' % tag, expr_y)
    return g


def sketch_on(body, plane, name, expr_off=None):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if expr_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, 0), App.Rotation())
        sk.setExpression('AttachmentOffset.Base.z', expr_off)
    return sk


# -------------------------------------------------------------------- Model

def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Distanzscheibe')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

    def pad(sk, name, expr_len):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        doc.recompute()
        return f

    # 1 Disc
    sk = sketch_on(body, xy, 'Sk_Scheibe')
    circle(sk, 0.0, 0.0, P(sh, 'disc_d') / 2, 'sc', '0', '0', 'Parameter.disc_d / 2')
    pad(sk, 'Scheibe', 'Parameter.disc_t')

    # 2 Rib on the face z = disc_t, along x, in two pieces either side of the
    #   square cut-out nose_gap around the bore
    nb, nl, gap, seg = P(sh, 'nose_b'), P(sh, 'nose_l'), P(sh, 'nose_gap'), P(sh, 'nose_seg')
    sk = sketch_on(body, xy, 'Sk_Nase', expr_off='Parameter.disc_t')
    rect(sk, gap / 2, -nb / 2, seg, nb, 'np',
         'Parameter.nose_gap / 2', '-Parameter.nose_b / 2',
         'Parameter.nose_seg', 'Parameter.nose_b')
    rect(sk, -nl / 2, -nb / 2, seg, nb, 'nm',
         '-Parameter.nose_l / 2', '-Parameter.nose_b / 2',
         'Parameter.nose_seg', 'Parameter.nose_b')
    nase = pad(sk, 'Nase', 'Parameter.nose_h')

    # 2a Fillet: all vertical edges of the rib (parallel to z, above the disc)
    t, h = P(sh, 'disc_t'), P(sh, 'nose_h')
    edges = []
    for i, e in enumerate(nase.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Curve.Direction
        bb = e.BoundBox
        if abs(abs(d.z) - 1.0) < 1e-6 and bb.ZMin > t - 1e-3 and abs(e.Length - h) < 1e-3:
            edges.append('Edge%d' % (i + 1))
    f = body.newObject('PartDesign::Fillet', 'Rundung')
    f.Base = (nase, edges)
    f.setExpression('Radius', 'Parameter.nose_r')
    doc.recompute()
    print('Fillet on %d edges' % len(edges))

    # 3 Hole through all
    sk = sketch_on(body, xy, 'Sk_Loch')
    circle(sk, 0.0, 0.0, P(sh, 'hole_d') / 2, 'ho', '0', '0', 'Parameter.hole_d / 2')
    f = body.newObject('PartDesign::Pocket', 'Loch')
    f.Profile = sk
    f.Type = 'ThroughAll'
    f.Midplane = True
    doc.recompute()
    return body


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


def set_visibility(doc, body):
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    s = body.Shape
    ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    r, t = P(sh, 'disc_d') / 2, P(sh, 'disc_t')
    zm = t / 2
    if inside(0.0, 0.0, zm):
        print('ERROR hole missing'); ok = False
    if not inside(P(sh, 'hole_d') / 2 + 0.5, 0.0, zm):
        print('ERROR no disc next to the hole'); ok = False
    if not inside(r - 0.5, 0.0, zm):
        print('ERROR disc missing at the rim'); ok = False
    if inside(r + 0.5, 0.0, zm):
        print('ERROR material outside the disc (x)'); ok = False
    zn = t + P(sh, 'nose_h') / 2            # rib height
    xs = P(sh, 'nose_gap') / 2 + 2.0        # next to the cut-out, on the rib
    if inside(P(sh, 'nose_gap') / 2 - 0.2, 0.0, zn) or inside(P(sh, 'hole_d') / 2 + 0.1, 0.0, zn):
        print('ERROR cut-out around the bore missing'); ok = False
    if not inside(P(sh, 'nose_gap') / 2 - 0.2, 0.0, zm) and P(sh, 'nose_gap') > P(sh, 'hole_d'):
        print('ERROR disc missing under the cut-out'); ok = False
    if not inside(xs, 0.0, zn):
        print('ERROR rib missing'); ok = False
    if inside(xs, 0.0, t + P(sh, 'nose_h') + 0.5):
        print('ERROR rib too high'); ok = False
    if inside(xs, P(sh, 'nose_b') / 2 + 0.3, zn):
        print('ERROR rib too wide'); ok = False
    # Fillet: the corner itself is gone, 1.5 mm beside it there is still rib
    cx, cy = P(sh, 'nose_gap') / 2 + 0.2, P(sh, 'nose_b') / 2 - 0.2
    if inside(cx, cy, zn):
        print('ERROR inner corner not filleted'); ok = False
    if not inside(cx + 1.5, cy, zn):
        print('ERROR rib next to the fillet missing'); ok = False
    if inside(0.0, 0.0, zn):
        print('ERROR hole does not go through the rib'); ok = False
    if inside(0.0, r - 1.0, zn):
        print('ERROR material on the face next to the rib'); ok = False
    if inside(r + 0.3, 0.0, zn):
        print('ERROR rib protrudes beyond the circumference'); ok = False
    if inside(xs, 0.0, -0.3):
        print('ERROR material below the disc'); ok = False
    print('Rib as printed %.1f wide, 2 x %.1f long, %.1f high, cut-out %.1f, fillet r %.1f; hole Ø %.1f'
          % (P(sh, 'nose_b'), P(sh, 'nose_seg'), P(sh, 'nose_h'), P(sh, 'nose_gap'), P(sh, 'nose_r'), P(sh, 'hole_d')))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('distanzscheibe')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'distanzscheibe.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'distanzscheibe.step'))
    body.Shape.exportStl(os.path.join(here, 'distanzscheibe.stl'))

    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
    print('Valid       %s' % body.Shape.isValid())
    print('Plausible   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None) and 'Invalid' in o.State]
    print('Invalid     %s' % (bad if bad else 'none'))
    offen = [o.Name for o in doc.Objects
             if o.isDerivedFrom('Sketcher::SketchObject') and not getattr(o, 'FullyConstrained', True)]
    print('Sketches    %d, not fully constrained: %s'
          % (len([o for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject')]),
             offen if offen else 'none'))


if __name__ == '__main__':
    main()
