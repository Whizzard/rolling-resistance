# -*- coding: utf-8 -*-
"""
Saddle for a second block vial (15.1 x 15.1 x 40 mm) on the calibration arm: a U that,
like the balance rider (beam_rider.py), is pushed onto the 15 x 25 rectangular tube
from above -- but WITHOUT screws. The cheeks bear against the tube with preload, at
the bottom a small claw on each side reaches under the tube. The vial lies on top,
guided sideways only by a low rim (rim_h), and is glued in. For printing, without
supports.

Tube (stated 2026-11-24): 15 wide, 25 high. Vial measured 2026-11-23 (as in
level_clip.py). The vial lies with its long axis ALONG the tube and so shows the
inclination of the calibration arm in its longitudinal direction.

POSITION: x runs along the tube, y across, z upwards. The tube lies at
y = +-beam_w/2, z = 0 (underside) to beam_h (top). The saddle is a profile in y-z,
CONSTANT over the length len along x -- therefore it prints LYING ON ITS END FACE
("as a U"): all walls stand vertical, all slopes are profile edges, nothing
overhangs.

Profile (y-z), from top to bottom:
    Rim       two strips rim_h high on the deck, flush with the cheeks outside; the
              vial lies between them with fit_vw clearance. No end walls, no tall
              side walls: the vial stays visible from above and from both sides, the
              rim only aligns it crosswise. It is held with glue on the deck.
    Deck      deck_t thick, rests WITHOUT clearance on the top of the tube -- this is
              the reference surface of the vial. The claws pull it there.
    Cheeks    leg_t thick, from the deck to below the tube, INCLINED INWARDS: at the
              top (at the deck) beam_w + fit_w wide inside, at the bottom at the claw
              only beam_w - preload. When pushed on they are pressed outwards by
              preload/2 per side at the bottom and clamp the tube.
    Claws     one hook each claw_d inwards under the underside of the tube. The top
              side rises from the hook end towards the cheek by claw_rise (about 15
              degrees): the preload pushes the hook inwards, the slope converts this
              into a downward pull and presses the deck onto the tube -- self-
              tensioning, and insensitive to a few tenths in the tube height. The hook
              end sits claw_gap below the underside of the tube, the slope only
              reaches the edge on the way to the cheek. Under the hook a 45 degree
              slope as a lead-in: when pushing on, the top edge of the tube spreads
              the cheeks.

Fitting: push straight on from above (cheeks spring claw_d + preload/2 per side,
with leg_t = 2 and a 25 mm lever about 0.9 % strain at the root -- PETG), or hook one
claw in at an angle and press the other side down. Removing: push one cheek outwards
at its lower edge.

Everything sits in ONE sketch on YZ (one closed polyline, 20 corners) and is bound to
the spreadsheet "Parameter" through expressions. Dimensions in mm.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/level_saddle.py
Produces hardware/level-saddle.FCStd, .step.

Printing: standing on the end face, the U lies on the bed. PETG (springs better than
PLA), 3 walls, 100 % infill does no harm at 7 cm3. With brim, the footprint is only
19 x 33 mm at 40 mm height.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

PARAMS = [
    # Tube (stated 2026-11-24) and vial (measured 2026-11-23)
    ('beam_w',    15.0, 'Tube width (y)'),
    ('beam_h',    25.0, 'Tube height (z)'),
    ('vial_l',    40.0, 'Vial length (x), measured'),
    ('vial_w',    15.1, 'Vial width (y), measured'),
    ('vial_h',    15.1, 'Vial height (z), measured -- for information only, the rim is lower'),
    ('fit_vw',     0.3, 'Play of the vial crosswise, TOTAL'),
    # Saddle
    ('fit_w',      0.3, 'Play crosswise at the deck, TOTAL'),
    ('preload',    0.6, 'Preload: cheeks at the bottom NARROWER than the tube by this much, TOTAL'),
    ('leg_t',      2.0, 'Cheek thickness'),
    ('deck_t',     2.5, 'Deck thickness'),
    ('rim_h',      3.0, 'Rim height beside the vial'),
    ('claw_d',     1.5, 'Claw reaches this far under the tube'),
    ('claw_tip_t', 0.8, 'Height of the claw end face'),
    ('claw_rise',  0.4, 'Rise of the claw top from the end to the cheek (0.4 over 1.5 = 15 degrees)'),
    ('claw_gap',   0.15, 'Claw end below the underside of the tube; the slope carries'),
    # Derived
    ('len',    '=vial_l',                       'Saddle length (x) = vial'),
    ('y_in',   '=(beam_w + fit_w) / 2',         'Cheek inside, at the top at the deck'),
    ('y_out',  '=y_in + leg_t',                 'Cheek outside, top; also deck and rim outside'),
    ('y_cl',   '=(beam_w - preload) / 2',       'Cheek inside, at the bottom at the claw'),
    ('y_clo',  '=y_cl + leg_t',                 'Cheek outside, bottom'),
    ('y_tip',  '=y_cl - claw_d',                'Claw end'),
    ('y_cav',  '=(vial_w + fit_vw) / 2',        'Vial pocket half width = rim inside'),
    ('rim_t',  '=y_out - y_cav',                'Rim thickness, follows'),
    ('z_top0', '=beam_h',                       'Deck bottom = top of the tube'),
    ('z_top1', '=z_top0 + deck_t',              'Deck top = seat of the vial'),
    ('z_rim',  '=z_top1 + rim_h',               'Rim top'),
    ('z_tip',  '=-claw_gap',                    'Claw end top'),
    ('z_root', '=z_tip + claw_rise',            'Claw top side at the cheek'),
    ('z_tipb', '=z_tip - claw_tip_t',           'Claw end bottom'),
    ('z_bot',  '=z_tipb - claw_d',              'Cheek bottom (end of the 45 degree slope)'),
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


def sketch_on(body, plane, name):
    sk = body.newObject('Sketcher::SketchObject', name)
    try: sk.AttachmentSupport = [(plane, '')]
    except Exception: sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    return sk


# Right half of the profile (y >= 0), from the deck at the top clockwise around the
# outside to the underside of the deck. The left half is the mirror image in reverse order.
HALF = [
    ('y_cav', 'z_top1'),   # deck top, at the rim
    ('y_cav', 'z_rim'),    # rim inside top
    ('y_out', 'z_rim'),    # rim outside top
    ('y_out', 'z_top0'),   # cheek outside, bend at the top of the tube
    ('y_clo', 'z_bot'),    # cheek outside bottom
    ('y_cl',  'z_bot'),    # foot of the 45 degree slope
    ('y_tip', 'z_tipb'),   # claw end bottom
    ('y_tip', 'z_tip'),    # claw end top
    ('y_cl',  'z_root'),   # claw top side at the cheek
    ('y_in',  'z_top0'),   # cheek inside at the deck
]


def profile(sh):
    pts, ex = [], []
    for sgn, half in ((+1, HALF), (-1, list(reversed(HALF)))):
        for yn, zn in half:
            pts.append((sgn * P(sh, yn), P(sh, zn)))
            ex.append((('' if sgn > 0 else '-') + 'Parameter.' + yn, 'Parameter.' + zn))
    return pts, ex


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'LevelSaddle')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    yz = planes['YZ_Plane']
    # One profile in the YZ plane (local x = y, local y = z), padded symmetrically along x.
    pts, ex = profile(sh)
    sk = sketch_on(body, yz, 'Sk_Profile')
    poly(sk, pts, 'p', ex)
    f = body.newObject('PartDesign::Pad', 'Saddle'); f.Profile = sk
    f.setExpression('Length', 'Parameter.len'); f.Midplane = True
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
    bw, bh = P(sh, 'beam_w'), P(sh, 'beam_h')
    yi, yo, ycl, ytip, ycav, lt = P(sh, 'y_in'), P(sh, 'y_out'), P(sh, 'y_cl'), P(sh, 'y_tip'), P(sh, 'y_cav'), P(sh, 'leg_t')
    zt0, zt1, zr, ztip, zroot, ztipb, zbot = (P(sh, n) for n in ('z_top0', 'z_top1', 'z_rim', 'z_tip', 'z_root', 'z_tipb', 'z_bot'))
    L2 = P(sh, 'len') / 2
    def both(fn, y, z, msg, want):
        nonlocal ok
        for sgn in (+1, -1):
            if inside(0, sgn * y, z) != want: print('ERROR ' + msg + ' at y=%+.1f z=%.1f' % (sgn * y, z)); ok = False
    # Tube space clear, deck rests directly on the top of the tube
    for z in (0.5, bh / 2, bh - 0.3):
        if inside(0, 0, z): print('ERROR tube space not clear, z=%.1f' % z); ok = False
    both(inside, bw / 2 - 0.5, bh / 2, 'tube space beside the centre not clear', False)
    if not inside(0, 0, zt0 + 0.1): print('ERROR deck does not rest on the tube'); ok = False
    if not inside(0, 0, (zt0 + zt1) / 2): print('ERROR deck missing'); ok = False
    both(inside, yo - 0.3, (zt0 + zt1) / 2, 'deck outside missing', True)
    # Rim: material outside, pocket clear, nothing above the rim
    both(inside, yo - P(sh, 'rim_t') / 2, zt1 + P(sh, 'rim_h') / 2, 'rim missing', True)
    both(inside, ycav - 0.2, zt1 + 0.3, 'rim protrudes into the pocket', False)
    if inside(0, 0, zt1 + 0.3): print('ERROR material in the vial pocket'); ok = False
    both(inside, yo - P(sh, 'rim_t') / 2, zr + 0.3, 'material above the rim', False)
    # Cheeks: material at mid-wall at three heights (the wall is inclined, centre interpolated)
    for z in (bh - 2, bh / 2, 2.0):
        t = (zt0 - z) / (zt0 - zroot)
        ym = (yi + t * (ycl - yi)) + lt / 2
        both(inside, ym, z, 'cheek missing', True)
    # Preload: inside wider at the top, narrower at the bottom than the tube
    if not yi > bw / 2: print('ERROR no clearance at the deck'); ok = False
    if not ycl < bw / 2: print('ERROR no preload'); ok = False
    if not ytip < ycl: print('ERROR claw does not reach under the tube'); ok = False
    # Claws: end face present, clear in front of it, the slope at the top (above the underside
    # of the tube at the cheek, below it at the end), the 45 degree slope at the bottom
    both(inside, ytip + 0.3, (ztip + ztipb) / 2, 'claw end face missing', True)
    both(inside, ytip - 0.3, (ztip + ztipb) / 2, 'claw too long', False)
    both(inside, ytip + 0.2, ztip + 0.2, 'claw end protrudes above the underside of the tube', False)
    if not zroot > 0 > ztip: print('ERROR claw slope does not cross the underside of the tube'); ok = False
    both(inside, ycl - 0.3, zroot - 0.3, 'claw root missing', True)
    both(inside, ycl - 0.3, zroot + 0.3, 'material above the claw root', False)
    both(inside, ycl - 0.5, zbot + 0.2, 'lead-in slope missing', False)
    both(inside, ycl + 0.3, zbot + 0.3, 'cheek foot missing', True)
    both(inside, ycl + 0.3, zbot - 0.3, 'material under the foot', False)
    # Length
    if not inside(L2 - 0.5, 0, (zt0 + zt1) / 2) or inside(L2 + 0.5, 0, (zt0 + zt1) / 2): print('ERROR length'); ok = False
    rt = P(sh, 'rim_t')
    if rt < 1.2: print('ERROR rim only %.2f thick: vial wider than tube + 2 cheeks' % rt); ok = False
    # Spring travel when pushing on, strain at the root of the cheek (cantilever)
    L = zt0 - zroot; d = P(sh, 'claw_d') + P(sh, 'preload') / 2
    eps = 3 * lt * d / (2 * L * L)
    print('Tube %.1f x %.1f; saddle outside %.1f wide, %.1f high, %.0f long; deck %.1f, cheeks %.1f, rim %.2f thick / %.1f high'
          % (bw, bh, 2 * yo, zr - zbot, 2 * L2, P(sh, 'deck_t'), lt, rt, P(sh, 'rim_h')))
    print('Cheeks inside top %.2f, bottom %.2f (tube %.1f): preload %.2f per side; claw %.1f under the tube, slope from %+.2f to %+.2f'
          % (2 * yi, 2 * ycl, bw, P(sh, 'preload') / 2, P(sh, 'claw_d'), ztip, zroot))
    print('Pushing on: %.2f mm spring travel per cheek over a %.1f mm lever, strain at the root approx. %.2f %% (PETG < 2 ok, PLA marginal)' % (d, L, 100 * eps))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('level-saddle')
    sh = build_sheet(doc); body = build(doc, sh); doc.recompute(); set_visibility(doc, body)
    fcstd = os.path.join(here, 'level-saddle.FCStd'); doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {body.Name, body.Tip.Name, 'X_Axis', 'Y_Axis', 'Z_Axis', 'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'level-saddle.step'))
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
