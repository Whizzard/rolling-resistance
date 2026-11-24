# -*- coding: utf-8 -*-
"""
Wandhalterung für ein iPad mini 3, als Kreuz, zum Drucken.

Aufbau wie hall_gehaeuse.py und xt60_halter.py: ein PartDesign-Body mit
Featurebaum plus ein Spreadsheet "Parameter"; alle Skizzen über Ausdrücke an
die Tabelle gebunden. Maße ändert man im GUI in der Tabelle, nicht in den
Skizzen. Maße in mm.

Aufruf (nur das FreeCAD-eigene Python kann FreeCAD importieren):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/ipad_halter.py

Erzeugt hardware/ipad-halter.FCStd, .step und .stl.

LAGE: Hochformat, Home-Button unten. Das iPad liegt mit dem Rücken auf der
Platte. z = 0 ist der Rücken der Halterung — flach, das ist die Druckauflage.
x läuft über die Breite des iPads, y über die Länge, +y ist oben.

    ipad_w x ipad_l x ipad_t = 134,7 x 200,0 x 7,5   Apple, technische Daten
                                                      iPad mini 3 — vor dem
                                                      Druck am Gerät nachmessen

Das Kreuz: ein senkrechter und ein waagerechter Arm, je arm_w breit, Platte
plate_t dick, alle zwölf Ecken in der Draufsicht mit corner_r gerundet. Die
Arme reichen bis an die Kanten des iPads; an ihren Enden sitzen die vier
Haltestellen. Die Ecken des iPads bleiben frei — dort fasst man an.

Die Kralle läuft über die VOLLE Armbreite, die Plattenrundung geht dort über
die ganze Höhe mit — bei 40 mm Armen bliebe sonst neben der Lücke nichts für
die Kralle übrig. Deshalb muss corner_r kleiner sein als claw_t, sonst
schneidet die Rundung durch die Innenwand. Führungen und Schnapper sind
dünner bzw. oben verjüngt und enden vor der Rundung (wall_len).

    links/rechts   FÜHRUNG       Wand guide_t dick, bündig mit der Vorderseite,
                                 kein Überstand. Sie hält nichts, sie richtet aus.
    unten          KRALLE        Wand claw_t plus Lippe claw_lip tief über der
                                 Vorderseite. In der Mitte eine Lücke claw_gap:
                                 der Home-Button bleibt frei, und das
                                 Lightning-Kabel kommt durch die Lücke in der
                                 Wand. Es bleiben zwei L-Stücke aussen, je
                                 claw_len breit — die tragen das Gewicht und
                                 sind deshalb die kräftigsten Teile.
    oben           SCHNAPPER     Wand clip_t mit kleiner Nase clip_nose. Die
                                 Einführschräge ist flach (clip_ramp = 2 x
                                 clip_nose, rund 27° zur Senkrechten), die
                                 Unterseite der Nase leicht geneigt (clip_under)
                                 — das rastet weich ein und lässt sich ohne
                                 Gewalt wieder lösen. Über der Schräge läuft
                                 die Wand als LASCHE tab_h hoch, um tab_flare
                                 nach hinten geneigt, von clip_t auf tab_tip
                                 verjüngt, der Knick aussen mit tab_r verrundet.
                                 Innen geht die Schräge OHNE ABSATZ in die
                                 Lasche über; tab_flare/tab_h ist dafür etwas
                                 steiler als die Schräge (2:1), sonst entsteht
                                 ein einspringender Knick, und den rundet der
                                 Kern an den Stirnseiten nicht. Daran zieht man
                                 zum Ausklinken. Einsetzen: unten in die Kralle,
                                 oben andrücken; die Schräge hebt das Armende
                                 jenseits der oberen Schraube (rund 50 mm
                                 Hebel), die Nase rastet über die Glaskante.

Kralle (6 mm Wand, 4 mm Lippe) und Schnapper (4,5 mm Wand) sind bewusst
kräftiger als die Führungen: beim Druck mit dem Rücken auf dem Bett stehen die
Wände aus Schichten, und Lippe und Nase hängen an einer Schichtfuge — da
bricht es ab, wenn es dünn ist. Die beiden Krallenstücke sind einzeln und
tragen das ganze Gewicht, darum die dicksten. Die Führungen seitlich halten
nichts und bleiben bei 3 mm.

Vier M6-Linsenkopfschrauben in Flachsenkungen von der iPad-Seite: zwei auf
der senkrechten Mittelachse, m6_pitch auseinander, zwei auf der waagerechten,
m6_pitch_h auseinander — je nachdem, wie die Wand hergibt, nimmt man das eine
Paar oder das andere oder alle vier. Kopfhöhe m6_head_h = 4 mm ANGENOMMEN (an
der Schraube messen), darunter bleiben m6_floor = 1,5 mm Boden — daraus folgt
die Plattendicke plate_t = 5,5. Die Senkung m6_head_d = 13 fasst DIN 7985
(dk 12) wie ISO 7380 (dk 10,6). Der Kopf liegt vollständig in der Platte, das
iPad liegt auf der Platte, nicht auf den Köpfen. Weil die Platte dicker wurde,
sind die Arme schmaler: arm_w 40.

RUNDUNGEN: Plattenecken corner_r in der Draufsicht. Kralle rundum —
senkrechte Endkanten mit claw_r, alle freien Kanten darüber (Oberkante,
Lippenvorderkante, Enden) mit edge_r. Schnapper: Knick der Lasche mit tab_r,
alle übrigen freien Kanten mit edge_r; Nase und Schräge bleiben scharf, das
ist die Rastkante. Die Führungen bekommen nur wall_r an den Endkanten.
Tangentiale Nahtkanten vorhandener Rundungen werden erkannt und übergangen.

DRUCK: Rücken auf das Bett. Die Lippe der Kralle (claw_lip = 4 mm) und die
Nase (1,5 mm) sind Überhänge ohne Stütze von unten; die Nase druckt frei, die
Lippe braucht Stützen oder man lässt sie als kurze Brücke laufen. PETG
empfohlen — die Platte federt beim Einschnappen, PLA ermüdet dabei.

Featurebaum:
    Arm_senkrecht    Rechteck arm_w x (Länge über beide Wände), z = 0...plate_t
    Arm_waagerecht   Rechteck (Breite über beide Wände) x arm_w
    Fuehrung         zwei Wände an x = +-(ipad_w/2 + clr/2), Länge wall_len
    Kralle_p/_m      L-Profil in YZ, je ein Stück neben der Lücke bis zum Rand
    Schnapper        Profil mit Nase und Lasche in YZ, Länge wall_len, mittig
    Rundung_Platte   alle zwölf Aussenecken in der Draufsicht, Radius corner_r,
                     an der Kralle über die volle Höhe
    Rundung_Fuehrung  senkrechte Endkanten der Führungen, Radius wall_r
    Rundung_Krallen   senkrechte Endkanten der Kralle, claw_r
    Rundung_Kanten    freie Kanten der Kralle, edge_r
    Rundung_Lasche    Knick der Lasche innen und aussen, tab_r
    Rundung_Schnapper freie Kanten des Schnappers, edge_r
    M6               vier Flachsenkungen, PartDesign-Hole mit Counterbore
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
    # Geraet, Apple technische Daten iPad mini 3 -> NACHMESSEN
    ('ipad_w',      134.7, 'iPad breit, Apple'),
    ('ipad_l',      200.0, 'iPad lang, Apple'),
    ('ipad_t',        7.5, 'iPad dick, Apple'),
    ('clr',           0.6, 'Spiel in Breite und Laenge, GESAMT -> 0,3 je Seite'),
    ('clr_z',         0.3, 'Luft zwischen Vorderseite und Lippe/Nase'),

    # Kreuz
    ('plate_t', '=m6_head_h + m6_floor', 'Platte dick: Kopf plus Boden; Ruecken = Druckauflage'),
    ('arm_w',        40.0, 'Arme breit, beide gleich'),
    ('corner_r',      4.0, 'Rundung aller Plattenecken; MUSS < claw_t sein'),
    ('wall_len', '=arm_w - 2 * corner_r', 'Fuehrungslaenge = gerader Teil der Armenden'),
    ('wall_r',        1.0, 'Rundung der senkrechten Endkanten der Fuehrungen'),
    ('claw_r',        2.0, 'Rundung der senkrechten Endkanten von Kralle und Schnapper'),
    ('edge_r',        1.2, 'Rundung der freien Kanten von Kralle und Schnapper'),

    # Fuehrung seitlich
    ('guide_t',       3.0, 'Fuehrungswand dick'),
    ('guide_h', '=plate_t + ipad_t', 'Fuehrungswand hoch, buendig mit Vorderseite'),

    # Kralle unten
    ('claw_t',        6.0, 'Krallenwand dick; zwei einzelne Stuecke tragen alles'),
    ('claw_lip',      4.0, 'Lippe ueber der Vorderseite, Tiefe'),
    ('claw_lip_t',    4.0, 'Lippe dick; haengt an einer Schichtfuge'),
    ('claw_gap',     20.0, 'Luecke mittig: Home-Button frei, Lightning durch'),
    ('claw_len', '=(arm_w - claw_gap) / 2', 'Laenge je Krallenstueck, bis zum Armrand'),

    # Schnapper oben
    ('clip_t',        4.5, 'Schnapperwand dick; steht aus Schichten, deshalb kraeftig'),
    ('clip_nose',     1.2, 'Nase ueber der Vorderseite, Tiefe'),
    ('clip_ramp', '=2 * clip_nose', 'Einfuehrschraege hoch; 2:1 -> rund 27 Grad zur Senkrechten'),
    ('clip_under',    0.6, 'Neigung der Nasenunterseite, Hoehe; 0 = waagerecht, hart'),
    ('clip_tip',      1.0, 'Nase an der Spitze senkrecht, zwischen Schraege und Unterseite'),
    ('tab_h',        10.0, 'Lasche hoch, ab Oberkante der Schraege'),
    ('tab_flare',     6.0, 'Lasche nach hinten geneigt, Versatz oben; > tab_h/2, s. Kopf'),
    ('tab_tip',       3.0, 'Lasche oben dick; unten clip_t'),
    ('tab_r',         6.0, 'Rundung des Knicks der Lasche'),

    # Schrauben M6 Linsenkopf, Flachsenkung von der iPad-Seite
    ('m6_d',          6.6, 'M6 Durchgang'),
    ('m6_head_d',    13.0, 'Flachsenkung; DIN 7985 dk 12, ISO 7380 dk 10,6 + Luft'),
    ('m6_head_h',     4.0, 'Kopfhoehe ANGENOMMEN -> an der Schraube messen'),
    ('m6_floor',      1.5, 'Boden unter dem Kopf'),
    ('m6_pitch',    100.0, 'Lochabstand senkrecht, auf der Mittelachse'),
    ('m6_pitch_h',   80.0, 'Lochabstand waagerecht, auf der Mittelachse'),

    # abgeleitet
    ('x_side',  '=ipad_w / 2 + clr / 2',   'Innenflaeche der Fuehrung, Betrag'),
    ('y_top',   '=ipad_l / 2 + clr / 2',   'Innenflaeche des Schnappers'),
    ('y_bot',   '=-(ipad_l / 2 + clr / 2)', 'Innenflaeche der Kralle'),
    ('z_front', '=plate_t + ipad_t + clr_z', 'Unterseite von Lippe und Nase'),
    ('claw_top', '=z_front + claw_lip_t',  'Kralle hoch'),
    ('z_ramp',  '=z_front + clip_under + clip_tip + clip_ramp', 'Oberkante der Schraege = Knick der Lasche'),
    ('clip_top', '=z_ramp + tab_h',       'Schnapper hoch'),
    ('claw_x',  '=claw_gap / 2 + claw_len / 2', 'Mitte eines Krallenstuecks, Betrag'),
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


def polygon(sk, pts, tag):
    """Geschlossener Linienzug. pts = [(x, y, expr_x, expr_y), ...]; jede Ecke
    wird über zwei Abstände zum Ursprung festgelegt und an Ausdrücke gebunden —
    damit ist der Zug ohne weitere Bedingungen voll bestimmt."""
    v = Vector
    n = len(pts)
    g = []
    for i in range(n):
        a, b = pts[i], pts[(i + 1) % n]
        g.append(sk.addGeometry(Part.LineSegment(v(a[0], a[1], 0), v(b[0], b[1], 0)), False))
    for i in range(n):
        sk.addConstraint(Sketcher.Constraint('Coincident', g[i], 2, g[(i + 1) % n], 1))
    for i, (x, y, ex, ey) in enumerate(pts):
        named(sk, Sketcher.Constraint('DistanceX', -1, 1, g[i], 1, x), '%s_x%d' % (tag, i))
        named(sk, Sketcher.Constraint('DistanceY', -1, 1, g[i], 1, y), '%s_y%d' % (tag, i))
        sk.setExpression('Constraints.%s_x%d' % (tag, i), ex)
        sk.setExpression('Constraints.%s_y%d' % (tag, i), ey)
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


def vertical_edges(shape, zmin, zmax):
    """Namen der Kanten parallel zu z, die genau von zmin bis zmax laufen."""
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.x - b.x) > 1e-6 or abs(a.y - b.y) > 1e-6:
            continue
        lo, hi = min(a.z, b.z), max(a.z, b.z)
        if abs(lo - zmin) < 1e-6 and abs(hi - zmax) < 1e-6:
            namen.append('Edge%d' % (i + 1))
    return namen


def wall_end_edges(shape, plate_t, region):
    """Senkrechte Kanten, die auf der Platte beginnen und ueber sie hinaus-
    reichen — die Endkanten der Waende. region(bb) waehlt den Bereich."""
    namen = []
    for i, e in enumerate(shape.Edges):
        if len(e.Vertexes) != 2:
            continue
        a, b = e.Vertexes[0].Point, e.Vertexes[1].Point
        if abs(a.x - b.x) > 1e-6 or abs(a.y - b.y) > 1e-6:
            continue
        lo, hi = min(a.z, b.z), max(a.z, b.z)
        if lo < plate_t - 1e-6 or hi < plate_t + 1e-3:
            continue
        if region(e.BoundBox):
            namen.append('Edge%d' % (i + 1))
    return namen


def tangent_seam(shape, edge):
    """Wahr, wenn die beiden Flaechen an der Kante tangential ineinander
    uebergehen — die Naht einer vorhandenen Rundung. Die laesst sich nicht
    noch einmal runden."""
    faces = shape.ancestorsOfType(edge, Part.Face)
    if len(faces) != 2:
        return False
    t = edge.FirstParameter + 0.5 * (edge.LastParameter - edge.FirstParameter)
    p = edge.valueAt(t)
    normals = []
    for f in faces:
        u, v = f.Surface.parameter(p)
        normals.append(f.normalAt(u, v))
    return abs(abs(normals[0].dot(normals[1])) - 1.0) < 1e-3


def free_edges(shape, plate_t, region, exclude):
    """Alle Kanten oberhalb der Platte in einem Bereich, ohne senkrechte
    (die sind schon gerundet) und ohne die in exclude(bb) genannten — das
    sind die einspringenden, an denen das iPad anliegt."""
    namen = []
    for i, e in enumerate(shape.Edges):
        bb = e.BoundBox
        if bb.ZMin < plate_t + 1e-3:
            continue                               # Fuss der Wand, einspringend
        if bb.XLength < 1e-6 and bb.YLength < 1e-6:
            continue                               # senkrecht
        if not region(bb) or exclude(bb):
            continue
        if tangent_seam(shape, e):
            continue                               # Naht einer Rundung
        namen.append('Edge%d' % (i + 1))
    return namen


def fillet(doc, body, name, edges, expr_r):
    tip = body.Tip
    f = body.newObject('PartDesign::Fillet', name)
    f.Base = (tip, edges)
    f.setExpression('Radius', expr_r)
    doc.recompute()
    if 'Invalid' in (f.State or []) or not body.Shape.isValid():
        print('%s fehlgeschlagen (%d Kanten), bleibt weg' % (name, len(edges)))
        body.removeObject(f)
        doc.removeObject(f.Name)
        doc.recompute()
        return None
    return f


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

    pt, arm = P(sh, 'plate_t'), P(sh, 'arm_w')
    xs, yt, yb = P(sh, 'x_side'), P(sh, 'y_top'), P(sh, 'y_bot')
    gt, ct, kt = P(sh, 'guide_t'), P(sh, 'clip_t'), P(sh, 'claw_t')
    zf = P(sh, 'z_front')

    # 1 Senkrechter Arm: von der Aussenseite der Kralle bis zur Aussenseite des
    #   Schnappers.
    sk = sketch_on(body, xy, 'Sk_Arm_senkrecht')
    rect(sk, -arm / 2, yb - kt, arm, (yt + ct) - (yb - kt), 'av',
         '-Parameter.arm_w / 2', 'Parameter.y_bot - Parameter.claw_t',
         'Parameter.arm_w',
         'Parameter.y_top + Parameter.clip_t - (Parameter.y_bot - Parameter.claw_t)')
    pad(sk, 'Arm_senkrecht', 'Parameter.plate_t')

    # 2 Waagerechter Arm: ueber beide Fuehrungswaende.
    sk = sketch_on(body, xy, 'Sk_Arm_waagerecht')
    rect(sk, -(xs + gt), -arm / 2, 2 * (xs + gt), arm, 'ah',
         '-(Parameter.x_side + Parameter.guide_t)', '-Parameter.arm_w / 2',
         '2 * (Parameter.x_side + Parameter.guide_t)', 'Parameter.arm_w')
    pad(sk, 'Arm_waagerecht', 'Parameter.plate_t')

    # 3 Fuehrung links und rechts: zwei Waende, beide in einer Skizze.
    sk = sketch_on(body, xy, 'Sk_Fuehrung')
    wl = P(sh, 'wall_len')
    rect(sk, xs, -wl / 2, gt, wl, 'fp',
         'Parameter.x_side', '-Parameter.wall_len / 2', 'Parameter.guide_t', 'Parameter.wall_len')
    rect(sk, -(xs + gt), -wl / 2, gt, wl, 'fm',
         '-(Parameter.x_side + Parameter.guide_t)', '-Parameter.wall_len / 2',
         'Parameter.guide_t', 'Parameter.wall_len')
    pad(sk, 'Fuehrung', 'Parameter.guide_h')

    # 5 Kralle: L-Profil in YZ (lokal x = global y, lokal y = global z), je ein
    #   Stueck links und rechts der Luecke. Die YZ-Normale zeigt nach +x, der
    #   Skizzenversatz schiebt das Profil auf x = +-claw_x; geprueft in check().
    kl, kt_lip = P(sh, 'claw_lip'), P(sh, 'claw_lip_t')
    ktop = P(sh, 'claw_top')
    for tag, sign in (('p', +1), ('m', -1)):
        sk = sketch_on(body, yz, 'Sk_Kralle_' + tag,
                       expr_off='%d * Parameter.claw_x' % sign)
        polygon(sk, [
            (yb - kt, 0.0,  'Parameter.y_bot - Parameter.claw_t', '0'),
            (yb,      0.0,  'Parameter.y_bot',                    '0'),
            (yb,      zf,   'Parameter.y_bot',                    'Parameter.z_front'),
            (yb + kl, zf,   'Parameter.y_bot + Parameter.claw_lip', 'Parameter.z_front'),
            (yb + kl, ktop, 'Parameter.y_bot + Parameter.claw_lip', 'Parameter.claw_top'),
            (yb - kt, ktop, 'Parameter.y_bot - Parameter.claw_t',   'Parameter.claw_top'),
        ], 'kr' + tag)
        pad(sk, 'Kralle_' + tag, 'Parameter.claw_len', midplane=True)

    # 6 Schnapper: Wand mit Nase und Lasche, mittig ueber wall_len. Profil in
    #   YZ (lokal x = global y, lokal y = global z), im Uhrzeigersinn ab dem
    #   Fuss innen: Fuss, Aussenwand bis zum Knick, Lasche nach hinten geneigt
    #   und verjuengt, Oberkante, Innenseite der Lasche hinunter bis zur
    #   Oberkante der Schraege, Einfuehrschraege, Nasenspitze, geneigte
    #   Unterseite.
    nose, ctop = P(sh, 'clip_nose'), P(sh, 'clip_top')
    zr = P(sh, 'z_ramp')
    fl, tip, under = P(sh, 'tab_flare'), P(sh, 'tab_tip'), P(sh, 'clip_under')
    sk = sketch_on(body, yz, 'Sk_Schnapper')
    polygon(sk, [
        (yt,            0.0,  'Parameter.y_top',                       '0'),
        (yt + ct,       0.0,  'Parameter.y_top + Parameter.clip_t',    '0'),
        (yt + ct,       zr,   'Parameter.y_top + Parameter.clip_t',    'Parameter.z_ramp'),
        (yt + fl + tip, ctop, 'Parameter.y_top + Parameter.tab_flare + Parameter.tab_tip',
                              'Parameter.clip_top'),
        (yt + fl,       ctop, 'Parameter.y_top + Parameter.tab_flare', 'Parameter.clip_top'),
        (yt,            zr,   'Parameter.y_top',                       'Parameter.z_ramp'),
        (yt - nose, zr - P(sh, 'clip_ramp'),
                              'Parameter.y_top - Parameter.clip_nose',
                              'Parameter.z_ramp - Parameter.clip_ramp'),
        (yt - nose, zf + under, 'Parameter.y_top - Parameter.clip_nose',
                              'Parameter.z_front + Parameter.clip_under'),
        (yt,            zf,   'Parameter.y_top',                       'Parameter.z_front'),
    ], 'sn')
    pad(sk, 'Schnapper', 'Parameter.wall_len', midplane=True)

    # 6a Rundung aller Plattenecken: jede senkrechte Kante, die am Ruecken
    #    beginnt — an den Enden des waagerechten Arms und an den vier
    #    einspringenden Ecken nur bis plate_t, an der Kralle bis zu deren
    #    Oberkante, weil die Wand dort buendig mit dem Plattenrand ist.
    edges = []
    for i, e in enumerate(body.Shape.Edges):
        bb = e.BoundBox
        if len(e.Vertexes) == 2 and bb.XLength < 1e-6 and bb.YLength < 1e-6 \
                and abs(bb.ZMin) < 1e-6:
            edges.append('Edge%d' % (i + 1))
    if len(edges) == 12:
        fillet(doc, body, 'Rundung_Platte', edges, 'Parameter.corner_r')
    else:
        print('Plattenkanten nicht eindeutig (%d), Rundung bleibt weg' % len(edges))

    # 7 Rundungen. Erst die senkrechten Endkanten der Waende (Draufsicht),
    #   dann alle freien Kanten von Kralle und Schnapper. Einspringende Kanten
    #   bleiben scharf: der Fuss der Waende und die Unterseite der Nase, da
    #   liegt das iPad an.
    guides = lambda bb: abs(bb.XMin) > xs - 1e-3 and abs(bb.YMax) < arm / 2 + 1e-3
    claws = lambda bb: bb.YMax < yb + P(sh, 'claw_lip') + 1e-3
    clip = lambda bb: bb.YMin > yt - nose - 1e-3

    edges = wall_end_edges(body.Shape, pt, guides)
    print('Fuehrung   %d Endkanten' % len(edges))
    if edges:
        fillet(doc, body, 'Rundung_Fuehrung', edges, 'Parameter.wall_r')

    # Senkrechte Endkanten der Kralle, gestuft: die Kanten der Innenwand enden
    #   oben in der einspringenden Kante unter der Lippe, was OCC nicht immer
    #   rundet. Erst alle, sonst nur die, die bis zur Oberkante durchlaufen
    #   (Lippenvorderkanten; die Aussenecken hat schon die Plattenrundung).
    def bis_oben(n):
        return abs(body.Shape.getElement(n).BoundBox.ZMax - ktop) < 1e-3

    alle = wall_end_edges(body.Shape, pt, claws)
    for stufe, edges in (('alle', alle),
                         ('nur durchlaufende', [n for n in alle if bis_oben(n)])):
        print('Kralle     %d Endkanten (%s)' % (len(edges), stufe))
        if edges and fillet(doc, body, 'Rundung_Krallen', edges, 'Parameter.claw_r'):
            break

    # Freie Kanten der Kralle; die einspringende Kante unter der Lippe
    #   (y = y_bot, z = z_front) bleibt scharf, da liegt das iPad an.
    def unter_der_lippe(bb):
        return bb.YLength < 1e-6 and abs(bb.YMin - yb) < 1e-3 and abs(bb.ZMin - zf) < 1e-3
    alle = free_edges(body.Shape, pt, claws, unter_der_lippe)

    def vor_der_innenwand(n):
        return body.Shape.getElement(n).BoundBox.YMin > yb + 1e-3

    def oberkante(n):
        return body.Shape.getElement(n).BoundBox.ZMin > ktop - 1e-3

    for stufe, edges in (('alle', alle),
                         ('ohne Innenwand', [n for n in alle if vor_der_innenwand(n)]),
                         ('nur Oberkanten', [n for n in alle if oberkante(n)])):
        print('Kralle     %d freie Kanten (%s)' % (len(edges), stufe))
        if edges and fillet(doc, body, 'Rundung_Kanten', edges, 'Parameter.edge_r'):
            break

    # Schnapper. Erst der Knick der Lasche: die waagerechten Kanten laengs x
    #   auf z = z_ramp, aussen (Wand -> Lasche) und innen (Schraege -> Lasche,
    #   fast gestreckt). Fehlt die innere, weil beide Flaechen zusammenfallen,
    #   wird nur aussen gerundet.
    edges = []
    for i, e in enumerate(body.Shape.Edges):
        bb = e.BoundBox
        if clip(bb) and bb.YLength < 1e-6 and bb.ZLength < 1e-6 \
                and abs(bb.ZMin - zr) < 1e-3 and bb.XLength > 1.0:
            edges.append('Edge%d' % (i + 1))
    print('Lasche     %d Knickkanten' % len(edges))
    if edges:
        fillet(doc, body, 'Rundung_Lasche', edges, 'Parameter.tab_r')

    # Dann alle freien Kanten des Schnappers oberhalb der Schraege, auch die
    #   senkrechten Aussenecken der Wand (sie laufen tangential in die
    #   Knickrundung und die Lasche weiter). Nase und Schraege bleiben scharf
    #   — und mit ihnen rundet der Kern auch nichts, was an sie anschliesst.
    def nase(bb):
        return bb.ZMax < zr + 1e-3
    frei = free_edges(body.Shape, pt, clip, nase)
    senkrecht = [n for n in wall_end_edges(body.Shape, pt, clip)
                 if body.Shape.getElement(n).BoundBox.YMin > yt + 1e-3]
    def oberkante_s(n):
        return body.Shape.getElement(n).BoundBox.ZMin > ctop - 1e-3
    for stufe, edges in (('alle', frei + senkrecht),
                         ('nur Oberkanten', [n for n in frei if oberkante_s(n)])):
        print('Schnapper  %d freie Kanten (%s)' % (len(edges), stufe))
        if edges and fillet(doc, body, 'Rundung_Schnapper', edges, 'Parameter.edge_r'):
            break

    # 8 M6-Flachsenkungen von der iPad-Seite. Skizze auf z = plate_t, die
    #   Bohrung geht gegen die Normale nach unten durch; die Senkung beginnt an
    #   der Skizzenebene und ist m6_head_h tief.
    sk = sketch_on(body, xy, 'Sk_M6', expr_off='Parameter.plate_t')
    for tag, sign in (('p', +1), ('m', -1)):
        circle(sk, 0.0, sign * P(sh, 'm6_pitch') / 2, P(sh, 'm6_d') / 2, 'm6v' + tag,
               '0', '%d * Parameter.m6_pitch / 2' % sign, 'Parameter.m6_d / 2')
        circle(sk, sign * P(sh, 'm6_pitch_h') / 2, 0.0, P(sh, 'm6_d') / 2, 'm6h' + tag,
               '%d * Parameter.m6_pitch_h / 2' % sign, '0', 'Parameter.m6_d / 2')
    h = body.newObject('PartDesign::Hole', 'M6')
    h.Profile = sk
    h.Threaded = False
    h.DepthType = 'ThroughAll'
    h.HoleCutType = 'Counterbore'
    h.setExpression('Diameter', 'Parameter.m6_d')
    h.setExpression('HoleCutDiameter', 'Parameter.m6_head_d')
    h.setExpression('HoleCutDepth', 'Parameter.m6_head_h')
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
    for o in doc.Objects:
        if hasattr(o, 'Visibility'):
            o.Visibility = False
    body.Visibility = True
    if body.Tip is not None:
        body.Tip.Visibility = True


def check(body, sh):
    """Geometrische Plausibilitaet — ein Vorzeichenfehler bei den Skizzenebenen
    soll hier auffallen, nicht erst im Slicer."""
    s = body.Shape
    bb = s.BoundBox
    ok = True

    def material(p, was):
        nonlocal ok
        if not s.isInside(p, 1e-6, True):
            print('FEHLER kein Material: %s bei (%.1f, %.1f, %.1f)' % (was, p.x, p.y, p.z))
            ok = False

    def luft(p, was):
        nonlocal ok
        if s.isInside(p, 1e-6, True):
            print('FEHLER Material statt Luft: %s bei (%.1f, %.1f, %.1f)' % (was, p.x, p.y, p.z))
            ok = False

    pt, zf = P(sh, 'plate_t'), P(sh, 'z_front')
    xs, yt, yb = P(sh, 'x_side'), P(sh, 'y_top'), P(sh, 'y_bot')
    gt, ct, kt = P(sh, 'guide_t'), P(sh, 'clip_t'), P(sh, 'claw_t')
    cx, nose = P(sh, 'claw_x'), P(sh, 'clip_nose')
    zmid = pt + P(sh, 'ipad_t') / 2

    # Ruecken flach
    if abs(bb.ZMin) > 1e-6:
        print('FEHLER Ruecken nicht auf z = 0: %.3f' % bb.ZMin); ok = False
    # Platte
    material(Vector(0, 0, pt / 2), 'Plattenmitte')
    material(Vector(0, yt, pt / 2), 'Platte unter dem Schnapper')
    material(Vector(0, yb, pt / 2), 'Platte unter der Kralle')
    material(Vector(xs, 0, pt / 2), 'Platte unter der Fuehrung')
    # Ecke gerundet: an der Armecke aussen darf nichts sein
    material(Vector(0, yt + ct - 0.2, pt / 2), 'Armende mittig')
    luft(Vector(P(sh, 'arm_w') / 2 - 0.3, yt + ct - 0.3, pt / 2), 'Armecke (Rundung)')
    # iPad-Raum frei
    for p in (Vector(0, 0, zmid), Vector(xs - 0.1, 0, zmid), Vector(-xs + 0.1, 0, zmid),
              Vector(0, yt - 0.1, zmid), Vector(0, yb + 0.1, zmid),
              Vector(0, yt - 0.1, zf - 0.1), Vector(0, yb + 0.1, zf - 0.1),
              Vector(cx, yb + 1.0, zf - 0.1)):
        luft(p, 'iPad-Raum')
    # Fuehrung
    for sign in (+1, -1):
        material(Vector(sign * (xs + gt / 2), 0, zmid), 'Fuehrung')
        luft(Vector(sign * (xs + gt / 2), 0, P(sh, 'guide_h') + 0.3), 'ueber der Fuehrung')
        luft(Vector(sign * (xs - 1.0), 0, zf + 1.0), 'vor der Fuehrung (kein Ueberstand)')
    # Kralle: zwei Stuecke, Luecke mittig
    for sign in (+1, -1):
        material(Vector(sign * cx, yb - kt / 2, zmid), 'Krallenwand')
        material(Vector(sign * cx, yb + P(sh, 'claw_lip') / 2, zf + P(sh, 'claw_lip_t') / 2), 'Krallenlippe')
    luft(Vector(0, yb - kt / 2, zmid), 'Luecke in der Krallenwand')
    luft(Vector(0, yb + 1.0, zf + 1.0), 'Luecke in der Lippe')
    luft(Vector(P(sh, 'claw_gap') / 2 - 0.3, yb - kt / 2, zmid), 'Lueckenrand')
    # Schnapper: Wand, Nase, Schraege
    material(Vector(0, yt + ct / 2, zmid), 'Schnapperwand')
    zr, ctop = P(sh, 'z_ramp'), P(sh, 'clip_top')
    material(Vector(0, yt - nose / 2, zf + P(sh, 'clip_under') + P(sh, 'clip_tip') / 2), 'Nase')
    luft(Vector(0, yt - nose + 0.1, zr - 0.1), 'Schraege der Nase')
    luft(Vector(0, yt - nose - 0.2, zf + 0.2), 'vor der Nase')
    luft(Vector(0, yt - nose + 0.1, zf + 0.05), 'geneigte Unterseite der Nase')
    material(Vector(0, yt + ct / 2, zr - 0.5), 'Wand unter dem Knick')
    if P(sh, 'tab_flare') / P(sh, 'tab_h') <= nose / P(sh, 'clip_ramp') + 1e-6:
        print('FEHLER Lasche flacher als die Schraege -> einspringender Knick innen'); ok = False
    material(Vector(0, yt + P(sh, 'tab_flare') + P(sh, 'tab_tip') / 2, ctop - 0.5), 'Lasche oben')
    luft(Vector(0, yt + 0.5, ctop - 0.5), 'vor der Lasche (nach hinten geneigt)')
    luft(Vector(0, yt + ct + P(sh, 'tab_flare') + P(sh, 'tab_tip'), ctop - 0.5), 'hinter der Lasche')
    print('Schnapper  Nase %.1f tief, Schraege %.1f hoch, Unterseite %.1f geneigt; Lasche %.1f hoch, '
          '%.1f nach hinten, %.1f -> %.1f dick'
          % (nose, P(sh, 'clip_ramp'), P(sh, 'clip_under'), P(sh, 'tab_h'),
             P(sh, 'tab_flare'), ct, P(sh, 'tab_tip')))
    # M6: Durchgang, Flachsenkung m6_head_h tief, Boden darunter
    r_cb = P(sh, 'm6_head_d') / 2 - 0.3
    hh, fl = P(sh, 'm6_head_h'), P(sh, 'm6_floor')
    for sign in (+1, -1):
        for x, y in ((0.0, sign * P(sh, 'm6_pitch') / 2),
                     (sign * P(sh, 'm6_pitch_h') / 2, 0.0)):
            luft(Vector(x, y, pt / 2), 'M6-Durchgang')
            luft(Vector(x + r_cb, y, pt - 0.1), 'Senkung oben')
            luft(Vector(x + r_cb, y, pt - hh + 0.1), 'Senkung am Grund')
            material(Vector(x + r_cb, y, pt - hh - 0.1), 'Boden unter dem Kopf')
            material(Vector(x + r_cb, y, 0.1), 'Boden am Ruecken')
    # Rundungen: an der Aussenecke der Kralle oben darf nichts mehr sein
    luft(Vector(cx + P(sh, 'claw_len') / 2 - 0.2, yb - kt + 0.2, P(sh, 'claw_top') - 0.2),
         'Krallenecke (Rundung)')
    print('Senkung  4 x Flachsenkung %.1f x %.1f tief, Boden %.1f, Platte %.1f; '
          'senkrecht %.0f, waagerecht %.0f'
          % (P(sh, 'm6_head_d'), hh, fl, pt, P(sh, 'm6_pitch'), P(sh, 'm6_pitch_h')))
    print('Kralle   %d Stuecke a %.1f mm, Wand %.1f, Lippe %.1f tief x %.1f dick, Luecke %.1f'
          % (2, P(sh, 'claw_len'), kt, P(sh, 'claw_lip'), P(sh, 'claw_lip_t'), P(sh, 'claw_gap')))
    if P(sh, 'corner_r') >= kt:
        print('FEHLER corner_r %.1f >= Wanddicke, Rundung schneidet die Innenwand' % P(sh, 'corner_r'))
        ok = False
    print('Hebel    obere Schraube bis Nase %.1f mm'
          % (yt - P(sh, 'm6_pitch') / 2))
    return ok


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('ipad-halter')
    sh = build_sheet(doc)
    body = build(doc, sh)
    doc.recompute()
    set_visibility(doc, body)

    fcstd = os.path.join(here, 'ipad-halter.FCStd')
    doc.saveAs(fcstd)
    write_gui_visibility(fcstd, {
        body.Name, body.Tip.Name,
        'X_Axis', 'Y_Axis', 'Z_Axis',
        'XY_Plane', 'XZ_Plane', 'YZ_Plane', 'Origin001'})
    Part.export([body], os.path.join(here, 'ipad-halter.step'))
    body.Shape.exportStl(os.path.join(here, 'ipad-halter.stl'))

    bb = body.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.1f cm3' % (body.Shape.Volume / 1000.0))
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
