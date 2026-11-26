#include "wallclock.h"

namespace wallclock {
namespace {
uint32_t g_epoch = 0, g_atMs = 0;
}
void set(uint32_t localEpochS) {
  // Ignore implausible values (before 2020).
  if (localEpochS < 1577836800UL) return;
  g_epoch = localEpochS; g_atMs = millis();
}
uint32_t now() { return g_epoch ? g_epoch + (millis() - g_atMs) / 1000 : 0; }
}  // namespace wallclock
