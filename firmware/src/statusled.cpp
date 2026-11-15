#include "statusled.h"
#include "config.h"

namespace statusled {
namespace {

uint32_t g_t0       = 0;
uint32_t g_lastStep = 0;
bool     g_done     = false;

// Farbkreis ohne Gleitkomma: sechs Sektoren zu je 256 Schritten, also 1536 gesamt.
// In jedem Sektor läuft genau ein Kanal linear hoch oder herunter.
void hueToRgb(uint16_t hue, uint8_t& r, uint8_t& g, uint8_t& b) {
  const uint8_t sector = static_cast<uint8_t>(hue >> 8);
  const uint8_t pos    = static_cast<uint8_t>(hue & 0xFF);
  switch (sector) {
    case 0:  r = 255;       g = pos;       b = 0;         break;
    case 1:  r = 255 - pos; g = 255;       b = 0;         break;
    case 2:  r = 0;         g = 255;       b = pos;       break;
    case 3:  r = 0;         g = 255 - pos; b = 255;       break;
    case 4:  r = pos;       g = 0;         b = 255;       break;
    default: r = 255;       g = 0;         b = 255 - pos; break;
  }
}

}  // namespace

void begin() {
  g_t0       = millis();
  g_lastStep = 0;
  g_done     = false;
  neopixelWrite(PIN_STATUS_LED, 0, 0, 0);
}

bool busy() { return !g_done; }

void service() {
  if (g_done) return;

  const uint32_t dt = millis() - g_t0;

  if (dt >= BOOT_RAINBOW_MS) {
    neopixelWrite(PIN_STATUS_LED, 0, 0, 0);
    g_done = true;
    return;
  }
  if (dt - g_lastStep < BOOT_RAINBOW_STEP_MS) return;
  g_lastStep = dt;

  // Ein voller Farbkreis über die gesamte Dauer. dt < BOOT_RAINBOW_MS, also hue < 1536.
  const uint16_t hue = static_cast<uint16_t>(static_cast<uint32_t>(dt) * 1536u
                                             / BOOT_RAINBOW_MS);
  uint8_t r, g, b;
  hueToRgb(hue, r, g, b);
  neopixelWrite(PIN_STATUS_LED, r / STATUS_LED_DIV, g / STATUS_LED_DIV,
                b / STATUS_LED_DIV);
}

}  // namespace statusled
