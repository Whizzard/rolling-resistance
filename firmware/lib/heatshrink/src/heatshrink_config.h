#ifndef HEATSHRINK_CONFIG_H
#define HEATSHRINK_CONFIG_H
/* Project configuration (2026-11-26): static, window 2^10, lookahead 2^5 -- on the raw log of
 * run 2026-11-26-1651 factor 3.54 (2^8/2^4: 3.24, 2^12/2^4: 3.19). About 6 kB of RAM.
 * tools/console.py decodes with the same two numbers. Encoder only. */
#define HEATSHRINK_DYNAMIC_ALLOC 0
#define HEATSHRINK_STATIC_INPUT_BUFFER_SIZE 1024
#define HEATSHRINK_STATIC_WINDOW_BITS 10
#define HEATSHRINK_STATIC_LOOKAHEAD_BITS 5
#define HEATSHRINK_DEBUGGING_LOGS 0
#define HEATSHRINK_USE_INDEX 1
#endif
