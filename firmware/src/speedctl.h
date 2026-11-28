#pragma once
//
// Speed control of the drum via the duty cycle of the motor controller.
//
// WHY NOT THE VESC SPEED CONTROLLER: it is tuned uselessly for this motor
// (setpoint 2400 ERPM → 780 ERPM reached), and `s_pid_min_erpm` = 900 blocks the
// lowest test step, because 10 km/h corresponds to only 881 ERPM.
// docs/06-open-questions.md A15.
//
// INSTEAD: feed-forward from the no-load characteristic measured on 2026-11-17
// (MOTOR_NO_LOAD_RPM_PER_V per bus volt, strictly linear over ten points), with a
// slow trim on top. The stand has been driven up to 99.2 km/h this way.
//
// THREE RULES, EACH LEARNED THE HARD WAY ON 2026-11-17:
//
//   1. On starting, the output is preset from the MEASURED speed.
//      Starting at zero while the drum turns is a brake command -- the bus
//      reached 53.3 V within half a second.
//   2. Reducing the output is allowed more slowly than raising it, for the same reason.
//   3. The output is clamped against the no-load characteristic of the setpoint,
//      otherwise the trim runs to the stop while the current limit determines the
//      acceleration -- and the drum overshoots as soon as the current is free.
//
#include <Arduino.h>

namespace speedctl {

void begin();

// Call cyclically, non-blocking. Computes at most every SPEEDCTL_PERIOD_MS.
void service();

// Target speed at the drum surface. 0 releases the drive
// (drum coasts). Limited to V_MAX_KMH -- the UI shows steps above it as
// locked, see docs/07-safety.md §5.
//
// Returns false if the request was refused: above V_MAX_KMH, or the drum is
// still noticeably turning in the opposite direction (then wait for standstill first).
bool setTargetKmh(float kmh);

float targetKmh();     // what is requested
float rampKmh();       // what the controller is aiming at right now, ramped
float kmh();           // actual value, from the controller speed
float duty();          // output being sent
bool  running();
bool  slowingDown();   // descent coasting, or a STOP is braking

// Fastest possible stop: via the relay brake (brake.h), at any speed
// (decision 2026-11-25 after the braking comparison, config.h RELAY BRAKE). Only without
// RELAY_BRAKE_WIRED does the controller brake with VESC_BRAKE_CURRENT_STOP below
// BRAKE_MAX_KMH; at high speed it regenerates, the power supply trips and the drum
// coasts -- the fault case is at the same time the fallback (docs/07-safety.md §3.2c).
void stop();
// Output applied, ramp running, drum standing (config.h STANDSTILL_KMH).
bool stalledUnderDuty();

// Release the drive, drum coasts. Always possible, even without the battery.
void coast();

// Reason of the last refusal, for the UI. "" if all went well.
const char* lastRefusal();

// EMERGENCY STOP. Two ways, both detection only -- the hardware does the disconnecting:
//   motorOpen()  finding: output above STALL_DUTY_MIN, motor current below
//                ESTOP_IMOT_MAX_A AND speed below ESTOP_SPEED_RATIO times the
//                no-load speed for the output, longer than ESTOP_DETECT_MS. A
//                connected motor cannot lag behind its voltage without current;
//                after the emergency stop the drum coasts and the gap grows
//                (reasoning in config.h).
//                Then the drive is released, the finding stays until a new
//                setpoint comes. Without a motor on the controller an applied
//                output is pointless -- and when the emergency stop is turned back,
//                the motor should not start with full output.
//   estopInput() the auxiliary contact on PIN_ESTOP_FB, only if ESTOP_FB_WIRED.
bool motorOpen();              // latching until standstill or new setpoint
bool estopInput();

// Learn the direction of rotation by hand.
//
// Which sign of the controller speed is the MEASURING DIRECTION depends on the
// phase order and changes as soon as someone reconnects the motor cable or the
// brake relays. Instead of fixing it in config.h it is measured:
// release the drive, turn the drum by hand in the measuring direction, sign into
// NVS.
//
// This does NOT replace the controller's Hall detection. The commutation -- which
// Hall state belongs to which angle -- can only be measured by the controller,
// because current has to be injected for it. This is only about the sign
// convention on top of it.
void learn();
bool learning();

// Breakaway measurement: raise the duty cycle slowly until the drum moves,
// then release at once. Yields the number missing in docs/03-parameters.md --
// and answers whether the clamp needs any reserve at start-up at all.
void breakaway();
bool breakingAway();

// CRAWL (2026-11-27): turn the drum at a moderate constant speed in either direction
// (dir -1 = against the measuring direction) for zero and load readings with the cogging
// ripple and the bearing friction averaged out (autozero.h). From standstill only. The duty
// ramps at CRAWL_RAMP_PER_S towards feed-forward plus a slow trim; crawlFreeze(true) holds
// it constant while reading. Ends by itself with crawlWhy() set on: wrong direction, faster
// than CRAWL_MAX_FACTOR x target, duty above CRAWL_DUTY_MAX, controller lost. coast() and
// stop() end it (stop() with the relay brake).
bool  crawl(int8_t dir, float kmh);
void  crawlFreeze(bool frozen);
// End the crawl by braking with the controller (VESC_BRAKE_CURRENT_STOP; the crawl is far
// below BRAKE_MAX_KMH). The brake stays on until crawl(), coast() or stop().
void  crawlBrake();
bool  crawling();
float crawlSpeed();         // km/h, positive in the crawl direction
const char* crawlWhy();
float breakawayDuty();      // duty cycle at breakaway, 0 = none yet
float breakawayAmps();      // motor current at that moment

// DETECT THE DRIVE DIRECTION (2026-11-26, builder's idea): the same slow start as the
// breakaway measurement, with the load cell watched. Driving in measuring direction the
// cell must count POSITIVE; if it goes negative, the sign is reversed and stored. Below
// DIRDETECT_MIN_N change at the cell nothing is changed (inconclusive). Needs a
// calibrated cell (the threshold is in N) and a zeroed one at standstill.
bool  detectDirection(const char** why);
bool  detectingDirection();
int   directionResult();    // 1 kept, -1 reversed, 0 inconclusive, -2 not run yet
float directionDeltaN();    // change at the cell, N (with the sign used while driving)

}  // namespace speedctl
