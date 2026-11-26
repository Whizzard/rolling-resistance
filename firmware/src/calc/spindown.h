#pragma once
// Spin-down fit by the integral method (docs/09-measurement-theory.md §9.3):
//   w(t) = w0 - (A/J) t - (B/J) I(t),   I(t) = Int_0^t w^2 dt
// Linear in (w0, -A/J, -B/J); the 3x3 normal equations are solved by Cramer's
// rule. Only sums are collected, no list of points.
#include <cmath>
#include <cstdint>

namespace calc {

struct SpindownSums {
  double n = 0, t = 0, i = 0, tt = 0, ti = 0, ii = 0, y = 0, yt = 0, yi = 0, yy = 0;
  void reset() { *this = SpindownSums(); }
  void add(double tS, double integral, double w) {
    n++; t += tS; i += integral; tt += tS * tS; ti += tS * integral; ii += integral * integral;
    y += w; yt += w * tS; yi += w * integral; yy += w * w;
  }
  static double det3(double a, double b, double c, double d, double e, double f, double g, double h, double k) {
    return a * (e * k - f * h) - b * (d * k - f * g) + c * (d * h - e * g);
  }
  // a1, b1 in Nm and Nm/(rad/s)^2 respectively for the moment of inertia J; residRms in rad/s.
  // false if the normal equations are singular (too few or identical points).
  bool solve(double J, float& a1, float& b1, double& w0, float& residRms) const {
    const double D = det3(n, t, i, t, tt, ti, i, ti, ii);
    if (n < 3 || std::fabs(D) < 1e-18) return false;
    w0 = det3(y, t, i, yt, tt, ti, yi, ti, ii) / D;
    const double ca = det3(n, y, i, t, yt, ti, i, yi, ii) / D;   // = -A/J
    const double cb = det3(n, t, y, t, tt, yt, i, ti, yi) / D;   // = -B/J
    const double rss = yy - w0 * y - ca * yt - cb * yi;          // residual from the sums
    residRms = rss > 0 ? static_cast<float>(std::sqrt(rss / n)) : 0.0f;
    a1 = static_cast<float>(-ca * J);
    b1 = static_cast<float>(-cb * J);
    return true;
  }
};

}  // namespace calc
