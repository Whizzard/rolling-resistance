# -*- coding: utf-8 -*-
"""
Clip for the 1 kg calibration weight: snaps around the necked-down neck, carries the
head, and has a central eye at the top for the hook. For printing, without supports.

Weight (measured 2026-11-24): neck just under 33 mm, head 45 mm, neck centre to top
of head about 15 mm.

STRUCTURE, from bottom to top (z = 0 is the underside of the collar, print bed):

    Collar    C-shaped ring around the neck, inside neck_d + fit, ring_t thick, coll_h
              high. The head rests on its top side. The opening (gap_w) points to +y --
              PARALLEL TO THE PENDULUM AXIS (the wire lies in y) -- and is somewhat
              narrower than the neck: the ring springs open when pushed on from the
              side and then holds positively.
    Webs      two webs in +x and -x, connect the collar to the posts; they lie under
              the rim of the head and help carry it.
    Posts     two posts outside the head (inside post_x, clearance to the head), arm_w
              wide (x) and arm_t thick (y). From the webs vertically up to top_clear
              above the head, then at 45 degrees inwards to rider width +
              2 * side_clear, then vertically to above the pivot. The wire bearings
              thus sit 1 mm beside the rider. There is NO cross member above the
              head: the collar holds the posts together at the bottom, the wire does
              so at the top; the weight simply hangs freely under the rider.
    Pivot     the wire hole (Ø wire_d + fit) runs in x through both posts, i.e. in
              the plane of the posts: the 2 mm wire lies ACROSS the tube, through post,
              rider hump, post (waagereiter.py), clamped outside on both sides. The
              tube runs LENGTHWISE between the posts; the posts are tall enough that
              the rider legs (rider_below below the tube) stay swing_clear above the
              head. When swinging (about x) the posts move beside the rider, not above
              it -- the rider's clamping screws sit outside at +-screw_x along the
              tube and are never in the way.

Everything sits in sketches on XY (collar, webs), XZ (posts) and YZ (wire hole) and is
bound to the spreadsheet "Parameter" through expressions. Dimensions in mm.

Invocation:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/gewichtsclip.py
Produces hardware/gewichtsclip.FCStd, .step, .stl.

Printing: standing on the collar. Posts vertical or at 45 degrees, the wire hole is
horizontal and small -- no overhangs beyond 45 degrees. PETG, 3 to 4 walls; 1 kg is
nothing for this part, the collar only has to tolerate the springing when pushed on (gap_w).
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

PARAMS = [
    # Weight, measured 2026-11-24
    ('neck_d',   33.0, 'Neck diameter, measured (just under 33)'),
    ('head_d',   45.0, 'Head diameter, measured'),
    ('head_h',   15.0, 'Top of collar to top of head, generous (measured from neck centre 15)'),
    # Clip
    ('fit',       0.6, 'Play at the neck, TOTAL'),
    ('ring_t',    4.0, 'Collar radial thickness'),
    ('coll_h',    8.0, 'Collar height'),
    ('gap_w',    30.0, 'Opening of the collar (smaller than the neck: snaps)'),
    ('head_clear', 1.5, 'Clearance between head and posts'),
    ('top_clear',  3.0, 'Clearance above the head up to the bend of the posts'),
    ('arm_w',     8.0, 'Post width (x)'),
    ('arm_t',     8.0, 'Post thickness (y)'),
    # Rider (waagereiter.py) and tube, for the post height
    ('rider_below', 12.0, 'Rider legs below the underside of the tube'),
    ('beam_h',   25.0, 'Tube height'),
    ('beam_w',   15.0, 'Tube width (x), must fit between the posts'),
    ('rider_w',  18.3, 'Rider outer width, across the tube (waagereiter: beam_w + fit_w + 2 leg_t)'),
    ('side_clear', 1.0, 'Clearance post to rider, per side, at the top'),
    ('hole_h',   15.0, 'Wire hole of the rider above the top of the tube'),
    ('swing_clear', 2.0, 'Clearance head to the rider legs'),
    ('wire_d',    2.0, 'Wire'),
    ('wire_fit',  0.3, 'Play in the wire hole'),
    # Derived
    ('r_in',   '=(neck_d + fit) / 2',          'Collar inside'),
    ('r_out',  '=r_in + ring_t',               'Collar outside'),
    ('post_x', '=head_d / 2 + head_clear',     'Posts inside (x)'),
    ('post_xo', '=post_x + arm_w',             'Posts outside (x)'),
    ('z_head', '=coll_h + head_h',             'Top of head'),
    ('z_v',    '=z_head + top_clear',          'Bend inside: end of the vertical inner wall'),
    ('z_vo',   '=z_v + arm_w * (sqrt(2) - 1)', 'Bend outside, so that the sloped part stays arm_w thick'),
    ('x_top',  '=rider_w / 2 + side_clear',    'Posts inside, top'),
    ('dx_bend', '=post_x - x_top',             'Inset of the posts, 45 degrees -> also the height'),
    ('z_bend1', '=z_v + dx_bend',              'End of the bend inside'),
    ('z_bend1o', '=z_vo + dx_bend',            'End of the bend outside'),
    ('z_pivot', '=z_head + swing_clear + rider_below + beam_h + hole_h', 'Wire hole centre'),
    ('z_top',  '=z_pivot + 8',                 'Posts top'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter'); sh.set('B1', 'Value'); sh.set('C1', 'Meaning')
    for i, (name, val, note) in enumerate(PARAMS):
        r = i + 2
        sh.set('A%d' % r, name); sh.setAlias('B%d' % r, name); sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(PARAMS):
        sh.set('B%d' % (i + 2), str(val))
    doc.recompute()
    return sh


def P(sh, name):
    return float(sh.get(name))


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
    sk.setExpression('Constraints.%s_r' % tag, er)
    sk.setExpression('Constraints.%s_x' % tag, ex)
    sk.setExpression('Constraints.%s_y' % tag, ey)
    return g


def sketch_on(body, plane, name):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Gewichtsclip')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, xz = planes['XY_Plane'], planes['XZ_Plane']

    def pad(sk, name, expr_len, midplane=False):
        f = body.newObject('PartDesign::Pad', name); f.Profile = sk
        f.setExpression('Length', expr_len); f.Midplane = midplane; doc.recompute(); return f

    def pocket(sk, name, expr_len=None):
        f = body.newObject('PartDesign::Pocket', name); f.Profile = sk
        if expr_len: f.setExpression('Length', expr_len)
        else: f.Type = 'ThroughAll'
        f.Midplane = True; doc.recompute(); return f
    yz = planes['YZ_Plane']

    r_in, r_out, coll_h = P(sh, 'r_in'), P(sh, 'r_out'), P(sh, 'coll_h')
    px, pxo, arm_t, arm_w = P(sh, 'post_x'), P(sh, 'post_xo'), P(sh, 'arm_t'), P(sh, 'arm_w')
    z_v = P(sh, 'z_v')

    # 1 Collar: ring
    sk = sketch_on(body, xy, 'Sk_Kragen')
    circle(sk, 0, 0, r_out, 'ko', '0', '0', 'Parameter.r_out')
    circle(sk, 0, 0, r_in, 'ki', '0', '0', 'Parameter.r_in')
    pad(sk, 'Kragen', 'Parameter.coll_h')

    # 2 Webs to +x and -x up to under the posts (1 mm into the ring)
    sk = sketch_on(body, xy, 'Sk_Laschen')
    rect(sk, r_out - 1, -arm_t / 2, pxo - r_out + 1, arm_t, 'lp',
         'Parameter.r_out - 1', '-Parameter.arm_t / 2', 'Parameter.post_xo - Parameter.r_out + 1', 'Parameter.arm_t')
    rect(sk, -pxo, -arm_t / 2, pxo - r_out + 1, arm_t, 'lm',
         '-Parameter.post_xo', '-Parameter.arm_t / 2', 'Parameter.post_xo - Parameter.r_out + 1', 'Parameter.arm_t')
    pad(sk, 'Laschen', 'Parameter.coll_h')

    # 3 Opening of the collar towards +y
    sk = sketch_on(body, xy, 'Sk_Oeffnung')
    rect(sk, -P(sh, 'gap_w') / 2, 0, P(sh, 'gap_w'), r_out + 2, 'g',
         '-Parameter.gap_w / 2', '0', 'Parameter.gap_w', 'Parameter.r_out + 2')
    pocket(sk, 'Oeffnung')

    # 4 Posts: two outlines in XZ, from the webs (1 mm overlap) up to z_top.
    z0 = coll_h - 1
    E = 'Parameter.'
    z_top, zp = P(sh, 'z_top'), P(sh, 'z_pivot')
    #   Each post: vertical up to z_v (inside) / z_vo (outside), 45 degrees inwards by
    #   dx_bend, vertical up to z_top. The sloped part stays arm_w thick (z_vo).
    xt, zb1, zb1o, zvo = P(sh, 'x_top'), P(sh, 'z_bend1'), P(sh, 'z_bend1o'), P(sh, 'z_vo')
    sk = sketch_on(body, xz, 'Sk_Pfosten')
    for sg, tag in ((+1, 'pp'), (-1, 'pm')):
        S = '' if sg > 0 else '-'
        pts = [(sg * px, z0), (sg * pxo, z0), (sg * pxo, zvo), (sg * (xt + arm_w), zb1o), (sg * (xt + arm_w), z_top),
               (sg * xt, z_top), (sg * xt, zb1), (sg * px, z_v)]
        ex = [(S + E + 'post_x', E + 'coll_h - 1'), (S + E + 'post_xo', E + 'coll_h - 1'), (S + E + 'post_xo', E + 'z_vo'),
              (S + '(' + E + 'x_top + ' + E + 'arm_w)', E + 'z_bend1o'), (S + '(' + E + 'x_top + ' + E + 'arm_w)', E + 'z_top'),
              (S + E + 'x_top', E + 'z_top'), (S + E + 'x_top', E + 'z_bend1'), (S + E + 'post_x', E + 'z_v')]
        poly(sk, pts, tag, ex)
    pad(sk, 'Pfosten', 'Parameter.arm_t', midplane=True)

    # 5 Wire hole, axis x, through both posts: sketch on YZ (local x = y, local y = z)
    sk = sketch_on(body, yz, 'Sk_Draht')
    circle(sk, 0, zp, (P(sh, 'wire_d') + P(sh, 'wire_fit')) / 2, 'w', '0', E + 'z_pivot', '(' + E + 'wire_d + ' + E + 'wire_fit) / 2')
    pocket(sk, 'Drahtloch')
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
        names = z.namelist(); doc_xml = z.read('Document.xml').decode('utf-8')
        data = {n: z.read(n) for n in names}
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
    import math
    s = body.Shape; ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    r_in, r_out, ch = P(sh, 'r_in'), P(sh, 'r_out'), P(sh, 'coll_h')
    px, pxo, z_v, arm_w = P(sh, 'post_x'), P(sh, 'post_xo'), P(sh, 'z_v'), P(sh, 'arm_w')
    zp, z_top, z_head = P(sh, 'z_pivot'), P(sh, 'z_top'), P(sh, 'z_head')
    zm = ch / 2
    if inside(0, 0, zm): print('ERROR neck not clear'); ok = False
    if not inside(r_in + 1, 0, zm) or not inside(0, -(r_in + 1), zm): print('ERROR collar missing'); ok = False
    if inside(0, r_in + 1, zm): print('ERROR opening missing'); ok = False
    if not inside((r_out + px) / 2, 0, zm): print('ERROR web missing'); ok = False
    xm = px + arm_w / 2
    xt = P(sh, 'x_top') + arm_w / 2
    for z in (ch + 1, z_v - 1):
        if not inside(xm, 0, z) or not inside(-xm, 0, z): print('ERROR post missing at the bottom at z=%.1f' % z); ok = False
    zb1 = P(sh, 'z_bend1')
    for z in (P(sh, 'z_bend1o') + 1, zp - 10, z_top - 1):
        if not inside(xt, 0, z) or not inside(-xt, 0, z): print('ERROR post missing at the top at z=%.1f' % z); ok = False
        if inside(xm, 0, z): print('ERROR post not inset at the top at z=%.1f' % z); ok = False
    # Sloped part: middle of the bend, arm_w thick perpendicular to the slope
    zmid = (z_v + zb1) / 2; xmid = (px + P(sh, 'x_top')) / 2
    d = arm_w / math.sqrt(2)   # outer line: offset (d, d) from the inner line, arm_w perpendicular to it
    if not inside(xmid + d - 0.3, 0, zmid + d - 0.3) or inside(xmid + d + 0.3, 0, zmid + d + 0.3): print('ERROR sloped part not arm_w thick'); ok = False
    if inside(xmid - 0.5, 0, zmid - 0.5): print('ERROR sloped part too thick inside'); ok = False
    hd, hh = P(sh, 'head_d') / 2, P(sh, 'head_h')
    for ang in (0, 45, 90, 135, 180):
        x, y = (hd - 0.2) * math.cos(math.radians(ang)), (hd - 0.2) * math.sin(math.radians(ang))
        for z in (ch + 0.5, ch + hh - 0.5):
            if inside(x, y, z): print('ERROR head collides at %d degrees, z=%.1f' % (ang, z)); ok = False
    for z in (ch + hh + 0.5, z_top): 
        if inside(0, 0, z): print('ERROR material above the head at z=%.1f' % z); ok = False
    # Wire hole through both posts, material beside it
    if inside(xt, 0, zp) or inside(-xt, 0, zp): print('ERROR wire hole missing'); ok = False
    if not inside(xt, 0, zp + 3) or not inside(xt, 0, zp - 3): print('ERROR post around the hole missing'); ok = False
    if P(sh, 'x_top') < P(sh, 'beam_w') / 2 + 1: print('ERROR tube does not fit between the posts at the top'); ok = False
    # Tube and rider between the posts, above the head
    bh, hole_h, rb, bw = P(sh, 'beam_h'), P(sh, 'hole_h'), P(sh, 'rider_below'), P(sh, 'beam_w')
    z_beam_top = zp - hole_h; z_legs = z_beam_top - bh - rb
    if z_legs < z_head + P(sh, 'swing_clear') - 1e-6: print('ERROR rider legs touch the head'); ok = False
    rw2 = P(sh, 'rider_w') / 2
    for z in (z_legs + 0.5, zp):
        if inside(rw2 + 0.5, 0, z) or inside(-rw2 - 0.5, 0, z): print('ERROR rider does not fit between the posts at z=%.1f' % z); ok = False
    for z in (z_legs + 0.5, z_beam_top - 0.5):
        if inside(bw / 2, 0, z) or inside(0, 0, z): print('ERROR tube space not clear at z=%.1f' % z); ok = False
    print('Collar inside Ø %.1f, opening %.1f (neck %.1f): snap travel %.1f mm' % (2 * r_in, P(sh, 'gap_w'), P(sh, 'neck_d'), P(sh, 'neck_d') - P(sh, 'gap_w')))
    print('Posts inside bottom %.1f (head r %.1f), top %.1f (rider %.1f + 2 x %.1f), %.0f x %.0f mm, up to %.1f; bend from z %.1f to %.1f'
          % (px, hd, P(sh, 'x_top'), P(sh, 'rider_w'), P(sh, 'side_clear'), arm_w, P(sh, 'arm_t'), z_top, z_v, zb1))
    print('Pivot %.1f above collar, tube underside %.1f, rider legs %.1f, top of head %.1f' % (zp, z_beam_top - bh, z_legs, z_head))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('gewichtsclip')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)
    fcstd = os.path.join(here, 'gewichtsclip.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {body.Name, body.Tip.Name, 'X_Axis', 'Y_Axis', 'Z_Axis',
                                 'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'gewichtsclip.step'))
    body.Shape.exportStl(os.path.join(here, 'gewichtsclip.stl'))
    bb = body.Shape.BoundBox
    print('Outer size  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volume      %.0f mm3' % body.Shape.Volume)
    print('Valid       %s' % body.Shape.isValid())
    print('Plausible   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None) and 'Invalid' in o.State]
    print('Invalid     %s' % (bad if bad else 'none'))
    offen = [o.Name for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject') and not getattr(o, 'FullyConstrained', True)]
    print('Sketches    %d, not fully constrained: %s' % (len([o for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject')]), offen if offen else 'none'))


if __name__ == '__main__':
    main()
