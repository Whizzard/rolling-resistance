#pragma once
// Load-cell calibration as a guided procedure (2026-11-26, builder: "when I calibrate, I
// calibrate"). Replaces adopting the check weight inside a run.
//
//   zero (no weight) -> loads in any order, each with its mass -> zero again -> evaluate
//
// Every reading is the mean over CAL_AVG_MS at standstill. Evaluation: least-squares line
// counts = a + b * F over all readings, F = m g L_cal / L_arm at the cell; factor 1/|b|.
// Reported in % of the cell's full scale: linearity (largest residual of a load), hysteresis
// (largest spread between readings of the same mass, zero included) and zero return (last
// zero minus first). Accept stores factor and record (settings::CalRecord) and appends a
// line to the calibration book /vmrr/calibration.md on the SD.
//
// Console: C0 start (reads zero) · Cm<kg> read with this mass hanging · Cz read zero ·
//          Ce evaluate · Ca accept · Cx cancel.
#include <Arduino.h>
#include "settings.h"

namespace calib {

enum St : uint8_t { OFF = 0, READING, READY, RESULT };

bool command(const char* t);   // everything after the 'C'; false (with a message) if refused
void service();                // cyclic, non-blocking
St   state();
// Single-point calibration (console k<kg>): stores the factor with its record and book line.
bool singlePoint(float npc);
// Is the stored factor valid for the stand as it is? 1 yes, 0 conditions unknown (no
// record), -1 no; text says why. Used by the run's checks.
int8_t validity(char* why, size_t n);
// For the state JSON: ,"calib":{...} or nothing while OFF and no result.
size_t json(char* out, size_t n);
// Protocol line for the run report.
void describe(Print& o);

}  // namespace calib
