# -*- coding: utf-8 -*-
"""
Gehäuse für die Wägeplatine (SparkFun HX711 Load Cell Amp) am Pendelrahmen.

Aufbau wie hall_gehaeuse.py: ein PartDesign-Body mit Featurebaum plus ein
Spreadsheet "Parameter"; alle Skizzen über Ausdrücke an die Tabelle gebunden.
Maße ändert man im GUI in der Tabelle, nicht in den Skizzen. Maße in mm.

Die Platine ist 30,6 x 23,0 mm und mit Bauteilen 4,3 mm dick, am Stand
nachgemessen (Nennmass laut Eagle: 30,48 x 22,86 = 1,2 x 0,9 Zoll). Das Spiel
steht als `fit_w` / `fit_l` / `fit_t` ausdruecklich in der Tabelle — nicht als
Aufschlag in der Formel versteckt. `fit_t` ist neu: der Abstand zwischen dem
Scheitel der Rastnasen und der Unterkante des Absatzes war vorher GENAU pcb_t,
also null Luft. Eine reale 1,6er Platine mit ihrer Dickentoleranz liess sich
deshalb nicht einrasten. Jetzt sitzt der Scheitel fit_t tiefer, die Platine
darf also bis zu 0,3 mm tiefer stehen. Die Lötpads liegen an den beiden SCHMALSEITEN — links die fünf
Anschlüsse der Wägezelle, rechts VDD/VCC/DAT/CLK/GND zum Rechner. Daraus
folgen zwei Dinge:

  * Der tragende Absatz läuft an den LÄNGSSEITEN. An den Schmalseiten würde er
    auf den Lötstellen sitzen.
  * Der Absatz ist UNSYMMETRISCH, und das ist der Kern der Fassung vom
    2026-11-21. Aus den Eagle-Daten von SparkFun (Repo sparkfun/HX711-Load-Cell-
    Amplifier, hardware/SparkFun_HX711_Load_Cell.brd, Umriss 30,48 x 22,86 mm):

        Kondensatorseite   C6 und C7, zwei 10 uF in Bauform 1210, Mitte
                           2,54 mm von der Kante, Gehaeuse bis 1,24 mm an die
                           Kante. Dazwischen C3 und C5 (0603) bis 2,04 mm.
        freie Seite        naechstes Bauteil erst bei 3,95 mm (R4, C1).

    Der alte, umlaufende Absatz von 1,8 mm griff also 1,8 mm weit auf die
    Platine und traf die beiden 1210er -- beim ersten Zusammenbau sind sie
    abgebrochen. Jetzt: ledge_cap = 0,8 mm auf der Kondensatorseite (+y),
    ledge_free = 2,8 mm auf der freien Seite (-y).

    In den ECKEN der Kondensatorseite ist der Absatz wieder voll breit: entlang
    dieser Kante sitzt das erste Kupfer bei x = 6,22 mm (Pad von C3) und das
    letzte endet bei 24,68 mm (Pad von C6), die Ecken sind also frei. Der
    Schmalstreifen laeuft nur dazwischen.

    Die Platine passt damit nur noch in EINER Lage hinein. Das ist Absicht:
    verkehrt herum stossen die Kondensatoren an die breiten Eckauflagen, und man
    merkt es, bevor etwas bricht.
  * Über der Platine bleibt Platz (cable_h), weil die Adern von beiden
    Pad-Reihen nach oben gehen, sich über die Platine legen und gemeinsam zu
    EINER Seite hinauslaufen. Deshalb liegen beide Kabelkanäle am selben Ende.

Featurebaum:
    Grundkoerper      Quader 50,6 x 28,9 x 12,3
    Kavitaet          breiter Teil, z = 0...3,6  (Platinendurchgang)
    Bauteilraum       schmaler Teil, volle Höhe  -> erzeugt den Absatz
    Eckauflagen       Absatz in den vier Ecken wieder voll breit
    Rastnasen         zwei Keile, mittig, unter der Platine
    Federfeld         Freistellung aussen, damit die Wand an den Nasen federt
    Schraubloch       M6 durch, mittig zwischen den Kanälen
    Kabelhals_p/_m    enger Hals an der offenen Seite — die Kralle
    Kabelnut_p/_m     Kreis darueber, steht ueber den Hals hinaus
    Maulfasen         vier kleine Fasen an den Maulkanten
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
import math

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
import Sketcher
from FreeCAD import Vector

# --------------------------------------------------------------- Parameter
# (Name, Wert oder Formel, Kommentar). Formeln beginnen mit '='.
PARAMS = [
    ('pcb_l',       30.6,  'Platine Laenge, gemessen'),
    ('pcb_w',       23.0,  'Platine Breite, gemessen'),
    ('fit_w',        0.4,  'Spiel in der Breite, GESAMT -> 0,2 je Seite'),
    ('fit_l',        0.6,  'Spiel in der Laenge, GESAMT -> 0,3 je Ende'),
    ('pcb_t',        1.6,  'Platine Dicke'),
    ('fit_t',        0.3,  'Spiel in der Dicke -> Platine darf tiefer sitzen'),
    ('pcb_stack',    4.3,  'Platine mit Bauteilen'),
    ('cable_h',      4.0,  'Platz ueber den Bauteilen fuer die Adern'),
    ('solder_gap',   2.0,  'Loetraum unter der Platine'),
    ('headroom', '=pcb_stack - pcb_t + cable_h', 'Bauteilraum ueber der Platine'),

    ('wall',         2.4,  'Seitenwand, 6 Bahnen bei 0,4er Duese'),
    ('top',          2.0,  'Deckelwand, bleibt bei 2,0 (freie Spannweite)'),
    ('end_wall',     2.0,  'Stirnwand am Zellenende'),
    ('rib',          2.0,  'Steg zur Schraube'),
    ('screw_blk',   14.0,  'Laenge des massiven Blocks'),

    ('cav_l',  '=pcb_l + fit_l',                 'Kavitaet Laenge'),
    ('cav_w',  '=pcb_w + fit_w',                 'Kavitaet Breite'),
    ('cav_h',  '=solder_gap + pcb_t + headroom', 'Kavitaet Hoehe'),
    ('cav_x0', '=end_wall',                      'Kavitaet Anfang'),
    ('cav_xm', '=cav_x0 + cav_l / 2',            'Kavitaet Mitte'),

    ('l_out',  '=end_wall + cav_l + rib + screw_blk', 'Aussenlaenge'),
    ('w_out',  '=cav_w + 2 * wall',                   'Aussenbreite'),
    ('h_out',  '=cav_h + top',                        'Aussenhoehe'),

    ('pcb_z0', '=solder_gap',          'Unterkante Platine'),
    ('pcb_z1', '=pcb_z0 + pcb_t',      'Oberkante Platine = Absatz'),

    ('ledge_cap',    0.8,  'Absatz KONDENSATORSEITE (+y) -- Bauteile bis 1,24 mm an die Kante'),
    ('ledge_free',   2.8,  'Absatz freie Laengsseite (-y) -- dort 3,9 mm frei'),
    ('ledge_corner', 2.8,  'Absatz in den ECKEN der Kondensatorseite'),
    ('corner_l',     5.0,  'Laenge der Eckauflage je Ende'),
    ('ledge_chamfer', 0.4, 'Fase an der Unterkante des Absatzes, innen'),
    ('ramp_l',       4.0,  'Laenge der Kabelrampe am Kanalende'),
    ('ramp_h',       1.5,  'Hoehe der Kabelrampe ueber der Platinenoberkante'),
    ('snap_w',       0.65, 'Rastnase Vorsprung je Seite'),
    ('snap_l',       6.0,  'Rastnase Laenge'),
    ('snap_h',       0.8,  'Rastnase halbe Hoehe'),
    ('snap_z', '=pcb_z1 - pcb_t - fit_t', 'Scheitel der Rastnase, fit_t unter der Nennlage'),

    ('relief_l',    14.0,  'Federfeld Laenge'),
    ('relief_t',     0.8,  'Federfeld Restwand'),
    ('relief_h', '=pcb_z1 + 2', 'Federfeld Hoehe'),

    ('screw_d',      6.6,  'M6 Durchgang, wie das Hall-Gehaeuse'),
    ('screw_y',      0.0,  'Schraube mittig, zwischen den Kanaelen'),
    ('screw_x', '=cav_x0 + cav_l + rib + screw_blk / 2', 'Schraube Lage'),

    # Gemeinsam fuer beide Kanaele
    ('cab_chamfer',  0.3,  'Fase am Nutmaul, Einfuehrhilfe fuers Kabel'),
    ('cab_y',        8.6,  'Kanalmitte, Betrag — je einmal +y und -y'),
    ('cab_x0', '=cav_x0 + cav_l', 'Kanalanfang, am Ende der Kavitaet'),
    ('cab_len','=l_out - cab_x0 + 1', 'Kanallaenge'),
    ('cab_deep_x0', '=cab_x0', 'Mulde Anfang'),
    ('cab_deep_l',   5.0,  'Mulde Laenge'),

    # Kanal +y — Leitung zum Rechner, fertiger Mantel
    ('cab_cable_p',  4.0,  'Kabel-Durchmesser, GEMESSEN'),
    ('cab_d_p',      3.9,  'Nutdurchmesser, 0,1 unter dem Kabel -> klemmt'),
    ('cab_mouth_p',  3.2,  'Halsweite an der offenen Seite -> Kralle'),
    ('cab_depth_p',  3.4,  'Nuttiefe, 0,6 unter dem Kabel -> Flaeche klemmt'),
    ('cab_deep_d_p', 4.6,  'Mulde Breite, UEBER dem Kabel -> keine Klemmung'),
    ('cab_deep_p',   5.2,  'Mulde Tiefe, keine Klemmung'),
    ('cab_z_p', '=cab_depth_p - cab_d_p / 2', 'Kreismitte ueber der offenen Seite'),
    ('cab_claw_p', '=(cab_d_p - cab_mouth_p) / 2', 'Hinterschnitt je Seite'),

    # Kanal -y — die vier duennen Litzen der Waegezelle plus Schrumpfschlauch.
    # 3,0 mm ist GESCHAETZT, nicht gemessen: 4 Adern AWG 28...30 mit 0,7...1,0 mm
    # Aussendurchmesser ergeben im Quadratbuendel (1+sqrt(2))*d = 1,7...2,4 mm,
    # dazu duennwandiger 2:1-Schrumpfschlauch mit 0,4...0,5 mm je Seite.
    # Macht 2,5...3,4 mm. Vor dem Druck mit dem Messschieber nachmessen.
    # Die Klemmung ist hier SANFTER (0,4 statt 0,6 unter dem Buendel): duenne
    # Litzen in Schrumpfschlauch sind weich, und 0,6 wuerden sie plattdruecken.
    ('cab_cable_m',  3.0,  'Buendel aus 4 Litzen + Schrumpfschlauch, GESCHAETZT'),
    ('cab_d_m',      2.9,  'Nutdurchmesser, 0,1 unter dem Buendel'),
    ('cab_mouth_m',  2.4,  'Halsweite -> Kralle 0,25 je Seite'),
    ('cab_depth_m',  2.6,  'Nuttiefe, 0,4 unter dem Buendel'),
    ('cab_deep_d_m', 3.6,  'Mulde Breite, UEBER dem Buendel'),
    ('cab_deep_m',   4.2,  'Mulde Tiefe, keine Klemmung'),
    ('cab_z_m', '=cab_depth_m - cab_d_m / 2', 'Kreismitte ueber der offenen Seite'),
    ('cab_claw_m', '=(cab_d_m - cab_mouth_m) / 2', 'Hinterschnitt je Seite'),
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
    #   UNSYMMETRISCH, siehe Kopf: +y ist die Kondensatorseite und bekommt nur
    #   ledge_cap, -y ist frei und trägt mit ledge_free den Hauptteil.
    y0 = -(P(sh, 'cav_w') / 2 - P(sh, 'ledge_free'))
    inner = P(sh, 'cav_w') - P(sh, 'ledge_free') - P(sh, 'ledge_cap')
    sk = sketch_on(body, xy, 'Sk_Bauteilraum')
    rect(sk, P(sh, 'cav_x0'), y0, P(sh, 'cav_l'), inner, 'btr',
         'Parameter.cav_x0', '-(Parameter.cav_w / 2 - Parameter.ledge_free)',
         'Parameter.cav_l',
         'Parameter.cav_w - Parameter.ledge_free - Parameter.ledge_cap')
    pocket(sk, 'Bauteilraum', 'Parameter.cav_h')

    # 3a Eckauflagen: auf der Kondensatorseite ist der Absatz nur dort schmal,
    #    wo wirklich Bauteile stehen. Entlang der Kante sitzt das erste Kupfer bei
    #    x = 6,22 mm (Pad von C3) und das letzte endet bei x = 24,68 mm (Pad von
    #    C6), bezogen auf die Platinenkante. In den Ecken ist also Platz, und dort
    #    bekommt die Platine ihre Auflage zurueck -- vier Punkte halten sie besser
    #    als ein durchgehender 0,8-mm-Streifen.
    ycorner = P(sh, 'cav_w') / 2 - P(sh, 'ledge_corner')
    e_ycorner = 'Parameter.cav_w / 2 - Parameter.ledge_corner'
    sk = sketch_on(body, xy, 'Sk_Eckauflagen', P(sh, 'pcb_z1'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.pcb_z1')
    rect(sk, P(sh, 'cav_x0'), ycorner, P(sh, 'corner_l'), P(sh, 'ledge_corner'), 'eck_a',
         'Parameter.cav_x0', e_ycorner, 'Parameter.corner_l', 'Parameter.ledge_corner')
    rect(sk, P(sh, 'cav_x0') + P(sh, 'cav_l') - P(sh, 'corner_l'), ycorner,
         P(sh, 'corner_l'), P(sh, 'ledge_corner'), 'eck_b',
         'Parameter.cav_x0 + Parameter.cav_l - Parameter.corner_l', e_ycorner,
         'Parameter.corner_l', 'Parameter.ledge_corner')
    pad(sk, 'Eckauflagen', 'Parameter.headroom')

    # 3b Fase an der Unterkante des Absatzes, innen umlaufend.
    #
    #    Dort treffen Platinenoberkante und Absatz aufeinander, beide bei z =
    #    pcb_z1, und zwar als 90-Grad-Kante. Eine Ader, die sich dorthin verirrt,
    #    wird beim Einschieben der Platine abgeschert. Die Fase macht daraus
    #    einen Keil: die Ader wird nach innen geschoben, statt gequetscht.
    tip = body.Tip
    namen = []
    yg = P(sh, 'cav_w') / 2 - 0.01
    xa, xb = P(sh, 'cav_x0') - 0.01, P(sh, 'cav_x0') + P(sh, 'cav_l') + 0.01
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        vs = e.Vertexes
        if any(abs(v.Point.z - P(sh, 'pcb_z1')) > 1e-6 for v in vs):
            continue
        if any(abs(v.Point.y) > yg for v in vs):
            continue
        if any(not (xa <= v.Point.x <= xb) for v in vs):
            continue
        namen.append('Edge%d' % (i + 1))
    if len(namen) < 3:
        raise RuntimeError('Absatzkanten nicht gefunden: %s' % namen)
    f = body.newObject('PartDesign::Chamfer', 'Auflagefasen')
    f.Base = (tip, namen)
    f.setExpression('Size', 'Parameter.ledge_chamfer')
    doc.recompute()

    # 3c Kabelrampe: am Kanalende steigt die Unterseite des Absatzes an.
    #
    #    Hier verlassen die Adern die Platine und tauchen in die Kabelkanaele
    #    ab -- und hier liegt seit den Eckauflagen Material ueber ihnen. Ohne
    #    die Rampe klemmt die Platine beim Einschieben genau hier die Litzen
    #    ein. Mit ihr oeffnet sich der Spalt keilfoermig auf ramp_h, und die
    #    Adern laufen hinaus statt hinein.
    #
    #    Skizze in XZ: lokal x = global x, lokal y = global z, Normale -Y.
    #    Der Schnitt geht symmetrisch ueber die ganze Kavitaetsbreite, die
    #    Seitenwaende bleiben stehen.
    x1 = P(sh, 'cav_x0') + P(sh, 'cav_l')
    z0 = P(sh, 'pcb_z1')
    sk = body.newObject('Sketcher::SketchObject', 'Sk_Kabelrampe')
    try:
        sk.AttachmentSupport = [(planes['XZ_Plane'], '')]
    except Exception:
        sk.Support = [(planes['XZ_Plane'], '')]
    sk.MapMode = 'FlatFace'
    v = Vector
    g = [sk.addGeometry(Part.LineSegment(v(x1, z0, 0), v(x1, z0 + P(sh, 'ramp_h'), 0)), False),
         sk.addGeometry(Part.LineSegment(v(x1, z0 + P(sh, 'ramp_h'), 0),
                                         v(x1 - P(sh, 'ramp_l'), z0, 0)), False),
         sk.addGeometry(Part.LineSegment(v(x1 - P(sh, 'ramp_l'), z0, 0), v(x1, z0, 0)), False)]
    for i in range(3):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % 3], 1))
    sk.addConstraint(Sketcher.Constraint('Vertical', g[0]))
    sk.addConstraint(Sketcher.Constraint('Horizontal', g[2]))
    named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[0], 1, x1), 'rampe_x')
    named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[0], 1, z0), 'rampe_z')
    named(sk, Sketcher.Constraint('DistanceY', g[0], 1, g[0], 2, P(sh, 'ramp_h')), 'rampe_h')
    named(sk, Sketcher.Constraint('DistanceX', g[2], 1, g[2], 2, P(sh, 'ramp_l')), 'rampe_l')
    sk.setExpression('Constraints.rampe_x', 'Parameter.cav_x0 + Parameter.cav_l')
    sk.setExpression('Constraints.rampe_z', 'Parameter.pcb_z1')
    sk.setExpression('Constraints.rampe_h', 'Parameter.ramp_h')
    sk.setExpression('Constraints.rampe_l', 'Parameter.ramp_l')
    fr = body.newObject('PartDesign::Pocket', 'Kabelrampe')
    fr.Profile = sk
    fr.Midplane = True
    fr.setExpression('Length', 'Parameter.cav_w')
    doc.recompute()

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

    # 6 Schraubloch, mittig — die beiden Kanäle liegen rechts und links davon
    sk = sketch_on(body, xy, 'Sk_Schraubloch')
    circle(sk, P(sh, 'screw_x'), P(sh, 'screw_y'), P(sh, 'screw_d') / 2, 'schr',
           'Parameter.screw_x', 'Parameter.screw_y', 'Parameter.screw_d / 2')
    pocket(sk, 'Schraubloch', 'Parameter.h_out + 1')

    # 7...10 Die zwei Kabelkanäle, je D-Profil (Rechteck + Rundung).
    #   Erst die Klemmstrecke über die ganze Länge, dann die tiefere Mulde am
    #   Übergang, wo das Kabel aus dem Gehäuse nach unten abbiegt.
    #   BEIDE KANAELE HABEN EIGENE MASSE: +y fuehrt die fertige Leitung zum
    #   Rechner, -y nur die vier duennen Litzen der Waegezelle im
    #   Schrumpfschlauch. Ein Kanal fuer beides waere fuer das Buendel zu weit,
    #   und dann klemmt nichts.
    for sign, t in ((+1, 'p'), (-1, 'm')):
        rm = P(sh, 'cab_deep_d_' + t) / 2
        e_rm = 'Parameter.cab_deep_d_%s / 2' % t
        e_y = '%d * Parameter.cab_y' % sign
        e_xm = '%d * Parameter.cab_y - Parameter.cab_deep_d_%s / 2' % (sign, t)
        y = sign * P(sh, 'cab_y')

        # Klemmstrecke, Omega: ein HALS, der enger ist als der Kreis darueber.
        # Der Kreis steht je Seite um cab_claw ueber den Hals hinaus — das ist
        # der Hinterschnitt, der das Kabel haelt, bevor die Flaeche darunter ist.
        # Ein blosser Anschnitt des Kreises koennte das nicht: ein enges Maul
        # zoege zwangslaeufig eine tiefe Nut nach sich, und die Nut muss bei
        # cab_depth bleiben, damit die Flaeche noch klemmt.
        e_hals = '%d * Parameter.cab_y - Parameter.cab_mouth_%s / 2' % (sign, t)
        sk = sketch_on(body, yz, 'Sk_Kabelhals_' + t, P(sh, 'cab_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
        rect(sk, y - P(sh, 'cab_mouth_' + t) / 2, 0.0,
             P(sh, 'cab_mouth_' + t), P(sh, 'cab_z_' + t), 'hals' + t,
             e_hals, '0', 'Parameter.cab_mouth_%s' % t, 'Parameter.cab_z_%s' % t)
        pocket(sk, 'Kabelhals_' + t, 'Parameter.cab_len')

        sk = sketch_on(body, yz, 'Sk_Kabelnut_' + t, P(sh, 'cab_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
        circle(sk, y, P(sh, 'cab_z_' + t), P(sh, 'cab_d_' + t) / 2, 'nut' + t,
               e_y, 'Parameter.cab_z_%s' % t, 'Parameter.cab_d_%s / 2' % t)
        pocket(sk, 'Kabelnut_' + t, 'Parameter.cab_len')

        # Mulde: U, weiter als das Kabel. Hier wird absichtlich nicht geklemmt,
        # hier biegt das Kabel ab.
        sk = sketch_on(body, yz, 'Sk_Kabelmulde_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        rect(sk, y - rm, 0.0, P(sh, 'cab_deep_d_' + t),
             P(sh, 'cab_deep_' + t) - rm, 'mul' + t,
             e_xm, '0', 'Parameter.cab_deep_d_%s' % t,
             'Parameter.cab_deep_%s - Parameter.cab_deep_d_%s / 2' % (t, t))
        pocket(sk, 'Kabelmulde_' + t, 'Parameter.cab_deep_l')

        sk = sketch_on(body, yz, 'Sk_Kabelmulde_rund_' + t, P(sh, 'cab_deep_x0'))
        sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
        circle(sk, y, P(sh, 'cab_deep_' + t) - rm, rm, 'mulr' + t,
               e_y, 'Parameter.cab_deep_%s - Parameter.cab_deep_d_%s / 2' % (t, t), e_rm)
        pocket(sk, 'Kabelmulde_rund_' + t, 'Parameter.cab_deep_l')

    # 11 Fase am Nutmaul, vier Kanten (zwei je Kanal).
    #
    #   Das Kabel muss durch den engeren Hals gedrueckt werden; ohne Fase fasst
    #   die scharfe Kante in den Mantel. Die runde Form bleibt unberuehrt — die
    #   Fase sitzt UNTER dem Hinterschnitt und nimmt von der Kralle nichts weg.
    #
    #   Dafuer gibt es eine harte Grenze: der Kreis tritt erst bei
    #       z = cab_depth - R - sqrt(R² - (Hals/2)²)
    #   aus dem Hals heraus. Bis dahin darf die Fase reichen, nicht weiter.
    tip = body.Tip
    kanten = []
    for sign, t in ((+1, 'p'), (-1, 'm')):
        R = P(sh, 'cab_d_' + t) / 2
        halb = P(sh, 'cab_mouth_' + t) / 2
        einsatz = P(sh, 'cab_depth_' + t) - R - math.sqrt(R * R - halb * halb)
        if P(sh, 'cab_chamfer') >= einsatz:
            raise RuntimeError(
                'Kanal %s: Fase %.2f reicht in den Hinterschnitt '
                '(beginnt bei z = %.3f)'
                % (t, P(sh, 'cab_chamfer'), einsatz))
        kanten += [sign * P(sh, 'cab_y') + side * halb for side in (+1, -1)]
    namen = []
    for i, e in enumerate(tip.Shape.Edges):
        if not isinstance(e.Curve, Part.Line):
            continue
        d = e.Vertexes[-1].Point - e.Vertexes[0].Point
        if abs(d.y) > 1e-6 or abs(d.z) > 1e-6:
            continue                                   # nicht laengs x
        q = e.Vertexes[0].Point
        if abs(q.z) > 1e-6:
            continue                                   # nicht an der offenen Seite
        if any(abs(q.y - k) < 1e-6 for k in kanten):
            namen.append('Edge%d' % (i + 1))
    if len(namen) != 4:
        raise RuntimeError('Maulkanten nicht eindeutig gefunden: %s' % namen)
    f = body.newObject('PartDesign::Chamfer', 'Maulfasen')
    f.Base = (tip, namen)
    f.setExpression('Size', 'Parameter.cab_chamfer')
    doc.recompute()

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
