# heatshrink (encoder only)

LZSS compressor for embedded systems by Scott Vokes, ISC licence (`LICENSE`).
Source: https://github.com/atomicobject/heatshrink, commit 7d419e1 (2015-12-08), files
`heatshrink_encoder.c/.h` and `heatshrink_common.h` unchanged; `heatshrink_config.h` is
this project's (static, window 2^10, lookahead 2^5). Used by the console command `Gz`
(file transfer over USB, `tools/console.py --get`); the decoder is in `tools/console.py`.
