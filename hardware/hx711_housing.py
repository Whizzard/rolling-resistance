# -*- coding: utf-8 -*-
"""
Housing for the load-cell board (SparkFun HX711 Load Cell Amp) on the pendulum frame.

Structure as in hall_housing.py: a PartDesign body with a feature tree plus a
spreadsheet "Parameter"; all sketches bound to the table through expressions.
Dimensions are changed in the GUI in the table, not in the sketches. Dimensions in mm.

The board is 30.6 x 23.0 mm and 4.3 mm thick with components, measured at the stand
(nominal per Eagle: 30.48 x 22.86 = 1.2 x 0.9 inch). The play is stated explicitly
in the table as `fit_w` / `fit_l` / `fit_t` — not hidden as an allowance in the
formula. `fit_t` is new: the distance between the apex of the snap lugs and the
lower edge of the ledge used to be EXACTLY pcb_t, i.e. zero clearance. A real 1.6 mm
board with its thickness tolerance therefore could not snap in. Now the apex sits
fit_t lower, so the board may sit up to 0.3 mm lower. The solder pads are on the two
SHORT SIDES — on the left the five connections of the load cell, on the right
VDD/VCC/DAT/CLK/GND to the computer. Two things follow from this:

  * The supporting ledge runs along the LONG SIDES. On the short sides it would
    sit on the solder joints.
  * The ledge is ASYMMETRIC, and that is the core of the version of
    2026-11-21. From SparkFun's Eagle data (repo sparkfun/HX711-Load-Cell-
    Amplifier, hardware/SparkFun_HX711_Load_Cell.brd, outline 30.48 x 22.86 mm):

        capacitor side     C6 and C7, two 10 uF in package 1210, centre
                           2.54 mm from the edge, body up to 1.24 mm from the
                           edge. Between them C3 and C5 (0603) up to 2.04 mm.
        free side          next component only at 3.95 mm (R4, C1).

    The old all-round ledge of 1.8 mm therefore reached 1.8 mm onto the
    board and hit the two 1210s -- they broke off at the first assembly.
    Now: ledge_cap = 0.8 mm on the capacitor side (+y),
    ledge_free = 2.8 mm on the free side (-y).

    In the CORNERS of the capacitor side the ledge is full width again: along
    this edge the first copper sits at x = 6.22 mm (pad of C3) and the last
    ends at 24.68 mm (pad of C6), so the corners are free. The narrow strip
    runs only in between.

    The board therefore fits in only ONE orientation. That is intentional:
    the wrong way round the capacitors hit the wide corner supports, and one
    notices before anything breaks.
  * Above the board there is room (cable_h), because the wires from both
    pad rows go upwards, lie across the board and leave together on ONE
    side. That is why both cable channels are at the same end.

Feature tree (object names as in FreeCAD):
    BaseBlock      base block, cuboid 50.6 x 28.9 x 12.3
    Cavity          cavity, wide part, z = 0...3.6  (board passage)
    ComponentSpace       component space, narrow part, full height  -> creates the ledge
    CornerLedges       corner supports: ledge full width again in the four corners
    SnapNoses         snap lugs: two wedges, centred, below the board
    SpringRelief         spring relief outside, so the wall flexes at the lugs
    ScrewHole       screw hole, M6 through, centred between the channels
    CableNeck_p/_m    cable neck: narrow neck at the open side — the claw
    CableGroove_p/_m     cable groove: circle above it, protrudes beyond the neck
    MouthChamfers         four small chamfers at the mouth edges
    CableTrough_p/_m   cable recess, deeper, at the transition, no clamping there

The clamping section is 1 mm narrower than the cable: the jacket is clamped between
groove and mounting surface, and THAT is the strain relief (docs/04-hardware.md §7).
The recess at the transition is deliberately deeper, so the jacket is not squashed
where the cable bends downwards.

Side wall 2.4 mm instead of 3.0: with a 0.4 mm nozzle that is exactly six lines,
so no thin infill seam in the middle. The **lid stays at 2.0 mm** — it spans the
cavity unsupported and sags more when thinner. The remaining wall at the spring
relief stays 0.8 mm, so the snap lugs flex unchanged.

Z = 0 is the open underside, which lies on the mounting surface.
"""

import os
import sys
import math

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# -------------------------------------------------------------- Parameters
# (name, value or formula, comment). Formulas start with '='.
PARAMS = [
    ('pcb_l',       30.6,  'Board length, measured'),
    ('pcb_w',       23.0,  'Board width, measured'),
    ('fit_w',        0.4,  'Play in width, TOTAL -> 0.2 per side'),
    ('fit_l',        0.6,  'Play in length, TOTAL -> 0.3 per end'),
    ('pcb_t',        1.6,  'Board thickness'),
    ('fit_t',        0.3,  'Play in thickness -> board may sit lower'),
    ('pcb_stack',    4.3,  'Board with components'),
    ('cable_h',      4.0,  'Room above the components for the wires'),
    ('solder_gap',   2.0,  'Solder space below the board'),
    ('headroom', '=pcb_stack - pcb_t + cable_h', 'Component space above the board'),

    ('wall',         2.4,  'Side wall, 6 lines with a 0.4 mm nozzle'),
    ('top',          2.0,  'Lid wall, stays at 2.0 (free span)'),
    ('end_wall',     2.0,  'End wall at the load-cell end'),
    ('rib',          2.0,  'Web to the screw'),
    ('screw_blk',   14.0,  'Length of the solid block'),

    ('cav_l',  '=pcb_l + fit_l',                 'Cavity length'),
    ('cav_w',  '=pcb_w + fit_w',                 'Cavity width'),
    ('cav_h',  '=solder_gap + pcb_t + headroom', 'Cavity height'),
    ('cav_x0', '=end_wall',                      'Cavity start'),
    ('cav_xm', '=cav_x0 + cav_l / 2',            'Cavity centre'),

    ('l_out',  '=end_wall + cav_l + rib + screw_blk', 'Outer length'),
    ('w_out',  '=cav_w + 2 * wall',                   'Outer width'),
    ('h_out',  '=cav_h + top',                        'Outer height'),

    ('pcb_z0', '=solder_gap',          'Board lower edge'),
    ('pcb_z1', '=pcb_z0 + pcb_t',      'Board upper edge = ledge'),

    ('ledge_cap',    0.8,  'Ledge CAPACITOR SIDE (+y) -- components up to 1.24 mm from the edge'),
    ('ledge_free',   2.8,  'Ledge free long side (-y) -- 3.9 mm free there'),
    ('ledge_corner', 2.8,  'Ledge in the CORNERS of the capacitor side'),
    ('corner_l',     5.0,  'Length of the corner support per end'),
    ('ledge_chamfer', 0.4, 'Chamfer at the lower edge of the ledge, inside'),
    ('ramp_l',       4.0,  'Length of the cable ramp at the channel end'),
    ('ramp_h',       1.5,  'Height of the cable ramp above the board upper edge'),
    ('snap_w',       0.65, 'Snap lug protrusion per side'),
    ('snap_l',       6.0,  'Snap lug length'),
    ('snap_h',       0.8,  'Snap lug half height'),
    ('snap_z', '=pcb_z1 - pcb_t - fit_t', 'Apex of the snap lug, fit_t below the nominal position'),

    ('relief_l',    14.0,  'Spring relief length'),
    ('relief_t',     0.8,  'Spring relief remaining wall'),
    ('relief_h', '=pcb_z1 + 2', 'Spring relief height'),

    ('screw_d',      6.6,  'M6 clearance, like the Hall housing'),
    ('screw_y',      0.0,  'Screw centred, between the channels'),
    ('screw_x', '=cav_x0 + cav_l + rib + screw_blk / 2', 'Screw position'),

    # Common to both channels
    ('cab_chamfer',  0.3,  'Chamfer at the groove mouth, lead-in for the cable'),
    ('cab_y',        8.6,  'Channel centre, magnitude — once each at +y and -y'),
    ('cab_x0', '=cav_x0 + cav_l', 'Channel start, at the end of the cavity'),
    ('cab_len','=l_out - cab_x0 + 1', 'Channel length'),
    ('cab_deep_x0', '=cab_x0', 'Recess start'),
    ('cab_deep_l',   5.0,  'Recess length'),

    # Channel +y — lead to the computer, finished jacket
    ('cab_cable_p',  4.0,  'Cable diameter, MEASURED'),
    ('cab_d_p',      3.9,  'Groove diameter, 0.1 below the cable -> clamps'),
    ('cab_mouth_p',  3.2,  'Neck width at the open side -> claw'),
    ('cab_depth_p',  3.4,  'Groove depth, 0.6 below the cable -> surface clamps'),
    ('cab_deep_d_p', 4.6,  'Recess width, ABOVE the cable -> no clamping'),
    ('cab_deep_p',   5.2,  'Recess depth, no clamping'),
    ('cab_z_p', '=cab_depth_p - cab_d_p / 2', 'Circle centre above the open side'),
    ('cab_claw_p', '=(cab_d_p - cab_mouth_p) / 2', 'Undercut per side'),

    # Channel -y — the four thin strands of the load cell plus heat-shrink tubing.
    # 3.0 mm is ESTIMATED, not measured: 4 wires AWG 28...30 with 0.7...1.0 mm
    # outer diameter give (1+sqrt(2))*d = 1.7...2.4 mm in a square bundle,
    # plus thin-walled 2:1 heat-shrink with 0.4...0.5 mm per side.
    # Makes 2.5...3.4 mm. Measure with the calliper before printing.
    # The clamping is GENTLER here (0.4 instead of 0.6 below the bundle): thin
    # strands in heat-shrink are soft, and 0.6 would flatten them.
    ('cab_cable_m',  3.0,  'Bundle of 4 strands + heat-shrink, ESTIMATED'),
    ('cab_d_m',      2.9,  'Groove diameter, 0.1 below the bundle'),
    ('cab_mouth_m',  2.4,  'Neck width -> claw 0.25 per side'),
    ('cab_depth_m',  2.6,  'Groove depth, 0.4 below the bundle'),
    ('cab_deep_d_m', 3.6,  'Recess width, ABOVE the bundle'),
    ('cab_deep_m',   4.2,  'Recess depth, no clamping'),
    ('cab_z_m', '=cab_depth_m - cab_d_m / 2', 'Circle centre above the open side'),
    ('cab_claw_m', '=(cab_d_m - cab_mouth_m) / 2', 'Undercut per side'),
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

    # 3 Component space, narrow part through the full height → creates the ledge.
    #   Narrower only in y: the ledge lies along the long sides, away from the pads.
    #   ASYMMETRIC, see header: +y is the capacitor side and gets only
    #   ledge_cap, -y is free and carries the main part with ledge_free.
    y0 = -(P(sh, 'cav_w') / 2 - P(sh, 'ledge_free'))
    inner = P(sh, 'cav_w') - P(sh, 'ledge_free') - P(sh, 'ledge_cap')
    sk = sketch_on(body, xy, 'Sk_ComponentSpace')
    rect(sk, P(sh, 'cav_x0'), y0, P(sh, 'cav_l'), inner, 'cmp',
         'Parameter.cav_x0', '-(Parameter.cav_w / 2 - Parameter.ledge_free)',
         'Parameter.cav_l',
         'Parameter.cav_w - Parameter.ledge_free - Parameter.ledge_cap')
    pocket(sk, 'ComponentSpace', 'Parameter.cav_h')

    # 3a Corner supports: on the capacitor side the ledge is narrow only where
    #    components really are. Along the edge the first copper sits at
    #    x = 6.22 mm (pad of C3) and the last ends at x = 24.68 mm (pad of
    #    C6), relative to the board edge. So there is room in the corners, and
    #    there the board gets its support back -- four points hold it better
    #    than a continuous 0.8 mm strip.
    ycorner = P(sh, 'cav_w') / 2 - P(sh, 'ledge_corner')
    e_ycorner = 'Parameter.cav_w / 2 - Parameter.ledge_corner'
    sk = sketch_on(body, xy, 'Sk_CornerLedges', P(sh, 'pcb_z1'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.pcb_z1')
    rect(sk, P(sh, 'cav_x0'), ycorner, P(sh, 'corner_l'), P(sh, 'ledge_corner'), 'corner_a',
         'Parameter.cav_x0', e_ycorner, 'Parameter.corner_l', 'Parameter.ledge_corner')
    rect(sk, P(sh, 'cav_x0') + P(sh, 'cav_l') - P(sh, 'corner_l'), ycorner,
         P(sh, 'corner_l'), P(sh, 'ledge_corner'), 'corner_b',
         'Parameter.cav_x0 + Parameter.cav_l - Parameter.corner_l', e_ycorner,
         'Parameter.corner_l', 'Parameter.ledge_corner')
    pad(sk, 'CornerLedges', 'Parameter.headroom')

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
    if len(edge_names) < 3:
        raise RuntimeError('Ledge edges not found: %s' % edge_names)
    f = body.newObject('PartDesign::Chamfer', 'LedgeChamfers')
    f.Base = (tip, edge_names)
    f.setExpression('Size', 'Parameter.ledge_chamfer')
    doc.recompute()

    # 3c Cable ramp: at the channel end the underside of the ledge rises.
    #
    #    Here the wires leave the board and dip into the cable channels --
    #    and since the corner supports there is material above them here.
    #    Without the ramp the board pinches the strands exactly here when it
    #    is pushed in. With it the gap opens like a wedge to ramp_h, and the
    #    wires run out instead of in.
    #
    #    Sketch in XZ: local x = global x, local y = global z, normal -Y.
    #    The cut is symmetric over the whole cavity width, the side walls
    #    stay standing.
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

    # 6 Screw hole, centred — the two channels lie to the right and left of it
    sk = sketch_on(body, xy, 'Sk_ScrewHole')
    circle(sk, P(sh, 'screw_x'), P(sh, 'screw_y'), P(sh, 'screw_d') / 2, 'scr',
           'Parameter.screw_x', 'Parameter.screw_y', 'Parameter.screw_d / 2')
    pocket(sk, 'ScrewHole', 'Parameter.h_out + 1')

    # 7...10 The two cable channels, each a D profile (rectangle + round).
    #   First the clamping section over the full length, then the deeper recess
    #   at the transition, where the cable bends down out of the housing.
    #   BOTH CHANNELS HAVE THEIR OWN DIMENSIONS: +y carries the finished lead to
    #   the computer, -y only the four thin strands of the load cell in
    #   heat-shrink. One channel for both would be too wide for the bundle,
    #   and then nothing clamps.
    for sign, t in ((+1, 'p'), (-1, 'm')):
        rm = P(sh, 'cab_deep_d_' + t) / 2
        e_rm = 'Parameter.cab_deep_d_%s / 2' % t
        e_y = '%d * Parameter.cab_y' % sign
        e_xm = '%d * Parameter.cab_y - Parameter.cab_deep_d_%s / 2' % (sign, t)
        y = sign * P(sh, 'cab_y')

        # Clamping section, omega: a NECK that is narrower than the circle above.
        # The circle protrudes beyond the neck by cab_claw per side — that is
        # the undercut that holds the cable before the surface is under it.
        # A mere cut of the circle could not do that: a narrow mouth would
        # inevitably entail a deep groove, and the groove must stay at
        # cab_depth so that the surface still clamps.
        e_neck = '%d * Parameter.cab_y - Parameter.cab_mouth_%s / 2' % (sign, t)
        sk = sketch_on(body, yz, 'Sk_CableNeck_' + t, P(sh, 'cab_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
        rect(sk, y - P(sh, 'cab_mouth_' + t) / 2, 0.0,
             P(sh, 'cab_mouth_' + t), P(sh, 'cab_z_' + t), 'neck' + t,
             e_neck, '0', 'Parameter.cab_mouth_%s' % t, 'Parameter.cab_z_%s' % t)
        pocket(sk, 'CableNeck_' + t, 'Parameter.cab_len')

        sk = sketch_on(body, yz, 'Sk_CableGroove_' + t, P(sh, 'cab_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
        circle(sk, y, P(sh, 'cab_z_' + t), P(sh, 'cab_d_' + t) / 2, 'grv' + t,
               e_y, 'Parameter.cab_z_%s' % t, 'Parameter.cab_d_%s / 2' % t)
        pocket(sk, 'CableGroove_' + t, 'Parameter.cab_len')

        # Recess: U, wider than the cable. Deliberately no clamping here,
        # this is where the cable bends.
        sk = sketch_on(body, yz, 'Sk_CableTrough_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        rect(sk, y - rm, 0.0, P(sh, 'cab_deep_d_' + t),
             P(sh, 'cab_deep_' + t) - rm, 'tr' + t,
             e_xm, '0', 'Parameter.cab_deep_d_%s' % t,
             'Parameter.cab_deep_%s - Parameter.cab_deep_d_%s / 2' % (t, t))
        pocket(sk, 'CableTrough_' + t, 'Parameter.cab_deep_l')

        sk = sketch_on(body, yz, 'Sk_CableTrough_Round_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        circle(sk, y, P(sh, 'cab_deep_' + t) - rm, rm, 'trr' + t,
               e_y, 'Parameter.cab_deep_%s - Parameter.cab_deep_d_%s / 2' % (t, t), e_rm)
        pocket(sk, 'CableTrough_Round_' + t, 'Parameter.cab_deep_l')

    # 11 Chamfer at the groove mouth, four edges (two per channel).
    #
    #   The cable has to be pressed through the narrower neck; without a chamfer
    #   the sharp edge bites into the jacket. The round shape stays untouched —
    #   the chamfer sits BELOW the undercut and takes nothing away from the claw.
    #
    #   There is a hard limit for this: the circle only emerges from the neck at
    #       z = cab_depth - R - sqrt(R² - (neck/2)²)
    #   The chamfer may reach up to there, no further.
    tip = body.Tip
    mouth_ys = []
    for sign, t in ((+1, 'p'), (-1, 'm')):
        R = P(sh, 'cab_d_' + t) / 2
        half_w = P(sh, 'cab_mouth_' + t) / 2
        chamfer_start = P(sh, 'cab_depth_' + t) - R - math.sqrt(R * R - half_w * half_w)
        if P(sh, 'cab_chamfer') >= chamfer_start:
            raise RuntimeError(
                'Channel %s: chamfer %.2f reaches into the undercut '
                '(starts at z = %.3f)'
                % (t, P(sh, 'cab_chamfer'), chamfer_start))
        mouth_ys += [sign * P(sh, 'cab_y') + side * half_w for side in (+1, -1)]
    edge_names = []
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Vertexes[-1].Point - e.Vertexes[0].Point
        if abs(d.y) > 1e-6 or abs(d.z) > 1e-6:
            continue                                   # not along x
        q = e.Vertexes[0].Point
        if abs(q.z) > 1e-6:
            continue                                   # not at the open side
        if any(abs(q.y - k) < 1e-6 for k in mouth_ys):
            edge_names.append('Edge%d' % (i + 1))
    if len(edge_names) != 4:
        raise RuntimeError('Mouth edges not found unambiguously: %s' % edge_names)
    f = body.newObject('PartDesign::Chamfer', 'MouthChamfers')
    f.Base = (tip, edge_names)
    f.setExpression('Size', 'Parameter.cab_chamfer')
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
    """Body visible, sketches and intermediate steps hidden."""
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hx711-housing')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'hx711-housing.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'hx711-housing.step'))
    body.Shape.exportStl(os.path.join(here, 'hx711-housing.stl'))

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
