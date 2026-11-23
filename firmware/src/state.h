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

enum Phase : uint8_t { IDLE = 0, TARE, SPINUP, SETTLE, POINT, COAST, FAULT,
                       WIZ_SPINUP, WIZ_LIFT };

// ASSISTENT Auslauf + Leerlauf (config.h WIZ_*): startWizard() faehrt die Trommel
// mit aufliegendem Rad auf WIZ_KMH (WIZ_SPINUP), wartet dann auf das Abheben
// (WIZ_LIFT): dreht das Rad langsamer, als es ueber die Trommel muesste, ist es
// abgehoben. Dann beginnt der Laufrad-Auslauf (wheel::spindownBegin) UND der
// Leerlauflauf ueber alle Plateaus -- die Trommel ist in genau dem Zustand, den
// er braucht. Beide Modelle werden am Ende gespeichert. wizardStep() fuer die
// Oberflaeche: 0 aus, 1 hochfahren, 2 Rad abheben, 3 Auslauf und Leerlauf laufen,
// 4 fertig.
void startWizard();
uint8_t wizardStep();
void wizardAck();      // Schritt 4 quittieren
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
