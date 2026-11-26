#pragma once
// Drum speed from the controller telemetry (ERPM / pole pairs, sign by measuring
// direction). The ONE place for it: revsrc (arbiter marks/controller) and
// speedctl (control) read here, neither reads the other -- until 2026-11-25 revsrc
// depended on speedctl and speedctl on revsrc.
//
// NAN without a connection to the controller: the last value would stay otherwise, and
// after a controller crash (docs/07-safety.md §4) the stand showed a spinning drum
// that had long stopped.
namespace drivespeed {
float rpm();   // mechanical, signed: positive in the measuring direction
float kmh();   // at the drum surface
}
