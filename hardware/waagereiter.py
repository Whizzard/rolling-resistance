# -*- coding: utf-8 -*-
"""
Reiter fuer den Waagebalken (Kalibrierarm): schiebbar, mit einer M6 quer unter dem
Balken geklemmt, oben eine Fahne mit dem Loch fuer den 2-mm-Drahthaken, an dem das
Kalibriergewicht haengt. Zum Drucken, ohne Stuetze.

Balken (Angabe 2026-11-24): 15 mm breit, 25 mm hoch. Loch fuer den Draht im Zentrum,
15 mm ueber der Balkenoberkante. Zwei Klemmschrauben, je eine an jedem Ende, AUSSERHALB
des Bereichs, den die Arme des Gewichtsclips beim Pendeln ueberstreichen (bei 15 Grad
und 40 mm unter dem Drehpunkt +-11 mm um die Mitte, dazu der Schraubenkopf).

LAGE: x laeuft entlang des Balkens, y quer, z nach oben. Der Balken liegt bei
y = +-beam_w/2, z = 0 (Unterkante) bis beam_h (Oberkante). Der Reiter ist ein U-Profil,
ueber die Laenge len entlang x konstant -- deshalb druckt er LIEGEND AUF DER STIRNSEITE
("als U"): alle Waende stehen senkrecht; Drahtloch und M6-Loecher (beide Achse y,
quer zum Rohr) sind kurze waagerechte Bruecken, das Drahtloch wird nachgebohrt.

Profil (y-z), von unten nach oben:
    Schenkel     zwei, leg_t = 1,5 dick, von z = -leg_below (unter dem Balken) bis zur
                 Decke; an jeder Schraube aussen ein Klotz boss_t dick (Gewinde und
                 Kopfauflage), vom Reiterende boss_len nach innen, innen mit
                 45-Grad-Anlauf (am +x-Ende ist das die Druckunterseite)
    Decke        top_t dick ueber dem Balken, mit fit_z Luft
    Hoecker      NUR an der Querbohrung, ueber die GANZE Breite des Reiters, symmetrisch:
                 oben hump_w lang (x), bis hump_over ueber die Lochmitte, beidseitig
                 45-Grad-Flanken zur Decke (die -x-Flanke ist beim liegenden Druck die
                 Unterseite und braucht keine Stuetze)
Loecher:
    Draht        Ø wire_d + fit, Achse y (QUER zum Rohr, parallel zur Klemmschraube),
                 mittig in x, hole_h ueber der Balkenoberkante, durch die ganze Breite:
                 der Draht laeuft im Hoecker ueber 2 * y_out, nicht nur ueber eine
                 schmale Fahne. Er ist der Drehpunkt fuer den Gewichtsclip
                 (gewichtsclip.py); die Lochmitte ist der Angriffspunkt in x.
    M6 quer      zwei, Achse y, bei x = +-screw_x, screw_below unter dem Balken: vorne
                 (+y) Durchgang Ø 6,4, hinten (-y) ein MODELLIERTES M6-Gewinde (PartDesign-
                 Bohrung, ISO-Profil, wie in esp_spange.py). Anziehen zieht die Schenkel
                 unten zusammen und klemmt den Balken. Mit 0,1 bis 0,15 mm Schichten
                 kommt das Gewinde brauchbar heraus; sonst einmal den Gewindebohrer
                 durchdrehen, der findet die Gaenge.

Aufruf:  /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/waagereiter.py
Erzeugt hardware/waagereiter.FCStd, .step, .stl.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

PARAMS = [
    ('beam_w',    15.0, 'Balken breit (y)'),
    ('beam_h',    25.0, 'Balken hoch (z)'),
    ('fit_w',      0.3, 'Spiel quer, GESAMT'),
    ('fit_z',      0.2, 'Luft zwischen Balkenoberkante und Decke'),
    ('len',       56.0, 'Reiter lang (x); Schrauben aussen, Arme des Clips in der Mitte'),
    ('screw_x',   22.0, 'Klemmschrauben bei +-x'),
    ('leg_t',      1.5, 'Schenkel dick'),
    ('boss_t',     2.5, 'Klotz an jeder Schraube, aussen auf dem Schenkel'),
    ('boss_len',  13.0, 'Klotz lang (x), ab Reiterende nach innen'),
    ('leg_below', 12.0, 'Schenkel unter die Balkenunterkante'),
    ('top_t',      4.0, 'Decke dick'),
    ('hump_w',    12.0, 'Hoecker oben lang (x), mittig; unten + 2 * Hoehe'),
    ('hump_over',  5.0, 'Hoecker ueber der Lochmitte'),
    ('hole_h',    15.0, 'Drahtloch ueber der Balkenoberkante, VORGABE'),
    ('wire_d',     2.0, 'Draht'),
    ('wire_fit',   0.3, 'Spiel im Drahtloch'),
    ('screw_below', 6.5, 'M6-Achse unter der Balkenunterkante'),
    ('m6_thru',    6.4, 'Durchgang vorne (+y)'),
    ('m6_thread', 'M6', 'Gewinde hinten (-y), modelliert'),
    ('m6_core',    5.0, 'Kerndurchmesser M6, nur fuer die Pruefung'),
    # Abgeleitet
    ('y_in',  '=(beam_w + fit_w) / 2',      'Schenkel innen'),
    ('y_out', '=y_in + leg_t',              'Schenkel aussen'),
    ('z_top0', '=beam_h + fit_z',           'Decke unten'),
    ('z_top1', '=z_top0 + top_t',           'Decke oben'),
    ('z_hole', '=beam_h + hole_h',          'Drahtloch Mitte'),
    ('z_hump', '=z_hole + hump_over',       'Hoecker oben'),
    ('y_boss', '=y_out + boss_t',           'Klotz aussen'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter'); sh.set('B1', 'Wert'); sh.set('C1', 'Bedeutung')
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
    body = doc.addObject('PartDesign::Body', 'Waagereiter')
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

    # 1 U-Profil in der YZ-Ebene (lokal x = y, lokal y = z), entlang x gepadded.
    pts = [(-yo, -lb), (yo, -lb), (yo, zt1), (-yo, zt1)]
    sk = sketch_on(body, yz, 'Sk_Aussen')
    poly(sk, pts, 'a', [('-Parameter.y_out', '-Parameter.leg_below'), ('Parameter.y_out', '-Parameter.leg_below'),
                        ('Parameter.y_out', 'Parameter.z_top1'), ('-Parameter.y_out', 'Parameter.z_top1')])
    pad(sk, 'Block', 'Parameter.len')
    # Balkenraum herausnehmen: von unten bis unter die Decke
    sk = sketch_on(body, yz, 'Sk_Balkenraum')
    rect(sk, -yi, -lb - 1, 2 * yi, lb + 1 + zt0, 'r',
         '-Parameter.y_in', '-Parameter.leg_below - 1', '2 * Parameter.y_in', 'Parameter.leg_below + 1 + Parameter.z_top0')
    pocket(sk, 'Balkenraum', midplane=True)

    # 2 Hoecker: Profil in XZ ueber die ganze Breite (Pad 2*y_out midplane): Trapez,
    #   oben x = +-hump_w/2 bei z_hump, beidseitig 45 Grad bis auf die Decke (0.5 Ueberlappung).
    hw, zhu = P(sh, 'hump_w') / 2, P(sh, 'z_hump')
    hh = zhu - zt1
    sk = sketch_on(body, xz, 'Sk_Hoecker')
    HH = '(Parameter.z_hump - Parameter.z_top1)'
    poly(sk, [(-hw - hh, zt1 - 0.5), (hw + hh, zt1 - 0.5), (hw, zhu), (-hw, zhu)], 'h',
         [('-Parameter.hump_w / 2 - ' + HH, 'Parameter.z_top1 - 0.5'),
          ('Parameter.hump_w / 2 + ' + HH, 'Parameter.z_top1 - 0.5'), ('Parameter.hump_w / 2', 'Parameter.z_hump'),
          ('-Parameter.hump_w / 2', 'Parameter.z_hump')])
    pad(sk, 'Hoecker', '2 * Parameter.y_out', midplane=True)

    # 2a Kloetze an den Schrauben: Trapeze in XY, innen (zur Reitermitte) 45-Grad-Anlauf,
    #    aussen auf beiden Schenkeln, von z = -leg_below bis z = -1 (Pad nach -z).
    sx, L2, bl, bt = P(sh, 'screw_x'), P(sh, 'len') / 2, P(sh, 'boss_len'), P(sh, 'boss_t')
    xy = planes['XY_Plane']
    sk = sketch_on(body, xy, 'Sk_Kloetze')
    for side, tag in ((+1, 'kp'), (-1, 'km')):
        # x_e = Reiterende, x_i = Innenkante; der Anlauf liegt an x_i
        x_e, x_i = side * L2, side * (L2 - bl)
        ex_e = ('' if side > 0 else '-') + 'Parameter.len / 2'
        ex_i = ex_e + (' - ' if side > 0 else ' + ') + 'Parameter.boss_len'
        ex_ib = ex_i + (' + ' if side > 0 else ' - ') + 'Parameter.boss_t'
        for ysign, t2 in ((+1, 'a'), (-1, 'b')):
            ys = ('' if ysign > 0 else '-') + 'Parameter.y_out' + (' - 0.5' if ysign > 0 else ' + 0.5')
            yb = ('' if ysign > 0 else '-') + 'Parameter.y_boss'
            poly(sk, [(x_i, ysign * (yo - 0.5)), (x_e, ysign * (yo - 0.5)), (x_e, ysign * (yo + bt)), (x_i + side * bt, ysign * (yo + bt))], tag + t2,
                 [(ex_i, ys), (ex_e, ys), (ex_e, yb), (ex_ib, yb)])
    f = body.newObject('PartDesign::Pad', 'Kloetze'); f.Profile = sk
    f.setExpression('Length', 'Parameter.leg_below - 1'); f.Reversed = True; doc.recompute()
    if not body.Shape.isInside(Vector(sx, yo + bt / 2, -P(sh, 'leg_below') / 2), 1e-6, True):
        f.Reversed = False; doc.recompute()
    assert body.Shape.isInside(Vector(sx, yo + bt / 2, -P(sh, 'leg_below') / 2), 1e-6, True), 'Kloetze gehen in die falsche Richtung'

    # 3 Drahtloch, Achse y (quer), mittig in x, durch die Fahne
    sk = sketch_on(body, xz, 'Sk_Draht')
    circle(sk, 0, zh, (P(sh, 'wire_d') + P(sh, 'wire_fit')) / 2, 'w', '0', 'Parameter.z_hole', '(Parameter.wire_d + Parameter.wire_fit) / 2')
    pocket(sk, 'Drahtloch', midplane=True)

    # 4 M6 quer (Achse y) unter dem Balken: Kernloch durch beide Schenkel, Durchgang
    #   nur vorne (+y). Skizze auf XZ (lokal x = x, lokal y = z), Pocket von y = 0 aus.
    zs = -P(sh, 'screw_below')
    sx = P(sh, 'screw_x')
    sk = sketch_on(body, xz, 'Sk_M6_Durchgang')
    circle(sk, sx, zs, P(sh, 'm6_thru') / 2, 'md1', 'Parameter.screw_x', '-Parameter.screw_below', 'Parameter.m6_thru / 2')
    circle(sk, -sx, zs, P(sh, 'm6_thru') / 2, 'md2', '-Parameter.screw_x', '-Parameter.screw_below', 'Parameter.m6_thru / 2')
    f = pocket(sk, 'M6_Durchgang', 'Parameter.y_boss + 1', reversed_=False)
    # Richtung pruefen: der Durchgang muss im +y-Schenkel liegen; sonst umdrehen.
    probe = Vector(sx, yi + P(sh, 'leg_t') / 2, zs + 2.9)   # knapp unter dem Durchgangsradius
    if body.Shape.isInside(probe, 1e-6, True):
        f.Reversed = True; doc.recompute()

    # M6-Gewinde hinten (-y): PartDesign-Bohrung mit modelliertem ISO-Gewinde, Skizze
    # auf der Aussenflaeche des hinteren Schenkels, Tiefe = Schenkel. Richtung wie oben
    # ueber eine Probe geprueft und notfalls umgedreht.
    sk = sketch_on(body, xz, 'Sk_M6_Gewinde')
    sk.AttachmentOffset = App.Placement(Vector(0, 0, 0), App.Rotation())
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.y_boss')     # XZ-Normale zeigt nach -y: Versatz +y_boss landet auf y = -y_boss
    circle(sk, sx, zs, 2.5, 'mt1', 'Parameter.screw_x', '-Parameter.screw_below', '2.5')
    circle(sk, -sx, zs, 2.5, 'mt2', '-Parameter.screw_x', '-Parameter.screw_below', '2.5')
    h = body.newObject('PartDesign::Hole', 'M6_Gewinde')
    h.Profile = sk
    h.DepthType = 'Dimension'
    h.setExpression('Depth', 'Parameter.leg_t + Parameter.boss_t + 0.5')
    h.Threaded = True
    h.ThreadType = 'ISOMetricProfile'
    thread = str(sh.get('m6_thread'))
    sizes = h.getEnumerationsOfProperty('ThreadSize')
    if thread not in sizes:
        alt = [z for z in sizes if z.startswith('M6')]
        if not alt: raise RuntimeError('Gewinde %s nicht in %s' % (thread, sizes))
        thread = alt[0]
    h.ThreadSize = thread
    h.ModelThread = True
    doc.recompute()
    back = Vector(sx, -(yi + P(sh, 'leg_t') / 2), zs)    # Mitte des hinteren Schenkels, Lochachse
    if body.Shape.isInside(back, 1e-6, True):
        # Loch ging in die falsche Richtung (Luft bzw. vorderer Schenkel): Skizze auf die
        # andere Seite und/oder umkehren, bis die Achse hinten frei ist.
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
    ymb = yo + P(sh, 'boss_t') / 2   # Mitte des Klotzes
    # Balken frei, Schenkel und Decke da
    for z in (1.0, bh / 2, bh - 0.5):
        if inside(0, 0, z) or inside(0, yi - 0.2, z): print('FEHLER Balkenraum nicht frei, z=%.1f' % z); ok = False
    if not inside(0, ym, bh / 2) or not inside(0, -ym, bh / 2): print('FEHLER Schenkel fehlt'); ok = False
    if not inside(0, 0, (zt0 + zt1) / 2): print('FEHLER Decke fehlt'); ok = False
    if not inside(0, ym, -lb + 1): print('FEHLER Schenkel reicht nicht unter den Balken'); ok = False
    # Fahne und Drahtloch
    hw = P(sh, 'hump_w') / 2
    for y in (0, yo - 0.3, -(yo - 0.3)):
        if inside(0, y, zh): print('FEHLER Drahtloch fehlt bei y=%.1f' % y); ok = False
        if not inside(hw - 1, y, zh): print('FEHLER Hoecker neben dem Loch fehlt bei y=%.1f' % y); ok = False
    if not inside(0, 0, zh + P(sh, 'hump_over') - 0.5): print('FEHLER Hoecker ueber dem Loch fehlt'); ok = False
    if inside(0, 0, zh + P(sh, 'hump_over') + 0.5): print('FEHLER Hoecker zu hoch'); ok = False
    hh = P(sh, 'z_hump') - zt1  # Hoeckerhoehe ueber der Decke
    for sgn in (+1, -1):
        if inside(sgn * (hw + hh + 1), 0, zt1 + 0.5): print('FEHLER Hoecker zu lang in x'); ok = False
        if not inside(sgn * (hw + hh - 1), 0, zt1 + 0.5): print('FEHLER Hoeckerflanke fehlt'); ok = False
    if inside(P(sh, 'len') / 2 - 1, 0, zt1 + 1): print('FEHLER Fahne laeuft noch durch'); ok = False
    bt = P(sh, 'boss_t')
    if not inside(P(sh, 'screw_x'), yo + bt / 2, -P(sh, 'leg_below') + 1.5): print('FEHLER Klotz fehlt'); ok = False
    if not inside(-P(sh, 'screw_x'), -(yo + bt / 2), -2): print('FEHLER Klotz -x/-y fehlt'); ok = False
    if inside(0, yo + bt / 2, -P(sh, 'leg_below') / 2): print('FEHLER Klotz in der Mitte'); ok = False
    # M6: hinten Kernloch (Material zwischen rc und rt), vorne Durchgang (frei bis rt)
    pr = rc + (rt - rc) / 2
    for sx in (P(sh, 'screw_x'), -P(sh, 'screw_x')):
        for yy in (-ym, -ymb):
            if inside(sx, yy, zs) or inside(sx, yy, zs + rc / 2 - 0.3): print('FEHLER Gewindeloch hinten fehlt'); ok = False
            if not inside(sx, yy, zs + 3.3): print('FEHLER hinten ist der Durchgang statt des Gewindes'); ok = False
        for yy in (ym, ymb):
            if inside(sx, yy, zs + pr): print('FEHLER Durchgang vorne fehlt'); ok = False
            if not inside(sx, yy, zs + rt + 0.5): print('FEHLER Schenkel ueber dem Durchgang fehlt'); ok = False
    if not inside(0, ym, zs): print('FEHLER mittig darf keine Schraube sein'); ok = False
    print('Balken %.1f x %.1f, Reiter aussen %.1f breit (Kloetze %.1f), %.1f hoch; Drahtloch Ø %.1f bei %.0f ueber dem Balken, %.1f lang'
          % (P(sh, 'beam_w'), bh, 2 * yo, 2 * P(sh, 'y_boss'), P(sh, 'z_hump') + lb, P(sh, 'wire_d') + P(sh, 'wire_fit'), P(sh, 'hole_h'), 2 * yo))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('waagereiter')
    sh = build_sheet(doc); body = build(doc, sh); doc.recompute(); set_visibility(doc, body)
    fcstd = os.path.join(here, 'waagereiter.FCStd'); doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {body.Name, body.Tip.Name, 'X_Axis', 'Y_Axis', 'Z_Axis', 'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'waagereiter.step'))
    body.Shape.exportStl(os.path.join(here, 'waagereiter.stl'))
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
