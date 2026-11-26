#pragma once
// Relay brake (docs/04-hardware.md §9): three automotive relays in the coil chain on the bus,
// below them the emergency stop, below that Q1 on GPIO40. Energised (GPIO40 high) = motor on
// the controller; dropped out = motor shorted = braked.
//
// RULE: the relays are energised as long as the firmware runs. The firmware lets them
// drop for a STOP (command s, FAULT, end of run), at any speed
// (decision 2026-11-25, config.h RELAY BRAKE), and energises them again as soon as the
// drum has stood for RELAY_RELEASE_STILL_MS. In addition a descent during a run: ONE braking
// action per setpoint, released with releaseEarly() before the target (config.h DESCENT
// IN THE RUN). Coast-down test and manual zero check stay as before.
// At reset GPIO40 is low (R2), so braked until begin() runs (N6).
#include <Arduino.h>

namespace brake {

void begin();
void service();
bool available();        // RELAY_BRAKE_WIRED
void engage();           // request braking via the relays; the controller must already be released
void releaseEarly();     // descent over: energise the relays without waiting for standstill
bool active();           // requested or dropped out, until the drum stands
bool engaged();          // dropped out = shorted
uint32_t releasedForMs();// since the last energising (0 while not energised)
const char* stateName(); // for status and /api/state

}  // namespace brake
