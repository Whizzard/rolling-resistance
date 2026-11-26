#pragma once
//
// GUIDED MEASUREMENT RUN ("Measurement" on the measurement page), since 2026-11-23.
//
// Fourteen steps, rationale in docs/11-review.md §3:
//
//   A Preparation (hands)        1 Create run  2 Visual inspection  3 System check
//                                4 Zeroing (wheel lifted, rotor free) + zero check
//                                  by turning further  5 Check weight  6 Idle-loss model
//                                7 Wheel model  8 Wheel placed
//   B Measurement drive (alone)  9 Warm-up  10 Reference point + plateaus
//                                11 Reference point after  12 Stopping
//   C Completion                 13 Zero point + idle-loss post-run (optional)  14 Report
//
// Since 2026-11-24 (docs/11-review.md): every point is corrected for J*dw/dt, the
// reference point (WARMUP_KMH) is driven right after the warm-up and after the
// plateaus (anchor drift over the whole measurement drive), pressure and tire
// temperature come with every point from the AirSpy, and the uncertainty per plateau
// uses the pooled spread of all plateaus.
//
// Everything that needs hands comes before the warm-up. After that the stand runs
// without an operator, and at the end report (.md) and data (.csv) are in the flash
// (LittleFS, /runs/<id>.*), retrievable via the user interface.
//
// The run sits ABOVE the state machine (state.h): it starts sequences and gets every
// measurement point back with its spread. Everything that deviates during the run
// becomes a numbered warning (W01 ...) in the report; none of it disappears in the
// console.
//
// Commands (console and user interface, prefix 'R'):
//   R          start               Rk  main button of the step (Continue, Confirmed, New)
//   Rs         secondary button (Skip, Adopt)
//   Rn         Again (check weight, system check)
//   Rb         Back, one preparation step (only while nothing is driving)
//   Rx         abort and close
//   Rm<k>=<v>  metadata: t tire, w wheel, b pressure bar, c room °C (start),
//              e room °C (end), p position of the sliding weight mm (-> wheel load from
//              the lever balance, settings::loadFromPosN), l wheel load MEASURED kg
//              (hanging scale, overrides the calculation, empty = calculation), n note,
//              d Unix time s
//
#include <Arduino.h>

namespace run {

// Live values come from main.cpp (power loss and Crr at steady speed).
struct LiveVals { float mNm, pW, crr; bool corr; };
typedef bool (*LiveFn)(LiveVals& out);

void begin(LiveFn live);
void service();                       // cyclic, non-blocking

bool command(const char* text);       // everything after the 'R'; false = refused
bool active();                        // step 1..14 or abort visible
// A run that is still measuring: active() minus the report / abort page at the end.
// Downloads, the SD listing, formatting and the raw log go by this (2026-11-26).
bool busy();
const char* id();                     // run id = session directory under storage::ROOT_DIR, "" if none
uint8_t step();                       // 0 off, 1..14, 99 aborted

// State of the run for /api/run, JSON into the buffer.
void json(char* buf, size_t len);

}  // namespace run
