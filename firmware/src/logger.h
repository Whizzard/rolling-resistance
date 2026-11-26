#pragma once
//
// CSV logging to the serial interface.
// One line per measurement point (= whole number of drum revolutions), not per sample.
//
#include <Arduino.h>

namespace logger {

void begin();
void header();

// A complete measurement point.
void point(const char* stateName,
           uint32_t rev,
           float v_kmh,
           float raw,
           float f_N,
           float m_Nm,
           float p_W,
           float crr,
           float load_N,
           float bar,
           const char* note);

// Angle-resolved extra line for diagnosing the drum seam and out-of-roundness.
void angleBins();

void note(const char* text);
// Like note(), but marked as an error: refusals, aborts, FAULT. The user interface
// colours by this, no longer by the first word (until 2026-11-25 main.cpp guessed from
// the German words "nicht", "kein", "Abbruch" ...).
void fail(const char* text);
const char* lastNote();    // last text passed, for the user interface
bool lastNoteBad();        // true if it came via fail()

}  // namespace logger
