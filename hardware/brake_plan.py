#!/usr/bin/env python3
"""Circuit detail of the short-circuit brake: coil chain, E-stop, switching module.

The circuit diagram (hardware/schematic.py) shows the brake as one box. This
plan shows what is inside and how the ESP32 triggers it. Proposal of
2026-11-24, docs/04-hardware.md §9 — not built yet.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/brake_plan.py

Produces hardware/brake-plan.pdf and hardware/brake-plan.png.
The wiring wire by wire is in hardware/brake_wiring.py.

TOPOLOGY (builder's decision, 2026-11-24): the three relay coils are in series
on the +bus, the lower end of the chain goes to the E-stop, and from the E-stop
EXACTLY ONE lead goes to the switching module. There a transistor pulls this lead
to ground. Relays energised = motor on the controller = brake released, only if BOTH
are closed: the E-stop (hand) AND the transistor (ESP32, GPIO40 high). If either
goes away — hand, reset, program error, broken wire, bus gone — the chain drops
out and the motor is short-circuited.

DATASHEET VALUES (onsemi/Diotec 2N5551): V_CEO 160 V, I_C 600 mA, h_FE 80 to 250,
V_CE(sat) 0.2 V at I_C = 50 mA / I_B = 5 mA, P_tot 625 mW. With 220 ohm at 3.3 V
about 11 mA of base current flows, forced beta 12 at 140 mA coil current: saturated.
The proof is the measurement U_CE < 0.3 V with the chain energised (test step 3 in
docs/04-hardware.md §9.5). 2N5551 and 1N4007 come from the existing assortment
(decision 2026-11-25). Alternative, if a logic MOSFET is at hand: IRL540N
(100 V, V_GS(th) 1 to 2 V), then 100 ohm into the gate and 10 kohm gate-source.
"""
import schemdraw
from schemdraw import elements as elm

schemdraw.use('matplotlib')

INK = '#1b2328'
MUT = '#4a5560'
PWR = '#9c3524'   # power path / bus voltage
SIG = '#1f6f8b'   # logic 3.3 V
AUX = '#4a7a2a'   # ground / option


def note(d, xy, text, color=MUT, size=7, halign='left', valign='top'):
    d += elm.Label().at(xy).label(text, fontsize=size, color=color,
                                  halign=halign, valign=valign)


with schemdraw.Drawing(show=False) as d:
    d.config(unit=1.6, fontsize=8, lw=1.2, color=INK)

    # ------------------------------------------------------------- title
    note(d, (-6.0, 4.2), 'Short-circuit brake — coil chain, E-stop and switching module',
         INK, 14)
    note(d, (-6.0, 3.55),
         'Proposal 2026-11-24, transistor fixed 2026-11-25, not built yet  ·  Source: hardware/brake_plan.py  ·  '
         'Description docs/04-hardware.md §9', MUT, 7.5)

    # --------------------------------------------------- coil chain on the left
    TOP = (0.0, 2.6)
    d += elm.Dot().at(TOP).color(PWR)
    d += elm.Line().at(TOP).left(1.6).color(PWR)
    note(d, (-1.7, 2.75), '+Bus 36…42 V\nTap: battery terminal + of the controller,\n'
         'i.e. BEHIND the 15 A fuse', PWR, 7, 'right', 'bottom')

    d += elm.Fuse().down().at(TOP).color(PWR)
    note(d, (-0.45, TOP[1] - 0.8), 'F1  500 mA', PWR, 7, 'right', 'center')
    d += elm.Dot(radius=0.06).color(PWR)
    N1 = d.here
    for k in ('K1', 'K2', 'K3'):
        y0 = d.here[1]
        d += elm.Inductor2(loops=3).down()
        note(d, (-0.45, (y0 + d.here[1]) / 2), f'{k}  coil 12 V', INK, 7, 'right', 'center')
    d += elm.Dot(radius=0.06).color(PWR)
    NA = d.here        # lower end of the coil chain
    note(d, (-0.45, NA[1]), 'Node A', MUT, 6.5, 'right', 'center')

    # Flyback diode parallel to the CHAIN, not across the E-stop as well
    d += elm.Line().at(NA).right(1.6)
    d += elm.Diode().up().toy(N1[1]).label('D1  1N4007', loc='right', fontsize=7)
    d += elm.Line().left(1.6)
    note(d, (1.9, (N1[1] + NA[1]) / 2 - 0.3),
         'Cathode to +bus, anode to node A.\nONLY across the coils — if it also spanned\n'
         'the E-stop, the coil current would have\nno path when the button opens.',
         MUT, 6.5)

    # E-stop: positive-opening break contact (NC) below the chain
    d += elm.Button(nc=True).down().at(NA).color(PWR)
    NB = d.here
    note(d, (-0.9, (NA[1] + NB[1]) / 2),
         'S1  E-STOP\npositive-opening, break contact (NC)\nmushroom button on the frame', PWR, 7, 'right', 'center')
    d += elm.Dot(radius=0.06).color(PWR)

    # ------------------------------------------------ the ONE lead
    WIRE_END = (NB[0] + 8.0, NB[1])
    d += elm.Line().at(NB).to(WIRE_END).color(PWR).linewidth(2.2)
    note(d, (NB[0] + 0.4, NB[1] + 0.12),
         'Lead 1 — the ONE wire from the E-stop to the switching module, 0.5 mm². '
         'Carries BUS VOLTAGE while the relays are dropped out: insulate like the power path.',
         PWR, 6.8, 'left', 'bottom')

    # --------------------------------------------------- switching module on the right
    d += elm.Dot(radius=0.06).at(WIRE_END).color(PWR)
    Q1 = elm.BjtNpn(circle=False).scale(1.5).at(WIRE_END).anchor('collector')
    d += Q1
    note(d, (Q1.collector[0] + 1.6, Q1.collector[1] - 1.0),
         'Q1  2N5551\nNPN · 160 V / 600 mA · TO-92\n(alternative: IRL540N, see below)', INK, 7, 'left', 'center')
    d += elm.Line().at(Q1.emitter).down(0.6)
    SRC = d.here
    d += elm.Dot(radius=0.06)
    d += elm.Line().down(0.6)
    d += elm.Ground()
    note(d, (SRC[0] + 0.3, SRC[1] - 0.95),
         'Bus −  (star point)\nown wire to the star point,\nnot via the GND pin of the DevKit',
         AUX, 6.5, 'left', 'top')

    # gate network
    d += elm.Line().at(Q1.base).left(1.4)
    G = d.here
    d += elm.Dot(radius=0.06)
    d += elm.Resistor().left().at(G).label('R1  220 Ω', loc='top', fontsize=7).color(SIG)
    d += elm.Line().left(0.8).color(SIG)
    d += elm.Dot(open=True).color(SIG)
    PIN = d.here
    note(d, (PIN[0] - 0.2, PIN[1] + 0.35),
         'GPIO40  PIN_BRAKE_RELEASE\nhigh = relays energised = brake released\n'
         'low / reset / open = braked', SIG, 7, 'right', 'top')
    d += elm.Resistor().down().at(G).toy(SRC[1]).label('R2  10 kΩ', loc='left', fontsize=7).color(SIG)
    d += elm.Line().to(SRC).color(SIG)
    note(d, (G[0] - 0.4, SRC[1] - 0.95),
         'R2 (base–emitter) holds the base at ground during reset,\neven against an internal pull-up (45 kΩ): 10 kΩ, not 100 kΩ.\n'
         'R1 220 Ω: ≈ 11 mA base current, forced β ≈ 12 at 140 mA.', MUT, 6.5, 'right', 'top')

    # option feedback: divider at the drain node
    d += elm.Line().at(WIRE_END).right(6.0).color(AUX).linestyle('--')
    d += elm.Resistor().down().label('R3  120 kΩ', loc='left', fontsize=7).color(AUX).linestyle('--')
    FB = d.here
    d += elm.Dot(radius=0.06).color(AUX)
    d += elm.Resistor().down().label('R4  10 kΩ', loc='left', fontsize=7).color(AUX).linestyle('--')
    d += elm.Ground().color(AUX)
    d += elm.Line().at(FB).right(1.4).color(AUX).linestyle('--')
    ZT = d.here
    d += elm.Dot(radius=0.06).color(AUX)
    d += elm.Line().right(0.8).color(AUX).linestyle('--')
    d += elm.Dot(open=True).color(AUX)
    note(d, (d.here[0] + 0.15, d.here[1] + 0.05), 'GPIO2  PIN_ESTOP_FB\n(option)', AUX, 7, 'left', 'center')
    d += elm.Zener().down().reverse().at(ZT).label('Z1  3.3 V', loc='right', fontsize=6.5).color(AUX).linestyle('--')
    d += elm.Ground().color(AUX)
    note(d, (WIRE_END[0] + 5.4, FB[1] - 2.6),
         'OPTION, no extra wire: the level at the collector is meaningful as long as GPIO40 is low.\n'
         'HIGH = bus present AND E-stop not pressed.  LOW = E-stop pressed or bus gone.\n'
         'Always LOW with GPIO40 high. Meant as a start check, not as E-stop detection during a run.\n'
         'Divider: 42 V → 3.23 V; 48 V (l_max_vin) → 3.69 V, hence Z1 as on the Hall inputs.',
         MUT, 6.5)

    # ------------------------------------------- contact side of one relay
    CX, CY = -6.0, NB[1] - 4.5
    note(d, (CX, CY), 'CONTACT SIDE, the same for each relay (docs/04-hardware.md §9.1)', INK, 8)
    note(d, (CX, CY - 0.45),
         '30   common        → motor phase (U, V, W — one relay each)\n'
         '87   make (NO)     → phase terminal of the controller (energised: motor on the controller)\n'
         '87a  break (NC)    → short-circuit node, all three 87a joined (dropped out: motor short-circuited)\n'
         '85 / 86  coil      → chain K1–K2–K3 as above, any polarity (use relays WITHOUT built-in diode)\n'
         'Phase leads ≥ 4 mm². The short-circuit node must NOWHERE be connected to a controller terminal (§10.6).',
         INK, 6.8)

    FY = CY - 2.7
    note(d, (CX, FY), 'SEQUENCE (firmware proposal, docs/04-hardware.md §9.4)', INK, 8)
    note(d, (CX, FY - 0.45),
         'Release:   GPIO40 high → wait RELAY_SETTLE_MS → only then drive the controller.\n'
         'Stop above RELAY_BRAKE_ABOVE_KMH (40 km/h):   release the controller (SET_CURRENT 0) → wait RELAY_DRIVER_OFF_MS → GPIO40 low.\n'
         '         MUTE the E-stop detection (no motor on the controller) meanwhile — the finding is intended now, not a FAULT.\n'
         '         Speed from the drum marks (GPIO4), not from the controller: without phases its observer sees nothing.\n'
         'Stop below that as before:   controller brakes with 20 A (BRAKE_MAX_KMH = 45, margin to the measured 54 km/h).\n'
         'Hand E-stop during a run:   unchanged — relays drop, the controller reports no more current, the detection rightly triggers.',
         INK, 6.8)

    note(d, (CX, FY - 3.2),
         'ALTERNATIVE to Q1:  IRL540N (N-MOSFET, 100 V, logic gate, V_GS(th) 1–2 V), then R1 = 100 Ω into the gate and R2 = 10 kΩ gate–source; drain to lead 1, source to bus −.\n'
         'The 1 kΩ base resistor that was here until 2026-11-24 is not enough to saturate the 2N5551 at 140 mA (the datasheet gives h_FE only up to 50 mA).\n'
         'NOT suitable:  2N2222 (40 V), BC337/BC547 (45 V), BC517 (30 V), D4184 modules (40 V), IRLZ44N (55 V), IRF520 (no logic gate) — too little margin to 42 V bus or not switchable with 3.3 V.',
         MUT, 6.5)

    d.save('hardware/brake-plan.pdf')
    d.save('hardware/brake-plan.png', dpi=130)
print('ok')
