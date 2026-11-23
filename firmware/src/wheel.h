#pragma once
//
// Laufradumfang aus dem Drehzahlverhältnis — integriert, nicht momentan.
//
// Am Latsch sind die Umfangsgeschwindigkeiten gleich, also
//     U_Rad = U_Trommel · (Trommelumdrehungen / Radumdrehungen)
// über DASSELBE Zeitfenster. Die Radumdrehungen sind exakt: der Geber liefert
// WHEEL_MARKS_PER_REV Impulse je Umdrehung, und das Fenster beginnt und endet
// auf einem Impuls. Die Trommelumdrehungen werden über die Zeit aufintegriert
// (revsrc::revPerSec, aus Regler oder Marken).
//
// GLEITENDES FENSTER der letzten WHEEL_CIRC_WINDOW_REVS Radumdrehungen, bei jedem
// Impuls neu gerechnet: fortlaufend, aber geglättet. Ein Momentanwert aus zwei
// Drehzahlen flatterte mit jedem Impuls (Stand 2026-11-22); ein wachsendes
// Fenster mit Stetigkeitsbedingung wurde nie voll, weil die Regelung auf dem
// Plateau pendelt. Seit 2026-11-23 so. Beim Stillstand des Rads verfällt es.
//
// Das Ergebnis ist der WIRKSAME Abrollumfang unter Last, nicht das Nennmaß.
//
#include <Arduino.h>

namespace wheel {

void begin();
void service();           // zyklisch aus der Hauptschleife

bool     valid();         // genug Umdrehungen auf stetiger Fahrt
float    circumferenceM();// NAN, solange nicht valid()
uint32_t revsUsed();      // Radumdrehungen im Fenster, hoechstens WHEEL_CIRC_WINDOW_REVS

}  // namespace wheel
