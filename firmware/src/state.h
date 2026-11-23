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
                       WIZ_SPINUP, WIZ_LIFT, HOLD };
enum RunMode : uint8_t { RUN_NONE = 0, RUN_BASELINE, RUN_MEASURE };

// SEQUENZEN (seit 2026-11-23, fuer den gefuehrten Messlauf in run.h): ein Lauf
// ist eine Liste von Geschwindigkeiten mit je pointsPer Messpunkten. Jeder
// fertige Punkt kommt mit seiner Streuung an onPoint; am Ende onDone(true). Bei
// coastAtEnd laeuft die Trommel danach aus (Befehle m0/m1), sonst bleibt sie auf
// der letzten Geschwindigkeit in HOLD, und der Aufrufer entscheidet.
// HOLD ist "faehrt mit Sollwert, aber nicht in einem Lauf": die Abschaltkriterien
// gelten, gemessen wird nicht. Warmlauf des Messlaufs.
struct PointStats {
  float    v;        // km/h, Mittel der Umdrehungen
  float    vSd;      // km/h, Streuung der Umdrehungsmittel
  float    raw;      // Counts, Mittel
  float    mNm;      // Nm, NAN ohne Kalibrierung
  float    sdNm;     // Nm, Streuung der Umdrehungsmittel (Typ A)
  float    dvdtMax;  // km/h/s, groesste Beschleunigung waehrend des Punkts
  float    imot, vin, tfet, tmot;
  uint8_t  revs, plateau, k;   // k: Punkt innerhalb des Plateaus, ab 0
  bool     ok;       // konstant gefahren, kein Reglerfehler
  uint32_t tMs;
};
typedef void (*PointFn)(const PointStats&);
typedef void (*SeqDoneFn)(bool completed);
bool startSequence(RunMode mode, const float* kmh, uint8_t n, uint8_t pointsPer,
                   bool coastAtEnd, PointFn onPoint, SeqDoneFn onDone);
bool startHold(float kmh);     // aus IDLE oder HOLD; false mit Grund in logger::lastNote()
void endHold();                // HOLD -> IDLE, Antrieb bleibt wie er ist

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
