#pragma once
//
// Umdrehungsquelle der Trommel — eine Schnittstelle, zwei Quellen.
//
// Die Zustandsmaschine braucht Geschwindigkeit, Umdrehungen und eine Winkellage.
// Dafür gibt es zwei Wege, und sie sind NICHT gleichwertig:
//
//   SRC_MARKS  die acht Trommelmarken plus Indexmarke (speed.cpp). Der richtige
//              Weg: er misst die Trommel selbst, kennt die Winkellage und merkt,
//              wenn sie steht.
//   SRC_VESC_INDEX  Reglerdrehzahl fuer v, aber die ECHTE Indexmarke fuer die
//              Umdrehungen. Seit 2026-11-24: ein einzelner Hall-Geber an der
//              Trommel reicht, damit "10 Umdrehungen" zehn echte sind und die
//              synchrone Mittelung die Drehordnungen wirklich loescht
//              (docs/11-review.md K4). Winkel-Bins gibt es nicht.
//   SRC_VESC   die Reglerdrehzahl, ERPM / MOTOR_POLE_PAIRS. Rückfall, solange gar
//              kein Geber an der Trommel verdrahtet ist. Die Umdrehungen werden
//              daraus INTEGRIERT, die Indexmarke ist gerechnet (bis zu eine
//              Umdrehung daneben), und eine Winkellage gibt es nicht.
//
// Welche Quelle gilt, entscheidet sich von selbst: sobald Marken kommen, gelten
// die Marken; kommt nur der Index, gilt er fuer die Umdrehungen. Die Quelle steht
// im JSON und im Protokollkopf — wer später eine Messreihe liest, soll nicht raten
// müssen, woher die Drehzahl kam.
//
// angleReference() ist der Grund, warum das hier ein eigenes Modul ist und keine
// Schalterei in state.cpp: ohne Marken gibt es keine Winkel-Bins, und eine aus der
// Reglerdrehzahl gerechnete Markennummer wäre eine erfundene Zahl.
//
#include <Arduino.h>

namespace revsrc {

enum Source : uint8_t { SRC_NONE = 0, SRC_MARKS, SRC_VESC, SRC_VESC_INDEX };

void begin();
void service();            // zyklisch, nicht blockierend

Source     source();
const char* sourceName();  // "marken" | "regler+index" | "regler" | "keine"
bool       angleReference();   // true NUR bei SRC_MARKS

float    revPerSec();      // Trommel
float    kmh();
float    omega();          // rad/s
float    dvdtKmhS();       // über DVDT_WINDOW_MS, NAN bis das Fenster voll ist
uint32_t revCount();       // ganze Umdrehungen seit begin()

// Von der Zustandsmaschine konsumiert: true genau einmal je Umdrehung.
bool consumeIndexFlag();
// true, wenn die Umdrehungen von einem echten Geber kommen (Marken oder Index).
bool realRevolutions();

// Nummer der zuletzt gesehenen Marke innerhalb der Umdrehung. Ohne Marken
// MARKS_PER_REV, und das ist für torque::accumulate() der ungültige Wert.
uint8_t markIndexInRev();

}  // namespace revsrc
