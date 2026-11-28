#pragma once
//
// Values determined at the stand and therefore held at runtime.
//
// WHY NOT IN config.h: drum circumference, lever arm and calibration factor are
// measured AT THE SETUP, not at the desk. A calibration run that forces a recompile
// does not get done -- and a calibration run not done is the source of the invented
// numbers CLAUDE.md is written against.
//
// THE PROJECT RULE IS STILL KEPT: config.h provides the defaults, and after every
// calibration the UI shows the finished line to enter into config.h and
// docs/03-parameters.md. The NVS is the working memory, the repo remains the
// source of truth. Whoever swaps the board loses the NVS -- and notices, because the
// firmware then reports "not calibrated" again.
//
#include <Arduino.h>
#include "config.h"

namespace settings {

// Loads from NVS. If a value is missing, the default from config.h applies.
void begin();

float drumCircumferenceM();
float drumRadiusM();            // derived, U / 2π
float torqueArmM();             // lever arm of the load cell
float loadcellNPerCount();

// Which sign of the controller speed is the MEASURING DIRECTION. +1 or -1.
//
// It depends on the phase order and changes as soon as someone reconnects the
// motor cable or the brake relays. It therefore belongs here and not in
// config.h: learnable by turning by hand, see speedctl::learn().
int8_t directionSign();

// false as long as not calibrated. Then torque:: delivers no forces, only raw values.
bool calibrated();

// Setters. Return false and change nothing if the value is implausible --
// a mistyped drum circumference of 28.37 m should not silently apply.
bool setDrumCircumferenceM(float m);
bool setTorqueArmM(float m);
// epoch: local time in s (from the browser), 0 = unknown. For "calibrated ... ago" in the display.
bool setLoadcellNPerCount(float nPerCount, uint32_t epoch = 0);
uint32_t calibEpoch();
bool setDirectionSign(int8_t sign);   // only +1 or -1

// Measuring speeds: the plateaus that the no-load and measuring runs go through in
// order. Default PLATEAUS_KMH from config.h, maintained via the UI, in NVS.
// The list is ALWAYS sorted ascending and without duplicates; addPlateau inserts
// at the right place.
uint8_t plateauCount();
float   plateauKmh(uint8_t i);           // 0 outside the list
bool    addPlateau(float kmh);           // false: implausible, full or already present
bool    removePlateau(float kmh);        // false: not in the list

// Load on the wheel, default LOAD_DEFAULT_N, changeable at the stand (command l).
float loadN();
bool  setLoadN(float n);
// Lever scale (config.h LOAD_*): empty weight at the axle, sliding weight, wheel arm a.
// loadFromPosN computes the load from the position b of the sliding weight (commands Ae/Aw/Aa/Ap).
float loadEmptyKg();  bool setLoadEmptyKg(float kg);
float loadWeightKg(); bool setLoadWeightKg(float kg);
float loadArmM();     bool setLoadArmM(float m);
float loadFromPosN(float posM);   // (m_empty + m_weight * b / a) * g; NAN for a nonsensical position

// Loss models, both in NVS (config.h, docs/09-measurement-theory.md eq. 11/12):
//   no-load  M_empty(w)      = A0 + B0 w^2        [Nm], w drum in rad/s
//   wheel    M_wheel(w_wheel) = A1 + B1 w_wheel^2  [Nm at the wheel]
// NAN = not determined. Then nothing is corrected, and Crr is called "raw".
float baselineA0();  float baselineB0();  bool setBaseline(float a0, float b0);
float wheelA1();     float wheelB1();     bool setWheelModel(float a1, float b1);
bool  haveBaseline();  bool haveWheelModel();
// Age since the last set IN THIS SESSION, ms. UINT32_MAX if the value comes from
// NVS, i.e. was set before the start: then nobody knows its age,
// and the guided measuring run (run.h) asks whether it still applies.
uint32_t baselineAgeMs(); uint32_t wheelModelAgeMs(); uint32_t tareAgeMs();
// Stand revision (2026-11-26): free text on the mechanical build that the constants do not
// capture (drum surface, cell mounting, modifications), dated by its stamp. Goes into every
// protocol and raw log. Console B<text>, Setup. Survives reset().
const char* standRev(); bool setStandRev(const char* t);
// Wheel mass with tire [kg]; gives J_wheel = WHEEL_J_FACTOR m r_wheel^2. NAN = unknown.
float wheelMassKg(); bool setWheelMassKg(float kg);
// MEASURED J_wheel [kg m^2] from the bifilar pendulum (docs/09-measurement-theory.md §9.2).
// When set it takes precedence over the estimate via WHEEL_J_FACTOR, and the
// uncertainty of the wheel model drops from UNC_WHEEL_EST_REL to UNC_WHEEL_REL. NAN = not
// measured; setWheelJKgm2(0) clears it.
float wheelJKgm2(); bool setWheelJKgm2(float j);
uint32_t wheelJSetMs();   // millis() of the last setWheelJKgm2 with a value, 0 = not in this session
// Cell arm at the time of calibration [m], recorded with every factor. Since the direct
// calibration (2026-11-28) the factor is N at the cell and does not depend on the arm; the
// protocol still shows it. NAN = unknown.
float armAtCalM();
// Calibration record (2026-11-26): under which conditions the factor was determined, and
// how. ver 0 = no record (factor from before), 1 = lever calibration over the calibration
// arm (until 2026-11-28; held only for the same cell and calibration arm), 2 = direct at the
// cell (weights on the Bowden cable, holds for any cell arm). linPctLoad was appended on
// 2026-11-28; a record stored before is read without it.
// calib.h checks it before every run.
struct CalRecord {
  uint8_t  ver;            // 2 (direct); 1 = lever
  uint8_t  gainPulses;     // HX711_GAIN_PULSES at calibration
  uint16_t sps;            // HX711_RATE_SPS at calibration
  uint32_t epoch;          // local time, 0 = unknown
  float    npc, armM, lcalM, spsMeas;   // lcalM: calibration arm, ver 1 only (NAN in ver 2)
  float    linPctFS, hystPctFS, zeroPctFS;
  uint8_t  nLoads, nPoints;
  char     method[22];     // "direct"; ver 1: "procedure", "single point (k)"
  float    linPctLoad;     // largest residual of a load in % of that load (added 2026-11-28; NAN in older records)
};
const CalRecord& calRecord(); bool setCalRecord(const CalRecord& r);
bool npcPlausible(float x);   // the same range check setLoadcellNPerCount applies
// Last measured wheel circumference [m] (wheel.h), survives standstill and
// restart -- the assistant needs it before anything turns. NAN = never measured.
float wheelCircM(); bool setWheelCircM(float m);

// Header data of the last measuring run (run.h step 1), in NVS: tire, wheel, note,
// pressure, room temperature, load, position of the sliding weight. Saved while
// typing and preset on the next creation -- after an abort, restart or
// flashing nobody starts from scratch (2026-11-24).
struct RunMeta {
  char  tire[40];
  char  rim[32];     // was "wheel" until 2026-11-26 (rim, since then with a separate hub)
  char  note[64];
  char  etrto[16];   // e.g. "42-406"
  char  tube[32];    // e.g. "Vittoria Latex"
  char  hub[32];     // since 2026-11-26: the hub carries most of the wheel loss (bearings)
  float bar, tempC, loadKg;
  int   posMm;
};
const RunMeta& runMeta();
void setRunMeta(const RunMeta& m);

// Where the wheel model (A1, B1) comes from (since 2026-11-26): the run inputs of the
// run whose spin-down produced it -- tire, tube, rim/wheel, ETRTO, pressure -- because
// the model belongs to that set-up and is not one for all wheels. In NVS ("wm_org").
// known = false: model from before 2026-11-26 or from the console, origin unknown.
struct WheelOrigin {
  RunMeta  m;
  char     run[24];   // run id, "" = spin-down wizard under Setup
  uint32_t epoch;     // local time of the fit (wallclock), 0 = unknown
  bool     known;
};
const WheelOrigin& wheelOrigin();
void setWheelOrigin(const RunMeta& m, const char* run, uint32_t epoch = 0);   // epoch 0 = now

// Static tare of the load cell (raw value with empty arm). In NVS so that it survives
// a restart -- before, it was gone after every flash, and 241 000 counts
// were shown as 6.4 N (2026-11-23). 0 = never zeroed.
long tareOffset();
bool setTareOffset(long counts);

// When each value was last changed, as a JSON object {"<NVS key>": local time s}.
// If a key is missing, the default from config.h applies; 0 = set, time unknown
// (set without a browser clock, or before 2026-11-26). Keys: drum_u, arm, calib_arm,
// n_per_count, dir_sign, plateaus, tare, load_n, a0 (no-load model), a1 (wheel model),
// m_wheel, u_wheel, j_wheel, arm_at_cal, ctrl_kg, ld_empty, ld_weight, ld_arm.
size_t stampsJson(char* out, size_t n);

// Back to the defaults from config.h, NVS is cleared.
void reset();

// true if at least one value differs from its default.
bool anyOverridden();

}  // namespace settings

// --------------------------------------------------------- Conversions
// They live here and not in config.h because they depend on the drum circumference
// or the calibration factor -- both overridable at runtime.

inline float kmhFromRevPerSec(float rps) {
  return rps * settings::drumCircumferenceM() * MS_TO_KMH;
}
inline float revPerSecFromKmh(float kmh) {
  return kmh * KMH_TO_MS / settings::drumCircumferenceM();
}
inline bool isCalibrated() { return settings::calibrated(); }
