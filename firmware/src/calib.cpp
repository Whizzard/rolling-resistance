#include "calib.h"

#include "config.h"
#include "logger.h"
#include "rawlog.h"
#include "revsrc.h"
#include "run.h"
#include "state.h"
#include "storage.h"
#include "torque.h"
#include "wallclock.h"
#include "build_id.h"
#include "calc/timefmt.h"

namespace calib {
namespace {

struct Pt { float kg, counts, sigma; uint32_t n; };
St       g_st = OFF;
Pt       g_pt[CAL_MAX_POINTS]; uint8_t g_n = 0;
float    g_pendingKg = 0.0f;            // mass of the reading in progress
uint32_t g_t0 = 0, g_reads0 = 0;
// Result of the evaluation
float    g_npc = NAN, g_a = NAN, g_b = NAN, g_lin = NAN, g_hyst = NAN, g_zero = NAN, g_sps = NAN;
float    g_res[CAL_MAX_POINTS];         // residual per point, % FS

bool allowed() {
  const float k = revsrc::kmh();
  // Outside a run, or in the run's check-weight step (wheel lifted, drum standing): there
  // the run offers Calibrate when the check weight deviates.
  const bool runOk = !run::busy() || run::step() == 5;   // 5 = S_CTRL (run.cpp)
  return runOk && state::phase() == state::IDLE && !isnan(k) && k < 0.5f && torque::isResponding();
}
float forceN(float kg) { return kg * 9.80665f * settings::calibArmM() / settings::torqueArmM(); }

void beginReading(float kg) {
  g_pendingKg = kg; g_t0 = millis(); g_reads0 = torque::readsTotal();
  torque::sampleBegin(); g_st = READING;
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
  g_lin = 0;
  for (uint8_t i = 0; i < g_n; i++) {
    g_res[i] = (g_pt[i].counts - (g_a + g_b * forceN(g_pt[i].kg))) / fs * 100.0f;
    if (g_pt[i].kg > 0 && fabsf(g_res[i]) > g_lin) g_lin = fabsf(g_res[i]);
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

// Calibration book on the SD: one table row per calibration, header when new.
void appendBook(const settings::CalRecord& r, float oldNpc) {
  fs::FS* f = storage::fs(storage::SD_CARD);
  if (!f || storage::info(storage::SD_CARD).state != storage::READY) { logger::note("Calibration book: no SD, record in flash only"); return; }
  storage::lockSd();
  char p[48]; snprintf(p, sizeof(p), "%s/calibration.md", storage::ROOT_DIR);
  const bool fresh = !f->exists(p);
  File o = f->open(p, FILE_APPEND);
  if (o) {
    if (fresh) {
      o.println("# Calibration book");
      o.println();
      o.println("Written by the stand (firmware calib.cpp), one row per calibration. % FS = of the");
      o.println("cell's full scale. Linearity: largest residual of a load from the fitted line.");
      o.println();
      o.println("| Time | Method | Loads | Factor N/count | Before | Linearity % FS | Hysteresis % FS | Zero return % FS | L_cal mm | Cell arm mm | HX711 SPS (measured) | Gain pulses | Firmware |");
      o.println("|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    }
    char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
    char line[400];
    snprintf(line, sizeof(line), "| %s | %s | %u | %.8f | %.8f | %.3f | %.3f | %.3f | %.1f | %.1f | %u (%.1f) | %u | %s |",
             t, r.method, r.nLoads, r.npc, oldNpc, r.linPctFS, r.hystPctFS, r.zeroPctFS,
             r.lcalM * 1000.0f, r.armM * 1000.0f, r.sps, r.spsMeas, r.gainPulses, FW_COMMIT);
    // NAN as a dash, not "nan"
    String s(line); s.replace("nan", "—");
    o.println(s);
    o.close();
  }
  storage::unlockSd();
}

bool store(const char* method, uint8_t loads, uint8_t points) {
  const float old = settings::loadcellNPerCount();
  if (!settings::setLoadcellNPerCount(g_npc, wallclock::now())) { logger::fail("Calibration: factor not stored"); return false; }
  settings::CalRecord r = {};
  r.ver = 1; r.gainPulses = HX711_GAIN_PULSES; r.sps = HX711_RATE_SPS; r.epoch = wallclock::now();
  r.npc = g_npc; r.armM = settings::torqueArmM(); r.lcalM = settings::calibArmM(); r.spsMeas = g_sps;
  r.linPctFS = g_lin; r.hystPctFS = g_hyst; r.zeroPctFS = g_zero; r.nLoads = loads; r.nPoints = points;
  snprintf(r.method, sizeof(r.method), "%s", method);
  settings::setCalRecord(r);
  appendBook(r, old);
  char t[120];
  snprintf(t, sizeof(t), "Calibrated (%s): %.4e N/count, before %.4e", method, g_npc, old);
  logger::note(t);
  rawlog::text(t);
  return true;
}

}  // namespace

St state() { return g_st; }

bool command(const char* t) {
  if (!t || !t[0]) return false;
  if (t[0] == 'x') { g_st = OFF; g_n = 0; torque::sampleStop(); logger::note("Calibration cancelled"); return true; }
  if (!allowed()) { logger::fail("Calibration: only at standstill, outside a run, cell responding"); return false; }
  switch (t[0]) {
    case '0':
      g_n = 0; g_npc = NAN; beginReading(0.0f); return true;
    case 'm': case 'z': {
      if (g_st != READY && g_st != RESULT) { logger::fail(g_st == READING ? "Calibration: still reading" : "Calibration: start with C0"); return false; }
      if (g_n >= CAL_MAX_POINTS) { logger::fail("Calibration: too many readings"); return false; }
      const float kg = t[0] == 'z' ? 0.0f : static_cast<float>(atof(t + 1));
      if (t[0] == 'm' && !(kg > 0 && forceN(kg) <= LOADCELL_CAPACITY_N)) { logger::fail("Calibration: mass implausible"); return false; }
      beginReading(kg); return true; }
    case 'e':
      if (g_st != READY && g_st != RESULT) { logger::fail("Calibration: nothing to evaluate"); return false; }
      if (!evaluate()) return false;
      g_st = RESULT; return true;
    case 'a': {
      if (g_st != RESULT) { logger::fail("Calibration: evaluate first"); return false; }
      uint8_t loads = 0; for (uint8_t i = 0; i < g_n; i++) if (g_pt[i].kg > 0) loads++;
      store("procedure", loads, g_n);
      g_st = OFF; g_n = 0; return true; }
    default: return false;
  }
}

void service() {
  if (g_st != READING) return;
  if (!allowed()) { g_st = OFF; torque::sampleStop(); logger::fail("Calibration aborted: drum moving or run started"); return; }
  if (millis() - g_t0 < CAL_AVG_MS) return;
  Pt& p = g_pt[g_n++];
  p.kg = g_pendingKg; p.counts = torque::sampleMean(); p.sigma = torque::sampleSigma(); p.n = torque::sampleN();
  g_sps = (torque::readsTotal() - g_reads0) * 1000.0f / (millis() - g_t0);
  torque::sampleStop();
  if (isnan(p.counts)) { g_n--; g_st = READY; logger::fail("Calibration: no values read"); return; }
  g_st = READY;
}

bool singlePoint(float npc) {
  if (g_st != OFF) { logger::fail("Calibration procedure open: finish or cancel it"); return false; }
  g_npc = npc; g_lin = g_hyst = g_zero = NAN; g_sps = NAN;
  return store("single point (k)", 1, 1);
}

int8_t validity(char* why, size_t n) {
  const settings::CalRecord& r = settings::calRecord();
  if (!settings::calibrated()) { snprintf(why, n, "not calibrated: Setup → Calibrate"); return -1; }
  if (r.ver != 1) { snprintf(why, n, "%.4g N/count, conditions unknown (from before the calibration record): calibrate in Setup", settings::loadcellNPerCount()); return 0; }
  char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
  if (fabsf(r.armM - settings::torqueArmM()) > 0.0001f) { snprintf(why, n, "cell arm now %.1f mm, calibrated at %.1f mm: recalibrate", settings::torqueArmM() * 1000.0f, r.armM * 1000.0f); return -1; }
  if (fabsf(r.lcalM - settings::calibArmM()) > 0.0001f) { snprintf(why, n, "calibration arm now %.1f mm, calibrated at %.1f mm: recalibrate", settings::calibArmM() * 1000.0f, r.lcalM * 1000.0f); return -1; }
  if (r.sps != HX711_RATE_SPS || r.gainPulses != HX711_GAIN_PULSES) { snprintf(why, n, "HX711 now %u SPS / gain %u, calibrated at %u / %u: recalibrate", (unsigned)HX711_RATE_SPS, (unsigned)HX711_GAIN_PULSES, (unsigned)r.sps, (unsigned)r.gainPulses); return -1; }
  snprintf(why, n, "%.4g N/count · %s, %s", r.npc, t, r.method);
  return 1;
}

size_t json(char* out, size_t n) {
  if (g_st == OFF) return 0;
  size_t p = snprintf(out, n, ",\"calib\":{\"st\":%u,\"ms\":%lu,\"avgMs\":%lu,\"pts\":[", (unsigned)g_st,
                      (unsigned long)(g_st == READING ? millis() - g_t0 : 0), (unsigned long)CAL_AVG_MS);
  for (uint8_t i = 0; i < g_n && p < n; i++)
    p += snprintf(out + p, n - p, "%s[%.4f,%.1f,%.1f,%s]", i ? "," : "", g_pt[i].kg, g_pt[i].counts, g_pt[i].sigma,
                  g_st == RESULT ? String(g_res[i], 3).c_str() : "null");
  if (p < n) p += snprintf(out + p, n - p, "],\"reading\":%s", g_st == READING ? String(g_pendingKg, 4).c_str() : "null");
  if (p < n && g_st == RESULT)
    p += snprintf(out + p, n - p, ",\"npc\":%.4e,\"old\":%.4e,\"lin\":%.3f,\"hyst\":%s,\"zero\":%s",
                  g_npc, settings::loadcellNPerCount(), g_lin,
                  isnan(g_hyst) ? "null" : String(g_hyst, 3).c_str(), isnan(g_zero) ? "null" : String(g_zero, 3).c_str());
  if (p < n) p += snprintf(out + p, n - p, ",\"masses\":[");
  for (uint8_t i = 0; i < sizeof(CAL_MASSES_G) / sizeof(CAL_MASSES_G[0]) && p < n; i++) p += snprintf(out + p, n - p, "%s%u", i ? "," : "", (unsigned)CAL_MASSES_G[i]);
  if (p < n) p += snprintf(out + p, n - p, "]}");
  return p < n ? p : 0;
}

void describe(Print& o) {
  const settings::CalRecord& r = settings::calRecord();
  o.print("| Calibration record | ");
  if (r.ver != 1) { o.println("none (factor from before 2026-11-26, conditions unknown) | settings |"); return; }
  char t[24]; calc::fmtTime(r.epoch, t, sizeof(t), true);
  o.print(t); o.print(", "); o.print(r.method); o.print(", "); o.print(r.nLoads); o.print(" load(s)");
  if (!isnan(r.linPctFS)) { o.print(", linearity "); o.print(r.linPctFS, 3); o.print(" % FS"); }
  if (!isnan(r.hystPctFS)) { o.print(", hysteresis "); o.print(r.hystPctFS, 3); o.print(" % FS"); }
  if (!isnan(r.zeroPctFS)) { o.print(", zero return "); o.print(r.zeroPctFS, 3); o.print(" % FS"); }
  o.print(", HX711 "); o.print(r.sps); o.print(" SPS gain pulses "); o.print(r.gainPulses);
  o.print(", L_cal "); o.print(r.lcalM * 1000.0f, 1); o.print(" mm, cell arm "); o.print(r.armM * 1000.0f, 1);
  o.println(" mm | settings, calibration book /vmrr/calibration.md |");
}

}  // namespace calib
