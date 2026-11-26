#!/usr/bin/env python3
"""
Drive the motor controller by hand — over USB, without the ESP32.

    python3 tools/vesc_console.py /dev/cu.usbmodem3041

Purpose: a first trial run on the test stand before the firmware sits in between.
Over USB the controller speaks the same packet protocol as over UART, so the
framing here is the same as in firmware/src/vesc/packet.cpp.

    Arrow up/down       setpoint ± VESC_ERPM_STEP_SMALL
    Arrow right/left    setpoint ± VESC_ERPM_STEP_LARGE
    Space               STOP — drive unpowered, drum coasts down
    0                   setpoint to zero
    q                   quit (stops first)

SAFETY — what this script does and what it does not do:

  * The setpoint is sent ONLY while the script is alive. If it crashes or the
    window is closed, nothing more arrives, and the controller switches off after
    its own timeout_msec. That is the second, independent layer.

  * DEAD MAN: if no key is pressed for DEADMAN_S, the setpoint goes to zero by
    itself. Whoever walks away does not leave the stand running.

  * On quitting — also on Ctrl-C or an error — the drive is released.

  * CAUTION: if the controller's timeout_brake_current is 0 (factory default),
    "switch off" means COASTING, not braking. The drum then takes 3 to 6 minutes.

  * This script is NOT an emergency stop. The emergency stop disconnects in hardware.
"""
import argparse
import curses
import math
import struct

from vescproto import frame, Reader  # noqa: E402
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing:  pip install pyserial")

# --- mirrored from firmware/src/config.h; the rationale is there --------------
ERPM_MAX        = 2400     # VESC_ERPM_MAX, provisional (pole count not measured)
STEP_SMALL      = 100      # VESC_ERPM_STEP_SMALL
STEP_LARGE      = 1000     # VESC_ERPM_STEP_LARGE
SEND_MS         = 100      # repeat the setpoint; < controller's timeout_msec
POLL_MS         = 200      # fetch readings (this is also the averaging window)
DEADMAN_S       = 30.0     # to zero by itself without a key press

COMM_FW_VERSION           = 0
COMM_SET_CURRENT          = 6
COMM_SET_RPM              = 8
COMM_GET_VALUES_SELECTIVE = 50

# 0 FET temp · 2 motor current · 3 input current · 6 duty cycle
# 7 ERPM · 8 bus voltage · 15 fault code
MASK = (1 << 0) | (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 15)
FIELD_BYTES = [2, 2, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, 4, 1, 4, 1, 6, 4, 4, 1]

FAULT = {
    0: "no fault", 1: "overvoltage", 2: "undervoltage",
    3: "gate driver (DRV)", 4: "overcurrent", 5: "overtemperature FET",
    6: "overtemperature motor", 7: "gate driver supply high",
    8: "gate driver supply low", 9: "MCU undervoltage",
    10: "watchdog reset", 19: "shutdown input (BRK)",
}


# ------------------------------------------------------------------ controller
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
        """Current zero: bridge off, machine coasts freely."""
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
                break          # older firmware does not deliver the later fields
            if   bit == 0:  self.v["tfet"] = struct.unpack_from(">h", d, i)[0] / 10
            elif bit == 2:  self.v["imot"] = struct.unpack_from(">i", d, i)[0] / 100
            elif bit == 3:  self.v["iin"]  = struct.unpack_from(">i", d, i)[0] / 100
            elif bit == 6:  self.v["duty"] = struct.unpack_from(">h", d, i)[0] / 1000
            elif bit == 7:  self.v["erpm"] = struct.unpack_from(">i", d, i)[0]
            elif bit == 8:  self.v["vin"]  = struct.unpack_from(">h", d, i)[0] / 10
            elif bit == 15: self.v["fault"] = d[i]
            i += w
        self.last_value = time.time()


# -------------------------------------------------------------- user interface
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

        # ---- keys ----
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
                msg = "STOP — drive unpowered, drum coasting down"
            elif ch == ord("0"):
                want, driving = 0, False
                vesc.release()
                msg = "setpoint 0"
            else:
                step = {curses.KEY_UP: STEP_SMALL, curses.KEY_DOWN: -STEP_SMALL,
                        curses.KEY_RIGHT: STEP_LARGE, curses.KEY_LEFT: -STEP_LARGE}.get(ch)
                if step:
                    fault = vesc.v.get("fault", 0)
                    if want == 0 and step > 0 and fault:
                        msg = f"start refused: controller reports fault {fault}"
                    else:
                        want = max(-ERPM_MAX, min(ERPM_MAX, want + step))
                        driving = want != 0
                        if not driving:
                            vesc.release()
                        msg = ""

        # ---- dead man ----
        if driving and now - last_key > DEADMAN_S:
            want, driving = 0, False
            vesc.release()
            msg = f"DEAD MAN: {DEADMAN_S:.0f} s without a key press — setpoint to zero"

        # ---- send ----
        if driving and (now - last_send) * 1000 >= SEND_MS:
            last_send = now
            vesc.set_rpm(want)
        if (now - last_poll) * 1000 >= POLL_MS:
            last_poll = now
            vesc.poll()
            if vesc.fw is None:
                vesc.ask_fw()

        # ---- display ----
        if now - last_draw > 0.1:
            last_draw = now
            draw(stdscr, vesc, want, driving, now - last_key, msg, poles)

        time.sleep(0.005)


def draw(stdscr, vesc, want, driving, idle, msg, poles):
    v = vesc.v
    alive = (time.time() - vesc.last_value) < 1.0
    stdscr.erase()
    w = stdscr.getmaxyx()[1]

    stdscr.addstr(0, 0, " Test stand — controller by hand ".ljust(w), curses.A_REVERSE)
    stdscr.addstr(1, 0, f" Controller: {vesc.fw or '—'}    "
                        f"{'connected' if alive else 'NO REPLY'}")

    def row(y, label, value, unit=""):
        stdscr.addstr(y, 2, f"{label:<22}")
        stdscr.addstr(y, 24, f"{value:>12}", curses.A_BOLD)
        stdscr.addstr(y, 37, f" {unit}")

    erpm = v.get("erpm")
    row(3, "Speed actual", f"{erpm}" if alive and erpm is not None else "—", "ERPM")
    if poles and alive and erpm is not None:
        row(4, f"  mechanical ({poles} PP)", f"{erpm / poles:.0f}", "1/min")
    row(5, "Setpoint", f"{want}", "ERPM" + ("   DRIVING" if driving else ""))
    row(6, "Bus voltage", f"{v['vin']:.1f}" if alive and "vin" in v else "—", "V")
    row(7, "Motor current", f"{v['imot']:.2f}" if alive and "imot" in v else "—", "A")
    row(8, "Input current", f"{v['iin']:.2f}" if alive and "iin" in v else "—", "A")
    row(9, "Duty cycle", f"{v['duty']:.3f}" if alive and "duty" in v else "—", "")
    row(10, "FET temperature", f"{v['tfet']:.1f}" if alive and "tfet" in v else "—", "°C")

    fault = v.get("fault", 0)
    txt = FAULT.get(fault, f"unknown ({fault})")
    stdscr.addstr(11, 2, f"{'Fault':<22}")
    stdscr.addstr(11, 24, f"{txt:>12}" if len(txt) <= 12 else txt,
                  curses.A_BOLD | (curses.A_REVERSE if fault else 0))

    if driving:
        left = max(0.0, DEADMAN_S - idle)
        stdscr.addstr(13, 2, f"Dead man: {left:4.0f} s until auto shut-off",
                      curses.A_REVERSE if left < 10 else 0)

    stdscr.addstr(15, 2, f"up/down ±{STEP_SMALL}   right/left ±{STEP_LARGE}"
                         f"   Space STOP   0 zero   q quit")
    stdscr.addstr(16, 2, "This is NOT an emergency stop. The e-stop disconnects in hardware.",
                  curses.A_DIM)
    if msg:
        stdscr.addstr(18, 2, msg[:w - 4], curses.A_BOLD)
    stdscr.refresh()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port", help="e.g. /dev/cu.usbmodem3041")
    ap.add_argument("--poles", type=int, default=0,
                    help="number of pole pairs, only for the conversion to 1/min. "
                         "Without it nothing is converted — a guessed "
                         "pole count would give an invented speed.")
    args = ap.parse_args()

    vesc = Vesc(args.port)
    try:
        curses.wrapper(run, vesc, args.poles)
    finally:
        # Also on Ctrl-C or an error: release the drive.
        try:
            for _ in range(3):
                vesc.release()
                time.sleep(0.05)
        except Exception:
            pass
        vesc.s.close()
        print("Drive released.")


if __name__ == "__main__":
    main()
