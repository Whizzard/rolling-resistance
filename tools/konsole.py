#!/usr/bin/env python3
"""Konsole des Pruefstands ueber USB: Befehle schicken, Antworten mitschreiben.

Dasselbe Befehlsvokabular wie Oberflaeche und serielle Konsole (docs/10-firmware.md
G5). Gedacht fuers Fehlersuchen und Steuern vom Rechner aus, ohne Browser:

    ~/.platformio/penv/bin/python tools/konsole.py i
    ~/.platformio/penv/bin/python tools/konsole.py j8 v15 sleep:6 c
    ~/.platformio/penv/bin/python tools/konsole.py --watch 30
    ~/.platformio/penv/bin/python tools/konsole.py --log lauf.txt j20 m0

Argumente sind Befehle, der Reihe nach; `sleep:<s>` wartet dazwischen. Nach dem
letzten Befehl wird noch --tail Sekunden mitgelesen (Vorgabe 2, bei `j<s>`
automatisch bis die Telemetrie zu Ende ist). Jede Zeile bekommt einen Zeitstempel
in Sekunden seit Start; eigene Befehle stehen mit `>>>` im Mitschnitt.

Der Port wird gesucht (/dev/cu.usbmodem*), --port setzt ihn fest. Braucht pyserial,
PlatformIOs Python hat es.

STOP ist immer `s`, Auslaufen `c`. Beides geht auch, waehrend ein j laeuft.
"""
import argparse
import glob
import re
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial fehlt — ~/.platformio/penv/bin/python benutzen")


def find_port(explicit):
    if explicit:
        return explicit
    ports = sorted(glob.glob('/dev/cu.usbmodem*')) or sorted(glob.glob('/dev/ttyACM*'))
    if not ports:
        sys.exit("kein USB-Port gefunden — haengt das Board am USB?")
    return ports[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('befehle', nargs='*', help="Befehle, dazwischen sleep:<s>")
    ap.add_argument('--port', help="serieller Port, sonst /dev/cu.usbmodem*")
    ap.add_argument('--watch', type=float, default=0.0,
                    help="nur mitlesen, so viele Sekunden")
    ap.add_argument('--tail', type=float, default=2.0,
                    help="nach dem letzten Befehl noch so lange lesen")
    ap.add_argument('--log', help="Mitschnitt zusaetzlich in diese Datei")
    a = ap.parse_args()

    port = find_port(a.port)
    s = serial.Serial(port, 115200, timeout=0.1)
    t0 = time.time()
    logf = open(a.log, 'a') if a.log else None

    def out(line):
        txt = "%7.2f  %s" % (time.time() - t0, line)
        print(txt)
        sys.stdout.flush()
        if logf:
            logf.write(txt + "\n")
            logf.flush()

    def read_for(seconds):
        end = time.time() + seconds
        rest = b""
        while time.time() < end:
            d = s.read(4096)
            if not d:
                continue
            rest += d
            while b"\n" in rest:
                ln, rest = rest.split(b"\n", 1)
                out(ln.decode('utf-8', 'replace').rstrip("\r"))
        if rest.strip():
            out(rest.decode('utf-8', 'replace'))

    out("# Port %s" % port)
    time.sleep(0.3)
    s.reset_input_buffer()

    if a.watch > 0 and not a.befehle:
        read_for(a.watch)
        return

    tail = a.tail
    for b in a.befehle:
        m = re.match(r'^sleep:(\d+(?:\.\d+)?)$', b)
        if m:
            read_for(float(m.group(1)))
            continue
        s.write((b + "\n").encode())
        out(">>> " + b)
        mj = re.match(r'^j(\d+(?:\.\d+)?)?$', b)
        if mj:
            tail = max(tail, float(mj.group(1) or 5) + 0.5)
        read_for(0.3)
    read_for(tail)


if __name__ == '__main__':
    main()
