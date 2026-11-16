#!/usr/bin/env python3
"""
Erzeugt aus firmware/web/index.html die Fassung fuer die Artifact-Umgebung.

Die Quelle ist das vollstaendige, eigenstaendige Dokument, das der Pruefstand
ausliefert. Die Artifact-Umgebung stellt <!doctype>, <head> und <body> selbst,
also wird dieser Rahmen hier entfernt — sonst waere er doppelt.
"""
import pathlib, sys

src = pathlib.Path(__file__).resolve().parent / "index.html"
dst = pathlib.Path(sys.argv[1])

t = src.read_text(encoding="utf-8")
t = t[t.index("<title>"):]                 # alles vor <title> faellt weg
t = t.replace("</head>\n<body>\n", "", 1)
t = t.rstrip()
for tail in ("</html>", "</body>"):
    if t.endswith(tail):
        t = t[: -len(tail)].rstrip()
dst.write_text(t + "\n", encoding="utf-8")
print(f"{dst.name}: {len(t)//1024} KB")
