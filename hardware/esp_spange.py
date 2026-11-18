# -*- coding: utf-8 -*-
"""
Befestigungsspange für das ESP-Gehäuse (ESP, ggf. Display, Verschaltung).

Zwei Stück nötig — das Gehäuse hat zwei Kopfseiten —, aber nur **ein** Teil
konstruiert: die Spange ist symmetrisch zur M6-Achse und passt deshalb an beide
Enden.

Drei Löcher im Dreieck:

      o-------------o     zwei M4, Mutter von unten eingelassen
       \           /      (Abstand M4_PITCH, Lochmitten)
        \         /
         \       /        Kontur läuft zur Mitte hin zusammen
          \     /
          +-----+
          | (O) |       eine M6, mittig, nach aussen versetzt --
          +-----+       weit genug fuer eine Karosseriescheibe Ø 18

Die beiden M4 kommen von oben durch die Ø-4,5-Bohrungen am Rand der Kopfseite in
die hier eingelassenen Muttern. Die M6 geht durch die überstehende Spitze auf die
Schiene. Zwei Schrauben, zwei Aufgaben: die M4 halten das Gehäuse, die M6 hält
die Spange.

Aufbau wie hall_gehaeuse.py und hx711_gehaeuse.py: PartDesign-Body mit
Featurebaum plus Spreadsheet "Parameter", alle Skizzen voll bestimmt und über
Ausdrücke an die Tabelle gebunden. Maße in mm.

Featurebaum:
    Platte        Sechseck-Umriss: breit unter dem Gehäuse, Spitze zur M6
    Mutterloecher zwei Sechskanttaschen von unten, M4
    M4_Durchgaenge zwei Ø 4,5 durch die Platte
    M6_Durchgang  Ø 6,6 in der Spitze
    Verrundung    die sechs senkrechten Umrisskanten

Die Oberseite ist **glatt** — keine erhöhten Auflagen. Sie liegt vollflächig an
der Kopfseite des Gehäuses. Die Muttern sitzen von unten in der Platte, die
deshalb 6,0 mm dick ist: 3,4 für die Mutter, 2,6 darüber.

Koordinaten: x = 0 ist die Symmetrieachse, y = 0 die Linie der beiden M4.
+y zeigt von der Gehäusemitte weg, nach draussen. z = 0 ist die Unterseite, die
auf der Schiene liegt.
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
# Abstand der beiden Ø-4,5-Bohrungen am Gehäuse, Mitte zu Mitte, in mm.
#
# Steht hier None, baut das Skript nichts, sondern sagt, was zu messen ist —
# eine erfundene Zahl, die wie ein Maß aussieht, ist schlimmer als eine Lücke
# (Projektregel, siehe CLAUDE.md).
#
# Am Gehäuse gemessen (2026-11-18): 50,0 mm.
M4_PITCH = 50.0

SCREW_EDGE = 6.2          # Lochmitte bis Aussenkante des Gehaeuses, vorgegeben


def params(pitch):
    """(Name, Wert oder Formel, Kommentar). Formeln beginnen mit '='."""
    return [
        ('m4_pitch',   pitch,        'Lochabstand am Gehaeuse, Mitte zu Mitte'),
        ('screw_edge', SCREW_EDGE,   'Lochmitte bis Gehaeusekante, vorgegeben'),

        ('m4_d',          4.5,  'Durchgang M4'),
        ('nut_af',        7.2,  'Mutter M4 ueber Schluesselflaechen, 7,0 + Spiel'),
        ('nut_h',         3.4,  'Mutter M4 hoch, 3,2 + Spiel'),
        ('m6_d',          6.6,  'Durchgang M6 zur Schiene'),

        ('m4_pad',       11.3,  'Material um eine Mutter: ueber Eck + 1,5 je Seite'),
        ('plate_t',       6.0,  'Plattendicke: Mutter 3,4 + 2,6 darueber'),
        ('fillet_r',      3.0,  'Verrundung der senkrechten Umrisskanten'),

        # Die M6 richtet sich nach der KAROSSERIESCHEIBE, nicht nach dem
        # Schraubenkopf: DIN 9021 fuer M6 ist Ø 18, ein Zylinderkopf nur Ø 10.
        # Danach wird sowohl der Versatz als auch die Spitze bemasst, damit die
        # Scheibe vollflaechig aufliegt und nicht an die Gehaeusekante stoesst.
        ('m6_washer_d',  18.0,  'Karosseriescheibe M6, DIN 9021'),
        ('m6_clear',      1.0,  'Luft Scheibenrand bis Gehaeusekante'),
        ('tip_wall',      1.5,  'Material neben der Scheibe an der Spitze'),
        ('m6_offset', '=screw_edge + m6_washer_d / 2 + m6_clear',
                       'M4-Linie bis M6-Achse'),

        ('y_in',  '=-screw_edge', 'Innenkante, unter dem Gehaeuse'),
        ('y_hip', '=screw_edge',  'Hier beginnt die Verjuengung: an der Gehaeusekante'),
        ('y_out', '=m6_offset + m6_washer_d / 2 + tip_wall',
                  'Aussenkante: Scheibe liegt vollflaechig'),

        ('w_wide', '=m4_pitch + m4_pad', 'Breite unter dem Gehaeuse'),
        ('w_tip',  '=m6_washer_d + 2 * tip_wall',
                   'Breite an der Spitze: Scheibe liegt vollflaechig'),

        ('len', '=y_out - y_in', 'Gesamtlaenge'),
    ]


def build_sheet(doc, pitch):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Wert')
    sh.set('C1', 'Bedeutung')
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


# ------------------------------------------------------------ Skizzenhilfen

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def polygon(sk, pts, tag, exprs):
    """Geschlossenes Vieleck, voll bestimmt über je DistanceX/Y zum Ursprung.

    n Linien haben 4n Freiheitsgrade, n Koinzidenzen nehmen 2n, die 2n Maße den
    Rest — genau bestimmt, ohne Horizontal/Vertikal, die hier überbestimmen
    würden.
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


def hexagon(sk, cx, af, tag, e_cx):
    """Sechskant um (cx, 0). Eckenradius af / sqrt(3).

    Die Schlüsselflächen stehen SENKRECHT zu x: über Flächen (af) misst man in
    x, über Eck (af / cos 30°) in y. Für die Funktion gleichgültig — der Sockel
    ist rund —, aber die Prüfpunkte muessen es wissen.
    """
    rc = af / math.sqrt(3.0)
    pts, exprs = [], []
    for i in range(6):
        a = math.radians(30.0 + 60.0 * i)
        f = math.cos(a) / math.sqrt(3.0)
        s = math.sin(a) / math.sqrt(3.0)
        pts.append((cx + rc * math.cos(a), rc * math.sin(a)))
        exprs.append(('%s %+.6f * Parameter.nut_af' % (e_cx, f),
                      '%+.6f * Parameter.nut_af' % s))
    return polygon(sk, pts, tag, exprs)


# ------------------------------------------------------------------- Modell

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
    body = doc.addObject('PartDesign::Body', 'Spange')
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

    # 1 Platte: volle Breite bis zur Gehaeusekante, dann zur Spitze zusammen.
    #   Die Verjuengung beginnt erst dort, weil die Sockel darunter die volle
    #   Breite brauchen.
    sk = sketch_on(body, xy, 'Sk_Platte')
    polygon(sk,
            [(-ww, yi), (ww, yi), (ww, yh), (wt, yo), (-wt, yo), (-ww, yh)], 'pl',
            [('-Parameter.w_wide / 2', 'Parameter.y_in'),
             ('Parameter.w_wide / 2',  'Parameter.y_in'),
             ('Parameter.w_wide / 2',  'Parameter.y_hip'),
             ('Parameter.w_tip / 2',   'Parameter.y_out'),
             ('-Parameter.w_tip / 2',  'Parameter.y_out'),
             ('-Parameter.w_wide / 2', 'Parameter.y_hip')])
    pad(sk, 'Platte', 'Parameter.plate_t')

    # 2 Muttertaschen von unten
    sk = sketch_on(body, xy, 'Sk_Muttern')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        hexagon(sk, sign * hx, P(sh, 'nut_af'), 'mu' + t,
                '%d * Parameter.m4_pitch / 2' % sign)
    pocket(sk, 'Mutterloecher', 'Parameter.nut_h')

    # 3 M4 durch die Platte
    sk = sketch_on(body, xy, 'Sk_M4')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        circle(sk, sign * hx, 0.0, P(sh, 'm4_d') / 2, 'm4' + t,
               '%d * Parameter.m4_pitch / 2' % sign, '0',
               'Parameter.m4_d / 2')
    pocket(sk, 'M4_Durchgaenge', 'Parameter.plate_t + 1')

    # 4 M6 in der Spitze
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, 0.0, P(sh, 'm6_offset'), P(sh, 'm6_d') / 2, 'm6',
           '0', 'Parameter.m6_offset', 'Parameter.m6_d / 2')
    pocket(sk, 'M6_Durchgang', 'Parameter.plate_t + 1')

    # 5 Verrundung der senkrechten Umrisskanten.
    #
    #   Die Kanten werden ueber ihre LAGE gesucht, nicht ueber ihren Index: ein
    #   Index verschiebt sich bei jeder Aenderung am Baum, eine Koordinate
    #   nicht. Die senkrechten Kanten der Muttertaschen liegen woanders und
    #   bleiben dadurch scharf — eine verrundete Sechskanttasche haelt keine
    #   Mutter mehr.
    ecken = [(-ww, yi), (ww, yi), (ww, yh), (wt, yo), (-wt, yo), (-ww, yh)]
    tip = body.Tip
    namen = []
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Vertexes[-1].Point - e.Vertexes[0].Point
        if abs(d.x) > 1e-6 or abs(d.y) > 1e-6:
            continue                                   # nicht senkrecht
        q = e.Vertexes[0].Point
        if any(abs(q.x - ex) < 1e-6 and abs(q.y - ey) < 1e-6 for ex, ey in ecken):
            namen.append('Edge%d' % (i + 1))
    if len(namen) != 6:
        raise RuntimeError('Umrisskanten nicht eindeutig gefunden: %s' % namen)
    f = body.newObject('PartDesign::Fillet', 'Verrundung')
    f.Base = (tip, namen)
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
        print('M4_PITCH ist TBD — es wird nichts gebaut.')
        print()
        print('Gebraucht wird der Abstand der beiden Ø-4,5-Bohrungen an der')
        print('Kopfseite des ESP-Gehaeuses, Mitte zu Mitte, in mm. Zu ermitteln:')
        print('  * Messschieber ueber die beiden Lochmitten, oder')
        print('  * Breite der Kopfseite minus 2 x %.1f mm (Randabstand).'
              % SCREW_EDGE)
        print()
        print('Dann oben M4_PITCH setzen und erneut laufen lassen. Alles andere')
        print('haengt daran und rechnet sich von selbst:')
        print('  Breite unter dem Gehaeuse = M4_PITCH + 11,3')
        print('  Laenge, Sockel, Muttertaschen, M6-Lage: fest')
        return 1

    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('esp-spange')
    sh = build_sheet(doc, M4_PITCH)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'esp-spange.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'esp-spange.step'))
    body.Shape.exportStl(os.path.join(here, 'esp-spange.stl'))

    bb = body.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.0f mm3' % body.Shape.Volume)
    print('Gueltig     %s' % body.Shape.isValid())
    bad = [o.Name for o in doc.Objects if getattr(o, 'State', None)
           and 'Invalid' in o.State]
    print('Fehlerhaft  %s' % (bad if bad else 'keine'))
    offen = [o.Name for o in doc.Objects
             if o.isDerivedFrom('Sketcher::SketchObject')
             and not getattr(o, 'FullyConstrained', True)]
    print('Skizzen     %d, davon nicht voll bestimmt: %s'
          % (len([o for o in doc.Objects
                  if o.isDerivedFrom('Sketcher::SketchObject')]),
             offen if offen else 'keine'))
    print('Material ueber der Mutter   %.2f mm'
          % (P(sh, 'plate_t') - P(sh, 'nut_h')))
    print('Verrundung                  r = %.1f mm an 6 Kanten'
          % P(sh, 'fillet_r'))
    print('M6-Achse hinter der Gehaeusekante %.2f mm'
          % (P(sh, 'm6_offset') - P(sh, 'screw_edge')))
    print('Karosseriescheibe \u00d8%.0f: Rand %.2f mm vor der Gehaeusekante, '
          'Auflage %.2f mm je Seite'
          % (P(sh, 'm6_washer_d'),
             P(sh, 'm6_offset') - P(sh, 'm6_washer_d') / 2 - P(sh, 'screw_edge'),
             P(sh, 'tip_wall')))
    print('Scheibenrand zur Muttertasche    %.2f mm'
          % ((P(sh, 'm6_offset') - P(sh, 'm6_washer_d') / 2)
             - P(sh, 'nut_af') / math.sqrt(3.0)))
    return 0


main()
