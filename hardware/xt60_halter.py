# -*- coding: utf-8 -*-
"""
Holder for the XT60 panel-mount connector (Amass XT60E-M) on the aluminium frame.

Structure as in hall_gehaeuse.py and hx711_gehaeuse.py: one PartDesign body with a
feature tree plus a spreadsheet "Parameter"; all sketches bound to the table through
expressions. Change dimensions in the GUI in the table, not in the sketches.
Dimensions in mm.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/xt60_halter.py

Produces hardware/xt60-halter.FCStd, .step and .stl.

POSITION: the connector LIES FLAT -- its long axis (the two ears) parallel to the
mounting surface, the mating direction parallel to the profile. The cable thus runs
along the frame, not away from it.

An L bracket: a base plate with ONE central M6 screw onto the profile, with an end
wall carrying the cut-out for the connector. The connector shell goes through the
wall, the ears rest against the BACK of the wall and are screwed with two M3 from
the front through the wall -- the nuts sit in the hex pockets the connector has in
its ears on the solder side. Two triangular ribs outside brace the wall against the
mating force.

ALL CONNECTOR DIMENSIONS come from the dimensioned drawing in the Amass datasheet
XT60E-M (Changzhou Amass Electronics, "XT60E-M.pdf"), read 2026-11-22:

    Front view      27.00 overall length across the ears
                    20.00 hole spacing of the ears (centre-centre)  <- READING, see below
                    15.50 shell width, 8.10 shell height (outer contour)
                    14.20 inner contour, 7.20 contact spacing
    Side view       16.70 overall depth from the solder tab to the mating face
                     4.00 flange thickness (the ears)
                     9.50 from the solder tab to ... (reference edge not
                          unambiguous in the copy); in any reading the shell
                          protrudes at least 7 mm beyond the flange
    Rear view       Ø 3.00 ear hole, hex pocket for the nut

The drawing in the datasheet is a small raster image. The hole spacing is the ONLY
dimension where a reading error makes the holder useless -- dealer listings online
give 16, 19 and 20 mm for "XT60 panel mount", depending on the variant. BEFORE
PRINTING measure it on the connector with calipers and set `con_pitch` in the table.

FULLY OPEN AT THE TOP: the cut-out runs at full shell width up to the top edge of
the wall. The wall is thus two posts, the connector is dropped in from above with
the leads already soldered on, and there is no bridge the printer would have to
span. The posts end at the height of the top of the shell; the ears (6.7 high,
centred) rest fully against them.

About the web next to the ear hole: on the XT60E-M it is inherently paper-thin --
hole spacing 20.0 with a 15.5 shell and Ø 3 means 2.25 mm from the hole edge to the
shell contour, with play and M3 clearance 0.4 mm remain. This holds with and
without the slot, it is the geometry of the connector. The slicer prints the sliver
or leaves it out, either is fine: the screw is guided by the ear, and three
quarters of the head rest on the post.

Feature tree:
    Grundplatte       block base_l x base_w x base_t, z = 0 is the contact face
    Stirnwand         block wall_t x base_w x wall_h at x = 0
    Rippe_p/_m        triangle in XZ, one at each outer edge
    Ausschnitt        rectangle through the wall, from the bottom of the shell to
                      above the top edge -> two posts
    Ohrloecher        2 x Ø screw_d through the wall, spacing con_pitch
    M6_Durchgang      Ø m6_d through the base plate, central
    M6_Senkung        Ø m6_head_d, m6_cb_t deep, from above -- socket head
    Fasen             on the upper long edges of the base plate

Assembly order: FIRST tighten the M6, THEN insert the connector -- the connector
lies above the screw, with the connector in place no key can be applied. That is
why the head is countersunk and the cut-out placed high enough that the shell
clears the screw head (con_z).

No key for the profile slot: as with the other holders, the clamping of the M6
alone holds it. Whoever wants a key sets key_w to the slot width of the profile
(measure it on the profile, do not guess) -- then a strip is added underneath.
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
    # Connector, datasheet Amass XT60E-M (see header)
    ('con_l',        27.0, 'Overall length across the ears, datasheet'),
    ('con_pitch',    20.0, 'Hole spacing of the ears, datasheet READING -> MEASURE'),
    ('con_hole_d',    3.0, 'Ear hole, datasheet'),
    ('con_shell_w',  15.5, 'Shell width (long side), datasheet'),
    ('con_shell_h',   8.1, 'Shell height (short side), datasheet'),
    ('con_flange_t',  4.0, 'Flange thickness, datasheet -- for information only'),
    ('con_depth',    16.7, 'Overall depth, datasheet -- for information only'),

    ('fit_shell',     0.5, 'Play at the cut-out, TOTAL -> 0.25 per side'),
    ('screw_d',       3.2, 'M3 clearance in the wall'),
    ('cut_w', '=con_shell_w + fit_shell', 'Cut-out width'),
    ('cut_h', '=con_shell_h + fit_shell', 'Cut-out height'),

    # Screw M6, socket head DIN 912: k = 6, dk = 10
    ('m6_d',          6.6, 'M6 clearance, as on the other holders'),
    ('m6_head_d',    11.0, 'Counterbore for the socket head, dk 10 + play'),
    ('m6_head_h',     6.0, 'Head height DIN 912 M6'),
    ('m6_cb_t',       3.0, 'Counterbore depth -> head still stands 3 mm proud'),
    ('head_clear',    1.5, 'Clearance between head and connector shell'),

    # Body
    ('base_t',        6.0, 'Base plate thickness; 3 mm counterbore leaves 3 mm floor'),
    ('base_l',       40.0, 'Base plate length, in mating direction'),
    ('base_w',       34.0, 'Base plate width; ears 27 + 2 ribs of 3 + clearance'),
    ('wall_t',        3.0, 'End wall thickness; the shell protrudes >= 7 mm'),
    ('rib_t',         3.0, 'Rib thickness'),
    ('rib_l',        22.0, 'Rib length at the bottom, from the back of the wall'),
    ('chamfer',       1.0, 'Chamfer on the upper long edges of the base plate'),
    ('key_w',         0.0, 'Slot key width; 0 = none. MEASURE the slot width on the profile'),
    ('key_h',         0.0, 'Slot key height; 0 = none'),

    ('m6_x',  '=base_l / 2',                       'Screw central'),
    ('con_z', '=base_t + m6_head_h - m6_cb_t + head_clear + cut_h / 2',
              'Centre of the cut-out above the contact face'),
    ('wall_h', '=con_z + cut_h / 2',               'Posts end at the top of the shell'),
    ('rib_y',  '=base_w / 2 - rib_t / 2',          'Rib centre, magnitude'),
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


def rect(sk, x0, y0, w, h, tag, expr_x0, expr_y0, expr_w, expr_h):
    """Axis-parallel rectangle, fully constrained and bound to expressions."""
    v = Vector
    g = [sk.addGeometry(Part.LineSegment(v(x0, y0, 0), v(x0 + w, y0, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0 + w, y0, 0), v(x0 + w, y0 + h, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0 + w, y0 + h, 0), v(x0, y0 + h, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0, y0 + h, 0), v(x0, y0, 0)), False)]
    for i in range(4):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 4], 1))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[0]))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[1]))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[2]))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[3]))
    named(sk, Sketcher.Constraint('DistanceX', g[0], 1, g[0], 2, w), tag + '_w')
    named(sk, Sketcher.Constraint('DistanceY', g[1], 1, g[1], 2, h), tag + '_h')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x0), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, y0), tag + '_y')
    sk.setExpression('Constraints.%s_w' % tag, expr_w)
    sk.setExpression('Constraints.%s_h' % tag, expr_h)
    sk.setExpression('Constraints.%s_x' % tag, expr_x0)
    sk.setExpression('Constraints.%s_y' % tag, expr_y0)
    return g


def circle(sk, cx, cy, r, tag, expr_x, expr_y, expr_r):
    g = sk.addGeometry(Part.Circle(Vector(cx, cy, 0), Vector(0, 0, 1), r), False)
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g, 3, cx), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g, 3, cy), tag + '_y')
    sk.setExpression('Constraints.%s_r' % tag, expr_r)
    sk.setExpression('Constraints.%s_x' % tag, expr_x)
    sk.setExpression('Constraints.%s_y' % tag, expr_y)
    return g


def triangle(sk, x0, y0, w, h, tag, expr_x0, expr_y0, expr_w, expr_h):
    """Right triangle: leg w along +x, leg h along +y, right angle at (x0, y0)."""
    v = Vector
    g = [sk.addGeometry(Part.LineSegment(v(x0, y0, 0), v(x0 + w, y0, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0 + w, y0, 0), v(x0, y0 + h, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0, y0 + h, 0), v(x0, y0, 0)), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[0]))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', g[0], 1, g[0], 2, w), tag + '_w')
    named(sk, Sketcher.Constraint('DistanceY', g[2], 2, g[2], 1, h), tag + '_h')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x0), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, y0), tag + '_y')
    sk.setExpression('Constraints.%s_w' % tag, expr_w)
    sk.setExpression('Constraints.%s_h' % tag, expr_h)
    sk.setExpression('Constraints.%s_x' % tag, expr_x0)
    sk.setExpression('Constraints.%s_y' % tag, expr_y0)
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
    body = doc.addObject('PartDesign::Body', 'Halter')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, xz, yz = planes['XY_Plane'], planes['XZ_Plane'], planes['YZ_Plane']

    def pad(sk, name, expr_len, midplane=False, reversed_=False):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Midplane = midplane
        f.Reversed = reversed_
        doc.recompute()
        return f

    def pocket_through(sk, name):
        """Through-all pocket in both directions -- regardless of where the
        sketch normal points."""
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    def pocket(sk, name, expr_len, reversed_=False):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Reversed = reversed_
        doc.recompute()
        return f

    # 1 Base plate. z = 0 is the contact face on the profile.
    sk = sketch_on(body, xy, 'Sk_Grundplatte')
    rect(sk, 0.0, -P(sh, 'base_w') / 2, P(sh, 'base_l'), P(sh, 'base_w'), 'gp',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.base_w')
    pad(sk, 'Grundplatte', 'Parameter.base_t')

    # 2 End wall at x = 0, across the full width.
    sk = sketch_on(body, xy, 'Sk_Stirnwand')
    rect(sk, 0.0, -P(sh, 'base_w') / 2, P(sh, 'wall_t'), P(sh, 'base_w'), 'sw',
         '0', '-Parameter.base_w / 2', 'Parameter.wall_t', 'Parameter.base_w')
    pad(sk, 'Stirnwand', 'Parameter.wall_h')

    # 3 Ribs: triangle in the XZ plane (local x = global x, local y = global z),
    #   one each at y = +rib_y and y = -rib_y, padded symmetrically about the
    #   sketch plane. In FreeCAD the sketch normal of the XZ plane points to -y;
    #   the offset is therefore entered with the opposite sign and is checked
    #   against the body below.
    for tag, sign in (('p', +1), ('m', -1)):
        sk = sketch_on(body, xz, 'Sk_Rippe_' + tag,
                       expr_off='%d * Parameter.rib_y' % (-sign))
        triangle(sk, P(sh, 'wall_t'), P(sh, 'base_t'), P(sh, 'rib_l'),
                 P(sh, 'wall_h') - P(sh, 'base_t'), 'rb' + tag,
                 'Parameter.wall_t', 'Parameter.base_t', 'Parameter.rib_l',
                 'Parameter.wall_h - Parameter.base_t')
        pad(sk, 'Rippe_' + tag, 'Parameter.rib_t', midplane=True)

    # 4 Cut-out for the shell, through the wall and open at the top: from the
    #   bottom of the shell to above the top edge of the wall. Sketch in YZ:
    #   local x = global y, local y = global z.
    sk = sketch_on(body, yz, 'Sk_Ausschnitt')
    z0 = P(sh, 'con_z') - P(sh, 'cut_h') / 2
    rect(sk, -P(sh, 'cut_w') / 2, z0, P(sh, 'cut_w'), P(sh, 'wall_h') - z0 + 1.0, 'cut',
         '-Parameter.cut_w / 2', 'Parameter.con_z - Parameter.cut_h / 2',
         'Parameter.cut_w', 'Parameter.wall_h - (Parameter.con_z - Parameter.cut_h / 2) + 1')
    pocket_through(sk, 'Ausschnitt')

    # 5 Ear holes, M3 through the wall, at the height of the cut-out centre.
    sk = sketch_on(body, yz, 'Sk_Ohrloecher')
    for tag, sign in (('p', +1), ('m', -1)):
        circle(sk, sign * P(sh, 'con_pitch') / 2, P(sh, 'con_z'),
               P(sh, 'screw_d') / 2, 'ohr' + tag,
               '%d * Parameter.con_pitch / 2' % sign, 'Parameter.con_z',
               'Parameter.screw_d / 2')
    pocket_through(sk, 'Ohrloecher')

    # 6 M6 centrally through the base plate.
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6',
           'Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    pocket_through(sk, 'M6_Durchgang')

    # 7 Counterbore for the socket head, from the top of the base plate
    #   downwards. The XY normal points to +z, the pocket without Reversed goes
    #   against the normal, i.e. down into the plate.
    sk = sketch_on(body, xy, 'Sk_M6_Senkung', expr_off='Parameter.base_t')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_head_d') / 2, 'cb',
           'Parameter.m6_x', '0', 'Parameter.m6_head_d / 2')
    pocket(sk, 'M6_Senkung', 'Parameter.m6_cb_t')

    # 8 Slot key, only if key_w > 0: strip under the base plate, lengthwise,
    #   centred. Width = slot width of the profile, measured on the profile.
    if P(sh, 'key_w') > 0 and P(sh, 'key_h') > 0:
        sk = sketch_on(body, xy, 'Sk_Nutnase')
        rect(sk, 0.0, -P(sh, 'key_w') / 2, P(sh, 'base_l'), P(sh, 'key_w'), 'key',
             '0', '-Parameter.key_w / 2', 'Parameter.base_l', 'Parameter.key_w')
        pad(sk, 'Nutnase', 'Parameter.key_h', reversed_=True)

    # 9 Chamfers on the two upper long edges of the base plate (exposed behind
    #   the ribs). The edges sought run along x at z = base_t at y = +-base_w/2.
    tip = body.Tip
    shape = tip.Shape
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.y - b.y) > 1e-6 or abs(a.z - b.z) > 1e-6:
            continue                                   # not along x
        if abs(a.z - P(sh, 'base_t')) > 1e-6:
            continue                                   # not on the top edge
        if abs(abs(a.y) - P(sh, 'base_w') / 2) > 1e-6:
            continue                                   # not outside
        namen.append('Edge%d' % (i + 1))
    if len(namen) == 2:
        f = body.newObject('PartDesign::Chamfer', 'Fasen')
        f.Base = (tip, namen)
        f.setExpression('Size', 'Parameter.chamfer')
        doc.recompute()
        if 'Invalid' in (f.State or []):
            print('Chamfers failed, left out')
            body.removeObject(f)
            doc.removeObject(f.Name)
            doc.recompute()
    else:
        print('Long edges ambiguous (%s), chamfers left out' % namen)

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
    bb = s.BoundBox
    ok = True
    # Ribs must lie within the width of the base plate
    if bb.YLength > P(sh, 'base_w') + 1e-3:
        print('ERROR ribs protrude sideways: width %.2f' % bb.YLength); ok = False
    # Rib: material at (wall_t + 1, +-rib_y, base_t + 1)
    for sign in (+1, -1):
        p = Vector(P(sh, 'wall_t') + 1.0, sign * P(sh, 'rib_y'), P(sh, 'base_t') + 1.0)
        if not s.isInside(p, 1e-6, True):
            print('ERROR no rib at y = %+.1f' % (sign * P(sh, 'rib_y'))); ok = False
    # Cut-out: air in the middle of the wall at connector height
    p = Vector(P(sh, 'wall_t') / 2, 0.0, P(sh, 'con_z'))
    if s.isInside(p, 1e-6, True):
        print('ERROR cut-out missing'); ok = False
    # Ear hole: air
    p = Vector(P(sh, 'wall_t') / 2, P(sh, 'con_pitch') / 2, P(sh, 'con_z'))
    if s.isInside(p, 1e-6, True):
        print('ERROR ear hole missing'); ok = False
    # Open at the top: air at the top edge of the wall, centred and at the cut-out rim
    for y in (0.0, P(sh, 'cut_w') / 2 - 0.2):
        p = Vector(P(sh, 'wall_t') / 2, y, P(sh, 'wall_h') - 0.5)
        if s.isInside(p, 1e-6, True):
            print('ERROR cut-out not open to the top at y = %.1f' % y); ok = False
    # Post: material outside next to the ear hole
    p = Vector(P(sh, 'wall_t') / 2, P(sh, 'con_pitch') / 2 + P(sh, 'screw_d') / 2 + 1.0,
               P(sh, 'con_z'))
    if not s.isInside(p, 1e-6, True):
        print('ERROR no post outside next to the ear hole'); ok = False
    web = P(sh, 'con_pitch') / 2 - P(sh, 'screw_d') / 2 - P(sh, 'cut_w') / 2
    print('Web ear hole to cut-out %.1f mm (connector geometry, see header)' % web)
    # M6: air through the plate, counterbore at the top, floor at the bottom
    if s.isInside(Vector(P(sh, 'm6_x'), 0.0, P(sh, 'base_t') / 2), 1e-6, True):
        print('ERROR M6 clearance hole missing'); ok = False
    if s.isInside(Vector(P(sh, 'm6_x'), P(sh, 'm6_head_d') / 2 - 0.5,
                         P(sh, 'base_t') - 0.5), 1e-6, True):
        print('ERROR counterbore missing'); ok = False
    if not s.isInside(Vector(P(sh, 'm6_x'), P(sh, 'm6_head_d') / 2 - 0.5, 0.5),
                      1e-6, True):
        print('ERROR counterbore goes through'); ok = False
    # Shell above the screw head: bottom of cut-out > top of head
    head_top = P(sh, 'base_t') - P(sh, 'm6_cb_t') + P(sh, 'm6_head_h')
    cut_bot = P(sh, 'con_z') - P(sh, 'cut_h') / 2
    print('Head top    z = %.1f, shell bottom z = %.1f, clearance %.1f'
          % (head_top, cut_bot, cut_bot - head_top))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('xt60-halter')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'xt60-halter.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'xt60-halter.step'))
    body.Shape.exportStl(os.path.join(here, 'xt60-halter.stl'))

    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
    print('Valid       %s' % body.Shape.isValid())
    print('Plausible   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None)
           and 'Invalid' in o.State]
    print('Invalid     %s' % (bad if bad else 'none'))
    offen = [o.Name for o in doc.Objects
             if o.isDerivedFrom('Sketcher::SketchObject')
             and not getattr(o, 'FullyConstrained', True)]
    print('Sketches    %d, not fully constrained: %s'
          % (len([o for o in doc.Objects
                  if o.isDerivedFrom('Sketcher::SketchObject')]),
             offen if offen else 'none'))


main()
