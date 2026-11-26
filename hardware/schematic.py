#!/usr/bin/env python3
"""Circuit diagram of the rolling-resistance test stand.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/schematic.py

Produces hardware/schematic.pdf and hardware/schematic.png.
Pin assignment see docs/04-hardware.md; change here and there together.
"""
import schemdraw
from schemdraw import elements as elm, flow

schemdraw.use('matplotlib')

INK   = '#1b2328'
PWR   = '#9c3524'   # power path
SIG   = '#1f6f8b'   # measurement signals
AUX   = '#4a7a2a'   # logic supply

def box(d, x, y, w, h, text, color=INK, lw=1.6):
    b = flow.Box(w=w, h=h).at((x, y)).label(text, fontsize=8.5).color(color).linewidth(lw)
    d += b
    return b

def wire(d, p1, p2, label=None, color=SIG, ls='-', loc='top', ofst=0.12):
    ln = elm.Line().at(p1).to(p2).color(color).linewidth(1.1).linestyle(ls)
    if label:
        ln = ln.label(label, loc=loc, fontsize=7, ofst=ofst, color=color)
    d += ln
    return ln

with schemdraw.Drawing(show=False) as d:
    d.config(unit=1.0, fontsize=9)

    # ---------------------------------------------------------------- title
    d += elm.Label().at((-2.0, 3.4)).label(
        'Rolling-resistance test stand — wiring', fontsize=15, color=INK)
    d += elm.Label().at((-2.0, 2.7)).label(
        'ESP32-S3-DevKitC-1 N16R8  ·  As of 2026-11-24  ·  Source: hardware/schematic.py',
        fontsize=7.5, color='#4a5560')

    # ---------------------------------------------------- power path at the top
    d += elm.Label().at((-12.0, 1.4)).label('POWER PATH  36 V', fontsize=8,
                                            color=PWR, halign='left')
    nt  = box(d, -12.0,  0.0, 2.6, 1.1, 'Power supply\n36 V / 10 A', PWR)
    si  = box(d,  -8.6,  0.0, 1.9, 1.1, 'Fuse\n15 A', PWR)
    ctrl = box(d,  -5.9,  0.0, 2.9, 1.1, 'FSESC 4.20 / 50 A\n8…60 V · UART', PWR)
    mot = box(d,  -2.2,  0.0, 2.9, 1.1, 'Hub motor\n36 V / 350 W', PWR)
    brk = box(d,   1.5,  0.0, 3.0, 1.1, 'Short-circuit brake\n3 × relay 12 V  (§9)', PWR)
    na  = box(d,   5.3,  0.0, 2.8, 1.1, 'E-STOP\npositive-opening, in\nthe coil chain', PWR, lw=2.6)

    wire(d, nt.E,  si.W,  '36 V', PWR)
    wire(d, si.E,  ctrl.W, None,   PWR)
    wire(d, ctrl.E, mot.W, '3 ph.', PWR)
    wire(d, mot.E, brk.W, '3 ph.', PWR)
    wire(d, brk.E, na.W,  'Coils', PWR)
    d += elm.Label().at((8.5, 0.15)).label(
        'Closed when de-energised = braked: coil chain\non the bus, E-stop AND transistor (GPIO40) must\nclose. Detail: hardware/brake-plan.pdf.\n'
        'Measured 25 s from above 400 rpm. NO\nmechanical brake — parking: wheel chock.',
        fontsize=6.8, color='#4a5560', halign='left')

    # --------------------------------------------------- routing channel at the top
    CH = -1.5                      # horizontal channel for signals going up
    d += elm.Label().at((-12.0, -3.4)).label('MEASUREMENT SIGNALS', fontsize=8,
                                                color=SIG, halign='left')

    # ------------------------------------------------------------- ESP32
    ESPX, ESPY = -2.6, -7.3
    esp = flow.Box(w=5.2, h=6.4).at((ESPX, ESPY)).label(
        'ESP32-S3\nDevKitC-1  N16R8\n\nWLAN + BLE\n3.3 V logic', fontsize=9.5
        ).color(INK).linewidth(2.2)
    d += esp
    d += elm.Label().at((ESPX+2.6, ESPY-3.65)).label('USB-C → PC', fontsize=7.5, color='#4a5560')

    # --------------------------------------------------- sensors on the left
    cell = box(d, -12.0, -4.4, 2.8, 1.1, 'Load cell 5 kg\nbeam  L = 80 mm', SIG)
    hx1   = box(d,  -8.4, -4.4, 2.4, 1.1, 'HX711 #1  torque\nVCC 5V · VDD 3.3V', SIG)
    cell2= box(d, -12.0, -6.1, 2.8, 1.1, 'Load cell 2\nwheel load (later)', SIG)
    hx2   = box(d,  -8.4, -6.1, 2.4, 1.1, 'HX711 #2  load\nVCC 5V · VDD 3.3V', SIG)
    hall1 = box(d, -12.0, -7.6, 2.8, 1.0, 'TLE4905L  × 8\ndrum marks', SIG)
    hall2 = box(d, -12.0, -8.9, 2.8, 1.0, 'TLE4905L\nindex mark', SIG)
    hall3 = box(d, -12.0,-10.2, 2.8, 1.0, 'TLE4905L\nwheel speed', SIG)

    wire(d, cell.E,  hx1.W, None, SIG)
    d += elm.Line().at(cell2.E).to(hx2.W).color(SIG).linestyle('--').linewidth(1.1)
    wire(d, hx1.E, (ESPX, hx1.E[1]), 'DOUT 15 · SCK 16', SIG)
    wire(d, hx2.E, (ESPX, hx2.E[1]), 'DOUT 17 · SCK 16', SIG)
    wire(d, hall1.E, (ESPX, hall1.E[1]), 'GPIO4  MARK', SIG)
    wire(d, hall2.E, (ESPX, hall2.E[1]), 'GPIO5  INDEX', SIG)
    wire(d, hall3.E, (ESPX, hall3.E[1]), 'GPIO6  WHEEL', SIG)

    # ------------------------------------------------------ peripherals on the right
    RX = ESPX + 5.2
    d += elm.Label().at((RX+2.2, -3.4)).label('OPERATION · LOGGING', fontsize=8,
                                                 color=AUX, halign='left')
    ui  = box(d, RX+2.2, -4.4, 3.4, 1.0, 'Control panel via WLAN\nphone / iPad', AUX)
    sd  = box(d, RX+2.2, -6.1, 2.7, 1.0, 'microSD\nSPI 3.3 V', AUX)
    tmp = box(d, RX+2.2, -7.8, 2.7, 1.0, 'Temperature\nI²C   (TBD)', AUX)
    ble = box(d, RX+2.2, -9.2, 2.7, 1.0, 'BLE pressure sensor\n(wireless)', AUX)

    d += elm.Line().at((RX, ui.W[1])).to(ui.W).color(AUX).linestyle(':').linewidth(1.1)
    wire(d, (RX, sd.W[1]),  sd.W,  'SPI 10-13', AUX)
    d += elm.Line().at((RX, tmp.W[1])).to(tmp.W).color(AUX).linestyle('--').linewidth(1.1)
    d += elm.Line().at((RX, ble.W[1])).to(ble.W).color(AUX).linestyle(':').linewidth(1.1)

    # ------------------------- signals from the ESP32 upwards, at right angles
    def up(x_from, y_from, x_to, y_to, label, color, ls='-', dy=0.0, lx=None):
        d.add(elm.Line().at((x_from, y_from)).to((x_from, CH+dy)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Line().at((x_from, CH+dy)).to((x_to, CH+dy)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Line().at((x_to, CH+dy)).to((x_to, y_to)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Label().at((lx if lx is not None else (x_from+x_to)/2, CH+dy+0.28)).label(
            label, fontsize=7, color=color))

    up(ESPX+1.4, ESPY+3.2, ctrl.S[0], ctrl.S[1], 'UART1   TX 18 · RX 21', SIG, dy=0.0)
    up(ESPX+3.6, ESPY+3.2, mot.S[0], mot.S[1], 'Motor Hall GPIO7 · TMP', SIG, dy=-0.75, lx=0.6)
    up(ESPX+4.2, ESPY+3.2, brk.S[0]-0.6, brk.S[1], 'Feedback GPIO2 (option, coil-chain level)', PWR, ls=':', dy=-1.5, lx=6.2)
    up(ESPX+4.8, ESPY+3.2, brk.S[0]+1.0, brk.S[1], 'Brake release GPIO40', SIG, dy=-2.25, lx=2.85)

    # --------------------------------------------------------- footnotes
    d += elm.Label().at((-12.0, -12.0)).label(
        'Ground:  connect logic and power ground at EXACTLY ONE point (star ground at the power supply).',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -12.45)).label(
        'HX711:  module with SEPARATE VCC/VDD. VCC = 5 V feeds the bridge, VDD = 3.3 V sets the logic level — the ESP32-S3 is not 5 V tolerant.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -12.9)).label(
        'TLE4905L:  V_S from 5 V (min. 3.8 V), open collector. Level via pull-up 470 ohm to 5 V and Zener diode 3.3 V AT THE ESP32, 4.7 nF each at the sensor,',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -13.33)).label(
        '           green LED in the sensor housing. Soldering and connection template: hardware/hall-plan.pdf.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -17.85)).label(
        'Not usable on the N16R8:  GPIO26…37 (flash + octal PSRAM), GPIO19/20 (USB), GPIO0/45/46 (strapping).',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -13.80)).label(
        'Supply:  since 2026-11-25 the logic is fed via the 5V pin from an XL7015 buck on the bus (5-80 V in, 0.8 A, common ground; docs/04-hardware.md §5.1), not via USB-C. Behind each USB socket sits a Schottky (D1, D7) to VCC_5V --',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -14.25)).label(
        '           back-feeding into the notebook is thus impossible, the notebook may stay on COM. There is NO protection diode before the 5V pin: polarity and voltage of the source are up to the supply lead.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -14.70)).label(
        'HX711 analogue section:  ferrite and 100 uF directly at the AVDD pin, star ground, not on the motor branch.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -15.15)).label(
        'FSESC 4.20:  UART on the COMM pin header (5V · 3.3V · − · ADC · TX · RX · ADC2). Connect only TX, RX and ground — NOT the 5 V of the controller,',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -16.05)).label(
        'Brake:  coil chain on the bus, E-stop at the bottom, then ONE lead to the 2N5551 at the ESP (GPIO40 high = energised = released; 10 kohm base-emitter, braked on reset). Detail: hardware/brake_plan.py.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -15.60)).label(
        '           the logic part hangs on USB. Motor Hall and TMP are on the SENSE header (− · H3 · H2 · H1 · TMP · 5V), see docs/04-hardware.md §10.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -17.40)).label(
        'Motor temperature:  sensor on TMP of the CONTROLLER, not on the ESP32 — a resistive sensor tolerates only ONE pull-up. The ESP32 reads the value via UART.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -16.50)).label(
        'Operation:  no D-pad and no LCD any more — operated via WLAN from the phone. Decided on 2026-11-22, docs/04-hardware.md §13.',
        fontsize=7.5, color='#4a5560', halign='left')

    d.save('hardware/schematic.pdf')
    d.save('hardware/schematic.png', dpi=130)
print('ok')
