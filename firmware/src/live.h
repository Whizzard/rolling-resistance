#pragma once
// Live values at steady speed: torque, power loss and Crr, averaged over
// LIVE_WINDOW_MS and corrected for idle and wheel losses (config.h STEADY_*).
// Part of main.cpp until 2026-11-25.
#include <Arduino.h>

#include "run.h"

namespace live {

struct Result { float mNm, pW, crr; bool corrected; uint16_t n; float dvdtNet; };

void   service(bool newSample);   // every pass; newSample: a new cell value is available
Result result();                  // all NAN while not steady, not calibrated or window empty
bool   steady();                  // speed counts as steady
bool   forRun(run::LiveVals& out);// live values for the guided measurement run (run.h)

}  // namespace live
