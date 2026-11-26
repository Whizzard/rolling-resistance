#pragma once
//
// Revolution source of the drum -- one interface, two sources.
//
// The state machine needs speed, revolutions and an angular position.
// There are two ways to get them, and they are NOT equivalent:
//
//   SRC_MARKS  the eight drum marks plus index mark (speed.cpp). The right
//              way: it measures the drum itself, knows the angular position and
//              notices when it stands.
//   SRC_VESC_INDEX  controller speed for v, but the REAL index mark for the
//              revolutions. Since 2026-11-24: a single Hall sensor on the
//              drum is enough for "10 revolutions" to be ten real ones and for the
//              synchronous averaging to really cancel the rotational orders
//              (docs/11-review.md K4). There are no angle bins.
//   SRC_VESC   the controller speed, ERPM / MOTOR_POLE_PAIRS. Fallback as long as
//              no sensor at all is wired on the drum. The revolutions are
//              INTEGRATED from it, the index mark is computed (up to one
//              revolution off), and there is no angular position.
//
// Which source applies is decided automatically: as soon as marks arrive, the
// marks apply; if only the index arrives, it applies for the revolutions. The source
// is in the JSON and in the log header -- whoever reads a measurement series later
// should not have to guess where the speed came from.
//
// angleReference() is the reason why this is a module of its own and not
// switching logic in state.cpp: without marks there are no angle bins, and a mark
// number computed from the controller speed would be an invented number.
//
#include <Arduino.h>

namespace revsrc {

enum Source : uint8_t { SRC_NONE = 0, SRC_MARKS, SRC_VESC, SRC_VESC_INDEX };

void begin();
void service();            // cyclic, non-blocking

Source     source();
const char* sourceName();  // "marks" | "controller+index" | "controller" | "none"
bool       angleReference();   // true ONLY with SRC_MARKS

float    revPerSec();      // drum
float    kmh();
float    omega();          // rad/s
float    dvdtKmhS();       // over DVDT_WINDOW_MS, NAN until the window is full
uint32_t revCount();       // whole revolutions since begin()

// Consumed by the state machine: true exactly once per revolution.
bool consumeIndexFlag();
// true if the revolutions come from a real sensor (marks or index).
bool realRevolutions();

// Number of the last seen mark within the revolution. Without marks
// MARKS_PER_REV, and that is the invalid value for torque::accumulate().
uint8_t markIndexInRev();

}  // namespace revsrc
