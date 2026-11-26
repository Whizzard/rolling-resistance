# -*- coding: utf-8 -*-
"""
Clip for a block vial (spirit-level vial, 15.1 x 15.1 x 40 mm) on the aluminium
profile, for printing.

Structure as in hall_housing.py and xt60_holder.py: one PartDesign body with a
feature tree plus a spreadsheet "Parameter"; all sketches bound to the table through
expressions. Change dimensions in the GUI in the table, not in the sketches.
Dimensions in mm.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/level_clip.py

Produces hardware/level-clip.FCStd, .step and .stl.

POSITION: z = 0 is the contact face on the profile. x runs along the profile (and
along the vial), y across it, symmetric about y = 0. The vial lies with its long axis
in x, so it measures the inclination of the profile in its longitudinal direction.

Along x, from left to right:

    PAD      END WALL       VIAL 40.0 (+ fit_l)         END WALL       PAD
    M6                                                                 M6

Two M6 button-head screws (ISO 7380) in the profile slot, one at each end, outside
the vial. Under the screw head there are base_t = 1.5 mm of material -- that is the
requirement; the whole base plate is that thick, so the vial lies in the same plane
as the screw seat and the underside is one single face. There is no counterbore, the
head sits on top.

The vial is to remain visible from THREE sides: from above and from both long sides.
The clip holds it at the END FACES and aligns it crosswise with the rails; it is held
with double-sided tape on the floor of the pocket (version 2026-11-24: the earlier
snap noses at the top are gone, the end walls finish FLUSH with the top of the vial).

    End walls     wall_t thick each, exactly vial_h high above the plate, flush
                  with the vial.
    Supports      support slopes OUTSIDE each end wall, one left and one right of
                  the screw head on the long rails: a wedge from the top of the end
                  wall down to the plate end, rail_t wide. They keep the wall from
                  tipping outwards -- the first version with 1.5 mm walls was
                  unstable.
    Rails         two rails rail_h high along the long sides, over the FULL length.
                  They align the vial crosswise and stiffen the base plate; on the
                  outside they merge into the supports.

The screw head sits between the two supports (clear width cav_w, the head has
head_d) with head_clear clearance to the end wall; the screwdriver comes in
vertically from above.

Feature tree:
    BasePlate       block base_l x base_w x base_t, z = 0 is the contact face
    Rails           2 x rail_t wide, rail_h high, full length, outside
    EndWalls       2 x wall_t thick, full width, wall_h = vial_h high
    Supports          wedges in XZ across the full width ...
    Supports_Clear     ... and the middle (cav_w) removed again: two ribs per end
                      remain
    M6_Holes        2 x Ø m6_d through the base plate

Printing: flat, base plate on the bed. No overhangs: the wedges slope down towards
the outside.

The vial size 15.1 x 15.1 x 40 was MEASURED on the part (2026-11-23). The head
diameter of the M6 is the standard value ISO 7380 and only serves to make the pad
long enough -- whoever uses other screws enters head_d.
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
    # Vial, measured 2026-11-23
    ('vial_l',       40.0, 'Vial length (x), measured'),
    ('vial_w',       15.1, 'Vial width (y), measured'),
    ('vial_h',       15.1, 'Vial height (z), measured'),
    ('fit_l',         0.3, 'Play in length, TOTAL'),
    ('fit_w',         0.3, 'Play in width, TOTAL'),

    # Screw M6, button head ISO 7380: dk ~10.5, k ~4
    ('m6_d',          6.6, 'M6 clearance, as on the other holders'),
    ('head_d',       10.5, 'Button head ISO 7380 M6, standard value -- only for the pad length'),
    ('head_edge',     2.0, 'Margin beside the head up to the plate end'),
    ('head_clear',    1.0, 'Clearance between head and end wall'),

    # Body (2026-11-24: more material than the first version with 1.5 / 1.2 / 1.5)
    ('base_t',        2.0, 'Base plate thickness = material under the screw head'),
    ('rail_t',        1.6, 'Long rail thickness, also width of the supports'),
    ('rail_h',        2.0, 'Long rail height above the plate; this much is hidden at the side'),
    ('wall_t',        2.0, 'End wall thickness'),

    # Derived
    ('cav_l',  '=vial_l + fit_l',                  'Pocket length'),
    ('cav_w',  '=vial_w + fit_w',                  'Pocket width'),
    ('base_w', '=cav_w + 2 * rail_t',              'Base plate width'),
    ('m6_x',   '=head_edge + head_d / 2',          'first screw from the plate end'),
    ('cav_x0', '=m6_x + head_d / 2 + head_clear + wall_t', 'Pocket starts'),
    ('base_l', '=2 * cav_x0 + cav_l',              'Base plate length'),
    ('m6_pitch', '=base_l - 2 * m6_x',             'Hole spacing of the M6'),
    ('wall_h', '=vial_h',                          'End wall height above the plate, flush with the vial'),
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
    """Numeric value of a parameter."""
    return float(sh.get(name))


# ----------------------------------------------------------- Sketch helpers

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def poly(sk, pts, tag, exprs):
    """Closed polyline, every vertex bound to expressions through
    DistanceX/DistanceY -> fully constrained. pts: [(x, y), ...], exprs:
    [(expr_x, expr_y), ...] in the same order."""
    v = Vector
    n = len(pts)
    g = []
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        g.append(sk.addGeometry(Part.LineSegment(v(a[0], a[1], 0), v(b[0], b[1], 0)),
                                False))
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i in range(n):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, pts[i][0]),
              '%s_x%d' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, pts[i][1]),
              '%s_y%d' % (tag, i))
        sk.setExpression('Constraints.%s_x%d' % (tag, i), exprs[i][0])
        sk.setExpression('Constraints.%s_y%d' % (tag, i), exprs[i][1])
    return g


def rect(sk, x0, y0, w, h, tag, ex0, ey0, ew, eh):
    """Axis-parallel rectangle via poly; expressions for corner and dimensions."""
    pts = [(x0, y0), (x0 + w, y0), (x0 + w, y0 + h), (x0, y0 + h)]
    ex1 = '(%s) + (%s)' % (ex0, ew)
    ey1 = '(%s) + (%s)' % (ey0, eh)
    exprs = [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)]
    return poly(sk, pts, tag, exprs)


def circle(sk, cx, cy, r, tag, expr_x, expr_y, expr_r):
    g = sk.addGeometry(Part.Circle(Vector(cx, cy, 0), Vector(0, 0, 1), r), False)
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g, 3, cx), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g, 3, cy), tag + '_y')
    sk.setExpression('Constraints.%s_r' % tag, expr_r)
    sk.setExpression('Constraints.%s_x' % tag, expr_x)
    sk.setExpression('Constraints.%s_y' % tag, expr_y)
    return g


# -------------------------------------------------------------------- Model

def sketch_on(body, plane, name, off=0.0, expr_off=None):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if off or expr_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, off), App.Rotation())
    if expr_off:
        sk.setExpression('AttachmentOffset.Base.z', expr_off)
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'LevelClip')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, xz = planes['XY_Plane'], planes['XZ_Plane']

    def pad(sk, name, expr_len, midplane=False):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Midplane = midplane
        doc.recompute()
        return f

    def pocket_through(sk, name):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    base_l, base_w, base_t = P(sh, 'base_l'), P(sh, 'base_w'), P(sh, 'base_t')
    cav_x0, cav_l, wall_t = P(sh, 'cav_x0'), P(sh, 'cav_l'), P(sh, 'wall_t')
    cav_x1 = cav_x0 + cav_l

    # 1 Base plate. z = 0 is the contact face on the profile.
    sk = sketch_on(body, xy, 'Sk_BasePlate')
    rect(sk, 0.0, -base_w / 2, base_l, base_w, 'bp',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.base_w')
    pad(sk, 'BasePlate', 'Parameter.base_t')

    # 2 Long rails outside, full length, on the plate.
    sk = sketch_on(body, xy, 'Sk_Rails', expr_off='Parameter.base_t')
    rect(sk, 0.0, base_w / 2 - P(sh, 'rail_t'), base_l, P(sh, 'rail_t'), 'rp',
         '0', 'Parameter.base_w / 2 - Parameter.rail_t',
         'Parameter.base_l', 'Parameter.rail_t')
    rect(sk, 0.0, -base_w / 2, base_l, P(sh, 'rail_t'), 'rm',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.rail_t')
    pad(sk, 'Rails', 'Parameter.rail_h')

    # 3 End walls, full width, on the plate.
    sk = sketch_on(body, xy, 'Sk_EndWalls', expr_off='Parameter.base_t')
    rect(sk, cav_x0 - wall_t, -base_w / 2, wall_t, base_w, 'wl',
         'Parameter.cav_x0 - Parameter.wall_t', '-Parameter.base_w / 2',
         'Parameter.wall_t', 'Parameter.base_w')
    rect(sk, cav_x1, -base_w / 2, wall_t, base_w, 'wr',
         'Parameter.cav_x0 + Parameter.cav_l', '-Parameter.base_w / 2',
         'Parameter.wall_t', 'Parameter.base_w')
    pad(sk, 'EndWalls', 'Parameter.wall_h')

    # 4 Support slopes: wedges in the XZ plane (local x = global x, local y =
    #   global z), outside each end wall from its top edge down to the plate
    #   end, starting at rail height. First padded across the full width, then
    #   the middle (cav_w) removed again -> two ribs rail_t wide per end, on the
    #   long rails, with the screw head between them.
    ztop = base_t + P(sh, 'wall_h')
    zrail = base_t + P(sh, 'rail_h')
    e_top = 'Parameter.base_t + Parameter.wall_h'
    e_rail = 'Parameter.base_t + Parameter.rail_h'
    xl, xr = cav_x0 - wall_t, cav_x1 + wall_t
    e_xl, e_xr = 'Parameter.cav_x0 - Parameter.wall_t', 'Parameter.cav_x0 + Parameter.cav_l + Parameter.wall_t'
    sk = sketch_on(body, xz, 'Sk_Supports')
    poly(sk, [(0.0, zrail), (xl, zrail), (xl, ztop)], 'sl',
         [('0', e_rail), (e_xl, e_rail), (e_xl, e_top)])
    poly(sk, [(xr, zrail), (base_l, zrail), (xr, ztop)], 'sr',
         [(e_xr, e_rail), ('Parameter.base_l', e_rail), (e_xr, e_top)])
    pad(sk, 'Supports', 'Parameter.base_w', midplane=True)
    sk = sketch_on(body, xz, 'Sk_Supports_Clear')
    rect(sk, 0.0, zrail, xl, ztop - zrail + 1.0, 'fl',
         '0', e_rail, e_xl, 'Parameter.wall_h - Parameter.rail_h + 1')
    rect(sk, xr, zrail, base_l - xr, ztop - zrail + 1.0, 'fr',
         e_xr, e_rail, 'Parameter.base_l - (' + e_xr + ')', 'Parameter.wall_h - Parameter.rail_h + 1')
    f = body.newObject('PartDesign::Pocket', 'Supports_Clear')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.cav_w')
    f.Midplane = True
    doc.recompute()

    # 5 M6 holes, one per pad, through the base plate.
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6l',
           'Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    circle(sk, base_l - P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6r',
           'Parameter.base_l - Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    pocket_through(sk, 'M6_Holes')

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
    """Body visible, sketches and intermediate steps hidden."""
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    """Geometric plausibility, so that a sign error in the sketch planes is not
    first noticed in the slicer."""
    s = body.Shape
    ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    base_t, base_w, base_l = P(sh, 'base_t'), P(sh, 'base_w'), P(sh, 'base_l')
    cav_x0, cav_l = P(sh, 'cav_x0'), P(sh, 'cav_l')
    cav_x1 = cav_x0 + cav_l
    xc = (cav_x0 + cav_x1) / 2
    ztop = base_t + P(sh, 'wall_h')
    zrail = base_t + P(sh, 'rail_h')

    # Base plate: material under the head, air in the hole
    for x in (P(sh, 'm6_x'), base_l - P(sh, 'm6_x')):
        if not inside(x + P(sh, 'm6_d') / 2 + 0.5, 0.0, base_t / 2):
            print('ERROR no plate next to the hole at x = %.1f' % x); ok = False
        if inside(x, 0.0, base_t / 2):
            print('ERROR M6 hole missing at x = %.1f' % x); ok = False
        if inside(x, 0.0, base_t + 0.5):
            print('ERROR material above the head at x = %.1f' % x); ok = False
    # Pocket: air in the middle and directly above the top of the vial
    if inside(xc, 0.0, base_t + P(sh, 'vial_h') / 2):
        print('ERROR pocket not clear'); ok = False
    if inside(xc, 0.0, ztop + 0.5):
        print('ERROR material above the pocket'); ok = False
    # Vial rests on the plate: material directly beneath
    if not inside(xc, 0.0, base_t / 2):
        print('ERROR no floor under the vial'); ok = False
    # Rails: material outside, air inside at rail height
    for sign in (+1, -1):
        y_rail = sign * (base_w / 2 - P(sh, 'rail_t') / 2)
        if not inside(xc, y_rail, base_t + P(sh, 'rail_h') / 2):
            print('ERROR no rail at y = %+.1f' % y_rail); ok = False
        if inside(xc, sign * (P(sh, 'cav_w') / 2 - 0.1), base_t + P(sh, 'rail_h') / 2):
            print('ERROR rail protrudes into the pocket at y = %+.1f' % y_rail); ok = False
    # End walls: material at mid-wall, up to the top
    for xw in (cav_x0 - P(sh, 'wall_t') / 2, cav_x1 + P(sh, 'wall_t') / 2):
        if not inside(xw, 0.0, ztop - 0.2):
            print('ERROR end wall missing at x = %.1f' % xw); ok = False
    # Supports: rib outside at half height, air beside it (above the head) and
    # above the slope
    for xw, sign in ((cav_x0 - P(sh, 'wall_t'), -1), (cav_x1 + P(sh, 'wall_t'), +1)):
        xm = xw + sign * 1.0                       # 1 mm in front of the wall: wedge almost full height
        zq = zrail + 0.5 * (ztop - zrail)
        yr = base_w / 2 - P(sh, 'rail_t') / 2
        if not inside(xm, yr, zq):
            print('ERROR support missing at x = %.1f' % xm); ok = False
        if inside(xm, 0.0, zq):
            print('ERROR support above the screw head at x = %.1f' % xm); ok = False
        xe = xw + sign * (P(sh, 'cav_x0') - P(sh, 'wall_t') - 0.5)   # just before the plate end
        if inside(xe, yr, zq):
            print('ERROR slope does not fall away at x = %.1f' % xe); ok = False
    # End wall flush: no material above ztop
    if inside(cav_x0 - P(sh, 'wall_t') / 2, 0.0, ztop + 0.3):
        print('ERROR end wall stands above the vial'); ok = False
    # Head clear next to the wall
    gap = cav_x0 - P(sh, 'wall_t') - (P(sh, 'm6_x') + P(sh, 'head_d') / 2)
    print('Clearance head to end wall %.1f mm, hole spacing %.1f mm' % (gap, P(sh, 'm6_pitch')))
    print('Hidden at the side %.1f of %.1f mm, end wall flush, supports %.1f wide'
          % (P(sh, 'rail_h'), P(sh, 'vial_h'), P(sh, 'rail_t')))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('level-clip')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'level-clip.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'level-clip.step'))
    body.Shape.exportStl(os.path.join(here, 'level-clip.stl'))

    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
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


main()
