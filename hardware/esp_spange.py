# -*- coding: utf-8 -*-
"""
Befestigungsspange für das ESP-Gehäuse (ESP, ggf. Display, Verschaltung).

Zwei Stück nötig — das Gehäuse hat zwei Kopfseiten —, aber nur **ein** Teil
konstruiert: die Spange ist symmetrisch zur M6-Achse und passt deshalb an beide
Enden.

Drei Löcher im Dreieck:

      o-------------o     zwei M4-GEWINDE direkt in der Platte
       \           /      (Abstand M4_PITCH, Lochmitten)
        \         /
         \       /        Kontur läuft zur Mitte hin zusammen
          \     /
          +-----+
          | (O) |       eine M6, mittig, nach aussen versetzt, mit
          +-----+       Flachsenkung fuer den Schraubenkopf

Die beiden M4 kommen von oben durch die Ø-4,5-Bohrungen am Rand der Kopfseite
direkt in Gewinde, die in der Platte sitzen. Die M6 geht durch die überstehende
Spitze auf die Schiene. Zwei Schrauben, zwei Aufgaben: die M4 halten das
Gehäuse, die M6 hält die Spange.

FASSUNG VOM 2026-11-23 — schlanker, duenner, ohne Muttern:

  * M4-GEWINDE statt Muttern. Wie bei der Not-Aus-Spange (hardware/not-aus-
    spange.FCStd): PartDesign-Bohrung, ISO M4x0,7, Gewinde modelliert, durch.
    Die Schraube greift in den Druck; 4 mm Platte sind knapp sechs Gaenge.
  * PLATTE 4,0 mm wie die Not-Aus-Spange. Die 6,0 waren nur fuer die Muttern.
  * M6 VERSENKT. Die Schrauben sind kurz, ISO 7380 Kugelkopf (Kopf Ø 10,5,
    3,3 hoch). Flachsenkung Ø m6_head_d = 11, so tief, dass m6_rest = 1,5 mm
    Boden bleiben; Durchgang 6,5. Der Kopf steht 0,8 mm ueber die Platte,
    neben dem Gehaeuse, das stoert niemanden.
  * SPITZE SCHLANKER. Versatz und Spitze richten sich jetzt nach dem KOPF
    (Ø 11) statt nach der Karosseriescheibe Ø 18, die mit der Senkung
    entfaellt. Die M6-Achse rueckt damit von 16,2 auf 12,7 mm hinter die
    M4-Linie, die Spitze von 21 auf 15 mm Breite.

Aufbau wie hall_gehaeuse.py und hx711_gehaeuse.py: PartDesign-Body mit
Featurebaum plus Spreadsheet "Parameter", alle Skizzen voll bestimmt und über
Ausdrücke an die Tabelle gebunden. Maße in mm.

Featurebaum:
    Platte        Sechseck-Umriss: breit unter dem Gehäuse, Spitze zur M6
    M4_Gewinde    zwei Gewindebohrungen M4x0,7 durch die Platte
    M6_Durchgang  Ø 6,5 in der Spitze
    M6_Senkung    Flachsenkung Ø 11 von oben, bis 1,5 mm Boden bleiben
    Verrundung    die sechs senkrechten Umrisskanten

Die Oberseite ist **glatt** — keine erhöhten Auflagen. Sie liegt vollflächig an
der Kopfseite des Gehäuses.

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

        ('m4_thread',  'M4x0.7', 'Gewinde direkt in der Platte, wie Not-Aus-Spange'),
        ('m6_d',          6.5,  'Durchgang M6 zur Schiene'),
        # Flachsenkung fuer den Kugelkopf ISO 7380 M6: dk 10,5, k 3,3.
        ('m6_head_d',    11.0,  'Flachsenkung, Kugelkopf dk 10,5 + Spiel'),
        ('m6_rest',       1.5,  'Boden unter dem Schraubenkopf'),
        ('m6_cb_t', '=plate_t - m6_rest', 'Tiefe der Flachsenkung'),

        ('m4_pad',       11.3,  'Material um die Gewinde: Platte so breit wie das Gehaeuse'),
        ('plate_t',       4.0,  'Plattendicke, wie die Not-Aus-Spange'),
        ('fillet_r',      3.0,  'Verrundung der senkrechten Umrisskanten'),

        # Versatz und Spitze richten sich nach dem versenkten KOPF: die Senkung
        # soll die Gehaeusekante nicht anschneiden, und neben ihr muss an der
        # Spitze Wand stehen bleiben.
        ('m6_clear',      1.0,  'Luft Senkungsrand bis Gehaeusekante'),
        ('tip_wall',      2.0,  'Wand neben der Senkung an der Spitze'),
        ('m6_offset', '=screw_edge + m6_head_d / 2 + m6_clear',
                       'M4-Linie bis M6-Achse'),

        ('y_in',  '=-screw_edge', 'Innenkante, unter dem Gehaeuse'),
        ('y_hip', '=screw_edge',  'Hier beginnt die Verjuengung: an der Gehaeusekante'),
        ('y_out', '=m6_offset + m6_head_d / 2 + tip_wall',
                  'Aussenkante: Wand neben der Senkung'),

        ('w_wide', '=m4_pitch + m4_pad', 'Breite unter dem Gehaeuse'),
        ('w_tip',  '=m6_head_d + 2 * tip_wall',
                   'Breite an der Spitze: Wand neben der Senkung'),

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

    # 2 M4-Gewinde durch die Platte: PartDesign-Bohrung mit modelliertem
    #   ISO-Gewinde, genau wie in hardware/not-aus-spange.FCStd. Die Skizze
    #   liefert nur die Mittelpunkte, den Durchmesser bestimmt das Gewinde.
    sk = sketch_on(body, xy, 'Sk_M4', P(sh, 'plate_t'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.plate_t')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        circle(sk, sign * hx, 0.0, 2.0, 'm4' + t,
               '%d * Parameter.m4_pitch / 2' % sign, '0', '2')
    h = body.newObject('PartDesign::Hole', 'M4_Gewinde')
    h.Profile = sk
    h.DepthType = 'ThroughAll'
    h.Threaded = True
    h.ThreadType = 'ISOMetricProfile'
    thread = str(sh.get('m4_thread'))
    sizes = h.getEnumerationsOfProperty('ThreadSize')
    if thread not in sizes:
        alt = [z for z in sizes if z.startswith('M4')]
        if not alt:
            raise RuntimeError('Gewinde %s nicht in %s' % (thread, sizes))
        thread = alt[0]
    h.ThreadSize = thread
    h.ModelThread = True
    doc.recompute()

    # 4 M6 in der Spitze
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, 0.0, P(sh, 'm6_offset'), P(sh, 'm6_d') / 2, 'm6',
           '0', 'Parameter.m6_offset', 'Parameter.m6_d / 2')
    pocket(sk, 'M6_Durchgang', 'Parameter.plate_t + 1')

    # 4a Flachsenkung von oben, bis m6_rest Boden bleiben. Skizze auf der
    #    Oberseite; die Tasche geht gegen die Normale, also nach unten.
    sk = sketch_on(body, xy, 'Sk_M6_Senkung', P(sh, 'plate_t'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.plate_t')
    circle(sk, 0.0, P(sh, 'm6_offset'), P(sh, 'm6_head_d') / 2, 'cb',
           '0', 'Parameter.m6_offset', 'Parameter.m6_head_d / 2')
    pocket(sk, 'M6_Senkung', 'Parameter.m6_cb_t', reversed_=False)

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
    print('M4-Gewinde                  %s, modelliert, %.1f mm lang'
          % (doc.getObject('M4_Gewinde').ThreadSize, P(sh, 'plate_t')))
    print('Verrundung                  r = %.1f mm an 6 Kanten'
          % P(sh, 'fillet_r'))
    print('M6-Achse hinter der Gehaeusekante %.2f mm'
          % (P(sh, 'm6_offset') - P(sh, 'screw_edge')))
    print('Flachsenkung \u00d8%.1f, %.2f mm tief, Boden %.2f mm, Wand neben dem '
          'Kopf %.2f mm'
          % (P(sh, 'm6_head_d'), P(sh, 'm6_cb_t'), P(sh, 'm6_rest'),
             (P(sh, 'w_tip') - P(sh, 'm6_head_d')) / 2))
    # Geometrie pruefen: Luft in der Senkung, Boden darunter, Durchgang offen.
    s = body.Shape
    yc, r = P(sh, 'm6_offset'), P(sh, 'm6_head_d') / 2 - 0.5
    ok = (not s.isInside(Vector(r, yc, P(sh, 'plate_t') - 0.5), 1e-6, True)
          and s.isInside(Vector(r, yc, P(sh, 'm6_rest') / 2), 1e-6, True)
          and not s.isInside(Vector(0, yc, P(sh, 'm6_rest') / 2), 1e-6, True))
    print('Senkung plausibel           %s' % ok)
    return 0


main()
