#pragma once
// Load-cell calibration, DIRECT at the cell (since 2026-11-28, builder): the weighing arm is
// lifted off the Bowden cable, the stirrup (hardware/weight_stirrup*.py) hangs on the cable
// and stays on during the zero reading; then the weights (CAL_MASSES_G: 200, 500, 1000,
// 2000 g) are hung in the stirrup one after the other. The force at the cell is m g, no
// lever ratio. The factor is N at the cell per count; every torque is that force times the
// cell arm (settings::torqueArmM()), which therefore enters 1:1.
//
//   zero (stirrup only) -> loads in any order, each with its mass -> zero again (optional)
//   -> evaluate -> accept
//
// Every reading is the mean over CAL_AVG_MS at rest. Evaluation: least-squares line
// counts = a + b F over all readings, F = m g; factor 1/|b|. Reported: linearity as the
// largest residual of a load, in % of the cell's full scale and in % of that load;
// hysteresis (largest spread between readings of the same mass, zero included) and zero
// return (last zero minus first) in % FS. Accept stores factor and record
// (settings::CalRecord, ver 2) and appends a line to the calibration book
// /vmrr/calibration.md on the SD and in the flash (mirror, 2026-11-28: history without a card).
//
// Until 2026-11-28 the cell was calibrated over the calibration arm (lever ratio L_cal / L)
// and checked with a check weight in every run; both were dropped.
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
// Is the stored factor valid for the stand as it is? 1 yes, 0 conditions unknown or an old
// lever calibration, -1 no; text says why. Used by the run's checks.
int8_t validity(char* why, size_t n);
// For the state JSON: ,"calib":{...} or nothing while OFF.
size_t json(char* out, size_t n);
// Protocol line for the run report.
void describe(Print& o);

}  // namespace calib
