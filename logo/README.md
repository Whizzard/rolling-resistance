# vmrr logo

As of 2026-11-26. Mark **E3 "Rolling"**, chosen from the proposals of 2026-11-26: a wheel
whose elapsed time is a filled sector (the measurement), speed lines behind it, the road
underneath. Geometric, line weight 4 on a 64 grid (speed lines 3).

| File | Use |
|---|---|
| `vmrr-mark.svg` | Mark, switches to white/yellow in a dark colour scheme; favicon of vmrr.de |
| `vmrr-mark-light.svg`, `vmrr-mark-dark.svg` | Fixed colours for light and dark backgrounds |
| `vmrr-icon.svg` | Mark inset on white, source of the opaque icons |
| `favicon.ico` (32, 16 px), `favicon-32.png` | Classic favicon |
| `apple-touch-icon.png` (180 px), `vmrr-icon-512.png` | Home-screen and app icons |

Colours: ink #16202a, accent #2f6f99 (the web UI blue); dark: #ffffff, accent #ffd23f.

Used in: the web UI (`firmware/web/index.html`: favicon and touch icon embedded, the mark
inline at the top left) and vmrr.de (in the hosts admin repo, `sites/vmrr.de/`).
The PNGs are rendered with `inkscape <svg> -w N -h N -o <png>`, the ICO with
`magick favicon-32.png -define icon:auto-resize=32,16 favicon.ico` (then `chmod 644`).
