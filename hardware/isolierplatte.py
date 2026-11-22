# -*- coding: utf-8 -*-
"""
Isolierplatten unter den beiden offenen Gehäusen: Wägeplatine (HX711) und
Hall-Geber. 0,5 mm dünn, in der Kontur des jeweiligen Gehäuses, mit dem
M6-Loch an derselben Stelle, und mit einem flachen Rand aussen herum, in dem
das Gehäuse sitzt.

Aufruf (nur das FreeCAD-eigene Python kann FreeCAD importieren):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/isolierplatte.py

Erzeugt je Gehäuse hardware/isolierplatte-hx711.* und hardware/isolierplatte-hall.*
(FCStd, STEP, STL).

WARUM: Beide Gehäuse sind unten offen, die Platine liegt mit der Lötseite
nach unten und nur solder_gap = 2 mm über der Montagefläche. Die ist
Aluminium. Die Platte schließt die offene Seite und legt Kunststoff
dazwischen.

KEINE EIGENEN MASSE: Kontur, Lochlage und Kanallage werden beim Bauen aus den
PARAMS-Listen von hx711_gehaeuse.py und hall_gehaeuse.py gelesen (per ast,
ohne die Skripte auszuführen). Ändert sich ein Gehäuse, läuft dieses Skript
noch einmal und die Platte folgt. Dieselbe Zahl soll nicht an zwei Stellen
stehen — das war im Projekt die Quelle fast aller Fehler.

KABELKANÄLE: Die Gehäuse klemmen das Kabel zwischen Kabelnut und
Montagefläche; die Klemmstrecke ist absichtlich wenige Zehntel enger als das
Kabel. 0,5 mm Platte darunter machten daraus das Doppelte und quetschten die
dünnen Zellenlitzen. Deshalb ist die Platte unter den Kanälen ausgeschnitten,
vom Ende der Kavität bis zum Plattenende, mit channel_clear Luft je Seite.
Das Kabel liegt dort direkt auf dem Aluminium — es ist isoliert, die Platine
nicht. Die Kavität selbst bleibt vollständig abgedeckt.

RAND: Die Platte steht rim_w = 1 mm über die Gehäusekontur hinaus und trägt
dort einen rim_h = 1 mm hohen Rand; innen bleibt die Gehäusegrundfläche plus
fit_rim Luft als flache Wanne. Das Gehäuse sitzt darin und kann beim Anziehen
der M6 nicht seitlich wandern. An den Kabelausgängen ist der Rand mit den
Kanalschlitzen durchbrochen, damit er das Kabel nicht abknickt.

Beim HALL-GEHÄUSE fehlt der Rand an der Geberseite (x = 0): die Stirnwand dort
ist nur 1,2 mm dick und steht mit dem Luftspalt vor den Magneten — ein
Millimeter Rand davor ginge in den Luftspalt. Die Platte endet vorn bündig
mit dem Gehäuse, die Wanne ist nach vorn offen; das Gehäuse wird von hinten
eingeschoben und sitzt an drei Seiten.

Featurebaum je Platte:
    Platte        Rechteck um rim_w groesser als das Gehaeuse, plate_t + rim_h dick
    Sitz          Gehaeusegrundflaeche + fit_rim, rim_h tief von oben -> die Wanne
    M6_Loch       Ø m6_d, durch
    Kanal...      je Kabelkanal ein Schlitz, durch Platte und Rand, zum Ende offen
"""

import ast
import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

HERE = os.path.dirname(os.path.abspath(__file__))

# Eigene Parameter der Platte. Alles andere kommt aus dem Gehäuse.
PLATE_PARAMS = [
    ('plate_t',        0.5, 'Plattendicke unter dem Gehaeuse'),
    ('rim_w',          1.0, 'Rand: Ueberstand ueber die Gehaeusekontur'),
    ('rim_h',          1.0, 'Rand: Hoehe ueber der Platte'),
    ('fit_rim',        0.2, 'Luft in der Wanne, GESAMT -> 0,1 je Seite'),
    ('channel_clear',  0.5, 'Luft je Seite am Kanalschlitz'),
]

# Welches Gehäuse liefert was. Die rechte Seite sind Namen aus dessen PARAMS.
#   hole_x/hole_y/hole_d  M6
#   chan_x0               Anfang der Kanalschlitze (Ende der Kavität)
#   channels              Liste (Lage y als Ausdruck, Breite als Ausdruck)
#   rim_front             Rand auch an der Stirnseite x = 0?
PLATES = {
    'hx711': {
        'source': 'hx711_gehaeuse.py',
        'rim_front': True,
        'hole_x': 'screw_x', 'hole_y': 'screw_y', 'hole_d': 'screw_d',
        'chan_x0': '=cav_x0 + cav_l',
        'channels': [('=cab_y',  '=cab_deep_d_p'),
                     ('=-cab_y', '=cab_deep_d_m')],
    },
    'hall': {
        'source': 'hall_gehaeuse.py',
        'rim_front': False,   # Geberseite: nichts vor die Stirnwand
        'hole_x': 'm6_x', 'hole_y': 'm6_y', 'hole_d': 'm6_d',
        'chan_x0': '=cav_x0 + cav_l',
        'channels': [('=cab_y', '=cab_d')],
    },
}


def read_params(filename):
    """PARAMS-Liste eines Gehäuseskripts lesen, ohne es auszuführen."""
    src = open(os.path.join(HERE, filename), encoding='utf-8').read()
    tree = ast.parse(src, filename)
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 \
                and getattr(node.targets[0], 'id', None) == 'PARAMS':
            return ast.literal_eval(node.value)
    raise RuntimeError('%s: keine PARAMS gefunden' % filename)


def build_sheet(doc, params):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Wert')
    sh.set('C1', 'Bedeutung')
    for i, (name, val, note) in enumerate(params):
        r = i + 2
        sh.set('A%d' % r, name)
        sh.setAlias('B%d' % r, name)
        sh.set('C%d' % r, note)
    doc.recompute()
    for i, (name, val, note) in enumerate(params):
        sh.set('B%d' % (i + 2), str(val))
    doc.recompute()
    return sh


def P(sh, name):
    return float(sh.get(name))


def expr(e):
    """'=a + b' -> 'Parameter.a + Parameter.b'; ein Name -> 'Parameter.name'."""
    import re
    s = e[1:] if e.startswith('=') else e
    return re.sub(r'\b([A-Za-z_]\w*)\b', r'Parameter.\1', s)


def value(sh, e):
    """Zahlenwert eines Ausdrucks ueber die Tabelle."""
    import re
    s = e[1:] if e.startswith('=') else e
    s = re.sub(r'\b([A-Za-z_]\w*)\b', lambda m: repr(P(sh, m.group(1))), s)
    return float(eval(s, {'__builtins__': {}}, {}))


# ------------------------------------------------------------ Skizzenhilfen

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


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

def build(doc, sh, spec):
    body = doc.addObject('PartDesign::Body', 'Platte')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

    def pocket_through(sk, name):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    # 1 Platte, um rim_w groesser als das Gehaeuse, in voller Hoehe plate_t +
    #   rim_h. Gleiches Koordinatensystem wie das Gehaeuse: x = 0 an der
    #   Stirnwand, y = 0 in der Mitte, z = 0 ist die Auflage auf dem Profil.
    over = P(sh, 'rim_w') + P(sh, 'fit_rim') / 2        # Ueberstand je Seite
    e_over = '(Parameter.rim_w + Parameter.fit_rim / 2)'
    x0 = -over if spec['rim_front'] else 0.0
    e_x0 = '-' + e_over if spec['rim_front'] else '0'
    sk = sketch_on(body, xy, 'Sk_Platte')
    rect(sk, x0, -P(sh, 'w_out') / 2 - over,
         P(sh, 'l_out') + over - x0, P(sh, 'w_out') + 2 * over, 'pl',
         e_x0, '-Parameter.w_out / 2 - ' + e_over,
         'Parameter.l_out + %s - (%s)' % (e_over, e_x0),
         'Parameter.w_out + 2 * ' + e_over)
    f = body.newObject('PartDesign::Pad', 'Platte')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.plate_t + Parameter.rim_h')
    doc.recompute()

    # 1a Sitz: die Gehaeusegrundflaeche plus Luft, rim_h tief von oben. Ohne
    #    Rand an der Stirnseite reicht das Rechteck ueber die Kante hinaus, die
    #    Wanne ist dort offen.
    fit = P(sh, 'fit_rim') / 2
    sx0 = -fit if spec['rim_front'] else -1.0
    e_sx0 = '-Parameter.fit_rim / 2' if spec['rim_front'] else '-1'
    sk = sketch_on(body, xy, 'Sk_Sitz', 'Parameter.plate_t + Parameter.rim_h')
    rect(sk, sx0, -P(sh, 'w_out') / 2 - fit,
         P(sh, 'l_out') + fit - sx0, P(sh, 'w_out') + 2 * fit, 'sitz',
         e_sx0, '-Parameter.w_out / 2 - Parameter.fit_rim / 2',
         'Parameter.l_out + Parameter.fit_rim / 2 - (%s)' % e_sx0,
         'Parameter.w_out + Parameter.fit_rim')
    f = body.newObject('PartDesign::Pocket', 'Sitz')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.rim_h')
    doc.recompute()

    # 2 M6-Loch, dieselbe Lage wie im Gehäuse.
    hx, hy, hd = spec['hole_x'], spec['hole_y'], spec['hole_d']
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, hx), P(sh, hy), P(sh, hd) / 2, 'm6',
           expr(hx), expr(hy), expr(hd) + ' / 2')
    pocket_through(sk, 'M6_Loch')

    # 3 Kanalschlitze: vom Ende der Kavität bis über das Plattenende hinaus.
    x0 = value(sh, spec['chan_x0'])
    for i, (e_y, e_w) in enumerate(spec['channels']):
        y, w = value(sh, e_y), value(sh, e_w) + 2 * P(sh, 'channel_clear')
        sk = sketch_on(body, xy, 'Sk_Kanal_%d' % (i + 1))
        rect(sk, x0, y - w / 2, P(sh, 'l_out') - x0 + over + 1.0, w, 'k%d' % (i + 1),
             expr(spec['chan_x0']),
             '(%s) - (%s) / 2 - Parameter.channel_clear' % (expr(e_y), expr(e_w)),
             'Parameter.l_out - (%s) + %s + 1' % (expr(spec['chan_x0']), e_over),
             '(%s) + 2 * Parameter.channel_clear' % expr(e_w))
        pocket_through(sk, 'Kanal_%d' % (i + 1))

    return body


# FreeCAD speichert die Sichtbarkeit fuer das GUI NICHT in Document.xml, sondern in
# einer GuiDocument.xml mit je einem ViewProvider pro Objekt. Headless entsteht die
# nicht — deshalb oeffnet sich eine so erzeugte Datei mit Default-Sichtbarkeiten:
# Skizzen an, Koerper aus. Die schreiben wir nach dem Speichern selbst ins Archiv.

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


def check(body, sh, spec):
    s = body.Shape
    ok = True
    hx, hy = P(sh, spec['hole_x']), P(sh, spec['hole_y'])
    if s.isInside(Vector(hx, hy, P(sh, 'plate_t') / 2), 1e-6, True):
        print('FEHLER M6-Loch fehlt'); ok = False
    x0 = value(sh, spec['chan_x0'])
    for e_y, e_w in spec['channels']:
        y = value(sh, e_y)
        if s.isInside(Vector(x0 + 1.0, y, P(sh, 'plate_t') / 2), 1e-6, True):
            print('FEHLER Kanalschlitz fehlt bei y = %.1f' % y); ok = False
        # Kavitaet bleibt zu: Material kurz vor dem Schlitzanfang
        if not s.isInside(Vector(x0 - 0.5, y, P(sh, 'plate_t') / 2), 1e-6, True):
            print('FEHLER Schlitz reicht in die Kavitaet'); ok = False
    # Wanne: Gehaeuseecke liegt auf plate_t, der Rand daneben steht rim_h hoeher
    t, h = P(sh, 'plate_t'), P(sh, 'rim_h')
    yc = P(sh, 'w_out') / 2
    if s.isInside(Vector(x0 - 2.0, yc - 0.5, t + h / 2), 1e-6, True):
        print('FEHLER Wanne fehlt (Material ueber der Gehaeuseflaeche)'); ok = False
    if not s.isInside(Vector(x0 - 2.0, yc + P(sh, 'fit_rim') / 2 + P(sh, 'rim_w') / 2,
                             t + h / 2), 1e-6, True):
        print('FEHLER Rand fehlt an der Laengsseite'); ok = False
    front = s.isInside(Vector(-P(sh, 'rim_w') / 2, 0.0, t + h / 2), 1e-6, True)
    if front != bool(spec['rim_front']):
        print('FEHLER Rand an der Stirnseite: %s, erwartet %s'
              % (front, spec['rim_front'])); ok = False
    # Steg zwischen Loch und Schlitzen
    for e_y, e_w in spec['channels']:
        y, w = value(sh, e_y), value(sh, e_w) + 2 * P(sh, 'channel_clear')
        web = abs(y - hy) - w / 2 - P(sh, spec['hole_d']) / 2
        print('Steg Loch-Kanal %.1f mm' % web)
        if web < 1.5:
            print('FEHLER Steg zwischen Loch und Kanal unter 1,5 mm'); ok = False
    return ok


def main():
    for key, spec in PLATES.items():
        params = read_params(spec['source']) + PLATE_PARAMS
        name = 'isolierplatte-' + key
        doc = App.newDocument(name)
        sh = build_sheet(doc, params)
        body = build(doc, sh, spec)
        doc.recompute()
        set_visibility(doc, body)

        fcstd = os.path.join(HERE, name + '.FCStd')
        doc.saveAs(fcstd)
        write_gui_visibility(fcstd, {
            body.Name, body.Tip.Name,
            'X_Axis', 'Y_Axis', 'Z_Axis',
            'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
        Part.export([body], os.path.join(HERE, name + '.step'))
        body.Shape.exportStl(os.path.join(HERE, name + '.stl'))

        bb = body.Shape.BoundBox
        print('== %s  aus %s' % (name, spec['source']))
        print('Aussenmass  %.1f x %.1f x %.1f mm'
              % (bb.XLength, bb.YLength, bb.ZLength))
        print('M6          x = %.1f, y = %.1f, Ø %.1f'
              % (P(sh, spec['hole_x']), P(sh, spec['hole_y']), P(sh, spec['hole_d'])))
        print('Gueltig     %s' % body.Shape.isValid())
        print('Plausibel   %s' % check(body, sh, spec))
        bad = [o.Name for o in doc.Objects if getattr(o, 'State', None)
               and 'Invalid' in o.State]
        print('Fehlerhaft  %s' % (bad if bad else 'keine'))
        offen = [o.Name for o in doc.Objects
                 if o.isDerivedFrom('Sketcher::SketchObject')
                 and not getattr(o, 'FullyConstrained', True)]
        print('Skizzen     nicht voll bestimmt: %s' % (offen if offen else 'keine'))
        App.closeDocument(doc.Name)


main()
