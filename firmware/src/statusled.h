#pragma once
//
// Status LED of the DevKit -- addressable RGB LED (WS2812) on PIN_STATUS_LED.
//
// It shows the state of the OPERATOR CONNECTION, nothing else:
//   green, steady  -- an operator computer checks in within LINK_TIMEOUT_MS
//   red, blinking  -- nobody is waiting yet, or it has gone away
//
// This makes the dead-man function readable from three metres away without
// looking at a screen.
//
#include <Arduino.h>

namespace statusled {

void begin();    // LED off
void service();  // call cyclically; non-blocking
bool busy();     // always false; kept for the caller

}  // namespace statusled
