#pragma once
//
// Speed acquisition of the drum.
//
// Core idea: the speed is ALWAYS computed from the time window over one whole
// revolution, i.e. t[n] - t[n-MARKS_PER_REV]. Spacing errors of the marks thus
// cancel exactly -- more marks therefore need no spacing accuracy, but deliver
// faster updates.
//
#include <Arduino.h>

namespace speed {

void begin();
// Cyclically from the main loop (revsrc::service): evaluates the timestamps of the
// drum marks -- revolutions, mark number, pattern of the intermediate magnet.
void service();

// Drum
float revPerSec();        // 0 as long as no complete window is available
float kmh();
float omega();            // rad/s
uint32_t markCount();     // since begin()
uint32_t revCount();      // index marks since begin()

// Consumed by the state machine: true exactly once per index mark.
bool consumeIndexFlag();

// Motor (for belt slip monitoring)
float motorRevPerSec();

// Test wheel, WHEEL_MARKS_PER_REV magnets on the spokes. Same method
// as on the drum: window over one whole revolution. 0 as long as no
// complete window is available or the wheel is standing.
float wheelRevPerSec();
// Pulse counter of the wheel and timestamp of the last pulse, as a pair.
void wheelPulses(uint32_t& count, uint32_t& lastUs);

// true if the drum speed deviates from motor speed/i by more than SLIP_TOLERANCE.
// Returns false as long as one of the two sources has no window yet.
bool slipDetected();

// true if no mark has come for STALL_TIMEOUT_MS.
bool stalled();

// Number of the last seen mark within the revolution (0..MARKS_PER_REV-1),
// for the angle-resolved evaluation.
uint8_t markIndexInRev();

// WHERE THE REVOLUTION COMES FROM (docs/04-hardware.md §8.6):
//   REV_COUNT       the 8 marks only: every 8th edge, mark 0 is the first one seen
//   REV_PATTERN     intermediate magnet in a 45° gap detected (two short intervals):
//                   fixed zero, the count re-locks every revolution
//   REV_INDEX_HALL  dedicated index Hall on PIN_INDEX
// The pattern is detected automatically (MARK_PATTERN_INDEX); without the intermediate
// magnet it stays at REV_COUNT.
enum RevSource : uint8_t { REV_COUNT = 0, REV_PATTERN, REV_INDEX_HALL };
RevSource revSource();
uint8_t   pulsesPerRev();     // 8, or 9 with a detected intermediate magnet
uint32_t  patternResyncs();   // pattern came at an unexpected position: count reset
uint32_t  missedPulses();     // too long intervals (swallowed edge) or pattern missing
uint32_t  glitchPulses();     // double-short in the middle of the revolution: glitch

// Wiring test (docs/10-firmware.md D4/D5): is every channel alive? Per sensor a
// separate pulse counter that does not touch the measurement counters and may
// therefore be reset at any time, plus the time since the last pulse and the pin level.
// Turn the drum or wheel by hand, the counters must follow.
enum Channel : uint8_t { CH_MARK = 0, CH_INDEX, CH_WHEEL, CH_MOTOR, CH_N };
struct ChannelState {
  uint32_t count;     // pulses since begin() or resetCounters()
  uint32_t sinceMs;   // since the last pulse; 0xFFFFFFFF if none came yet
  bool     low;       // pin level: low = magnet in front of the sensor
};
const char*  channelName(Channel ch);   // "Marks" | "Index" | "Wheel" | "Motor"
ChannelState channel(Channel ch);
uint32_t markEchoes();   // rejected double pulses at the drum marks (config.h MARK_ECHO_PCT)
void patternDiag(float& refUs, uint8_t& hits, uint16_t& since, uint32_t& seen, uint32_t& resets);
// Last n mark intervals in degrees, the last perRev of them = 360 (diagnostics). Number of values.
uint8_t recentIntervalsDeg(float* out, uint8_t n, uint8_t perRev);
void resetCounters();

// Raw data (rawlog.h): fetch edges from sequence number `from`, at most max. Returns the
// sequence number after the last one delivered; lost = overwritten in between (ring
// RAWLOG_EDGE_RING too small or main loop away too long). Channel as Channel,
// bit 7 = rejected as echo (MARK_ECHO_PCT). Time: micros().
uint32_t rawEdges(uint32_t from, uint32_t* us, uint8_t* ch, uint32_t max, uint32_t* lost);

}  // namespace speed
