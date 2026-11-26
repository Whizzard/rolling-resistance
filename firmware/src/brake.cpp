#include "brake.h"

#include "config.h"
#include "logger.h"
#include "revsrc.h"

namespace brake {
namespace {

enum St : uint8_t { RELEASED, PENDING, ENGAGED };
St       g_st = ENGAGED;
uint32_t g_sinceMs = 0, g_releasedMs = 0, g_stillMs = 0;

void setPin(bool release) { digitalWrite(PIN_BRAKE_RELEASE, release ? HIGH : LOW); }

}  // namespace

void begin() {
  pinMode(PIN_BRAKE_RELEASE, OUTPUT);
  setPin(false);                       // as at reset: braked
  g_st = ENGAGED; g_sinceMs = millis();
  if (RELAY_BRAKE_WIRED) { setPin(true); g_st = RELEASED; g_releasedMs = millis(); }
}

bool available() { return RELAY_BRAKE_WIRED; }

// Descent during a run (speedctl): energise the relays again BEFORE standstill, ONCE per
// setpoint. The controller is released (N2); the contacts open the short under current.
void releaseEarly() {
  if (!RELAY_BRAKE_WIRED || g_st == RELEASED) return;
  setPin(true); g_st = RELEASED; g_releasedMs = millis(); g_stillMs = 0;
}

void engage() {
  if (!RELAY_BRAKE_WIRED || g_st != RELEASED) return;
  g_st = PENDING; g_sinceMs = millis();   // first RELAY_DRIVER_OFF_MS, then drop out (N2)
}

void service() {
  if (!RELAY_BRAKE_WIRED) return;
  const uint32_t now = millis();
  if (g_st == PENDING && now - g_sinceMs >= RELAY_DRIVER_OFF_MS) {
    setPin(false); g_st = ENGAGED; g_sinceMs = now; g_stillMs = 0;
    logger::note("Relay brake: motor shorted");
  }
  if (g_st == ENGAGED) {
    // Standstill from revsrc (drum marks, otherwise controller Hall below 267 1/min).
    // NAN -> no standstill: without a speed the brake stays until someone measures.
    if (revsrc::revPerSec() < STANDSTILL_RPS) {
      if (g_stillMs == 0) g_stillMs = now;
      else if (now - g_stillMs >= RELAY_RELEASE_STILL_MS) {
        setPin(true); g_st = RELEASED; g_releasedMs = now;
        logger::note("Relay brake: drum stopped, relays on");
      }
    } else {
      g_stillMs = 0;
    }
  }
}

bool active()  { return RELAY_BRAKE_WIRED && g_st != RELEASED; }
bool engaged() { return RELAY_BRAKE_WIRED && g_st == ENGAGED; }
uint32_t releasedForMs() { return g_st == RELEASED ? millis() - g_releasedMs : 0; }
const char* stateName() {
  if (!RELAY_BRAKE_WIRED) return "not wired";
  switch (g_st) { case RELEASED: return "energized"; case PENDING: return "dropping"; default: return "shorted"; }
}

}  // namespace brake
