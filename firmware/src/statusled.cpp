#include "statusled.h"
#include "config.h"
#include "webui.h"
#include "rawlog.h"

namespace statusled {

// Operator connection and raw-data recording (config.h, since 2026-11-26):
//   blue blinking slowly  -- nobody checks in (yet), dead man's switch open
//   green steady          -- an operator computer checks in within LINK_TIMEOUT_MS
//   red blinking fast     -- raw data are recorded
//   red / blue alternating -- recording without an operator computer
//
// Written only when the colour changes. neopixelWrite() occupies the RMT and takes
// about 30 us; doing that on every loop pass would be a waste.

void begin() {
  neopixelWrite(PIN_STATUS_LED, 0, 0, 0);
}

void service() {
  const bool up  = webui::clientSeen() && webui::linkAlive();
  const bool rec = rawlog::active();
  const uint32_t t = millis();
  const bool fast = (t / STATUS_REC_BLINK_MS) % 2 == 0;
  const bool slow = (t / STATUS_BLINK_MS) % 2 == 0;
  constexpr uint8_t D = STATUS_LED_DIV;
  uint8_t r = 0, g = 0, b = 0;
  if (rec && up)      { if (fast) r = 220 / D; }
  else if (rec)       { if (fast) r = 220 / D; else b = 220 / D; }
  else if (up)        { g = 190 / D; }
  else                { if (slow) b = 220 / D; }
  static uint32_t last = 0xFFFFFFFFu;
  const uint32_t rgb = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
  if (rgb != last) { neopixelWrite(PIN_STATUS_LED, r, g, b); last = rgb; }
}

bool busy() { return false; }

}  // namespace statusled
