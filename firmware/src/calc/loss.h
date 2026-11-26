#pragma once
// Loss torque at the drum shaft from the two models (docs/09 §3, §9):
//   baseline    M_idle(w)  = A0 + B0 w^2               at the drum
//   test wheel  M_wheel(w) = A1 + B1 w_wheel^2         at the WHEEL axle,
//               converted with r_drum / r_wheel (same surface speed).
// A model that is missing contributes 0. Same code for live display (main.cpp)
// and report (run.cpp); analysis/crr.py computes the same in Python.
#include <cmath>

namespace calc {

inline float lossNm(float omegaDrum, float rDrum,
                    float a0, float b0, bool haveBase,
                    float a1, float b1, bool haveWheel, float uWheel) {
  float m = 0.0f;
  if (haveBase && !std::isnan(a0) && !std::isnan(b0)) m += a0 + b0 * omegaDrum * omegaDrum;
  if (haveWheel && !std::isnan(uWheel) && uWheel > 0.1f && rDrum > 0.0f) {
    const float rWheel = uWheel / (2.0f * static_cast<float>(M_PI));
    const float wWheel = omegaDrum * rDrum / rWheel;
    m += (a1 + b1 * wWheel * wWheel) * rDrum / rWheel;
  }
  return m;
}

}  // namespace calc
