#!/usr/bin/env python3
"""
Motorregler von Hand fahren — über USB, ohne ESP32.

    python3 tools/vesc_console.py /dev/cu.usbmodem3041

Zweck: ein erster Probelauf am Prüfstand, bevor die Firmware dazwischensteht.
Der Regler spricht über USB dasselbe Paketprotokoll wie über UART, deshalb ist
der Rahmen hier derselbe wie in firmware/src/vesc/packet.cpp.

    Pfeil hoch/runter   Sollwert ± VESC_ERPM_STEP_SMALL
    Pfeil rechts/links  Sollwert ± VESC_ERPM_STEP_LARGE
    Leertaste           STOP — Antrieb stromlos, Trommel läuft aus
    0                   Sollwert auf null
    q                   Beenden (stoppt vorher)

SICHERHEIT — was dieses Skript tut und was es nicht tut:

  * Der Sollwert wird NUR gesendet, solange das Skript lebt. Stürzt es ab oder
    wird das Fenster geschlossen, kommt nichts mehr, und der Regler stellt nach
    seinem eigenen timeout_msec ab. Das ist die zweite, unabhängige Ebene.

  * TOTMANN: Kommt DEADMAN_S lang kein Tastendruck, fährt der Sollwert von selbst
    auf null. Wer weggeht, lässt den Stand nicht laufen.

  * Beim Beenden — auch bei Strg-C oder einem Fehler — wird freigegeben.

  * ACHTUNG: Steht timeout_brake_current des Reglers auf 0 (Werksvorgabe), heißt
    "abstellen" AUSLAUFEN, nicht bremsen. Die Trommel braucht dann 3 bis 6 Minuten.

  * Dieses Skript ist KEIN Not-Aus. Der Not-Aus trennt in Hardware.
"""
import argparse
import curses
import math
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial fehlt:  pip install pyserial")

# --- gespiegelt aus firmware/src/config.h; Begründung steht dort -------------
ERPM_MAX        = 2400     # VESC_ERPM_MAX, vorläufig (Polzahl nicht gemessen)
STEP_SMALL      = 100      # VESC_ERPM_STEP_SMALL
STEP_LARGE      = 1000     # VESC_ERPM_STEP_LARGE
SEND_MS         = 100      # Sollwert wiederholen; < timeout_msec des Reglers
POLL_MS         = 200      # Messwerte holen (ist zugleich das Mittelungsfenster)
DEADMAN_S       = 30.0     # ohne Tastendruck von selbst auf null

COMM_FW_VERSION           = 0
COMM_SET_CURRENT          = 6
COMM_SET_RPM              = 8
COMM_GET_VALUES_SELECTIVE = 50

# 0 FET-Temp · 2 Motorstrom · 3 Eingangsstrom · 6 Aussteuerung
# 7 ERPM · 8 Busspannung · 15 Fehlercode
MASK = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 15)
FIELD_BYTES = [2, 2, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, 4, 1, 4, 1, 6, 4, 4, 1]

FAULT = {
    0: "kein Fehler", 1: "Ueberspannung", 2: "Unterspannung",
    3: "Treiberstufe (DRV)", 4: "Ueberstrom", 5: "Uebertemperatur FET",
    6: "Uebertemperatur Motor", 7: "Treiberversorgung hoch",
    8: "Treiberversorgung niedrig", 9: "MCU-Unterspannung",
    10: "Neustart durch Watchdog", 19: "Abschalteingang (BRK)",
}


# ---------------------------------------------------------------- Paketrahmen
def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(payload: bytes) -> bytes:
    n = len(payload)
    head = bytes([2, n]) if n <= 255 else bytes([3, n >> 8, n & 0xFF])
    return head + payload + struct.pack(">H", crc16(payload)) + b"\x03"


class Reader:
    """Puffert und wertet bei Fehlern ab der naechsten Position neu aus —
    dieselbe Eigenschaft wie die C++-Fassung, damit ein Stoerbyte hoechstens
    ein Paket kostet."""

    def __init__(self):
        self.buf = bytearray()

    def feed(self, data: bytes):
        self.buf += data
        while True:
            if not self.buf:
                return
            if self.buf[0] == 2:
                hdr = 2
                if len(self.buf) < 2:
                    return
                ln = self.buf[1]
            elif self.buf[0] == 3:
                hdr = 3
                if len(self.buf) < 3:
                    return
                ln = (self.buf[1] << 8) | self.buf[2]
            else:
                del self.buf[0]
                continue
            if ln == 0 or ln > 512:
                del self.buf[0]
                continue
            total = hdr + ln + 3
            if len(self.buf) < total:
                return
            body = bytes(self.buf[hdr:hdr + ln])
            ok = (self.buf[total - 1] == 3 and
                  crc16(body) == struct.unpack(">H", self.buf[hdr + ln:hdr + ln + 2])[0])
            if ok:
                del self.buf[:total]
                yield body
            else:
                del self.buf[0]


# ---------------------------------------------------------------------- Regler
class Vesc:
    def __init__(self, port):
        self.s = serial.Serial(port, 115200, timeout=0)
        self.rx = Reader()
        self.v = {}
        self.last_value = 0.0
        self.fw = None

    def send(self, payload):
        self.s.write(frame(payload))

    def set_rpm(self, erpm):
        self.send(bytes([COMM_SET_RPM]) + struct.pack(">i", int(erpm)))

    def release(self):
        """Strom null: Bruecke sperren, Maschine laeuft frei aus."""
        self.send(bytes([COMM_SET_CURRENT]) + struct.pack(">i", 0))

    def poll(self):
        self.send(bytes([COMM_GET_VALUES_SELECTIVE]) + struct.pack(">I", MASK))

    def ask_fw(self):
        self.send(bytes([COMM_FW_VERSION]))

    def service(self):
        data = self.s.read(4096)
        if not data:
            return
        for body in self.rx.feed(data):
            if not body:
                continue
            if body[0] == COMM_GET_VALUES_SELECTIVE:
                self._values(body)
            elif body[0] == COMM_FW_VERSION and len(body) >= 3:
                hw = bytes(body[3:]).split(b"\x00")[0].decode("ascii", "replace")
                self.fw = f"{body[1]}.{body[2]:02d} / {hw}"

    def _values(self, d):
        if len(d) < 5:
            return
        mask = struct.unpack_from(">I", d, 1)[0]
        i = 5
        for bit in range(22):
            if not (mask >> bit) & 1:
                continue
            w = FIELD_BYTES[bit]
            if i + w > len(d):
                break          # aeltere Firmware liefert hintere Felder nicht
            if   bit == 0:  self.v["tfet"] = struct.unpack_from(">h", d, i)[0] / 10
            elif bit == 2:  self.v["imot"] = struct.unpack_from(">i", d, i)[0] / 100
            elif bit == 3:  self.v["iin"]  = struct.unpack_from(">i", d, i)[0] / 100
            elif bit == 6:  self.v["duty"] = struct.unpack_from(">h", d, i)[0] / 1000
            elif bit == 7:  self.v["erpm"] = struct.unpack_from(">i", d, i)[0]
            elif bit == 8:  self.v["vin"]  = struct.unpack_from(">h", d, i)[0] / 10
            elif bit == 15: self.v["fault"] = d[i]
            i += w
        self.last_value = time.time()


# ------------------------------------------------------------------ Oberflaeche
def run(stdscr, vesc, poles):
    curses.curs_set(0)
    stdscr.nodelay(True)
    stdscr.keypad(True)

    want = 0
    driving = False
    last_send = last_poll = last_draw = 0.0
    last_key = time.time()
    msg = ""

    vesc.ask_fw()

    while True:
        now = time.time()
        vesc.service()

        # ---- Tasten ----
        try:
            ch = stdscr.getch()
        except curses.error:
            ch = -1
        if ch != -1:
            last_key = now
            if ch in (ord("q"), ord("Q")):
                break
            elif ch == ord(" "):
                want, driving = 0, False
                vesc.release()
                msg = "STOP — Antrieb stromlos, Trommel laeuft aus"
            elif ch == ord("0"):
                want, driving = 0, False
                vesc.release()
                msg = "Sollwert 0"
            else:
                step = {curses.KEY_UP: STEP_SMALL, curses.KEY_DOWN: -STEP_SMALL,
                        curses.KEY_RIGHT: STEP_LARGE, curses.KEY_LEFT: -STEP_LARGE}.get(ch)
                if step:
                    fault = vesc.v.get("fault", 0)
                    if want == 0 and step > 0 and fault:
                        msg = f"Start verweigert: Regler meldet Fehler {fault}"
                    else:
                        want = max(-ERPM_MAX, min(ERPM_MAX, want + step))
                        driving = want != 0
                        if not driving:
                            vesc.release()
                        msg = ""

        # ---- Totmann ----
        if driving and now - last_key > DEADMAN_S:
            want, driving = 0, False
            vesc.release()
            msg = f"TOTMANN: {DEADMAN_S:.0f} s ohne Tastendruck — Sollwert auf null"

        # ---- Senden ----
        if driving and (now - last_send) * 1000 >= SEND_MS:
            last_send = now
            vesc.set_rpm(want)
        if (now - last_poll) * 1000 >= POLL_MS:
            last_poll = now
            vesc.poll()
            if vesc.fw is None:
                vesc.ask_fw()

        # ---- Anzeige ----
        if now - last_draw > 0.1:
            last_draw = now
            draw(stdscr, vesc, want, driving, now - last_key, msg, poles)

        time.sleep(0.005)


def draw(stdscr, vesc, want, driving, idle, msg, poles):
    v = vesc.v
    alive = (time.time() - vesc.last_value) < 1.0
    stdscr.erase()
    w = stdscr.getmaxyx()[1]

    stdscr.addstr(0, 0, " Pruefstand — Regler von Hand ".ljust(w), curses.A_REVERSE)
    stdscr.addstr(1, 0, f" Regler: {vesc.fw or '—'}    "
                        f"{'verbunden' if alive else 'KEINE ANTWORT'}")

    def row(y, label, value, unit=""):
        stdscr.addstr(y, 2, f"{label:<22}")
        stdscr.addstr(y, 24, f"{value:>12}", curses.A_BOLD)
        stdscr.addstr(y, 37, f" {unit}")

    erpm = v.get("erpm")
    row(3, "Drehzahl ist", f"{erpm}" if alive and erpm is not None else "—", "ERPM")
    if poles and alive and erpm is not None:
        row(4, f"  mechanisch ({poles} PP)", f"{erpm / poles:.0f}", "1/min")
    row(5, "Sollwert", f"{want}", "ERPM" + ("   FAEHRT" if driving else ""))
    row(6, "Busspannung", f"{v['vin']:.1f}" if alive and "vin" in v else "—", "V")
    row(7, "Motorstrom", f"{v['imot']:.2f}" if alive and "imot" in v else "—", "A")
    row(8, "Eingangsstrom", f"{v['iin']:.2f}" if alive and "iin" in v else "—", "A")
    row(9, "Aussteuerung", f"{v['duty']:.3f}" if alive and "duty" in v else "—", "")
    row(10, "FET-Temperatur", f"{v['tfet']:.1f}" if alive and "tfet" in v else "—", "°C")

    fault = v.get("fault", 0)
    txt = FAULT.get(fault, f"unbekannt ({fault})")
    stdscr.addstr(11, 2, f"{'Fehler':<22}")
    stdscr.addstr(11, 24, f"{txt:>12}" if len(txt) <= 12 else txt,
                  curses.A_BOLD | (curses.A_REVERSE if fault else 0))

    if driving:
        left = max(0.0, DEADMAN_S - idle)
        stdscr.addstr(13, 2, f"Totmann: {left:4.0f} s bis Selbstabschaltung",
                      curses.A_REVERSE if left < 10 else 0)

    stdscr.addstr(15, 2, f"hoch/runter ±{STEP_SMALL}   rechts/links ±{STEP_LARGE}"
                         f"   Leertaste STOP   0 Null   q Ende")
    stdscr.addstr(16, 2, "Dies ist KEIN Not-Aus. Der Not-Aus trennt in Hardware.",
                  curses.A_DIM)
    if msg:
        stdscr.addstr(18, 2, msg[:w - 4], curses.A_BOLD)
    stdscr.refresh()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="z. B. /dev/cu.usbmodem3041")
    ap.add_argument("--poles", type=int, default=0,
                    help="Polpaarzahl, nur fuer die Umrechnung auf 1/min. "
                         "Ohne Angabe wird nicht umgerechnet — eine geratene "
                         "Polzahl ergaebe eine erfundene Drehzahl.")
    args = ap.parse_args()

    vesc = Vesc(args.port)
    try:
        curses.wrapper(run, vesc, args.poles)
    finally:
        # Auch bei Strg-C oder einem Fehler: Antrieb freigeben.
        try:
            for _ in range(3):
                vesc.release()
                time.sleep(0.05)
        except Exception:
            pass
        vesc.s.close()
        print("Antrieb freigegeben.")


if __name__ == "__main__":
    main()
