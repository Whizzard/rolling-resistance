#pragma once
//
// CSV-Protokollierung auf die serielle Schnittstelle.
// Eine Zeile pro Messpunkt (= ganze Zahl von Trommelumdrehungen), nicht pro Sample.
//
#include <Arduino.h>

namespace logger {

void begin();
void header();

// Ein vollständiger Messpunkt.
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

// Winkelaufgelöste Zusatzzeile zur Diagnose von Trommelnaht und Unrundheit.
void angleBins();

void note(const char* text);
const char* lastNote();    // zuletzt uebergebener Text, fuer die Oberflaeche

}  // namespace logger
