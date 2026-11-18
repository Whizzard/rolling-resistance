# -*- coding: utf-8 -*-
"""
Gehäuse für die Wägeplatine (SparkFun HX711 Load Cell Amp) am Pendelrahmen.

Aufbau wie hall_gehaeuse.py: ein PartDesign-Body mit Featurebaum plus ein
Spreadsheet "Parameter"; alle Skizzen über Ausdrücke an die Tabelle gebunden.
Maße ändert man im GUI in der Tabelle, nicht in den Skizzen. Maße in mm.

Die Platine ist 30,6 x 23,1 mm und mit Bauteilen 4,3 mm dick, am Stand
nachgemessen. Die Lötpads liegen an den beiden SCHMALSEITEN — links die fünf
Anschlüsse der Wägezelle, rechts VDD/VCC/DAT/CLK/GND zum Rechner. Daraus
folgen zwei Dinge:

  * Der tragende Absatz läuft an den LÄNGSSEITEN. An den Schmalseiten würde er
    auf den Lötstellen sitzen.
  * Über der Platine bleibt Platz (cable_h), weil die Adern von beiden
    Pad-Reihen nach oben gehen, sich über die Platine legen und gemeinsam zu
    EINER Seite hinauslaufen. Deshalb liegen beide Kabelkanäle am selben Ende.

Featurebaum:
    Grundkoerper      Quader 50,6 x 28,9 x 12,3
    Kavitaet          breiter Teil, z = 0...3,6  (Platinendurchgang)
    Bauteilraum       schmaler Teil, volle Höhe  -> erzeugt den Absatz
    Rastnasen         zwei Keile, mittig, unter der Platine
    Federfeld         Freistellung aussen, damit die Wand an den Nasen federt
    Laschen           Kopfblock beidseitig verbreitert
    Laschenloecher    2x Ø 4,5 durch, 6,2 mm von beiden Kanten
    Senkungen         Ø 8 von oben fuer den M4-Kopf
    Kabelnut_p/_m     Klemmstrecke je Kanal, Omega (Kreis mit Anschnitt)
    Kabelmulde_p/_m   tiefer, am Übergang, dort wird nicht geklemmt

Die Klemmstrecke ist 1 mm enger als das Kabel: der Mantel wird zwischen Nut und
Montagefläche eingeklemmt, und DAS ist die Zugentlastung (docs/04-hardware.md §7).
Die Mulde am Übergang ist absichtlich tiefer, damit der Mantel dort, wo das Kabel
nach unten abbiegt, nicht gequetscht wird.

Seitenwand 2,4 mm statt 3,0: bei einer 0,4er Düse sind das genau sechs Bahnen,
also keine dünne Füllnaht in der Mitte. Der **Deckel bleibt bei 2,0 mm** — er
überbrückt die Kavität freitragend und hängt dünner durch. Die Restwand am
Federfeld bleibt 0,8 mm, die Rastnasen federn also unverändert.

Z = 0 ist die offene Unterseite, die auf der Montagefläche liegt.
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
    ('pcb_l',       30.6,  'Platine Laenge, gemessen'),
    ('pcb_w',       23.1,  'Platine Breite, gemessen'),
    ('pcb_t',        1.6,  'Platine Dicke'),
    ('pcb_stack',    4.3,  'Platine mit Bauteilen'),
    ('cable_h',      4.0,  'Platz ueber den Bauteilen fuer die Adern'),
    ('solder_gap',   2.0,  'Loetraum unter der Platine'),
    ('headroom', '=pcb_stack - pcb_t + cable_h', 'Bauteilraum ueber der Platine'),

    ('wall',         2.4,  'Seitenwand, 6 Bahnen bei 0,4er Duese'),
    ('top',          2.0,  'Deckelwand, bleibt bei 2,0 (freie Spannweite)'),
    ('end_wall',     2.0,  'Stirnwand am Zellenende'),
    ('rib',          2.0,  'Steg zur Schraube'),
    ('screw_blk',   14.0,  'Laenge des massiven Blocks'),

    ('cav_l',  '=pcb_l + 2',                     'Kavitaet Laenge'),
    ('cav_w',  '=pcb_w + 1',                     'Kavitaet Breite'),
    ('cav_h',  '=solder_gap + pcb_t + headroom', 'Kavitaet Hoehe'),
    ('cav_x0', '=end_wall',                      'Kavitaet Anfang'),
    ('cav_xm', '=cav_x0 + cav_l / 2',            'Kavitaet Mitte'),

    ('l_out',  '=end_wall + cav_l + rib + screw_blk', 'Aussenlaenge'),
    ('w_out',  '=cav_w + 2 * wall',                   'Aussenbreite'),
    ('h_out',  '=cav_h + top',                        'Aussenhoehe'),

    ('pcb_z0', '=solder_gap',          'Unterkante Platine'),
    ('pcb_z1', '=pcb_z0 + pcb_t',      'Oberkante Platine = Absatz'),

    ('ledge_w',      1.8,  'Absatz je Laengsseite'),
    ('snap_w',       0.65, 'Rastnase Vorsprung je Seite'),
    ('snap_l',       6.0,  'Rastnase Laenge'),
    ('snap_h',       0.8,  'Rastnase halbe Hoehe'),
    ('snap_z', '=pcb_z0',  'Scheitel der Rastnase'),

    ('relief_l',    14.0,  'Federfeld Laenge'),
    ('relief_t',     0.8,  'Federfeld Restwand'),
    ('relief_h', '=pcb_z1 + 2', 'Federfeld Hoehe'),

    # Befestigung: zwei Laschen an der Kopfseite, je eine Spange darunter
    # (hx711_spange.py). Die mittige M6 ist dadurch entfallen.
    ('lug_hole_d',   4.5,  'Durchgang fuer M4 in die Spange'),
    ('lug_edge',     6.2,  'Lochmitte bis Aussenkante, Vorgabe'),
    ('lug_clear',    2.5,  'Material zwischen Loch und Kabelmulde'),
    ('lug_y',  '=cab_y + cab_deep_d / 2 + lug_clear + lug_hole_d / 2',
               'Lochmitte in y'),
    ('lug_y_out', '=lug_y + lug_edge', 'Aussenkante der Lasche'),
    ('lug_x',  '=l_out - lug_edge',    'Lochmitte in x, 6,2 auch zur Stirn'),
    ('lug_l',  '=2 * lug_edge',        'Laschenlaenge, Loch mittig darin'),
    ('lug_cb_d',     8.0,  'Senkung fuer den M4-Kopf'),
    ('lug_cb_depth', 6.3,  'Senkungstiefe: Klemmweg 13,0 -> M4 x 14 mit 1 mm Luft'),

    ('cab_cable_d',  4.0,  'Kabel-Durchmesser, gemessen'),
    ('cab_d',        3.9,  'Nutdurchmesser, 0,1 unter dem Kabel -> klemmt'),
    ('cab_mouth',    3.4,  'Maulweite am Anschnitt -> Kralle, Omega statt U'),
    ('cab_z', '=sqrt(cab_d / 2 * cab_d / 2 - cab_mouth / 2 * cab_mouth / 2)',
              'Kreismitte ueber der offenen Seite, aus dem Anschnitt'),
    ('cab_depth', '=cab_z + cab_d / 2', 'Nuttiefe, ergibt sich'),
    ('cab_y',        4.5,  'Kanalmitte, Betrag — je einmal +y und -y'),
    ('cab_x0', '=cav_x0 + cav_l', 'Kanalanfang, am Ende der Kavitaet'),
    ('cab_len','=l_out - cab_x0 + 1', 'Kanallaenge'),
    ('cab_deep_d',   4.6,  'Mulde Breite, UEBER dem Kabel -> keine Klemmung'),
    ('cab_deep',     5.2,  'Mulde Tiefe, keine Klemmung'),
    ('cab_deep_x0', '=cab_x0', 'Mulde Anfang'),
    ('cab_deep_l',   5.0,  'Mulde Laenge'),
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


def wedge(sk, sign, yw, zc, dw, dh, tag):
    """Symmetrischer Keil an einer Wand. Skizze in YZ: lokal x = global y."""
    v = Vector
    x_wall = sign * yw
    x_tip = x_wall - sign * dw
    p_bot = v(x_wall, zc - dh, 0)
    p_tip = v(x_tip, zc, 0)
    p_top = v(x_wall, zc + dh, 0)
    g = [sk.addGeometry(Part.LineSegment(p_bot, p_tip), False),
         sk.addGeometry(Part.LineSegment(p_tip, p_top), False),
         sk.addGeometry(Part.LineSegment(p_top, p_bot), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x_wall), tag + '_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, zc - dh), tag + '_z')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[1], 2, 2 * dh), tag + '_hh')
    named(sk, Sketcher.Constraint('DistanceX', g[0], 1, g[0], 2, -sign * dw), tag + '_dw')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[0], 2, dh), tag + '_dh')
    s = '%d' % sign
    sk.setExpression('Constraints.%s_x' % tag, '%s * (Parameter.cav_w / 2)' % s)
    sk.setExpression('Constraints.%s_z' % tag, 'Parameter.snap_z - Parameter.snap_h')
    sk.setExpression('Constraints.%s_hh' % tag, '2 * Parameter.snap_h')
    sk.setExpression('Constraints.%s_dw' % tag, '%d * Parameter.snap_w' % -sign)
    sk.setExpression('Constraints.%s_dh' % tag, 'Parameter.snap_h')
    return g


# ------------------------------------------------------------------- Modell

def sketch_on(body, plane, name, x_off=0.0):
    sk = body.newObject('Sketcher::SketchObject', name)
    try:
        sk.AttachmentSupport = [(plane, '')]
    except Exception:
        sk.Support = [(plane, '')]
    sk.MapMode = 'FlatFace'
    if x_off:
        sk.AttachmentOffset = App.Placement(Vector(0, 0, x_off), App.Rotation())
    return sk


def build(doc, sh):
    body = doc.addObject('PartDesign::Body', 'Gehaeuse')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy, yz = planes['XY_Plane'], planes['YZ_Plane']

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

    # 1 Grundkörper
    sk = sketch_on(body, xy, 'Sk_Aussen')
    rect(sk, 0.0, -P(sh, 'w_out') / 2, P(sh, 'l_out'), P(sh, 'w_out'), 'aussen',
         '0', '-Parameter.w_out / 2', 'Parameter.l_out', 'Parameter.w_out')
    pad(sk, 'Grundkoerper', 'Parameter.h_out')

    # 2 Kavität, breiter Teil bis Platinenoberkante
    sk = sketch_on(body, xy, 'Sk_Kavitaet')
    rect(sk, P(sh, 'cav_x0'), -P(sh, 'cav_w') / 2, P(sh, 'cav_l'), P(sh, 'cav_w'), 'kav',
         'Parameter.cav_x0', '-Parameter.cav_w / 2', 'Parameter.cav_l', 'Parameter.cav_w')
    pocket(sk, 'Kavitaet', 'Parameter.pcb_z1')

    # 3 Bauteilraum, schmaler Teil durch die volle Höhe → erzeugt den Absatz.
    #   Schmaler nur in y: der Absatz liegt an den Längsseiten, weg von den Pads.
    inner = P(sh, 'cav_w') - 2 * P(sh, 'ledge_w')
    sk = sketch_on(body, xy, 'Sk_Bauteilraum')
    rect(sk, P(sh, 'cav_x0'), -inner / 2, P(sh, 'cav_l'), inner, 'btr',
         'Parameter.cav_x0', '-(Parameter.cav_w - 2 * Parameter.ledge_w) / 2',
         'Parameter.cav_l', 'Parameter.cav_w - 2 * Parameter.ledge_w')
    pocket(sk, 'Bauteilraum', 'Parameter.cav_h')

    # 4 Rastnasen
    sk = sketch_on(body, yz, 'Sk_Rastnasen',
                   P(sh, 'cav_xm') - P(sh, 'snap_l') / 2)
    sk.setExpression('AttachmentOffset.Base.z',
                     'Parameter.cav_xm - Parameter.snap_l / 2')
    for sign in (+1, -1):
        wedge(sk, sign, P(sh, 'cav_w') / 2, P(sh, 'snap_z'),
              P(sh, 'snap_w'), P(sh, 'snap_h'),
              'nase_p' if sign > 0 else 'nase_m')
    pad(sk, 'Rastnasen', 'Parameter.snap_l')

    # 5 Federfeld
    depth = P(sh, 'w_out') / 2 - P(sh, 'cav_w') / 2 - P(sh, 'relief_t')
    e_depth = 'Parameter.w_out / 2 - Parameter.cav_w / 2 - Parameter.relief_t'
    e_in = 'Parameter.cav_w / 2 + Parameter.relief_t'
    y_in = P(sh, 'cav_w') / 2 + P(sh, 'relief_t')
    x0 = P(sh, 'cav_xm') - P(sh, 'relief_l') / 2
    e_x0 = 'Parameter.cav_xm - Parameter.relief_l / 2'
    sk = sketch_on(body, xy, 'Sk_Federfeld')
    rect(sk, x0, y_in, P(sh, 'relief_l'), depth, 'relp',
         e_x0, e_in, 'Parameter.relief_l', e_depth)
    rect(sk, x0, -P(sh, 'w_out') / 2, P(sh, 'relief_l'), depth, 'relm',
         e_x0, '-Parameter.w_out / 2', 'Parameter.relief_l', e_depth)
    pocket(sk, 'Federfeld', 'Parameter.relief_h')

    # 6 Laschen: der Kopfblock wird beidseitig verbreitert, damit das Loch
    #   die vorgegebenen 6,2 mm Randabstand bekommt. Volle Hoehe, weil das
    #   Loch durch die ganze Hoehe geht und sonst oberhalb der Lasche nur
    #   1 mm Material neben dem Loch bliebe.
    e_ll = 'Parameter.l_out - Parameter.lug_l'
    e_lb = 'Parameter.lug_y_out - Parameter.w_out / 2'
    x0l = P(sh, 'l_out') - P(sh, 'lug_l')
    bl  = P(sh, 'lug_y_out') - P(sh, 'w_out') / 2
    sk = sketch_on(body, xy, 'Sk_Laschen')
    rect(sk, x0l, P(sh, 'w_out') / 2, P(sh, 'lug_l'), bl, 'lasp',
         e_ll, 'Parameter.w_out / 2', 'Parameter.lug_l', e_lb)
    rect(sk, x0l, -P(sh, 'lug_y_out'), P(sh, 'lug_l'), bl, 'lasm',
         e_ll, '-Parameter.lug_y_out', 'Parameter.lug_l', e_lb)
    pad(sk, 'Laschen', 'Parameter.h_out')

    # 7 Durchgangsloecher
    sk = sketch_on(body, xy, 'Sk_Laschenloecher')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        circle(sk, P(sh, 'lug_x'), sign * P(sh, 'lug_y'),
               P(sh, 'lug_hole_d') / 2, 'lh' + t,
               'Parameter.lug_x', '%d * Parameter.lug_y' % sign,
               'Parameter.lug_hole_d / 2')
    pocket(sk, 'Laschenloecher', 'Parameter.h_out + 1')

    # 8 Senkungen von oben, damit eine M4 x 12 reicht und der Kopf nicht aufsteht
    #   Die Skizze liegt auf der Deckelebene: eine XY-Skizze bei z = 0 wuerde
    #   mit Reversed nach OBEN schneiden, also von der offenen Unterseite her.
    sk = sketch_on(body, xy, 'Sk_Senkungen', P(sh, 'h_out'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.h_out')
    for sign, t in ((+1, 'p'), (-1, 'm')):
        circle(sk, P(sh, 'lug_x'), sign * P(sh, 'lug_y'),
               P(sh, 'lug_cb_d') / 2, 'cb' + t,
               'Parameter.lug_x', '%d * Parameter.lug_y' % sign,
               'Parameter.lug_cb_d / 2')
    pocket(sk, 'Senkungen', 'Parameter.lug_cb_depth', reversed_=False)

    # 7...10 Die zwei Kabelkanäle, je D-Profil (Rechteck + Rundung).
    #   Erst die Klemmstrecke über die ganze Länge, dann die tiefere Mulde am
    #   Übergang, wo das Kabel aus dem Gehäuse nach unten abbiegt.
    rm = P(sh, 'cab_deep_d') / 2
    e_rm = 'Parameter.cab_deep_d / 2'
    for sign, t in ((+1, 'p'), (-1, 'm')):
        e_y = '%d * Parameter.cab_y' % sign
        e_xm = '%d * Parameter.cab_y - Parameter.cab_deep_d / 2' % sign
        y = sign * P(sh, 'cab_y')

        # Klemmstrecke: EIN Kreis, dessen Mitte ueber der offenen Seite liegt.
        # Der Anschnitt an z = 0 ist dadurch enger als der Kreis -> Kralle.
        sk = sketch_on(body, yz, 'Sk_Kabelnut_' + t, P(sh, 'cab_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
        circle(sk, y, P(sh, 'cab_z'), P(sh, 'cab_d') / 2, 'nut' + t,
               e_y, 'Parameter.cab_z', 'Parameter.cab_d / 2')
        pocket(sk, 'Kabelnut_' + t, 'Parameter.cab_len')

        # Mulde: U, weiter als das Kabel. Hier wird absichtlich nicht geklemmt,
        # hier biegt das Kabel ab.
        sk = sketch_on(body, yz, 'Sk_Kabelmulde_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        rect(sk, y - rm, 0.0, P(sh, 'cab_deep_d'), P(sh, 'cab_deep') - rm, 'mul' + t,
             e_xm, '0', 'Parameter.cab_deep_d',
             'Parameter.cab_deep - Parameter.cab_deep_d / 2')
        pocket(sk, 'Kabelmulde_' + t, 'Parameter.cab_deep_l')

        sk = sketch_on(body, yz, 'Sk_Kabelmulde_rund_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        circle(sk, y, P(sh, 'cab_deep') - rm, rm, 'mulr' + t,
               e_y, 'Parameter.cab_deep - Parameter.cab_deep_d / 2', e_rm)
        pocket(sk, 'Kabelmulde_rund_' + t, 'Parameter.cab_deep_l')

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


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hx711-gehaeuse')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'hx711-gehaeuse.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'hx711-gehaeuse.step'))
    body.Shape.exportStl(os.path.join(here, 'hx711-gehaeuse.stl'))

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
