#pragma once
//
// Wheel circumference from the speed ratio -- integrated, not instantaneous.
//
// At the contact patch the surface speeds are equal, so
//     U_wheel = U_drum · (drum revolutions / wheel revolutions)
// over THE SAME time window. The wheel revolutions are exact: the sensor delivers
// WHEEL_MARKS_PER_REV pulses per revolution, and the window starts and ends
// on a pulse. The drum revolutions are integrated over time
// (revsrc::revPerSec, from the controller or the marks).
//
// SLIDING WINDOW of the last WHEEL_CIRC_WINDOW_REVS wheel revolutions, recomputed on
// every pulse: continuous, but smoothed. An instantaneous value from two speeds
// fluttered with every pulse (as of 2026-11-22); a growing window with a steadiness
// condition never filled, because the control oscillates on the plateau.
// Like this since 2026-11-23. At wheel standstill it expires.
//
// The result is the EFFECTIVE rolling circumference under load, not the nominal size.
//
#include <Arduino.h>

namespace wheel {

void begin();
void service();           // cyclically from the main loop

bool     valid();         // enough revolutions at steady speed
float    circumferenceRawM();  // before the plausibility check, diagnostics
float    circumferenceM();// NAN as long as not valid()
uint32_t revsUsed();      // wheel revolutions in the window, at most WHEEL_CIRC_WINDOW_REVS

// WHEEL SPIN-DOWN (docs/08-stand.md §6, 09-measurement-theory eq. 12): wheel lifted,
// spun up by hand or by the drive, then left to coast freely. The pulse intervals
// give w_wheel(t), J_wheel turns the deceleration into a torque:
//     -J dw/dt = A1 + B1 w^2
// Fitted with the INTEGRAL METHOD (w(t) = w0 - a t - b Int w^2 dt, linear in
// w0, a, b) via regression sums while the wheel coasts; once it stands, the result is
// saved (settings::setWheelModel). J_wheel comes measured from the settings
// (bifilar pendulum, settings::wheelJKgm2) or estimated as WHEEL_J_FACTOR m r_wheel^2
// with the wheel mass from the settings and r_wheel from the measured circumference --
// so that must have been determined once, and the mass entered.
bool  spindownBegin();     // false: mass or circumference missing
bool  spindownActive();
uint32_t spindownRevs();
float jWheelKgm2();          // NAN if mass or circumference missing
bool  jWheelMeasured();      // true: from the bifilar pendulum, otherwise estimated
float spindownResidRadS(); // residual of the last fit, rad/s; NAN without a fit
uint32_t spindownFitN();   // support points of the last fit

}  // namespace wheel
