# -*- coding: utf-8 -*-
"""
Halter für einen Trommelmarken-Magneten.

Quelle für magnethalter.FCStd / .step / .stl. Alle Maße in mm.
8 Stück, gleichmäßig auf einem Teilkreis der Trommel-Stirnfläche.

Der Magnet ist ein gewöhnlicher ungebohrter Scheibenmagnet. Er liegt in einer
nach unten offenen Tasche und wird von der Trommel-Stirnfläche selbst gehalten —
kein Deckel, kein Kleber nötig. Dadurch sitzt keine Schraube im Flussweg, was
bei Magneten mit Senkloch das eigentliche Problem wäre:

    Senklochmagnet + Stahlschraube = Kurzschluss von Pol zu Pol mitten durch
    den Magneten. Das Feld wird ringförmig statt mittig, der Schaltpunkt
    unsauber, im Grenzfall zwei Flanken pro Magnet.

Schnitt in Längsrichtung (Z = 0 ist die Trommel-Stirnfläche):

      Sensorseite
         ^
         |   0,8 mm Frontwand
    +----+---------------+----+   z = H
    |    |   Tasche      | \/ |   <- M3 Senkschraube
    |    |   (Magnet)    |    |
    +----+---------------+----+   z = 0   Trommelfläche
      ||                              ||
     Zapfen Ø3                     (Schraube ins Gewinde)

Warum ein Zapfen und nicht nur die Schraube: mit einer einzigen Schraube kann
sich der Halter um sie drehen. Bei 9,5 mm Abstand sind 5° Verdrehung schon
0,83 mm Bogen — das ist mehr, als die ganze Genauigkeit wert war, für die man
überhaupt schraubt statt klebt. Der Zapfen greift in eine zweite gebohrte
Ø3,2-Bohrung und legt die Winkellage fest.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
from FreeCAD import Vector

# ---------------------------------------------------------------- Parameter

# Scheibenmagnet, ungebohrt. Ø10 x 3 N42 liefert bei 4,5 mm Gesamtabstand
# rund 106 mT — das Sechsfache des ungünstigsten B_OP des TLE4905L (18 mT).
MAG_D, MAG_T = 10.0, 3.0
MAG_FIT = 0.3   # Durchmesserspiel in der Tasche (Druckmaß)
MAG_SLACK = 0.2 # Tiefenspiel

FRONT = 0.8     # Wand zwischen Magnet und Sensor; geht in den Luftspalt ein
WALL  = 1.8     # Mindestwand aussen

# M3-Senkschraube DIN 7991, nichtmagnetisch (A2/A4 oder Messing)
SCREW_D   = 3.4
HEAD_D    = 6.3
HEAD_ANG  = 90.0

# Zapfen gegen Verdrehen, greift in eine gebohrte Ø3,2-Bohrung
BOSS_D, BOSS_H = 3.0, 2.0

PITCH = 9.5     # Tasche <-> Schraube und Tasche <-> Zapfen

EDGE_R = 3.0    # Eckenverrundung

# ------------------------------------------------------------- Abgeleitetes

POCKET_D = MAG_D + MAG_FIT
POCKET_H = MAG_T + MAG_SLACK
H_OUT = FRONT + POCKET_H
L_OUT = 2 * (PITCH + HEAD_D / 2.0 + WALL)
W_OUT = max(POCKET_D, HEAD_D) + 2 * WALL

# Senkungstiefe aus Kopfdurchmesser und Senkwinkel (90° -> Tiefe = halbe Differenz)
CSK_H = (HEAD_D - SCREW_D) / 2.0

assert CSK_H < H_OUT - 1.5, 'Senkung laesst zu wenig Material unter dem Kopf'
assert FRONT >= 0.8, 'Frontwand unter 0,8 mm ist nicht zuverlaessig druckbar'


def build():
    body = Part.makeBox(L_OUT, W_OUT, H_OUT,
                        Vector(-L_OUT / 2.0, -W_OUT / 2.0, 0.0))

    # Senkrechte Kanten verrunden
    try:
        vert = [e for e in body.Edges
                if abs(e.Vertexes[0].Point.x - e.Vertexes[1].Point.x) < 1e-6
                and abs(e.Vertexes[0].Point.y - e.Vertexes[1].Point.y) < 1e-6]
        body = body.makeFillet(EDGE_R, vert)
    except Exception as exc:                      # Verrundung ist Kosmetik
        print('Hinweis: Verrundung uebersprungen (%s)' % exc)

    # Zapfen unterhalb der Montageebene
    body = body.fuse(
        Part.makeCylinder(BOSS_D / 2.0, BOSS_H,
                          Vector(-PITCH, 0.0, -BOSS_H), Vector(0, 0, 1)))

    # Magnettasche, nach unten offen
    body = body.cut(
        Part.makeCylinder(POCKET_D / 2.0, POCKET_H,
                          Vector(0.0, 0.0, 0.0), Vector(0, 0, 1)))

    # Schraubendurchgang mit Senkung
    body = body.cut(
        Part.makeCylinder(SCREW_D / 2.0, H_OUT + BOSS_H + 2.0,
                          Vector(PITCH, 0.0, -BOSS_H - 1.0), Vector(0, 0, 1)))
    body = body.cut(
        Part.makeCone(SCREW_D / 2.0, HEAD_D / 2.0, CSK_H,
                      Vector(PITCH, 0.0, H_OUT - CSK_H), Vector(0, 0, 1)))

    return body.removeSplitter()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('magnethalter')
    feat = doc.addObject('Part::Feature', 'Magnethalter')
    feat.Shape = build()
    feat.Label = 'Magnethalter Trommelmarke'
    doc.recompute()

    doc.saveAs(os.path.join(here, 'magnethalter.FCStd'))
    Part.export([feat], os.path.join(here, 'magnethalter.step'))
    feat.Shape.exportStl(os.path.join(here, 'magnethalter.stl'))

    bb = feat.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm (mit Zapfen)'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Bauhoehe    %.1f mm ueber der Trommelflaeche' % H_OUT)
    print('Senkung     %.2f mm tief, %.1f mm Material darunter'
          % (CSK_H, H_OUT - CSK_H))
    print('Volumen     %.0f mm3, x8 = %.1f cm3' % (feat.Shape.Volume,
                                                   8 * feat.Shape.Volume / 1000.0))
    print('Gueltig     %s' % feat.Shape.isValid())


main()
