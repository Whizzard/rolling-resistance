#!/usr/bin/env python3
"""Backs up the complete configuration of the motor controller. READ-ONLY."""
import serial, struct, sys, time, pathlib, datetime

from vescproto import frame, unframe_first

def transact(s, payload, timeout=3.0):
    s.reset_input_buffer()
    s.write(frame(payload)); s.flush()
    raw, t0 = b'', time.time()
    while time.time() - t0 < timeout:
        chunk = s.read(512)
        if chunk:
            raw += chunk
            r = unframe_first(raw)
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

summary = [f"# VESC configuration, backed up {datetime.date.today().isoformat()}",
           f"# Firmware {major}.{minor:02d}   Hardware {hw}   STM32 UUID {uuid}",
           f"# Raw bytes as delivered by the controller. To restore, send exactly these",
           f"# as payload of COMM_SET_MCCONF (13) or COMM_SET_APPCONF (16)",
           f"# — with the command ID at position 0 replaced.", ""]

for name, cmd in (("mcconf", 14), ("appconf", 17)):
    r = transact(s, bytes([cmd]))
    if r is None:
        print(f"{name}: NO REPLY"); summary.append(f"{name}: no reply"); continue
    f = outdir / f"vesc-{name}-fw{major}.{minor:02d}.bin"
    f.write_bytes(r)
    sig = struct.unpack_from('>I', r, 1)[0]
    print(f"{name}: {len(r)} bytes, signature {sig} -> {f.name}")
    summary.append(f"{name}: {len(r)} bytes, signature {sig}, file {f.name}")
    summary.append("  " + r.hex())
    summary.append("")

(outdir / "vesc-original.txt").write_text("\n".join(summary) + "\n", encoding="utf-8")
s.close()
print(f"\n-> {outdir}/vesc-original.txt")
