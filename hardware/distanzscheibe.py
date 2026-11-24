# -*- coding: utf-8 -*-
"""
Distanzscheibe mit Rastnase fuer das Aluminiumprofil, zum Drucken.

Scheibe 40 mm aussen, 5 mm dick, Loch 8,5 mm (M8 Durchgang), auf der einen
Stirnseite eine Rastnase 8 mm breit und 3 mm hoch, als Steg quer ueber die ganze
Flaeche -- die Scheibe liegt flach auf dem Aluprofil, der Steg greift in die Nut und
verdrehsichert sie, die M8 geht durch die Scheibe in die Nut. Um die Bohrung ist der
Steg auf nose_gap = 9 mm ausgespart (zwei Stegstuecke mit geraden Innenkanten, kein
Rundungsrest um das Loch). Alle senkrechten Kanten der Stegstuecke, innen an der
Aussparung und aussen am Umfang, sind mit nose_r = 1 mm verrundet -- die Enden
werden damit oval.

Aufbau wie libellen_clip.py: ein PartDesign-Body mit Featurebaum plus Spreadsheet
"Parameter"; alle Skizzen ueber Ausdruecke an die Tabelle gebunden. Masse aendert man
im GUI in der Tabelle, nicht in den Skizzen. Masse in mm.

Aufruf (nur das FreeCAD-eigene Python kann FreeCAD importieren):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/distanzscheibe.py

Erzeugt hardware/distanzscheibe.FCStd, .step und .stl.

LAGE: Scheibe in der XY-Ebene, Lochachse = z, z = 0 ist die Flachseite, die vom
Profil weg zeigt. Der Steg sitzt auf der Stirnseite z = disc_t, laeuft entlang x
(= Nutrichtung), ist nose_b breit (y) und nose_h hoch (z). Seine Laenge ist so
gewaehlt, dass die Ecken gerade auf dem Umfang liegen: nose_l = 2 sqrt(r^2 -
(nose_b/2)^2), nichts steht ueber die Scheibe hinaus. Die Flanken laufen parallel
(kein Keil), damit der Steg in der Nut spielfrei anliegt: Breite = nose_w - nose_fit.

Featurebaum:
    Scheibe     Kreis Ø disc_d, Pad disc_t
    Nase        zwei Rechtecke je (nose_l - nose_gap)/2 x nose_b auf der Stirnseite,
                beidseits der Aussparung nose_gap um die Bohrung, Pad nose_h
    Rundung     Fillet nose_r an den acht senkrechten Stegkanten
    Loch        Ø hole_d durch die Scheibe

Druck: Scheibe mit der stegfreien Seite aufs Bett, der Steg steht oben -- keine
Stuetze, keine Ueberhaenge.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# --------------------------------------------------------------- Parameter
PARAMS = [
    ('disc_d',    40.0, 'Scheibe Aussendurchmesser'),
    ('disc_t',     5.0, 'Scheibe dick'),
    ('hole_d',     8.5, 'Loch, M8 Durchgang'),
    ('nose_w',     8.0, 'Rastnase breit, Nennmass der Profilnut'),
    ('nose_fit',   0.2, 'Spiel in der Nutbreite, GESAMT -- Druckaufmass'),
    ('nose_h',     3.0, 'Rastnase hoch, axial ueber die Stirnseite'),
    ('nose_gap',   9.0, 'Aussparung des Stegs um die Bohrung, Gesamtbreite'),
    ('nose_r',     1.0, 'Rundung der senkrechten Stegkanten, innen und aussen'),
    # Abgeleitet
    ('nose_b',  '=nose_w - nose_fit',                           'Steg gedruckt breit'),
    ('nose_l',  '=2 * sqrt((disc_d / 2)^2 - (nose_b / 2)^2)',   'Steg lang (gesamt): Ecken genau auf dem Umfang'),
    ('nose_seg', '=(nose_l - nose_gap) / 2',                     'ein Stegstueck lang'),
]


def build_sheet(doc):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Wert')
    sh.set('C1', 'Bedeutung')
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
    return float(sh.get(name))


# ------------------------------------------------------------ Skizzenhilfen

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def poly(sk, pts, tag, exprs):
    v = Vector
    n = len(pts)
    g = []
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
    ex1 = '(%s) + (%s)' % (ex0, ew)
    ey1 = '(%s) + (%s)' % (ey0, eh)
    return poly(sk, pts, tag, [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)])


def circle(sk, cx, cy, r, tag, expr_x, expr_y, expr_r):
    g = sk.addGeometry(Part.Circle(Vector(cx, cy, 0), Vector(0, 0, 1), r), False)
    named(sk, Sketcher.Constraint('Radius', g, r), tag + '_r')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g, 3, cx), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g, 3, cy), tag + '_y')
    sk.setExpression('Constraints.%s_r' % tag, expr_r)
    sk.setExpression('Constraints.%s_x' % tag, expr_x)
    sk.setExpression('Constraints.%s_y' % tag, expr_y)
    return g


def sketch_on(body, plane, name, expr_off=None):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if expr_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, 0), App.Rotation())
        sk.setExpression('AttachmentOffset.Base.z', expr_off)
    return sk


# ------------------------------------------------------------------- Modell

def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Distanzscheibe')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

    def pad(sk, name, expr_len):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        doc.recompute()
        return f

    # 1 Scheibe
    sk = sketch_on(body, xy, 'Sk_Scheibe')
    circle(sk, 0.0, 0.0, P(sh, 'disc_d') / 2, 'sc', '0', '0', 'Parameter.disc_d / 2')
    pad(sk, 'Scheibe', 'Parameter.disc_t')

    # 2 Steg auf der Stirnseite z = disc_t, entlang x, in zwei Stuecken beidseits der
    #   eckigen Aussparung nose_gap um die Bohrung
    nb, nl, gap, seg = P(sh, 'nose_b'), P(sh, 'nose_l'), P(sh, 'nose_gap'), P(sh, 'nose_seg')
    sk = sketch_on(body, xy, 'Sk_Nase', expr_off='Parameter.disc_t')
    rect(sk, gap / 2, -nb / 2, seg, nb, 'np',
         'Parameter.nose_gap / 2', '-Parameter.nose_b / 2',
         'Parameter.nose_seg', 'Parameter.nose_b')
    rect(sk, -nl / 2, -nb / 2, seg, nb, 'nm',
         '-Parameter.nose_l / 2', '-Parameter.nose_b / 2',
         'Parameter.nose_seg', 'Parameter.nose_b')
    nase = pad(sk, 'Nase', 'Parameter.nose_h')

    # 2a Rundung: alle senkrechten Kanten des Stegs (parallel z, oberhalb der Scheibe)
    t, h = P(sh, 'disc_t'), P(sh, 'nose_h')
    edges = []
    for i, e in enumerate(nase.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Curve.Direction
        bb = e.BoundBox
        if abs(abs(d.z) - 1.0) < 1e-6 and bb.ZMin > t - 1e-3 and abs(e.Length - h) < 1e-3:
            edges.append('Edge%d' % (i + 1))
    f = body.newObject('PartDesign::Fillet', 'Rundung')
    f.Base = (nase, edges)
    f.setExpression('Radius', 'Parameter.nose_r')
    doc.recompute()
    print('Rundung an %d Kanten' % len(edges))

    # 3 Loch durch alles
    sk = sketch_on(body, xy, 'Sk_Loch')
    circle(sk, 0.0, 0.0, P(sh, 'hole_d') / 2, 'ho', '0', '0', 'Parameter.hole_d / 2')
    f = body.newObject('PartDesign::Pocket', 'Loch')
    f.Profile = sk
    f.Type = 'ThroughAll'
    f.Midplane = True
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


def set_visibility(doc, body):
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    s = body.Shape
    ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    r, t = P(sh, 'disc_d') / 2, P(sh, 'disc_t')
    zm = t / 2
    if inside(0.0, 0.0, zm):
        print('FEHLER Loch fehlt'); ok = False
    if not inside(P(sh, 'hole_d') / 2 + 0.5, 0.0, zm):
        print('FEHLER keine Scheibe neben dem Loch'); ok = False
    if not inside(r - 0.5, 0.0, zm):
        print('FEHLER Scheibe am Rand fehlt'); ok = False
    if inside(r + 0.5, 0.0, zm):
        print('FEHLER Material ausserhalb der Scheibe (x)'); ok = False
    zn = t + P(sh, 'nose_h') / 2            # Steghoehe
    xs = P(sh, 'nose_gap') / 2 + 2.0        # neben der Aussparung, auf dem Steg
    if inside(P(sh, 'nose_gap') / 2 - 0.2, 0.0, zn) or inside(P(sh, 'hole_d') / 2 + 0.1, 0.0, zn):
        print('FEHLER Aussparung um die Bohrung fehlt'); ok = False
    if not inside(P(sh, 'nose_gap') / 2 - 0.2, 0.0, zm) and P(sh, 'nose_gap') > P(sh, 'hole_d'):
        print('FEHLER Scheibe unter der Aussparung fehlt'); ok = False
    if not inside(xs, 0.0, zn):
        print('FEHLER Steg fehlt'); ok = False
    if inside(xs, 0.0, t + P(sh, 'nose_h') + 0.5):
        print('FEHLER Steg zu hoch'); ok = False
    if inside(xs, P(sh, 'nose_b') / 2 + 0.3, zn):
        print('FEHLER Steg zu breit'); ok = False
    # Rundung: die Ecke selbst ist weg, 1,5 mm daneben ist noch Steg
    cx, cy = P(sh, 'nose_gap') / 2 + 0.2, P(sh, 'nose_b') / 2 - 0.2
    if inside(cx, cy, zn):
        print('FEHLER Innenecke nicht verrundet'); ok = False
    if not inside(cx + 1.5, cy, zn):
        print('FEHLER Steg neben der Rundung fehlt'); ok = False
    if inside(0.0, 0.0, zn):
        print('FEHLER Loch geht nicht durch den Steg'); ok = False
    if inside(0.0, r - 1.0, zn):
        print('FEHLER Material auf der Stirnseite neben dem Steg'); ok = False
    if inside(r + 0.3, 0.0, zn):
        print('FEHLER Steg steht ueber den Umfang'); ok = False
    if inside(xs, 0.0, -0.3):
        print('FEHLER Material unter der Scheibe'); ok = False
    print('Steg gedruckt %.1f breit, 2 x %.1f lang, %.1f hoch, Aussparung %.1f, Rundung r %.1f; Loch Ø %.1f'
          % (P(sh, 'nose_b'), P(sh, 'nose_seg'), P(sh, 'nose_h'), P(sh, 'nose_gap'), P(sh, 'nose_r'), P(sh, 'hole_d')))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('distanzscheibe')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'distanzscheibe.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'distanzscheibe.step'))
    body.Shape.exportStl(os.path.join(here, 'distanzscheibe.stl'))

    bb = body.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm' % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.0f mm3' % body.Shape.Volume)
    print('Gueltig     %s' % body.Shape.isValid())
    print('Plausibel   %s' % check(body, sh))
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None) and 'Invalid' in o.State]
    print('Fehlerhaft  %s' % (bad if bad else 'keine'))
    offen = [o.Name for o in doc.Objects
             if o.isDerivedFrom('Sketcher::SketchObject') and not getattr(o, 'FullyConstrained', True)]
    print('Skizzen     %d, davon nicht voll bestimmt: %s'
          % (len([o for o in doc.Objects if o.isDerivedFrom('Sketcher::SketchObject')]),
             offen if offen else 'keine'))


if __name__ == '__main__':
    main()
