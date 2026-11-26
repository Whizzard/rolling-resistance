#!/usr/bin/env python3
"""
Produces from firmware/web/index.html the version for the Artifact environment.

The source is the complete, standalone document that the test stand serves.
The Artifact environment provides <!doctype>, <head> and <body> itself,
so this frame is removed here — otherwise it would be there twice.
"""
import pathlib, sys

src = pathlib.Path(__file__).resolve().parent / "index.html"
dst = pathlib.Path(sys.argv[1])

t = src.read_text(encoding="utf-8")
t = t[t.index("<title>"):]                 # everything before <title> is dropped
t = t.replace("</head>\n<body>\n", "", 1)
t = t.rstrip()
for tail in ("</html>", "</body>"):
    if t.endswith(tail):
        t = t[: -len(tail)].rstrip()
dst.write_text(t + "\n", encoding="utf-8")
print(f"{dst.name}: {len(t)//1024} KB")
