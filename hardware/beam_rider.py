# -*- coding: utf-8 -*-
"""
Rider for the balance beam (calibration arm): slidable, clamped with an M6 across
under the beam, at the top a tab with the hole for the 2 mm wire hook from which the
calibration weight hangs. For printing, without supports.

Beam (stated 2026-11-24): 15 mm wide, 25 mm high. Hole for the wire in the centre,
15 mm above the top of the beam. Two clamping screws, one at each end, OUTSIDE the
area swept by the arms of the weight clip while swinging (at 15 degrees and 40 mm
below the pivot +-11 mm about the centre, plus the screw head).

POSITION: x runs along the beam, y across, z upwards. The beam lies at
y = +-beam_w/2, z = 0 (underside) to beam_h (top). The rider is a U profile, constant
over the length len along x -- therefore it prints LYING ON ITS END FACE ("as a U"):
all walls stand vertical; the wire hole and M6 holes (both axis y, across the tube)
are short horizontal bridges, the wire hole is drilled out afterwards.

Profile (y-z), from bottom to top:
    Legs         two, leg_t = 1.5 thick, from z = -leg_below (below the beam) up to
                 the roof; leg_below follows from the M6 hole (hole + under_wall).
                 At each screw a block boss_t thick outside (thread and head seat),
                 from the rider end boss_len inwards, centred on the M6 axis (from the
                 leg bottom to as far above the axis), with a 45 degree lead-in on
                 the inside (at the +x end that is the print underside).
                 INNER blocks (2026-11-26, builder: "the same pads inside"): the same
                 footprint mirrored to the inside of each leg at each screw -- same x
                 extent and 45 degree lead-in, from the leg bottom up -- but only as
                 deep (y) as the claw (to y_tip) and only up to the claw top line, so
                 they keep the claws' play to the beam. At the bottom a 45 degree
                 lead-in over claw_d, so the beam top edge spreads the legs when the
                 rider is pushed on, as at the claws. The M6 thread then runs through
                 outer block + leg + inner block.
    Roof         top_t thick above the beam, with fit_z clearance
    Claws        one on each leg at the underside of the beam, reaching claw_d under
                 it -- the snap claws of the vial saddle (level_saddle.py, same
                 claw_d, claw_tip_t, claw_rise). The top side rises from the hook end
                 to the leg by claw_rise; under the hook a 45 degree lead-in, so the
                 lower edge of the beam spreads the legs when the rider is pushed on
                 from above and the claws click in under the beam. Height: with the
                 roof resting on the beam and the legs unloaded (fit_w/2 play per
                 side) the slope just touches the lower edge of the beam (claw_gap
                 follows from that): no play, no preload. The rider still slides
                 along the beam; the M6 screws only clamp -- pulling the legs in,
                 the slope turns that into a pull that holds the roof on the beam.
    Hump         ONLY at the cross bore, across the FULL width of the rider,
                 symmetric: hump_w long (x) at the top, up to hump_over above the hole
                 centre, 45 degree flanks down to the roof on both sides (the -x flank
                 is the underside when printing lying down and needs no support)
Holes:
    Wire         Ø wire_d + fit, axis y (ACROSS the tube, parallel to the clamping
                 screw), centred in x, hole_h above the top of the beam, through the
                 full width: the wire runs in the hump over 2 * y_out, not just over a
                 narrow tab. It is the pivot for the weight clip (weight_clip.py);
                 the hole centre is the point of load application in x.
    M6 across    two, axis y, at x = +-screw_x, screw_below below the beam -- since
                 the claws do the locating (2026-11-26), as high as possible: the
                 clearance hole is tangent to the underside of the beam, so the screw
                 runs directly under the beam at the height of the claws. The holes
                 run through the inner blocks (see Legs), which fill the claw line
                 there. Front
                 (+y) clearance Ø 6.4, back (-y) a MODELLED M6 thread (PartDesign
                 hole, ISO profile, as in esp_bracket.py). Tightening pulls the legs
                 together at the bottom and clamps the beam. With 0.1 to 0.15 mm
                 layers the thread comes out usable; otherwise run a tap through once,
                 it finds the turns.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/beam_rider.py
Produces hardware/beam-rider.FCStd, .step, .stl.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

PARAMS = [
    ('beam_w',    15.0, 'Beam width (y)'),
    ('beam_h',    25.0, 'Beam height (z)'),
    ('fit_w',      0.3, 'Play across, TOTAL'),
    ('fit_z',      0.2, 'Clearance between top of beam and roof'),
    ('len',       56.0, 'Rider length (x); screws outside, arms of the clip in the middle'),
    ('screw_x',   22.0, 'Clamping screws at +-x'),
    ('leg_t',      1.5, 'Leg thickness'),
    ('boss_t',     2.5, 'Block at each screw, outside on the leg'),
    ('boss_len',  13.0, 'Block length (x), from the rider end inwards'),
    ('under_wall', 2.3, 'Leg below the M6 clearance hole, as in the first version (12 - 6.5 - 3.2)'),
    ('top_t',      4.0, 'Roof thickness'),
    ('hump_w',    12.0, 'Hump length at the top (x), centred; at the bottom + 2 * height'),
    ('hump_over',  5.0, 'Hump above the hole centre'),
    ('hole_h',    15.0, 'Wire hole above the top of the beam, GIVEN'),
    ('wire_d',     2.0, 'Wire'),
    ('wire_fit',   0.3, 'Play in the wire hole'),
    ('m6_thru',    6.4, 'Clearance at the front (+y)'),
    ('m6_thread', 'M6', 'Thread at the back (-y), modelled'),
    ('m6_core',    5.0, 'Core diameter M6, only for the check'),
    # Snap claws, as in level_saddle.py
    ('claw_d',     1.5, 'Claw reaches this far under the beam (from the leg inside)'),
    ('claw_tip_t', 0.8, 'Height of the claw end face'),
    ('claw_rise',  0.4, 'Rise of the claw top from the end to the leg (0.4 over 1.5 = 15 degrees)'),
    # Derived
    ('y_in',  '=(beam_w + fit_w) / 2',      'Legs inside'),
    ('y_out', '=y_in + leg_t',              'Legs outside'),
    ('z_top0', '=beam_h + fit_z',           'Roof bottom'),
    ('z_top1', '=z_top0 + top_t',           'Roof top'),
    ('z_hole', '=beam_h + hole_h',          'Wire hole centre'),
    ('z_hump', '=z_hole + hump_over',       'Hump top'),
    ('y_boss', '=y_out + boss_t',           'Block outside'),
    ('screw_below', '=m6_thru / 2',         'M6 axis below the underside of the beam: clearance hole tangent to it'),
    ('leg_below', '=screw_below + m6_thru / 2 + under_wall', 'Legs below the underside of the beam'),
    ('z_boss', '=leg_below - 2 * screw_below', 'Block top: as far above the M6 axis as the leg bottom is below it'),
    ('y_tip',  '=y_in - claw_d',            'Claw end'),
    ('claw_gap', '=claw_rise * (claw_d - fit_w / 2) / claw_d', 'Claw end below the underside of the beam, roof seated: slope touches the beam edge'),
    ('z_tip',  '=fit_z - claw_gap',         'Claw end top (model: roof fit_z above the beam)'),
    ('z_root', '=z_tip + claw_rise',        'Claw top side at the leg'),
    ('z_tipb', '=z_tip - claw_tip_t',       'Claw end bottom'),
    ('z_lead', '=z_tipb - claw_d',          'End of the 45 degree lead-in at the leg'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter'); sh.set('B1', 'Value'); sh.set('C1', 'Meaning')
    for i, (name, val, note) in enumerate(PARAMS):
        r = i + 2; sh.set('A%d' % r, name); sh.setAlias('B%d' % r, name); sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(PARAMS):
        sh.set('B%d' % (i + 2), str(val))
    doc.recompute()
    return sh


def P(sh, name): return float(sh.get(name))


def named(sk, con, name):
    i = sk.addConstraint(con); sk.renameConstraint(i, name); return i


def poly(sk, pts, tag, exprs):
    v = Vector; n = len(pts); g = []
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
    ex1 = '(%s) + (%s)' % (ex0, ew); ey1 = '(%s) + (%s)' % (ey0, eh)
    return poly(sk, pts, tag, [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)])


def circle(sk, cx, cy, r, tag, ex, ey, er):
    g = sk.addGeometry(Part.Circle(Vector(cx, cy, 0), Vector(0, 0, 1), r), False)
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g, 3, cx), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g, 3, cy), tag + '_y')
    sk.setExpression('Constraints.%s_r' % tag, er); sk.setExpression('Constraints.%s_x' % tag, ex); sk.setExpression('Constraints.%s_y' % tag, ey)
    return g


def sketch_on(body, plane, name):
    sk = body.newObject('Sketcher::SketchObject', name)
    try: sk.AttachmentSupport = [(plane, '')]
    except Exception: sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'BeamRider')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    yz, xz = planes['YZ_Plane'], planes['XZ_Plane']

    def pad(sk, name, expr_len, midplane=True):
        f = body.newObject('PartDesign::Pad', name); f.Profile = sk
        f.setExpression('Length', expr_len); f.Midplane = midplane; doc.recompute(); return f

    def pocket(sk, name, expr_len=None, reversed_=False, midplane=False):
        f = body.newObject('PartDesign::Pocket', name); f.Profile = sk
        if expr_len: f.setExpression('Length', expr_len)
        else: f.Type = 'ThroughAll'
        f.Midplane = midplane; f.Reversed = reversed_; doc.recompute(); return f

    yi, yo, lb = P(sh, 'y_in'), P(sh, 'y_out'), P(sh, 'leg_below')
    zt0, zt1, zh = P(sh, 'z_top0'), P(sh, 'z_top1'), P(sh, 'z_hole')

    # 1 U profile in the YZ plane (local x = y, local y = z), padded along x.
    pts = [(-yo, -lb), (yo, -lb), (yo, zt1), (-yo, zt1)]
    sk = sketch_on(body, yz, 'Sk_Outer')
    poly(sk, pts, 'a', [('-Parameter.y_out', '-Parameter.leg_below'), ('Parameter.y_out', '-Parameter.leg_below'),
                        ('Parameter.y_out', 'Parameter.z_top1'), ('-Parameter.y_out', 'Parameter.z_top1')])
    pad(sk, 'Block', 'Parameter.len')
    # Remove the beam space: from the bottom up to below the roof
    sk = sketch_on(body, yz, 'Sk_BeamSpace')
    rect(sk, -yi, -lb - 1, 2 * yi, lb + 1 + zt0, 'r',
         '-Parameter.y_in', '-Parameter.leg_below - 1', '2 * Parameter.y_in', 'Parameter.leg_below + 1 + Parameter.z_top0')
    pocket(sk, 'BeamSpace', midplane=True)

    # 2 Hump: profile in XZ across the full width (pad 2*y_out midplane): trapezoid,
    #   at the top x = +-hump_w/2 at z_hump, 45 degrees on both sides down to the roof (0.5 overlap).
    hw, zhu = P(sh, 'hump_w') / 2, P(sh, 'z_hump')
    hh = zhu - zt1
    sk = sketch_on(body, xz, 'Sk_Hump')
    HH = '(Parameter.z_hump - Parameter.z_top1)'
    poly(sk, [(-hw - hh, zt1 - 0.5), (hw + hh, zt1 - 0.5), (hw, zhu), (-hw, zhu)], 'h',
         [('-Parameter.hump_w / 2 - ' + HH, 'Parameter.z_top1 - 0.5'),
          ('Parameter.hump_w / 2 + ' + HH, 'Parameter.z_top1 - 0.5'), ('Parameter.hump_w / 2', 'Parameter.z_hump'),
          ('-Parameter.hump_w / 2', 'Parameter.z_hump')])
    pad(sk, 'Hump', '2 * Parameter.y_out', midplane=True)

    # 2a Blocks at the screws: trapezoids in XY, 45 degree lead-in on the inside (towards
    #    the rider centre), outside on both legs, from z = z_boss down to -leg_below (pad towards -z).
    sx, L2, bl, bt = P(sh, 'screw_x'), P(sh, 'len') / 2, P(sh, 'boss_len'), P(sh, 'boss_t')
    xy = planes['XY_Plane']
    sk = sketch_on(body, xy, 'Sk_Bosses')
    sk.AttachmentOffset = App.Placement(Vector(0, 0, P(sh, 'z_boss')), App.Rotation())
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.z_boss')
    for side, tag in ((+1, 'bp'), (-1, 'bm')):
        # x_e = rider end, x_i = inner edge; the lead-in is at x_i
        x_e, x_i = side * L2, side * (L2 - bl)
        ex_e = ('' if side > 0 else '-') + 'Parameter.len / 2'
        ex_i = ex_e + (' - ' if side > 0 else ' + ') + 'Parameter.boss_len'
        ex_ib = ex_i + (' + ' if side > 0 else ' - ') + 'Parameter.boss_t'
        for ysign, t2 in ((+1, 'a'), (-1, 'b')):
            ys = ('' if ysign > 0 else '-') + 'Parameter.y_out' + (' - 0.5' if ysign > 0 else ' + 0.5')
            yb = ('' if ysign > 0 else '-') + 'Parameter.y_boss'
            poly(sk, [(x_i, ysign * (yo - 0.5)), (x_e, ysign * (yo - 0.5)), (x_e, ysign * (yo + bt)), (x_i + side * bt, ysign * (yo + bt))], tag + t2,
                 [(ex_i, ys), (ex_e, ys), (ex_e, yb), (ex_ib, yb)])
    f = body.newObject('PartDesign::Pad', 'Bosses'); f.Profile = sk
    f.setExpression('Length', 'Parameter.z_boss + Parameter.leg_below'); f.Reversed = True; doc.recompute()
    if not body.Shape.isInside(Vector(sx, yo + bt / 2, -P(sh, 'leg_below') / 2), 1e-6, True):
        f.Reversed = False; doc.recompute()
    assert body.Shape.isInside(Vector(sx, yo + bt / 2, -P(sh, 'leg_below') / 2), 1e-6, True), 'blocks go in the wrong direction'

    # 2b Snap claws: one polygon per leg in YZ (local x = y, local y = z), 0.5 mm into
    #    the leg, padded over the full length like the U profile.
    sk = sketch_on(body, yz, 'Sk_Claws')
    # (y name, y offset, z name) per corner: leg inside, hook top, hook end, lead-in
    corners = [('y_in', 0.5, 'z_root'), ('y_in', 0.0, 'z_root'), ('y_tip', 0.0, 'z_tip'),
               ('y_tip', 0.0, 'z_tipb'), ('y_in', 0.0, 'z_lead'), ('y_in', 0.5, 'z_lead')]
    for sgn, tag in ((+1, 'cp'), (-1, 'cm')):
        S = '' if sgn > 0 else '-'
        cs = corners if sgn > 0 else list(reversed(corners))
        pts = [(sgn * (P(sh, yn) + dy), P(sh, zn)) for yn, dy, zn in cs]
        ex = [(S + '(Parameter.%s + %g)' % (yn, dy), 'Parameter.' + zn) for yn, dy, zn in cs]
        poly(sk, pts, tag, ex)
    pad(sk, 'Claws', 'Parameter.len')

    # 2c Inner blocks: the outer block footprint mirrored to the inside of each leg,
    #    y from the leg (0.5 into it) to the claw end y_tip, 45 degree lead-in towards
    #    the rider centre; padded from z_tip (below the claw slope) down to -leg_below.
    sk = sketch_on(body, xy, 'Sk_Inner_Bosses')
    sk.AttachmentOffset = App.Placement(Vector(0, 0, P(sh, 'z_tip')), App.Rotation())
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.z_tip')
    cdp = P(sh, 'claw_d'); ytp = P(sh, 'y_tip')
    for side, tag in ((+1, 'ip'), (-1, 'im')):
        x_e, x_i = side * L2, side * (L2 - bl)
        ex_e = ('' if side > 0 else '-') + 'Parameter.len / 2'
        ex_i = ex_e + (' - ' if side > 0 else ' + ') + 'Parameter.boss_len'
        ex_ic = ex_i + (' + ' if side > 0 else ' - ') + 'Parameter.claw_d'
        for ysign, t2 in ((+1, 'a'), (-1, 'b')):
            S = '' if ysign > 0 else '-'
            yl = S + '(Parameter.y_in + 0.5)'; yt = S + 'Parameter.y_tip'
            poly(sk, [(x_i, ysign * (yi + 0.5)), (x_e, ysign * (yi + 0.5)), (x_e, ysign * ytp), (x_i + side * cdp, ysign * ytp)], tag + t2,
                 [(ex_i, yl), (ex_e, yl), (ex_e, yt), (ex_ic, yt)])
    f = body.newObject('PartDesign::Pad', 'Inner_Bosses'); f.Profile = sk
    f.setExpression('Length', 'Parameter.z_tip + Parameter.leg_below'); f.Reversed = True; doc.recompute()
    probe = Vector(sx, ytp + 0.3, -P(sh, 'leg_below') / 2)
    if not body.Shape.isInside(probe, 1e-6, True):
        f.Reversed = False; doc.recompute()
    assert body.Shape.isInside(probe, 1e-6, True), 'inner blocks go in the wrong direction'
    # 45 degree lead-in at the bottom of the inner blocks (YZ profile, through all along x)
    sk = sketch_on(body, yz, 'Sk_Inner_Lead')
    for sgn, tag in ((+1, 'lp'), (-1, 'lm')):
        S = '' if sgn > 0 else '-'
        cs = [('y_in', 0.0, -1.0, 0), ('y_in', 0.0, 0.0, 0), ('y_tip', 0.0, 0.0, 1), ('y_tip', -0.5, 0.0, 1), ('y_tip', -0.5, -1.0, 0)]
        if sgn < 0: cs = list(reversed(cs))
        pts = [(sgn * (P(sh, yn) + dy), -P(sh, 'leg_below') + dz + c * cdp) for yn, dy, dz, c in cs]
        ex = [(S + '(Parameter.%s + %g)' % (yn, dy), '-Parameter.leg_below + %g%s' % (dz, ' + Parameter.claw_d' if c else '')) for yn, dy, dz, c in cs]
        poly(sk, pts, tag, ex)
    pocket(sk, 'Inner_Lead', midplane=True)

    # 3 Wire hole, axis y (across), centred in x, through the tab
    sk = sketch_on(body, xz, 'Sk_Wire')
    circle(sk, 0, zh, (P(sh, 'wire_d') + P(sh, 'wire_fit')) / 2, 'w', '0', 'Parameter.z_hole', '(Parameter.wire_d + Parameter.wire_fit) / 2')
    pocket(sk, 'WireHole', midplane=True)

    # 4 M6 across (axis y) under the beam: core hole through both legs, clearance
    #   only at the front (+y). Sketch on XZ (local x = x, local y = z), pocket from y = 0.
    zs = -P(sh, 'screw_below')
    sx = P(sh, 'screw_x')
    sk = sketch_on(body, xz, 'Sk_M6_Clearance')
    circle(sk, sx, zs, P(sh, 'm6_thru') / 2, 'md1', 'Parameter.screw_x', '-Parameter.screw_below', 'Parameter.m6_thru / 2')
    circle(sk, -sx, zs, P(sh, 'm6_thru') / 2, 'md2', '-Parameter.screw_x', '-Parameter.screw_below', 'Parameter.m6_thru / 2')
    f = pocket(sk, 'M6_Clearance', 'Parameter.y_boss + 1', reversed_=False)
    # Check direction: the clearance must be in the +y leg; otherwise reverse.
    probe = Vector(sx, yi + P(sh, 'leg_t') / 2, zs + 2.9)   # just below the clearance radius
    if body.Shape.isInside(probe, 1e-6, True):
        f.Reversed = True; doc.recompute()

    # M6 thread at the back (-y): PartDesign hole with modelled ISO thread, sketch on
    # the outer face of the back leg, depth = leg. Direction checked with a probe as
    # above and reversed if necessary.
    sk = sketch_on(body, xz, 'Sk_M6_Thread')
    sk.AttachmentOffset = App.Placement(Vector(0, 0, 0), App.Rotation())
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.y_boss')     # XZ normal points to -y: offset +y_boss lands at y = -y_boss
    circle(sk, sx, zs, 2.5, 'mt1', 'Parameter.screw_x', '-Parameter.screw_below', '2.5')
    circle(sk, -sx, zs, 2.5, 'mt2', '-Parameter.screw_x', '-Parameter.screw_below', '2.5')
    h = body.newObject('PartDesign::Hole', 'M6_Thread')
    h.Profile = sk
    h.DepthType = 'Dimension'
    h.setExpression('Depth', 'Parameter.y_boss - Parameter.y_tip + 0.5')
    h.Threaded = True
    h.ThreadType = 'ISOMetricProfile'
    thread = str(sh.get('m6_thread'))
    sizes = h.getEnumerationsOfProperty('ThreadSize')
    if thread not in sizes:
        alt = [z for z in sizes if z.startswith('M6')]
        if not alt: raise RuntimeError('Thread %s not in %s' % (thread, sizes))
        thread = alt[0]
    h.ThreadSize = thread
    h.ModelThread = True
    doc.recompute()
    back = Vector(sx, -(yi + P(sh, 'leg_t') / 2), zs)    # centre of the back leg, hole axis
    if body.Shape.isInside(back, 1e-6, True):
        # Hole went in the wrong direction (air or front leg): move the sketch to the
        # other side and/or reverse until the axis at the back is clear.
        for off, rev in (('-Parameter.y_boss', False), ('Parameter.y_boss', True), ('-Parameter.y_boss', True)):
            sk.setExpression('AttachmentOffset.Base.z', off); h.Reversed = rev; doc.recompute()
            if not body.Shape.isInside(back, 1e-6, True): break
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
    import re, shutil, zipfile
    with zipfile.ZipFile(fcstd) as z:
        names = z.namelist(); doc_xml = z.read('Document.xml').decode('utf-8'); data = {n: z.read(n) for n in names}
    order, seen = [], set()
    for o in re.findall(r'<Object name="([^"]+)"', doc_xml):
        if o not in seen: seen.add(o); order.append(o)
    xml = _GUI_HEAD % len(order)
    for o in order: xml += _GUI_VP % (o, 'true' if o in visible else 'false')
    xml += _GUI_TAIL
    tmp = fcstd + '.new'
    with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as z:
        for n in names:
            if n != 'GuiDocument.xml': z.writestr(n, data[n])
        z.writestr('GuiDocument.xml', xml)
    shutil.move(tmp, fcstd)


def set_visibility(doc, body):
    for o in doc.Objects:
        if hasattr(o, 'Visibility'): o.Visibility = False
    body.Visibility = True
    if body.Tip is not None: body.Tip.Visibility = True


def check(body, sh):
    s = body.Shape; ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    yi, yo, lb, lt = P(sh, 'y_in'), P(sh, 'y_out'), P(sh, 'leg_below'), P(sh, 'leg_t')
    bh, zt0, zt1, zh = P(sh, 'beam_h'), P(sh, 'z_top0'), P(sh, 'z_top1'), P(sh, 'z_hole')
    zs, rc, rt = -P(sh, 'screw_below'), P(sh, 'm6_core') / 2, P(sh, 'm6_thru') / 2
    ym = yi + lt / 2
    ymb = yo + P(sh, 'boss_t') / 2   # centre of the block
    # Beam clear, legs and roof present
    for z in (1.0, bh / 2, bh - 0.5):
        if inside(0, 0, z) or inside(0, yi - 0.2, z): print('ERROR beam space not clear, z=%.1f' % z); ok = False
    if not inside(0, ym, bh / 2) or not inside(0, -ym, bh / 2): print('ERROR leg missing'); ok = False
    if not inside(0, 0, (zt0 + zt1) / 2): print('ERROR roof missing'); ok = False
    if not inside(0, ym, -lb + 1): print('ERROR leg does not reach below the beam'); ok = False
    # Tab and wire hole
    hw = P(sh, 'hump_w') / 2
    for y in (0, yo - 0.3, -(yo - 0.3)):
        if inside(0, y, zh): print('ERROR wire hole missing at y=%.1f' % y); ok = False
        if not inside(hw - 1, y, zh): print('ERROR hump next to the hole missing at y=%.1f' % y); ok = False
    if not inside(0, 0, zh + P(sh, 'hump_over') - 0.5): print('ERROR hump above the hole missing'); ok = False
    if inside(0, 0, zh + P(sh, 'hump_over') + 0.5): print('ERROR hump too high'); ok = False
    hh = P(sh, 'z_hump') - zt1  # hump height above the roof
    for sgn in (+1, -1):
        if inside(sgn * (hw + hh + 1), 0, zt1 + 0.5): print('ERROR hump too long in x'); ok = False
        if not inside(sgn * (hw + hh - 1), 0, zt1 + 0.5): print('ERROR hump flank missing'); ok = False
    if inside(P(sh, 'len') / 2 - 1, 0, zt1 + 1): print('ERROR tab still runs through'); ok = False
    bt = P(sh, 'boss_t')
    if not inside(P(sh, 'screw_x'), yo + bt / 2, -P(sh, 'leg_below') + 1.5): print('ERROR block missing'); ok = False
    if not inside(-P(sh, 'screw_x'), -(yo + bt / 2), -P(sh, 'leg_below') + 1.5): print('ERROR block -x/-y missing'); ok = False
    if inside(0, yo + bt / 2, -P(sh, 'leg_below') / 2): print('ERROR block in the middle'); ok = False
    # M6: core hole at the back (material between rc and rt), clearance at the front (clear up to rt)
    pr = rc + (rt - rc) / 2
    for sx in (P(sh, 'screw_x'), -P(sh, 'screw_x')):
        for yy in (-ym, -ymb):
            if inside(sx, yy, zs) or inside(sx, yy, zs + rc / 2 - 0.3): print('ERROR threaded hole at the back missing'); ok = False
            if not inside(sx, yy, zs + 3.3): print('ERROR the clearance is at the back instead of the thread'); ok = False
        for yy in (ym, ymb):
            if inside(sx, yy, zs + pr): print('ERROR clearance at the front missing'); ok = False
            if not inside(sx, yy, zs + rt + 0.5): print('ERROR leg above the clearance missing'); ok = False
    if not inside(0, ym, zs): print('ERROR there must be no screw in the middle'); ok = False
    # Snap claws: end face, slope crossing the beam edge at the roof-seated height,
    # lead-in below, notched only at the screws; M6 shank path clear under the beam
    ytip, ztip, zroot, ztipb, zlead = (P(sh, n) for n in ('y_tip', 'z_tip', 'z_root', 'z_tipb', 'z_lead'))
    cd, cr, fz = P(sh, 'claw_d'), P(sh, 'claw_rise'), P(sh, 'fit_z')
    for sgn in (+1, -1):
        for x in (0, P(sh, 'len') / 2 - 0.5):
            if not inside(x, sgn * (ytip + 0.3), (ztip + ztipb) / 2): print('ERROR claw end face missing at y=%+.1f' % (sgn * ytip)); ok = False
        if inside(0, sgn * (ytip - 0.3), (ztip + ztipb) / 2): print('ERROR claw too long'); ok = False
        if inside(0, sgn * (ytip + 0.2), ztip + 0.2): print('ERROR claw end above its top'); ok = False
        if not inside(0, sgn * (yi - 0.3), zroot - 0.3) or inside(0, sgn * (yi - 0.3), zroot + 0.3): print('ERROR claw root wrong'); ok = False
        if inside(0, sgn * (yi - 0.5), zlead + 0.2): print('ERROR lead-in slope missing'); ok = False
        if not inside(0, sgn * (yi - 0.1), zlead + 0.6): print('ERROR lead-in slope too steep'); ok = False
    z_edge = ztip + cr * (P(sh, 'beam_w') / 2 - ytip) / cd       # slope height at the beam edge
    if abs(z_edge - fz) > 1e-6: print('ERROR slope meets the beam edge at %.3f, not at the roof clearance %.3f' % (z_edge, fz)); ok = False
    if not ytip < P(sh, 'beam_w') / 2 - 1: print('ERROR claw does not reach under the beam'); ok = False
    if zs + rt > 1e-6: print('ERROR M6 clearance cuts into the beam'); ok = False
    for sx_ in (P(sh, 'screw_x'), -P(sh, 'screw_x')):
        for yy in (yi - 0.3, ytip + 0.3, 0):
            if inside(sx_, yy, zs + rt - 0.3): print('ERROR shank path blocked at x=%+.1f y=%+.1f' % (sx_, yy)); ok = False
        for yy in (-(ytip + 0.3), -(yi - 0.3)):
            if inside(sx_, yy, zs) or inside(sx_, yy, zs + rc - 0.3): print('ERROR thread core blocked at x=%+.1f y=%+.1f' % (sx_, yy)); ok = False
            if not inside(sx_, yy, zs + 3.3) and not inside(sx_, yy, zs - 3.3): print('ERROR no thread wall in the inner block'); ok = False
    # Inner blocks: present at the screws over the claw depth, absent in the middle,
    # below the claw top line, lead-in at the bottom
    ib0, ib1 = P(sh, 'len') / 2 - P(sh, 'boss_len'), P(sh, 'len') / 2
    lb_ = P(sh, 'leg_below')
    for sgx in (+1, -1):
        for sgy in (+1, -1):
            xm = sgx * (ib0 + ib1) / 2
            if not inside(xm, sgy * (ytip + 0.3), zs - rt - 1.0): print('ERROR inner block missing at x=%+.1f y=%+.1f' % (xm, sgy * ytip)); ok = False
            if not inside(sgx * (ib1 - 0.5), sgy * (ytip + 0.3), ztip - 0.3): print('ERROR inner block below the claw missing'); ok = False
            if inside(sgx * (ib1 - 0.5), sgy * (ytip + 0.2), ztip + 0.2): print('ERROR inner block rises above the claw line'); ok = False
            if inside(sgx * (ib1 - 0.5), sgy * (ytip + 0.3), -lb_ + 0.2): print('ERROR inner lead-in missing'); ok = False
            if not inside(sgx * (ib1 - 0.5), sgy * (yi - 0.1), -lb_ + 0.3): print('ERROR inner lead-in too deep'); ok = False
            if inside(sgx * (ib0 - 0.5), sgy * (ytip + 0.3), zs): print('ERROR inner block reaches past the outer block'); ok = False
            if inside(sgx * (ib0 + 0.3), sgy * (ytip + 0.2), zs - rt - 1.0): print('ERROR inner block lead-in (x) missing'); ok = False
    if inside(0, ytip + 0.3, zs): print('ERROR inner block in the middle'); ok = False
    ub = P(sh, 'leg_below') - (P(sh, 'screw_below') + rt)
    if ub < P(sh, 'under_wall') - 1e-6: print('ERROR leg below the M6 hole thinner than under_wall'); ok = False
    zb = P(sh, 'z_boss')
    if not inside(P(sh, 'screw_x'), yo + bt / 2, zb - 0.3) or inside(P(sh, 'screw_x'), yo + bt / 2, zb + 0.3): print('ERROR block top not at z_boss'); ok = False
    L = P(sh, 'z_top0') - zroot; d = cd - P(sh, 'fit_w') / 2; lt_ = P(sh, 'leg_t')
    print('Claws %.1f under the beam, end %.2f below its underside with the roof seated, slope %.1f over %.1f; '
          'pushing on: %.2f mm per leg over %.1f mm, strain at the root approx. %.2f %%'
          % (cd, P(sh, 'claw_gap'), cr, cd, d, L, 100 * 3 * lt_ * d / (2 * L * L)))
    print('M6 axis %.2f below the beam (hole top %.2f, %.2f when seated; shank top %.2f seated), leg %.2f below the beam, %.2f under the hole; '
          'blocks from %.2f to %+.2f (%.2f each side of the axis); inner blocks %.2f deep from %.2f up to the claw line (%+.2f), thread length %.2f'
          % (-zs, zs + rt, zs + rt - fz, zs + 3.0 - fz, P(sh, 'leg_below'), ub, -P(sh, 'leg_below'), zb, zb - zs, cd, -P(sh, 'leg_below'), ztip, P(sh, 'y_boss') - ytip))
    print('Beam %.1f x %.1f, rider outside %.1f wide (blocks %.1f), %.1f high; wire hole Ø %.1f at %.0f above the beam, %.1f long'
          % (P(sh, 'beam_w'), bh, 2 * yo, 2 * P(sh, 'y_boss'), P(sh, 'z_hump') + lb, P(sh, 'wire_d') + P(sh, 'wire_fit'), P(sh, 'hole_h'), 2 * yo))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('beam-rider')
    sh = build_sheet(doc); body = build(doc, sh); doc.recompute(); set_visibility(doc, body)
    fcstd = os.path.join(here, 'beam-rider.FCStd'); doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {body.Name, body.Tip.Name, 'X_Axis', 'Y_Axis', 'Z_Axis', 'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'beam-rider.step'))
    body.Shape.exportStl(os.path.join(here, 'beam-rider.stl'))
    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
    print('Valid       %s' % body.Shape.isValid())
    print('Plausible   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None) and 'Invalid' in o.State]
    print('Invalid     %s' % (bad if bad else 'none'))
    open_ = [o.Name for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject') and not getattr(o, 'FullyConstrained', True)]
    print('Sketches    %d, not fully constrained: %s' % (len([o for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject')]), open_ if open_ else 'none'))


if __name__ == '__main__':
    main()
