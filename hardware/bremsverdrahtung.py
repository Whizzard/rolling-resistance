#!/usr/bin/env python3
"""Wiring of the short-circuit brake — wire by wire.

The circuit detail (hardware/bremsplan.py) shows WHAT is connected to what. This
plan shows HOW: every part with its terminals and every wire individually with
signal names. Proposal of 2026-11-24, docs/04-hardware.md §9 — not built yet.

The source is this text; the PDF is generated from it:

    .venv/bin/python hardware/bremsverdrahtung.py

Produces hardware/bremsverdrahtung.pdf and hardware/bremsverdrahtung.png.

PRINCIPLE as in the connection plan: pin numbers are given only where they are certain.
The terminal designations 30 / 85 / 86 / 87 / 87a are those of automotive changeover
relays (DIN 72552); whether the coil terminals 85 and 86 of the purchased relay contain
a diode must be checked on the part BEFORE installation (relays with a diode have a fixed
coil polarity and do not fit a series connection with a common flyback diode).
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # logic 3.3 V
AUX  = "#4a7a2a"   # ground / option
PWR  = "#9c3524"   # bus voltage / power path
LINE = "#c9d2d8"

PITCH = 0.62
HEAD  = 1.25
FOOT  = 0.35
W     = 16.0
MONO  = {"family": "DejaVu Sans Mono"}
ZEILE = 0.40

fig, ax = plt.subplots(figsize=(13.5, 19))


def modul(x, ytop, w, titel, untertitel, links=None, rechts=None, farbe=INK):
    links  = links or []
    rechts = rechts or []
    rows = max(len(links), len(rechts))
    h = HEAD + PITCH * rows + FOOT
    ax.add_patch(FancyBboxPatch((x, ytop - h), w, h,
                                boxstyle="round,pad=0,rounding_size=0.12",
                                linewidth=1.8, edgecolor=farbe, facecolor="white", zorder=2))
    ax.text(x + 0.25, ytop - 0.42, titel, fontsize=10.5, weight="bold", color=farbe,
            va="center", zorder=3)
    if untertitel:
        ax.text(x + 0.25, ytop - 0.85, untertitel, fontsize=7.4, color=MUT,
                va="center", zorder=3)
    anker = {}
    for seite, pins in (("links", links), ("rechts", rechts)):
        for i, (nr, name, pc) in enumerate(pins):
            py = ytop - HEAD - i * PITCH
            kante = x if seite == "links" else x + w
            if nr is not None:
                bx = x + 0.10 if seite == "links" else x + w - 0.50
                ax.add_patch(Rectangle((bx, py - 0.17), 0.40, 0.34, linewidth=0.9,
                                       edgecolor=MUT, facecolor="#eef2f4", zorder=3))
                ax.text(bx + 0.20, py, str(nr), fontsize=7.2, ha="center", va="center",
                        color=INK, zorder=4, **MONO)
            tx = x + 0.64 if seite == "links" else x + w - 0.64
            ax.text(tx, py, name, fontsize=8.2, color=pc, zorder=3, **MONO,
                    ha="left" if seite == "links" else "right", va="center")
            sx = kante - 0.30 if seite == "links" else kante + 0.30
            ax.plot([kante, sx], [py, py], color=pc, linewidth=1.3, zorder=2)
            anker[name] = (sx, py)
    return anker, h


def strang(paare):
    n = len(paare)
    xs = [p[0][0] for p in paare] + [p[1][0] for p in paare]
    x_links, x_rechts = min(xs), max(xs)
    spanne = x_rechts - x_links
    for i, (a, b, label, farbe) in enumerate(paare):
        (x1, y1), (x2, y2) = a, b
        xm = x_links + spanne * (i + 1) / (n + 1)
        ax.plot([x1, xm, xm, x2], [y1, y1, y2, y2], color=farbe, linewidth=1.5,
                zorder=1, solid_capstyle="round")
        ax.text(xm + 0.10, (y1 + y2) / 2, label, fontsize=7.4, color=farbe,
                va="center", ha="left", zorder=4, **MONO,
                bbox=dict(boxstyle="round,pad=0.14", fc="white", ec="none"))


def ueberschrift(y, nummer, titel, hinweis=None):
    y -= 0.35
    ax.plot([0.2, W - 0.2], [y, y], color=LINE, linewidth=2.4, zorder=0)
    ax.text(0.2, y - 0.45, f"{nummer}  {titel}", fontsize=11.5, weight="bold",
            color=INK, va="center")
    if hinweis:
        ax.text(W - 0.2, y - 0.45, hinweis, fontsize=7.8, color=PWR,
                ha="right", va="center")
    return y - 1.05


def fussnote(y, text, farbe=PWR):
    ax.text(0.4, y, text, fontsize=7.8, color=farbe, va="top", linespacing=1.5)
    return y - ZEILE * (text.count("\n") + 1) - 0.30


# ============================================================ header
y = 0.0
ax.text(0.2, y, "Short-circuit brake — wiring", fontsize=17, color=INK, va="top")
y -= 0.70
ax.text(0.2, y, "Proposal 2026-11-24, transistor 2N5551 fixed 2026-11-25, not built yet  ·  Source: hardware/bremsverdrahtung.py  ·  "
                "Circuit hardware/bremsplan.pdf  ·  Text docs/04-hardware.md §9",
        fontsize=8, color=MUT, va="top")
y -= 0.45

# ============================================================ 1 coil chain
y = ueberschrift(y, "1", "Coil chain: +bus → F1 → K1 → K2 → K3 → E-stop → lead 1",
                 "140 mA, but BUS VOLTAGE — power-path insulation")

bus, h1 = modul(0.4, y, 3.8, "Controller FSESC 4.20", "battery terminals, behind 15 A fuse",
                rechts=[(None, "BATT +", PWR), (None, "BATT −  (star point)", AUX)])

rel, h2 = modul(7.2, y, 3.8, "Relay block K1 K2 K3", "coil side, terminals 85 / 86",
                links=[(None, "F1 500 mA", PWR),
                       (86, "K1 coil", INK), (85, "K1 coil", INK),
                       (86, "K2 coil", INK), (85, "K2 coil", INK),
                       (86, "K3 coil", INK), (85, "K3 coil  = node A", INK)],
                rechts=[(None, "D1 cathode", INK), (None, "D1 anode", INK)])

na, h3 = modul(12.4, y, 3.4, "S1 E-stop", "break contact, positive-opening",
               links=[(None, "NC in", PWR), (None, "NC out → lead 1", PWR)])

# Bus+ -> F1 ; chain inside the relay block as jumpers
strang([(bus["BATT +"], rel["F1 500 mA"], "+bus, 0.5 mm²", PWR)])
# jumpers F1->86, 85->86, 85->86 as short loops left of the block
xb = rel["F1 500 mA"][0] - 0.55
x0, y_f1 = rel["F1 500 mA"]
pin_y = [y_f1 - PITCH * i for i in range(1, 7)]   # 86/85 K1, 86/85 K2, 86/85 K3
for a, b, txt in (((x0, y_f1), (x0, pin_y[0]), "F1 → K1·86"),
                  ((x0, pin_y[1]), (x0, pin_y[2]), "K1·85 → K2·86"),
                  ((x0, pin_y[3]), (x0, pin_y[4]), "K2·85 → K3·86")):
    ax.plot([a[0], xb, xb, b[0]], [a[1], a[1], b[1], b[1]], color=INK, linewidth=1.3, zorder=1)
    ax.text(xb - 0.1, (a[1] + b[1]) / 2, txt, fontsize=6.8, color=MUT, ha="right", va="center", **MONO)
# node A -> E-stop
kA = rel["K3 coil  = node A"]
xk, xl = 11.4, xb - 0.45
y_unten = y - h2 - 0.35                      # around below the relay block, not through it
ax.plot([kA[0], xl, xl, xk, xk, na["NC in"][0]],
        [kA[1], kA[1], y_unten, y_unten, na["NC in"][1], na["NC in"][1]],
        color=PWR, linewidth=1.5, zorder=1)
ax.text(xk + 0.1, (y_unten + na["NC in"][1]) / 2, "node A → S1\n0.5 mm²", fontsize=7.4, color=PWR,
        va="center", ha="left", zorder=4, **MONO, bbox=dict(boxstyle="round,pad=0.14", fc="white", ec="none"))
# D1 on the right of the block: cathode to +bus side (86 of K1), anode to node A
dk, da = rel["D1 cathode"], rel["D1 anode"]
ax.plot([dk[0], dk[0] + 0.35, dk[0] + 0.35, da[0]], [dk[1], dk[1], da[1], da[1]],
        color=INK, linewidth=1.3, zorder=1)
ax.text(7.2, y_unten - 0.3, "D1 1N4007 on the relay block: cathode ↔ K1·86 (behind F1), anode ↔ K3·85 (node A)",
        fontsize=7.0, color=INK, va="top", ha="left", **MONO)

y -= max(h1, h2, h3) + 1.35
y = fussnote(y, "F1 sits at the tap, not at the switching module: a short in the chain or on lead 1 must not\n"
                "depend on the 15 A main fuse. D1 lies ONLY across the three coils (K1·86 to K3·85),\n"
                "not across the E-stop as well — otherwise the coil current has no path when the button opens.\n"
                "Coil polarity is arbitrary, but: relays WITHOUT built-in diode. Before installation check with the\n"
                "ohmmeter that 85↔86 measures the same in both directions (≈ 80–90 Ω per coil).")

# ============================================================ 2 switching module
y = ueberschrift(y, "2", "Switching module at the ESP32: lead 1, ground, base",
                 "GPIO40 high = released · low / reset = braked")

na2, h1 = modul(0.4, y, 2.4, "S1 E-stop", "",
                rechts=[(None, "NC out", PWR)])

mod, h2 = modul(5.6, y, 4.6, "Switching module", "perfboard next to the DevKit",
                links=[(None, "Q1 collector", PWR),
                       (None, "Q1 emitter", AUX)],
                rechts=[(None, "base input (R1)", SIG),
                        (None, "divider node (opt.)", AUX)])

esp, h3 = modul(12.4, y, 3.4, "ESP32-S3 DevKitC-1", "3.3 V logic",
                links=[(40, "PIN_BRAKE_RELEASE", SIG),
                       (2, "PIN_ESTOP_FB (opt.)", AUX),
                       (None, "GND (stays on COMM −)", MUT)])

l1a, l1b = na2["NC out"], mod["Q1 collector"]
ax.plot([l1a[0], l1b[0]], [l1a[1], l1b[1]], color=PWR, linewidth=1.5, zorder=1)
ax.text((l1a[0] + l1b[0]) / 2, l1a[1] + 0.18, "LEAD 1\n0.5 mm², up to 42 V", fontsize=7.0, color=PWR,
        va="bottom", ha="center", zorder=4, **MONO)
strang([(mod["base input (R1)"], esp["PIN_BRAKE_RELEASE"], "base, short", SIG),
        (mod["divider node (opt.)"], esp["PIN_ESTOP_FB (opt.)"], "option", AUX)])
# source: own wire to the star point (out of the module)
sp = mod["Q1 emitter"]
yb = y - h2 - 0.35
ax.plot([sp[0], sp[0] - 0.4, sp[0] - 0.4, sp[0] + 0.6], [sp[1], sp[1], yb, yb], color=AUX, linewidth=1.5, zorder=1)
ax.text(sp[0] + 0.75, yb, "→ star point bus − at the controller (BATT −), 0.5 mm², OWN wire", fontsize=7.4,
        color=AUX, va="center", ha="left", **MONO)

y -= max(h1, h2, h3) + 0.85
y = fussnote(y, "The coil current (140 mA) flows via the emitter to bus −. It must NOT run via the GND pin of the DevKit\n"
                "and the COMM ground to the controller: that stays the logic-to-power link for signals (plus, since\n"
                "2026-11-25, the minus of the XL7015 buck). Hence the emitter gets its own wire to the star point.\n"
                "The base reference is still given because ESP GND is on the same node — it is all one ground.")
y = fussnote(y, "On the module: Q1 2N5551 (E-B-C, label facing the viewer: emitter left) · R1 220 Ω between base input\n"
                "and base · R2 10 kΩ base–emitter · option: R3 120 kΩ collector → GPIO2 node, R4 10 kΩ and Z1 3.3 V from the\n"
                "node to ground. No heat sink: 140 mA × 0.3 V ≈ 40 mW. Alternative IRL540N: 100 Ω into the gate, 10 kΩ gate–source.", MUT)

# ============================================================ 3 contact side
y = ueberschrift(y, "3", "Contact side (already built, for completeness)",
                 "≥ 4 mm², mind the phase order (§10.6)")

mot, h1 = modul(0.4, y, 3.6, "Hub motor", "three phases",
                rechts=[(None, "phase U", PWR), (None, "phase V", PWR), (None, "phase W", PWR)])
kon, h2 = modul(6.4, y, 4.2, "Relay block K1 K2 K3", "contact side, per relay",
                links=[(30, "K1 common", PWR), (30, "K2 common", PWR), (30, "K3 common", PWR)],
                rechts=[(87, "K1 make (NO)", PWR), (87, "K2 make (NO)", PWR), (87, "K3 make (NO)", PWR),
                        ("87a", "K1·K2·K3 break (NC)", INK)])
reg, h3 = modul(12.0, y, 3.6, "Controller FSESC 4.20", "phase terminals",
                links=[(None, "A1", PWR), (None, "B1", PWR), (None, "C1", PWR)])

strang([(mot["phase U"], kon["K1 common"], "U", PWR),
        (mot["phase V"], kon["K2 common"], "V", PWR),
        (mot["phase W"], kon["K3 common"], "W", PWR)])
strang([(kon["K1 make (NO)"], reg["A1"], "energised", PWR),
        (kon["K2 make (NO)"], reg["B1"], "", PWR),
        (kon["K3 make (NO)"], reg["C1"], "", PWR)])
oa = kon["K1·K2·K3 break (NC)"]
y_oa = y - h3 - 0.45                         # below the controller box, so the label stays clear of it
ax.plot([oa[0], oa[0] + 0.4, oa[0] + 0.4, oa[0] + 0.6], [oa[1], oa[1], y_oa, y_oa],
        color=INK, linewidth=1.5, zorder=1)
ax.text(oa[0] + 0.7, y_oa, "all three 87a onto ONE short-circuit node —\nnowhere connected to A1/B1/C1",
        fontsize=7.4, color=INK, va="center", ha="left", **MONO,
        bbox=dict(boxstyle="round,pad=0.14", fc="white", ec="none"))

y -= max(h1, h2, h3) + 0.35
y = fussnote(y, "Which motor phase goes to which relay does not matter — but the assignment motor→controller must be\n"
                "the same as with direct wiring (§10.6: two swapped phases = humming motor without torque).", MUT)

# ============================================================ wire list
y -= 0.35
ax.plot([0.2, W - 0.2], [y, y], color=LINE, linewidth=2.4)
y -= 0.80
ax.text(0.2, y, "Wire list", fontsize=11.5, weight="bold", color=INK, va="top")
y -= 0.85
ax.text(0.2, y,
        "1  Controller BATT + → F1 → relay block K1·86              0.5 mm², power-path insulation\n"
        "2  Relay block K3·85 (node A) → S1 E-stop NC               0.5 mm², along the frame to the mushroom button\n"
        "3  S1 E-stop NC → switching module Q1 collector (LEAD 1)   0.5 mm², carries up to 42 V — the only wire from the chain to the module\n"
        "4  Switching module Q1 emitter → controller BATT − (star)  0.5 mm², own wire, not via the DevKit\n"
        "5  ESP32 GPIO40 → switching module base input (R1)         signal wire, short, twisted with a GND wire from the DevKit\n"
        "6  ESP32 GPIO2  → switching module divider node (R3/R4/Z1) option, signal wire\n"
        "D1 on the relay block between K1·86 and K3·85 · F1 directly at the tap BATT +",
        fontsize=8.2, color=INK, va="top", linespacing=1.6, **MONO)
y -= ZEILE * 8 + 0.6

y = fussnote(y, "Before the first release (docs/04-hardware.md §9.5): 1. Measure the chain without module (≈ 257 Ω from BATT + to lead 1,\n"
                "E-stop closed; ∞ pressed). 2. Module without chain: GPIO40 high → collector to emitter conducts (diode test). 3. Together,\n"
                "motor disconnected: GPIO40 high → relays click, U_CE < 0.3 V, current ≈ 140 mA; GPIO40 low → drop out; press E-stop\n"
                "while high → drop out. 4. Reset the ESP with the chain energised → drop out. Only then connect the motor.")

ax.set_xlim(0, W)
ax.set_ylim(y, 0.6)
ax.axis("off")
fig.savefig("hardware/bremsverdrahtung.pdf", bbox_inches="tight")
fig.savefig("hardware/bremsverdrahtung.png", dpi=130, bbox_inches="tight")
print(f"ok — drawing height {abs(y):.1f} units")
