#pragma once
// Least-squares line and standard deviation from regression sums.
//
// Arduino-free, so that it is testable on the computer (firmware/test/test_calc,
// `pio test -e native`). Until 2026-11-25 the same calculation was in the firmware
// five times (revsrc, state twice, main, run) -- each with its own degeneracy limit.
// The limits remain the caller's business (minN, minDen), the formula is one.
#include <cmath>
#include <cstdint>

namespace calc {

struct LinFit {
  double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
  void add(double x, double y) { n++; sx += x; sy += y; sxx += x * x; sxy += x * y; }
  void reset() { n = sx = sy = sxx = sxy = 0; }
  double den() const { return n * sxx - sx * sx; }
  // Slope b of y = a + b x. NAN below minN points or when the x do not
  // spread enough (den <= minDen).
  double slope(unsigned minN = 2, double minDen = 1e-12) const {
    if (n < minN || !(den() > minDen)) return NAN;
    return (n * sxy - sx * sy) / den();
  }
  double intercept(double b) const { return n > 0 ? (sy - b * sx) / n : NAN; }
  bool solve(float& a, float& b, unsigned minN = 3, double minDen = 1e-12) const {
    const double bb = slope(minN, minDen);
    if (std::isnan(bb)) return false;
    b = static_cast<float>(bb);
    a = static_cast<float>(intercept(bb));
    return true;
  }
};

// Sample standard deviation from sum and sum of squares. NAN below two
// values; 0 when rounding makes the variance negative.
inline float sdFromSums(double s, double ss, uint32_t n) {
  if (n < 2) return NAN;
  const double var = (ss - s * s / n) / (n - 1);
  return var > 0 ? static_cast<float>(std::sqrt(var)) : 0.0f;
}

}  // namespace calc
