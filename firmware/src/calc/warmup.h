#pragma once
// Has the warm-up settled? (run.cpp, since 2026-11-26)
//
// Input: the last n minute values of the corrected torque M (Nm) and, if the pressure
// sensor sends it, the tire temperature (degC), oldest first. Settled when max - min of
// the n torques is at most tol (absolute: M is the small remainder after the losses, a
// relative band would scale with it) and the tire temperature has risen by at most
// kPerMin per minute over the window. NaN temperatures (no sensor) skip the temperature
// test; a NaN torque means "not settled".
// Pure function, host-tested (test/test_calc).
#include <cmath>
#include <cstdint>

namespace calc {

inline bool warmupSettled(const float* p, const float* tTire, uint8_t n, float tol, float kPerMin) {
  if (n < 2) return false;
  float lo = p[0], hi = p[0];
  for (uint8_t i = 0; i < n; i++) {
    if (std::isnan(p[i])) return false;
    lo = p[i] < lo ? p[i] : lo; hi = p[i] > hi ? p[i] : hi;
  }
  if (hi - lo > tol) return false;
  if (tTire && !std::isnan(tTire[0]) && !std::isnan(tTire[n - 1])
      && (tTire[n - 1] - tTire[0]) / static_cast<float>(n - 1) > kPerMin) return false;
  return true;
}

}  // namespace calc
