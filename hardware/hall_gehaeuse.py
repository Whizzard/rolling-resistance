# -*- coding: utf-8 -*-
"""
Gehäuse für einen Hall-Geber (TLE4905L) an der Trommel.

Quelle für hall-gehaeuse.FCStd / .step / .stl — die Datei wird aus diesem
Skript erzeugt, nicht umgekehrt. Alle Maße in mm (CAD-Ausnahme von der
SI-Regel: FreeCAD rechnet intern in mm).

Aufbau (X = Längsachse, Y = Breite, Z = Höhe, Z=0 ist die offene Unterseite):

    Z
    ^            LED 3 mm
    |              |
    +--------------v------------------------+   z = H_OUT
    |  Deckelwand                           |
    |  +---------------------------+        |
    |  |                           |  massiv|
    |  |   Kavität (Platine)       |  Block |
    |  |                           |   M6   |
    +--+---------------------------+--------+   z = 0  (offen, Alublech)
       ^                                ^
       | Hall-Geber dicht hinter        | Kabelkanal (neben der Schraube,
       | der dünnen Stirnwand           |  Kabel wird vom Alublech geklemmt)
    x=0                              x = L_OUT

Montage: Gehäuse mit der offenen Seite auf das Alublech, eine M6-Schraube
zieht es fest. Das Kabel liegt im Kanal und wird dabei gegen das Blech
geklemmt (Zugentlastung). Das Blech deckt die Platine ab; Isolation durch
Klebeband oder Kleber auf der Blechseite.
"""

import os
import sys

sys.path.append('/Applications/FreeCAD.app/Contents/Resources/lib')

import FreeCAD as App
import Part
from FreeCAD import Vector

# ---------------------------------------------------------------- Parameter

# Platine (handgesägte Lochrasterplatine, 2,54 mm Raster, 7 x 5 Löcher)
PCB_L, PCB_W, PCB_T = 18.0, 13.0, 1.6

# Kavität = Platine + Spiel, Höhe nach dem höchsten Bauteil (3-mm-LED)
CAV_L = PCB_L + 2.0
CAV_W = PCB_W + 1.0
CAV_H = 8.0

WALL      = 3.0   # Seitenwände
TOP       = 2.0   # Deckelwand
END_WALL  = 1.2   # Stirnwand vor dem Hall-Geber: so dünn wie druckbar,
                  # sie geht direkt in den Magnet-Luftspalt ein
RIB       = 2.0   # Steg zwischen Kavität und Schraubenblock

# M6-Durchgangsloch (mittelfein, DIN EN 20273)
M6_D      = 6.6
SCREW_BLK = 14.0  # Länge des massiven Blocks, Platz für Kopf + Scheibe
M6_Y      = -4.5  # aus der Mitte versetzt, damit der Kanal daneben passt

# Kabelkanal: 3-adrig, Mantel Ø 4 mm. Nut flacher als das Kabel dick ist,
# damit das Alublech es klemmt.
CAB_D     = 4.4
CAB_DEPTH = 3.4
CAB_Y     = 5.5

LED_D     = 3.2   # Durchsteckloch für die 3-mm-LED
LED_X     = 15.0  # von der Stirnwand aus, über dem hinteren Platinenende

# ------------------------------------------------------------- Abgeleitetes

W_OUT = CAV_W + 2 * WALL
H_OUT = CAV_H + TOP
L_OUT = END_WALL + CAV_L + RIB + SCREW_BLK

CAV_X0 = END_WALL
CAV_X1 = CAV_X0 + CAV_L
M6_X   = CAV_X1 + RIB + SCREW_BLK / 2.0

assert abs(M6_Y) + M6_D / 2.0 < W_OUT / 2.0 - 1.5, 'M6-Loch zu dicht am Rand'
assert CAB_Y + CAB_D / 2.0 < W_OUT / 2.0 - 1.5, 'Kabelkanal zu dicht am Rand'
assert CAB_Y - CAB_D / 2.0 > M6_Y + M6_D / 2.0, 'Kanal und M6-Loch kollidieren'
assert CAB_DEPTH < 4.0, 'Nut tiefer als das Kabel dick ist, keine Klemmung'


def box(lx, ly, lz, x=0.0, y=0.0, z=0.0):
    """Quader, in Y um die Mittelebene zentriert."""
    return Part.makeBox(lx, ly, lz, Vector(x, y - ly / 2.0, z))


def build():
    body = box(L_OUT, W_OUT, H_OUT)

    # Kavität, nach unten offen
    body = body.cut(box(CAV_L, CAV_W, CAV_H, CAV_X0, 0.0, 0.0))

    # M6-Durchgang
    body = body.cut(
        Part.makeCylinder(M6_D / 2.0, H_OUT + 2.0,
                          Vector(M6_X, M6_Y, -1.0), Vector(0, 0, 1)))

    # Kabelkanal: D-Nut (Rechteck unten, Halbrund oben), läuft von der
    # Kavität bis aus der Stirnseite heraus
    cab_x0 = CAV_X1 - 3.0
    cab_l = L_OUT - cab_x0 + 1.0
    r = CAB_D / 2.0
    slot = box(cab_l, CAB_D, CAB_DEPTH - r, cab_x0, CAB_Y, 0.0)
    slot = slot.fuse(
        Part.makeCylinder(r, cab_l, Vector(cab_x0, CAB_Y, CAB_DEPTH - r),
                          Vector(1, 0, 0)))
    body = body.cut(slot)

    # LED-Durchsteckloch im Deckel
    body = body.cut(
        Part.makeCylinder(LED_D / 2.0, TOP + 2.0,
                          Vector(LED_X, 0.0, CAV_H - 1.0), Vector(0, 0, 1)))

    return body.removeSplitter()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    doc = App.newDocument('hall-gehaeuse')
    feat = doc.addObject('Part::Feature', 'Gehaeuse')
    feat.Shape = build()
    feat.Label = 'Hall-Gebergehaeuse'
    doc.recompute()

    doc.saveAs(os.path.join(here, 'hall-gehaeuse.FCStd'))
    Part.export([feat], os.path.join(here, 'hall-gehaeuse.step'))
    feat.Shape.exportStl(os.path.join(here, 'hall-gehaeuse.stl'))

    bb = feat.Shape.BoundBox
    print('Aussenmass  %.1f x %.1f x %.1f mm'
          % (bb.XLength, bb.YLength, bb.ZLength))
    print('Volumen     %.0f mm3' % feat.Shape.Volume)
    print('Gueltig     %s' % feat.Shape.isValid())


main()
