#pragma once
//
// Reifendruck.
//
// Stand: Handeingabe. Der BLE-Client ist ein Rumpf — Sensormodell und damit
// Service-/Characteristic-UUIDs stehen nicht fest (siehe docs/06-offene-fragen.md B4).
// Wichtig ist, dass der Wert von Anfang an im CSV steht, egal woher er kommt.
//
#include <Arduino.h>

namespace tirepressure {

enum Source : uint8_t { SRC_NONE = 0, SRC_MANUAL, SRC_BLE };

void begin();
void service();

float bar();          // NAN, solange nichts gesetzt wurde
Source source();
const char* sourceName();

void setManual(float bar);

}  // namespace tirepressure
