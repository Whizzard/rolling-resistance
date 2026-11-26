#pragma once
//
// State machine of the test stand.
//
//   IDLE ──t──> TARE ──> IDLE
//   IDLE ──b/m──> SPINUP ──> SETTLE ──> POINT ──┐
//                    ^                          │ next plateau
//                    └──────────────────────────┘
//                                 │ done
//                                 v
//                               COAST ──> IDLE
//
//   From any state on a fault: FAULT (latching, acknowledgement only at standstill).
//
// Measurement happens exclusively in state POINT, on a speed plateau and over a
// whole number of drum revolutions. Rationale:
// docs/09-measurement-theory.md §1.7 and §3.5.
//
#include <Arduino.h>

namespace state {

enum Phase : uint8_t { IDLE = 0, TARE, SPINUP, SETTLE, POINT, COAST, FAULT,
                       WIZ_SPINUP, WIZ_LIFT, HOLD };
enum RunMode : uint8_t { RUN_NONE = 0, RUN_BASELINE, RUN_MEASURE };

// SEQUENCES (since 2026-11-23, for the guided measurement run in run.h): a run is
// a list of speeds with pointsPer measurement points each. Every finished point
// goes with its spread to onPoint; at the end onDone(true). With coastAtEnd the
// drum then coasts down (commands m0/m1), otherwise it stays at the last speed in
// HOLD, and the caller decides.
// HOLD is "driving with a setpoint, but not in a run": the shutdown criteria apply,
// nothing is measured. Warm-up of the measurement run.
struct PointStats {
  float    v;        // km/h, mean of the revolutions
  float    vSd;      // km/h, spread of the revolution means
  float    raw;      // counts, mean
  float    mNm;      // Nm, CORRECTED for inertia: mRawNm - mInertNm. NAN without calibration
  float    mRawNm;   // Nm, as measured
  float    mInertNm; // Nm, J_eff * net dw/dt over the point (docs/12 K1)
  float    dvdtNet;  // km/h/s, slope of the regression line v(t) over the point
  float    sdNm;     // Nm, spread of the revolution means (type A)
  float    rippleNm; // Nm, peak-to-peak of the single values in the point (ripple)
  float    uSyncNm;  // Nm, residual error of the averaging without a real index: ripple/2/revs, otherwise 0
  float    dvdtMax;  // km/h/s, largest acceleration during the point (diagnostics)
  float    imot, vin, tfet, tmot;
  uint8_t  revs, plateau, k;   // k: point within the plateau, from 0
  bool     ok;       // |dvdtNet| < POINT_DVDT_NET_MAX_KMH_S, at target, no controller fault
  bool     realRevs; // revolutions from a real sensor
  uint32_t tMs;
};
typedef void (*PointFn)(const PointStats&);
typedef void (*SeqDoneFn)(bool completed);
bool startSequence(RunMode mode, const float* kmh, uint8_t n, uint8_t pointsPer,
                   bool coastAtEnd, PointFn onPoint, SeqDoneFn onDone);
bool startHold(float kmh);     // from IDLE or HOLD; false with the reason in logger::lastNote()
void endHold();                // HOLD -> IDLE, the drive stays as it is

// WIZARD spin-down + idle loss (config.h WIZ_*): startWizard() runs the drum up to
// WIZ_KMH with the wheel resting (WIZ_SPINUP), then waits for the lift (WIZ_LIFT):
// if the wheel turns slower than it would via the drum, it is lifted. Then the wheel
// spin-down (wheel::spindownBegin) AND the idle-loss run over all plateaus begin --
// the drum is in exactly the state that run needs. Both models are stored at the
// end. wizardStep() for the user interface: 0 off, 1 spin-up, 2 lift wheel,
// 3 spin-down and idle loss running, 4 done.
// With callbacks (guided measurement run, run.cpp step 6): the idle-loss points go
// to onPoint, the end to onDone, and the drum then stays in HOLD instead of coasting
// down -- the caller brakes. Without callbacks as before (Setup).
void startWizard(PointFn onPoint = nullptr, SeqDoneFn onDone = nullptr);
uint8_t wizardStep();
void wizardAck();      // acknowledge step 4

void begin();
void service();

Phase phase();
const char* name();
RunMode runMode();

// Runs
void startBaseline();   // wheel LIFTED — idle-loss curve
void startMeasure();    // wheel resting — measurement
void startTare();       // only at standstill
void stopRun();         // end and BRAKE (relay; without RELAY_BRAKE_WIRED controller 20 A)
void coastRun();        // end and release — drum coasts down freely
void clearFault();      // only at standstill

// Parameters from the operator
void setLoadN(float n);
float loadN();

// Progress (for display)
uint8_t plateauIndex();
uint8_t pointIndex();          // point within the plateau, from 0
uint8_t seqLength();           // speeds of the running sequence
uint8_t pointsPerPlateau();
uint32_t settleElapsedMs();    // in SETTLE: since the start of settling
uint8_t revsInPoint();

const char* faultReason();

}  // namespace state
