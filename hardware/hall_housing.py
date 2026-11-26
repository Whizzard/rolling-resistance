# -*- coding: utf-8 -*-
"""
Housing for a Hall sensor (TLE4905L) at the drum.

Produces hall-housing.FCStd as a **PartDesign body with a feature tree** plus a
spreadsheet "Parameter". All sketches are bound to the spreadsheet through
expressions — dimensions are changed in the GUI in the table, not in the sketches.

This script is the generator, not the truth: whoever works in the GUI maintains
the FCStd. Whoever wants to rebuild the tree runs this. Dimensions in mm.

Feature tree (object names as in FreeCAD):
    BaseBlock   base block, cuboid 37.2 x 20 x 12
    Cavity       cavity, wide part, z = 0...3.6  (board passage)
    ComponentSpace    component space, narrow part, z = 0...10  -> creates the ledge
    LedgeChamfers   chamfer at the inner lower edge of the ledge
    CableRamp     cable ramp: the ledge rises at the cable end so nothing jams
    SnapNoses      snap lugs: two wedges, centred, below the board
    SpringRelief      spring relief outside, so the wall flexes at the lugs
    ScrewHole    screw hole, M6 through
    CableGroove       cable groove, clamping section, D profile (rectangle + round)
    CableTrough     cable recess, deeper, in front of the board, no clamping there
    LED_Hole       LED hole, Ø3.2 in the lid

Z = 0 is the open underside, which lies on the aluminium sheet.
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
    ('pcb_l',       18.0,  'Board length'),
    ('pcb_w',       13.0,  'Board width'),
    ('pcb_t',        1.6,  'Board thickness'),
    ('solder_gap',   2.0,  'Solder space below the board'),
    ('headroom',     6.4,  'Component space above the board'),

    ('wall',         3.0,  'Side wall'),
    ('top',          2.0,  'Lid wall'),
    ('end_wall',     1.2,  'End wall in front of the sensor'),
    ('rib',          2.0,  'Web to the screw'),
    ('screw_blk',   14.0,  'Length of the solid block'),

    ('cav_l',  '=pcb_l + 2',                     'Cavity length'),
    ('cav_w',  '=pcb_w + 1',                     'Cavity width'),
    ('cav_h',  '=solder_gap + pcb_t + headroom', 'Cavity height'),
    ('cav_x0', '=end_wall',                      'Cavity start'),
    ('cav_xm', '=cav_x0 + cav_l / 2',            'Cavity centre'),

    ('l_out',  '=end_wall + cav_l + rib + screw_blk', 'Outer length'),
    ('w_out',  '=cav_w + 2 * wall',                   'Outer width'),
    ('h_out',  '=cav_h + top',                        'Outer height'),

    ('pcb_z0', '=solder_gap',          'Board lower edge'),
    ('pcb_z1', '=pcb_z0 + pcb_t',      'Board upper edge = ledge'),

    ('ledge_w',      1.8,  'Ledge per side'),
    ('ledge_chamfer', 0.4, 'Chamfer at the lower edge of the ledge, inside'),
    ('ramp_l',       4.0,  'Length of the cable ramp at the cable end'),
    ('ramp_h',       1.5,  'Height of the cable ramp above the board upper edge'),
    ('snap_w',       0.65, 'Snap lug protrusion per side'),
    ('snap_l',       6.0,  'Snap lug length'),
    ('snap_h',       0.8,  'Snap lug half height'),
    ('snap_z', '=pcb_z0',  'Apex of the snap lug'),

    ('relief_l',    12.0,  'Spring relief length'),
    ('relief_t',     0.8,  'Spring relief remaining wall'),
    ('relief_h', '=pcb_z1 + 2', 'Spring relief height'),

    ('m6_d',         6.6,  'M6 clearance'),
    ('m6_y',        -4.5,  'M6 offset from the centre'),
    ('m6_x',  '=cav_x0 + cav_l + rib + screw_blk / 2', 'M6 position'),

    ('cab_d',        4.4,  'Cable groove width'),
    ('cab_depth',    3.4,  'Cable groove depth, clamping section'),
    ('cab_y',        5.5,  'Cable groove position'),
    ('cab_x0', '=cav_x0 + cav_l - 3', 'Cable groove start'),
    ('cab_len','=l_out - cab_x0 + 1', 'Cable groove length'),
    ('cab_deep',     5.2,  'Recess depth, no clamping'),
    ('cab_deep_x0', 20.0,  'Recess start'),
    ('cab_deep_l',   6.0,  'Recess length'),

    ('led_d',        3.2,  'LED through hole'),
    ('led_x',       15.0,  'LED position'),
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


def wedge(sk, sign, yw, zc, dw, dh, tag):
    """Symmetric wedge on a wall. Sketch in YZ: local x = global y."""
    v = Vector
    x_wall = sign * yw
    x_tip = x_wall - sign * dw
    p_bot = v(x_wall, zc - dh, 0)
    p_tip = v(x_tip, zc, 0)
    p_top = v(x_wall, zc + dh, 0)
    g = [sk.addGeometry(Part.LineSegment(p_bot, p_tip), False),
         sk.addGeometry(Part.LineSegment(p_tip, p_top), False),
         sk.addGeometry(Part.LineSegment(p_top, p_bot), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x_wall), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, zc - dh), tag + '_z')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[1], 2, 2 * dh), tag + '_hh')
    named(sk, Sketcher.Constraint('DistanceX', g[0], 1, g[0], 2, -sign * dw), tag + '_dw')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[0], 2, dh), tag + '_dh')
    s = '%d' % sign
    sk.setExpression('Constraints.%s_x' % tag, '%s * (Parameter.cav_w / 2)' % s)
    sk.setExpression('Constraints.%s_z' % tag, 'Parameter.snap_z - Parameter.snap_h')
    sk.setExpression('Constraints.%s_hh' % tag, '2 * Parameter.snap_h')
    sk.setExpression('Constraints.%s_dw' % tag, '%d * Parameter.snap_w' % -sign)
    sk.setExpression('Constraints.%s_dh' % tag, 'Parameter.snap_h')
    return g


# -------------------------------------------------------------------- Model

def sketch_on(body, plane, name, x_off=0.0):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if x_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, x_off), App.Rotation())
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Housing')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, yz = planes['XY_Plane'], planes['YZ_Plane']

    def pad(sk, name, expr_len, reversed_=False):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Reversed = reversed_
        doc.recompute()
        return f

    def pocket(sk, name, expr_len, reversed_=True):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Reversed = reversed_
        doc.recompute()
        return f

    # 1 Base block
    sk = sketch_on(body, xy, 'Sk_Outer')
    rect(sk, 0.0, -P(sh, 'w_out') / 2, P(sh, 'l_out'), P(sh, 'w_out'), 'outer',
         '0', '-Parameter.w_out / 2', 'Parameter.l_out', 'Parameter.w_out')
    pad(sk, 'BaseBlock', 'Parameter.h_out')

    # 2 Cavity, wide part up to the board upper edge
    sk = sketch_on(body, xy, 'Sk_Cavity')
    rect(sk, P(sh, 'cav_x0'), -P(sh, 'cav_w') / 2, P(sh, 'cav_l'), P(sh, 'cav_w'), 'cav',
         'Parameter.cav_x0', '-Parameter.cav_w / 2', 'Parameter.cav_l', 'Parameter.cav_w')
    pocket(sk, 'Cavity', 'Parameter.pcb_z1')

    # 3 Component space, narrow part through the full height → creates the ledge
    inner = P(sh, 'cav_w') - 2 * P(sh, 'ledge_w')
    sk = sketch_on(body, xy, 'Sk_ComponentSpace')
    rect(sk, P(sh, 'cav_x0'), -inner / 2, P(sh, 'cav_l'), inner, 'cmp',
         'Parameter.cav_x0', '-(Parameter.cav_w - 2 * Parameter.ledge_w) / 2',
         'Parameter.cav_l', 'Parameter.cav_w - 2 * Parameter.ledge_w')
    pocket(sk, 'ComponentSpace', 'Parameter.cav_h')

    # 3b Chamfer at the lower edge of the ledge, all round on the inside.
    #
    #    There the board upper edge and the ledge meet, both at z = pcb_z1, as a
    #    90-degree edge. A wire that strays there is sheared off when the board
    #    is pushed in. The chamfer turns this into a wedge: the wire is pushed
    #    inwards instead of being squashed.
    tip = body.Tip
    edge_names = []
    yg = P(sh, 'cav_w') / 2 - 0.01
    xa, xb = P(sh, 'cav_x0') - 0.01, P(sh, 'cav_x0') + P(sh, 'cav_l') + 0.01
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        vs = e.Vertexes
        if any(abs(v.Point.z - P(sh, 'pcb_z1')) > 1e-6 for v in vs):
            continue
        if any(abs(v.Point.y) > yg for v in vs):
            continue
        if any(not (xa <= v.Point.x <= xb) for v in vs):
            continue
        edge_names.append('Edge%d' % (i + 1))
    if len(edge_names) < 2:
        raise RuntimeError('Ledge edges not found: %s' % edge_names)
    f = body.newObject('PartDesign::Chamfer', 'LedgeChamfers')
    f.Base = (tip, edge_names)
    f.setExpression('Size', 'Parameter.ledge_chamfer')
    doc.recompute()

    # 3c Cable ramp: at the cable end the underside of the ledge rises.
    #
    #    Here the wire leaves the board and dips into the cable groove -- and
    #    the groove already starts 3 mm INSIDE the cavity, i.e. it runs under
    #    the ledge. Without the ramp the board jams exactly here when pushed
    #    in. With it the gap opens like a wedge.
    #
    #    Sketch in XZ: local x = global x, local y = global z, normal -Y.
    x1 = P(sh, 'cav_x0') + P(sh, 'cav_l')
    z0 = P(sh, 'pcb_z1')
    sk = body.newObject('Sketcher::SketchObject', 'Sk_CableRamp')
    try:
        sk.AttachmentSupport = [(planes['XZ_Plane'], '')]
    except Exception:
        sk.Support = [(planes['XZ_Plane'], '')]
    sk.MapMode = 'FlatFace'
    v = Vector
    g = [sk.addGeometry(Part.LineSegment(v(x1, z0, 0), v(x1, z0 + P(sh, 'ramp_h'), 0)), False),
         sk.addGeometry(Part.LineSegment(v(x1, z0 + P(sh, 'ramp_h'), 0),
                                         v(x1 - P(sh, 'ramp_l'), z0, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x1 - P(sh, 'ramp_l'), z0, 0), v(x1, z0, 0)), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[0]))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x1), 'rmp_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, z0), 'rmp_z')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[0], 2, P(sh, 'ramp_h')), 'rmp_h')
    named(sk, Sketcher.Constraint('DistanceX', g[2], 1, g[2], 2, P(sh, 'ramp_l')), 'rmp_l')
    sk.setExpression('Constraints.rmp_x', 'Parameter.cav_x0 + Parameter.cav_l')
    sk.setExpression('Constraints.rmp_z', 'Parameter.pcb_z1')
    sk.setExpression('Constraints.rmp_h', 'Parameter.ramp_h')
    sk.setExpression('Constraints.rmp_l', 'Parameter.ramp_l')
    fr = body.newObject('PartDesign::Pocket', 'CableRamp')
    fr.Profile = sk
    fr.Midplane = True
    fr.setExpression('Length', 'Parameter.cav_w')
    doc.recompute()

    # 4 Snap lugs
    sk = sketch_on(body, yz, 'Sk_SnapNoses',
                   P(sh, 'cav_xm') - P(sh, 'snap_l') / 2)
    sk.setExpression('AttachmentOffset.Base.z',
                     'Parameter.cav_xm - Parameter.snap_l / 2')
    for sign in (+1, -1):
        wedge(sk, sign, P(sh, 'cav_w') / 2, P(sh, 'snap_z'),
              P(sh, 'snap_w'), P(sh, 'snap_h'),
              'nose_p' if sign > 0 else 'nose_m')
    pad(sk, 'SnapNoses', 'Parameter.snap_l')

    # 5 Spring relief
    depth = P(sh, 'w_out') / 2 - P(sh, 'cav_w') / 2 - P(sh, 'relief_t')
    e_depth = 'Parameter.w_out / 2 - Parameter.cav_w / 2 - Parameter.relief_t'
    e_in = 'Parameter.cav_w / 2 + Parameter.relief_t'
    y_in = P(sh, 'cav_w') / 2 + P(sh, 'relief_t')
    x0 = P(sh, 'cav_xm') - P(sh, 'relief_l') / 2
    e_x0 = 'Parameter.cav_xm - Parameter.relief_l / 2'
    sk = sketch_on(body, xy, 'Sk_SpringRelief')
    rect(sk, x0, y_in, P(sh, 'relief_l'), depth, 'relp',
         e_x0, e_in, 'Parameter.relief_l', e_depth)
    rect(sk, x0, -P(sh, 'w_out') / 2, P(sh, 'relief_l'), depth, 'relm',
         e_x0, '-Parameter.w_out / 2', 'Parameter.relief_l', e_depth)
    pocket(sk, 'SpringRelief', 'Parameter.relief_h')

    # 6 Screw hole
    sk = sketch_on(body, xy, 'Sk_ScrewHole')
    circle(sk, P(sh, 'm6_x'), P(sh, 'm6_y'), P(sh, 'm6_d') / 2, 'm6',
           'Parameter.m6_x', 'Parameter.m6_y', 'Parameter.m6_d / 2')
    pocket(sk, 'ScrewHole', 'Parameter.h_out + 1')

    # 7+8 Cable groove: rectangle + round, clamping section
    r = P(sh, 'cab_d') / 2
    e_r = 'Parameter.cab_d / 2'
    sk = sketch_on(body, yz, 'Sk_CableGroove', P(sh, 'cab_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
    rect(sk, P(sh, 'cab_y') - r, 0.0, P(sh, 'cab_d'), P(sh, 'cab_depth') - r, 'grv',
         'Parameter.cab_y - Parameter.cab_d / 2', '0',
         'Parameter.cab_d', 'Parameter.cab_depth - Parameter.cab_d / 2')
    pocket(sk, 'CableGroove', 'Parameter.cab_len')

    sk = sketch_on(body, yz, 'Sk_CableGroove_Round', P(sh, 'cab_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
    circle(sk, P(sh, 'cab_y'), P(sh, 'cab_depth') - r, r, 'grvr',
           'Parameter.cab_y', 'Parameter.cab_depth - Parameter.cab_d / 2', e_r)
    pocket(sk, 'CableGroove_Round', 'Parameter.cab_len')

    # 9+10 Cable recess, deeper
    sk = sketch_on(body, yz, 'Sk_CableTrough', P(sh, 'cab_deep_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
    rect(sk, P(sh, 'cab_y') - r, 0.0, P(sh, 'cab_d'), P(sh, 'cab_deep') - r, 'tr',
         'Parameter.cab_y - Parameter.cab_d / 2', '0',
         'Parameter.cab_d', 'Parameter.cab_deep - Parameter.cab_d / 2')
    pocket(sk, 'CableTrough', 'Parameter.cab_deep_l')

    sk = sketch_on(body, yz, 'Sk_CableTrough_Round', P(sh, 'cab_deep_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
    circle(sk, P(sh, 'cab_y'), P(sh, 'cab_deep') - r, r, 'trr',
           'Parameter.cab_y', 'Parameter.cab_deep - Parameter.cab_d / 2', e_r)
    pocket(sk, 'CableTrough_Round', 'Parameter.cab_deep_l')

    # 11 LED hole
    sk = sketch_on(body, xy, 'Sk_LED')
    circle(sk, P(sh, 'led_x'), 0.0, P(sh, 'led_d') / 2, 'led',
           'Parameter.led_x', '0', 'Parameter.led_d / 2')
    pocket(sk, 'LED_Hole', 'Parameter.h_out + 1')

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
    """Body visible, sketches and intermediate steps hidden.

    Without the GUI FreeCAD does not do this by itself: when creating in the GUI,
    PartDesign hides the previous feature and its sketch along with it. In a
    script everything stays as it was created -- so it is done by hand.
    """
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hall-housing')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'hall-housing.FCStd')
    doc.saveAs(fcstd)
    # Origin children stay true; they are hidden through the Origin group,
    # exactly as FreeCAD writes it itself.
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'hall-housing.step'))
    body.Shape.exportStl(os.path.join(here, 'hall-housing.stl'))

    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
    print('Valid       %s' % body.Shape.isValid())
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
