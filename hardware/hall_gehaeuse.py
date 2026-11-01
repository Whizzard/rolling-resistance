# -*- coding: utf-8 -*-
"""
Gehäuse für einen Hall-Geber (TLE4905L) an der Trommel.

Erzeugt hall-gehaeuse.FCStd als **PartDesign-Body mit Featurebaum** plus ein
Spreadsheet "Parameter". Alle Skizzen sind über Ausdrücke an das Spreadsheet
gebunden — Maße ändert man im GUI in der Tabelle, nicht in den Skizzen.

Dieses Skript ist der Erzeuger, nicht die Wahrheit: wer im GUI arbeitet, pflegt
die FCStd. Wer den Baum neu aufbauen will, läuft hier durch. Maße in mm.

Featurebaum:
    Grundkoerper   Quader 37,2 x 20 x 12
    Kavitaet       breiter Teil, z = 0...3,6  (Platinendurchgang)
    Bauteilraum    schmaler Teil, z = 0...10  -> erzeugt den Absatz
    Rastnasen      zwei Keile, mittig, unter der Platine
    Federfeld      Freistellung aussen, damit die Wand an den Nasen federt
    Schraubloch    M6 durch
    Kabelnut       Klemmstrecke, D-Profil (Rechteck + Rundung)
    Kabelmulde     tiefer, vor der Platine, dort wird nicht geklemmt
    LED_Loch       Ø3,2 im Deckel

Z = 0 ist die offene Unterseite, die auf dem Alublech liegt.
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
    ('pcb_l',       18.0,  'Platine Laenge'),
    ('pcb_w',       13.0,  'Platine Breite'),
    ('pcb_t',        1.6,  'Platine Dicke'),
    ('solder_gap',   2.0,  'Loetraum unter der Platine'),
    ('headroom',     6.4,  'Bauteilraum ueber der Platine'),

    ('wall',         3.0,  'Seitenwand'),
    ('top',          2.0,  'Deckelwand'),
    ('end_wall',     1.2,  'Stirnwand vor dem Geber'),
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

    ('ledge_w',      1.8,  'Absatz je Seite'),
    ('snap_w',       0.65, 'Rastnase Vorsprung je Seite'),
    ('snap_l',       6.0,  'Rastnase Laenge'),
    ('snap_h',       0.8,  'Rastnase halbe Hoehe'),
    ('snap_z', '=pcb_z0',  'Scheitel der Rastnase'),

    ('relief_l',    12.0,  'Federfeld Laenge'),
    ('relief_t',     0.8,  'Federfeld Restwand'),
    ('relief_h', '=pcb_z1 + 2', 'Federfeld Hoehe'),

    ('m6_d',         6.6,  'M6 Durchgang'),
    ('m6_y',        -4.5,  'M6 aus der Mitte versetzt'),
    ('m6_x',  '=cav_x0 + cav_l + rib + screw_blk / 2', 'M6 Lage'),

    ('cab_d',        4.4,  'Kabelnut Breite'),
    ('cab_depth',    3.4,  'Kabelnut Tiefe, Klemmstrecke'),
    ('cab_y',        5.5,  'Kabelnut Lage'),
    ('cab_x0', '=cav_x0 + cav_l - 3', 'Kabelnut Anfang'),
    ('cab_len','=l_out - cab_x0 + 1', 'Kabelnut Laenge'),
    ('cab_deep',     5.2,  'Mulde Tiefe, keine Klemmung'),
    ('cab_deep_x0', 20.0,  'Mulde Anfang'),
    ('cab_deep_l',   6.0,  'Mulde Laenge'),

    ('led_d',        3.2,  'LED Durchsteckloch'),
    ('led_x',       15.0,  'LED Lage'),
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

    # 3 Bauteilraum, schmaler Teil durch die volle Höhe → erzeugt den Absatz
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

    # 6 Schraubloch
    sk = sketch_on(body, xy, 'Sk_Schraubloch')
    circle(sk, P(sh, 'm6_x'), P(sh, 'm6_y'), P(sh, 'm6_d') / 2, 'm6',
           'Parameter.m6_x', 'Parameter.m6_y', 'Parameter.m6_d / 2')
    pocket(sk, 'Schraubloch', 'Parameter.h_out + 1')

    # 7+8 Kabelnut: Rechteck + Rundung, Klemmstrecke
    r = P(sh, 'cab_d') / 2
    e_r = 'Parameter.cab_d / 2'
    sk = sketch_on(body, yz, 'Sk_Kabelnut', P(sh, 'cab_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
    rect(sk, P(sh, 'cab_y') - r, 0.0, P(sh, 'cab_d'), P(sh, 'cab_depth') - r, 'nut',
         'Parameter.cab_y - Parameter.cab_d / 2', '0',
         'Parameter.cab_d', 'Parameter.cab_depth - Parameter.cab_d / 2')
    pocket(sk, 'Kabelnut', 'Parameter.cab_len')

    sk = sketch_on(body, yz, 'Sk_Kabelnut_rund', P(sh, 'cab_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_x0')
    circle(sk, P(sh, 'cab_y'), P(sh, 'cab_depth') - r, r, 'nutr',
           'Parameter.cab_y', 'Parameter.cab_depth - Parameter.cab_d / 2', e_r)
    pocket(sk, 'Kabelnut_rund', 'Parameter.cab_len')

    # 9+10 Kabelmulde, tiefer
    sk = sketch_on(body, yz, 'Sk_Kabelmulde', P(sh, 'cab_deep_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
    rect(sk, P(sh, 'cab_y') - r, 0.0, P(sh, 'cab_d'), P(sh, 'cab_deep') - r, 'mul',
         'Parameter.cab_y - Parameter.cab_d / 2', '0',
         'Parameter.cab_d', 'Parameter.cab_deep - Parameter.cab_d / 2')
    pocket(sk, 'Kabelmulde', 'Parameter.cab_deep_l')

    sk = sketch_on(body, yz, 'Sk_Kabelmulde_rund', P(sh, 'cab_deep_x0'))
    sk.setExpression('AttachmentOffset.Base.z', 'Parameter.cab_deep_x0')
    circle(sk, P(sh, 'cab_y'), P(sh, 'cab_deep') - r, r, 'mulr',
           'Parameter.cab_y', 'Parameter.cab_deep - Parameter.cab_d / 2', e_r)
    pocket(sk, 'Kabelmulde_rund', 'Parameter.cab_deep_l')

    # 11 LED-Loch
    sk = sketch_on(body, xy, 'Sk_LED')
    circle(sk, P(sh, 'led_x'), 0.0, P(sh, 'led_d') / 2, 'led',
           'Parameter.led_x', '0', 'Parameter.led_d / 2')
    pocket(sk, 'LED_Loch', 'Parameter.h_out + 1')

    return body


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hall-gehaeuse')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()

    doc.saveAs(os.path.join(here, 'hall-gehaeuse.FCStd'))
    Part.export([body], os.path.join(here, 'hall-gehaeuse.step'))
    body.Shape.exportStl(os.path.join(here, 'hall-gehaeuse.stl'))

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
