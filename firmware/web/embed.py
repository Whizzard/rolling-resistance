#!/usr/bin/env python3
"""
Backt firmware/web/index.html als gzip-Bytefeld nach firmware/src/webui_html.h.

Die Quelle ist index.html — dort wird editiert. Dieses Skript ist der Erzeuger,
nicht die Wahrheit. Nach jeder Aenderung an index.html erneut laufen lassen;
platformio.ini ruft es als pre-Skript automatisch auf.
"""
import gzip, pathlib, sys

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE / "index.html"
OUT = HERE.parent / "src" / "webui_html.h"

raw = SRC.read_bytes()
# mtime=0, damit identischer Inhalt identische Bytes ergibt und der Build nicht
# bei jedem Lauf neu linkt.
gz = gzip.compress(raw, 9, mtime=0)

lines = []
for i in range(0, len(gz), 16):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in gz[i:i+16]) + ",")

OUT.write_text(
    "#pragma once\n"
    "// ERZEUGT von firmware/web/embed.py aus firmware/web/index.html — nicht editieren.\n"
    f"// Quelle {len(raw)} Bytes, gzip {len(gz)} Bytes ({100*len(gz)//len(raw)} %).\n"
    "#include <Arduino.h>\n\n"
    f"constexpr size_t WEBUI_HTML_GZ_LEN = {len(gz)};\n"
    "const uint8_t WEBUI_HTML_GZ[] PROGMEM = {\n"
    + "\n".join(lines) + "\n};\n",
    encoding="utf-8")

print(f"webui_html.h: {len(raw)} -> {len(gz)} Bytes gzip ({100*len(gz)//len(raw)} %)")
