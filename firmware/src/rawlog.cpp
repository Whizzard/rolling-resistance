#include "rawlog.h"
#include "config.h"
#include "calc/rawlog.h"
#include "calc/timefmt.h"
#include "logger.h"
#include "settings.h"
#include "torque.h"
#include "speed.h"
#include "speedctl.h"
#include "brake.h"
#include "state.h"
#include "run.h"
#include "tirepressure.h"
#include "wallclock.h"
#include "vesc/vesc.h"

#include <atomic>
#include <esp_heap_caps.h>

namespace rawlog {
namespace {

using calc::rrl::Encoder;
using calc::rrl::REC_MAX;

// ------------------------------------------------------------------ Ring
// One producer (main loop, core 1), one consumer (storage task, core 0).
// head/tail count bytes continuously; the size is a power of two.
uint8_t* g_buf = nullptr;
uint32_t g_size = 0;
std::atomic<uint32_t> g_head{0}, g_tail{0};

// Writer state. Transitions: start() IDLE->OPEN_REQ, task OPEN_REQ->OPEN,
// stop() OPEN/OPEN_REQ->STOP_REQ, task STOP_REQ->IDLE (ring empty) or on error ->IDLE.
enum W : uint8_t { W_IDLE = 0, W_OPEN_REQ, W_OPEN, W_STOP_REQ };
std::atomic<uint8_t> g_w{W_IDLE};
std::atomic<const char*> g_err{nullptr};   // set by the task, reported by service()

storage::Media g_media = storage::SD_CARD;
char     g_file[48] = "";
char     g_hdr[640];
size_t   g_hdrLen = 0;
std::atomic<uint32_t> g_written{0};

// Main loop only:
bool     g_rec = false;          // records are being queued
bool     g_manual = false;
bool     g_autoRefused = false;  // run active, but no storage: do not report again every round
Encoder  g_enc;
uint32_t g_dropped = 0, g_gapBytes = 0;
uint32_t g_lastSync = 0;
uint32_t g_edgeFrom = 0;
uint32_t g_vescSeen = 0;
uint32_t g_tireAge = UINT32_MAX;
struct Ctrl { int16_t target, duty; uint8_t brake, phase; } g_ctrl = {INT16_MIN, INT16_MIN, 0xFF, 0xFF};

// Task only:
File     g_fh;
uint32_t g_lastFlush = 0;
uint8_t  g_chunk[RAWLOG_CHUNK_BYTES];

uint32_t freeBytes() { return g_size - (g_head.load() - g_tail.load()); }

void copyIn(const uint8_t* p, size_t n) {
  uint32_t h = g_head.load();
  for (size_t i = 0; i < n; i++, h++) g_buf[h & (g_size - 1)] = p[i];
  g_head.store(h);
}

// One record into the ring. If it does not fit, it is dropped AND the encoder's
// time reference is reset -- otherwise every following interval would be wrong.
template <typename F>
void put(uint32_t us, F encode) {
  if (!g_rec || !g_buf) return;
  uint8_t tmp[REC_MAX + 16];
  const uint32_t prev = g_enc.prevUs;
  const size_t gap = g_gapBytes ? g_enc.gap(tmp, us, g_gapBytes) : 0;
  const size_t n = gap + encode(tmp + gap);
  if (n > freeBytes()) {
    g_enc.prevUs = prev;
    g_gapBytes += n - gap; g_dropped += n - gap;
    return;
  }
  g_gapBytes = 0;
  copyIn(tmp, n);
}

void sync(uint32_t us) {
  put(us, [&](uint8_t* o) { return g_enc.sync(o, us, wallclock::now()); });
  g_lastSync = millis();
}

// ------------------------------------------------------------------ Header
void buildHeader() {
  char t[24]; calc::fmtTime(wallclock::now(), t, sizeof(t), true);
  const int n = snprintf(g_hdr, sizeof(g_hdr),
    "format=rrl1\nfirmware=" __DATE__ " " __TIME__ "\nstart_local=%s\nunix_start_local=%lu\n"
    "time_unit=us (micros of the ESP32)\n"
    "edge_channels=0 drum marks,1 drum index,2 test wheel,3 motor hall; bit7 = rejected as echo\n"
    "marks_per_rev=%u\nwheel_marks_per_rev=%u\nmotor_pole_pairs=%u\n"
    "drum_circumference_m=%.5f\ntorque_arm_m=%.5f\nloadcell_n_per_count=%.9f\ntare_offset_counts=%ld\n"
    "direction_sign=%d\nvesc_poll_ms=%lu\n",
    t, static_cast<unsigned long>(wallclock::now()),
    static_cast<unsigned>(MARKS_PER_REV), static_cast<unsigned>(WHEEL_MARKS_PER_REV),
    static_cast<unsigned>(MOTOR_POLE_PAIRS),
    settings::drumCircumferenceM(), settings::torqueArmM(), settings::loadcellNPerCount(),
    torque::tareOffset(), static_cast<int>(settings::directionSign()),
    static_cast<unsigned long>(VESC_POLL_MS));
  size_t k = n > 0 ? static_cast<size_t>(n) : 0;
  if (k > sizeof(g_hdr) - 1 - calc::rrl::HEADER_FIXED) k = sizeof(g_hdr) - 1 - calc::rrl::HEADER_FIXED;
  g_hdrLen = calc::rrl::HEADER_FIXED + k;
  // Insert the header at the front: magic + length, text behind it.
  memmove(g_hdr + calc::rrl::HEADER_FIXED, g_hdr, g_hdrLen - calc::rrl::HEADER_FIXED);
  memcpy(g_hdr, calc::rrl::MAGIC, 4);
  calc::rrl::put16(reinterpret_cast<uint8_t*>(g_hdr) + 4, static_cast<uint16_t>(g_hdrLen - calc::rrl::HEADER_FIXED));
}

void buildName() {
  const uint32_t t = wallclock::now();
  if (t) {
    char s[24]; calc::fmtTime(t, s, sizeof(s), false);
    snprintf(g_file, sizeof(g_file), "/raw/%s%02lu.rrl", s, static_cast<unsigned long>(t % 60));
  } else {
    snprintf(g_file, sizeof(g_file), "/raw/up%lu.rrl", static_cast<unsigned long>(millis() / 1000));
  }
}

bool startInternal(bool manualStart) {
  if (g_w.load() != W_IDLE) { logger::fail(g_w.load() == W_STOP_REQ ? "Raw data: still writing" : "Raw data already on"); return false; }
  if (!g_buf) { logger::fail("Raw data: no buffer"); return false; }
  // SD ONLY (2026-11-26): writing to the flash locks the sensor ISRs (storage.h).
  if (storage::info(storage::SD_CARD).state != storage::READY) { logger::fail("No SD"); return false; }
  g_media = storage::SD_CARD;
  const int8_t pct = storage::info(g_media).pct;
  if (pct >= RAWLOG_STOP_PCT) { logger::fail("Storage full"); return false; }

  buildHeader();
  buildName();
  g_head.store(0); g_tail.store(0);
  g_written.store(0);
  g_err.store(nullptr);
  g_dropped = g_gapBytes = 0;
  g_ctrl = {INT16_MIN, INT16_MIN, 0xFF, 0xFF};
  g_vescSeen = vesc::values().count;
  g_rec = true;
  g_manual = manualStart;
  sync(micros());
  g_w.store(W_OPEN_REQ);
  Serial.print("# Raw data -> "); Serial.print(storage::name(g_media)); Serial.println(g_file);
  return true;
}

void stopInternal() {
  if (!g_rec) return;
  g_rec = false;
  g_manual = false;
  uint8_t w = g_w.load();
  if (w == W_OPEN || w == W_OPEN_REQ) g_w.compare_exchange_strong(w, W_STOP_REQ);
}

// ------------------------------------------------------------------ Task side
void fail(const char* why) {
  if (g_fh) g_fh.close();
  g_err.store(why);
  g_tail.store(g_head.load());
  g_w.store(W_IDLE);
}

// Up to one chunk from the ring into the file. false on write error.
bool writeChunk(uint32_t n) {
  const uint32_t t = g_tail.load();
  for (uint32_t i = 0; i < n; i++) g_chunk[i] = g_buf[(t + i) & (g_size - 1)];
  if (g_fh.write(g_chunk, n) != n) return false;
  g_tail.store(t + n);
  g_written.fetch_add(n);
  return true;
}

bool openFile(fs::FS& f) {
  // Never overwrite: same name after reboot without clock -> suffix.
  char name[sizeof(g_file)];
  strncpy(name, g_file, sizeof(name));
  for (char c = 'b'; f.exists(name) && c <= 'z'; c++)
    snprintf(name, sizeof(name), "%.*s-%c.rrl", static_cast<int>(strlen(g_file) - 4), g_file, c);
  strncpy(g_file, name, sizeof(g_file));
  g_fh = f.open(g_file, FILE_WRITE);
  if (!g_fh) return false;
  if (g_fh.write(reinterpret_cast<const uint8_t*>(g_hdr), g_hdrLen) != g_hdrLen) { g_fh.close(); return false; }
  g_written.store(g_hdrLen);
  g_lastFlush = millis();
  return true;
}

}  // namespace

// ------------------------------------------------------------------ public
void begin() {
  g_size = RAWLOG_BUF_BYTES;
  g_buf = static_cast<uint8_t*>(heap_caps_malloc(g_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!g_buf) {   // without PSRAM: small in internal RAM, only enough for the SD
    g_size = 32 * 1024;
    g_buf = static_cast<uint8_t*>(malloc(g_size));
  }
  Serial.print("# Raw data: ring "); Serial.print(g_buf ? g_size / 1024 : 0); Serial.println(" kB");
}

void service(bool loadcellFresh) {
  const uint32_t nowMs = millis();

  // Standstill for the flash (config.h RAWLOG_FLASH_IDLE_MS): no sensor has
  // reported, the drive is off, the state machine is idle.
  bool quiet = !speedctl::running() && (state::phase() == state::IDLE || state::phase() == state::FAULT);
  for (uint8_t c = 0; c < speed::CH_N && quiet; c++)
    if (speed::channel(static_cast<speed::Channel>(c)).sinceMs < RAWLOG_FLASH_IDLE_MS) quiet = false;
  storage::setFlashWritable(quiet);

  // Report errors of the writer task (from the main loop: logger is not thread-safe).
  if (const char* e = g_err.exchange(nullptr)) {
    g_rec = false; g_manual = false;
    g_autoRefused = run::active();   // do not restart and fail again every round
    logger::fail(e);
  }

  // Automatically with the guided run.
  if (!g_manual) {
    const bool want = run::active();
    if (want && !g_rec && !g_autoRefused && g_w.load() == W_IDLE) {
      if (!startInternal(false)) g_autoRefused = true;
    }
    if (!want) { g_autoRefused = false; if (g_rec) stopInternal(); }
  }

  // Always fetch edges, so that the ISRs' ring does not overflow.
  {
    uint32_t us[32]; uint8_t ch[32]; uint32_t lost = 0;
    for (;;) {
      const uint32_t before = g_edgeFrom;
      g_edgeFrom = speed::rawEdges(g_edgeFrom, us, ch, 32, &lost);
      const uint32_t n = g_edgeFrom - before - lost;
      if (lost && g_rec) { char t[40]; snprintf(t, sizeof(t), "edges_lost=%lu", static_cast<unsigned long>(lost)); text(t); }
      for (uint32_t i = 0; i < n; i++) put(us[i], [&](uint8_t* o) { return g_enc.edge(o, us[i], ch[i]); });
      if (n < 32) break;
    }
  }
  if (!g_rec) return;

  const uint32_t nowUs = micros();
  if (nowMs - g_lastSync >= RAWLOG_SYNC_MS) { sync(nowUs); g_ctrl.phase = 0xFF; }   // CTRL with every SYNC

  if (loadcellFresh) {
    const int32_t raw = torque::lastRaw();
    put(nowUs, [&](uint8_t* o) { return g_enc.hx711(o, nowUs, raw); });
  }

  const vesc::Values& v = vesc::values();
  if (v.count != g_vescSeen) {
    g_vescSeen = v.count;
    put(v.rxUs, [&](uint8_t* o) {
      return g_enc.vesc(o, v.rxUs, v.erpm, v.tacho, v.raw.duty1000, v.raw.iMot100, v.raw.iIn100,
                        v.raw.vIn10, v.raw.tFet10, v.raw.tMot10, v.fault, v.status);
    });
  }

  const uint32_t age = tirepressure::ageMs();
  if (age != UINT32_MAX && age < g_tireAge) {
    const float b = tirepressure::bar(), t = tirepressure::tempC(), bt = tirepressure::battery();
    const uint16_t mbar = isnan(b) ? 0xFFFF : static_cast<uint16_t>(lroundf(b * 1000.0f));
    const int16_t  t10  = isnan(t) ? INT16_MIN : static_cast<int16_t>(lroundf(t * 10.0f));
    const uint8_t  bat  = isnan(bt) ? 0xFF : static_cast<uint8_t>(lroundf(bt));
    put(nowUs, [&](uint8_t* o) { return g_enc.tire(o, nowUs, mbar, t10, bat); });
  }
  g_tireAge = age;

  const Ctrl c = {static_cast<int16_t>(lroundf(speedctl::targetKmh() * 100.0f)),
                  static_cast<int16_t>(lroundf(speedctl::duty() * 10000.0f)),
                  static_cast<uint8_t>(brake::engaged() ? 1 : 0),
                  static_cast<uint8_t>(state::phase())};
  if (c.target != g_ctrl.target || c.duty != g_ctrl.duty || c.brake != g_ctrl.brake || c.phase != g_ctrl.phase) {
    g_ctrl = c;
    put(nowUs, [&](uint8_t* o) { return g_enc.ctrl(o, nowUs, c.target, c.duty, c.brake, c.phase); });
  }
}

void writerService() {
  const uint8_t st = g_w.load();
  if (st == W_IDLE) return;
  fs::FS* f = storage::fs(g_media);
  if (!f) { fail("Raw data: storage lost"); return; }

  if (st == W_OPEN_REQ || (st == W_STOP_REQ && !g_fh)) {
    if (!openFile(*f)) { fail("Raw data: file not created"); return; }
    uint8_t exp = W_OPEN_REQ;
    g_w.compare_exchange_strong(exp, W_OPEN);   // stop() may have come in between
  }
  const bool stopping = g_w.load() == W_STOP_REQ;
  uint32_t avail = g_head.load() - g_tail.load();
  // Whole chunks; the remainder only when flushing or stopping.
  const bool flushDue = millis() - g_lastFlush >= RAWLOG_FLUSH_MS;
  while (avail >= RAWLOG_CHUNK_BYTES || ((flushDue || stopping) && avail > 0)) {
    const uint32_t n = avail < RAWLOG_CHUNK_BYTES ? avail : RAWLOG_CHUNK_BYTES;
    if (!writeChunk(n)) { fail(storage::info(g_media).pct >= RAWLOG_STOP_PCT ? "Storage full" : "Raw data: write error"); return; }
    avail -= n;
  }
  if (flushDue) { g_fh.flush(); g_lastFlush = millis(); }

  if (stopping && g_head.load() == g_tail.load()) {
    g_fh.close();
    Serial.print("# Raw data closed: "); Serial.print(g_file); Serial.print(", ");
    Serial.print(g_written.load()); Serial.println(" B");
    g_w.store(W_IDLE);
    return;
  }
  if (storage::info(g_media).pct >= RAWLOG_STOP_PCT) {
    g_fh.close();
    g_err.store("Storage full");
    g_tail.store(g_head.load());
    g_w.store(W_IDLE);
  }
}

void mediaLost(storage::Media m) {
  if (g_w.load() == W_IDLE || g_media != m) return;
  fail("Raw data: storage lost");
}

bool start() { return startInternal(true); }
void stop() {
  if (!g_rec) { logger::note("Raw data off"); return; }
  stopInternal();
}
bool active() { return g_rec || g_w.load() != W_IDLE; }
bool manual() { return g_manual; }
bool draining() { return !g_rec && g_w.load() != W_IDLE; }
storage::Media media() { return g_media; }
const char* fileName() { return g_file; }
uint32_t bytesWritten() { return g_written.load(); }
uint32_t bytesPending() { return g_head.load() - g_tail.load(); }
uint32_t bytesDropped() { return g_dropped; }

void text(const char* s) {
  const uint32_t us = micros();
  put(us, [&](uint8_t* o) { return g_enc.text(o, us, s); });
}

}  // namespace rawlog
