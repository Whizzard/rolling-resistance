#!/usr/bin/env python3
"""Anschlussplan des Rollwiderstandspruefstands — Leitung fuer Leitung.

Der Schaltplan (hardware/schaltplan.py) zeigt, WAS womit verbunden ist.
Dieser Plan zeigt, WIE: jedes Bauteil mit seinen Anschluessen in der Reihenfolge,
in der sie dort liegen, und jede Ader einzeln mit Signalnamen.

Quelle ist dieser Text, das PDF wird daraus erzeugt:

    .venv/bin/python hardware/anschlussplan.py

Erzeugt hardware/anschlussplan.pdf und hardware/anschlussplan.png.
Pinbelegung siehe docs/04-hardware.md §12; Aenderungen hier und dort gemeinsam.

GRUNDSATZ: Pinnummern stehen hier NUR, wo sie belegt sind. Wo die Zaehlrichtung am
Bauteil nicht feststeht — Leiste COMM des Reglers, Bauform des Steuerkreuzes — steht
ein Fragezeichen und daneben, wie sie zu bestimmen ist. Eine erfundene Pinnummer ist
schlimmer als eine Luecke: nach ihr wird geloetet.

Das Layout laeuft mit einem Cursor von oben nach unten; jeder Abschnitt meldet seine
Hoehe zurueck. Feste y-Werte gibt es nicht — sonst laufen die Boxen ineinander, sobald
irgendwo ein Pin dazukommt.
"""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch

INK  = "#1b2328"
MUT  = "#6b7a85"
SIG  = "#1f6f8b"   # Messsignale
AUX  = "#4a7a2a"   # Versorgung Logik
PWR  = "#9c3524"   # Leistungspfad / Warnung
LINE = "#c9d2d8"

PITCH   = 0.62        # Zeilenabstand der Pins
HEAD    = 1.25        # Kopfhoehe einer Box (Titel + Untertitel)
FOOT    = 0.35        # Luft unter dem letzten Pin
W       = 16.0        # Zeichenbreite
MONO    = {"family": "DejaVu Sans Mono"}

fig, ax = plt.subplots(figsize=(13.5, 19))


def modul(x, ytop, w, titel, untertitel, links=None, rechts=None, farbe=INK):
    """Bauteil mit Anschluessen links und/oder rechts.

    links/rechts: Liste von (nr, name, farbe); nr darf None sein.
    Gibt (anker, hoehe) zurueck. anker: {name: (x, y)}.
    """
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


def strang(paare, farbe_default=SIG):
    """Mehrere Adern zwischen zwei Bauteilen.

    paare: Liste von (ankerA, ankerB, label, farbe). Die senkrechten Teilstuecke
    werden gestaffelt, damit sich die Beschriftungen nicht stapeln.
    """
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


# Eine Textzeile bei 7,8 pt mit Zeilenabstand 1,5 ist rund 0,40 Zeicheneinheiten
# hoch (die Achse laeuft mit etwa 30 pt je Einheit). Danach richten sich die
# Vorschuebe — sonst laufen Fussnoten in die naechste Ueberschrift.
ZEILE = 0.40


def ueberschrift(y, nummer, titel, hinweis=None):
    y -= 0.35                                    # Luft ueber dem Strich
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


# ============================================================ Kopf
y = 0.0
ax.text(0.2, y, "Rollwiderstandsprüfstand — Anschlussplan MVP", fontsize=17, color=INK,
        va="top")
y -= 0.70
ax.text(0.2, y, "ESP32-S3-DevKitC-1 N16R8  ·  Stand 2026-11-16  ·  "
                "Quelle: hardware/anschlussplan.py  ·  Pinbelegung docs/04-hardware.md §12",
        fontsize=8, color=MUT, va="top")
y -= 0.45
ax.text(0.2, y, "Die Zahlen am ESP32 sind GPIO-Nummern vom Aufdruck des Boards, "
                "keine Steckerpositionen. „?​\" = Zählrichtung am Bauteil noch unbestimmt.",
        fontsize=8, color=MUT, va="top")
y -= 1.1

# ============================================================ 1 Wägezelle
y = ueberschrift(y, "1", "Wägezelle → HX711 → ESP32",
                 "Zellenleitung ohne Steckverbinder, max. 10 cm")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "DevKitC-1", rechts=[
    (None, "3V3", AUX), (None, "GND", AUX),
    (15, "GPIO15", SIG), (16, "GPIO16", SIG)])

hx, h2 = modul(7.0, y, 4.2, "HX711", "Modul mit getrennten VCC/VDD",
               links=[(1, "VDD", AUX), (2, "VCC", AUX), (3, "DAT", SIG),
                      (4, "CLK", SIG), (5, "GND", AUX)],
               rechts=[(None, "E+", PWR), (None, "E−", PWR),
                       (None, "A+", SIG), (None, "A−", SIG)])

zelle, h3 = modul(13.0, y, 2.8, "Wägezelle", "5 kg Biegebalken", links=[
    (None, "rot", PWR), (None, "schwarz", PWR), (None, "grün", SIG), (None, "weiß", SIG)])

strang([(esp["3V3"], hx["VDD"], "3,3 V", AUX),
        (esp["3V3"], hx["VCC"], "3,3 V  MVP", AUX),
        (esp["GND"], hx["GND"], "GND", AUX),
        (esp["GPIO15"], hx["DAT"], "DOUT", SIG),
        (esp["GPIO16"], hx["CLK"], "SCK +100 Ω", SIG)])

strang([(hx["E+"], zelle["rot"], "Erregung +", PWR),
        (hx["E−"], zelle["schwarz"], "Erregung −", PWR),
        (hx["A+"], zelle["grün"], "Signal +", SIG),
        (hx["A−"], zelle["weiß"], "Signal −", SIG)])

y -= max(h1, h2, h3) + 0.35
y = fussnote(y, "Aderfarben der Zelle sind die übliche Belegung — am Exemplar durchmessen.\n"
                "VCC und VDD liegen im MVP beide auf 3,3 V; für den Messlauf VCC auf 5 V.\n"
                "Die vier Zellenadern gehen direkt in die Schraubklemme, ohne Steckverbinder.")

# ============================================================ 2 LCD
y = ueberschrift(y, "2", "LCD 4×20 über PCF8574-Backpack", "optional — das UI ersetzt es")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "I²C", rechts=[
    (None, "3V3", AUX), (None, "GND", AUX), (8, "GPIO8", SIG), (9, "GPIO9", SIG)])

lcd, h2 = modul(8.4, y, 4.0, "PCF8574", "Backpack am LCD", links=[
    (1, "GND", AUX), (2, "VCC", AUX), (3, "SDA", SIG), (4, "SCL", SIG)])

strang([(esp["GND"], lcd["GND"], "GND", AUX),
        (esp["3V3"], lcd["VCC"], "3,3 V", AUX),
        (esp["GPIO8"], lcd["SDA"], "SDA + 3,3 kΩ→3V3", SIG),
        (esp["GPIO9"], lcd["SCL"], "SCL + 3,3 kΩ→3V3", SIG)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "Reihenfolge GND·VCC·SDA·SCL ist am Backpack aufgedruckt — ablesen, nicht annehmen.\n"
                "Zuerst mit 3,3 V versuchen: dann ziehen die 4k7 auf dem Backpack ebenfalls nach 3,3 V\n"
                "und liegen innerhalb der Spezifikation. I²C-Adresse scannen (0x27 oder 0x3F).")

# ============================================================ 3 Steuerkreuz
y = ueberschrift(y, "3", "Steuerkreuz, fünf Kontakte",
                 "keine Widerstände, keine Kondensatoren")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "interner Pull-up", rechts=[
    (11, "GPIO11", SIG), (12, "GPIO12", SIG), (13, "GPIO13", SIG),
    (14, "GPIO14", SIG), (39, "GPIO39", SIG), (None, "GND", AUX)])

kr, h2 = modul(8.4, y, 4.4, "Fünfwege-Taster", "z. B. ALPS SKQU", links=[
    ("?", "hoch", SIG), ("?", "runter", SIG), ("?", "rein", SIG),
    ("?", "raus", SIG), ("?", "Mitte", SIG), ("?", "gemeinsam", AUX)])

strang([(esp["GPIO11"], kr["hoch"], "UP", SIG),
        (esp["GPIO12"], kr["runter"], "DOWN", SIG),
        (esp["GPIO13"], kr["rein"], "IN — Schrittweite", SIG),
        (esp["GPIO14"], kr["raus"], "OUT", SIG),
        (esp["GPIO39"], kr["Mitte"], "OK", SIG),
        (esp["GND"], kr["gemeinsam"], "GND", AUX)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "Die Kontaktnummern hängen an der Bauform und stehen noch nicht fest. Vor dem Löten mit\n"
                "dem Durchgangsprüfer bestimmen: der gemeinsame Anschluss ist der, der gegen ALLE\n"
                "übrigen schaltet. Die drei Hall-Adern am Motor sind untereinander beliebig, hier nicht.")

# ============================================================ 4 Regler
y = ueberschrift(y, "4", "Motorregler FSESC 4.20, Leiste COMM",
                 "COMM 5V NICHT anschließen")

esp, h1 = modul(0.4, y, 3.4, "ESP32-S3", "UART1", rechts=[
    (18, "GPIO18", SIG), (21, "GPIO21", SIG), (None, "GND", AUX)])

comm, h2 = modul(8.4, y, 4.4, "FSESC 4.20", "Stiftleiste COMM, 7-polig", links=[
    ("?", "5V", PWR), ("?", "3.3V", MUT), ("?", "−", AUX), ("?", "ADC", MUT),
    ("?", "TX", SIG), ("?", "RX", SIG), ("?", "ADC2", MUT)])

strang([(esp["GPIO18"], comm["RX"], "TX → RX", SIG),
        (esp["GPIO21"], comm["TX"], "RX ← TX", SIG),
        (esp["GND"], comm["−"], "GND — Sternpunkt", AUX)])

# 5V durchkreuzen
x5, y5 = comm["5V"]
ax.plot([x5 - 0.55, x5 - 0.05], [y5 + 0.20, y5 - 0.20], color=PWR, linewidth=2.2, zorder=5)
ax.plot([x5 - 0.55, x5 - 0.05], [y5 - 0.20, y5 + 0.20], color=PWR, linewidth=2.2, zorder=5)
ax.text(x5 - 0.75, y5, "nicht belegen", fontsize=7.4, color=PWR, va="center", ha="right")

y -= max(h1, h2) + 0.35
y = fussnote(y, "Welches Ende der Leiste Pin 1 ist, geht aus dem Handbuch NICHT hervor. Vor dem\n"
                "Anschließen am Regler — über dessen Micro-USB versorgt — 5 V und Masse nachmessen,\n"
                "sonst wird sauber spiegelverkehrt verdrahtet. Die Reihenfolge oben ist die des Handbuchs.\n"
                "Die Masse am Regler ist die EINZIGE Verbindung zwischen Logik- und Leistungsmasse.")

# ============================================================ 5 Speisung
y = ueberschrift(y, "5", "Speisung über USB-C", "beide Buchsen dürfen gleichzeitig speisen")

usb, h1 = modul(0.4, y, 4.2, "USB-C", "zwei Buchsen am Board", rechts=[
    (None, "COM — CH343", AUX), (None, "USB — nativ", AUX)])

q, h2 = modul(9.4, y, 6.2, "Quellen", "", links=[
    (None, "Notebook: Flashen + Konsole", AUX),
    (None, "Netzteil 5 V / ≥ 1 A", AUX)])

strang([(usb["COM — CH343"], q["Notebook: Flashen + Konsole"], "bei Bedarf", AUX),
        (usb["USB — nativ"], q["Netzteil 5 V / ≥ 1 A"], "dauerhaft", AUX)])

y -= max(h1, h2) + 0.35
y = fussnote(y, "Nur an COM flasht esptool ohne Tastendruck, und nur dort kommt die serielle Konsole heraus\n"
                "(Serial = UART0 auf GPIO 43/44, weil ARDUINO_USB_CDC_ON_BOOT nicht gesetzt ist).\n"
                "Den 5V-Pin NICHT einspeisen — USB, 5V-Pin und 3V3-Pin schließen sich gegenseitig aus.\n"
                "Bei USB-Speisung ist der 5V-Pin AUSGANG (VBUS), 3V3 kommt aus dem Onboard-LDO.", MUT)

# ============================================================ Nachspann
y -= 0.35
ax.plot([0.2, W - 0.2], [y, y], color=LINE, linewidth=2.4)
y -= 0.80
ax.text(0.2, y, "Was im MVP NICHT verdrahtet wird", fontsize=11.5, weight="bold",
        color=INK, va="top")
y -= 0.85
ax.text(0.2, y,
        "Not-Aus, Sicherung und die drei Bremsrelais (§9) fehlen. Ohne sie gibt es nur freien Auslauf von\n"
        "3 bis 6 Minuten — DER MOTOR DREHT IM MVP NICHT. Der Regler wird verkabelt und die UART-Strecke\n"
        "geprüft, mehr nicht.\n\n"
        "Ebenfalls noch nicht: Hall-Geber (GPIO 4 Marken, 5 Index, 6 Laufrad, 7 Motor-Hall), microSD\n"
        "(GPIO 10 CS), zweite Wägezelle (GPIO 17), Bremsrelais (GPIO 40), Not-Aus-Rückmeldung (GPIO 2).\n"
        "Die Drehzahl kommt im MVP über UART vom Regler.",
        fontsize=8.2, color=INK, va="top", linespacing=1.5)
y -= ZEILE * 9 + 1.0
ax.text(0.2, y,
        "Leitungen:   1  Zelle→HX711, 4-adrig, 10 cm, ohne Stecker   ·   "
        "2  HX711→ESP32, LiYCY 6×0,14 geschirmt, JST-XH 6-polig\n"
        "3  ESP32→LCD, 4-adrig   ·   4  ESP32→Steuerkreuz, 6-adrig   ·   "
        "5  ESP32→FSESC, 3-adrig verdrillt, getrennt von den Phasen",
        fontsize=7.8, color=MUT, va="top", linespacing=1.6)
y -= ZEILE * 2 + 0.5

ax.set_xlim(0, W)
ax.set_ylim(y, 0.6)
ax.axis("off")
fig.savefig("hardware/anschlussplan.pdf", bbox_inches="tight")
fig.savefig("hardware/anschlussplan.png", dpi=130, bbox_inches="tight")
print(f"ok — Zeichenhoehe {abs(y):.1f} Einheiten")
