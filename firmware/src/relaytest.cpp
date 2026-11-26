#include "relaytest.h"

#include "brake.h"
#include "config.h"
#include "logger.h"
#include "revsrc.h"
#include "run.h"
#include "speed.h"
#include "state.h"
#include "torque.h"

namespace relaytest {
namespace {

enum Ph : uint8_t { OFF, QUIET, SWITCHED, WAIT };
Ph       g_ph = OFF;
uint8_t  g_n = 0, g_k = 0;
uint32_t g_t0 = 0;
uint32_t g_cnt[speed::CH_N], g_echoW = 0, g_echoM = 0;
uint32_t g_quiet[speed::CH_N + 2];     // edge deltas of the quiet window (+ wheel, mark echoes)
uint32_t g_reads = 0;                  // torque::readsTotal at the last look
double   g_sum = 0; uint32_t g_num = 0; long g_lo = 0, g_hi = 0;
long     g_qMean = 0, g_qPp = 0;

void snap() {
  for (uint8_t c = 0; c < speed::CH_N; c++) g_cnt[c] = speed::channel(static_cast<speed::Channel>(c)).count;
  g_echoW = speed::wheelEchoes(); g_echoM = speed::markEchoes();
  g_sum = 0; g_num = 0; g_lo = LONG_MAX; g_hi = LONG_MIN;
}
void deltas(uint32_t* d) {
  for (uint8_t c = 0; c < speed::CH_N; c++) d[c] = speed::channel(static_cast<speed::Channel>(c)).count - g_cnt[c];
  d[speed::CH_N] = speed::wheelEchoes() - g_echoW; d[speed::CH_N + 1] = speed::markEchoes() - g_echoM;
}
// New HX711 values since the last look: only the newest is visible, enough at 80 SPS
// and a 1 kHz loop.
void sampleCell() {
  const uint32_t r = torque::readsTotal();
  if (r == g_reads) return;
  g_reads = r;
  const long v = torque::lastRaw();
  g_sum += v; g_num++; if (v < g_lo) g_lo = v; if (v > g_hi) g_hi = v;
}
bool still() { const float k = revsrc::kmh(); return !isnan(k) && k < 0.5f; }

}  // namespace

bool start(uint8_t n) {
  if (g_ph != OFF) { logger::fail("Relay test already running"); return false; }
  if (!brake::available()) { logger::fail("Relay test: no relay brake"); return false; }
  if (run::busy() || state::phase() != state::IDLE || !still() || brake::active()) {
    logger::fail("Relay test: only at standstill, outside a run, relays on"); return false; }
  g_n = n ? n : 1; g_k = 0; g_ph = QUIET; g_t0 = millis(); g_reads = torque::readsTotal(); snap();
  Serial.printf("# Relay test: %u cycles, %lu ms windows. Relays will click.\n", g_n, (unsigned long)RELAYTEST_WIN_MS);
  return true;
}

void service() {
  if (g_ph == OFF) return;
  sampleCell();
  const uint32_t now = millis();
  if (!still() || state::phase() != state::IDLE) { g_ph = OFF; logger::fail("Relay test aborted: drum moving or drive active"); return; }
  if (g_ph == QUIET && now - g_t0 >= RELAYTEST_WIN_MS) {
    deltas(g_quiet);
    g_qMean = g_num ? static_cast<long>(g_sum / g_num) : 0; g_qPp = g_num ? g_hi - g_lo : 0;
    snap(); brake::engage(); g_ph = SWITCHED; g_t0 = now;
  } else if (g_ph == SWITCHED && now - g_t0 >= RELAYTEST_WIN_MS) {
    uint32_t d[speed::CH_N + 2]; deltas(d);
    const long dev = g_num ? max(labs(g_hi - g_qMean), labs(g_lo - g_qMean)) : 0;
    Serial.printf("# Relay test %u/%u  edges quiet|switched: marks %lu|%lu (echo %lu|%lu)  index %lu|%lu  wheel %lu|%lu (echo %lu|%lu)  motor %lu|%lu"
                  "  load cell p-p quiet %ld, switched max dev %ld counts (%lu values)\n",
                  g_k + 1, g_n,
                  (unsigned long)g_quiet[speed::CH_MARK], (unsigned long)d[speed::CH_MARK], (unsigned long)g_quiet[speed::CH_N + 1], (unsigned long)d[speed::CH_N + 1],
                  (unsigned long)g_quiet[speed::CH_INDEX], (unsigned long)d[speed::CH_INDEX],
                  (unsigned long)g_quiet[speed::CH_WHEEL], (unsigned long)d[speed::CH_WHEEL], (unsigned long)g_quiet[speed::CH_N], (unsigned long)d[speed::CH_N],
                  (unsigned long)g_quiet[speed::CH_MOTOR], (unsigned long)d[speed::CH_MOTOR],
                  g_qPp, dev, (unsigned long)g_num);
    g_ph = WAIT; g_t0 = now;
  } else if (g_ph == WAIT && !brake::active() && brake::releasedForMs() >= RELAY_SETTLE_MS) {
    if (++g_k >= g_n) { g_ph = OFF; Serial.println("# Relay test done"); return; }
    snap(); g_ph = QUIET; g_t0 = now;
  }
}

bool active() { return g_ph != OFF; }

}  // namespace relaytest
