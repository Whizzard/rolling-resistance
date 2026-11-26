#include "drivespeed.h"

#include <Arduino.h>

#include "config.h"
#include "settings.h"
#include "vesc/vesc.h"

namespace drivespeed {

float rpm() {
  if (!vesc::connected()) return NAN;
  return static_cast<float>(vesc::values().erpm) / MOTOR_POLE_PAIRS * settings::directionSign();
}

float kmh() { return kmhFromRevPerSec(rpm() / 60.0f); }

}  // namespace drivespeed
