# -*- coding: utf-8 -*-
"""
Halter für den XT60-Einbaustecker (Amass XT60E-M) am Aluminiumrahmen.

Aufbau wie hall_gehaeuse.py und hx711_gehaeuse.py: ein PartDesign-Body mit
Featurebaum plus ein Spreadsheet "Parameter"; alle Skizzen über Ausdrücke an
die Tabelle gebunden. Maße ändert man im GUI in der Tabelle, nicht in den
Skizzen. Maße in mm.

Aufruf (nur das FreeCAD-eigene Python kann FreeCAD importieren):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/xt60_halter.py

Erzeugt hardware/xt60-halter.FCStd, .step und .stl.

LAGE: Der Stecker LIEGT — seine lange Achse (die beiden Ohren) parallel zur
Montagefläche, die Steckrichtung parallel zum Profil. Das Kabel läuft damit am
Rahmen entlang, nicht von ihm weg.

Ein L-Winkel: eine Grundplatte mit EINER zentralen M6-Schraube auf das Profil,
daran eine Stirnwand mit dem Ausschnitt für den Stecker. Die Steckerhülse geht
durch die Wand, die Ohren liegen auf der RÜCKSEITE der Wand an und werden mit
zwei M3 von vorn durch die Wand geschraubt — die Muttern sitzen in den
Sechskant-Taschen, die der Stecker auf seiner Lötseite in den Ohren hat. Zwei
Dreiecksrippen aussen stützen die Wand gegen die Steckkraft.

ALLE STECKERMASSE stammen aus der Maßzeichnung im Amass-Datenblatt XT60E-M
(Changzhou Amass Electronics, "XT60E-M.pdf"), abgelesen 2026-11-22:

    Vorderansicht   27,00 Gesamtlänge über die Ohren
                    20,00 Lochabstand der Ohren (Mitte-Mitte)  ← LESART, s. u.
                    15,50 Hülse breit, 8,10 Hülse hoch (Aussenkontur)
                    14,20 Innenkontur, 7,20 Kontaktabstand
    Seitenansicht   16,70 Gesamttiefe von der Lötfahne bis zur Steckfläche
                     4,00 Flanschdicke (die Ohren)
                     9,50 von der Lötfahne bis ... (Bezugskante in der Kopie
                          nicht eindeutig); die Hülse steht in jeder Lesart
                          mindestens 7 mm vor den Flansch
    Rückansicht     Ø3,00 Ohrbohrung, Sechskant-Tasche für die Mutter

Die Zeichnung im Datenblatt ist eine kleine Rastergrafik. Der Lochabstand ist
das EINZIGE Mass, bei dem ein Ablesefehler den Halter unbrauchbar macht —
Händlerangaben im Netz nennen 16, 19 und 20 mm für "XT60 Einbau", je nach
Variante. VOR DEM DRUCK am Stecker mit dem Messschieber nachmessen und
`con_pitch` in der Tabelle setzen.

OBEN OFFEN: Vom Ausschnitt läuft ein Schlitz bis zur Oberkante der Wand, damit
die angelöteten Leitungen von oben eingelegt werden und nicht durch den
Ausschnitt gefädelt werden müssen. Der Schlitz ist NICHT so breit wie die
Hülse: die Ohrlöcher des XT60E-M liegen nur 0,4 mm neben der Hülsenkontur
(Lochabstand 20,0 bei 15,5 Hülse, Ø3), ein Schlitz in Hülsenbreite nähme den
Pfosten an der Schraube alles Material. 11 mm reichen für zwei 12-AWG-
Leitungen und lassen neben den Löchern 2,9 mm stehen.

Featurebaum:
    Grundplatte       Quader base_l x base_w x base_t, z = 0 ist die Auflage
    Stirnwand         Quader wall_t x base_w x wall_h bei x = 0
    Rippe_p/_m        Dreieck in XZ, je eine an der Aussenkante
    Ausschnitt        Rechteck durch die Wand für die Hülse
    Kabelschlitz      Rechteck vom Ausschnitt bis über die Oberkante
    Ohrloecher        2 x Ø screw_d durch die Wand, Abstand con_pitch
    M6_Durchgang      Ø m6_d durch die Grundplatte, zentral
    M6_Senkung        Ø m6_head_d, m6_cb_t tief, von oben — Zylinderkopf
    Fasen             an den oberen Längskanten der Grundplatte

Reihenfolge der Montage: ERST die M6 anziehen, DANN den Stecker einsetzen —
der Stecker liegt über der Schraube, mit Stecker ist kein Schlüssel mehr
anzusetzen. Deshalb ist der Kopf versenkt und der Ausschnitt so hoch gelegt,
dass die Hülse über dem Schraubenkopf frei bleibt (con_z).

Keine Nase für die Profilnut: wie bei den anderen Haltern hält allein die
Klemmung der M6. Wer eine Nase will, setzt key_w auf die Nutbreite des
Profils (am Profil messen, nicht raten) — dann kommt unten eine Leiste dazu.
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
    # Stecker, Datenblatt Amass XT60E-M (siehe Kopf)
    ('con_l',        27.0, 'Gesamtlaenge ueber die Ohren, Datenblatt'),
    ('con_pitch',    20.0, 'Lochabstand der Ohren, Datenblatt-LESART -> NACHMESSEN'),
    ('con_hole_d',    3.0, 'Ohrbohrung, Datenblatt'),
    ('con_shell_w',  15.5, 'Huelse breit (lange Seite), Datenblatt'),
    ('con_shell_h',   8.1, 'Huelse hoch (kurze Seite), Datenblatt'),
    ('con_flange_t',  4.0, 'Flanschdicke, Datenblatt -- nur zur Information'),
    ('con_depth',    16.7, 'Gesamttiefe, Datenblatt -- nur zur Information'),

    ('fit_shell',     0.5, 'Spiel am Ausschnitt, GESAMT -> 0,25 je Seite'),
    ('screw_d',       3.2, 'M3 Durchgang in der Wand'),
    ('cut_w', '=con_shell_w + fit_shell', 'Ausschnitt breit'),
    ('cut_h', '=con_shell_h + fit_shell', 'Ausschnitt hoch'),
    ('slot_w',       11.0, 'Kabelschlitz nach oben; < con_pitch - screw_d - 5'),

    # Schraube M6, Zylinderkopf DIN 912: k = 6, dk = 10
    ('m6_d',          6.6, 'M6 Durchgang, wie die anderen Halter'),
    ('m6_head_d',    11.0, 'Senkung fuer den Zylinderkopf, dk 10 + Spiel'),
    ('m6_head_h',     6.0, 'Kopfhoehe DIN 912 M6'),
    ('m6_cb_t',       3.0, 'Senkungstiefe -> Kopf steht noch 3 mm ueber'),
    ('head_clear',    1.5, 'Luft zwischen Kopf und Steckerhuelse'),

    # Koerper
    ('base_t',        6.0, 'Grundplatte dick; 3 mm Senkung lassen 3 mm Boden'),
    ('base_l',       40.0, 'Grundplatte lang, in Steckrichtung'),
    ('base_w',       34.0, 'Grundplatte breit; Ohren 27 + 2 Rippen a 3 + Luft'),
    ('wall_t',        3.0, 'Stirnwand dick; die Huelse steht >= 7 mm vor'),
    ('rib_t',         3.0, 'Rippe dick'),
    ('rib_l',        22.0, 'Rippe lang am Boden, ab Wandrueckseite'),
    ('chamfer',       1.0, 'Fase an den oberen Laengskanten der Grundplatte'),
    ('key_w',         0.0, 'Nutnase breit; 0 = keine. Nutbreite am Profil MESSEN'),
    ('key_h',         0.0, 'Nutnase hoch; 0 = keine'),

    ('m6_x',  '=base_l / 2',                       'Schraube zentral'),
    ('con_z', '=base_t + m6_head_h - m6_cb_t + head_clear + cut_h / 2',
              'Mitte des Ausschnitts ueber der Auflage'),
    ('wall_h', '=con_z + cut_h / 2 + 3',           'Stirnwand hoch, 3 mm Rand ueber der Huelse'),
    ('rib_y',  '=base_w / 2 - rib_t / 2',          'Rippenmitte, Betrag'),
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


def rect(sk, x0, y0, w, h, tag, expr_x0, expr_y0, expr_w, expr_h):
    """Achsparalleles Rechteck, voll bestimmt und an Ausdrücke gebunden."""
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


def triangle(sk, x0, y0, w, h, tag, expr_x0, expr_y0, expr_w, expr_h):
    """Rechtwinkliges Dreieck: Kathete w entlang +x, Kathete h entlang +y,
    rechter Winkel bei (x0, y0)."""
    v = Vector
    g = [sk.addGeometry(Part.LineSegment(v(x0, y0, 0), v(x0 + w, y0, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0 + w, y0, 0), v(x0, y0 + h, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x0, y0 + h, 0), v(x0, y0, 0)), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[0]))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', g[0], 1, g[0], 2, w), tag + '_w')
    named(sk, Sketcher.Constraint('DistanceY', g[2], 2, g[2], 1, h), tag + '_h')
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x0), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, y0), tag + '_y')
    sk.setExpression('Constraints.%s_w' % tag, expr_w)
    sk.setExpression('Constraints.%s_h' % tag, expr_h)
    sk.setExpression('Constraints.%s_x' % tag, expr_x0)
    sk.setExpression('Constraints.%s_y' % tag, expr_y0)
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
    body = doc.addObject('PartDesign::Body', 'Halter')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, xz, yz = planes['XY_Plane'], planes['XZ_Plane'], planes['YZ_Plane']

    def pad(sk, name, expr_len, midplane=False, reversed_=False):
        f = body.newObject('PartDesign::Pad', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Midplane = midplane
        f.Reversed = reversed_
        doc.recompute()
        return f

    def pocket_through(sk, name):
        """Durchgehende Tasche in beide Richtungen — unabhängig davon, wohin
        die Skizzennormale zeigt."""
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    def pocket(sk, name, expr_len, reversed_=False):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.setExpression('Length', expr_len)
        f.Reversed = reversed_
        doc.recompute()
        return f

    # 1 Grundplatte. z = 0 ist die Auflage auf dem Profil.
    sk = sketch_on(body, xy, 'Sk_Grundplatte')
    rect(sk, 0.0, -P(sh, 'base_w') / 2, P(sh, 'base_l'), P(sh, 'base_w'), 'gp',
         '0', '-Parameter.base_w / 2', 'Parameter.base_l', 'Parameter.base_w')
    pad(sk, 'Grundplatte', 'Parameter.base_t')

    # 2 Stirnwand bei x = 0, ueber die volle Breite.
    sk = sketch_on(body, xy, 'Sk_Stirnwand')
    rect(sk, 0.0, -P(sh, 'base_w') / 2, P(sh, 'wall_t'), P(sh, 'base_w'), 'sw',
         '0', '-Parameter.base_w / 2', 'Parameter.wall_t', 'Parameter.base_w')
    pad(sk, 'Stirnwand', 'Parameter.wall_h')

    # 3 Rippen: Dreieck in der XZ-Ebene (lokal x = global x, lokal y = global z),
    #   je eines bei y = +rib_y und y = -rib_y, symmetrisch um die Skizzenebene
    #   gezogen. Die Skizzennormale der XZ-Ebene zeigt in FreeCAD nach -y; der
    #   Versatz ist deshalb mit umgekehrtem Vorzeichen eingetragen und wird
    #   unten gegen den Koerper geprueft.
    for tag, sign in (('p', +1), ('m', -1)):
        sk = sketch_on(body, xz, 'Sk_Rippe_' + tag,
                       expr_off='%d * Parameter.rib_y' % (-sign))
        triangle(sk, P(sh, 'wall_t'), P(sh, 'base_t'), P(sh, 'rib_l'),
                 P(sh, 'wall_h') - P(sh, 'base_t'), 'rb' + tag,
                 'Parameter.wall_t', 'Parameter.base_t', 'Parameter.rib_l',
                 'Parameter.wall_h - Parameter.base_t')
        pad(sk, 'Rippe_' + tag, 'Parameter.rib_t', midplane=True)

    # 4 Ausschnitt fuer die Huelse, durch die Wand. Skizze in YZ:
    #   lokal x = global y, lokal y = global z.
    sk = sketch_on(body, yz, 'Sk_Ausschnitt')
    rect(sk, -P(sh, 'cut_w') / 2, P(sh, 'con_z') - P(sh, 'cut_h') / 2,
         P(sh, 'cut_w'), P(sh, 'cut_h'), 'cut',
         '-Parameter.cut_w / 2', 'Parameter.con_z - Parameter.cut_h / 2',
         'Parameter.cut_w', 'Parameter.cut_h')
    pocket_through(sk, 'Ausschnitt')

    # 4a Kabelschlitz: vom Ausschnitt bis ueber die Oberkante der Wand.
    sk = sketch_on(body, yz, 'Sk_Kabelschlitz')
    rect(sk, -P(sh, 'slot_w') / 2, P(sh, 'con_z'),
         P(sh, 'slot_w'), P(sh, 'wall_h') - P(sh, 'con_z') + 1.0, 'slot',
         '-Parameter.slot_w / 2', 'Parameter.con_z',
         'Parameter.slot_w', 'Parameter.wall_h - Parameter.con_z + 1')
    pocket_through(sk, 'Kabelschlitz')

    # 5 Ohrloecher, M3 durch die Wand, auf Hoehe der Ausschnittmitte.
    sk = sketch_on(body, yz, 'Sk_Ohrloecher')
    for tag, sign in (('p', +1), ('m', -1)):
        circle(sk, sign * P(sh, 'con_pitch') / 2, P(sh, 'con_z'),
               P(sh, 'screw_d') / 2, 'ohr' + tag,
               '%d * Parameter.con_pitch / 2' % sign, 'Parameter.con_z',
               'Parameter.screw_d / 2')
    pocket_through(sk, 'Ohrloecher')

    # 6 M6 zentral durch die Grundplatte.
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_d') / 2, 'm6',
           'Parameter.m6_x', '0', 'Parameter.m6_d / 2')
    pocket_through(sk, 'M6_Durchgang')

    # 7 Senkung fuer den Zylinderkopf, von der Oberseite der Grundplatte nach
    #   unten. Die XY-Normale zeigt nach +z, die Tasche geht ohne Reversed
    #   gegen die Normale, also nach unten in die Platte.
    sk = sketch_on(body, xy, 'Sk_M6_Senkung', expr_off='Parameter.base_t')
    circle(sk, P(sh, 'm6_x'), 0.0, P(sh, 'm6_head_d') / 2, 'cb',
           'Parameter.m6_x', '0', 'Parameter.m6_head_d / 2')
    pocket(sk, 'M6_Senkung', 'Parameter.m6_cb_t')

    # 8 Nutnase, nur wenn key_w > 0: Leiste unter der Grundplatte, laengs,
    #   mittig. Breite = Nutbreite des Profils, am Profil gemessen.
    if P(sh, 'key_w') > 0 and P(sh, 'key_h') > 0:
        sk = sketch_on(body, xy, 'Sk_Nutnase')
        rect(sk, 0.0, -P(sh, 'key_w') / 2, P(sh, 'base_l'), P(sh, 'key_w'), 'key',
             '0', '-Parameter.key_w / 2', 'Parameter.base_l', 'Parameter.key_w')
        pad(sk, 'Nutnase', 'Parameter.key_h', reversed_=True)

    # 9 Fasen an den beiden oberen Laengskanten der Grundplatte (hinter den
    #   Rippen frei liegend). Gesucht werden die Kanten laengs x auf z = base_t
    #   an y = +-base_w/2.
    tip = body.Tip
    shape = tip.Shape
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.y - b.y) > 1e-6 or abs(a.z - b.z) > 1e-6:
            continue                                   # nicht laengs x
        if abs(a.z - P(sh, 'base_t')) > 1e-6:
            continue                                   # nicht auf der Oberkante
        if abs(abs(a.y) - P(sh, 'base_w') / 2) > 1e-6:
            continue                                   # nicht aussen
        namen.append('Edge%d' % (i + 1))
    if len(namen) == 2:
        f = body.newObject('PartDesign::Chamfer', 'Fasen')
        f.Base = (tip, namen)
        f.setExpression('Size', 'Parameter.chamfer')
        doc.recompute()
        if 'Invalid' in (f.State or []):
            print('Fasen fehlgeschlagen, bleiben weg')
            body.removeObject(f)
            doc.removeObject(f.Name)
            doc.recompute()
    else:
        print('Laengskanten nicht eindeutig (%s), Fasen bleiben weg' % namen)

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
    bb = s.BoundBox
    ok = True
    # Rippen muessen innerhalb der Grundplattenbreite liegen
    if bb.YLength > P(sh, 'base_w') + 1e-3:
        print('FEHLER Rippen stehen seitlich ueber: Breite %.2f' % bb.YLength); ok = False
    # Rippe: Material bei (wall_t + 1, +-rib_y, base_t + 1)
    for sign in (+1, -1):
        p = Vector(P(sh, 'wall_t') + 1.0, sign * P(sh, 'rib_y'), P(sh, 'base_t') + 1.0)
        if not s.isInside(p, 1e-6, True):
            print('FEHLER keine Rippe bei y = %+.1f' % (sign * P(sh, 'rib_y'))); ok = False
    # Ausschnitt: Luft in der Wandmitte auf Steckerhoehe
    p = Vector(P(sh, 'wall_t') / 2, 0.0, P(sh, 'con_z'))
    if s.isInside(p, 1e-6, True):
        print('FEHLER Ausschnitt fehlt'); ok = False
    # Ohrloch: Luft
    p = Vector(P(sh, 'wall_t') / 2, P(sh, 'con_pitch') / 2, P(sh, 'con_z'))
    if s.isInside(p, 1e-6, True):
        print('FEHLER Ohrloch fehlt'); ok = False
    # Kabelschlitz: Luft an der Oberkante der Wand, mittig
    p = Vector(P(sh, 'wall_t') / 2, 0.0, P(sh, 'wall_h') - 0.5)
    if s.isInside(p, 1e-6, True):
        print('FEHLER Kabelschlitz fehlt'); ok = False
    # Pfosten neben dem Ohrloch: Material zwischen Loch und Schlitz
    web = P(sh, 'con_pitch') / 2 - P(sh, 'screw_d') / 2 - P(sh, 'slot_w') / 2
    print('Steg Ohrloch-Schlitz %.1f mm' % web)
    if web < 2.5:
        print('FEHLER Steg neben dem Ohrloch unter 2,5 mm'); ok = False
    # M6: Luft durch die Platte, Senkung oben, Boden unten
    if s.isInside(Vector(P(sh, 'm6_x'), 0.0, P(sh, 'base_t') / 2), 1e-6, True):
        print('FEHLER M6-Durchgang fehlt'); ok = False
    if s.isInside(Vector(P(sh, 'm6_x'), P(sh, 'm6_head_d') / 2 - 0.5,
                         P(sh, 'base_t') - 0.5), 1e-6, True):
        print('FEHLER Senkung fehlt'); ok = False
    if not s.isInside(Vector(P(sh, 'm6_x'), P(sh, 'm6_head_d') / 2 - 0.5, 0.5),
                      1e-6, True):
        print('FEHLER Senkung geht durch'); ok = False
    # Huelse ueber dem Schraubenkopf: Unterkante Ausschnitt > Kopfoberkante
    head_top = P(sh, 'base_t') - P(sh, 'm6_cb_t') + P(sh, 'm6_head_h')
    cut_bot = P(sh, 'con_z') - P(sh, 'cut_h') / 2
    print('Kopf oben   z = %.1f, Huelse unten z = %.1f, Luft %.1f'
          % (head_top, cut_bot, cut_bot - head_top))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('xt60-halter')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'xt60-halter.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'xt60-halter.step'))
    body.Shape.exportStl(os.path.join(here, 'xt60-halter.stl'))

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
