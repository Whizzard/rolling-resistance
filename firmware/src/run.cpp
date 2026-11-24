#include "run.h"
#include <LittleFS.h>
#include <stdarg.h>
#include <new>
#include "config.h"
#include "settings.h"
#include "state.h"
#include "torque.h"
#include "speedctl.h"
#include "revsrc.h"
#include "vesc/vesc.h"
#include "wheel.h"
#include "logger.h"
#include "tirepressure.h"

namespace run {
namespace {

// ------------------------------------------------------------- Schritte
enum Step : uint8_t { S_OFF = 0, S_META = 1, S_VISUAL, S_CHECKS, S_TARE, S_CTRL, S_BASE,
                      S_WHEEL, S_READY, S_WARMUP, S_PLATEAUS, S_ANCHOR, S_STOP, S_POST,
                      S_REPORT, S_ABORT = 99 };
Step     g_step = S_OFF;
uint8_t  g_sub = 0;
uint32_t g_subMs = 0;          // seit wann im Unterschritt
LiveFn   g_live = nullptr;

// ------------------------------------------------------------- Laufsatz
struct Meta {
  char tire[40] = "", wheel[32] = "", note[64] = "";
  float bar = NAN, tempC = NAN, tempEndC = NAN, loadKg = NAN;
  int posMm = 0;
  uint32_t epoch = 0, epochMs = 0;   // Unixzeit vom Browser und millis() dabei
};
struct Check { const char* name; int8_t res; char val[56]; };  // res 1 ok, 0 faellig, -1 fehlt
struct Point {
  float v = NAN, vSd = NAN, m = NAN, mRaw = NAN, mInert = NAN, dvdtNet = NAN, sd = NAN, ripple = NAN, uSync = NAN;
  float imot = NAN, vin = NAN, tfet = NAN, tmot = NAN, dvdt = NAN, bar = NAN, tTire = NAN;
  uint8_t plateau = 0, k = 0, revs = 0; bool ok = false, realRevs = false; uint32_t tMs = 0;
};
struct Warm { uint8_t min; float m, p, crr, bar, tTire; };

constexpr uint8_t  CHECK_N = 14;
constexpr uint8_t  WARN_N  = 16;
constexpr uint8_t  WARM_N  = WARMUP_MS / 60000 + 1;
constexpr uint8_t  MEAS_N  = PLATEAU_MAX * RUN_POINTS_PER_PLATEAU;

struct Record {
  char     id[24] = "";
  Meta     meta;
  Check    checks[CHECK_N];
  uint8_t  checkN = 0;
  float    tareSigma = NAN, noisePp = NAN;
  float    tareCheckN = NAN;             // Nullpruefung durch Weiterdrehen, auf F_roll
  bool     tareCheckOk = false;
  float    ctrlNm = NAN, ctrlDev = NAN;   // Kontrollgewicht: Ist und Abweichung %
  bool     ctrlSkipped = false;
  Point    pre[PLATEAU_MAX];  uint8_t preN = 0;
  Point    post[PLATEAU_MAX]; uint8_t postN = 0;
  float    a0 = NAN, b0 = NAN, a0Old = NAN, b0Old = NAN;   // Leerlaufmodell dieses Laufs und das davor
  bool     baseTaken = false;   // uebernommen statt gefahren
  bool     wheelUsed = false;
  Warm     warm[WARM_N]; uint8_t warmN = 0;
  float    barWarm = NAN, tTireWarm = NAN;   // Druck und Reifentemperatur am Ende des Warmlaufs
  Point    meas[MEAS_N]; uint8_t measN = 0;
  Point    anchorPre[RUN_POINTS_PER_PLATEAU]; uint8_t anchorPreN = 0;   // Referenzpunkt direkt nach dem Warmlauf
  Point    anchor[RUN_POINTS_PER_PLATEAU]; uint8_t anchorN = 0;         // derselbe Punkt nach den Plateaus
  float    anchorKmh = NAN;
  float    zeroEndN = NAN;               // Nullpunkt nach dem Auslauf, Rad abgehoben, auf F_roll
  char     warn[WARN_N][80]; uint8_t warnN = 0;
  char     abortWhy[64] = "";
  uint32_t startMs = 0, endMs = 0, stopMs = 0;
  float    plateaus[PLATEAU_MAX]; uint8_t plateauN = 0;
  float    seq[PLATEAU_MAX + 1]; uint8_t seqN = 0;   // Referenzpunkt + Plateaus fuer die Messfahrt
  float    loadN = NAN;
  char     fileMd[40] = "", fileCsv[40] = "";
};
Record g_r;

// ------------------------------------------------------------- Hilfen
void warn(const char* code, const char* fmt, ...) {
  if (g_r.warnN >= WARN_N) return;
  char* dst = g_r.warn[g_r.warnN++];
  const int n = snprintf(dst, 80, "%s ", code);
  va_list ap; va_start(ap, fmt);
  vsnprintf(dst + n, 80 - n, fmt, ap);
  va_end(ap);
  logger::note(dst);
}

void addCheck(const char* name, int8_t res, const char* fmt, ...) {
  if (g_r.checkN >= CHECK_N) return;
  Check& c = g_r.checks[g_r.checkN++];
  c.name = name; c.res = res;
  va_list ap; va_start(ap, fmt);
  vsnprintf(c.val, sizeof(c.val), fmt, ap);
  va_end(ap);
}

void go(Step s, uint8_t sub = 0) { g_step = s; g_sub = sub; g_subMs = millis(); }
void sub(uint8_t s) { g_sub = s; g_subMs = millis(); }
uint32_t inSubMs() { return millis() - g_subMs; }

// Unixzeit jetzt, 0 ohne Browserzeit.
uint32_t epochNow() {
  return g_r.meta.epoch ? g_r.meta.epoch + (millis() - g_r.meta.epochMs) / 1000 : 0;
}
// Unixzeit -> "YYYY-MM-DD HH:MM" (Howard Hinnants civil_from_days); der Browser schickt
// die Zeit bereits als Ortszeit in Sekunden.
void fmtTime(uint32_t t, char* out, size_t n, bool withTime) {
  if (!t) { snprintf(out, n, "—"); return; }
  const uint32_t days = t / 86400, rem = t % 86400;
  const int64_t z = static_cast<int64_t>(days) + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t y = static_cast<int64_t>(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  const int64_t yy = y + (m <= 2 ? 1 : 0);
  if (withTime) snprintf(out, n, "%04lld-%02lu-%02lu %02lu:%02lu", static_cast<long long>(yy),
                         static_cast<unsigned long>(m), static_cast<unsigned long>(d),
                         static_cast<unsigned long>(rem / 3600), static_cast<unsigned long>(rem % 3600 / 60));
  else snprintf(out, n, "%04lld-%02lu-%02lu-%02lu%02lu", static_cast<long long>(yy),
                static_cast<unsigned long>(m), static_cast<unsigned long>(d),
                static_cast<unsigned long>(rem / 3600), static_cast<unsigned long>(rem % 3600 / 60));
}

float omegaOf(float kmh) { return kmh / 3.6f / settings::drumRadiusM(); }
float rDrum() { return settings::drumRadiusM(); }
// Rohwert-Mittel der Stichprobe als Moment ueber der Tara, Nm. NAN ohne Kalibrierung.
float sampleOffsetNm() {
  if (!settings::calibrated() || torque::sampleN() == 0) return NAN;
  return (torque::sampleMean() - torque::tareOffset()) * settings::loadcellNPerCount() * settings::torqueArmM();
}

// Verlustmodelle fuer die Korrektur: Leerlauf aus diesem Lauf (oder uebernommen),
// Rad aus den Einstellungen mit dem gespeicherten Umfang.
float lossNm(float kmh) {
  const float w = omegaOf(kmh);
  float m = 0.0f;
  if (!isnan(g_r.a0)) m += g_r.a0 + g_r.b0 * w * w;
  if (g_r.wheelUsed) {
    const float uRad = settings::wheelCircM();
    const float rRad = uRad / (2.0f * PI);
    const float wRad = w * rDrum() / rRad;
    m += (settings::wheelA1() + settings::wheelB1() * wRad * wRad) * rDrum() / rRad;
  }
  return m;
}
float crrRaw(float mNm) { return (isnan(mNm) || isnan(g_r.loadN)) ? NAN : mNm / rDrum() / g_r.loadN; }
float crrKor(float kmh, float mNm) { return isnan(mNm) ? NAN : crrRaw(mNm - lossNm(kmh)); }

// Fit M = a + b w^2 ueber Punkte
bool fitAB(const Point* p, uint8_t n, float& a, float& b) {
  double sx = 0, sy = 0, sxx = 0, sxy = 0; uint8_t k = 0;
  for (uint8_t i = 0; i < n; i++) {
    if (isnan(p[i].m)) continue;
    const double x = omegaOf(p[i].v); const double xx = x * x;
    sx += xx; sy += p[i].m; sxx += xx * xx; sxy += xx * p[i].m; k++;
  }
  const double den = k * sxx - sx * sx;
  if (k < 3 || fabs(den) < 1e-12) return false;
  b = static_cast<float>((k * sxy - sx * sy) / den);
  a = static_cast<float>((sy - b * sx) / k);
  return true;
}

// Mittel der gueltigen Punkte einer Liste, Nm. NAN ohne gueltige.
float meanM(const Point* p, uint8_t n, uint8_t& cnt) {
  double s = 0; cnt = 0;
  for (uint8_t i = 0; i < n; i++) if (p[i].ok && !isnan(p[i].m)) { s += p[i].m; cnt++; }
  return cnt ? static_cast<float>(s / cnt) : NAN;
}

// Gepoolte Standardabweichung der Punkte innerhalb der Plateaus (docs/12 W5): mit
// drei Punkten je Plateau ist die einzelne Streuung auf +-50 % unsicher, gepoolt
// ueber alle Plateaus hat sie Sum(n_i - 1) Freiheitsgrade.
float pooledSd(uint8_t& dof) {
  double ssq = 0; dof = 0;
  for (uint8_t pl = 0; pl < g_r.plateauN; pl++) {
    double s = 0, ss = 0; uint8_t n = 0;
    for (uint8_t i = 0; i < g_r.measN; i++) {
      const Point& p = g_r.meas[i];
      if (p.plateau != pl || !p.ok || isnan(p.m)) continue;
      s += p.m; ss += p.m * p.m; n++;
    }
    if (n > 1) { ssq += ss - s * s / n; dof += n - 1; }
  }
  return dof ? static_cast<float>(sqrt(ssq / dof)) : NAN;
}

// Leerlaufunsicherheit auf F_roll: groesste Drift Vor-/Nachlauf, sonst das Kriterium.
float baselineUncN() {
  if (!g_r.postN) return RUN_DRIFT_LIMIT_N;
  float u = 0;
  for (uint8_t i = 0; i < g_r.postN && i < g_r.preN; i++) {
    const float d = fabsf(g_r.post[i].m - g_r.pre[i].m) / rDrum(); if (d > u) u = d; }
  return u;
}

// Statistik je Plateau ueber die Messpunkte
struct PlStat { float v, m, sd, froll, croh, ckor, u, uA, mInert; uint8_t n, all, dof; };
PlStat plateauStat(uint8_t plateau) {
  PlStat s = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, 0, 0, 0};
  double sm = 0, smm = 0, sv = 0, sdIn = 0, sSync = 0, sIn = 0; uint8_t n = 0;
  for (uint8_t i = 0; i < g_r.measN; i++) {
    const Point& p = g_r.meas[i];
    if (p.plateau != plateau) continue;
    s.all++;
    if (!p.ok || isnan(p.m)) continue;
    sm += p.m; smm += p.m * p.m; sv += p.v; n++;
    if (!isnan(p.sd)) sdIn += p.sd / sqrtf(p.revs ? p.revs : 1);
    if (!isnan(p.uSync)) sSync += p.uSync;
    if (!isnan(p.mInert)) sIn += fabsf(p.mInert);
  }
  s.n = n;
  if (!n) return s;
  s.m = static_cast<float>(sm / n); s.v = static_cast<float>(sv / n);
  if (n > 1) { const double var = (smm - sm * sm / n) / (n - 1); s.sd = var > 0 ? static_cast<float>(sqrt(var)) : 0.0f; }
  s.mInert = static_cast<float>(sIn / n);
  const float mR = s.m - lossNm(s.v);
  s.froll = mR / rDrum();
  s.croh = crrRaw(s.m);
  s.ckor = crrRaw(mR);
  // Unsicherheit (docs/11 R9, docs/12 D3), k = 1:
  //   Typ A    gepoolte Streuung der Punkte ueber alle Plateaus / sqrt(n); ohne
  //            Pool die Streuung innerhalb des Punkts
  //   Typ B    Kalibrierfaktor, Kalibrierarm, Auflast, Reifendruck, Trommellager
  //   Leerlauf Drift Vor-/Nachlauf, sonst das Kriterium
  //   Rad      gemessenes oder geschaetztes J_Rad
  //   Sync     Rest der Mittelung ohne echten Index (ripple/2/N)
  //   Traegheit UNC_J_REL vom abgezogenen Scheinmoment
  uint8_t dof = 0;
  const float sPool = pooledSd(dof);
  s.dof = dof;
  s.uA = !isnan(sPool) ? sPool / sqrtf(n)
       : (n > 1 && !isnan(s.sd)) ? s.sd / sqrtf(n) : static_cast<float>(sdIn / n);
  const float uLeerNm = baselineUncN() * rDrum();
  const float uRad = g_r.wheelUsed ? (wheel::jRadMeasured() ? UNC_RAD_REL : UNC_RAD_EST_REL) : 0.0f;
  const float rel = sqrtf(sq(s.uA / mR) + sq(UNC_K_REL) + sq(UNC_LCAL_REL) + sq(UNC_N_REL)
                        + sq(UNC_P_REL) + sq(UNC_BEARING_REL)
                        + sq(uLeerNm / mR) + sq(uRad)
                        + sq(static_cast<float>(sSync / n) / mR)
                        + sq(UNC_J_REL * s.mInert / mR));
  s.u = fabsf(s.ckor) * rel;
  return s;
}

// ------------------------------------------------------------- Rueckrufe der Sequenz
void fromStats(Point& p, const state::PointStats& st) {
  p.v = st.v; p.vSd = st.vSd; p.m = st.mNm; p.mRaw = st.mRawNm; p.mInert = st.mInertNm; p.dvdtNet = st.dvdtNet;
  p.sd = st.sdNm; p.ripple = st.rippleNm; p.uSync = st.uSyncNm; p.imot = st.imot; p.vin = st.vin;
  p.tfet = st.tfet; p.tmot = st.tmot; p.dvdt = st.dvdtMax; p.plateau = st.plateau; p.k = st.k;
  p.revs = st.revs; p.ok = st.ok; p.realRevs = st.realRevs; p.tMs = st.tMs;
  p.bar = tirepressure::bar(); p.tTire = tirepressure::tempC();
}
void onPrePoint(const state::PointStats& st)  { if (g_r.preN  < PLATEAU_MAX) fromStats(g_r.pre[g_r.preN++], st); }
void onPostPoint(const state::PointStats& st) { if (g_r.postN < PLATEAU_MAX) fromStats(g_r.post[g_r.postN++], st); }
// Messfahrt: Sequenzindex 0 ist der Referenzpunkt, danach die Plateaus.
void onMeasPoint(const state::PointStats& st) {
  if (st.plateau == 0) {
    if (g_r.anchorPreN < RUN_POINTS_PER_PLATEAU) fromStats(g_r.anchorPre[g_r.anchorPreN++], st);
    return;
  }
  if (g_r.measN >= MEAS_N) return;
  Point& p = g_r.meas[g_r.measN++];
  fromStats(p, st);
  p.plateau = st.plateau - 1;
  if (!p.ok) warn("W03", "Punkt %.1f km/h #%u ungueltig: dv/dt netto %.3f km/h/s", p.v, p.k + 1, p.dvdtNet);
}
void onAnchorPoint(const state::PointStats& st) { if (g_r.anchorN < RUN_POINTS_PER_PLATEAU) fromStats(g_r.anchor[g_r.anchorN++], st); }

void writeFiles();
void abortRun(const char* why);
void seqDone(bool completed);
void finishRun() { writeFiles(); go(S_REPORT); }

// Sequenz ueber alle Plateaus, Leerlauf (ein Punkt je Plateau).
bool startBaselineSeq(state::PointFn fn) {
  return state::startSequence(state::RUN_BASELINE, g_r.plateaus, g_r.plateauN, 1, false, fn, seqDone);
}

void anchorDrift(float& dNm, bool& have) {
  uint8_t n0 = 0, n1 = 0;
  const float m0 = meanM(g_r.anchorPre, g_r.anchorPreN, n0), m1 = meanM(g_r.anchor, g_r.anchorN, n1);
  have = n0 && n1; dNm = have ? m1 - m0 : NAN;
}

void seqDone(bool completed) {
  if (!completed) {
    if (g_step == S_BASE || g_step == S_PLATEAUS || g_step == S_ANCHOR || g_step == S_POST)
      abortRun(state::phase() == state::FAULT ? state::faultReason() : "gestoppt");
    return;
  }
  switch (g_step) {
    case S_BASE:
      if (fitAB(g_r.pre, g_r.preN, g_r.a0, g_r.b0)) settings::setBaseline(g_r.a0, g_r.b0);
      else warn("W12", "Leerlaufmodell aus dem Vorlauf nicht bestimmbar");
      state::stopRun(); sub(3);   // bremsen, dann Rad absenken
      break;
    case S_PLATEAUS:
      go(S_ANCHOR);
      if (!state::startSequence(state::RUN_MEASURE, &g_r.anchorKmh, 1, RUN_POINTS_PER_PLATEAU, false, onAnchorPoint, seqDone))
        abortRun(logger::lastNote());
      break;
    case S_ANCHOR: {
      float d; bool have; anchorDrift(d, have);
      if (have && fabsf(d) / rDrum() > RUN_ANCHOR_WARN_N)
        warn("W04", "Ankerdrift %.3f N auf F_roll zwischen Referenzpunkt vor und nach den Plateaus", fabsf(d) / rDrum());
      go(S_STOP);
      g_r.stopMs = millis();
      state::stopRun();
      break;
    }
    case S_POST: {
      float worst = 0;
      for (uint8_t i = 0; i < g_r.postN && i < g_r.preN; i++) {
        const float d = fabsf(g_r.post[i].m - g_r.pre[i].m) / rDrum();
        if (d > worst) worst = d;
      }
      if (g_r.preN && worst > RUN_DRIFT_LIMIT_N) warn("W08", "Leerlaufdrift %.3f N ueber %.2f N: Reihe verwerfen", worst, RUN_DRIFT_LIMIT_N);
      state::stopRun(); sub(3);
      break;
    }
    default: break;
  }
}

// ------------------------------------------------------------- Systempruefung
void runChecks() {
  g_r.checkN = 0;
  const vesc::Values& v = vesc::values();
  if (!vesc::connected()) addCheck("Regler", -1, "kein Regler");
  else if (v.fault) addCheck("Regler", -1, "Fehler: %s", vesc::faultText(v.fault));
  else addCheck("Regler", 1, "FW %u.%02u · %.1f V · kein Fehler", vesc::fwMajor(), vesc::fwMinor(), v.vIn);
  addCheck("Motor am Regler", (speedctl::motorOpen() || speedctl::estopInput()) ? -1 : 1,
           speedctl::motorOpen() ? "Trennung erkannt" : speedctl::estopInput() ? "Not-Aus" : "Trennung ruhig");
  if (!torque::isResponding()) addCheck("Waegezelle", -1, "HX711 antwortet nicht");
  else if (torque::isSaturated()) addCheck("Waegezelle", -1, "am Anschlag");
  else {
    const float fN = torque::newtons();
    if (isnan(fN)) addCheck("Waegezelle", 1, "antwortet");
    else addCheck("Waegezelle", 1, "antwortet · %.1f %% vom Endwert", fabsf(fN) / LOADCELL_CAPACITY_N * 100.0f);
  }
  addCheck("Rauschen Stillstand", 1, "protokolliert: Spitze-Spitze %.0f Counts (%lu s), Grenze TBD", g_r.noisePp, static_cast<unsigned long>(RUN_NOISE_MS / 1000));
  if (settings::calibrated()) addCheck("Kalibrierfaktor", 1, "%.4g N/Count", settings::loadcellNPerCount());
  else addCheck("Kalibrierfaktor", -1, "nicht kalibriert (TBD)");
  // Der Faktor gilt nur fuer den Zellenarm, mit dem kalibriert wurde (docs/12 D2).
  if (!isnan(settings::armAtCalM()) && fabsf(settings::armAtCalM() - settings::torqueArmM()) > 0.0001f)
    addCheck("Zellenarm", -1, "L jetzt %.1f mm, kalibriert bei %.1f mm: neu kalibrieren", settings::torqueArmM() * 1000.0f, settings::armAtCalM() * 1000.0f);
  else addCheck("Zellenarm", 1, "%.1f mm · L_kal %.1f mm%s", settings::torqueArmM() * 1000.0f, settings::calibArmM() * 1000.0f,
                isnan(settings::armAtCalM()) ? " (Arm bei Kalibrierung unbekannt)" : "");
  const uint32_t ta = settings::tareAgeMs();
  if (ta == UINT32_MAX) addCheck("Tara", 0, "aus dem Speicher, Alter unbekannt → Schritt 4");
  else addCheck("Tara", 0, "vor %lu min → Schritt 4 erneuert", static_cast<unsigned long>(ta / 60000));
  if (!settings::haveBaseline()) addCheck("Leerlaufmodell", -1, "fehlt → Schritt 6");
  else if (settings::baselineAgeMs() == UINT32_MAX) addCheck("Leerlaufmodell", 0, "aus dem Speicher, Alter unbekannt → Schritt 6");
  else addCheck("Leerlaufmodell", 1, "vor %lu min · A0 %.4f · B0 %.2e", static_cast<unsigned long>(settings::baselineAgeMs() / 60000), settings::baselineA0(), settings::baselineB0());
  if (!settings::haveWheelModel()) addCheck("Radmodell", -1, "fehlt → Einrichtung, Assistent Auslauf");
  else addCheck("Radmodell", wheel::jRadMeasured() ? 1 : 0, "A1 %.5f · B1 %.2e · J %.4f kg·m² %s", settings::wheelA1(), settings::wheelB1(), wheel::jRadKgm2(),
                wheel::jRadMeasured() ? "gemessen" : "GESCHAETZT (Bifilar fehlt, u ±3,5 %)");
  if (isnan(settings::wheelCircM())) addCheck("Radumfang", -1, "nie gemessen");
  else addCheck("Radumfang", 1, "%.0f mm", settings::wheelCircM() * 1000.0f);
  addCheck("Umdrehungen", revsrc::realRevolutions() || !speedctl::running() ? 1 : 0, "Quelle %s%s", revsrc::sourceName(),
           revsrc::realRevolutions() ? "" : " (ohne Index: gerechnete Umdrehungen, Restfehler im Budget)");
  if (tirepressure::source() == tirepressure::SRC_BLE && tirepressure::fresh())
    addCheck("Reifendruck", 1, "%s · %.2f bar · %.1f °C · Batterie %.0f %%", tirepressure::name(), tirepressure::bar(), tirepressure::tempC(), tirepressure::battery());
  else if (!isnan(g_r.meta.bar)) addCheck("Reifendruck", 1, "Handeingabe %.2f bar (kein AirSpy)", g_r.meta.bar);
  else addCheck("Reifendruck", -1, "weder AirSpy noch Eingabe");
  if (!g_r.plateauN) addCheck("Plateaus", -1, "keine Messgeschwindigkeiten");
  else {
    char l[48]; size_t p = 0;
    for (uint8_t i = 0; i < g_r.plateauN && p < sizeof(l) - 8; i++) p += snprintf(l + p, sizeof(l) - p, "%s%.1f", i ? " · " : "", g_r.plateaus[i]);
    addCheck("Plateaus", 1, "%s km/h, Referenz %.1f davor und danach", l, WARMUP_KMH);
  }
  addCheck("Auflast", isnan(g_r.loadN) ? -1 : 1, isnan(g_r.loadN) ? "nicht gesetzt" : "%.1f N", g_r.loadN);
}

bool checksBlock() {
  for (uint8_t i = 0; i < g_r.checkN; i++) if (g_r.checks[i].res < 0) return true;
  return false;
}

// ------------------------------------------------------------- Protokoll
void pf(Print& o, float v, uint8_t d) { if (isnan(v)) o.print("—"); else o.print(v, d); }

void writeMd(Print& o) {
  char t[24];
  o.print("# Messlauf "); o.println(g_r.id);
  o.println();
  o.print("Reifen: "); o.print(g_r.meta.tire); o.print(" · Laufrad: "); o.print(g_r.meta.wheel);
  o.print(" · Druck kalt: "); pf(o, g_r.meta.bar, 2); o.print(" bar · nach Warmlauf: "); pf(o, g_r.barWarm, 2); o.print(" bar");
  o.print(" · Auflast: "); pf(o, g_r.meta.loadKg, 1);
  o.print(" kg ("); pf(o, g_r.loadN, 1); o.print(" N) · Laufgewicht: "); o.print(g_r.meta.posMm); o.println(" mm  ");
  o.print("Raumtemperatur Start: "); pf(o, g_r.meta.tempC, 1); o.print(" °C · Ende: "); pf(o, g_r.meta.tempEndC, 1);
  o.print(" °C · Reifentemperatur nach Warmlauf: "); pf(o, g_r.tTireWarm, 1); o.print(" °C · Start ");
  fmtTime(g_r.meta.epoch, t, sizeof(t), true); o.print(t); o.print(" · Ende ");
  fmtTime(epochNow(), t, sizeof(t), true); o.print(t);
  o.print(" · Dauer "); o.print((g_r.endMs - g_r.startMs) / 60000); o.print(" min · Firmware " __DATE__ " " __TIME__);
  o.print(" · Regler FW "); o.print(vesc::fwMajor()); o.print("."); o.print(vesc::fwMinor());
  o.print(" · Drehzahlquelle "); o.println(revsrc::sourceName());
  if (g_r.meta.note[0]) { o.print("Notiz: "); o.println(g_r.meta.note); }
  if (g_r.abortWhy[0]) { o.println(); o.print("**ABGEBROCHEN: "); o.print(g_r.abortWhy); o.println("**"); }
  o.println();
  o.println("## Pruefungen");
  o.println("| Pruefung | Ergebnis | Wert |"); o.println("|---|---|---|");
  for (uint8_t i = 0; i < g_r.checkN; i++) {
    const Check& c = g_r.checks[i];
    o.print("| "); o.print(c.name); o.print(" | "); o.print(c.res > 0 ? "ok" : c.res == 0 ? "faellig, erledigt" : "FEHLT");
    o.print(" | "); o.print(c.val); o.println(" |");
  }
  o.print("| Nullen, Rad abgehoben | ok | σ = "); pf(o, g_r.tareSigma, 1); o.println(" Counts |");
  o.print("| Nullpruefung Weiterdrehen | "); o.print(isnan(g_r.tareCheckN) ? "nicht gemacht" : (g_r.tareCheckOk ? "ok" : "ABWEICHUNG (W13)"));
  o.print(" | "); if (!isnan(g_r.tareCheckN)) { pf(o, g_r.tareCheckN, 3); o.print(" N auf F_roll, Grenze "); o.print(RUN_ZERO_LIMIT_N, 2); o.print(" N"); } o.println(" |");
  o.print("| Kontrollgewicht | "); o.print(g_r.ctrlSkipped ? "uebersprungen" : (fabsf(g_r.ctrlDev) < RUN_CTRL_OK_PCT ? "ok" : "Warnung"));
  o.print(" | "); if (g_r.ctrlSkipped) o.print("W01"); else { pf(o, g_r.ctrlNm, 3); o.print(" Nm · "); pf(o, g_r.ctrlDev, 2); o.print(" %"); } o.println(" |");
  o.print("| Nullpunkt nach dem Lauf | "); o.print(isnan(g_r.zeroEndN) ? "nicht geprueft" : (fabsf(g_r.zeroEndN) <= RUN_ZERO_LIMIT_N ? "ok" : "DRIFT (W14)"));
  o.print(" | "); if (!isnan(g_r.zeroEndN)) { pf(o, g_r.zeroEndN, 3); o.print(" N auf F_roll"); } o.println(" |");
  o.println();
  o.println("## Kalibrierstand");
  o.println("| Groesse | Wert | Herkunft |"); o.println("|---|---|---|");
  o.print("| U Trommel | "); o.print(settings::drumCircumferenceM(), 4); o.println(" m | Einstellungen |");
  o.print("| L Zellenarm | "); o.print(settings::torqueArmM(), 4); o.print(" m | Einstellungen, bei Kalibrierung "); pf(o, settings::armAtCalM(), 4); o.println(" m |");
  o.print("| L_kal Kalibrierarm | "); o.print(settings::calibArmM(), 4); o.println(" m | Einstellungen (bestimmt die Momentenkette) |");
  o.print("| N_per_count | "); o.print(settings::loadcellNPerCount(), 8); o.println(" N | Kalibrierung |");
  o.print("| J Trommel | "); o.print(DRUM_INERTIA_KGM2, 3); o.print(" kg·m² ± "); o.print(UNC_J_REL * 100, 0); o.println(" % | config.h, gerechnet; Traegheitskorrektur je Punkt |");
  o.print("| Tara | "); o.print(torque::tareOffset()); o.println(" Counts | Schritt 4 |");
  o.print("| A0 / B0 | "); pf(o, g_r.a0, 5); o.print(" Nm / "); pf(o, g_r.b0, 7); o.print(" | ");
  o.print(g_r.baseTaken ? "uebernommen (W02)" : "Vorlauf dieses Laufs");
  if (!isnan(g_r.a0Old)) { o.print(", davor "); pf(o, g_r.a0Old, 5); o.print(" / "); pf(o, g_r.b0Old, 7); } o.println(" |");
  o.print("| A1 / B1 / J_Rad | ");
  if (g_r.wheelUsed) { o.print(settings::wheelA1(), 5); o.print(" / "); o.print(settings::wheelB1(), 8); o.print(" / "); pf(o, wheel::jRadKgm2(), 4);
    o.print(" kg·m² | Einstellungen, J "); o.print(wheel::jRadMeasured() ? "gemessen (Bifilar)" : "GESCHAETZT (WHEEL_J_FACTOR)"); o.println(" |"); }
  else o.println("— | nicht angewandt (W09) |");
  o.print("| Radumfang | "); pf(o, settings::wheelCircM(), 4); o.println(" m | Drehzahlverhaeltnis |");
  o.print("| Typ B relativ | k "); o.print(UNC_K_REL, 4); o.print(" · L_kal "); o.print(UNC_LCAL_REL, 4); o.print(" · N "); o.print(UNC_N_REL, 4);
  o.print(" · p "); o.print(UNC_P_REL, 4); o.print(" · Lager "); o.print(UNC_BEARING_REL, 4); o.print(" · Rad ");
  o.print(g_r.wheelUsed ? (wheel::jRadMeasured() ? UNC_RAD_REL : UNC_RAD_EST_REL) : 0.0f, 4); o.println(" | config.h |");
  o.println();
  if (g_r.preN) {
    o.println("## Leerlauf Vorlauf");
    o.println("| v km/h | M Nm | M roh Nm | J·dω/dt Nm | σ_U Nm |"); o.println("|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.preN; i++) { const Point& p = g_r.pre[i]; o.print("| "); pf(o, p.v, 1); o.print(" | "); pf(o, p.m, 4); o.print(" | "); pf(o, p.mRaw, 4);
      o.print(" | "); pf(o, p.mInert, 4); o.print(" | "); pf(o, p.sd, 4); o.println(" |"); }
    o.println();
  }
  if (g_r.warmN) {
    o.print("## Warmlauf "); o.print(WARMUP_KMH, 1); o.println(" km/h");
    o.println("| min | M Nm | P W | Crr korr. | p bar | T Reifen °C |"); o.println("|---|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.warmN; i++) { const Warm& w = g_r.warm[i]; o.print("| "); o.print(w.min); o.print(" | "); pf(o, w.m, 4); o.print(" | "); pf(o, w.p, 1);
      o.print(" | "); pf(o, w.crr, 5); o.print(" | "); pf(o, w.bar, 2); o.print(" | "); pf(o, w.tTire, 1); o.println(" |"); }
    if (g_r.warmN >= 6) { const float a = g_r.warm[g_r.warmN - 6].p, b = g_r.warm[g_r.warmN - 1].p;
      o.println(); o.print("Aenderung letzte 5 min: "); pf(o, (b / a - 1.0f) * 100.0f, 1); o.println(" %"); }
    o.println();
  }
  if (g_r.measN || g_r.anchorPreN) {
    o.println("## Messpunkte");
    o.println("Reihenfolge: Referenzpunkt direkt nach dem Warmlauf, dann die Plateaus aufsteigend, dann derselbe Referenzpunkt (Anker). M ist um J·dω/dt korrigiert.");
    o.println();
    o.println("| Teil | v km/h | Punkt | M Nm | M roh Nm | J·dω/dt Nm | dv/dt netto | σ_U Nm | Welligkeit Nm | I_mot A | U_bus V | p bar | T Reifen | gueltig |");
    o.println("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    auto row = [&](const char* kind, const Point& p) {
      o.print("| "); o.print(kind); o.print(" | "); pf(o, p.v, 2); o.print(" | "); o.print(p.k + 1); o.print(" | "); pf(o, p.m, 4); o.print(" | "); pf(o, p.mRaw, 4);
      o.print(" | "); pf(o, p.mInert, 4); o.print(" | "); pf(o, p.dvdtNet, 3); o.print(" | "); pf(o, p.sd, 4); o.print(" | "); pf(o, p.ripple, 3);
      o.print(" | "); pf(o, p.imot, 2); o.print(" | "); pf(o, p.vin, 1); o.print(" | "); pf(o, p.bar, 2); o.print(" | "); pf(o, p.tTire, 1);
      o.print(" | "); o.print(p.ok ? "ja" : "nein"); o.println(" |"); };
    for (uint8_t i = 0; i < g_r.anchorPreN; i++) row("Referenz vor", g_r.anchorPre[i]);
    for (uint8_t i = 0; i < g_r.measN; i++) row("Plateau", g_r.meas[i]);
    for (uint8_t i = 0; i < g_r.anchorN; i++) row("Referenz nach", g_r.anchor[i]);
    o.println();
    o.println("## Ergebnis je Plateau");
    o.println("u(Crr) mit k = 1; Typ A aus der gepoolten Streuung der Punkte ueber alle Plateaus (Freiheitsgrade ν), Typ B aus config.h, Leerlauf aus der Drift Vor/Nach bzw. dem Kriterium.");
    o.println();
    o.println("| v km/h | n | M Nm | σ Nm | F_roll N | Crr roh | Crr korr. | u(Crr) k=1 | ν | u_A/M |"); o.println("|---|---|---|---|---|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.plateauN; i++) { const PlStat s = plateauStat(i);
      o.print("| "); o.print(g_r.plateaus[i], 1); o.print(" | "); o.print(s.n); o.print("/"); o.print(s.all); o.print(" | "); pf(o, s.m, 4); o.print(" | "); pf(o, s.sd, 4);
      o.print(" | "); pf(o, s.froll, 3); o.print(" | "); pf(o, s.croh, 5); o.print(" | "); pf(o, s.ckor, 5); o.print(" | ± "); pf(o, s.u, 5);
      o.print(" | "); o.print(s.dof); o.print(" | "); pf(o, isnan(s.uA) || isnan(s.m) ? NAN : s.uA / (s.m - lossNm(s.v)) * 100.0f, 2); o.println(" % |"); }
    o.println();
  }
  if (g_r.anchorPreN || g_r.anchorN) {
    o.print("## Anker "); o.print(g_r.anchorKmh, 1); o.println(" km/h");
    float d; bool have; anchorDrift(d, have);
    if (have) { o.print("ΔM "); pf(o, d, 4); o.print(" Nm · ΔCrr "); pf(o, crrRaw(d), 5); o.print(" · "); pf(o, fabsf(d) / rDrum(), 3);
      o.print(" N auf F_roll (Grenze "); o.print(RUN_ANCHOR_WARN_N, 2); o.println(" N), nach minus vor"); }
    else o.println("kein gueltiger Vergleich (Referenz vor oder nach fehlt)");
    o.println();
  }
  o.println("## Leerlauf Vor/Nach");
  if (g_r.postN) {
    o.println("| v km/h | vor Nm | nach Nm | Δ auf F_roll N |"); o.println("|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.postN && i < g_r.preN; i++) { o.print("| "); pf(o, g_r.post[i].v, 1); o.print(" | "); pf(o, g_r.pre[i].m, 4); o.print(" | "); pf(o, g_r.post[i].m, 4);
      o.print(" | "); pf(o, fabsf(g_r.post[i].m - g_r.pre[i].m) / rDrum(), 3); o.println(" |"); }
  } else o.println("Nachlauf nicht gefahren (W05).");
  o.println();
  o.println("## Warnungen");
  if (!g_r.warnN) o.println("keine");
  for (uint8_t i = 0; i < g_r.warnN; i++) { o.print("- "); o.println(g_r.warn[i]); }
  o.println();
  o.println("## Rohdaten");
  o.print(g_r.fileCsv); o.println(" · Spalten: kind,plateau,k,t_s,v_kmh,v_sd,m_Nm,m_raw_Nm,m_inert_Nm,sd_Nm,ripple_Nm,dvdt_net,i_mot,v_in,t_fet,t_mot,p_bar,t_tire,revs,real_revs,ok");
}

void csvPoint(Print& o, const char* kind, const Point& p) {
  o.print(kind); o.print(','); o.print(p.plateau); o.print(','); o.print(p.k); o.print(',');
  o.print((p.tMs - g_r.startMs) / 1000); o.print(',');
  pf(o, p.v, 2); o.print(','); pf(o, p.vSd, 3); o.print(','); pf(o, p.m, 5); o.print(','); pf(o, p.mRaw, 5); o.print(',');
  pf(o, p.mInert, 5); o.print(','); pf(o, p.sd, 5); o.print(','); pf(o, p.ripple, 4); o.print(','); pf(o, p.dvdtNet, 4); o.print(',');
  pf(o, p.imot, 2); o.print(','); pf(o, p.vin, 2); o.print(','); pf(o, p.tfet, 1); o.print(','); pf(o, p.tmot, 1); o.print(',');
  pf(o, p.bar, 2); o.print(','); pf(o, p.tTire, 1); o.print(','); o.print(p.revs); o.print(','); o.print(p.realRevs ? 1 : 0); o.print(','); o.println(p.ok ? 1 : 0);
}
void writeCsv(Print& o) {
  o.print("# Messlauf "); o.print(g_r.id); o.print("  U="); o.print(settings::drumCircumferenceM(), 4);
  o.print(" r="); o.print(rDrum(), 4); o.print(" L="); o.print(settings::torqueArmM(), 4); o.print(" L_cal="); o.print(settings::calibArmM(), 4);
  o.print(" N_per_count="); o.print(settings::loadcellNPerCount(), 8); o.print(" load_N="); pf(o, g_r.loadN, 1);
  o.print(" A0="); pf(o, g_r.a0, 5); o.print(" B0="); pf(o, g_r.b0, 7);
  if (g_r.wheelUsed) { o.print(" A1="); o.print(settings::wheelA1(), 6); o.print(" B1="); o.print(settings::wheelB1(), 9);
    o.print(" U_rad="); o.print(settings::wheelCircM(), 4); o.print(" J_rad="); o.print(wheel::jRadKgm2(), 5);
    o.print(" J_rad_src="); o.print(wheel::jRadMeasured() ? "gem" : "gesch"); }
  o.print(" J_drum="); o.print(DRUM_INERTIA_KGM2, 3);
  o.print(" unc_k="); o.print(UNC_K_REL, 4); o.print(" unc_lcal="); o.print(UNC_LCAL_REL, 4); o.print(" unc_n="); o.print(UNC_N_REL, 4);
  o.print(" unc_p="); o.print(UNC_P_REL, 4); o.print(" unc_bearing="); o.print(UNC_BEARING_REL, 4);
  o.print(" unc_rad="); o.print(g_r.wheelUsed ? (wheel::jRadMeasured() ? UNC_RAD_REL : UNC_RAD_EST_REL) : 0.0f, 4);
  o.print(" unc_j="); o.print(UNC_J_REL, 3); o.print(" drift_limit_N="); o.print(RUN_DRIFT_LIMIT_N, 3);
  o.print(" source="); o.println(revsrc::sourceName());
  o.println("kind,plateau,k,t_s,v_kmh,v_sd,m_Nm,m_raw_Nm,m_inert_Nm,sd_Nm,ripple_Nm,dvdt_net,i_mot,v_in,t_fet,t_mot,p_bar,t_tire,revs,real_revs,ok");
  for (uint8_t i = 0; i < g_r.preN; i++)    csvPoint(o, "baseline_pre", g_r.pre[i]);
  for (uint8_t i = 0; i < g_r.warmN; i++) { const Warm& w = g_r.warm[i];
    o.print("warmup,0,"); o.print(w.min); o.print(','); o.print(w.min * 60); o.print(','); o.print(WARMUP_KMH, 2);
    o.print(",,"); pf(o, w.m, 5); o.print(",,,,,,,,,,"); pf(o, w.bar, 2); o.print(','); pf(o, w.tTire, 1); o.println(",,,1"); }
  for (uint8_t i = 0; i < g_r.anchorPreN; i++) csvPoint(o, "anchor_pre", g_r.anchorPre[i]);
  for (uint8_t i = 0; i < g_r.measN; i++)   csvPoint(o, "measure", g_r.meas[i]);
  for (uint8_t i = 0; i < g_r.anchorN; i++) csvPoint(o, "anchor_post", g_r.anchor[i]);
  for (uint8_t i = 0; i < g_r.postN; i++)   csvPoint(o, "baseline_post", g_r.post[i]);
}

void writeFiles() {
  g_r.endMs = millis();
  snprintf(g_r.fileMd, sizeof(g_r.fileMd), "/runs/%s.md", g_r.id);
  snprintf(g_r.fileCsv, sizeof(g_r.fileCsv), "/runs/%s.csv", g_r.id);
  File f = LittleFS.open(g_r.fileMd, "w");
  if (f) { writeMd(f); f.close(); } else { g_r.fileMd[0] = '\0'; logger::note("Protokoll nicht geschrieben: Dateisystem"); }
  f = LittleFS.open(g_r.fileCsv, "w");
  if (f) { writeCsv(f); f.close(); } else g_r.fileCsv[0] = '\0';
  Serial.print("# Protokoll: "); Serial.print(g_r.fileMd); Serial.print("  Daten: "); Serial.println(g_r.fileCsv);
}

void abortRun(const char* why) {
  if (g_step == S_ABORT || g_step == S_OFF) return;
  strncpy(g_r.abortWhy, why ? why : "abgebrochen", sizeof(g_r.abortWhy) - 1);
  warn("W06", "Lauf abgebrochen in Schritt %u: %s", static_cast<unsigned>(g_step), g_r.abortWhy);
  if (speedctl::running()) state::stopRun();
  torque::sampleStop();
  writeFiles();
  go(S_ABORT);
}

// ------------------------------------------------------------- Start
void startRun() {
  g_r.~Record();
  new (&g_r) Record();   // kein Temporaer auf dem Stack der Hauptschleife
  g_r.startMs = millis();
  g_r.plateauN = settings::plateauCount();
  for (uint8_t i = 0; i < g_r.plateauN; i++) g_r.plateaus[i] = settings::plateauKmh(i);
  g_r.loadN = state::loadN();
  g_r.meta.loadKg = isnan(g_r.loadN) ? NAN : g_r.loadN / 9.80665f;
  g_r.meta.bar = tirepressure::bar();
  g_r.a0Old = settings::baselineA0(); g_r.b0Old = settings::baselineB0();
  // Referenzpunkt = Warmlaufgeschwindigkeit; die Plateaus folgen aufsteigend.
  g_r.anchorKmh = WARMUP_KMH;
  g_r.seqN = 0;
  g_r.seq[g_r.seqN++] = g_r.anchorKmh;
  for (uint8_t i = 0; i < g_r.plateauN && g_r.seqN < PLATEAU_MAX + 1; i++) g_r.seq[g_r.seqN++] = g_r.plateaus[i];
  snprintf(g_r.id, sizeof(g_r.id), "lauf-%lu", static_cast<unsigned long>(millis() / 1000));
  go(S_META);
}

void setMeta(char key, const char* val) {
  Meta& m = g_r.meta;
  const float f = atof(val);
  switch (key) {
    case 't': strncpy(m.tire, val, sizeof(m.tire) - 1); break;
    case 'w': strncpy(m.wheel, val, sizeof(m.wheel) - 1); break;
    case 'n': strncpy(m.note, val, sizeof(m.note) - 1); break;
    case 'b': m.bar = f > 0 ? f : NAN; if (f > 0) tirepressure::setManual(f); break;
    case 'c': m.tempC = val[0] ? f : NAN; break;
    case 'e': m.tempEndC = val[0] ? f : NAN; break;
    case 'l': if (f > 0) { m.loadKg = f; g_r.loadN = f * 9.80665f; state::setLoadN(g_r.loadN); } break;
    case 'p': m.posMm = static_cast<int>(f); break;
    case 'd': m.epoch = static_cast<uint32_t>(strtoul(val, nullptr, 10)); m.epochMs = millis();
              fmtTime(m.epoch, g_r.id, sizeof(g_r.id), false); break;
    default: break;
  }
}

// Nullpruefung nach dem Weiterdrehen und Nullpunkt nach dem Lauf: dieselbe Rechnung.
float zeroDeviationN() {
  const float m = sampleOffsetNm();
  return isnan(m) ? NAN : m / rDrum();
}

// ------------------------------------------------------------- Knoepfe
// ok = Hauptknopf, alt = Nebenknopf, redo = Nochmal.
void onOk() {
  switch (g_step) {
    case S_META:
      if (isnan(g_r.meta.tempC)) warn("W07", "Raumtemperatur nicht eingetragen");
      if (tirepressure::source() == tirepressure::SRC_BLE && tirepressure::fresh()) g_r.meta.bar = tirepressure::bar();
      go(S_VISUAL); break;
    case S_VISUAL: go(S_CHECKS); g_r.noisePp = 0; break;
    case S_CHECKS: if (g_sub == 1 && !checksBlock()) go(S_TARE); break;
    case S_TARE:
      if (g_sub == 0) { state::startTare(); sub(1); }                 // Rad frei -> nullen
      else if (g_sub == 2) { torque::sampleBegin(); sub(3); }          // weitergedreht -> messen
      else if (g_sub == 4) go(S_CTRL);                                 // Ergebnis -> weiter
      break;
    case S_CTRL:
      if (g_sub == 0) { torque::sampleBegin(); sub(1); }              // haengt -> messen
      else if (g_sub == 2) go(S_BASE);                                 // Ergebnis -> weiter
      break;
    case S_BASE:
      if (g_sub == 0) sub(1);                       // Neu fahren -> Rad frei bestaetigen
      else if (g_sub == 1) {                        // bestaetigt -> Vorlauf
        g_r.preN = 0;
        if (startBaselineSeq(onPrePoint)) sub(2); else abortRun(logger::lastNote());
      } else if (g_sub == 4) go(S_WHEEL);           // Rad abgesenkt
      break;
    case S_WHEEL:
      if (settings::haveWheelModel() && !isnan(settings::wheelCircM())) { g_r.wheelUsed = true; go(S_READY); }
      break;
    case S_READY:
      if (state::startHold(WARMUP_KMH)) { go(S_WARMUP); g_r.warmN = 0; } else abortRun(logger::lastNote());
      break;
    case S_POST:
      if (g_sub == 0) { torque::sampleBegin(); sub(1); }              // Rad abgehoben -> Nullpunkt pruefen
      break;
    case S_REPORT: case S_ABORT: go(S_OFF); break;
    default: break;
  }
}
void onAlt() {
  switch (g_step) {
    case S_TARE:
      if (g_sub == 4) { warn("W13", "Nullpunkt winkelabhaengig: %.3f N auf F_roll, trotzdem weiter", g_r.tareCheckN); go(S_CTRL); }
      break;
    case S_CTRL:
      if (g_sub == 0) { g_r.ctrlSkipped = true; warn("W01", "Kontrollgewicht uebersprungen"); go(S_BASE); }
      else if (g_sub == 2) { warn("W10", "Kontrollgewicht weicht %.2f %% ab, trotzdem weiter", g_r.ctrlDev); go(S_BASE); }
      break;
    case S_BASE:
      if (g_sub == 0 && settings::haveBaseline()) {
        g_r.a0 = settings::baselineA0(); g_r.b0 = settings::baselineB0(); g_r.baseTaken = true;
        if (settings::baselineAgeMs() == UINT32_MAX) warn("W02", "Leerlaufmodell unbekannten Alters uebernommen");
        go(S_WHEEL);
      }
      break;
    case S_WHEEL: g_r.wheelUsed = false; warn("W09", "ohne Radmodell: Crr enthaelt den Laufradverlust"); go(S_READY); break;
    case S_WARMUP:
      warn("W11", "Warmlauf nach %lu min abgebrochen", static_cast<unsigned long>(inSubMs() / 60000));
      sub(1);   // -> Plateaus
      break;
    case S_POST: if (g_sub == 0) { warn("W05", "Nachlauf uebersprungen: Drift der Leerlaufkennlinie und Nullpunkt ungeprueft"); finishRun(); } break;
    default: break;
  }
}
void onRedo() {
  if (g_step == S_CTRL && g_sub == 2) sub(0);
  if (g_step == S_TARE && g_sub == 4) { g_r.tareCheckN = NAN; sub(0); }
}

}  // namespace

// ------------------------------------------------------------- oeffentlich
void begin(LiveFn live) {
  g_live = live;
  if (!LittleFS.begin(true)) Serial.println("# LittleFS: Start fehlgeschlagen, kein Protokoll im Flash");
  else { if (!LittleFS.exists("/runs")) LittleFS.mkdir("/runs");
    Serial.print("# LittleFS: "); Serial.print(LittleFS.usedBytes() / 1024); Serial.print(" von ");
    Serial.print(LittleFS.totalBytes() / 1024); Serial.println(" kB belegt"); }
}

void service() {
  if (g_step == S_OFF || g_step == S_ABORT || g_step == S_REPORT) return;
  // Ein Fehler der Zustandsmaschine bricht ab, egal in welchem Schritt.
  if (state::phase() == state::FAULT) { abortRun(state::faultReason()); return; }

  switch (g_step) {
    case S_CHECKS:
      if (g_sub == 0) {
        long mean = 0, ripple = 0; torque::displayStats(mean, ripple);
        if (ripple > g_r.noisePp) g_r.noisePp = ripple;
        if (inSubMs() >= RUN_NOISE_MS) { runChecks(); sub(1); }
      }
      break;
    case S_TARE:
      if (g_sub == 1 && state::phase() == state::IDLE && !torque::tareBusy() && inSubMs() > 300) {
        g_r.tareSigma = torque::tareSigma();
        sub(2);   // -> Weiterdrehen
      }
      if (g_sub == 3 && inSubMs() >= RUN_ZERO_AVG_MS) {
        torque::sampleStop();
        g_r.tareCheckN = zeroDeviationN();
        g_r.tareCheckOk = !isnan(g_r.tareCheckN) && fabsf(g_r.tareCheckN) <= RUN_ZERO_LIMIT_N;
        sub(4);
      }
      break;
    case S_CTRL:
      if (g_sub == 1 && inSubMs() >= RUN_CTRL_AVG_MS) {
        torque::sampleStop();
        const float m = sampleOffsetNm();
        const float soll = RUN_CTRL_MASS_KG * 9.80665f * settings::calibArmM();
        g_r.ctrlNm = m; g_r.ctrlDev = (m / soll - 1.0f) * 100.0f;
        sub(2);
      }
      break;
    case S_BASE:
      if (g_sub == 3 && state::phase() == state::IDLE) sub(4);   // steht -> Rad absenken
      break;
    case S_WARMUP:
      if (g_sub == 0) {
        // je Minute ein Live-Punkt; nach WARMUP_MS weiter
        const uint8_t minute = inSubMs() / 60000;
        if (minute >= 1 && g_r.warmN < WARM_N && (g_r.warmN == 0 || g_r.warm[g_r.warmN - 1].min < minute)) {
          LiveVals lv; Warm& w = g_r.warm[g_r.warmN++];
          w.min = minute; w.bar = tirepressure::bar(); w.tTire = tirepressure::tempC();
          if (g_live && g_live(lv)) { w.m = lv.mNm; w.p = lv.pW; w.crr = lv.crr; } else { w.m = w.p = w.crr = NAN; }
        }
        if (inSubMs() >= WARMUP_MS) sub(1);
        if (state::phase() != state::HOLD) { abortRun("Warmlauf unterbrochen"); return; }
      }
      if (g_sub == 1) {
        g_r.barWarm = tirepressure::bar(); g_r.tTireWarm = tirepressure::tempC();
        g_r.measN = 0; g_r.anchorPreN = 0;
        go(S_PLATEAUS);
        if (!state::startSequence(state::RUN_MEASURE, g_r.seq, g_r.seqN, RUN_POINTS_PER_PLATEAU, false, onMeasPoint, seqDone))
          abortRun(logger::lastNote());
      }
      break;
    case S_STOP:
      if (state::phase() == state::IDLE) go(S_POST);
      break;
    case S_POST:
      if (g_sub == 1 && inSubMs() >= RUN_ZERO_AVG_MS) {
        torque::sampleStop();
        g_r.zeroEndN = zeroDeviationN();
        if (!isnan(g_r.zeroEndN) && fabsf(g_r.zeroEndN) > RUN_ZERO_LIMIT_N)
          warn("W14", "Nullpunkt nach dem Lauf um %.3f N auf F_roll verschoben: Drift von Zelle, Kabel oder Vorspannung", g_r.zeroEndN);
        g_r.postN = 0;
        if (startBaselineSeq(onPostPoint)) sub(2); else abortRun(logger::lastNote());
      }
      if (g_sub == 3 && state::phase() == state::IDLE) finishRun();
      break;
    default: break;
  }
}

bool command(const char* t) {
  if (!t) return false;
  switch (t[0]) {
    case '\0':
      if (g_step != S_OFF) { logger::note("Messlauf laeuft schon"); return false; }
      if (state::phase() != state::IDLE) { logger::note("Start abgelehnt: nicht im Zustand IDLE"); return false; }
      startRun(); return true;
    case 'k': onOk(); return true;
    case 's': onAlt(); return true;
    case 'n': onRedo(); return true;
    case 'x':
      if (g_step == S_OFF) return true;
      if (g_step == S_REPORT || g_step == S_ABORT) { go(S_OFF); return true; }
      abortRun("vom Bediener abgebrochen"); return true;
    case 'm':
      if (t[1] && t[2] == '=') { setMeta(t[1], t + 3); return true; }
      return false;
    default: return false;
  }
}

bool active() { return g_step != S_OFF; }
uint8_t step() { return static_cast<uint8_t>(g_step); }

// ------------------------------------------------------------- JSON
namespace {
size_t jnum(char* b, size_t n, float v, uint8_t d) { return isnan(v) ? snprintf(b, n, "null") : snprintf(b, n, "%.*f", d, v); }
size_t jstr(char* b, size_t n, const char* s) {   // Anfuehrungszeichen und Rueckstrich maskiert
  size_t p = 0; if (p < n) b[p++] = '"';
  for (; *s && p + 3 < n; s++) { if (*s == '"' || *s == '\\') b[p++] = '\\'; if (*s == '\n') { b[p++] = ' '; continue; } b[p++] = *s; }
  if (p < n) b[p++] = '"';
  if (p < n) b[p] = '\0';
  return p;
}
#define J(...) do { if (p < len) p += snprintf(buf + p, len - p, __VA_ARGS__); } while (0)
#define JN(v, d) do { if (p < len) p += jnum(buf + p, len - p, (v), (d)); } while (0)
#define JS(s) do { if (p < len) p += jstr(buf + p, len - p, (s)); } while (0)
// [v, k, M, sd, (Crr roh, Crr korr,) ok, M_traeg, dv/dt netto]
void jpoint(char* buf, size_t len, size_t& p, const Point& x, bool withCrr) {
  J("["); JN(x.v, 1); J(",%u,", x.k + 1); JN(x.m, 4); J(","); JN(x.sd, 4); J(",");
  if (withCrr) { JN(crrRaw(x.m), 5); J(","); JN(crrKor(x.v, x.m), 5); J(","); }
  J("%s,", x.ok ? "true" : "false"); JN(x.mInert, 4); J(","); JN(x.dvdtNet, 3); J("]");
}
}  // namespace

void json(char* buf, size_t len) {
  size_t p = 0;
  // Knoepfe je Schritt: ok = Hauptknopf, alt = Nebenknopf, redo = Nochmal; "" = weg.
  const char *ok = "", *alt = "", *redo = "", *title = "", *txt = "";
  switch (g_step) {
    case S_META:   title = "Lauf anlegen"; ok = "Weiter"; break;
    case S_VISUAL: title = "Sichtpruefung"; txt = "Trommel, Wiegeteil frei, Kupplung, Rad gesichert, Blockierkeil raus, Not-Aus griffbereit."; ok = "Geprueft"; break;
    case S_CHECKS: title = "Systempruefung"; if (g_sub == 1) { ok = checksBlock() ? "" : "Weiter"; txt = checksBlock() ? "Etwas fehlt. Unter Einrichtung beheben, dann neu starten." : ""; } else txt = "Rauschen im Stillstand, 5 s."; break;
    case S_TARE:   title = "Nullen";
      if (g_sub == 0) { txt = "Rad mit dem Exzenter ABHEBEN. Trommel von Hand anstossen und auspendeln lassen: der Rotor muss frei in seine Rastlage fallen, sonst steckt das Rastmoment im Nullpunkt."; ok = "Rad frei, Trommel steht"; }
      else if (g_sub == 1) txt = "Trommel steht. 200 Werte.";
      else if (g_sub == 2) { txt = "Trommel von Hand eine Viertelumdrehung weiterdrehen, auspendeln lassen."; ok = "Weitergedreht"; }
      else if (g_sub == 3) txt = "misst";
      else { ok = g_r.tareCheckOk ? "Weiter" : ""; alt = g_r.tareCheckOk ? "" : "Trotzdem weiter"; redo = "Nochmal nullen";
             txt = g_r.tareCheckOk ? "" : "Nullpunkt haengt vom Winkel ab: haelt etwas den Rotor fest (Rad auf der Trommel, Kabel)?"; }
      break;
    case S_CTRL:   title = "Kontrollgewicht";
      if (g_sub == 0) { txt = "1000 g an die Marke des Kalibrierarms haengen, Rad bleibt abgehoben."; ok = "Haengt"; alt = "Ueberspringen"; }
      else if (g_sub == 1) txt = "misst";
      else { ok = fabsf(g_r.ctrlDev) < RUN_CTRL_OK_PCT ? "Weiter" : ""; alt = (fabsf(g_r.ctrlDev) >= RUN_CTRL_OK_PCT && fabsf(g_r.ctrlDev) < RUN_CTRL_WARN_PCT) ? "Trotzdem weiter" : ""; redo = "Nochmal";
             txt = fabsf(g_r.ctrlDev) >= RUN_CTRL_WARN_PCT ? "Abweichung zu gross: Kalibrierung pruefen." : "Gewicht abnehmen."; }
      break;
    case S_BASE:   title = "Leerlaufmodell";
      if (g_sub == 0) { ok = "Neu fahren"; alt = settings::haveBaseline() ? "Uebernehmen" : ""; txt = settings::haveBaseline() ? (settings::baselineAgeMs() == UINT32_MAX ? "Gespeichert, Alter unbekannt." : "Aus dieser Sitzung.") : "Kein Modell vorhanden."; }
      else if (g_sub == 1) { title = "Rad abgehoben?"; txt = "Rad bleibt frei von der Trommel, Kontrollgewicht ist abgenommen."; ok = "Ja, Vorlauf starten"; }
      else if (g_sub == 2) { title = "Leerlauf Vorlauf"; }
      else if (g_sub == 3) { title = "Anhalten"; }
      else { title = "Rad absenken"; txt = "Trommel steht. Rad auf die Trommel."; ok = "Abgesenkt"; }
      break;
    case S_WHEEL:  title = "Radmodell";
      if (settings::haveWheelModel() && !isnan(settings::wheelCircM())) { ok = "Uebernehmen"; alt = "Ohne";
        txt = wheel::jRadMeasured() ? "J_Rad gemessen." : "J_Rad nur geschaetzt (0,85 m r²): Unsicherheit ±3,5 %. Bifilarpendel unter Einrichtung eintragen."; }
      else { txt = "Kein Radmodell. Einrichtung, Assistent Auslauf."; alt = "Ohne weiter"; }
      break;
    case S_READY:  title = "Rad aufgelegt"; txt = "Laufgewicht in Position, Holm waagerecht, Rad mittig, Druck kalt notiert."; ok = "Messfahrt starten"; break;
    case S_WARMUP: title = "Warmlauf"; alt = "Abkuerzen"; break;
    case S_PLATEAUS: title = (state::plateauIndex() == 0) ? "Referenzpunkt" : "Plateaus"; break;
    case S_ANCHOR: title = "Referenzpunkt danach"; break;
    case S_STOP:   title = "Anhalten"; break;
    case S_POST:   title = "Nachlauf Leerlauf";
      if (g_sub == 0) { txt = "Rad abheben. Dann wird zuerst der Nullpunkt geprueft, dann der Leerlauf warm wiederholt. Raumtemperatur jetzt eintragen."; ok = "Abgehoben"; alt = "Ueberspringen"; }
      else if (g_sub == 1) { title = "Nullpunkt"; txt = "misst"; }
      else if (g_sub == 3) title = "Anhalten";
      break;
    case S_REPORT: title = "Protokoll"; ok = "Schliessen"; break;
    case S_ABORT:  title = "Abgebrochen"; txt = g_r.abortWhy; ok = "Schliessen"; break;
    default: break;
  }
  J("{\"step\":%u,\"sub\":%u,\"id\":", static_cast<unsigned>(g_step), static_cast<unsigned>(g_sub)); JS(g_r.id);
  J(",\"title\":"); JS(title); J(",\"txt\":"); JS(txt);
  J(",\"ok\":"); JS(ok); J(",\"alt\":"); JS(alt); J(",\"redo\":"); JS(redo);
  J(",\"subMs\":%lu,\"warmMs\":%lu", static_cast<unsigned long>(inSubMs()), static_cast<unsigned long>(WARMUP_MS));
  J(",\"tareSigma\":"); JN(g_r.tareSigma, 1);
  J(",\"tareChk\":"); JN(g_r.tareCheckN, 3); J(",\"zeroEnd\":"); JN(g_r.zeroEndN, 3); J(",\"zeroLim\":%.3f", RUN_ZERO_LIMIT_N);
  J(",\"ctrlNm\":"); JN(g_r.ctrlNm, 3); J(",\"ctrlDev\":"); JN(g_r.ctrlDev, 2);
  J(",\"ctrlSoll\":%.3f", RUN_CTRL_MASS_KG * 9.80665f * settings::calibArmM());
  J(",\"bar\":"); JN(tirepressure::bar(), 2); J(",\"barWarm\":"); JN(g_r.barWarm, 2); J(",\"tTire\":"); JN(tirepressure::tempC(), 1);
  J(",\"checks\":[");
  for (uint8_t i = 0; i < g_r.checkN; i++) { J("%s[", i ? "," : ""); JS(g_r.checks[i].name); J(",%d,", g_r.checks[i].res); JS(g_r.checks[i].val); J("]"); }
  J("],\"pre\":[");
  for (uint8_t i = 0; i < g_r.preN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.pre[i], false); }
  J("],\"a0\":"); JN(g_r.a0, 5); J(",\"b0\":"); JN(g_r.b0, 7); J(",\"a0Old\":"); JN(g_r.a0Old, 5);
  J(",\"post\":[");
  for (uint8_t i = 0; i < g_r.postN; i++) { J("%s[", i ? "," : ""); JN(g_r.post[i].v, 1); J(","); JN(g_r.post[i].m, 4); J(",");
    JN(i < g_r.preN ? fabsf(g_r.post[i].m - g_r.pre[i].m) / rDrum() : NAN, 3); J("]"); }
  J("],\"warm\":[");
  for (uint8_t i = 0; i < g_r.warmN; i++) { J("%s[%u,", i ? "," : "", g_r.warm[i].min); JN(g_r.warm[i].p, 1); J(","); JN(g_r.warm[i].crr, 5); J(","); JN(g_r.warm[i].bar, 2); J("]"); }
  J("],\"anchorPre\":[");
  for (uint8_t i = 0; i < g_r.anchorPreN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.anchorPre[i], true); }
  J("],\"pts\":[");
  for (uint8_t i = 0; i < g_r.measN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.meas[i], true); }
  J("],\"stat\":[");
  if (g_r.measN) for (uint8_t i = 0; i < g_r.plateauN; i++) { const PlStat s = plateauStat(i);
    J("%s[%.1f,%u,%u,", i ? "," : "", g_r.plateaus[i], s.n, s.all); JN(s.m, 4); J(","); JN(s.sd, 4); J(","); JN(s.croh, 5); J(","); JN(s.ckor, 5); J(","); JN(s.u, 5); J(",%u]", s.dof); }
  J("],\"anchor\":[");
  for (uint8_t i = 0; i < g_r.anchorN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.anchor[i], true); }
  J("],\"anchorKmh\":"); JN(g_r.anchorKmh, 1);
  { float d; bool have; anchorDrift(d, have); J(",\"anchorDN\":"); JN(have ? d / rDrum() : NAN, 3); }
  J(",\"warn\":[");
  for (uint8_t i = 0; i < g_r.warnN; i++) { J("%s", i ? "," : ""); JS(g_r.warn[i]); }
  J("],\"md\":"); JS(g_r.fileMd); J(",\"csv\":"); JS(g_r.fileCsv);
  J(",\"plateaus\":[");
  for (uint8_t i = 0; i < g_r.plateauN; i++) J("%s%.1f", i ? "," : "", g_r.plateaus[i]);
  J("]}");
  if (p >= len) { buf[len - 1] = '\0'; }
}
#undef J
#undef JN
#undef JS

}  // namespace run
