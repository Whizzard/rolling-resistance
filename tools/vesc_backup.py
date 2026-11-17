#!/usr/bin/env python3
"""Sichert die vollstaendige Konfiguration des Reglers. NUR lesend."""
import serial, struct, sys, time, pathlib, datetime

def crc16(d):
    c = 0
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c

def frame(p):
    n = len(p)
    head = bytes([2, n]) if n <= 255 else bytes([3, n >> 8, n & 0xFF])
    return head + p + struct.pack('>H', crc16(p)) + b'\x03'

def unframe(raw):
    i = 0
    while i < len(raw):
        if raw[i] == 2 and i + 2 <= len(raw):
            ln, start = raw[i+1], i + 2
        elif raw[i] == 3 and i + 3 <= len(raw):
            ln, start = (raw[i+1] << 8) | raw[i+2], i + 3
        else:
            i += 1; continue
        end = start + ln
        if end + 3 <= len(raw) and raw[end+2] == 3 and \
           crc16(raw[start:end]) == struct.unpack('>H', raw[end:end+2])[0]:
            return raw[start:end]
        i += 1
    return None

def transact(s, payload, timeout=3.0):
    s.reset_input_buffer()
    s.write(frame(payload)); s.flush()
    raw, t0 = b'', time.time()
    while time.time() - t0 < timeout:
        chunk = s.read(512)
        if chunk:
            raw += chunk
            r = unframe(raw)
            if r is not None:
                return r
        else:
            time.sleep(0.02)
    return None

port, outdir = sys.argv[1], pathlib.Path(sys.argv[2])
outdir.mkdir(parents=True, exist_ok=True)
s = serial.Serial(port, 115200, timeout=0.2)
time.sleep(0.3)

fw = transact(s, bytes([0]))
major, minor = fw[1], fw[2]
hw = bytes(fw[3:]).split(b'\x00')[0].decode('ascii', 'replace')
uuid = fw[3+len(hw)+1 : 3+len(hw)+1+12].hex()
print(f"Firmware {major}.{minor:02d}, Hardware {hw!r}, UUID {uuid}")

summary = [f"# VESC-Konfiguration, gesichert {datetime.date.today().isoformat()}",
           f"# Firmware {major}.{minor:02d}   Hardware {hw}   STM32-UUID {uuid}",
           f"# Rohbytes wie vom Regler geliefert. Zum Zurueckspielen genau so",
           f"# als Nutzlast von COMM_SET_MCCONF (13) bzw. COMM_SET_APPCONF (16)",
           f"# senden — mit der Kennung an Position 0 ersetzt.", ""]

for name, cmd in (("mcconf", 14), ("appconf", 17)):
    r = transact(s, bytes([cmd]))
    if r is None:
        print(f"{name}: KEINE ANTWORT"); summary.append(f"{name}: keine Antwort"); continue
    f = outdir / f"vesc-{name}-fw{major}.{minor:02d}.bin"
    f.write_bytes(r)
    sig = struct.unpack_from('>I', r, 1)[0]
    print(f"{name}: {len(r)} Bytes, Signatur {sig} -> {f.name}")
    summary.append(f"{name}: {len(r)} Bytes, Signatur {sig}, Datei {f.name}")
    summary.append("  " + r.hex())
    summary.append("")

(outdir / "vesc-original.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
s.close()
print(f"\n-> {outdir}/vesc-original.txt")
