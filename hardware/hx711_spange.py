# -*- coding: utf-8 -*-
"""
Befestigungsspange für das Gehäuse der Wägeplatine. Zwei Stück, links und rechts.

Aufbau wie hall_gehaeuse.py und hx711_gehaeuse.py: PartDesign-Body mit
Featurebaum plus Spreadsheet "Parameter", alle Skizzen voll bestimmt und über
Ausdrücke an die Tabelle gebunden. Maße in mm.

Sie verbindet zwei Dinge:

  * **nach oben** mit einer der beiden Laschen des Gehäuses (§7.4): eine M4 von
    oben durch die Lasche in die hier von unten eingelassene Mutter.
  * **nach unten** mit der Schiene: eine M6 durch den überstehenden Rand,
    mittig darin.

Der **Sockel** ist der erhöhte Teil unter der Lasche. Er muss erhöht sein: in der
Klemmstrecke steht das Kabel rund 1,0 mm unter der offenen Unterseite des
Gehäuses vor (Omega-Nut, Kreis Ø 3,9 um z = 0,955). Läge die Spange flach an,
drückte sie darauf. Mit `sp_boss_h` bleibt dort Luft.

Die Kontur verjüngt sich zur Gehäusemitte hin. Weiter als `sp_w_in` geht das
nicht: die Muttertasche braucht über Eck 8,3 mm plus Wand.

Featurebaum:
    Platte        Trapez, verjüngt zur Gehäusemitte
    Sockel        erhöht unter der Lasche, hält die Mutter
    Mutterloch    Sechskant von unten, M4
    M4_Durchgang  Ø 4,5 durch den Sockel
    M6_Durchgang  Ø 6,6 im überstehenden Rand, mittig

Koordinaten: y = 0 ist die M4-Achse, +y zeigt von der Gehäusemitte weg.
z = 0 ist die Unterseite, die auf der Schiene liegt.

SCHRAUBENLÄNGE: Sockeloberkante bis Mutterunterkante sind sp_t + sp_boss_h.
Zusammen mit der Senkung im Gehäuse (`lug_cb_depth`) ergibt das die Länge der
M4. Beide Dateien gehören zusammen geändert.
"""

import os
import sys
import math

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# --------------------------------------------------------------- Parameter
PARAMS = [
    ('sp_edge',       6.2,  'Lochmitte bis Kante, wie am Gehaeuse'),
    ('sp_span',      12.0,  'M4-Achse bis M6-Achse'),
    ('sp_w_in',      11.0,  'Breite innen, von der Muttertasche bestimmt'),
    ('sp_w_out', '=2 * sp_edge', 'Breite aussen, M6 mittig im Rand'),

    ('sp_t',          5.0,  'Plattendicke'),
    ('sp_boss_h',     2.0,  'Sockel ueber der Platte, Luft fuer das Kabel'),
    ('sp_boss_top', '=sp_t + sp_boss_h', 'Sockeloberkante, liegt an der Lasche'),

    ('sp_m4_d',       4.5,  'Durchgang M4'),
    ('sp_nut_af',     7.2,  'Mutter M4 ueber Schluesselflaechen, 7,0 + Spiel'),
    ('sp_nut_h',      3.4,  'Mutter M4 hoch, 3,2 + Spiel'),
    ('sp_m6_d',       6.6,  'Durchgang M6 zur Schiene'),

    ('sp_y_in',  '=-sp_edge',           'Innenkante'),
    ('sp_y_out', '=sp_span + sp_edge',  'Aussenkante'),
    ('sp_len',   '=sp_y_out - sp_y_in', 'Gesamtlaenge'),
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


def polygon(sk, pts, tag, exprs):
    """Geschlossenes Vieleck, voll bestimmt über je DistanceX/Y zum Ursprung.

    4 Linien haben 16 Freiheitsgrade, 4 Koinzidenzen nehmen 8, die 8 Maße den
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


def rect(sk, x0, y0, w, h, tag, expr_x0, expr_y0, expr_w, expr_h):
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


def hexagon(sk, af, tag):
    """Sechskant um den Ursprung, Schlüsselflächen parallel zu x.

    Eckenradius ist af / sqrt(3). Die Eckpunkte werden einzeln bemaßt und an
    `sp_nut_af` gebunden, damit die Tasche der Mutter in der Tabelle folgt.
    """
    rc = af / math.sqrt(3.0)
    pts, exprs = [], []
    for i in range(6):
        a = math.radians(30.0 + 60.0 * i)
        pts.append((rc * math.cos(a), rc * math.sin(a)))
        exprs.append(('%+.6f * Parameter.sp_nut_af' % (math.cos(a) / math.sqrt(3.0)),
                      '%+.6f * Parameter.sp_nut_af' % (math.sin(a) / math.sqrt(3.0))))
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

    wi, wo = P(sh, 'sp_w_in') / 2, P(sh, 'sp_w_out') / 2
    yi, yo = P(sh, 'sp_y_in'), P(sh, 'sp_y_out')

    # 1 Platte, Trapez: innen schmaler
    sk = sketch_on(body, xy, 'Sk_Platte')
    polygon(sk,
            [(-wi, yi), (wi, yi), (wo, yo), (-wo, yo)], 'pl',
            [('-Parameter.sp_w_in / 2',  'Parameter.sp_y_in'),
             ('Parameter.sp_w_in / 2',   'Parameter.sp_y_in'),
             ('Parameter.sp_w_out / 2',  'Parameter.sp_y_out'),
             ('-Parameter.sp_w_out / 2', 'Parameter.sp_y_out')])
    pad(sk, 'Platte', 'Parameter.sp_t')

    # 2 Sockel, erhöht unter der Lasche
    sk = sketch_on(body, xy, 'Sk_Sockel')
    rect(sk, -wi, yi, P(sh, 'sp_w_in'), 2 * P(sh, 'sp_edge'), 'so',
         '-Parameter.sp_w_in / 2', 'Parameter.sp_y_in',
         'Parameter.sp_w_in', '2 * Parameter.sp_edge')
    pad(sk, 'Sockel', 'Parameter.sp_boss_top')

    # 3 Muttertasche von unten
    sk = sketch_on(body, xy, 'Sk_Mutter')
    hexagon(sk, P(sh, 'sp_nut_af'), 'mu')
    pocket(sk, 'Mutterloch', 'Parameter.sp_nut_h')

    # 4 M4 durch den Sockel
    sk = sketch_on(body, xy, 'Sk_M4')
    circle(sk, 0.0, 0.0, P(sh, 'sp_m4_d') / 2, 'm4',
           '0', '0', 'Parameter.sp_m4_d / 2')
    pocket(sk, 'M4_Durchgang', 'Parameter.sp_boss_top + 1')

    # 5 M6 im überstehenden Rand, mittig
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, 0.0, P(sh, 'sp_span'), P(sh, 'sp_m6_d') / 2, 'm6',
           '0', 'Parameter.sp_span', 'Parameter.sp_m6_d / 2')
    pocket(sk, 'M6_Durchgang', 'Parameter.sp_t + 1')

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
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hx711-spange')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'hx711-spange.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'hx711-spange.step'))
    body.Shape.exportStl(os.path.join(here, 'hx711-spange.stl'))

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


main()
