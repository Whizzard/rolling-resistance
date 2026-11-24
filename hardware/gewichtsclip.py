# -*- coding: utf-8 -*-
"""
Clip fuer das 1-kg-Kalibriergewicht: schnappt um den eingeschnuerten Hals, traegt den
Kopf, und hat oben eine zentrale Oese fuer den Haken. Zum Drucken, ohne Stuetze.

Gewicht (gemessen 2026-11-24): Hals knapp 33 mm, Kopf 45 mm, Halsmitte bis Oberkante
Kopf etwa 15 mm.

AUFBAU, von unten nach oben (z = 0 ist die Unterseite des Kragens, Druckbett):

    Kragen    C-foermiger Ring um den Hals, innen neck_d + fit, ring_t dick, coll_h hoch.
              Der Kopf liegt auf seiner Oberseite auf. Die Oeffnung (gap_w) zeigt nach
              +y -- PARALLEL ZUR PENDELACHSE (der Draht liegt in y) -- und ist etwas
              schmaler als der Hals: der Ring federt beim Aufschieben von der Seite auf
              und haelt dann formschluessig.
    Laschen   zwei Stege in +x und -x, verbinden den Kragen mit den Pfosten; sie liegen
              unter dem Rand des Kopfs und tragen mit.
    Pfosten   zwei Pfosten ausserhalb des Kopfs (innen post_x, Luft zum Kopf), arm_w
              breit (x) und arm_t dick (y). Von den Laschen senkrecht bis top_clear
              ueber den Kopf, dann mit 45 Grad nach innen auf Reiterbreite +
              2 * side_clear, dann senkrecht bis ueber den Drehpunkt. Die Drahtlager
              sitzen damit 1 mm neben dem Reiter. Es gibt KEINEN Querverbinder ueber
              dem Kopf: der Kragen haelt die Pfosten unten zusammen, oben tut es der
              Draht; das Gewicht haengt einfach frei unter dem Reiter.
    Drehpunkt das Drahtloch (Ø wire_d + fit) geht in x durch beide Pfosten, also in
              der Pfostenebene: der 2-mm-Draht liegt QUER zum Rohr, durch Pfosten,
              Reiterhoecker, Pfosten (waagereiter.py), aussen beidseitig geklemmt. Das
              Rohr laeuft LAENGS zwischen den Pfosten hindurch; die Pfosten sind so
              hoch, dass die Reiterschenkel (rider_below unter dem Rohr) swing_clear
              ueber dem Kopf bleiben. Beim Pendeln (um x) bewegen sich die Pfosten neben
              dem Reiter, nicht ueber ihm -- die Klemmschrauben des Reiters liegen
              aussen bei +-screw_x laengs des Rohrs und sind nie im Weg.

Alles sitzt in Skizzen auf XY (Kragen, Laschen), XZ (Pfosten) und YZ (Drahtloch) und ist ueber
Ausdruecke an das Spreadsheet "Parameter" gebunden. Masse in mm.

Aufruf:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/gewichtsclip.py
Erzeugt hardware/gewichtsclip.FCStd, .step, .stl.

Druck: stehend auf dem Kragen. Pfosten senkrecht bzw. 45 Grad, das Drahtloch ist
waagerecht und klein -- keine Ueberhaenge ueber 45 Grad. PETG, 3 bis 4 Waende; 1 kg ist fuer das Teil nichts,
der Kragen muss nur die Federung beim Aufschieben vertragen (gap_w).
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

PARAMS = [
    # Gewicht, gemessen 2026-11-24
    ('neck_d',   33.0, 'Hals Durchmesser, gemessen (knapp 33)'),
    ('head_d',   45.0, 'Kopf Durchmesser, gemessen'),
    ('head_h',   15.0, 'Oberkante Kragen bis Oberkante Kopf, grosszuegig (gemessen ab Halsmitte 15)'),
    # Clip
    ('fit',       0.6, 'Spiel im Hals, GESAMT'),
    ('ring_t',    4.0, 'Kragen radial dick'),
    ('coll_h',    8.0, 'Kragen hoch'),
    ('gap_w',    30.0, 'Oeffnung des Kragens (kleiner als der Hals: schnappt)'),
    ('head_clear', 1.5, 'Luft zwischen Kopf und Pfosten'),
    ('top_clear',  3.0, 'Luft ueber dem Kopf bis zum Knick der Pfosten'),
    ('arm_w',     8.0, 'Pfosten breit (x)'),
    ('arm_t',     8.0, 'Pfosten dick (y)'),
    # Reiter (waagereiter.py) und Rohr, fuer die Pfostenhoehe
    ('rider_below', 12.0, 'Reiterschenkel unter der Rohrunterkante'),
    ('beam_h',   25.0, 'Rohr hoch'),
    ('beam_w',   15.0, 'Rohr breit (x), muss zwischen die Pfosten passen'),
    ('rider_w',  18.3, 'Reiter aussen breit, quer zum Rohr (waagereiter: beam_w + fit_w + 2 leg_t)'),
    ('side_clear', 1.0, 'Luft Pfosten zu Reiter, je Seite, oben'),
    ('hole_h',   15.0, 'Drahtloch des Reiters ueber der Rohroberkante'),
    ('swing_clear', 2.0, 'Luft Kopf zu den Reiterschenkeln'),
    ('wire_d',    2.0, 'Draht'),
    ('wire_fit',  0.3, 'Spiel im Drahtloch'),
    # Abgeleitet
    ('r_in',   '=(neck_d + fit) / 2',          'Kragen innen'),
    ('r_out',  '=r_in + ring_t',               'Kragen aussen'),
    ('post_x', '=head_d / 2 + head_clear',     'Pfosten innen (x)'),
    ('post_xo', '=post_x + arm_w',             'Pfosten aussen (x)'),
    ('z_head', '=coll_h + head_h',             'Oberkante Kopf'),
    ('z_v',    '=z_head + top_clear',          'Knick innen: Ende der senkrechten Innenwand'),
    ('z_vo',   '=z_v + arm_w * (sqrt(2) - 1)', 'Knick aussen, so dass der Schraegteil arm_w dick bleibt'),
    ('x_top',  '=rider_w / 2 + side_clear',    'Pfosten innen, oben'),
    ('dx_bend', '=post_x - x_top',             'Einzug der Pfosten, 45 Grad -> auch die Hoehe'),
    ('z_bend1', '=z_v + dx_bend',              'Ende des Knicks innen'),
    ('z_bend1o', '=z_vo + dx_bend',            'Ende des Knicks aussen'),
    ('z_pivot', '=z_head + swing_clear + rider_below + beam_h + hole_h', 'Drahtloch Mitte'),
    ('z_top',  '=z_pivot + 8',                 'Pfosten oben'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter'); sh.set('B1', 'Wert'); sh.set('C1', 'Bedeutung')
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

    # 1 Kragen: Ring
    sk = sketch_on(body, xy, 'Sk_Kragen')
    circle(sk, 0, 0, r_out, 'ko', '0', '0', 'Parameter.r_out')
    circle(sk, 0, 0, r_in, 'ki', '0', '0', 'Parameter.r_in')
    pad(sk, 'Kragen', 'Parameter.coll_h')

    # 2 Laschen nach +x und -x bis unter die Pfosten (1 mm in den Ring hinein)
    sk = sketch_on(body, xy, 'Sk_Laschen')
    rect(sk, r_out - 1, -arm_t / 2, pxo - r_out + 1, arm_t, 'lp',
         'Parameter.r_out - 1', '-Parameter.arm_t / 2', 'Parameter.post_xo - Parameter.r_out + 1', 'Parameter.arm_t')
    rect(sk, -pxo, -arm_t / 2, pxo - r_out + 1, arm_t, 'lm',
         '-Parameter.post_xo', '-Parameter.arm_t / 2', 'Parameter.post_xo - Parameter.r_out + 1', 'Parameter.arm_t')
    pad(sk, 'Laschen', 'Parameter.coll_h')

    # 3 Oeffnung des Kragens nach +y
    sk = sketch_on(body, xy, 'Sk_Oeffnung')
    rect(sk, -P(sh, 'gap_w') / 2, 0, P(sh, 'gap_w'), r_out + 2, 'g',
         '-Parameter.gap_w / 2', '0', 'Parameter.gap_w', 'Parameter.r_out + 2')
    pocket(sk, 'Oeffnung')

    # 4 Pfosten: zwei Rechtecke in XZ, von den Laschen (1 mm Ueberlappung) bis z_top.
    z0 = coll_h - 1
    E = 'Parameter.'
    z_top, zp = P(sh, 'z_top'), P(sh, 'z_pivot')
    #   Jeder Pfosten: senkrecht bis z_v (innen) / z_vo (aussen), 45 Grad nach innen um
    #   dx_bend, senkrecht bis z_top. Der Schraegteil bleibt arm_w dick (z_vo).
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

    # 5 Drahtloch, Achse x, durch beide Pfosten: Skizze auf YZ (lokal x = y, lokal y = z)
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
    if inside(0, 0, zm): print('FEHLER Hals nicht frei'); ok = False
    if not inside(r_in + 1, 0, zm) or not inside(0, -(r_in + 1), zm): print('FEHLER Kragen fehlt'); ok = False
    if inside(0, r_in + 1, zm): print('FEHLER Oeffnung fehlt'); ok = False
    if not inside((r_out + px) / 2, 0, zm): print('FEHLER Lasche fehlt'); ok = False
    xm = px + arm_w / 2
    xt = P(sh, 'x_top') + arm_w / 2
    for z in (ch + 1, z_v - 1):
        if not inside(xm, 0, z) or not inside(-xm, 0, z): print('FEHLER Pfosten unten fehlt bei z=%.1f' % z); ok = False
    zb1 = P(sh, 'z_bend1')
    for z in (P(sh, 'z_bend1o') + 1, zp - 10, z_top - 1):
        if not inside(xt, 0, z) or not inside(-xt, 0, z): print('FEHLER Pfosten oben fehlt bei z=%.1f' % z); ok = False
        if inside(xm, 0, z): print('FEHLER Pfosten oben nicht eingezogen bei z=%.1f' % z); ok = False
    # Schraegteil: Mitte des Knicks, senkrecht zur Schraege arm_w dick
    zmid = (z_v + zb1) / 2; xmid = (px + P(sh, 'x_top')) / 2
    d = arm_w / math.sqrt(2)   # Aussenlinie: Versatz (d, d) von der Innenlinie, senkrecht dazu arm_w
    if not inside(xmid + d - 0.3, 0, zmid + d - 0.3) or inside(xmid + d + 0.3, 0, zmid + d + 0.3): print('FEHLER Schraegteil nicht arm_w dick'); ok = False
    if inside(xmid - 0.5, 0, zmid - 0.5): print('FEHLER Schraegteil innen zu dick'); ok = False
    hd, hh = P(sh, 'head_d') / 2, P(sh, 'head_h')
    for ang in (0, 45, 90, 135, 180):
        x, y = (hd - 0.2) * math.cos(math.radians(ang)), (hd - 0.2) * math.sin(math.radians(ang))
        for z in (ch + 0.5, ch + hh - 0.5):
            if inside(x, y, z): print('FEHLER Kopf stoesst an bei %d Grad, z=%.1f' % (ang, z)); ok = False
    for z in (ch + hh + 0.5, z_top): 
        if inside(0, 0, z): print('FEHLER Material ueber dem Kopf bei z=%.1f' % z); ok = False
    # Drahtloch durch beide Pfosten, Material daneben
    if inside(xt, 0, zp) or inside(-xt, 0, zp): print('FEHLER Drahtloch fehlt'); ok = False
    if not inside(xt, 0, zp + 3) or not inside(xt, 0, zp - 3): print('FEHLER Pfosten um das Loch fehlt'); ok = False
    if P(sh, 'x_top') < P(sh, 'beam_w') / 2 + 1: print('FEHLER Rohr passt oben nicht zwischen die Pfosten'); ok = False
    # Rohr und Reiter zwischen den Pfosten, ueber dem Kopf
    bh, hole_h, rb, bw = P(sh, 'beam_h'), P(sh, 'hole_h'), P(sh, 'rider_below'), P(sh, 'beam_w')
    z_beam_top = zp - hole_h; z_legs = z_beam_top - bh - rb
    if z_legs < z_head + P(sh, 'swing_clear') - 1e-6: print('FEHLER Reiterschenkel beruehren den Kopf'); ok = False
    rw2 = P(sh, 'rider_w') / 2
    for z in (z_legs + 0.5, zp):
        if inside(rw2 + 0.5, 0, z) or inside(-rw2 - 0.5, 0, z): print('FEHLER Reiter passt nicht zwischen die Pfosten bei z=%.1f' % z); ok = False
    for z in (z_legs + 0.5, z_beam_top - 0.5):
        if inside(bw / 2, 0, z) or inside(0, 0, z): print('FEHLER Rohrraum nicht frei bei z=%.1f' % z); ok = False
    print('Kragen innen Ø %.1f, Oeffnung %.1f (Hals %.1f): Schnappweg %.1f mm' % (2 * r_in, P(sh, 'gap_w'), P(sh, 'neck_d'), P(sh, 'neck_d') - P(sh, 'gap_w')))
    print('Pfosten innen unten %.1f (Kopf r %.1f), oben %.1f (Reiter %.1f + 2 x %.1f), %.0f x %.0f mm, bis %.1f; Knick von z %.1f bis %.1f'
          % (px, hd, P(sh, 'x_top'), P(sh, 'rider_w'), P(sh, 'side_clear'), arm_w, P(sh, 'arm_t'), z_top, z_v, zb1))
    print('Drehpunkt %.1f ueber Kragen, Rohrunterkante %.1f, Reiterschenkel %.1f, Kopf oben %.1f' % (zp, z_beam_top - bh, z_legs, z_head))
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
    print('Aussenmass  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.0f mm3' % body.Shape.Volume)
    print('Gueltig     %s' % body.Shape.isValid())
    print('Plausibel   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None) and 'Invalid' in o.State]
    print('Fehlerhaft  %s' % (bad if bad else 'keine'))
    offen = [o.Name for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject') and not getattr(o, 'FullyConstrained', True)]
    print('Skizzen     %d, davon nicht voll bestimmt: %s' % (len([o for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject')]), offen if offen else 'keine'))


if __name__ == '__main__':
    main()
