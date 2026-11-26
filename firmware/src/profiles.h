#pragma once
//
// WHEEL PROFILES (since 2026-11-26): lists of tires, tubes, rims and hubs, and the
// combinations of them that have been measured. A profile is one combination
// tire + tube + rim + hub and carries what belongs to exactly that wheel: the wheel
// model (A1, B1) with its origin, the wheel circumference, the mass and the measured
// J_wheel. The wheel model depends on all four parts, so it is kept per profile and
// never transferred to another combination.
//
// SINCE 2026-11-26 (second revision) everything about the wheel is entered in Setup,
// beforehand: the four parts (dropdowns of the lists, "New..." for a new entry), the
// mass, the bifilar pendulum readings (J_wheel), and for the hub spokes, axle, brake,
// drive wheel and colour. Step 1 of a run only chooses a COMPLETE profile (missing() == "");
// Setup has a "Measure" button per profile that starts the run with it. Setup also
// renames and deletes list entries (refused while a profile uses them).
//
// STORAGE: one text file /profiles.tsv in the flash (LittleFS) -- settings, written
// rarely and only at standstill (storage::flashWritable()); a change while the drum
// turns is written as soon as it stands. No fixed number of entries: the limit is
// the flash (a few hundred bytes per profile).
//
// SD COPY (2026-11-26): the four lists are also kept as Markdown tables in
// /vmrr/wheels.md on the SD card, editable on any computer; edits are taken over when the
// card is mounted (CRC stamp at the end of the file tells an edited file from one the
// stand wrote). Profiles stay in the flash only. Details in profiles.cpp.
//
// Format, one record per line, fields separated by TAB:
//   T <name> <etrto> <epoch>        tire
//   U <name> <maker> <model> <material> <epoch>                    tube
//   R <name> <maker> <model> <inner_width_mm> <bsd_mm> <epoch>     rim
// (U/R lines with only <name> <epoch> are read too: before the details, 2026-11-26)
//   H <name> <spokes> <axle> <brake> <drive> <colour> <epoch>    hub
//   P <tire> <tube> <rim> <hub> <a1> <b1> <u_wheel> <m_wheel> <j_wheel> <bar> <run> <epoch>
//     <bif_d_mm> <bif_l_mm> <bif_n> <bif_t_s>     (bifilar readings, J computed from them)
// First line "RRP2" (RRP1 of the first revision is read too).
// Numbers empty = unknown. epoch = local time (wallclock) of the last change.
//
// Thread safety: called from the main loop (run.cpp) and the web task (webui.cpp);
// a mutex guards everything.
#include <Arduino.h>

namespace profiles {

enum List : uint8_t { TIRE = 0, TUBE, RIM, HUB, LIST_N };

// Hub attributes (choices as in the UI; "" = not entered).
struct Hub {
  int      spokes = 0;                    // number of spokes, 0 = not entered
  String   axle;                          // "12 mm thru-axle" | "15 mm thru-axle" | "19 mm thru-axle" | "quick release" | "nutted axle"
  String   brake;                         // "none" | "drum" | "drum with cooling fins" | "disc"
  int8_t   drive = -1;                    // drive wheel (freewheel/cassette): 1 yes, 0 no, -1 not entered
  String   colour;                        // "silver" | "black" | free text
};

// Tube and rim details (since 2026-11-26). Kept as text, numbers as text in mm.
//   tube: a[0] maker, a[1] model, a[2] material "latex" | "butyl" | "TPU" | "tubeless"
//   rim:  a[0] maker, a[1] model, a[2] inner width mm, a[3] bead seat diameter mm (ETRTO, e.g. 622)
constexpr uint8_t ATTR_N = 4;
struct Attr { String a[ATTR_N]; };
bool setAttr(List l, const char* name, const Attr& at, const char** why);   // TUBE or RIM; creates the entry
bool attr(List l, const char* name, Attr& out);

struct Profile {
  String   tire, tube, rim, hub;
  float    a1 = NAN, b1 = NAN;            // wheel model, Nm and Nm/(rad/s)^2
  float    uWheel = NAN, mWheel = NAN, jWheel = NAN;   // m, kg, kg m^2 (J only if measured)
  float    bar = NAN;                     // pressure at the spin-down of the model
  String   run;                           // run id of the model, "" = spin-down wizard
  uint32_t epoch = 0;                     // when the model was measured (wallclock), 0 = unknown
  float    bifD = NAN, bifL = NAN, bifN = NAN, bifT = NAN;   // bifilar: spacing mm, length mm, n, time s
  bool hasModel() const { return !isnan(a1) && !isnan(b1); }
};

// What a profile still lacks before a run may use it, as text ("" = complete):
// tire, ETRTO, tube and its material, rim with inner width and bead seat diameter (which
// must match the tire's ETRTO), hub, mass, and the hub's spokes, axle, brake and drive wheel.
// The bifilar pendulum and the colour are optional (without J it is estimated, W-note).
String missing(int i);

void begin();      // after storage::begin()
// Add or change one list entry from a Markdown table row as in wheels.md, e.g.
// tag 'T', "| Schwalbe One | 28-406 |" (console P<tag><row>). false + why on bad input.
bool addRow(char tag, const char* row, const char** why);
void service();    // main loop: writes pending changes once the flash may be written

// Index of the profile for exactly this combination, -1 if none. Names compare without
// case and surrounding spaces.
int  find(const char* tire, const char* tube, const char* rim, const char* hub);
// Copy of profile i; false if i is out of range.
bool get(int i, Profile& out);
// Create (i = -1) or change a profile from Setup. New names are added to the lists. A
// changed combination drops the wheel model (it belonged to the old one); a combination
// that another profile already has is refused. Returns the index, -1 + why on refusal.
int  save(int i, const char* tire, const char* etrto, const char* tube, const char* rim, const char* hub,
          float massKg, float bifD, float bifL, float bifN, float bifT, float jWheel, const char** why);
// Hub attributes (Setup); creates the hub entry if new.
bool setHub(const char* name, const Hub& h, const char** why);
bool hub(const char* name, Hub& out);
String etrto(const char* tire);            // ETRTO of a tire entry, "" if none
// Wheel data of profile i (after the spin-down in step 6, or from step 1).
void setModel(int i, float a1, float b1, float uWheel, float bar, const char* run, uint32_t epoch);
void setMass(int i, float kg);
void setJ(int i, float kgm2);             // NAN clears

// Setup edits. false + reason in why.
bool rename(List l, const char* from, const char* to, const char** why);
bool remove(List l, const char* name, const char** why);
bool setEtrto(const char* tire, const char* etrto, const char** why);
bool removeProfile(int i, const char** why);
bool clearModel(int i, const char** why);

// Everything as JSON: {"tire":[{"n":..,"e":..}],"tube":[..],"rim":[..],"hub":[..],
// "prof":[{"t":..,"s":..,"r":..,"h":..,"a1":..,"b1":..,"u":..,"m":..,"j":..,"bar":..,"run":..,"ep":..}],"pend":bool}
void json(String& out);

}  // namespace profiles
