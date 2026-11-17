#!/usr/bin/env python3
"""
Messwerte des Reglers mitschreiben — über USB, rein lesend.

    python3 tools/vesc_log.py /dev/cu.usbmodem3041 --out /tmp/vesc.csv

Fragt COMM_GET_VALUES zyklisch ab und schreibt jede Antwort als CSV-Zeile. Sendet
KEINE Sollwerte; der Motor kann davon nicht anlaufen.

Wozu: beim Aufbau zusehen. Motor von Hand drehen und schauen, ob die Drehzahl folgt.
Den Motor anwärmen und schauen, ob die Temperatur steigt — damit ist belegt, ob am
TMP-Eingang wirklich ein Fühler hängt. Spannung und Strom im Leerlauf beobachten.

ACHTUNG, zwei Eigenschaften des Protokolls:

  * Die Strom- und Spannungsvektorfelder sind MITTELWERTE SEIT DER LETZTEN ABFRAGE,
    und das Lesen setzt den Akkumulator zurück. Das Abfrageintervall IST also das
    Mittelungsfenster — steht als Spalte `dt_ms` mit in der Datei.

  * Hängt gleichzeitig das VESC-Tool über USB dran, stehlen sich beide die Proben.
    Immer nur ein Abfrager.
"""
import argparse
import signal
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial fehlt:  pip install pyserial")

COMM_GET_VALUES = 4

FAULT = {
    0: "ok", 1: "ueberspannung", 2: "unterspannung", 3: "drv", 4: "ueberstrom",
    5: "temp_fet", 6: "temp_motor", 7: "drv_uv_hoch", 8: "drv_uv_niedrig",
    9: "mcu_uv", 10: "watchdog", 19: "brk",
}


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


def unframe(buf: bytearray):
    """Liefert vollstaendige Nutzlasten und raeumt sie aus dem Puffer."""
    while True:
        if not buf:
            return
        if buf[0] == 2:
            hdr = 2
            if len(buf) < 2:
                return
            ln = buf[1]
        elif buf[0] == 3:
            hdr = 3
            if len(buf) < 3:
                return
            ln = (buf[1] << 8) | buf[2]
        else:
            del buf[0]
            continue
        if ln == 0 or ln > 512:
            del buf[0]
            continue
        total = hdr + ln + 3
        if len(buf) < total:
            return
        body = bytes(buf[hdr:hdr + ln])
        if buf[total - 1] == 3 and crc16(body) == struct.unpack(">H", buf[hdr + ln:hdr + ln + 2])[0]:
            del buf[:total]
            yield body
        else:
            del buf[0]


# Feste Reihenfolge von COMM_GET_VALUES, seit Firmware 3.x unveraendert.
def parse(d: bytes):
    def i16(o): return struct.unpack_from(">h", d, o)[0]
    def i32(o): return struct.unpack_from(">i", d, o)[0]
    if len(d) < 53:
        return None
    r = {
        "tfet":   i16(0) / 10,
        "tmot":   i16(2) / 10,
        "imot":   i32(4) / 100,
        "iin":    i32(8) / 100,
        "id":     i32(12) / 100,
        "iq":     i32(16) / 100,
        "duty":   i16(20) / 1000,
        "erpm":   i32(22),
        "vin":    i16(26) / 10,
        "ah":     i32(28) / 1e4,
        "wh":     i32(36) / 1e4,
        "tacho":  i32(44),
        "tachoa": i32(48),
        "fault":  d[52],
    }
    return r


COLS = ["t_s", "dt_ms", "erpm", "vin_V", "imot_A", "iin_A", "id_A", "iq_A",
        "duty", "tfet_C", "tmot_C", "tacho", "tacho_abs", "fault", "fault_txt"]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--out", default="/tmp/vesc.csv")
    ap.add_argument("--hz", type=float, default=5.0, help="Abfragerate (Vorgabe 5)")
    ap.add_argument("--seconds", type=float, default=0.0,
                    help="nach so vielen Sekunden beenden; 0 = bis Strg-C")
    args = ap.parse_args()

    s = serial.Serial(args.port, 115200, timeout=0)
    buf = bytearray()
    period = 1.0 / args.hz
    t0 = time.time()
    last_poll = 0.0
    last_rx = t0
    n = 0

    stop = False
    def on_sig(*_):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGINT, on_sig)
    signal.signal(signal.SIGTERM, on_sig)

    with open(args.out, "w", buffering=1, encoding="utf-8") as f:
        f.write("# Messwerte des Motorreglers, rein lesend. Stroeme sind Mittelwerte\n")
        f.write(f"# ueber dt_ms, weil das Lesen den Akkumulator zurueckscheibt.\n")
        f.write(",".join(COLS) + "\n")
        print(f"schreibe {args.out} mit {args.hz:g} Hz — Strg-C beendet", flush=True)

        while not stop:
            now = time.time()
            if args.seconds and now - t0 > args.seconds:
                break
            if now - last_poll >= period:
                last_poll = now
                s.write(frame(bytes([COMM_GET_VALUES])))

            data = s.read(4096)
            if data:
                buf += data
                for body in unframe(buf):
                    if not body or body[0] != COMM_GET_VALUES:
                        continue
                    r = parse(body[1:])
                    if r is None:
                        continue
                    dt = (now - last_rx) * 1000
                    last_rx = now
                    n += 1
                    f.write(f"{now - t0:.3f},{dt:.0f},{r['erpm']},{r['vin']:.1f},"
                            f"{r['imot']:.2f},{r['iin']:.2f},{r['id']:.2f},{r['iq']:.2f},"
                            f"{r['duty']:.3f},{r['tfet']:.1f},{r['tmot']:.1f},"
                            f"{r['tacho']},{r['tachoa']},{r['fault']},"
                            f"{FAULT.get(r['fault'], 'unbekannt')}\n")
                    if n % max(1, int(args.hz)) == 0:
                        print(f"{now - t0:7.1f}s  {r['erpm']:>7d} ERPM  "
                              f"{r['vin']:5.1f} V  {r['imot']:6.2f} A  "
                              f"FET {r['tfet']:5.1f}  Motor {r['tmot']:6.1f}  "
                              f"Tacho {r['tachoa']:>8d}  {FAULT.get(r['fault'], '?')}",
                              flush=True)
            else:
                time.sleep(0.002)

    s.close()
    print(f"\n{n} Zeilen geschrieben nach {args.out}")


if __name__ == "__main__":
    main()
