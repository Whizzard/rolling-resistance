# -*- coding: utf-8 -*-
"""
Clip für eine Block-Libelle (Wasserwaagen-Libelle, 15,1 x 15,1 x 40 mm) auf dem
Aluminiumprofil, zum Drucken.

Aufbau wie hall_gehaeuse.py und xt60_halter.py: ein PartDesign-Body mit
Featurebaum plus ein Spreadsheet "Parameter"; alle Skizzen über Ausdrücke an
die Tabelle gebunden. Maße ändert man im GUI in der Tabelle, nicht in den
Skizzen. Maße in mm.

Aufruf (nur das FreeCAD-eigene Python kann FreeCAD importieren):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/libellen_clip.py

Erzeugt hardware/libellen-clip.FCStd, .step und .stl.

LAGE: z = 0 ist die Auflage auf dem Profil. x läuft entlang des Profils (und
entlang der Libelle), y quer dazu, symmetrisch um y = 0. Die Libelle liegt mit
ihrer langen Achse in x, also misst sie die Neigung des Profils in seiner
Längsrichtung.

Entlang x, von links nach rechts:

    PAD      STIRNWAND      LIBELLE 40,0 (+ fit_l)      STIRNWAND      PAD
    M6                                                                 M6

Zwei M6 mit Linsenkopf (ISO 7380) in der Profilnut, eine je Ende, ausserhalb der
Libelle. Unter dem Schraubenkopf liegen base_t = 1,5 mm Material — das ist die
Vorgabe; die ganze Grundplatte ist so dick, dann liegt die Libelle in derselben
Ebene wie die Schraubenauflage und die Unterseite ist eine einzige Fläche. Die
Senkung entfällt, der Kopf steht oben auf.

Die Libelle soll von DREI Seiten sichtbar bleiben: von oben und von beiden
Längsseiten. Deshalb hält der Clip sie an den STIRNSEITEN:

    Stirnwaende   je wall_t dick, bis knapp über die Libelle hoch, an den
                  Pads, flexibel.
    Nasen         kleine Dreiecke oben an der Innenseite jeder Stirnwand,
                  nose_d tief über die Oberkante der Libelle, nose_w breit,
                  mittig. Die Schräge zeigt nach oben: beim Eindrücken von
                  oben biegt die Libelle die Stirnwände auseinander, die Nasen
                  schnappen über die Oberkante. Rund 27° zur Senkrechten
                  (nose_d : nose_h = 1 : 2), die Unterseite waagerecht.
                  Lösen: eine Stirnwand mit dem Daumen nach aussen drücken,
                  Libelle anheben.
    Leisten       zwei niedrige Leisten rail_h hoch entlang der Längsseiten,
                  über die GANZE Länge. Sie richten die Libelle quer aus und
                  versteifen die dünne Grundplatte. Von der Libelle verdecken
                  sie unten rail_h, der Rest der Längsseite bleibt frei.

Die Stirnwand biegt sich beim Einschnappen um nose_d an der Oberkante aus; bei
wall_t = 1,5 und rund 14 mm freier Höhe über den Leisten ist das weniger als
1 % Dehnung am Fuss — PETG und PLA machen das dauerhaft mit. Der Schraubenkopf
sitzt mit head_clear Luft neben der Wand und unterhalb dessen, wo sie sich
bewegt.

Featurebaum:
    Grundplatte       Quader base_l x base_w x base_t, z = 0 ist die Auflage
    Leisten           2 x rail_t breit, rail_h hoch, volle Länge, aussen
    Stirnwaende       2 x wall_t dick, volle Breite, wall_h hoch über der Platte
    Nasen             2 x Dreieck in XZ, nose_w breit, mittig
    M6_Loecher        2 x Ø m6_d durch die Grundplatte

Druck: flach, Grundplatte aufs Bett. Die Nasen stehen 0,6 mm frei über — eine
Düsenbreite, das druckt jeder FDM-Drucker ohne Stütze.

Das Libellenmass 15,1 x 15,1 x 40 ist am Teil GEMESSEN (2026-11-23). Der
Kopfdurchmesser der M6 ist der Normwert ISO 7380 und dient nur dazu, das Pad
lang genug zu machen — wer andere Schrauben nimmt, trägt head_d ein.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# --------------------------------------------------------------- Parameter
# (Name, Wert oder Formel, Kommentar). Formeln beginnen mit '='.
PARAMS = [
    # Libelle, gemessen 2026-11-23
    ('vial_l',       40.0, 'Libelle lang (x), gemessen'),
    ('vial_w',       15.1, 'Libelle breit (y), gemessen'),
    ('vial_h',       15.1, 'Libelle hoch (z), gemessen'),
    ('fit_l',         0.3, 'Spiel in der Laenge, GESAMT'),
    ('fit_w',         0.3, 'Spiel in der Breite, GESAMT'),
    ('fit_h',         0.1, 'Luft zwischen Oberkante Libelle und Nasenunterseite'),

    # Schraube M6, Linsenkopf ISO 7380: dk ~10,5, k ~4
    ('m6_d',          6.6, 'M6 Durchgang, wie die anderen Halter'),
    ('head_d',       10.5, 'Linsenkopf ISO 7380 M6, Normwert -- nur fuer die Padlaenge'),
    ('head_edge',     2.0, 'Rand neben dem Kopf bis zum Plattenende'),
    ('head_clear',    1.0, 'Luft zwischen Kopf und Stirnwand'),

    # Koerper
    ('base_t',        1.5, 'Grundplatte dick = Material unter dem Schraubenkopf, VORGABE'),
    ('rail_t',        1.2, 'Laengsleiste dick'),
    ('rail_h',        2.0, 'Laengsleiste hoch ueber der Platte; so viel ist seitlich verdeckt'),
    ('wall_t',        1.5, 'Stirnwand dick; duenn genug zum Federn'),
    ('nose_d',        0.6, 'Nase ueber die Oberkante der Libelle'),
    ('nose_h',        1.2, 'Nase hoch -> Schraege rund 27 Grad zur Senkrechten'),
    ('nose_w',       10.0, 'Nase breit, mittig auf der Stirnwand'),

    # Abgeleitet
    ('cav_l',  '=vial_l + fit_l',                  'Fach lang'),
    ('cav_w',  '=vial_w + fit_w',                  'Fach breit'),
    ('base_w', '=cav_w + 2 * rail_t',              'Grundplatte breit'),
    ('m6_x',   '=head_edge + head_d / 2',          'erste Schraube ab Plattenende'),
    ('cav_x0', '=m6_x + head_d / 2 + head_clear + wall_t', 'Fach beginnt'),
    ('base_l', '=2 * cav_x0 + cav_l',              'Grundplatte lang'),
    ('m6_pitch', '=base_l - 2 * m6_x',             'Lochabstand der M6'),
    ('wall_h', '=vial_h + fit_h + nose_h',         'Stirnwand hoch ueber der Platte'),
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
    """Zahlenwert eines Parameters."""
    return float(sh.get(name))


# ------------------------------------------------------------ Skizzenhilfen

def named(sk, con, name):
    i = sk.addConstraint(con)
    sk.renameConstraint(i, name)
    return i


def poly(sk, pts, tag, exprs):
    """Geschlossener Polygonzug, jeder Eckpunkt über DistanceX/DistanceY an
    Ausdrücke gebunden -> voll bestimmt. pts: [(x, y), ...], exprs:
    [(expr_x, expr_y), ...] in derselben Reihenfolge."""
    v = Vector
    n = len(pts)
    g = []
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        g.append(sk.addGeometry(Part.LineSegment(v(a[0], a[1], 0), v(b[0], b[1], 0)),
                                False))
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i in range(n):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, pts[i][0]),
              '%s_x%d' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, pts[i][1]),
              '%s_y%d' % (tag, i))
        sk.setExpression('Constraints.%s_x%d' % (tag, i), exprs[i][0])
        sk.setExpression('Constraints.%s_y%d' % (tag, i), exprs[i][1])
    return g


def rect(sk, x0, y0, w, h, tag, ex0, ey0, ew, eh):
    """Achsparalleles Rechteck über poly; Ausdrücke für Ecke und Maße."""
    pts = [(x0, y0), (x0 + w, y0), (x0 + w, y0 + h), (x0, y0 + h)]
    ex1 = '(%s) + (%s)' % (ex0, ew)
    ey1 = '(%s) + (%s)' % (ey0, eh)
    exprs = [(ex0, ey0), (ex1, ey0), (ex1, ey1), (ex0, ey1)]
    return poly(sk, pts, tag, exprs)


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

def sketch_on(body, plane, name, off=0.0, expr_off=None):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if off or expr_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, off), App.Rotation())
    if expr_off:
        sk.setExpression('AttachmentOffset.Base.z', expr_off)
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Clip')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, xz = planes['XY_Plane'], planes['XZ_Plane']

    def pad(sk, name, expr_len, midplane=False):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Midplane = midplane
        doc.recompute()
        return f

    def pocket_through(sk, name):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    base_l, base_w, base_t = P(sh, 'base_l'), P(sh, 'base_w'), P(sh, 'base_t')
    cav_x0, cav_l, wall_t = P(sh, 'cav_x0'), P(sh, 'cav_l'), P(sh, 'wall_t')
    cav_x1 = cav_x0 + cav_l

    # 1 Grundplatte. z = 0 ist die Auflage auf dem Profil.
    sk = sketch_on(body, xy, 'Sk_Grundplatte')
    rect(sk, 0.0, -base_w / 2, base_l, base_w, 'gp',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.base_w')
    pad(sk, 'Grundplatte', 'Parameter.base_t')

    # 2 Laengsleisten aussen, volle Laenge, auf der Platte.
    sk = sketch_on(body, xy, 'Sk_Leisten', expr_off='Parameter.base_t')
    rect(sk, 0.0, base_w / 2 - P(sh, 'rail_t'), base_l, P(sh, 'rail_t'), 'lp',
         '0', 'Parameter.base_w / 2 - Parameter.rail_t',
         'Parameter.base_l', 'Parameter.rail_t')
    rect(sk, 0.0, -base_w / 2, base_l, P(sh, 'rail_t'), 'lm',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.rail_t')
    pad(sk, 'Leisten', 'Parameter.rail_h')

    # 3 Stirnwaende, volle Breite, auf der Platte.
    sk = sketch_on(body, xy, 'Sk_Stirnwaende', expr_off='Parameter.base_t')
    rect(sk, cav_x0 - wall_t, -base_w / 2, wall_t, base_w, 'wl',
         'Parameter.cav_x0 - Parameter.wall_t', '-Parameter.base_w / 2',
         'Parameter.wall_t', 'Parameter.base_w')
    rect(sk, cav_x1, -base_w / 2, wall_t, base_w, 'wr',
         'Parameter.cav_x0 + Parameter.cav_l', '-Parameter.base_w / 2',
         'Parameter.wall_t', 'Parameter.base_w')
    pad(sk, 'Stirnwaende', 'Parameter.wall_h')

    # 4 Nasen: Dreiecke in der XZ-Ebene (lokal x = global x, lokal y = global z),
    #   oben an der Innenseite jeder Stirnwand, symmetrisch um y = 0 gezogen.
    #   Ecken: oben an der Wand, unten an der Wand, unten an der Spitze -> die
    #   Hypotenuse ist die Einfuehrschraege, die Unterseite waagerecht.
    ztop = base_t + P(sh, 'wall_h')
    zbot = ztop - P(sh, 'nose_h')
    e_top = 'Parameter.base_t + Parameter.wall_h'
    e_bot = 'Parameter.base_t + Parameter.wall_h - Parameter.nose_h'
    sk = sketch_on(body, xz, 'Sk_Nasen')
    poly(sk, [(cav_x0, ztop), (cav_x0 + P(sh, 'nose_d'), zbot), (cav_x0, zbot)], 'nl',
         [('Parameter.cav_x0', e_top),
          ('Parameter.cav_x0 + Parameter.nose_d', e_bot),
          ('Parameter.cav_x0', e_bot)])
    poly(sk, [(cav_x1, ztop), (cav_x1, zbot), (cav_x1 - P(sh, 'nose_d'), zbot)], 'nr',
         [('Parameter.cav_x0 + Parameter.cav_l', e_top),
          ('Parameter.cav_x0 + Parameter.cav_l', e_bot),
          ('Parameter.cav_x0 + Parameter.cav_l - Parameter.nose_d', e_bot)])
    pad(sk, 'Nasen', 'Parameter.nose_w', midplane=True)

    # 5 M6-Loecher, eines je Pad, durch die Grundplatte.
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6l',
           'Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    circle(sk, base_l - P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6r',
           'Parameter.base_l - Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    pocket_through(sk, 'M6_Loecher')

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
    """GuiDocument.xml ins FCStd-Archiv schreiben."""
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
    """Körper sichtbar, Skizzen und Zwischenschritte aus."""
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    """Geometrische Plausibilitaet, damit ein Vorzeichenfehler bei den
    Skizzenebenen nicht erst im Slicer auffaellt."""
    s = body.Shape
    ok = True
    inside = lambda x, y, z: s.isInside(Vector(x, y, z), 1e-6, True)
    base_t, base_w, base_l = P(sh, 'base_t'), P(sh, 'base_w'), P(sh, 'base_l')
    cav_x0, cav_l = P(sh, 'cav_x0'), P(sh, 'cav_l')
    cav_x1 = cav_x0 + cav_l
    xc = (cav_x0 + cav_x1) / 2
    ztop = base_t + P(sh, 'wall_h')
    zbot = ztop - P(sh, 'nose_h')

    # Grundplatte: Material unter dem Kopf, Luft im Loch
    for x in (P(sh, 'm6_x'), base_l - P(sh, 'm6_x')):
        if not inside(x + P(sh, 'm6_d') / 2 + 0.5, 0.0, base_t / 2):
            print('FEHLER keine Platte neben dem Loch bei x = %.1f' % x); ok = False
        if inside(x, 0.0, base_t / 2):
            print('FEHLER M6-Loch fehlt bei x = %.1f' % x); ok = False
        if inside(x, 0.0, base_t + 0.5):
            print('FEHLER Material ueber dem Kopf bei x = %.1f' % x); ok = False
    # Fach: Luft in der Mitte und direkt ueber der Oberkante der Libelle
    if inside(xc, 0.0, base_t + P(sh, 'vial_h') / 2):
        print('FEHLER Fach nicht frei'); ok = False
    if inside(xc, 0.0, ztop + 0.5):
        print('FEHLER Material ueber dem Fach'); ok = False
    # Libelle liegt auf der Platte: Material direkt darunter
    if not inside(xc, 0.0, base_t / 2):
        print('FEHLER kein Boden unter der Libelle'); ok = False
    # Leisten: Material aussen, Luft innen auf Leistenhoehe
    for sign in (+1, -1):
        y_rail = sign * (base_w / 2 - P(sh, 'rail_t') / 2)
        if not inside(xc, y_rail, base_t + P(sh, 'rail_h') / 2):
            print('FEHLER keine Leiste bei y = %+.1f' % y_rail); ok = False
        if inside(xc, sign * (P(sh, 'cav_w') / 2 - 0.1), base_t + P(sh, 'rail_h') / 2):
            print('FEHLER Leiste ragt ins Fach bei y = %+.1f' % y_rail); ok = False
    # Stirnwaende: Material in Wandmitte, bis oben
    for xw in (cav_x0 - P(sh, 'wall_t') / 2, cav_x1 + P(sh, 'wall_t') / 2):
        if not inside(xw, 0.0, ztop - 0.2):
            print('FEHLER Stirnwand fehlt bei x = %.1f' % xw); ok = False
    # Nasen: Material im Dreieck, Luft darunter (Libelle) und neben der Nase
    q = P(sh, 'nose_d') / 4
    for xn, sign in ((cav_x0, +1), (cav_x1, -1)):
        if not inside(xn + sign * q, 0.0, zbot + P(sh, 'nose_h') / 2):
            print('FEHLER Nase fehlt bei x = %.1f' % xn); ok = False
        if inside(xn + sign * q, 0.0, zbot - 0.3):
            print('FEHLER Nase ragt unter die Libellenoberkante bei x = %.1f' % xn); ok = False
        if inside(xn + sign * q, P(sh, 'nose_w') / 2 + 0.5, zbot + P(sh, 'nose_h') / 2):
            print('FEHLER Nase zu breit bei x = %.1f' % xn); ok = False
    # Kopf neben der Wand frei
    gap = cav_x0 - P(sh, 'wall_t') - (P(sh, 'm6_x') + P(sh, 'head_d') / 2)
    print('Luft Kopf-Stirnwand %.1f mm, Lochabstand %.1f mm' % (gap, P(sh, 'm6_pitch')))
    print('Seitlich verdeckt %.1f von %.1f mm, Nase %.1f mm ueber der Oberkante'
          % (P(sh, 'rail_h'), P(sh, 'vial_h'), P(sh, 'nose_d')))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('libellen-clip')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'libellen-clip.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'libellen-clip.step'))
    body.Shape.exportStl(os.path.join(here, 'libellen-clip.stl'))

    bb = body.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.0f mm3' % body.Shape.Volume)
    print('Gueltig     %s' % body.Shape.isValid())
    print('Plausibel   %s' % check(body, sh))
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
