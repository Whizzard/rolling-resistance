#pragma once
// Relay pulse test (2026-11-26): at standstill the brake relays drop and pull in again
// n times through the normal brake path (brake::engage). Per cycle the edges on all four
// inputs and the largest load-cell excursion within RELAYTEST_WIN_MS after the command are
// compared with an equally long quiet window just before it. Finds phantom pulses
// coupled in by the relay coils or the motor short (found in run 2026-11-26-1542 on the
// test-wheel input). Console X<n>, results as console lines.
#include <Arduino.h>

namespace relaytest {
bool start(uint8_t n);   // false (with a message) if not at standstill, in a run or no relays
void service();          // cyclic, non-blocking
bool active();
}
