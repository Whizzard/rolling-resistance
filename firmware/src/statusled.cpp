#include "statusled.h"
#include "config.h"
#include "webui.h"

namespace statusled {

// State of the operator connection, the LED shows nothing more:
//   green, steady  -- an operator computer checks in within LINK_TIMEOUT_MS
//   red, blinking  -- nobody is waiting yet, or it has gone away
//
// A rainbow used to run here at boot as a sign of life. It is no longer needed:
// the connection indicator says the same and something useful besides, and it is
// up a few milliseconds after the start.
//
// Written only on a state change. neopixelWrite() occupies the RMT and takes
// about 30 us; doing that on every loop pass would be a waste.

void begin() {
  neopixelWrite(PIN_STATUS_LED, 0, 0, 0);
}

void service() {
  const bool up = webui::clientSeen() && webui::linkAlive();
  static bool lastUp = false;
  static bool lastOn = false;
  static bool first  = true;

  if (up) {
    if (first || !lastUp || !lastOn) {
      neopixelWrite(PIN_STATUS_LED, 0, 190 / STATUS_LED_DIV, 0);
      lastOn = true;
    }
  } else {
    const bool on = (millis() / STATUS_BLINK_MS) % 2 == 0;
    if (first || on != lastOn || lastUp) {
      neopixelWrite(PIN_STATUS_LED, on ? 220 / STATUS_LED_DIV : 0, 0, 0);
      lastOn = on;
    }
  }
  lastUp = up;
  first  = false;
}

bool busy() { return false; }

}  // namespace statusled
