# -*- coding: utf-8 -*-
"""
Insulating plates under the two open housings: load-cell board (HX711) and
Hall sensor. 0.5 mm thin, in the outline of the respective housing, with the
M6 hole in the same place, and with a low rim around the outside in which
the housing sits.

Invocation (only FreeCAD's own Python can import FreeCAD):

    /Applications/FreeCAD.app/Contents/Resources/bin/python hardware/insulating_plate.py

Produces per housing hardware/insulating-plate-hx711.* and hardware/insulating-plate-hall.*
(FCStd, STEP, STL).

WHY: Both housings are open at the bottom, the board lies with the solder side
down and only solder_gap = 2 mm above the mounting surface. That surface is
aluminium. The plate closes the open side and puts plastic in between.

NO DIMENSIONS OF ITS OWN: outline, hole position and channel position are read
at build time from the PARAMS lists of hx711_housing.py and hall_housing.py
(via ast, without executing the scripts). If a housing changes, this script is
run again and the plate follows. The same number should not stand in two
places — in this project that was the source of almost all errors.

CABLE CHANNELS: The housings clamp the cable between cable groove and mounting
surface; the clamping section is deliberately a few tenths narrower than the
cable. A 0.5 mm plate underneath would double that and squash the thin
load-cell strands. That is why the plate is cut out under the channels, from
the end of the cavity to the end of the plate, with channel_clear clearance per
side. The cable lies directly on the aluminium there — it is insulated, the
board is not. The cavity itself stays completely covered.

RIM: The plate protrudes rim_w = 1 mm beyond the housing outline and carries a
rim_h = 1 mm high rim there; inside, the housing footprint plus fit_rim
clearance remains as a shallow tray. The housing sits in it and cannot move
sideways when the M6 is tightened. At the cable exits the rim is interrupted by
the channel slots so that it does not kink the cable.

On the HALL HOUSING the rim is missing on the sensor side (x = 0): the end wall
there is only 1.2 mm thick and faces the magnets across the air gap — a
millimetre of rim in front of it would eat into the air gap. The plate ends
flush with the housing at the front, the tray is open to the front; the housing
is slid in from behind and is located on three sides.

Feature tree per plate (object names as in FreeCAD):
    Plate        plate, rectangle rim_w larger than the housing, plate_t + rim_h thick
    Seat          seat, housing footprint + fit_rim, rim_h deep from above -> the tray
    M6_Hole       M6 hole, Ø m6_d, through
    Channel...      one slot per cable channel, through plate and rim, open to the end
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

# The plate's own parameters. Everything else comes from the housing.
PLATE_PARAMS = [
    ('plate_t',        0.5, 'Plate thickness under the housing'),
    ('rim_w',          1.0, 'Rim: overhang beyond the housing outline'),
    ('rim_h',          1.0, 'Rim: height above the plate'),
    ('fit_rim',        0.2, 'Clearance in the tray, TOTAL -> 0.1 per side'),
    ('channel_clear',  0.5, 'Clearance per side at the channel slot'),
]

# Which housing supplies what. The right-hand side are names from its PARAMS.
#   hole_x/hole_y/hole_d  M6
#   chan_x0               start of the channel slots (end of the cavity)
#   channels              list (position y as expression, width as expression)
#   rim_front             rim also on the front side x = 0?
PLATES = {
    'hx711': {
        'source': 'hx711_housing.py',
        'rim_front': True,
        'hole_x': 'screw_x', 'hole_y': 'screw_y', 'hole_d': 'screw_d',
        'chan_x0': '=cav_x0 + cav_l',
        'channels': [('=cab_y',  '=cab_deep_d_p'),
                     ('=-cab_y', '=cab_deep_d_m')],
    },
    'hall': {
        'source': 'hall_housing.py',
        'rim_front': False,   # sensor side: nothing in front of the end wall
        'hole_x': 'm6_x', 'hole_y': 'm6_y', 'hole_d': 'm6_d',
        'chan_x0': '=cav_x0 + cav_l',
        'channels': [('=cab_y', '=cab_d')],
    },
}


def read_params(filename):
    """Read the PARAMS list of a housing script without executing it."""
    src = open(os.path.join(HERE, filename), encoding='utf-8').read()
    tree = ast.parse(src, filename)
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 \
                and getattr(node.targets[0], 'id', None) == 'PARAMS':
            return ast.literal_eval(node.value)
    raise RuntimeError('%s: no PARAMS found' % filename)


def build_sheet(doc, params):
    sh = doc.addObject('Spreadsheet::Sheet', 'Parameter')
    sh.set('A1', 'Parameter')
    sh.set('B1', 'Value')
    sh.set('C1', 'Meaning')
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
    """'=a + b' -> 'Parameter.a + Parameter.b'; a name -> 'Parameter.name'."""
    import re
    s = e[1:] if e.startswith('=') else e
    return re.sub(r'\b([A-Za-z_]\w*)\b', r'Parameter.\1', s)


def value(sh, e):
    """Numeric value of an expression via the table."""
    import re
    s = e[1:] if e.startswith('=') else e
    s = re.sub(r'\b([A-Za-z_]\w*)\b', lambda m: repr(P(sh, m.group(1))), s)
    return float(eval(s, {'__builtins__': {}}, {}))


# ----------------------------------------------------------- Sketch helpers

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


# -------------------------------------------------------------------- Model

def build(doc, sh, spec):
    body = doc.addObject('PartDesign::Body', 'InsulatingPlate')
    planes = {f.Role: f for f in body.Origin.OriginFeatures}
    xy = planes['XY_Plane']

    def pocket_through(sk, name):
        f = body.newObject('PartDesign::Pocket', name)
        f.Profile = sk
        f.Type = 'ThroughAll'
        f.Midplane = True
        doc.recompute()
        return f

    # 1 Plate, rim_w larger than the housing, at the full height plate_t +
    #   rim_h. Same coordinate system as the housing: x = 0 at the end wall,
    #   y = 0 in the middle, z = 0 is the contact face on the profile.
    over = P(sh, 'rim_w') + P(sh, 'fit_rim') / 2        # overhang per side
    e_over = '(Parameter.rim_w + Parameter.fit_rim / 2)'
    x0 = -over if spec['rim_front'] else 0.0
    e_x0 = '-' + e_over if spec['rim_front'] else '0'
    sk = sketch_on(body, xy, 'Sk_Plate')
    rect(sk, x0, -P(sh, 'w_out') / 2 - over,
         P(sh, 'l_out') + over - x0, P(sh, 'w_out') + 2 * over, 'pl',
         e_x0, '-Parameter.w_out / 2 - ' + e_over,
         'Parameter.l_out + %s - (%s)' % (e_over, e_x0),
         'Parameter.w_out + 2 * ' + e_over)
    f = body.newObject('PartDesign::Pad', 'Plate')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.plate_t + Parameter.rim_h')
    doc.recompute()

    # 1a Seat: the housing footprint plus clearance, rim_h deep from above.
    #    Without a rim on the front side the rectangle extends beyond the edge,
    #    the tray is open there.
    fit = P(sh, 'fit_rim') / 2
    sx0 = -fit if spec['rim_front'] else -1.0
    e_sx0 = '-Parameter.fit_rim / 2' if spec['rim_front'] else '-1'
    sk = sketch_on(body, xy, 'Sk_Seat', 'Parameter.plate_t + Parameter.rim_h')
    rect(sk, sx0, -P(sh, 'w_out') / 2 - fit,
         P(sh, 'l_out') + fit - sx0, P(sh, 'w_out') + 2 * fit, 'seat',
         e_sx0, '-Parameter.w_out / 2 - Parameter.fit_rim / 2',
         'Parameter.l_out + Parameter.fit_rim / 2 - (%s)' % e_sx0,
         'Parameter.w_out + Parameter.fit_rim')
    f = body.newObject('PartDesign::Pocket', 'Seat')
    f.Profile = sk
    f.setExpression('Length', 'Parameter.rim_h')
    doc.recompute()

    # 2 M6 hole, same position as in the housing.
    hx, hy, hd = spec['hole_x'], spec['hole_y'], spec['hole_d']
    sk = sketch_on(body, xy, 'Sk_M6')
    circle(sk, P(sh, hx), P(sh, hy), P(sh, hd) / 2, 'm6',
           expr(hx), expr(hy), expr(hd) + ' / 2')
    pocket_through(sk, 'M6_Hole')

    # 3 Channel slots: from the end of the cavity to beyond the end of the plate.
    x0 = value(sh, spec['chan_x0'])
    for i, (e_y, e_w) in enumerate(spec['channels']):
        y, w = value(sh, e_y), value(sh, e_w) + 2 * P(sh, 'channel_clear')
        sk = sketch_on(body, xy, 'Sk_Channel_%d' % (i + 1))
        rect(sk, x0, y - w / 2, P(sh, 'l_out') - x0 + over + 1.0, w, 'k%d' % (i + 1),
             expr(spec['chan_x0']),
             '(%s) - (%s) / 2 - Parameter.channel_clear' % (expr(e_y), expr(e_w)),
             'Parameter.l_out - (%s) + %s + 1' % (expr(spec['chan_x0']), e_over),
             '(%s) + 2 * Parameter.channel_clear' % expr(e_w))
        pocket_through(sk, 'Channel_%d' % (i + 1))

    return body


# FreeCAD does NOT store the GUI visibility in Document.xml but in a
# GuiDocument.xml with one ViewProvider per object. Headless, that file is not
# created -- so a file produced this way opens with default visibilities:
# sketches on, bodies off. We write it into the archive ourselves after saving.

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
        print('ERROR M6 hole missing'); ok = False
    x0 = value(sh, spec['chan_x0'])
    for e_y, e_w in spec['channels']:
        y = value(sh, e_y)
        if s.isInside(Vector(x0 + 1.0, y, P(sh, 'plate_t') / 2), 1e-6, True):
            print('ERROR channel slot missing at y = %.1f' % y); ok = False
        # cavity stays closed: material just before the slot start
        if not s.isInside(Vector(x0 - 0.5, y, P(sh, 'plate_t') / 2), 1e-6, True):
            print('ERROR slot reaches into the cavity'); ok = False
    # tray: housing corner lies on plate_t, the rim beside it stands rim_h higher
    t, h = P(sh, 'plate_t'), P(sh, 'rim_h')
    yc = P(sh, 'w_out') / 2
    if s.isInside(Vector(x0 - 2.0, yc - 0.5, t + h / 2), 1e-6, True):
        print('ERROR tray missing (material above the housing footprint)'); ok = False
    if not s.isInside(Vector(x0 - 2.0, yc + P(sh, 'fit_rim') / 2 + P(sh, 'rim_w') / 2,
                             t + h / 2), 1e-6, True):
        print('ERROR rim missing on the long side'); ok = False
    front = s.isInside(Vector(-P(sh, 'rim_w') / 2, 0.0, t + h / 2), 1e-6, True)
    if front != bool(spec['rim_front']):
        print('ERROR rim on the front side: %s, expected %s'
              % (front, spec['rim_front'])); ok = False
    # web between hole and slots
    for e_y, e_w in spec['channels']:
        y, w = value(sh, e_y), value(sh, e_w) + 2 * P(sh, 'channel_clear')
        web = abs(y - hy) - w / 2 - P(sh, spec['hole_d']) / 2
        print('Web hole-channel %.1f mm' % web)
        if web < 1.5:
            print('ERROR web between hole and channel below 1.5 mm'); ok = False
    return ok


def main():
    for key, spec in PLATES.items():
        params = read_params(spec['source']) + PLATE_PARAMS
        name = 'insulating-plate-' + key
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
        print('== %s  from %s' % (name, spec['source']))
        print('Outer size  %.1f x %.1f x %.1f mm'
              % (bb.XLength, bb.YLength, bb.ZLength))
        print('M6          x = %.1f, y = %.1f, Ø %.1f'
              % (P(sh, spec['hole_x']), P(sh, spec['hole_y']), P(sh, spec['hole_d'])))
        print('Valid       %s' % body.Shape.isValid())
        print('Plausible   %s' % check(body, sh, spec))
        bad = [o.Name for o in doc.Objects if getattr(o, 'State', None)
               and 'Invalid' in o.State]
        print('Invalid     %s' % (bad if bad else 'none'))
        open_ = [o.Name for o in doc.Objects
                 if o.isDerivedFrom('Sketcher::SketchObject')
                 and not getattr(o, 'FullyConstrained', True)]
        print('Sketches    not fully constrained: %s' % (open_ if open_ else 'none'))
        App.closeDocument(doc.Name)


main()
