#pragma once
//
// Laufradumfang aus dem Drehzahlverhältnis — integriert, nicht momentan.
//
// Am Latsch sind die Umfangsgeschwindigkeiten gleich, also
//     U_Rad = U_Trommel · (Trommelumdrehungen / Radumdrehungen)
// über DASSELBE Zeitfenster. Die Radumdrehungen sind exakt: der Geber liefert
// WHEEL_MARKS_PER_REV Impulse je Umdrehung, und das Fenster beginnt und endet
// auf einem Impuls. Die Trommelumdrehungen werden über die Zeit aufintegriert
// (revsrc::revPerSec, aus Regler oder Marken). Je länger das Fenster, desto
// kleiner der Fehler des Integrals am Rand — deshalb wächst es, solange die
// Fahrt stetig ist, und wird nur neu begonnen, wenn sich die Geschwindigkeit
// um mehr als WHEEL_CIRC_STEADY ändert oder das Rad steht.
//
// Angezeigt wird erst ab WHEEL_CIRC_MIN_REVS Radumdrehungen. Ein Momentanwert
// aus zwei Drehzahlen flatterte mit jedem Impuls — das war der Stand vom
// 2026-11-22 und ist am 2026-11-23 ersetzt worden.
//
// Das Ergebnis ist der WIRKSAME Abrollumfang unter Last, nicht das Nennmaß.
//
#include <Arduino.h>

namespace wheel {

void begin();
void service();           // zyklisch aus der Hauptschleife

bool     valid();         // genug Umdrehungen auf stetiger Fahrt
float    circumferenceM();// NAN, solange nicht valid()
uint32_t revsUsed();      // Radumdrehungen im laufenden Fenster

}  // namespace wheel
