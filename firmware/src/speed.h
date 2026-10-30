#pragma once
//
// Drehzahlerfassung der Trommel.
//
// Kernidee: die Geschwindigkeit wird IMMER aus dem Zeitfenster über eine ganze
// Umdrehung berechnet, also t[n] - t[n-MARKS_PER_REV]. Damit fallen Teilungsfehler
// der Marken exakt heraus — mehr Marken erfordern deshalb keine Teilungsgenauigkeit,
// liefern aber eine schnellere Aktualisierung.
//
#include <Arduino.h>

namespace speed {

void begin();

// Trommel
float revPerSec();        // 0, solange kein vollständiges Fenster vorliegt
float kmh();
float omega();            // rad/s
uint32_t markCount();     // seit begin()
uint32_t revCount();      // Indexmarken seit begin()

// Von der Zustandsmaschine konsumiert: true genau einmal pro Indexmarke.
bool consumeIndexFlag();

// Motor (für die Riemenschlupf-Überwachung)
float motorRevPerSec();

// true, wenn die Trommeldrehzahl aus Motordrehzahl/i um mehr als SLIP_TOLERANCE
// abweicht. Liefert false, solange eine der beiden Quellen noch kein Fenster hat.
bool slipDetected();

// true, wenn seit STALL_TIMEOUT_MS keine Marke mehr kam.
bool stalled();

// Nummer der zuletzt gesehenen Marke innerhalb der Umdrehung (0..MARKS_PER_REV-1),
// für die winkelaufgelöste Auswertung.
uint8_t markIndexInRev();

}  // namespace speed
