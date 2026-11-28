#include "build_id.h"
#include "rawlog.h"
#include "run.h"
#include "config.h"
#include "calc/rawlog.h"
#include "calc/timefmt.h"
#include "logger.h"
#include "settings.h"
#include "calib.h"
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

using calc::rawcsv::Encoder;
using calc::rawcsv::REC_MAX;

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
char     g_file[80] = "";    // <base>.csv, the stream
char     g_base[72] = "";    // /vmrr/<session>/raw_<session>[-b...] without extension
char     g_hdr[calc::rawcsv::HEADER_MAX];   // metadata JSON without the file keys (<base>.json)
size_t   g_hdrLen = 0;
char     g_origin[64] = "";  // first sentence of the legend
char     g_leg[calc::rawcsv::LEGEND_MAX];   // task only: legend Markdown (<base>.md)
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

// One record line into the ring. If it does not fit, it is dropped and counted; the next
// line that fits is preceded by a gap line with the dropped byte count. The time in
// the lines is absolute, so nothing else has to be repaired after a drop.
template <typename F>
void put(uint32_t us, F encode) {
  if (!g_rec || !g_buf) return;
  char tmp[REC_MAX + 48];
  const size_t gap = g_gapBytes ? g_enc.gap(tmp, sizeof(tmp), us, g_gapBytes) : 0;
  const size_t k = encode(tmp + gap, sizeof(tmp) - gap);
  if (!k) return;                       // did not fit the line buffer: cannot happen with REC_MAX
  const size_t n = gap + k;
  if (n > freeBytes()) {
    g_gapBytes += k; g_dropped += k;
    return;
  }
  g_gapBytes = 0;
  copyIn(reinterpret_cast<const uint8_t*>(tmp), n);
}

void sync(uint32_t us) {
  put(us, [&](char* o, size_t n) { return g_enc.sync(o, n, us, wallclock::now()); });
  g_lastSync = millis();
}

// ------------------------------------------------------------------ Header
// <base>.json: version, stand, dimensions, calibration, columns (builder 2026-11-27: "header
// with meta info on version, setup, dimensions and calibration"; as a separate JSON next to
// the CSV stream). Built here in the main loop, written by the storage task.
const char* const PHASE_NAMES[] = {"IDLE", "TARE", "SPINUP", "SETTLE", "POINT", "COAST", "FAULT",
                                   "WIZ_SPINUP", "WIZ_LIFT", "HOLD"};   // state.h Phase, in order

void buildHeader() {
  char t[24]; calc::fmtTime(wallclock::now(), t, sizeof(t), true);
  char why[160]; const int8_t valid = calib::validity(why, sizeof(why));
  calc::rawcsv::HeaderInfo h;
  h.firmware = __DATE__ " " __TIME__; h.commit = FW_COMMIT; h.standRev = settings::standRev();
  h.startLocal = t; h.unixStartLocal = wallclock::now();
  h.runId = run::active() ? run::id() : "";
  h.marksPerRev = MARKS_PER_REV; h.wheelMarksPerRev = WHEEL_MARKS_PER_REV; h.motorPolePairs = MOTOR_POLE_PAIRS;
  h.drumCircM = settings::drumCircumferenceM(); h.torqueArmM = settings::torqueArmM();
  h.calibArmM = settings::calibArmM(); h.wheelCircM = settings::wheelCircM();
  h.directionSign = settings::directionSign(); h.vescPollMs = VESC_POLL_MS;
  h.hx711RateSps = HX711_RATE_SPS; h.hx711GainPulses = HX711_GAIN_PULSES; h.syncMs = RAWLOG_SYNC_MS;
  h.loadcellCapacityN = LOADCELL_CAPACITY_N; h.loadEmptyKg = settings::loadEmptyKg();
  h.loadWeightKg = settings::loadWeightKg(); h.loadArmM = settings::loadArmM();
  h.nPerCount = settings::calibrated() ? settings::loadcellNPerCount() : NAN;   // TBD -> null
  h.tareOffset = torque::tareOffset();
  h.validity = valid; h.validityWhy = why;
  const settings::CalRecord& r = settings::calRecord();
  if (r.ver != 0) {
    h.cal.present = true; h.cal.ver = r.ver; h.cal.gainPulses = r.gainPulses; h.cal.sps = r.sps;
    h.cal.epoch = r.epoch; h.cal.npc = r.npc; h.cal.armM = r.armM; h.cal.lcalM = r.lcalM;
    h.cal.spsMeas = r.spsMeas; h.cal.linPctFS = r.linPctFS; h.cal.hystPctFS = r.hystPctFS;
    h.cal.zeroPctFS = r.zeroPctFS; h.cal.nLoads = r.nLoads; h.cal.nPoints = r.nPoints;
    static char method[sizeof(r.method) + 1];
    memcpy(method, r.method, sizeof(r.method)); method[sizeof(r.method)] = '\0';
    h.cal.method = method;
  }
  h.phases = PHASE_NAMES; h.phaseN = sizeof(PHASE_NAMES) / sizeof(PHASE_NAMES[0]);
  g_hdrLen = calc::rawcsv::header(g_hdr, sizeof(g_hdr), h);
  if (!g_hdrLen) {   // cannot happen with HEADER_MAX; never write broken JSON
    g_hdrLen = static_cast<size_t>(snprintf(g_hdr, sizeof(g_hdr),
      "{\"format\":\"%s\",\"version\":%d,\"commit\":\"" FW_COMMIT "\",\"error\":\"header too long\"}\n",
      calc::rawcsv::FORMAT, calc::rawcsv::VERSION));
  }
}

// Session directory /vmrr/<id>/ (storage.h): the run's id during a run, otherwise a new
// one from the clock. The files are raw_<session>.csv/.json/.md in it (the session also in the file
// name since 2026-11-26, so a file copied out on its own still says where it belongs);
// openFile() creates the directory.
// A run records from step 2 on (since 2026-11-26): step 1 only creates the run (choose
// the wheel), and the Measure tab opens it on a tap -- that alone must not start a raw log.
bool runWantsRaw() { return run::busy() && run::step() >= 2; }

char g_dir[40] = "";
void buildName() {
  char sess[24];
  if (run::active() && run::id()[0]) strncpy(sess, run::id(), sizeof(sess) - 1), sess[sizeof(sess) - 1] = '\0';
  else storage::sessionName(sess, sizeof(sess));
  snprintf(g_dir, sizeof(g_dir), "%s/%s", storage::ROOT_DIR, sess);
  snprintf(g_base, sizeof(g_base), "%s/raw_%s", g_dir, sess);
  snprintf(g_file, sizeof(g_file), "%s.csv", g_base);
  if (g_manual) snprintf(g_origin, sizeof(g_origin), "Started by hand (console `L1` or Manual), not by a run.");
  else snprintf(g_origin, sizeof(g_origin), "Recorded by the guided run `%s`.", sess);
}

// Writes a whole buffer to a new file. false on error.
bool writeWhole(fs::FS& f, const char* path, const char* a, size_t an, const char* b = nullptr, size_t bn = 0) {
  File o = f.open(path, FILE_WRITE);
  if (!o) return false;
  bool ok = o.write(reinterpret_cast<const uint8_t*>(a), an) == an;
  if (ok && b) ok = o.write(reinterpret_cast<const uint8_t*>(b), bn) == bn;
  o.close();
  return ok;
}

bool startInternal(bool manualStart) {
  if (g_w.load() != W_IDLE) { logger::fail(g_w.load() == W_STOP_REQ ? "Raw data: still writing" : "Raw data already on"); return false; }
  if (!g_buf) { logger::fail("Raw data: no buffer"); return false; }
  // SD ONLY (2026-11-26): writing to the flash locks the sensor ISRs (storage.h).
  if (storage::info(storage::SD_CARD).state != storage::READY) { logger::fail("No SD"); return false; }
  g_media = storage::SD_CARD;
  const int8_t pct = storage::info(g_media).pct;
  if (pct >= RAWLOG_STOP_PCT) { logger::fail("Storage full"); return false; }

  g_manual = manualStart;   // buildName() writes the origin from it
  buildHeader();
  buildName();
  g_head.store(0); g_tail.store(0);
  g_written.store(0);
  g_err.store(nullptr);
  g_dropped = g_gapBytes = 0;
  g_ctrl = {INT16_MIN, INT16_MIN, 0xFF, 0xFF};
  g_vescSeen = vesc::values().count;
  g_rec = true;
  g_enc.begin(micros());
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

bool exists3(fs::FS& f, const char* base) {
  char p[sizeof(g_base) + 8];
  for (const char* ext : {".csv", ".json", ".md"}) { snprintf(p, sizeof(p), "%s%s", base, ext); if (f.exists(p)) return true; }
  return false;
}

// <base>.json and <base>.md first, then the stream <base>.csv.
bool openFile(fs::FS& f) {
  // Never overwrite: same name after reboot without clock -> suffix.
  char base[sizeof(g_base)];
  strncpy(base, g_base, sizeof(base));
  storage::makeDir(f, g_dir);
  for (char c = 'b'; exists3(f, base) && c <= 'z'; c++) snprintf(base, sizeof(base), "%.64s-%c", g_base, c);
  strncpy(g_base, base, sizeof(g_base));
  snprintf(g_file, sizeof(g_file), "%s.csv", g_base);
  const char* name = g_base + strlen(g_dir) + 1;   // raw_<session>[-b]
  char csvName[48], mdName[48], p[sizeof(g_base) + 8];
  snprintf(csvName, sizeof(csvName), "%s.csv", name);
  snprintf(mdName, sizeof(mdName), "%s.md", name);
  char files[160];
  const size_t fn = calc::rawcsv::headerFiles(files, sizeof(files) - 1, csvName, mdName);
  files[fn] = ',';
  snprintf(p, sizeof(p), "%s.json", g_base);
  if (!fn || !writeWhole(f, p, files, fn + 1, g_hdr + 1, g_hdrLen - 1)) return false;
  const size_t ln = calc::rawcsv::legend(g_leg, sizeof(g_leg), name, g_origin);
  snprintf(p, sizeof(p), "%s.md", g_base);
  if (ln) writeWhole(f, p, g_leg, ln);   // the legend is a convenience: no reason to refuse the log
  g_fh = f.open(g_file, FILE_WRITE);
  if (!g_fh) return false;
  g_written.store(0);
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
    g_autoRefused = runWantsRaw();   // do not restart and fail again every round
    logger::fail(e);
  }

  // Automatically with the guided run.
  if (!g_manual) {
    const bool want = runWantsRaw();
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
      for (uint32_t i = 0; i < n; i++) put(us[i], [&](char* o, size_t k) { return g_enc.edge(o, k, us[i], ch[i]); });
      if (n < 32) break;
    }
  }
  if (!g_rec) return;

  const uint32_t nowUs = micros();
  if (nowMs - g_lastSync >= RAWLOG_SYNC_MS) { sync(nowUs); g_ctrl.phase = 0xFF; }   // CTRL with every SYNC

  if (loadcellFresh) {
    const int32_t raw = torque::lastRaw();
    put(nowUs, [&](char* o, size_t k) { return g_enc.hx711(o, k, nowUs, raw); });
  }

  const vesc::Values& v = vesc::values();
  if (v.count != g_vescSeen) {
    g_vescSeen = v.count;
    put(v.rxUs, [&](char* o, size_t k) {
      return g_enc.vesc(o, k, v.rxUs, v.erpm, v.tacho, v.raw.duty1000, v.raw.iMot100, v.raw.iIn100,
                        v.raw.vIn10, v.raw.tFet10, v.raw.tMot10, v.fault, v.status);
    });
  }

  const uint32_t age = tirepressure::ageMs();
  if (age != UINT32_MAX && age < g_tireAge) {
    const float b = tirepressure::bar(), t = tirepressure::tempC(), bt = tirepressure::battery();
    const uint16_t mbar = isnan(b) ? 0xFFFF : static_cast<uint16_t>(lroundf(b * 1000.0f));
    const int16_t  t10  = isnan(t) ? INT16_MIN : static_cast<int16_t>(lroundf(t * 10.0f));
    const uint8_t  bat  = isnan(bt) ? 0xFF : static_cast<uint8_t>(lroundf(bt));
    put(nowUs, [&](char* o, size_t k) { return g_enc.tire(o, k, nowUs, mbar, t10, bat); });
  }
  g_tireAge = age;

  const Ctrl c = {static_cast<int16_t>(lroundf(speedctl::targetKmh() * 100.0f)),
                  static_cast<int16_t>(lroundf(speedctl::duty() * 10000.0f)),
                  static_cast<uint8_t>(brake::engaged() ? 1 : 0),
                  static_cast<uint8_t>(state::phase())};
  if (c.target != g_ctrl.target || c.duty != g_ctrl.duty || c.brake != g_ctrl.brake || c.phase != g_ctrl.phase) {
    g_ctrl = c;
    put(nowUs, [&](char* o, size_t k) { return g_enc.ctrl(o, k, nowUs, c.target, c.duty, c.brake, c.phase); });
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
  put(us, [&](char* o, size_t k) { return g_enc.note(o, k, us, s); });
}

}  // namespace rawlog
