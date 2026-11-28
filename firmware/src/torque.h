#pragma once
//
// Torque measurement via the cradle (dynamometer) machine: HX711 + 5 kg load cell on the lever arm.
//
// Core formula (without speed!):
//     F_roll = F · L / r
//     M      = F · L
//     P      = M · ω
//
#include <Arduino.h>

namespace torque {

void begin();

// Non-blocking. Fetches a value when the HX711 is ready.
// Returns true when a new raw value is available.
bool update();

long  lastRaw();
uint32_t readsTotal();    // HX711 values read since boot (rate check, console i)

// Moving average over the last DISPLAY_AVG_N raw values, plus peak-to-peak.
// ONLY for the DISPLAY -- a number jittering at 80 Hz is not readable
// (docs/10-firmware.md W5). The MEASUREMENT averages independently of this,
// revolution-synchronously, via accumulate()/averageRaw(). Filled in update(), i.e.
// exactly when the HX711 has really delivered a new value -- not by the clock.
void displayStats(long& mean, long& ripple);
// The last up to TRACE_N raw values, oldest first, for the live curve (run.cpp, step 4).
uint16_t trace(long* out, uint16_t max);
bool  isSaturated();      // raw value at the limit → cell overloaded or broken cable
bool  isResponding();     // HX711 delivers values at all

// Scaled quantities. All return NAN as long as LOADCELL_N_PER_COUNT is not
// calibrated -- on purpose, so that an invented number never ends up in the log.
float newtons();
float momentNm();
float rollForceN();                 // tangential force at the drum surface
float powerW(float omega);

// Static tare at standstill: averages TARE_SAMPLES raw values.
//
// NON-BLOCKING. tareBegin() returns at once, collection happens in update(),
// and tareBusy() says when it is over. The earlier version held up the loop for
// up to 3 s -- that stopped the web server too, the UI saw a dropout,
// and it violated docs/10-firmware.md G1 (no delay() in the main path).
void     tareBegin();
bool     tareBusy();
uint16_t tareCount();      // 0..TARE_SAMPLES, for the progress
float    tareSigma();      // standard deviation of the raw values of the last tare, NAN before
long     tareOffset();

// --- Revolution-synchronous averaging ---------------------------------------
// Every raw value is sorted into an angle bin by the mark number.
// A measurement point is always the mean over whole revolutions.

void accumulate(uint8_t markIndex);  // call after every update()
void resetAccumulator();
uint32_t sampleCount();
double   sumRaw();                   // sum of the raw values since reset (for partial means per revolution)
float    averageRaw();               // mean of the raw values since reset
float    averageN();                 // same in newtons (NAN without calibration)
long     rippleRaw();                // peak-to-peak of the raw values since reset (ripple within the point)
float    binAverageRaw(uint8_t bin); // angle bin, for the out-of-round diagnostics
uint8_t  binCount();

// --- Sample at standstill ----------------------------------------------------
// For the zero checks (run.cpp) and the calibration readings (calib.cpp): sampleBegin(), wait, then
// sampleMean()/sampleSigma() over ALL values read since -- not the 40-value
// display ring, which covers only 0.5 s (docs/11-review.md D5).
// Independent of the revolution accumulator, which is only filled in state POINT.
void     sampleBegin();
void     sampleStop();
uint32_t sampleN();
float    sampleMean();               // raw value, NAN without values
float    sampleSigma();              // raw value per single value, NAN below 2 values

}  // namespace torque
