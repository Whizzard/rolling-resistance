#include "statusled.h"
#include "config.h"
#include "webui.h"

namespace statusled {

// Zustand der Bedienverbindung, mehr zeigt die LED nicht:
//   gruen, stetig  — ein Bedienrechner meldet sich innerhalb LINK_TIMEOUT_MS
//   rot, blinkend  — es wartet noch keiner, oder er ist weggeblieben
//
// Frueher lief hier beim Booten ein Regenbogen als Lebenszeichen. Den braucht es
// nicht mehr: die Verbindungsanzeige sagt dasselbe und noch etwas Nuetzliches dazu,
// und sie steht schon wenige Millisekunden nach dem Start.
//
// Geschrieben wird nur bei Zustandswechsel. neopixelWrite() belegt die RMT und
// dauert rund 30 us; das bei jedem Schleifendurchlauf zu tun waere Verschwendung.

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
