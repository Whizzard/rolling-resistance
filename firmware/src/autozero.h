#pragma once
// Automatic zero point with the drum motor (2026-11-27, builder: "more automation, fewer
// manual steps"). Wheel lifted. Also used for load readings (calibration, check weight):
// start(false), result = mean with the load on.
//
// Method (builder 2026-11-27): the drum runs at AZ_SPIN_KMH (speedctl::crawl), first in
// the measuring direction, then against it. Once the speed has been steady for
// AZ_SPIN_STEADY_MS the duty is frozen and the cell is averaged over AZ_SPIN_REVS whole
// drum revolutions. Over whole revolutions the cogging ripple (and an unbalance) averages
// out; the drum friction (pillow-block bearings, air) the motor works against shows with
// opposite sign in the two directions, so the mean of both is the zero and half their
// difference the friction. Between the directions the controller brakes
// (speedctl::crawlBrake); below STANDSTILL_KMH the other direction starts at once.
// A reading whose first and last revolution differ by more than AZ_SPIN_DRIFT_KMH is
// repeated (the drum's inertia would sit in the cell). The check value is the spread of the single-revolution means within a direction. The
// test wheel's Hall counting = the wheel is on the drum: the procedure stops.
#include <Arduino.h>

namespace autozero {

enum St : uint8_t { OFF = 0, SPINUP, READ, BRAKE, DONE, FAILED };

bool  start(bool setTare);   // setTare: the result becomes the tare (else only measured)
void  stop();
void  service();
St    state();
uint8_t position();          // 0-based, the reading in progress (0 = +, 1 = -)
uint8_t done();              // finished readings
float mean(uint8_t i);       // raw counts per reading
float degTurned(uint8_t i);  // degrees averaged over in reading i (whole revolutions)
float resultCounts();        // mean of both directions, NAN until DONE
float rangeCounts();         // largest spread of the revolution means within one direction
float sigmaCounts();         // pooled single-value spread (includes the cogging ripple)
float plusCounts();          // mean in measuring direction
float minusCounts();         // ... against it; (plus - minus) / 2 = drum friction
const char* why();           // reason when FAILED
const char* phaseText();     // what happens now, for the UI

}  // namespace autozero
