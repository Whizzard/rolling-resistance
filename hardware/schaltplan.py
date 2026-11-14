#!/usr/bin/env python3
"""Schaltplan des Rollwiderstandspruefstands.

Quelle ist dieser Text, das PDF wird daraus erzeugt:

    .venv/bin/python hardware/schaltplan.py

Erzeugt hardware/schaltplan.pdf und hardware/schaltplan.png.
Pinbelegung siehe docs/04-hardware.md; Aenderungen hier und dort gemeinsam.
"""
import schemdraw
from schemdraw import elements as elm, flow

schemdraw.use('matplotlib')

INK   = '#1b2328'
PWR   = '#9c3524'   # Leistungspfad
SIG   = '#1f6f8b'   # Messsignale
AUX   = '#4a7a2a'   # Versorgung Logik

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

    # ---------------------------------------------------------------- Titel
    d += elm.Label().at((-2.0, 3.4)).label(
        'Rollwiderstandsprüfstand — Verdrahtung', fontsize=15, color=INK)
    d += elm.Label().at((-2.0, 2.7)).label(
        'ESP32-S3-DevKitC-1 N16R8  ·  Stand 2026-11-14  ·  Quelle: hardware/schaltplan.py',
        fontsize=7.5, color='#4a5560')

    # ---------------------------------------------------- Leistungspfad oben
    d += elm.Label().at((-12.0, 1.4)).label('LEISTUNGSPFAD  36 V', fontsize=8,
                                            color=PWR, halign='left')
    nt  = box(d, -12.0,  0.0, 2.6, 1.1, 'Netzteil\n36 V / 10 A', PWR)
    na  = box(d,  -8.6,  0.0, 2.8, 1.1, 'NOT-AUS\nzwangsöffnend', PWR, lw=2.6)
    si  = box(d,  -5.0,  0.0, 1.9, 1.1, 'Sicherung\n15 A', PWR)
    reg = box(d,  -2.3,  0.0, 2.9, 1.1, 'FSESC 4.20 / 50 A\n8…60 V · UART', PWR)
    mot = box(d,   1.4,  0.0, 2.9, 1.1, 'Nabenmotor\n36 V / 350 W', PWR)
    chop= box(d,   5.1,  0.0, 3.0, 1.1, 'mech. Trommelbremse\nim Motor  (A8)', PWR)

    wire(d, nt.E,  na.W,  '36 V', PWR)
    wire(d, na.E,  si.W,  None,   PWR)
    wire(d, si.E,  reg.W, None,   PWR)
    wire(d, reg.E, mot.W, '3 Ph.', PWR)
    d += elm.Line().at(mot.E).to(chop.W).color(PWR).linestyle(':').linewidth(1.1)
    d += elm.Label().at((6.6, -1.0)).label(
        'A8 zurückgestellt: kein Bremswiderstand,\nkein Kurzschlussschütz. Frei auslaufen\nlassen, zum Feststellen die Trommelbremse.',
        fontsize=6.8, color='#4a5560')

    # --------------------------------------------------- Routing-Kanal oben
    CH = -1.5                      # waagerechter Kanal fuer Signale nach oben
    d += elm.Label().at((-12.0, -3.4)).label('MESSSIGNALE', fontsize=8,
                                                color=SIG, halign='left')

    # ------------------------------------------------------------- ESP32
    ESPX, ESPY = -2.6, -7.3
    esp = flow.Box(w=5.2, h=6.4).at((ESPX, ESPY)).label(
        'ESP32-S3\nDevKitC-1  N16R8\n\nWLAN + BLE\n3,3 V Logik', fontsize=9.5
        ).color(INK).linewidth(2.2)
    d += esp
    d += elm.Label().at((ESPX+2.6, ESPY-3.65)).label('USB-C → PC', fontsize=7.5, color='#4a5560')

    # --------------------------------------------------- Sensorik links
    zelle = box(d, -12.0, -4.4, 2.8, 1.1, 'Wägezelle 10 kg\nBiegebalken', SIG)
    hx1   = box(d,  -8.4, -4.4, 2.4, 1.1, 'HX711 #1  Moment\nVCC 5 V · VDD 3,3 V', SIG)
    zelle2= box(d, -12.0, -6.1, 2.8, 1.1, 'Wägezelle 2\nAuflast (später)', SIG)
    hx2   = box(d,  -8.4, -6.1, 2.4, 1.1, 'HX711 #2  Auflast\nVCC 5 V · VDD 3,3 V', SIG)
    hall1 = box(d, -12.0, -7.6, 2.8, 1.0, 'TLE4905L  × 8\nTrommelmarken', SIG)
    hall2 = box(d, -12.0, -8.9, 2.8, 1.0, 'TLE4905L\nIndexmarke', SIG)
    hall3 = box(d, -12.0,-10.2, 2.8, 1.0, 'TLE4905L\nLaufraddrehzahl', SIG)

    wire(d, zelle.E,  hx1.W, None, SIG)
    d += elm.Line().at(zelle2.E).to(hx2.W).color(SIG).linestyle('--').linewidth(1.1)
    wire(d, hx1.E, (ESPX, hx1.E[1]), 'DOUT 15 · SCK 16', SIG)
    wire(d, hx2.E, (ESPX, hx2.E[1]), 'DOUT 17 · SCK 16', SIG)
    wire(d, hall1.E, (ESPX, hall1.E[1]), 'GPIO4  MARK', SIG)
    wire(d, hall2.E, (ESPX, hall2.E[1]), 'GPIO5  INDEX', SIG)
    wire(d, hall3.E, (ESPX, hall3.E[1]), 'GPIO6  WHEEL', SIG)

    # ------------------------------------------------------ Peripherie rechts
    RX = ESPX + 5.2
    d += elm.Label().at((RX+2.2, -3.4)).label('BEDIENUNG · PROTOKOLL', fontsize=8,
                                                 color=AUX, halign='left')
    lvl = box(d, RX+2.2, -4.4, 2.3, 1.0, 'I²C-Pegel-\nwandler', AUX)
    lcd = box(d, RX+5.4, -4.4, 2.7, 1.0, 'LCD 2004\nPCF8574  5 V', AUX)
    sd  = box(d, RX+2.2, -6.1, 2.7, 1.0, 'microSD\nSPI 3,3 V', AUX)
    tmp = box(d, RX+2.2, -7.8, 2.7, 1.0, 'Temperatur\nI²C   (TBD)', AUX)
    btn = box(d, RX+2.2, -9.2, 2.7, 1.0, 'Taster + LED', AUX)
    ble = box(d, RX+5.4, -9.2, 2.7, 1.0, 'BLE-Drucksensor\n(drahtlos)', AUX)

    wire(d, (RX, lvl.W[1]), lvl.W, 'SDA 8 · SCL 9', AUX)
    wire(d, lvl.E, lcd.W, None, AUX)
    wire(d, (RX, sd.W[1]),  sd.W,  'SPI  CS 10', AUX)
    d += elm.Line().at((RX, tmp.W[1])).to(tmp.W).color(AUX).linestyle('--').linewidth(1.1)
    wire(d, (RX, btn.W[1]), btn.W, 'GPIO14 · 48', AUX)
    d += elm.Line().at(btn.E).to(ble.W).color(AUX).linestyle(':').linewidth(1.1)

    # ------------------------- Signale vom ESP32 nach oben, rechtwinklig
    def up(x_from, y_from, x_to, y_to, label, color, ls='-', dy=0.0):
        d.add(elm.Line().at((x_from, y_from)).to((x_from, CH+dy)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Line().at((x_from, CH+dy)).to((x_to, CH+dy)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Line().at((x_to, CH+dy)).to((x_to, y_to)).color(color).linestyle(ls).linewidth(1.1))
        d.add(elm.Label().at(((x_from+x_to)/2, CH+dy+0.28)).label(label, fontsize=7, color=color))

    up(ESPX+1.4, ESPY+3.2, reg.S[0], reg.S[1], 'UART1   TX 18 · RX 21', SIG, dy=0.0)
    up(ESPX+3.6, ESPY+3.2, mot.S[0], mot.S[1], 'Motor-Hall GPIO7 · PTC GPIO1 (ADC1)', SIG, dy=-0.75)
    up(ESPX+0.4, ESPY+3.2, na.S[0],  na.S[1],  'Not-Aus-Rückmeldung GPIO2 (nur Anzeige)', PWR, ls=':', dy=-1.5)

    # --------------------------------------------------------- Fussnoten
    d += elm.Label().at((-12.0, -12.0)).label(
        'Masse:  Logik- und Leistungsmasse an GENAU EINEM Punkt verbinden (Sternmasse am Netzteil).',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -12.45)).label(
        'HX711:  Modul mit GETRENNTEN VCC/VDD. VCC = 5 V speist die Bruecke, VDD = 3,3 V setzt den Logikpegel \u2014 der ESP32-S3 ist nicht 5-V-tolerant.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -12.9)).label(
        'TLE4905L:  V_S aus 5 V (min. 3,8 V), Open Collector mit Pull-up 2,2…4,7 kΩ nach 3,3 V, 100 nF am Sensor.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -13.35)).label(
        'Nicht nutzbar am N16R8:  GPIO26…37 (Flash + Octal-PSRAM), GPIO19/20 (USB), GPIO0/45/46 (Strapping).',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -13.80)).label(
        'Speisung:  Logik AUSSCHLIESSLICH ueber USB-C \u2014 5V-Pin und USB schliessen sich aus. Beide Buchsen duerfen gleichzeitig speisen:',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -14.25)).label(
        '           Netzteil an der einen, Notebook zum Flashen an der anderen. Der 5V-Pin ist dann AUSGANG (VBUS) fuer Geber, HX711-VCC und LCD.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -14.70)).label(
        'HX711-Analogteil:  Ferrit und 100 uF direkt am AVDD-Pin, Sternmasse, nicht am Motorzweig.',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -15.15)).label(
        'FSESC 4.20:  UART an der Stiftleiste COMM (5V · 3.3V · − · ADC · TX · RX · ADC2). Nur TX, RX und Masse anschliessen — die 5 V des Reglers NICHT,',
        fontsize=7.5, color='#4a5560', halign='left')
    d += elm.Label().at((-12.0, -15.60)).label(
        '           der Logikteil haengt am USB. Motor-Hall und TMP liegen an der Leiste SENSE (− · H3 · H2 · H1 · TMP · 5V), siehe docs/04-hardware.md §10.',
        fontsize=7.5, color='#4a5560', halign='left')

    d.save('hardware/schaltplan.pdf')
    d.save('hardware/schaltplan.png', dpi=130)
print('ok')
