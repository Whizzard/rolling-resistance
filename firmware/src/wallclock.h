#pragma once
//
// Date and time from the web client: the stand has no clock of its own and no internet.
// The browser appends its local time in seconds to every state request
// (/api/state?now=...), from then on millis() keeps counting.
//
#include <Arduino.h>

namespace wallclock {
void     set(uint32_t localEpochS);   // from the browser, local time in s since 1970
uint32_t now();                       // local time in s; 0 as long as no browser has been there
}  // namespace wallclock
