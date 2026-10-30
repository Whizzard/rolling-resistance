#pragma once
//
// Momentenmessung über die Pendelmaschine: HX711 + 10-kg-Wägezelle am Hebelarm.
//
// Kernformel (ohne Geschwindigkeit!):
//     F_roll = F · L / r
//     M      = F · L
//     P      = M · ω
//
#include <Arduino.h>

namespace torque {

void begin();

// Nicht blockierend. Holt einen Wert, wenn der HX711 bereit ist.
// Liefert true, wenn ein neuer Rohwert vorliegt.
bool update();

long  lastRaw();
bool  isSaturated();      // Rohwert am Anschlag → Zelle überlastet oder Kabelbruch
bool  isResponding();     // HX711 liefert überhaupt Werte

// Skalierte Größen. Alle liefern NAN, solange LOADCELL_N_PER_COUNT nicht
// kalibriert ist — absichtlich, damit nie eine erfundene Zahl im Log landet.
float newtons();
float momentNm();
float rollForceN();                 // Tangentialkraft an der Trommeloberfläche
float powerW(float omega);

// Statische Tara im Stillstand: mittelt TARE_SAMPLES Rohwerte.
// Blockiert bis zu ~3 s — nur im Zustand IDLE aufrufen.
void tareStatic();
long tareOffset();

// --- Umdrehungssynchrone Mittelung ------------------------------------------
// Jeder Rohwert wird mit der Markennummer in einen Winkel-Bin einsortiert.
// Ein Messpunkt ist immer der Mittelwert über ganze Umdrehungen.

void accumulate(uint8_t markIndex);  // nach jedem update() aufrufen
void resetAccumulator();
uint32_t sampleCount();
float    averageRaw();               // Mittelwert der Rohwerte seit reset
float    averageN();                 // dito in Newton (NAN ohne Kalibrierung)
float    binAverageRaw(uint8_t bin); // Winkel-Bin, für die Unrundheits-Diagnose
uint8_t  binCount();

}  // namespace torque
