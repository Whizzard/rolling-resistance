#!/usr/bin/env python3
"""
Backt eine HTML-Datei als gzip-Bytefeld in einen C++-Header.

    python3 embed.py index.html ../src/webui_html.h WEBUI_HTML_GZ

Die Quelle ist das HTML — dort wird editiert. Dieses Skript ist der Erzeuger, nicht
die Wahrheit. platformio.ini ruft es als pre-Skript auf, damit der Header nicht
veralten kann.
"""
import gzip, pathlib, sys

if len(sys.argv) != 4:
    sys.exit(f"Aufruf: {sys.argv[0]} <quelle.html> <ziel.h> <SYMBOL>")

src, dst, sym = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]

raw = src.read_bytes()
# mtime=0, damit gleicher Inhalt gleiche Bytes ergibt und der Build nicht jedes Mal
# neu linkt.
gz = gzip.compress(raw, 9, mtime=0)

lines = ["  " + ", ".join(f"0x{b:02x}" for b in gz[i:i + 16]) + ","
         for i in range(0, len(gz), 16)]

dst.write_text(
    "#pragma once\n"
    f"// ERZEUGT von firmware/web/embed.py aus {src.name} — nicht editieren.\n"
    f"// Quelle {len(raw)} Bytes, gzip {len(gz)} Bytes ({100 * len(gz) // len(raw)} %).\n"
    "#include <Arduino.h>\n\n"
    f"constexpr size_t {sym}_LEN = {len(gz)};\n"
    f"const uint8_t {sym}[] PROGMEM = {{\n"
    + "\n".join(lines) + "\n};\n",
    encoding="utf-8")

print(f"{dst.name}: {len(raw)} -> {len(gz)} Bytes gzip ({100 * len(gz) // len(raw)} %)")
