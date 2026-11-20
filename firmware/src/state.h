#pragma once
//
// Zustandsmaschine des Prüfstands.
//
//   IDLE ──t──> TARE ──> IDLE
//   IDLE ──b/m──> SPINUP ──> SETTLE ──> POINT ──┐
//                    ^                          │ nächstes Plateau
//                    └──────────────────────────┘
//                                 │ fertig
//                                 v
//                               COAST ──> IDLE
//
//   Aus jedem Zustand bei Fehler: FAULT (rastend, Quittierung nur im Stillstand).
//
// Gemessen wird ausschließlich im Zustand POINT, auf einem Geschwindigkeits-
// Plateau und über eine ganze Zahl von Trommelumdrehungen. Begründung:
// docs/09-messtheorie.md §1.7 und §3.5.
//
#include <Arduino.h>

namespace state {

enum Phase : uint8_t { IDLE = 0, TARE, SPINUP, SETTLE, POINT, COAST, FAULT };
enum RunMode : uint8_t { RUN_NONE = 0, RUN_BASELINE, RUN_MEASURE };

void begin();
void service();

Phase phase();
const char* name();
RunMode runMode();

// Läufe
void startBaseline();   // Laufrad ABGEHOBEN — Leerlaufkennlinie
void startMeasure();    // Laufrad aufgelegt — Messung
void startTare();       // nur im Stillstand
void stopRun();         // beenden und BREMSEN (20 A)
void coastRun();        // beenden und freigeben — Trommel läuft frei aus
void clearFault();      // nur im Stillstand

// Parameter aus der Bedienung
void setLoadN(float n);
float loadN();

// Fortschritt (für Anzeige)
uint8_t plateauIndex();
uint8_t revsInPoint();

const char* faultReason();

}  // namespace state
