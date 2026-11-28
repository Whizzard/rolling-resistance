#include "calib.h"

#include "config.h"
#include "logger.h"
#include "rawlog.h"
#include "run.h"
#include "storage.h"
#include "torque.h"
#include "wallclock.h"
#include "build_id.h"
#include "calc/timefmt.h"

namespace calib {
namespace {

constexpr float G = 9.80665f;

struct Pt { float kg, counts, sigma; uint32_t n; };
St       g_st = OFF;
Pt       g_pt[CAL_MAX_POINTS]; uint8_t g_n = 0;
float    g_pendingKg = 0.0f;            // mass of the reading in progress
uint32_t g_t0 = 0, g_reads0 = 0;
// Result of the evaluation
float    g_npc = NAN, g_a = NAN, g_b = NAN, g_lin = NAN, g_linLoad = NAN, g_hyst = NAN, g_zero = NAN, g_sps = NAN;
float    g_res[CAL_MAX_POINTS];         // residual per point, % FS
float    g_resLoad[CAL_MAX_POINTS];     // residual per load, % of that load (NAN for a zero)

// The arm is lifted, so neither the drum nor a run matters -- except that a run must not
// lose its zero to a calibration in between.
bool allowed() { return !run::busy() && torque::isResponding(); }
float forceN(float kg) { return kg * G; }

void beginReading(float kg) {
  g_pendingKg = kg; g_t0 = millis(); g_reads0 = torque::readsTotal();
  torque::sampleBegin();
  g_st = READING;
}

bool evaluate() {
  // Least squares counts = a + b F.
  double sx = 0, sy = 0, sxx = 0, sxy = 0; uint8_t loads = 0;
  for (uint8_t i = 0; i < g_n; i++) {
    const double x = forceN(g_pt[i].kg), y = g_pt[i].counts;
    sx += x; sy += y; sxx += x * x; sxy += x * y; if (g_pt[i].kg > 0) loads++;
  }
  const double d = g_n * sxx - sx * sx;
  if (loads == 0 || g_n < 2 || fabs(d) < 1e-9) { logger::fail("Calibration: needs a zero and at least one load"); return false; }
  g_b = static_cast<float>((g_n * sxy - sx * sy) / d);
  g_a = static_cast<float>((sy - g_b * sx) / g_n);
  if (!(fabsf(g_b) > 0)) { logger::fail("Calibration: no deflection"); return false; }
  g_npc = 1.0f / fabsf(g_b);
  const float fs = fabsf(g_b) * LOADCELL_CAPACITY_N;   // counts at full scale
  g_lin = 0; g_linLoad = 0;
  for (uint8_t i = 0; i < g_n; i++) {
    const float r = g_pt[i].counts - (g_a + g_b * forceN(g_pt[i].kg));
    g_res[i] = r / fs * 100.0f;
    g_resLoad[i] = g_pt[i].kg > 0 ? r / (fabsf(g_b) * forceN(g_pt[i].kg)) * 100.0f : NAN;
    if (g_pt[i].kg > 0) {
      if (fabsf(g_res[i]) > g_lin) g_lin = fabsf(g_res[i]);
      if (fabsf(g_resLoad[i]) > g_linLoad) g_linLoad = fabsf(g_resLoad[i]);
    }
  }
  // Hysteresis: spread of readings of the same mass (zero included).
  g_hyst = NAN;
  for (uint8_t i = 0; i < g_n; i++)
    for (uint8_t j = i + 1; j < g_n; j++)
      if (fabsf(g_pt[i].kg - g_pt[j].kg) < 1e-4f) {
        const float h = fabsf(g_pt[i].counts - g_pt[j].counts) / fs * 100.0f;
        if (isnan(g_hyst) || h > g_hyst) g_hyst = h;
      }
  // Zero return: last zero minus first zero.
  int8_t z0 = -1, z1 = -1;
  for (uint8_t i = 0; i < g_n; i++) if (g_pt[i].kg == 0) { if (z0 < 0) z0 = i; z1 = i; }
  g_zero = (z0 >= 0 && z1 > z0) ? (g_pt[z1].counts - g_pt[z0].counts) / fs * 100.0f : NAN;
  if (!settings::npcPlausible(g_npc)) { logger::fail("Calibration: factor implausible"); return false; }
  return true;
}

// Calibration book table: one row per calibration. A book started before the
// direct calibration gets a new table with its own header below the old one.
const char* const BOOK_HEAD =
  "| Time | Method | Loads | Max load kg | Factor N/count | Before | Linearity % FS | Linearity % of load | Hysteresis % FS | Zero return % FS | Cell arm mm | HX711 SPS (measured) | Gain pulses | Firmware |";
const char* const BOOK_RULE = "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|";

// Calibration book: one table row per calibration, header when new. Kept on the SD AND
// mirrored in the flash (builder 2026-11-28: the history must survive without a card, to
// see drift over time). Calibrating happens at standstill outside a run, so the flash
// write is allowed (storage.h).
bool writeBook(fs::FS* f, const settings::CalRecord& r, float oldNpc, float maxKg) {
  if (!f) return false;
  storage::makeDir(*f, storage::ROOT_DIR);
  char p[48]; snprintf(p, sizeof(p), "%s/calibration.md", storage::ROOT_DIR);
  bool headed = false;
  const bool fresh = !f->exists(p);
  if (!fresh) {   // does the book already carry the current table header?
    File in = f->open(p, FILE_READ);
    if (in) { headed = in.find("Linearity % of load"); in.close(); }
  }
  File o = f->open(p, FILE_APPEND);
  if (!o) return false;
  if (fresh) {
    o.println("# Calibration book");
    o.println();
    o.println("Written by the stand (firmware calib.cpp), one row per calibration, on the SD and");
    o.println("mirrored in the flash. % FS = of the cell's full scale. Linearity: largest residual");
    o.println("of a load from the fitted line.");
  }
  if (!headed) {
    o.println();
    o.println("## Direct calibration at the cell (since 2026-11-28)");
    o.println();
    o.println("Arm lifted, stirrup on the Bowden cable (in the zero), weights in the stirrup: F = m g.");
    o.println();
    o.println(BOOK_HEAD);
    o.println(BOOK_RULE);
  }
  char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
  char line[420];
  snprintf(line, sizeof(line), "| %s | %s | %u | %.3f | %.8f | %.8f | %.3f | %.3f | %.3f | %.3f | %.1f | %u (%.1f) | %u | %s |",
           t, r.method, r.nLoads, maxKg, r.npc, oldNpc, r.linPctFS, r.linPctLoad, r.hystPctFS, r.zeroPctFS,
           r.armM * 1000.0f, r.sps, r.spsMeas, r.gainPulses, FW_COMMIT);
  String s(line); s.replace("nan", "—");   // NAN as a dash, not "nan"
  o.println(s);
  o.close();
  return true;
}

// ---- Keeping the two books equal (builder 2026-11-28): whatever one side lacks is
// added from the other. A row is one table line of a calibration; rows are compared as
// text (both sides are written by writeBook with the same format). The side with more rows
// is the base, the other side's missing rows are appended to it, and the result is written
// to both (temporary file, then renamed). Runs when the SD appears (each mount) and the
// flash may be written (standstill), outside a run.
const char* BOOK_PATH = "/vmrr/calibration.md";
const char* BOOK_TMP  = "/vmrr/calibration.tmp";

String readAll(fs::FS* f) {
  String out;
  if (!f || !f->exists(BOOK_PATH)) return out;
  File in = f->open(BOOK_PATH, FILE_READ);
  if (!in) return out;
  out = in.readString();
  in.close();
  return out;
}
bool isRow(const String& l) {
  return l.startsWith("| ") && l.indexOf("---") < 0 && !l.startsWith("| Time |");
}
// Rows of b that a lacks, each with its line end.
String missingRows(const String& a, const String& b) {
  String add;
  int i = 0;
  while (i < (int)b.length()) {
    int e = b.indexOf('\n', i); if (e < 0) e = b.length();
    String l = b.substring(i, e); l.trim();
    if (isRow(l) && a.indexOf(l) < 0) { add += l; add += "\n"; }
    i = e + 1;
  }
  return add;
}
int rowCount(const String& t) {
  int n = 0, i = 0;
  while (i < (int)t.length()) {
    int e = t.indexOf('\n', i); if (e < 0) e = t.length();
    String l = t.substring(i, e); l.trim();
    if (isRow(l)) n++;
    i = e + 1;
  }
  return n;
}
bool writeAll(fs::FS* f, const String& t) {
  storage::makeDir(*f, storage::ROOT_DIR);
  File o = f->open(BOOK_TMP, FILE_WRITE);
  if (!o) return false;
  const size_t n = o.print(t);
  o.close();
  if (n != t.length()) { f->remove(BOOK_TMP); return false; }
  f->remove(BOOK_PATH);
  return f->rename(BOOK_TMP, BOOK_PATH);
}
uint16_t g_bookMountSeen = 0xFFFF;   // storage::sdMounts() already synced
void syncBooks() {
  if (storage::info(storage::SD_CARD).state != storage::READY || storage::info(storage::FLASH).state != storage::READY) return;
  if (!storage::flashWritable() || run::busy() || storage::sdMounts() == g_bookMountSeen) return;
  fs::FS* sd = storage::fs(storage::SD_CARD); fs::FS* fl = storage::fs(storage::FLASH);
  storage::lockSd();
  const String a = readAll(sd), b = readAll(fl);
  bool ok = true;
  if (a != b) {
    const String toA = missingRows(a, b), toB = missingRows(b, a);
    if (toA.length() || toB.length()) {
      // base: the side with more rows (SD on a tie); a side without a book takes the other whole
      const bool baseSd = a.length() && (rowCount(a) >= rowCount(b) || !b.length());
      String m = baseSd ? a : b;
      if (!m.endsWith("\n")) m += "\n";
      m += baseSd ? toA : toB;
      ok = writeAll(sd, m) && writeAll(fl, m);
      char t[90]; snprintf(t, sizeof(t), "Calibration book synced: %d row(s) to SD, %d to flash",
                           rowCount(toA), rowCount(toB));
      logger::note(ok ? t : "Calibration book: sync failed");
    }
  }
  storage::unlockSd();
  g_bookMountSeen = storage::sdMounts();   // also after a failure: no retry loop, the next mount or calibration tries again
}

void appendBook(const settings::CalRecord& r, float oldNpc, float maxKg) {
  bool sd = false, fl = false;
  if (storage::info(storage::SD_CARD).state == storage::READY) {
    storage::lockSd();
    sd = writeBook(storage::fs(storage::SD_CARD), r, oldNpc, maxKg);
    storage::unlockSd();
  }
  if (storage::info(storage::FLASH).state == storage::READY && storage::flashWritable())
    fl = writeBook(storage::fs(storage::FLASH), r, oldNpc, maxKg);
  g_bookMountSeen = 0xFFFF;   // compare again at the next chance: one side may have missed the row
  if (!sd || !fl) logger::note(!sd && !fl ? "Calibration book: not written, record in settings only"
                                          : !sd ? "Calibration book: no SD, flash only" : "Calibration book: flash busy, SD only");
}

bool store(uint8_t loads, uint8_t points) {
  const float old = settings::loadcellNPerCount();
  if (!settings::setLoadcellNPerCount(g_npc, wallclock::now())) { logger::fail("Calibration: factor not stored"); return false; }
  settings::CalRecord r = {};
  r.ver = 2; r.gainPulses = HX711_GAIN_PULSES; r.sps = HX711_RATE_SPS; r.epoch = wallclock::now();
  r.npc = g_npc; r.armM = settings::torqueArmM(); r.lcalM = NAN; r.spsMeas = g_sps;
  r.linPctFS = g_lin; r.linPctLoad = g_linLoad; r.hystPctFS = g_hyst; r.zeroPctFS = g_zero; r.nLoads = loads; r.nPoints = points;
  snprintf(r.method, sizeof(r.method), "%s", "direct");
  settings::setCalRecord(r);
  float maxKg = 0; for (uint8_t i = 0; i < g_n; i++) if (g_pt[i].kg > maxKg) maxKg = g_pt[i].kg;
  appendBook(r, old, maxKg);
  char t[120];
  snprintf(t, sizeof(t), "Calibrated (direct): %.4e N/count, before %.4e", g_npc, old);
  logger::note(t);
  rawlog::text(t);
  return true;
}

}  // namespace

St state() { return g_st; }

bool command(const char* t) {
  if (!t || !t[0]) return false;
  if (t[0] == 'x') { g_st = OFF; g_n = 0; torque::sampleStop(); return true; }
  if (!allowed()) { logger::fail("Calibration: outside a run, cell responding"); return false; }
  switch (t[0]) {
    case '0':
      g_n = 0; g_npc = NAN; beginReading(0.0f); return true;
    case 'm': case 'z': {
      if (g_st != READY && g_st != RESULT) { logger::fail(g_st == READING ? "Calibration: still reading" : "Calibration: start with C0"); return false; }
      if (g_n >= CAL_MAX_POINTS) { logger::fail("Calibration: too many readings"); return false; }
      const float kg = t[0] == 'z' ? 0.0f : static_cast<float>(atof(t + 1));
      if (t[0] == 'm' && !(kg > 0 && forceN(kg) <= LOADCELL_CAPACITY_N)) { logger::fail("Calibration: mass implausible"); return false; }
      g_npc = NAN;   // a new reading invalidates an evaluation
      beginReading(kg); return true; }
    case 'e':
      if (g_st != READY && g_st != RESULT) { logger::fail("Calibration: nothing to evaluate"); return false; }
      if (!evaluate()) return false;
      g_st = RESULT; return true;
    case 'a': {
      if (g_st != RESULT) { logger::fail("Calibration: evaluate first"); return false; }
      uint8_t loads = 0; for (uint8_t i = 0; i < g_n; i++) if (g_pt[i].kg > 0) loads++;
      store(loads, g_n);
      g_st = OFF; g_n = 0; return true; }
    default: return false;
  }
}

void service() {
  if (g_st == OFF) syncBooks();
  if (g_st != READING) return;
  if (!allowed()) { g_st = OFF; torque::sampleStop(); logger::fail("Calibration aborted: run started or cell silent"); return; }
  if (millis() - g_t0 < CAL_AVG_MS) return;
  Pt& p = g_pt[g_n++];
  p.kg = g_pendingKg; p.counts = torque::sampleMean(); p.sigma = torque::sampleSigma(); p.n = torque::sampleN();
  g_sps = (torque::readsTotal() - g_reads0) * 1000.0f / (millis() - g_t0);
  torque::sampleStop();
  if (isnan(p.counts)) { g_n--; g_st = READY; logger::fail("Calibration: no values read"); return; }
  g_st = READY;
}

int8_t validity(char* why, size_t n) {
  const settings::CalRecord& r = settings::calRecord();
  if (!settings::calibrated()) { snprintf(why, n, "not calibrated: Setup → Calibrate"); return -1; }
  if (r.ver == 0) { snprintf(why, n, "%.4g N/count, conditions unknown (no calibration record): calibrate in Setup", settings::loadcellNPerCount()); return 0; }
  if (r.ver == 1) { snprintf(why, n, "%.4g N/count, lever calibration (before 2026-11-28): recalibrate directly", settings::loadcellNPerCount()); return 0; }
  char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
  if (r.sps != HX711_RATE_SPS || r.gainPulses != HX711_GAIN_PULSES) { snprintf(why, n, "HX711 now %u SPS / gain %u, calibrated at %u / %u: recalibrate", (unsigned)HX711_RATE_SPS, (unsigned)HX711_GAIN_PULSES, (unsigned)r.sps, (unsigned)r.gainPulses); return -1; }
  snprintf(why, n, "%.4g N/count · %s, %s", r.npc, t, r.method);
  return 1;
}

namespace {
String num(float v, unsigned int d) { return isnan(v) ? String("null") : String(v, d); }
}

size_t json(char* out, size_t n) {
  if (g_st == OFF) return 0;
  size_t p = snprintf(out, n, ",\"calib\":{\"st\":%u,\"ms\":%lu,\"avgMs\":%lu,\"pts\":[", (unsigned)g_st,
                      (unsigned long)(g_st == READING ? millis() - g_t0 : 0), (unsigned long)CAL_AVG_MS);
  // per point: kg, counts, sigma, residual % FS, residual % of the load (both null before
  // evaluating), number of values (the UI draws the scatter of the mean, sigma / sqrt(n))
  for (uint8_t i = 0; i < g_n && p < n; i++)
    p += snprintf(out + p, n - p, "%s[%.4f,%.1f,%.1f,%s,%s,%lu]", i ? "," : "", g_pt[i].kg, g_pt[i].counts, g_pt[i].sigma,
                  g_st == RESULT ? num(g_res[i], 3).c_str() : "null", g_st == RESULT ? num(g_resLoad[i], 3).c_str() : "null",
                  static_cast<unsigned long>(g_pt[i].n));
  if (p < n) p += snprintf(out + p, n - p, "],\"reading\":%s", g_st == READING ? String(g_pendingKg, 4).c_str() : "null");
  if (p < n && g_st == RESULT)
    p += snprintf(out + p, n - p, ",\"npc\":%.4e,\"old\":%.4e,\"a\":%.2f,\"b\":%.4f,\"lin\":%.3f,\"linLoad\":%.3f,\"hyst\":%s,\"zero\":%s",
                  g_npc, settings::loadcellNPerCount(), g_a, g_b, g_lin, g_linLoad, num(g_hyst, 3).c_str(), num(g_zero, 3).c_str());
  if (p < n) p += snprintf(out + p, n - p, ",\"g\":%.5f,\"fs\":%.2f,\"masses\":[", G, LOADCELL_CAPACITY_N);
  for (uint8_t i = 0; i < sizeof(CAL_MASSES_G) / sizeof(CAL_MASSES_G[0]) && p < n; i++) p += snprintf(out + p, n - p, "%s%u", i ? "," : "", (unsigned)CAL_MASSES_G[i]);
  if (p < n) p += snprintf(out + p, n - p, "]}");
  return p < n ? p : 0;
}

void describe(Print& o) {
  const settings::CalRecord& r = settings::calRecord();
  o.print("| Calibration record | ");
  if (r.ver == 0) { o.println("none (factor from before 2026-11-26, conditions unknown) | settings |"); return; }
  char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
  o.print(t); o.print(", "); o.print(r.method); o.print(", "); o.print(r.nLoads); o.print(" load(s)");
  if (r.ver == 1) o.print(" (lever calibration over the calibration arm, before 2026-11-28)");
  if (!isnan(r.linPctFS)) { o.print(", linearity "); o.print(r.linPctFS, 3); o.print(" % FS"); }
  if (r.ver == 2 && !isnan(r.linPctLoad)) { o.print(" / "); o.print(r.linPctLoad, 3); o.print(" % of load"); }
  if (!isnan(r.hystPctFS)) { o.print(", hysteresis "); o.print(r.hystPctFS, 3); o.print(" % FS"); }
  if (!isnan(r.zeroPctFS)) { o.print(", zero return "); o.print(r.zeroPctFS, 3); o.print(" % FS"); }
  o.print(", HX711 "); o.print(r.sps); o.print(" SPS gain pulses "); o.print(r.gainPulses);
  if (r.ver == 1) { o.print(", L_cal "); o.print(r.lcalM * 1000.0f, 1); o.print(" mm"); }
  o.print(", cell arm "); o.print(r.armM * 1000.0f, 1);
  o.println(" mm | settings, calibration book /vmrr/calibration.md (SD and flash) |");
}

}  // namespace calib
