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
    |  |   Bauteilraum             |  massiv|
    |  |- - - - - - - - - - - - - -|  Block |
    |  |   Platine        ]Rastnase|   M6   |
    |  |===========================|        |   Auflageleisten
    |  |   Lötraum                 |        |
    +--+---------------------------+--------+   z = 0  (offen, Alublech)
       ^                                ^
       | Hall-Geber dicht hinter        | Kabelkanal (neben der Schraube,
       | der dünnen Stirnwand           |  Kabel wird vom Alublech geklemmt)
    x=0                              x = L_OUT

Montage: Gehäuse mit der offenen Seite auf das Alublech, eine M6-Schraube
zieht es fest. Das Kabel liegt im Kanal und wird dabei gegen das Blech
geklemmt (Zugentlastung).

Die Platine liegt NICHT mehr direkt am Blech, sondern auf zwei Leisten. Damit
bleibt darunter Platz für Lötstellen, und das Blech kann sie nicht berühren —
die Isolation durch Klebeband wird damit zur Rückfallebene, nicht zur
Voraussetzung.
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

SOLDER_GAP = 2.0   # lichte Höhe unter der Platine für Lötstellen und Isolation
HEADROOM   = 6.4   # über der Platine, bestimmt vom aufrecht stehenden Geber

# Kavität = Platine + Spiel
CAV_L = PCB_L + 2.0
CAV_W = PCB_W + 1.0          # 0,5 mm Luft je Seite
CAV_H = SOLDER_GAP + PCB_T + HEADROOM

WALL      = 3.0   # Seitenwände
TOP       = 2.0   # Deckelwand
END_WALL  = 1.2   # Stirnwand vor dem Hall-Geber: so dünn wie druckbar,
                  # sie geht direkt in den Magnet-Luftspalt ein
RIB       = 2.0   # Steg zwischen Kavität und Schraubenblock

# Auflageleisten: durchgehend über die ganze Kavitätslänge, beidseitig gleich.
RAIL_W    = 1.8   # Vorsprung je Seite → 1,3 mm Auflage bei 0,5 mm Luft

# Rastnasen: nur mittig, dort federt das freigestellte Wandfeld (RELIEF_*).
# Symmetrische Schräge nach oben UND unten — die Platine wird durch Vorspannung
# gehalten, nicht durch einen Hinterschnitt, und lässt sich ohne Werkzeug wieder
# herausdrücken. Scheitel liegt auf der Platinenoberkante.
SNAP_W    = 0.65  # Vorsprung je Seite → 0,15 mm Überdeckung
SNAP_L    = 6.0
SNAP_H    = 0.8   # halbe Höhe des Keils, Schräge oben wie unten

# Freistellung von außen, damit das Wandfeld hinter der Nase federn kann
RELIEF_L  = 12.0
RELIEF_T  = 0.8   # Restwandstärke

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
CAV_XM = (CAV_X0 + CAV_X1) / 2.0        # Mitte der Kavität
M6_X   = CAV_X1 + RIB + SCREW_BLK / 2.0

PCB_Z0 = SOLDER_GAP                      # Unterkante Platine
PCB_Z1 = PCB_Z0 + PCB_T                  # Oberkante Platine
SNAP_Z = PCB_Z1                          # Scheitel der Rastnase

YW = CAV_W / 2.0                         # Innenwand der Kavität

assert abs(M6_Y) + M6_D / 2.0 < W_OUT / 2.0 - 1.5, 'M6-Loch zu dicht am Rand'
assert CAB_Y + CAB_D / 2.0 < W_OUT / 2.0 - 1.5, 'Kabelkanal zu dicht am Rand'
assert CAB_Y - CAB_D / 2.0 > M6_Y + M6_D / 2.0, 'Kanal und M6-Loch kollidieren'
assert CAB_DEPTH < 4.0, 'Nut tiefer als das Kabel dick ist, keine Klemmung'
assert RAIL_W > (CAV_W - PCB_W) / 2.0, 'Leisten schmaler als das Platinenspiel'
assert SNAP_W > (CAV_W - PCB_W) / 2.0, 'Rastnase greift nicht über die Platine'
assert CAV_XM + RELIEF_L / 2.0 < CAV_X1 + RIB, 'Freistellung laeuft in den Kanal'


def box(lx, ly, lz, x=0.0, y=0.0, z=0.0):
    """Quader, in Y um die Mittelebene zentriert."""
    return Part.makeBox(lx, ly, lz, Vector(x, y - ly / 2.0, z))


def rail(sign):
    """Auflageleiste, durchgehend; Oberkante = Unterkante Platine."""
    return Part.makeBox(CAV_L, RAIL_W, SOLDER_GAP,
                        Vector(CAV_X0,
                               sign * YW - (RAIL_W if sign > 0 else 0.0),
                               0.0))


def snap(sign):
    """Rastnase: symmetrischer Keil, Scheitel auf Platinenoberkante."""
    y_wall = sign * YW
    y_tip = y_wall - sign * SNAP_W
    pts = [Vector(0.0, y_wall, SNAP_Z - SNAP_H),
           Vector(0.0, y_tip,  SNAP_Z),
           Vector(0.0, y_wall, SNAP_Z + SNAP_H),
           Vector(0.0, y_wall, SNAP_Z - SNAP_H)]
    face = Part.Face(Part.makePolygon(pts))
    solid = face.extrude(Vector(SNAP_L, 0, 0))
    solid.translate(Vector(CAV_XM - SNAP_L / 2.0, 0, 0))
    return solid


def build():
    body = box(L_OUT, W_OUT, H_OUT)

    # Kavität, nach unten offen
    body = body.cut(box(CAV_L, CAV_W, CAV_H, CAV_X0, 0.0, 0.0))

    # Auflageleisten und Rastnasen wieder hinzufügen
    for sign in (+1, -1):
        body = body.fuse(rail(sign))
        body = body.fuse(snap(sign))

    # Freistellung von außen: lässt das Wandfeld hinter der Nase federn
    depth = W_OUT / 2.0 - YW - RELIEF_T
    for y0 in (YW + RELIEF_T, -W_OUT / 2.0):
        body = body.cut(
            Part.makeBox(RELIEF_L, depth, CAV_H - 1.0,
                         Vector(CAV_XM - RELIEF_L / 2.0, y0, 1.0)))

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
    print('Platine     z = %.1f ... %.1f mm, Loetraum %.1f mm'
          % (PCB_Z0, PCB_Z1, SOLDER_GAP))
    print('Rastnase    Scheitel z = %.1f, Ueberdeckung %.2f mm je Seite, %.0f mm lang'
          % (SNAP_Z, SNAP_W - (CAV_W - PCB_W) / 2.0, SNAP_L))
    print('Auflage     %.2f mm je Seite, durchgehend %.0f mm'
          % (RAIL_W - (CAV_W - PCB_W) / 2.0, CAV_L))
    print('Federfeld   %.1f mm Restwand ueber %.1f mm Laenge'
          % (RELIEF_T, RELIEF_L))
    print('Volumen     %.0f mm3' % feat.Shape.Volume)
    print('Gueltig     %s' % feat.Shape.isValid())


main()
