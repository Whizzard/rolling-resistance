#include "speed.h"
#include "config.h"
#include "settings.h"

namespace speed {
namespace {

// ---------------------------------------------------------------- Drum
// The ISR ONLY stores timestamps (CLAUDE.md: ISRs short). Everything that turns them
// into a revolution, a mark number or the pattern of the intermediate magnet is
// computed by service() in the main loop from the ring.
constexpr uint8_t BUF = 16;   // >= pulses per revolution (8 or 9) + 1

volatile uint32_t g_markUs[BUF] = {0};
volatile uint32_t g_markCount = 0;      // pulse n is at g_markUs[n % BUF]
volatile uint32_t g_lastMarkUs = 0;
volatile uint32_t g_markDtUs = 0;     // last accepted mark interval (echo filter)
volatile uint32_t g_markEchoes = 0;   // rejected double pulses since start

volatile uint32_t g_revCount = 0;
volatile bool     g_indexFlag = false;
volatile bool     g_indexSeen = false;  // dedicated index Hall wired
volatile bool     g_idxResync = false;  // index came, next mark is mark 0

// Evaluation (service() only):
uint32_t g_seen = 0;                    // pulses processed so far
uint8_t  g_markInRev = 0;               // last seen REGULAR mark, 0..MARKS_PER_REV-1
uint8_t  g_pulseInRev = 0;              // pulses since mark 0, incl. intermediate pulse
uint8_t  g_pulsesPerRev = MARKS_PER_REV;
bool     g_pattern = false;             // intermediate magnet confirmed
bool     g_synced = false;              // mark 0 already set once from the pattern
uint8_t  g_patternHits = 0;
uint16_t g_sinceIndex = 0xFFFF;         // pulses since the last intermediate pulse
float    g_refUs = 0.0f;                // running mean of the regular intervals
bool     g_prevShort = false;
float    g_refMaxUs = 0.0f;             // reference above this: too slow to classify (begin())
uint32_t g_resyncs = 0, g_missed = 0, g_glitches = 0;
uint32_t g_patternSeen = 0, g_patternResets = 0;   // diagnostics: pattern seen, state reset

volatile uint32_t g_motorLastUs = 0;
volatile uint32_t g_motorPeriodUs = 0;

// Wheel: ring buffer as on the drum.
constexpr uint8_t WBUF = WHEEL_MARKS_PER_REV + 1;
volatile uint32_t g_wheelUs[WBUF] = {0};
volatile uint8_t  g_wheelHead = 0;
volatile uint32_t g_wheelCount = 0;

// Wiring test: separate counter per channel (resettable) and the last timestamp.
// Counted is what the measurement sees too -- i.e. after the noise filter.
volatile uint32_t g_chCount[CH_N]  = {0};
volatile uint32_t g_chLastUs[CH_N] = {0};
volatile uint32_t g_wheelLastUs = 0;
constexpr uint8_t CH_PIN[CH_N] = {PIN_MARK, PIN_INDEX, PIN_WHEEL, PIN_MOTOR_HALL};
const char* const CH_NAME[CH_N] = {"Marks", "Index", "Wheel", "Motor"};

// One spinlock for all pairs (counter + timestamp). Until 2026-11-25 noInterrupts()
// only locked its own core -- the web task reads on core 0, the ISRs run on core 1,
// it protected nothing there. portENTER_CRITICAL locks across cores;
// the ISRs take the same lock (…_ISR), a few dozen cycles each.
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t g_lastIndexUs = 0;

// Raw data (rawlog.h): EVERY accepted edge with timestamp and channel, plus those
// rejected as echo (bit 7). Only store; collected in the main loop.
static_assert((RAWLOG_EDGE_RING & (RAWLOG_EDGE_RING - 1)) == 0, "RAWLOG_EDGE_RING: power of two");
volatile uint32_t g_edgeUs[RAWLOG_EDGE_RING];
volatile uint8_t  g_edgeCh[RAWLOG_EDGE_RING];
volatile uint32_t g_edgeHead = 0;

inline void IRAM_ATTR edge(uint8_t ch, uint32_t nowUs) {
  const uint32_t h = g_edgeHead;
  g_edgeUs[h & (RAWLOG_EDGE_RING - 1)] = nowUs;
  g_edgeCh[h & (RAWLOG_EDGE_RING - 1)] = ch;
  g_edgeHead = h + 1;
}

void IRAM_ATTR tick(Channel ch, uint32_t nowUs) {
  g_chCount[ch]++;
  g_chLastUs[ch] = nowUs;
  edge(ch, nowUs);
}

void IRAM_ATTR onMark() {
  const uint32_t now = micros();
  // Filter out glitches and bounce. The shortest real interval is at the
  // intermediate magnet (22.5°): 10.6 ms at 60 km/h, 6.4 ms at 100 km/h.
  const uint32_t dt = now - g_lastMarkUs;
  if (dt < MIN_PULSE_US) return;
  // Double pulse of the same mark (config.h MARK_ECHO_PCT): reject and count.
  if (g_markDtUs != 0 && g_markDtUs < STALL_TIMEOUT_MS * 1000UL &&
      dt < g_markDtUs / 100UL * MARK_ECHO_PCT) {
    portENTER_CRITICAL_ISR(&g_mux);
    g_markEchoes++;
    edge(CH_MARK | 0x80, now);
    portEXIT_CRITICAL_ISR(&g_mux);
    return;
  }
  portENTER_CRITICAL_ISR(&g_mux);
  g_markDtUs = dt;
  g_lastMarkUs = now;
  g_markUs[g_markCount % BUF] = now;
  g_markCount++;
  tick(CH_MARK, now);
  portEXIT_CRITICAL_ISR(&g_mux);
}

void IRAM_ATTR onIndex() {
  const uint32_t now = micros();
  // Debounced like the marks: without a filter a bouncing index sensor doubled the
  // revolutions and silently halved the averaging window (finding 2026-11-25).
  if (now - g_lastIndexUs < MIN_PULSE_US) return;
  portENTER_CRITICAL_ISR(&g_mux);
  g_lastIndexUs = now;
  g_indexSeen = true;
  g_idxResync = true;   // index defines the angle zero: next mark is mark 0
  g_revCount++;
  g_indexFlag = true;
  tick(CH_INDEX, now);
  portEXIT_CRITICAL_ISR(&g_mux);
}

void IRAM_ATTR onMotorHall() {
  const uint32_t now = micros();
  const uint32_t dt = now - g_motorLastUs;
  if (dt < MOTOR_HALL_MIN_US) return;  // noise filter (config.h)
  portENTER_CRITICAL_ISR(&g_mux);
  g_motorPeriodUs = dt;
  g_motorLastUs = now;
  tick(CH_MOTOR, now);
  portEXIT_CRITICAL_ISR(&g_mux);
}

void IRAM_ATTR onWheel() {
  const uint32_t now = micros();
  if (now - g_wheelLastUs < MIN_PULSE_US) return;
  portENTER_CRITICAL_ISR(&g_mux);
  g_wheelLastUs = now;
  g_wheelHead = (g_wheelHead + 1) % WBUF;
  g_wheelUs[g_wheelHead] = now;
  g_wheelCount++;
  tick(CH_WHEEL, now);
  portEXIT_CRITICAL_ISR(&g_mux);
}

void resetPattern() {
  g_patternResets++;
  g_pattern = false; g_synced = false; g_patternHits = 0; g_sinceIndex = 0xFFFF;
  g_pulsesPerRev = MARKS_PER_REV; g_refUs = 0.0f; g_prevShort = false; g_pulseInRev = 0;
}

// Median of the MARK_REF_N intervals ending with pulse `last`.
float medianIntervalUs(uint32_t last) {
  uint32_t v[MARK_REF_N];
  for (uint8_t k = 0; k < MARK_REF_N; k++) v[k] = g_markUs[(last - k) % BUF] - g_markUs[(last - k - 1) % BUF];
  for (uint8_t i = 1; i < MARK_REF_N; i++) { const uint32_t x = v[i]; int8_t j = i - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
  return static_cast<float>(v[MARK_REF_N / 2]);
}

inline void newRev() { g_revCount++; g_indexFlag = true; g_pulseInRev = 0; }

}  // namespace

void begin() {
  // Regular mark interval at MARK_PATTERN_MIN_KMH: 1/MARKS_PER_REV revolution.
  g_refMaxUs = DRUM_CIRCUMFERENCE_M / (MARK_PATTERN_MIN_KMH * KMH_TO_MS) / MARKS_PER_REV * 1e6f;
  pinMode(PIN_MARK, INPUT_PULLUP);
  pinMode(PIN_INDEX, INPUT_PULLUP);
  pinMode(PIN_WHEEL, INPUT_PULLUP);
  pinMode(PIN_MOTOR_HALL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_MARK), onMark, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_INDEX), onIndex, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_WHEEL), onWheel, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_HALL), onMotorHall, FALLING);
}

// Revolutions and mark number from the timestamps. Three cases:
//   index Hall wired:       the index counts the revolution, the next mark is mark 0.
//   intermediate magnet     two short intervals (~22.5° each) -- the pulse in between
//   detected:               is the intermediate pulse, the one after it mark 0. 9 pulses
//                           per revolution, 8 regular marks; the pattern re-locks the
//                           count every revolution (lost or extra pulses are counted
//                           and corrected).
//   8 marks only:           every MARKS_PER_REV-th edge is a revolution, mark 0 is
//                           the first one seen.
// The pattern is only confirmed once it came MARK_PATTERN_CONFIRM times at a spacing of
// exactly MARKS_PER_REV + 1 pulses -- a single glitch also produces two short
// intervals, but not periodically.
void service() {
  portENTER_CRITICAL(&g_mux);
  const uint32_t cnt = g_markCount;
  portEXIT_CRITICAL(&g_mux);
  if (stalled()) { if (g_pattern || g_refUs > 0) resetPattern(); }
  if (g_seen + BUF <= cnt) g_seen = cnt - 1;   // too far behind: catch up, discard the rest
  while (g_seen < cnt) {
    const uint32_t n = g_seen;
    const uint32_t t = g_markUs[n % BUF];
    const bool havePrev = n >= 1 && (n - 1) + BUF > cnt;
    const uint32_t dt = havePrev ? t - g_markUs[(n - 1) % BUF] : 0;
    g_seen++;

    if (g_idxResync) { g_idxResync = false; g_markInRev = 0; g_pulseInRev = 0; g_prevShort = false; continue; }

    // Reference: median of the preceding MARK_REF_N intervals. It is always a regular
    // interval (at most 2 of 9 are halves) and follows the speed. Until 2026-11-25 it was
    // a moving average of the regular intervals only: if it started with a slow interval
    // on start-up (817 ms), EVERY interval counted as short afterwards, the average was
    // never updated again, and the intermediate magnet was never detected.
    // Classification only from MARK_PATTERN_MIN_KMH: below that the speed changes so much
    // within one revolution that a regular interval looks "short".
    const bool haveRef  = havePrev && n >= MARK_REF_N + 1 && (n - 1 - MARK_REF_N) + BUF > cnt;
    g_refUs = haveRef ? medianIntervalUs(n - 1) : 0.0f;
    const bool fast     = g_refUs > 0.0f && g_refUs < g_refMaxUs;
    const bool classify = havePrev && fast && dt < 1000000UL;
    const bool shortDt  = classify && dt < MARK_SHORT_RATIO * g_refUs;
    const bool longDt   = classify && dt > MARK_MISSED_RATIO * g_refUs;
    if (longDt) g_missed++;
    if (g_sinceIndex < 0xFFFF) g_sinceIndex++;

    // Two short intervals in a row: pulse n-1 was the intermediate pulse, n is mark 0.
    if (MARK_PATTERN_INDEX && !g_indexSeen && shortDt && g_prevShort) {
      g_prevShort = false;
      g_patternSeen++;
      if (!g_pattern) {
        g_patternHits = (g_sinceIndex == MARKS_PER_REV + 1) ? g_patternHits + 1 : 1;
        g_sinceIndex = 0;
        if (g_patternHits >= MARK_PATTERN_CONFIRM) { g_pattern = true; g_pulsesPerRev = MARKS_PER_REV + 1; }
        else { g_markInRev = (g_markInRev + 1) % MARKS_PER_REV; if (g_markInRev == 0) newRev(); continue; }
      } else {
        // Near the expected position? Otherwise a glitch that makes two halves --
        // unless the pattern came exactly MARKS_PER_REV + 1 pulses after the previous one:
        // then it is real and the count was off. Without this exception the count locked
        // wrongly once on 2026-11-25 and afterwards took the real pattern for a glitch
        // every revolution (131 glitches, 129 swallowed in 30 s).
        const bool periodic = g_sinceIndex == MARKS_PER_REV + 1;
        g_sinceIndex = 0;
        if (g_synced && !periodic && g_pulseInRev < MARKS_PER_REV - 1) { g_glitches++; g_markInRev = (g_markInRev + 1) % MARKS_PER_REV; g_pulseInRev++; continue; }
        if (g_synced && (g_markInRev != MARKS_PER_REV - 1 || g_pulseInRev != MARKS_PER_REV)) g_resyncs++;
      }
      g_synced = true;
      g_markInRev = 0;
      newRev();
      continue;
    }
    g_prevShort = shortDt;

    if (g_pattern) {
      g_pulseInRev++;
      if (shortDt) continue;                       // first short one: intermediate pulse, not a mark
      g_markInRev = (g_markInRev + 1) % MARKS_PER_REV;
      if (g_markInRev == 0) { g_missed++; newRev(); }   // pattern missing: go by count
    } else {
      g_markInRev = (g_markInRev + 1) % MARKS_PER_REV;
      if (g_markInRev == 0 && !g_indexSeen) newRev();
    }
  }
}

float revPerSec() {
  portENTER_CRITICAL(&g_mux);
  const uint32_t cnt = g_markCount;
  const uint32_t lastUs = g_lastMarkUs;
  portEXIT_CRITICAL(&g_mux);
  const uint8_t ppr = g_pulsesPerRev;
  if (cnt <= ppr) return 0.0f;
  const uint32_t tNew = g_markUs[(cnt - 1) % BUF];
  const uint32_t tOld = g_markUs[(cnt - 1 - ppr) % BUF];

  const uint32_t span = tNew - tOld;   // one full revolution, independent of the spacing
  if (span == 0) return 0.0f;

  // Is the drum standing right now? Then the last timestamp is old. The yardstick is the
  // MARK INTERVAL, not the revolution: with two revolutions as the deadline the display
  // hung at 1.7 km/h for twelve seconds in the braking comparison on 2026-11-25 while the
  // drum had long stopped (one revolution takes 6 s there) -- and the relay brake
  // waited that long for "standstill".
  if (micros() - lastUs > (span / ppr) * 2 + SPEED_STALE_GRACE_US) return 0.0f;

  return 1000000.0f / static_cast<float>(span);
}

float kmh() { return kmhFromRevPerSec(revPerSec()); }
float omega() { return omegaFromRevPerSec(revPerSec()); }

uint32_t markCount() { return g_markCount; }
uint32_t revCount() { return g_revCount; }

bool consumeIndexFlag() {
  // Read and clear in one: an index pulse between the two would otherwise be lost.
  portENTER_CRITICAL(&g_mux);
  const bool was = g_indexFlag;
  g_indexFlag = false;
  portEXIT_CRITICAL(&g_mux);
  return was;
}

uint8_t  markIndexInRev() { return g_markInRev; }
uint8_t  pulsesPerRev()   { return g_pulsesPerRev; }
RevSource revSource() {
  if (g_indexSeen) return REV_INDEX_HALL;
  if (g_pattern)   return REV_PATTERN;
  return REV_COUNT;
}
uint32_t patternResyncs() { return g_resyncs; }
uint32_t missedPulses()   { return g_missed; }
uint32_t glitchPulses()   { return g_glitches; }

float motorRevPerSec() {
  portENTER_CRITICAL(&g_mux);
  const uint32_t period = g_motorPeriodUs;
  const uint32_t last = g_motorLastUs;
  portEXIT_CRITICAL(&g_mux);

  if (period == 0) return 0.0f;
  if (micros() - last > 500000UL) return 0.0f;  // standing
  return 1000000.0f / (static_cast<float>(period) * MOTOR_PULSES_PER_REV);
}

void wheelPulses(uint32_t& count, uint32_t& lastUs) {
  portENTER_CRITICAL(&g_mux);
  count = g_wheelCount;
  lastUs = g_wheelLastUs;
  portEXIT_CRITICAL(&g_mux);
}

float wheelRevPerSec() {
  if (g_wheelCount <= WHEEL_MARKS_PER_REV) return 0.0f;

  portENTER_CRITICAL(&g_mux);
  const uint8_t head = g_wheelHead;
  const uint32_t tNew = g_wheelUs[head];
  const uint32_t tOld = g_wheelUs[(head + WBUF - WHEEL_MARKS_PER_REV) % WBUF];
  const uint32_t lastUs = g_wheelLastUs;
  portEXIT_CRITICAL(&g_mux);

  const uint32_t span = tNew - tOld;   // one full wheel revolution
  if (span == 0) return 0.0f;
  if (micros() - lastUs > (span / WHEEL_MARKS_PER_REV) * 2 + SPEED_STALE_GRACE_US) return 0.0f;   // standing (mark interval, see above)
  return 1000000.0f / static_cast<float>(span);
}

bool slipDetected() {
  const float drum = revPerSec();
  const float motor = motorRevPerSec();
  if (drum < 0.2f || motor < 0.2f) return false;  // too slow to tell

  const float expected = motor / BELT_RATIO;
  return fabsf(expected - drum) / expected > SLIP_TOLERANCE;
}

bool stalled() {
  portENTER_CRITICAL(&g_mux);
  const uint32_t last = g_lastMarkUs;
  portEXIT_CRITICAL(&g_mux);
  return (micros() - last) > STALL_TIMEOUT_MS * 1000UL;
}

const char* channelName(Channel ch) { return CH_NAME[ch]; }
uint32_t markEchoes() { return g_markEchoes; }
void patternDiag(float& refUs, uint8_t& hits, uint16_t& since, uint32_t& seen, uint32_t& resets) {
  refUs = g_refUs; hits = g_patternHits; since = g_sinceIndex; seen = g_patternSeen; resets = g_patternResets;
}

// The last n mark intervals in degrees, normalised to the sum of the last pulsesPerRev
// intervals = 360 degrees (intermediate magnet diagnostics). n <= BUF - 1. Newest last.
uint8_t recentIntervalsDeg(float* out, uint8_t n, uint8_t perRev) {
  portENTER_CRITICAL(&g_mux);
  const uint32_t cnt = g_markCount;
  uint32_t ts[BUF];
  for (uint8_t i = 0; i < BUF; i++) ts[i] = g_markUs[i];
  portEXIT_CRITICAL(&g_mux);
  if (n > BUF - 1) n = BUF - 1;
  if (perRev > n) perRev = n;
  if (cnt < static_cast<uint32_t>(n) + 1) return 0;
  float rev = 0.0f;
  for (uint8_t k = 0; k < perRev; k++) rev += static_cast<float>(ts[(cnt - 1 - k) % BUF] - ts[(cnt - 2 - k) % BUF]);
  if (!(rev > 0.0f)) return 0;
  for (uint8_t k = 0; k < n; k++) {
    const uint32_t i = cnt - n + k;
    out[k] = static_cast<float>(ts[i % BUF] - ts[(i - 1) % BUF]) / rev * 360.0f;
  }
  return n;
}

ChannelState channel(Channel ch) {
  portENTER_CRITICAL(&g_mux);
  const uint32_t count = g_chCount[ch];
  const uint32_t last  = g_chLastUs[ch];
  portEXIT_CRITICAL(&g_mux);
  ChannelState s;
  s.count   = count;
  s.sinceMs = count ? (micros() - last) / 1000UL : 0xFFFFFFFFUL;
  s.low     = digitalRead(CH_PIN[ch]) == LOW;
  return s;
}

void resetCounters() {
  portENTER_CRITICAL(&g_mux);
  for (uint8_t i = 0; i < CH_N; i++) { g_chCount[i] = 0; g_chLastUs[i] = 0; }
  portEXIT_CRITICAL(&g_mux);
  g_resyncs = g_missed = g_glitches = 0;
  g_markEchoes = 0;
}

uint32_t rawEdges(uint32_t from, uint32_t* us, uint8_t* ch, uint32_t max, uint32_t* lost) {
  portENTER_CRITICAL(&g_mux);
  const uint32_t head = g_edgeHead;
  *lost = 0;
  if (head - from > RAWLOG_EDGE_RING) { *lost = head - from - RAWLOG_EDGE_RING; from = head - RAWLOG_EDGE_RING; }
  uint32_t n = 0;
  while (from != head && n < max) {
    us[n] = g_edgeUs[from & (RAWLOG_EDGE_RING - 1)];
    ch[n] = g_edgeCh[from & (RAWLOG_EDGE_RING - 1)];
    from++; n++;
  }
  portEXIT_CRITICAL(&g_mux);
  return from;
}

}  // namespace speed
