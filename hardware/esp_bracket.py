# -*- coding: utf-8 -*-
"""
Mounting clip for the ESP enclosure (ESP, display if fitted, wiring).

Two pieces needed -- the enclosure has two end faces --, but only **one** part
designed: the clip is symmetric about the M6 axis and therefore fits both ends.

Three holes in a triangle:

      o-------------o     two M4 THREADS directly in the plate
       \           /      (spacing M4_PITCH, hole centres)
        \         /
         \       /        outline narrows towards the middle
          \     /
          +-----+
          | (O) |       one M6, centred, offset outwards, with a
          +-----+       flat counterbore for the screw head

The two M4 come from above through the Ø 4.5 holes at the edge of the end face
directly into threads that sit in the plate. The M6 goes through the protruding tip
onto the rail. Two screws, two jobs: the M4 hold the enclosure, the M6 holds the
clip.

VERSION OF 2026-11-23 -- slimmer, thinner, without nuts:

  * M4 THREADS instead of nuts. As with the e-stop clip (hardware/estop-bracket.FCStd):
    PartDesign hole, ISO M4x0.7, thread modelled, through.
    The screw bites into the print; 4 mm of plate are just under six turns.
  * PLATE 4.0 mm like the e-stop clip. The 6.0 were only for the nuts.
  * M6 COUNTERSUNK. The screws are short, ISO 7380 button head (head Ø 10.5,
    3.3 high). Flat counterbore Ø m6_head_d = 11, deep enough that m6_rest =
    1.5 mm of floor remain; clearance 6.5. The head stands 0.8 mm above the
    plate, next to the enclosure, where it bothers nobody.
  * SLIMMER TIP. Offset and tip now follow the HEAD (Ø 11) instead of the
    body washer Ø 18, which is dropped with the counterbore. The M6 axis thus
    moves from 16.2 to 12.7 mm behind the M4 line, the tip from 21 to 15 mm
    width.

Structure as in hall_housing.py and hx711_housing.py: PartDesign body with a
feature tree plus spreadsheet "Parameter", all sketches fully constrained and bound
to the table through expressions. Dimensions in mm.

Feature tree:
    Plate        hexagonal outline: wide under the enclosure, tip towards the M6
    M4_Thread    two threaded holes M4x0.7 through the plate
    M6_Clearance  Ø 6.5 in the tip
    M6_Counterbore    flat counterbore Ø 11 from above, until 1.5 mm of floor remain
    Fillet    the six vertical outline edges

The top side is **smooth** -- no raised pads. It lies flat over its full area
against the end face of the enclosure.

Coordinates: x = 0 is the axis of symmetry, y = 0 the line of the two M4.
+y points away from the enclosure centre, outwards. z = 0 is the underside that
rests on the rail.
"""

import os
import sys
import math

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# ------------------------------------------------------------------- TBD
#
# Spacing of the two Ø 4.5 holes on the enclosure, centre to centre, in mm.
#
# If this is None, the script builds nothing but says what has to be measured --
# an invented number that looks like a measurement is worse than a gap
# (project rule, see CLAUDE.md).
#
# Measured on the enclosure (2026-11-18): 50.0 mm.
M4_PITCH = 50.0

SCREW_EDGE = 6.2          # hole centre to outer edge of the enclosure, given


def params(pitch):
    """(name, value or formula, comment). Formulas start with '='."""
    return [
        ('m4_pitch',   pitch,        'Hole spacing on the enclosure, centre to centre'),
        ('screw_edge', SCREW_EDGE,   'Hole centre to enclosure edge, given'),

        ('m4_thread',  'M4x0.7', 'Thread directly in the plate, as on the e-stop clip'),
        ('m6_d',          6.5,  'M6 clearance to the rail'),
        # Flat counterbore for the button head ISO 7380 M6: dk 10.5, k 3.3.
        ('m6_head_d',    11.0,  'Flat counterbore, button head dk 10.5 + play'),
        ('m6_rest',       1.5,  'Floor under the screw head'),
        ('m6_cb_t', '=plate_t - m6_rest', 'Depth of the flat counterbore'),

        ('m4_pad',       11.3,  'Material around the threads: plate as wide as the enclosure'),
        ('plate_t',       4.0,  'Plate thickness, as the e-stop clip'),
        ('fillet_r',      3.0,  'Fillet of the vertical outline edges'),

        # Offset and tip follow the countersunk HEAD: the counterbore must not
        # cut into the enclosure edge, and beside it a wall has to remain at
        # the tip.
        ('m6_clear',      1.0,  'Clearance from counterbore rim to enclosure edge'),
        ('tip_wall',      2.0,  'Wall beside the counterbore at the tip'),
        ('m6_offset', '=screw_edge + m6_head_d / 2 + m6_clear',
                       'M4 line to M6 axis'),

        ('y_in',  '=-screw_edge', 'Inner edge, under the enclosure'),
        ('y_hip', '=screw_edge',  'The taper starts here: at the enclosure edge'),
        ('y_out', '=m6_offset + m6_head_d / 2 + tip_wall',
                  'Outer edge: wall beside the counterbore'),

        ('w_wide', '=m4_pitch + m4_pad', 'Width under the enclosure'),
        ('w_tip',  '=m6_head_d + 2 * tip_wall',
                   'Width at the tip: wall beside the counterbore'),

        ('len', '=y_out - y_in', 'Overall length'),
    ]


def build_sheet(doc, pitch):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Value')
    sh.set('C1', 'Meaning')
    rows = params(pitch)
    for i, (name, val, note) in enumerate(rows):
        r = i + 2
        sh.set('A%d' % r, name)
        sh.setAlias('B%d' % r, name)
        sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(rows):
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


def polygon(sk, pts, tag, exprs):
    """Closed polygon, fully constrained by one DistanceX/Y each to the origin.

    n lines have 4n degrees of freedom, n coincidences take 2n, the 2n dimensions
    the rest -- exactly constrained, without Horizontal/Vertical, which would
    over-constrain here.
    """
    n = len(pts)
    g = [sk.addGeometry(Part.LineSegment(Vector(pts[i][0], pts[i][1], 0),
                                         Vector(pts[(i + 1) % n][0],
                                                pts[(i + 1) % n][1], 0)), False)
         for i in range(n)]
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i in range(n):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, pts[i][0]),
              '%s%d_x' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, pts[i][1]),
              '%s%d_y' % (tag, i))
        sk.setExpression('Constraints.%s%d_x' % (tag, i), exprs[i][0])
        sk.setExpression('Constraints.%s%d_y' % (tag, i), exprs[i][1])
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



# -------------------------------------------------------------------- Model

def sketch_on(body, plane, name, z_off=0.0):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if z_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, z_off), App.Rotation())
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Bracket')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

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

    ww, wt = P(sh, 'w_wide') / 2, P(sh, 'w_tip') / 2
    yi, yh, yo = P(sh, 'y_in'), P(sh, 'y_hip'), P(sh, 'y_out')
    hx = P(sh, 'm4_pitch') / 2

    # 1 Plate: full width up to the enclosure edge, then narrowing to the tip.
    #   The taper only starts there because the bosses underneath need the full
    #   width.
    sk = sketch_on(body, xy, 'Sk_Plate')
    polygon(sk,
            [(-ww, yi), (ww, yi), (ww, yh), (wt, yo), (-wt, yo), (-ww, yh)], 'pl',
            [('-Parameter.w_wide / 2', 'Parameter.y_in'),
             ('Parameter.w_wide / 2',  'Parameter.y_in'),
             ('Parameter.w_wide / 2',  'Parameter.y_hip'),
             ('Parameter.w_tip / 2',   'Parameter.y_out'),
             ('-Parameter.w_tip / 2',  'Parameter.y_out'),
             ('-Parameter.w_wide / 2', 'Parameter.y_hip')])
    pad(sk, 'Plate', 'Parameter.plate_t')

    # 2 M4 threads through the plate: PartDesign hole with a modelled ISO
    #   thread, exactly as in hardware/estop-bracket.FCStd. The sketch only
    #   supplies the centres, the thread determines the diameter.
    sk = sketch_on(body, xy, 'Sk_M4', P(sh, 'plate_t'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.plate_t')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        circle(sk, sign * hx, 0.0, 2.0, 'm4' + t,
               '%d * Parameter.m4_pitch / 2' % sign, '0', '2')
    h = body.newObject('PartDesign::Hole', 'M4_Thread')
    h.Profile = sk
    h.DepthType = 'ThroughAll'
    h.Threaded = True
    h.ThreadType = 'ISOMetricProfile'
    thread = str(sh.get('m4_thread'))
    sizes = h.getEnumerationsOfProperty('ThreadSize')
    if thread not in sizes:
        alt = [z for z in sizes if z.startswith('M4')]
        if not alt:
            raise RuntimeError('Thread %s not in %s' % (thread, sizes))
        thread = alt[0]
    h.ThreadSize = thread
    h.ModelThread = True
    doc.recompute()

    # 4 M6 in the tip
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, 0.0, P(sh, 'm6_offset'), P(sh, 'm6_d') / 2, 'm6',
           '0', 'Parameter.m6_offset', 'Parameter.m6_d / 2')
    pocket(sk, 'M6_Clearance', 'Parameter.plate_t + 1')

    # 4a Flat counterbore from above, until m6_rest of floor remains. Sketch on
    #    the top side; the pocket goes against the normal, i.e. downwards.
    sk = sketch_on(body, xy, 'Sk_M6_Counterbore', P(sh, 'plate_t'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.plate_t')
    circle(sk, 0.0, P(sh, 'm6_offset'), P(sh, 'm6_head_d') / 2, 'cb',
           '0', 'Parameter.m6_offset', 'Parameter.m6_head_d / 2')
    pocket(sk, 'M6_Counterbore', 'Parameter.m6_cb_t', reversed_=False)

    # 5 Fillet of the vertical outline edges.
    #
    #   The edges are found by their POSITION, not by their index: an index
    #   shifts with every change to the tree, a coordinate does not. The
    #   vertical edges of the nut pockets lie elsewhere and so stay sharp -- a
    #   filleted hex pocket no longer holds a nut.
    corners = [(-ww, yi), (ww, yi), (ww, yh), (wt, yo), (-wt, yo), (-ww, yh)]
    tip = body.Tip
    edge_names = []
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Vertexes[-1].Point - e.Vertexes[0].Point
        if abs(d.x) > 1e-6 or abs(d.y) > 1e-6:
            continue                                   # not vertical
        q = e.Vertexes[0].Point
        if any(abs(q.x - ex) < 1e-6 and abs(q.y - ey) < 1e-6 for ex, ey in corners):
            edge_names.append('Edge%d' % (i + 1))
    if len(edge_names) != 6:
        raise RuntimeError('Outline edges not found unambiguously: %s' % edge_names)
    f = body.newObject('PartDesign::Fillet', 'Fillet')
    f.Base = (tip, edge_names)
    f.setExpression('Radius', 'Parameter.fillet_r')
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
    return len(order)


def set_visibility(doc, body):
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def main():
    if M4_PITCH is None:
        print('M4_PITCH is TBD -- nothing is built.')
        print()
        print('Needed is the spacing of the two Ø 4.5 holes on the end face')
        print('of the ESP enclosure, centre to centre, in mm. To determine it:')
        print('  * calipers across the two hole centres, or')
        print('  * width of the end face minus 2 x %.1f mm (edge distance).'
              % SCREW_EDGE)
        print()
        print('Then set M4_PITCH above and run again. Everything else')
        print('depends on it and follows by itself:')
        print('  width under the enclosure = M4_PITCH + 11.3')
        print('  length, bosses, nut pockets, M6 position: fixed')
        return 1

    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('esp-bracket')
    sh = build_sheet(doc, M4_PITCH)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'esp-bracket.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'esp-bracket.step'))

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
    print('M4 thread                   %s, modelled, %.1f mm long'
          % (doc.getObject('M4_Thread').ThreadSize, P(sh, 'plate_t')))
    print('Fillet                      r = %.1f mm on 6 edges'
          % P(sh, 'fillet_r'))
    print('M6 axis behind the enclosure edge %.2f mm'
          % (P(sh, 'm6_offset') - P(sh, 'screw_edge')))
    print('Flat counterbore \u00d8%.1f, %.2f mm deep, floor %.2f mm, wall beside the '
          'head %.2f mm'
          % (P(sh, 'm6_head_d'), P(sh, 'm6_cb_t'), P(sh, 'm6_rest'),
             (P(sh, 'w_tip') - P(sh, 'm6_head_d')) / 2))
    # Check geometry: air in the counterbore, floor beneath, clearance hole open.
    s = body.Shape
    yc, r = P(sh, 'm6_offset'), P(sh, 'm6_head_d') / 2 - 0.5
    ok = (not s.isInside(Vector(r, yc, P(sh, 'plate_t') - 0.5), 1e-6, True)
          and s.isInside(Vector(r, yc, P(sh, 'm6_rest') / 2), 1e-6, True)
          and not s.isInside(Vector(0, yc, P(sh, 'm6_rest') / 2), 1e-6, True))
    print('Counterbore plausible       %s' % ok)
    return 0


main()
