# -*- coding: utf-8 -*-
"""
Wall mount for an iPad mini 3, cross-shaped, for printing.

Structure as in hall_housing.py and xt60_holder.py: one PartDesign body with a
feature tree plus a spreadsheet "Parameter"; all sketches are bound to the table
via expressions. Dimensions are changed in the GUI in the table, not in the
sketches. Dimensions in mm.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/ipad_holder.py

Produces hardware/ipad-holder.FCStd, .step and .stl.

ORIENTATION: portrait, home button at the bottom. The iPad lies with its back on
the plate. z = 0 is the back of the mount — flat, that is the print bed face.
x runs across the width of the iPad, y along its length, +y is up.

    ipad_w x ipad_l x ipad_t = 134.7 x 200.0 x 7.5   Apple, technical specs
                                                      iPad mini 3 — measure on
                                                      the device before printing

The cross: one vertical and one horizontal arm, each arm_w wide, plate
plate_t thick, all twelve corners rounded with corner_r in top view. The arms
reach to the edges of the iPad; the four holding points sit at their ends. The
corners of the iPad stay free — that is where you grip it.

The claw runs over the FULL arm width, and the plate rounding there continues
over the full height — with 40 mm arms nothing would otherwise be left for the
claw next to the gap. Therefore corner_r must be smaller than claw_t, or the
rounding cuts through the inner wall. Guides and snap clip are thinner or
tapered at the top and end before the rounding (wall_len).

    left/right     GUIDE         wall guide_t thick, flush with the front face,
                                 no overhang. It holds nothing, it aligns.
    bottom         CLAW          wall claw_t plus a lip claw_lip deep over the
                                 front face. In the middle a gap claw_gap: the
                                 home button stays free, and the Lightning
                                 cable comes through the gap in the wall. Two
                                 L pieces remain on the outside, each claw_len
                                 wide — they carry the weight and are therefore
                                 the strongest parts.
    top            SNAP CLIP     wall clip_t with a small nose clip_nose. The
                                 lead-in ramp is shallow (clip_ramp = 2 x
                                 clip_nose, about 27° to the vertical), the
                                 underside of the nose slightly inclined
                                 (clip_under) — it snaps in softly and can be
                                 released again without force. Above the ramp
                                 the wall continues as a TAB tab_h high, inclined
                                 backwards by tab_flare, tapered from clip_t to
                                 tab_tip, the outer bend rounded with tab_r.
                                 On the inside the ramp runs into the tab
                                 WITHOUT A STEP; tab_flare/tab_h is slightly
                                 steeper than the ramp (2:1) for that, otherwise
                                 a re-entrant bend results, and the kernel does
                                 not round it at the end faces. You pull on it to
                                 release. Inserting: bottom into the claw, press
                                 at the top; the ramp lifts the arm end beyond
                                 the upper screw (about 50 mm lever), the nose
                                 snaps over the edge of the glass.

Claw (6 mm wall, 4 mm lip) and snap clip (4.5 mm wall) are deliberately
stronger than the guides: printed with the back on the bed, the walls are built
from layers, and lip and nose hang on a layer line — that is where it breaks
when it is thin. The two claw pieces are separate and carry the whole weight,
hence the thickest. The side guides hold nothing and stay at 3 mm.

Four M6 button-head screws in flat counterbores from the iPad side: two on the
vertical centre axis, m6_pitch apart, two on the horizontal one, m6_pitch_h
apart — depending on what the wall allows, use one pair or the other or all
four. Head height m6_head_h = 4 mm ASSUMED (measure on the screw), below it
m6_floor = 1.5 mm floor remains — this gives the plate thickness plate_t = 5.5.
The counterbore m6_head_d = 13 takes DIN 7985 (dk 12) as well as ISO 7380
(dk 10.6). The head lies completely within the plate, the iPad lies on the
plate, not on the heads. Because the plate got thicker, the arms are narrower:
arm_w 40.

ROUNDINGS: plate corners corner_r in top view. Claw all round — vertical end
edges with claw_r, all free edges above (top edge, front edge of the lip, ends)
with edge_r. Snap clip: bend of the tab with tab_r, all other free edges with
edge_r; nose and ramp stay sharp, that is the latching edge. The guides only get
wall_r on the end edges. Tangential seam edges of existing roundings are
detected and skipped.

PRINTING: back on the bed. The lip of the claw (claw_lip = 4 mm) and the nose
(1.5 mm) are overhangs without support from below; the nose prints freely, the
lip needs supports or is left as a short bridge. PETG recommended — the plate
flexes when snapping in, PLA fatigues from that.

Feature tree:
    Arm_Vertical    rectangle arm_w x (length across both walls), z = 0...plate_t
    Arm_Horizontal   rectangle (width across both walls) x arm_w
    Guide         two walls at x = +-(ipad_w/2 + clr/2), length wall_len
    Claw_p/_m      L profile in YZ, one piece each beside the gap up to the edge
    Snap        profile with nose and tab in YZ, length wall_len, centred
    Plate_Fillet   all twelve outer corners in top view, radius corner_r,
                     at the claw over the full height
    Guide_Fillet  vertical end edges of the guides, radius wall_r
    Claw_Fillet   vertical end edges of the claw, claw_r
    Edge_Fillet    free edges of the claw, edge_r
    Tab_Fillet    bend of the tab inside and outside, tab_r
    Snap_Fillet free edges of the snap clip, edge_r
    M6               four flat counterbores, PartDesign Hole with counterbore
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
    # Device, Apple technical specs iPad mini 3 -> MEASURE
    ('ipad_w',      134.7, 'iPad width, Apple'),
    ('ipad_l',      200.0, 'iPad length, Apple'),
    ('ipad_t',        7.5, 'iPad thickness, Apple'),
    ('clr',           0.6, 'clearance in width and length, TOTAL -> 0.3 per side'),
    ('clr_z',         0.3, 'gap between front face and lip/nose'),

    # Cross
    ('plate_t', '=m6_head_h + m6_floor', 'plate thickness: head plus floor; back = print bed face'),
    ('arm_w',        40.0, 'arm width, both the same'),
    ('corner_r',      4.0, 'rounding of all plate corners; MUST be < claw_t'),
    ('wall_len', '=arm_w - 2 * corner_r', 'guide length = straight part of the arm ends'),
    ('wall_r',        1.0, 'rounding of the vertical end edges of the guides'),
    ('claw_r',        2.0, 'rounding of the vertical end edges of claw and snap clip'),
    ('edge_r',        1.2, 'rounding of the free edges of claw and snap clip'),

    # Side guide
    ('guide_t',       3.0, 'guide wall thickness'),
    ('guide_h', '=plate_t + ipad_t', 'guide wall height, flush with the front face'),

    # Claw at the bottom
    ('claw_t',        6.0, 'claw wall thickness; two separate pieces carry everything'),
    ('claw_lip',      4.0, 'lip over the front face, depth'),
    ('claw_lip_t',    4.0, 'lip thickness; hangs on a layer line'),
    ('claw_gap',     20.0, 'gap in the middle: home button free, Lightning through'),
    ('claw_len', '=(arm_w - claw_gap) / 2', 'length per claw piece, up to the arm edge'),

    # Snap clip at the top
    ('clip_t',        4.5, 'snap clip wall thickness; built from layers, hence strong'),
    ('clip_nose',     1.2, 'nose over the front face, depth'),
    ('clip_ramp', '=2 * clip_nose', 'lead-in ramp height; 2:1 -> about 27 degrees to the vertical'),
    ('clip_under',    0.6, 'incline of the nose underside, height; 0 = horizontal, hard'),
    ('clip_tip',      1.0, 'nose vertical at the tip, between ramp and underside'),
    ('tab_h',        10.0, 'tab height, from the top edge of the ramp'),
    ('tab_flare',     6.0, 'tab inclined backwards, offset at the top; > tab_h/2, see header'),
    ('tab_tip',       3.0, 'tab thickness at the top; at the bottom clip_t'),
    ('tab_r',         6.0, 'rounding of the tab bend'),

    # Screws M6 button head, flat counterbore from the iPad side
    ('m6_d',          6.6, 'M6 clearance hole'),
    ('m6_head_d',    13.0, 'flat counterbore; DIN 7985 dk 12, ISO 7380 dk 10.6 + clearance'),
    ('m6_head_h',     4.0, 'head height ASSUMED -> measure on the screw'),
    ('m6_floor',      1.5, 'floor under the head'),
    ('m6_pitch',    100.0, 'hole spacing vertical, on the centre axis'),
    ('m6_pitch_h',   80.0, 'hole spacing horizontal, on the centre axis'),

    # derived
    ('x_side',  '=ipad_w / 2 + clr / 2',   'inner face of the guide, absolute value'),
    ('y_top',   '=ipad_l / 2 + clr / 2',   'inner face of the snap clip'),
    ('y_bot',   '=-(ipad_l / 2 + clr / 2)', 'inner face of the claw'),
    ('z_front', '=plate_t + ipad_t + clr_z', 'underside of lip and nose'),
    ('claw_top', '=z_front + claw_lip_t',  'claw height'),
    ('z_ramp',  '=z_front + clip_under + clip_tip + clip_ramp', 'top edge of the ramp = bend of the tab'),
    ('clip_top', '=z_ramp + tab_h',       'snap clip height'),
    ('claw_x',  '=claw_gap / 2 + claw_len / 2', 'centre of a claw piece, absolute value'),
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


def polygon(sk, pts, tag):
    """Closed polyline. pts = [(x, y, expr_x, expr_y), ...]; every corner is
    fixed by two distances to the origin and bound to expressions — so the
    polyline is fully constrained without further constraints."""
    v = Vector
    n = len(pts)
    g = []
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        g.append(sk.addGeometry(Part.LineSegment(v(a[0], a[1], 0), v(b[0], b[1], 0)), False))
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i, (x, y, ex, ey) in enumerate(pts):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, x), '%s_x%d' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, y), '%s_y%d' % (tag, i))
        sk.setExpression('Constraints.%s_x%d' % (tag, i), ex)
        sk.setExpression('Constraints.%s_y%d' % (tag, i), ey)
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


def vertical_edges(shape, zmin, zmax):
    """Names of the edges parallel to z that run exactly from zmin to zmax."""
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.x - b.x) > 1e-6 or abs(a.y - b.y) > 1e-6:
            continue
        lo, hi = min(a.z, b.z), max(a.z, b.z)
        if abs(lo - zmin) < 1e-6 and abs(hi - zmax) < 1e-6:
            namen.append('Edge%d' % (i + 1))
    return namen


def wall_end_edges(shape, plate_t, region):
    """Vertical edges that start on the plate and reach beyond it — the end
    edges of the walls. region(bb) selects the area."""
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.x - b.x) > 1e-6 or abs(a.y - b.y) > 1e-6:
            continue
        lo, hi = min(a.z, b.z), max(a.z, b.z)
        if lo < plate_t - 1e-6 or hi < plate_t + 1e-3:
            continue
        if region(e.BoundBox):
            namen.append('Edge%d' % (i + 1))
    return namen


def tangent_seam(shape, edge):
    """True if the two faces at the edge merge tangentially — the seam of an
    existing rounding. That cannot be rounded a second time."""
    faces = shape.ancestorsOfType(edge, Part.Face)
    if len(faces) != 2:
        return False
    t = edge.FirstParameter + 0.5 * (edge.LastParameter - edge.FirstParameter)
    p = edge.valueAt(t)
    normals = []
    for f in faces:
        u, v = f.Surface.parameter(p)
        normals.append(f.normalAt(u, v))
    return abs(abs(normals[0].dot(normals[1])) - 1.0) < 1e-3


def free_edges(shape, plate_t, region, exclude):
    """All edges above the plate in an area, without the vertical ones
    (already rounded) and without those named by exclude(bb) — those are the
    re-entrant ones the iPad rests against."""
    namen = []
    for i, e in enumerate(shape.Edges):
        bb = e.BoundBox
        if bb.ZMin < plate_t + 1e-3:
            continue                               # foot of the wall, re-entrant
        if bb.XLength < 1e-6 and bb.YLength < 1e-6:
            continue                               # vertical
        if not region(bb) or exclude(bb):
            continue
        if tangent_seam(shape, e):
            continue                               # seam of a rounding
        namen.append('Edge%d' % (i + 1))
    return namen


def fillet(doc, body, name, edges, expr_r):
    tip = body.Tip
    f = body.newObject('PartDesign::Fillet', name)
    f.Base = (tip, edges)
    f.setExpression('Radius', expr_r)
    doc.recompute()
    if 'Invalid' in (f.State or []) or not body.Shape.isValid():
        print('%s failed (%d edges), left out' % (name, len(edges)))
        body.removeObject(f)
        doc.removeObject(f.Name)
        doc.recompute()
        return None
    return f


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'IpadHolder')
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

    pt, arm = P(sh, 'plate_t'), P(sh, 'arm_w')
    xs, yt, yb = P(sh, 'x_side'), P(sh, 'y_top'), P(sh, 'y_bot')
    gt, ct, kt = P(sh, 'guide_t'), P(sh, 'clip_t'), P(sh, 'claw_t')
    zf = P(sh, 'z_front')

    # 1 Vertical arm: from the outside of the claw to the outside of the
    #   snap clip.
    sk = sketch_on(body, xy, 'Sk_Arm_Vertical')
    rect(sk, -arm / 2, yb - kt, arm, (yt + ct) - (yb - kt), 'av',
         '-Parameter.arm_w / 2', 'Parameter.y_bot - Parameter.claw_t',
         'Parameter.arm_w',
         'Parameter.y_top + Parameter.clip_t - (Parameter.y_bot - Parameter.claw_t)')
    pad(sk, 'Arm_Vertical', 'Parameter.plate_t')

    # 2 Horizontal arm: across both guide walls.
    sk = sketch_on(body, xy, 'Sk_Arm_Horizontal')
    rect(sk, -(xs + gt), -arm / 2, 2 * (xs + gt), arm, 'ah',
         '-(Parameter.x_side + Parameter.guide_t)', '-Parameter.arm_w / 2',
         '2 * (Parameter.x_side + Parameter.guide_t)', 'Parameter.arm_w')
    pad(sk, 'Arm_Horizontal', 'Parameter.plate_t')

    # 3 Guide left and right: two walls, both in one sketch.
    sk = sketch_on(body, xy, 'Sk_Guide')
    wl = P(sh, 'wall_len')
    rect(sk, xs, -wl / 2, gt, wl, 'gp',
         'Parameter.x_side', '-Parameter.wall_len / 2', 'Parameter.guide_t', 'Parameter.wall_len')
    rect(sk, -(xs + gt), -wl / 2, gt, wl, 'gm',
         '-(Parameter.x_side + Parameter.guide_t)', '-Parameter.wall_len / 2',
         'Parameter.guide_t', 'Parameter.wall_len')
    pad(sk, 'Guide', 'Parameter.guide_h')

    # 5 Claw: L profile in YZ (local x = global y, local y = global z), one
    #   piece each left and right of the gap. The YZ normal points to +x, the
    #   sketch offset moves the profile to x = +-claw_x; checked in check().
    kl, kt_lip = P(sh, 'claw_lip'), P(sh, 'claw_lip_t')
    ktop = P(sh, 'claw_top')
    for tag, sign in (('p', +1), ('m', -1)):
        sk = sketch_on(body, yz, 'Sk_Claw_' + tag,
                       expr_off='%d * Parameter.claw_x' % sign)
        polygon(sk, [
            (yb - kt, 0.0,  'Parameter.y_bot - Parameter.claw_t', '0'),
            (yb,      0.0,  'Parameter.y_bot',                    '0'),
            (yb,      zf,   'Parameter.y_bot',                    'Parameter.z_front'),
            (yb + kl, zf,   'Parameter.y_bot + Parameter.claw_lip', 'Parameter.z_front'),
            (yb + kl, ktop, 'Parameter.y_bot + Parameter.claw_lip', 'Parameter.claw_top'),
            (yb - kt, ktop, 'Parameter.y_bot - Parameter.claw_t',   'Parameter.claw_top'),
        ], 'kr' + tag)
        pad(sk, 'Claw_' + tag, 'Parameter.claw_len', midplane=True)

    # 6 Snap clip: wall with nose and tab, centred over wall_len. Profile in
    #   YZ (local x = global y, local y = global z), clockwise from the inner
    #   foot: foot, outer wall up to the bend, tab inclined backwards and
    #   tapered, top edge, inside of the tab down to the top edge of the ramp,
    #   lead-in ramp, nose tip, inclined underside.
    nose, ctop = P(sh, 'clip_nose'), P(sh, 'clip_top')
    zr = P(sh, 'z_ramp')
    fl, tip, under = P(sh, 'tab_flare'), P(sh, 'tab_tip'), P(sh, 'clip_under')
    sk = sketch_on(body, yz, 'Sk_Snap')
    polygon(sk, [
        (yt,            0.0,  'Parameter.y_top',                       '0'),
        (yt + ct,       0.0,  'Parameter.y_top + Parameter.clip_t',    '0'),
        (yt + ct,       zr,   'Parameter.y_top + Parameter.clip_t',    'Parameter.z_ramp'),
        (yt + fl + tip, ctop, 'Parameter.y_top + Parameter.tab_flare + Parameter.tab_tip',
                              'Parameter.clip_top'),
        (yt + fl,       ctop, 'Parameter.y_top + Parameter.tab_flare', 'Parameter.clip_top'),
        (yt,            zr,   'Parameter.y_top',                       'Parameter.z_ramp'),
        (yt - nose, zr - P(sh, 'clip_ramp'),
                              'Parameter.y_top - Parameter.clip_nose',
                              'Parameter.z_ramp - Parameter.clip_ramp'),
        (yt - nose, zf + under, 'Parameter.y_top - Parameter.clip_nose',
                              'Parameter.z_front + Parameter.clip_under'),
        (yt,            zf,   'Parameter.y_top',                       'Parameter.z_front'),
    ], 'sn')
    pad(sk, 'Snap', 'Parameter.wall_len', midplane=True)

    # 6a Rounding of all plate corners: every vertical edge that starts at the
    #    back — at the ends of the horizontal arm and at the four re-entrant
    #    corners only up to plate_t, at the claw up to its top edge, because
    #    the wall is flush with the plate edge there.
    edges = []
    for i, e in enumerate(body.Shape.Edges):
        bb = e.BoundBox
        if len(e.Vertexes) == 2 and bb.XLength < 1e-6 and bb.YLength < 1e-6 \
                and abs(bb.ZMin) < 1e-6:
            edges.append('Edge%d' % (i + 1))
    if len(edges) == 12:
        fillet(doc, body, 'Plate_Fillet', edges, 'Parameter.corner_r')
    else:
        print('plate edges ambiguous (%d), rounding left out' % len(edges))

    # 7 Roundings. First the vertical end edges of the walls (top view), then
    #   all free edges of claw and snap clip. Re-entrant edges stay sharp: the
    #   foot of the walls and the underside of the nose, the iPad rests there.
    guides = lambda bb: abs(bb.XMin) > xs - 1e-3 and abs(bb.YMax) < arm / 2 + 1e-3
    claws = lambda bb: bb.YMax < yb + P(sh, 'claw_lip') + 1e-3
    clip = lambda bb: bb.YMin > yt - nose - 1e-3

    edges = wall_end_edges(body.Shape, pt, guides)
    print('Guide      %d end edges' % len(edges))
    if edges:
        fillet(doc, body, 'Guide_Fillet', edges, 'Parameter.wall_r')

    # Vertical end edges of the claw, in stages: the edges of the inner wall end
    #   at the top in the re-entrant edge under the lip, which OCC does not
    #   always round. First all, otherwise only those running through to the top
    #   edge (lip front edges; the outer corners already have the plate rounding).
    def to_top(n):
        return abs(body.Shape.getElement(n).BoundBox.ZMax - ktop) < 1e-3

    all_edges = wall_end_edges(body.Shape, pt, claws)
    for stage, edges in (('all', all_edges),
                         ('only continuous', [n for n in all_edges if to_top(n)])):
        print('Claw       %d end edges (%s)' % (len(edges), stage))
        if edges and fillet(doc, body, 'Claw_Fillet', edges, 'Parameter.claw_r'):
            break

    # Free edges of the claw; the re-entrant edge under the lip
    #   (y = y_bot, z = z_front) stays sharp, the iPad rests there.
    def below_lip(bb):
        return bb.YLength < 1e-6 and abs(bb.YMin - yb) < 1e-3 and abs(bb.ZMin - zf) < 1e-3
    all_edges = free_edges(body.Shape, pt, claws, below_lip)

    def before_inner_wall(n):
        return body.Shape.getElement(n).BoundBox.YMin > yb + 1e-3

    def top_edge(n):
        return body.Shape.getElement(n).BoundBox.ZMin > ktop - 1e-3

    for stage, edges in (('all', all_edges),
                         ('without inner wall', [n for n in all_edges if before_inner_wall(n)]),
                         ('only top edges', [n for n in all_edges if top_edge(n)])):
        print('Claw       %d free edges (%s)' % (len(edges), stage))
        if edges and fillet(doc, body, 'Edge_Fillet', edges, 'Parameter.edge_r'):
            break

    # Snap clip. First the bend of the tab: the horizontal edges along x at
    #   z = z_ramp, outside (wall -> tab) and inside (ramp -> tab, almost
    #   straight). If the inner one is missing because both faces coincide,
    #   only the outside is rounded.
    edges = []
    for i, e in enumerate(body.Shape.Edges):
        bb = e.BoundBox
        if clip(bb) and bb.YLength < 1e-6 and bb.ZLength < 1e-6 \
                and abs(bb.ZMin - zr) < 1e-3 and bb.XLength > 1.0:
            edges.append('Edge%d' % (i + 1))
    print('Tab        %d bend edges' % len(edges))
    if edges:
        fillet(doc, body, 'Tab_Fillet', edges, 'Parameter.tab_r')

    # Then all free edges of the snap clip above the ramp, including the
    #   vertical outer corners of the wall (they continue tangentially into the
    #   bend rounding and the tab). Nose and ramp stay sharp — and with them the
    #   kernel also rounds nothing that adjoins them.
    def below_ramp(bb):
        return bb.ZMax < zr + 1e-3
    free = free_edges(body.Shape, pt, clip, below_ramp)
    vertical = [n for n in wall_end_edges(body.Shape, pt, clip)
                 if body.Shape.getElement(n).BoundBox.YMin > yt + 1e-3]
    def top_edge_s(n):
        return body.Shape.getElement(n).BoundBox.ZMin > ctop - 1e-3
    for stage, edges in (('all', free + vertical),
                         ('only top edges', [n for n in free if top_edge_s(n)])):
        print('Snap clip  %d free edges (%s)' % (len(edges), stage))
        if edges and fillet(doc, body, 'Snap_Fillet', edges, 'Parameter.edge_r'):
            break

    # 8 M6 flat counterbores from the iPad side. Sketch at z = plate_t, the
    #   hole goes through downwards against the normal; the counterbore starts
    #   at the sketch plane and is m6_head_h deep.
    sk = sketch_on(body, xy, 'Sk_M6', expr_off='Parameter.plate_t')
    for tag, sign in (('p', +1), ('m', -1)):
        circle(sk, 0.0, sign * P(sh, 'm6_pitch') / 2, P(sh, 'm6_d') / 2, 'm6v' + tag,
               '0', '%d * Parameter.m6_pitch / 2' % sign, 'Parameter.m6_d / 2')
        circle(sk, sign * P(sh, 'm6_pitch_h') / 2, 0.0, P(sh, 'm6_d') / 2, 'm6h' + tag,
               '%d * Parameter.m6_pitch_h / 2' % sign, '0', 'Parameter.m6_d / 2')
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

    def luft(p, was):
        nonlocal ok
        if s.isInside(p, 1e-6, True):
            print('ERROR material instead of air: %s at (%.1f, %.1f, %.1f)' % (was, p.x, p.y, p.z))
            ok = False

    pt, zf = P(sh, 'plate_t'), P(sh, 'z_front')
    xs, yt, yb = P(sh, 'x_side'), P(sh, 'y_top'), P(sh, 'y_bot')
    gt, ct, kt = P(sh, 'guide_t'), P(sh, 'clip_t'), P(sh, 'claw_t')
    cx, nose = P(sh, 'claw_x'), P(sh, 'clip_nose')
    zmid = pt + P(sh, 'ipad_t') / 2

    # Back flat
    if abs(bb.ZMin) > 1e-6:
        print('ERROR back not at z = 0: %.3f' % bb.ZMin); ok = False
    # Plate
    material(Vector(0, 0, pt / 2), 'plate centre')
    material(Vector(0, yt, pt / 2), 'plate under the snap clip')
    material(Vector(0, yb, pt / 2), 'plate under the claw')
    material(Vector(xs, 0, pt / 2), 'plate under the guide')
    # Corner rounded: nothing may be at the outer arm corner
    material(Vector(0, yt + ct - 0.2, pt / 2), 'arm end centre')
    luft(Vector(P(sh, 'arm_w') / 2 - 0.3, yt + ct - 0.3, pt / 2), 'arm corner (rounding)')
    # iPad space free
    for p in (Vector(0, 0, zmid), Vector(xs - 0.1, 0, zmid), Vector(-xs + 0.1, 0, zmid),
              Vector(0, yt - 0.1, zmid), Vector(0, yb + 0.1, zmid),
              Vector(0, yt - 0.1, zf - 0.1), Vector(0, yb + 0.1, zf - 0.1),
              Vector(cx, yb + 1.0, zf - 0.1)):
        luft(p, 'iPad space')
    # Guide
    for sign in (+1, -1):
        material(Vector(sign * (xs + gt / 2), 0, zmid), 'guide')
        luft(Vector(sign * (xs + gt / 2), 0, P(sh, 'guide_h') + 0.3), 'above the guide')
        luft(Vector(sign * (xs - 1.0), 0, zf + 1.0), 'in front of the guide (no overhang)')
    # Claw: two pieces, gap in the middle
    for sign in (+1, -1):
        material(Vector(sign * cx, yb - kt / 2, zmid), 'claw wall')
        material(Vector(sign * cx, yb + P(sh, 'claw_lip') / 2, zf + P(sh, 'claw_lip_t') / 2), 'claw lip')
    luft(Vector(0, yb - kt / 2, zmid), 'gap in the claw wall')
    luft(Vector(0, yb + 1.0, zf + 1.0), 'gap in the lip')
    luft(Vector(P(sh, 'claw_gap') / 2 - 0.3, yb - kt / 2, zmid), 'gap edge')
    # Snap clip: wall, nose, ramp
    material(Vector(0, yt + ct / 2, zmid), 'snap clip wall')
    zr, ctop = P(sh, 'z_ramp'), P(sh, 'clip_top')
    material(Vector(0, yt - nose / 2, zf + P(sh, 'clip_under') + P(sh, 'clip_tip') / 2), 'nose')
    luft(Vector(0, yt - nose + 0.1, zr - 0.1), 'ramp of the nose')
    luft(Vector(0, yt - nose - 0.2, zf + 0.2), 'in front of the nose')
    luft(Vector(0, yt - nose + 0.1, zf + 0.05), 'inclined underside of the nose')
    material(Vector(0, yt + ct / 2, zr - 0.5), 'wall under the bend')
    if P(sh, 'tab_flare') / P(sh, 'tab_h') <= nose / P(sh, 'clip_ramp') + 1e-6:
        print('ERROR tab shallower than the ramp -> re-entrant bend inside'); ok = False
    material(Vector(0, yt + P(sh, 'tab_flare') + P(sh, 'tab_tip') / 2, ctop - 0.5), 'tab top')
    luft(Vector(0, yt + 0.5, ctop - 0.5), 'in front of the tab (inclined backwards)')
    luft(Vector(0, yt + ct + P(sh, 'tab_flare') + P(sh, 'tab_tip'), ctop - 0.5), 'behind the tab')
    print('Snap clip  nose %.1f deep, ramp %.1f high, underside %.1f inclined; tab %.1f high, '
          '%.1f backwards, %.1f -> %.1f thick'
          % (nose, P(sh, 'clip_ramp'), P(sh, 'clip_under'), P(sh, 'tab_h'),
             P(sh, 'tab_flare'), ct, P(sh, 'tab_tip')))
    # M6: clearance hole, flat counterbore m6_head_h deep, floor below
    r_cb = P(sh, 'm6_head_d') / 2 - 0.3
    hh, fl = P(sh, 'm6_head_h'), P(sh, 'm6_floor')
    for sign in (+1, -1):
        for x, y in ((0.0, sign * P(sh, 'm6_pitch') / 2),
                     (sign * P(sh, 'm6_pitch_h') / 2, 0.0)):
            luft(Vector(x, y, pt / 2), 'M6 clearance hole')
            luft(Vector(x + r_cb, y, pt - 0.1), 'counterbore top')
            luft(Vector(x + r_cb, y, pt - hh + 0.1), 'counterbore bottom')
            material(Vector(x + r_cb, y, pt - hh - 0.1), 'floor under the head')
            material(Vector(x + r_cb, y, 0.1), 'floor at the back')
    # Roundings: nothing may remain at the upper outer corner of the claw
    luft(Vector(cx + P(sh, 'claw_len') / 2 - 0.2, yb - kt + 0.2, P(sh, 'claw_top') - 0.2),
         'claw corner (rounding)')
    print('Counterbore 4 x flat counterbore %.1f x %.1f deep, floor %.1f, plate %.1f; '
          'vertical %.0f, horizontal %.0f'
          % (P(sh, 'm6_head_d'), hh, fl, pt, P(sh, 'm6_pitch'), P(sh, 'm6_pitch_h')))
    print('Claw     %d pieces of %.1f mm, wall %.1f, lip %.1f deep x %.1f thick, gap %.1f'
          % (2, P(sh, 'claw_len'), kt, P(sh, 'claw_lip'), P(sh, 'claw_lip_t'), P(sh, 'claw_gap')))
    if P(sh, 'corner_r') >= kt:
        print('ERROR corner_r %.1f >= wall thickness, rounding cuts the inner wall' % P(sh, 'corner_r'))
        ok = False
    print('Lever    upper screw to nose %.1f mm'
          % (yt - P(sh, 'm6_pitch') / 2))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('ipad-holder')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'ipad-holder.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'ipad-holder.step'))
    body.Shape.exportStl(os.path.join(here, 'ipad-holder.stl'))

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


main()
