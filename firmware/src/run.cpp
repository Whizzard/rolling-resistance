#include "run.h"
#include "build_id.h"
#include "speed.h"
#include "calib.h"
#include "autozero.h"
#include "wallclock.h"
#include "calc/warmup.h"
#include "calc/json.h"
#include "calc/linfit.h"
#include "calc/loss.h"
#include "calc/timefmt.h"
#include <LittleFS.h>
#include "storage.h"
#include "rawlog.h"
#include "profiles.h"
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

// ------------------------------------------------------------- Steps
enum Step : uint8_t { S_OFF = 0, S_META = 1, S_VISUAL, S_CHECKS, S_TARE, S_CTRL, S_BASE,
                      S_WHEEL, S_READY, S_WARMUP, S_PLATEAUS, S_ANCHOR, S_STOP, S_POST,
                      S_REPORT, S_ABORT = 99 };
Step     g_step = S_OFF;
uint8_t  g_sub = 0;
uint32_t g_subMs = 0;          // since when in the sub-step
LiveFn   g_live = nullptr;

// ------------------------------------------------------------- Run record
struct Meta {
  char tire[40] = "", rim[32] = "", note[64] = "", hub[32] = "";   // rim: "wheel" until 2026-11-26
  char etrto[16] = "", tube[32] = "";   // ETRTO size and inner tube: since 2026-11-25, the tube accounts for up to 14 %
  float bar = NAN, tempC = NAN, tempEndC = NAN, loadKg = NAN;
  float loadCalcKg = NAN, loadMeasKg = NAN;   // calculated from the position / measured with the hanging scale
  int posMm = 0;
  int8_t tireNew = -1;                 // 1 new, 0 already ridden, -1 not stated (since 2026-11-26)
  float treadMm = NAN;                 // measured thickness of the tire (casing + tread), mm, optional
  uint32_t epoch = 0, epochMs = 0;   // Unix time from the browser and millis() at that moment
};
struct Check { const char* name; int8_t res; char val[160]; };  // res 1 ok, 0 due, -1 missing, 2 recorded (no limit yet)
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
  float    tareSigma = NAN;
  float    azMean[AZ_POSITIONS] = {}, azDeg[AZ_POSITIONS] = {};   // per position: raw mean, degrees turned before
  bool     tareAuto = false;
  bool     postAuto = false;              // post-run zero by the motor (autozero.h)
  bool     ctrlAuto = false;              // check-weight step: the reading in progress spins the drum (autozero.h)              // zero by the drum motor at several positions (autozero.h, since 2026-11-27)
  long     tarePp = 0;                    // peak-to-peak of the tare values, counts (the noise record since 2026-11-26)
  long     tareRaw = 0;                   // zero point taken in step 4, raw counts
  bool     tareTaken = false;
  char     rawFile[88] = "";              // raw-data file of this run ("" = none, no SD)
  float    tareCheckN = NAN;             // zero check by turning further, on F_roll
  bool     tareCheckOk = false;
  float    ctrlNm = NAN, ctrlDev = NAN;   // check weight: actual and deviation %
  float    ctrlRaw = NAN, ctrlRaw2 = NAN; // check weight above tare in counts: before / after the end-stop press
  float    ctrlRep = NAN;                 // second reading against the first, %
  bool     ctrlSkipped = false;
  bool     ctrlOnHint = false;            // Next pressed, but the weight is still hanging
  int32_t  turnTacho0 = 0;                // tachometer at the start of turning further
  float    turnDeg = NAN;                 // turned so far, degrees (NAN without controller)
  uint32_t turnStandstillMs = 0;               // since when still after the quarter turn
  bool     turnReady = false;                  // quarter turn done and still long enough: button shown
  float    stopPeakPct = NAN;             // end-stop check: peak in % of the cell's full scale
  bool     stopSkipped = false;
  float    stopZeroN = NAN;               // zero point after the end-stop press, N on F_roll (overload bias)
  Point    pre[PLATEAU_MAX * RUN_BASELINE_POINTS];  uint8_t preN = 0;
  Point    post[PLATEAU_MAX * RUN_BASELINE_POINTS]; uint8_t postN = 0;
  float    a0 = NAN, b0 = NAN, a0Old = NAN, b0Old = NAN;   // idle-loss model of this run and the one before
  bool     baseTaken = false;   // adopted instead of driven
  bool     wheelUsed = false;
  bool     wheelFresh = false;           // wheel model from the pre-run of this run (step 6)
  float    a1Old = NAN, b1Old = NAN;      // wheel model stored before this run, and where it came from
  settings::WheelOrigin woOld = {};
  int      profile = -1;                  // wheel profile of this run (profiles.h), from step 1
  bool     prevSame = false;              // a1Old/b1Old come from this profile (same wheel)
  Warm     warm[WARM_N]; uint8_t warmN = 0;
  uint8_t  warmSettledMin = 0;            // minute at which the warm-up was found settled, 0 = not (ran to the end or skipped)
  LiveVals liveLast = {NAN, NAN, NAN, false}; uint32_t liveLastMs = 0;   // last valid live value in the warm-up
  double   wSumM = 0, wSumP = 0, wSumC = 0; uint32_t wCnt = 0;           // live values of the current warm-up minute
  float    barWarm = NAN, tTireWarm = NAN;   // pressure and tire temperature at the end of the warm-up
  Point    meas[MEAS_N]; uint8_t measN = 0;
  Point    anchorPre[RUN_POINTS_PER_PLATEAU]; uint8_t anchorPreN = 0;   // reference point right after the warm-up
  Point    anchor[RUN_POINTS_PER_PLATEAU]; uint8_t anchorN = 0;         // the same point after the plateaus
  float    anchorKmh = NAN;
  float    zeroEndN = NAN;               // zero point after the spin-down, wheel lifted, on F_roll
  char     warn[WARN_N][120]; uint8_t warnN = 0;
  char     abortWhy[96] = "";
  uint32_t startMs = 0, endMs = 0, stopMs = 0;
  uint32_t readsStart = 0, readsEnd = 0;   // HX711 values at start and end: the rate actually reached
  float    plateaus[PLATEAU_MAX]; uint8_t plateauN = 0;
  float    seq[PLATEAU_MAX + 1]; uint8_t seqN = 0;   // reference point + plateaus for the measurement drive
  float    loadN = NAN;
  char     fileMd[56] = "", fileCsv[56] = "";   // full paths on fileMedia
  uint8_t  fileMedia = 0;                        // storage::Media of the two files
};
Record g_r;

// ------------------------------------------------------------- Helpers
void warn(const char* code, const char* fmt, ...) {
  if (g_r.warnN >= WARN_N) return;
  char* dst = g_r.warn[g_r.warnN++];
  const int n = snprintf(dst, sizeof(g_r.warn[0]), "%s ", code);
  va_list ap; va_start(ap, fmt);
  vsnprintf(dst + n, sizeof(g_r.warn[0]) - n, fmt, ap);
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

// Every step change also goes into the raw log as a note line "run <step>.<sub>", so
// the noise window, zeroing, check weight and so on can be found in the raw stream later.
void rawMark() { char t[24]; snprintf(t, sizeof(t), "run %u.%u", static_cast<unsigned>(g_step), static_cast<unsigned>(g_sub)); rawlog::text(t); }
// A result in the raw log, next to the samples it came from.
void rawNote(const char* fmt, ...) {
  char t[96]; va_list a; va_start(a, fmt); vsnprintf(t, sizeof(t), fmt, a); va_end(a);
  rawlog::text(t);
}
void go(Step s, uint8_t sub = 0) { g_step = s; g_sub = sub; g_subMs = millis(); rawMark(); }
void sub(uint8_t s) { g_sub = s; g_subMs = millis(); rawMark(); }
uint32_t inSubMs() { return millis() - g_subMs; }

// Unix time now, 0 without browser time.
// Clock time from the web client (wallclock.h); the time stamp from step 1 only as a fallback.
uint32_t epochNow() {
  const uint32_t w = wallclock::now();
  if (w) return w;
  return g_r.meta.epoch ? g_r.meta.epoch + (millis() - g_r.meta.epochMs) / 1000 : 0;
}
// Unix time -> "YYYY-MM-DD HH:MM" (Howard Hinnant's civil_from_days); the browser already
// sends the time as local time in seconds.
void fmtTime(uint32_t t, char* out, size_t n, bool withTime) { calc::fmtTime(t, out, n, withTime); }

float omegaOf(float kmh) { return kmh / 3.6f / settings::drumRadiusM(); }
// Raw-value difference of the cell as force on F_roll, N. NAN without calibration.
float countsToFrollN(float counts) {
  if (!settings::calibrated() || isnan(counts)) return NAN;
  return counts * settings::loadcellNPerCount() * settings::torqueArmM() / settings::drumRadiusM();
}
float rDrum() { return settings::drumRadiusM(); }
// Raw-value mean of the sample as torque above the tare, Nm. NAN without calibration.
float sampleOffsetNm() {
  if (!settings::calibrated() || torque::sampleN() == 0) return NAN;
  return (torque::sampleMean() - torque::tareOffset()) * settings::loadcellNPerCount() * settings::torqueArmM();
}

// Loss models for the correction: idle loss from this run (or adopted),
// wheel from the settings with the stored circumference.
float lossNm(float kmh) {
  return calc::lossNm(omegaOf(kmh), rDrum(), g_r.a0, g_r.b0, !isnan(g_r.a0),
                      settings::wheelA1(), settings::wheelB1(), g_r.wheelUsed, settings::wheelCircM());
}
float crrRaw(float mNm) { return (isnan(mNm) || isnan(g_r.loadN)) ? NAN : mNm / rDrum() / g_r.loadN; }
float crrKor(float kmh, float mNm) { return isnan(mNm) ? NAN : crrRaw(mNm - lossNm(kmh)); }

// Fit M = a + b w^2 over points
bool fitAB(const Point* p, uint8_t n, float& a, float& b) {
  calc::LinFit fit;
  for (uint8_t i = 0; i < n; i++) {
    if (isnan(p[i].m)) continue;
    const double x = omegaOf(p[i].v);
    fit.add(x * x, p[i].m);
  }
  return fit.solve(a, b);   // three points, otherwise false
}

// Mean of the valid points of a list, Nm. NAN without valid ones.
float meanM(const Point* p, uint8_t n, uint8_t& cnt) {
  double s = 0; cnt = 0;
  for (uint8_t i = 0; i < n; i++) if (p[i].ok && !isnan(p[i].m)) { s += p[i].m; cnt++; }
  return cnt ? static_cast<float>(s / cnt) : NAN;
}

// Pooled standard deviation of the points within the plateaus (docs/12 W5): with
// three points per plateau a single spread is uncertain to +-50 %; pooled over all
// plateaus it has Sum(n_i - 1) degrees of freedom.
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

// Pre/post idle loss of plateau k: means over its points (RUN_BASELINE_POINTS each), the
// spread within each, and the drift post - pre on F_roll. NaN where a side has no point.
struct Drift { float pre = NAN, post = NAN, preSpread = NAN, postSpread = NAN, n = NAN; };
void meanOf(const Point* p, uint8_t cnt, uint8_t k, float& mean, float& spread) {
  float s = 0, lo = INFINITY, hi = -INFINITY; uint8_t c = 0;
  for (uint8_t i = 0; i < cnt; i++) if (p[i].plateau == k && !isnan(p[i].m)) { s += p[i].m; c++; lo = fminf(lo, p[i].m); hi = fmaxf(hi, p[i].m); }
  mean = c ? s / c : NAN; spread = c ? hi - lo : NAN;
}
Drift drift(uint8_t k) {
  Drift d;
  meanOf(g_r.pre, g_r.preN, k, d.pre, d.preSpread);
  meanOf(g_r.post, g_r.postN, k, d.post, d.postSpread);
  if (!isnan(d.pre) && !isnan(d.post)) d.n = (d.post - d.pre) / rDrum();
  return d;
}

// Idle-loss uncertainty on F_roll: largest drift pre-/post-run, otherwise the criterion.
float baselineUncN() {
  if (!g_r.postN) return RUN_DRIFT_LIMIT_N;
  float u = 0;
  for (uint8_t k = 0; k < g_r.plateauN; k++) { Drift d = drift(k); if (!isnan(d.n) && fabsf(d.n) > u) u = fabsf(d.n); }
  return u;
}

// Statistics per plateau over the measurement points
struct PlStat { float v, m, sd, froll, cRaw, cCorr, u, uA, mInert; uint8_t n, all, dof; };
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
  s.sd = calc::sdFromSums(sm, smm, n);
  s.mInert = static_cast<float>(sIn / n);
  const float mR = s.m - lossNm(s.v);
  s.froll = mR / rDrum();
  s.cRaw = crrRaw(s.m);
  s.cCorr = crrRaw(mR);
  // Uncertainty (docs/11 R9, docs/12 D3), k = 1:
  //   Type A   pooled spread of the points over all plateaus / sqrt(n); without
  //            pool the spread within the point
  //   Type B   calibration factor, calibration arm, wheel load, tire pressure, drum bearing
  //   Idle     drift pre-/post-run, otherwise the criterion
  //   Wheel    measured or estimated J_wheel
  //   Sync     residual of the averaging without a real index (ripple/2/N)
  //   Inertia  UNC_J_REL of the subtracted apparent torque
  uint8_t dof = 0;
  const float sPool = pooledSd(dof);
  s.dof = dof;
  s.uA = !isnan(sPool) ? sPool / sqrtf(n)
       : (n > 1 && !isnan(s.sd)) ? s.sd / sqrtf(n) : static_cast<float>(sdIn / n);
  const float uIdleNm = baselineUncN() * rDrum();
  const float uWheel = g_r.wheelUsed ? (wheel::jWheelMeasured() ? UNC_WHEEL_REL : UNC_WHEEL_EST_REL) : 0.0f;
  const float rel = sqrtf(sq(s.uA / mR) + sq(UNC_K_REL) + sq(UNC_LCAL_REL) + sq(UNC_N_REL)
                        + sq(UNC_P_REL) + sq(UNC_BEARING_REL)
                        + sq(uIdleNm / mR) + sq(uWheel)
                        + sq(static_cast<float>(sSync / n) / mR)
                        + sq(UNC_J_REL * s.mInert / mR));
  s.u = fabsf(s.cCorr) * rel;
  return s;
}

// ------------------------------------------------------------- Sequence callbacks
void fromStats(Point& p, const state::PointStats& st) {
  p.v = st.v; p.vSd = st.vSd; p.m = st.mNm; p.mRaw = st.mRawNm; p.mInert = st.mInertNm; p.dvdtNet = st.dvdtNet;
  p.sd = st.sdNm; p.ripple = st.rippleNm; p.uSync = st.uSyncNm; p.imot = st.imot; p.vin = st.vin;
  p.tfet = st.tfet; p.tmot = st.tmot; p.dvdt = st.dvdtMax; p.plateau = st.plateau; p.k = st.k;
  p.revs = st.revs; p.ok = st.ok; p.realRevs = st.realRevs; p.tMs = st.tMs;
  p.bar = tirepressure::bar(); p.tTire = tirepressure::tempC();
}
void onPrePoint(const state::PointStats& st)  { if (g_r.preN  < PLATEAU_MAX * RUN_BASELINE_POINTS) fromStats(g_r.pre[g_r.preN++], st); }
void onPostPoint(const state::PointStats& st) { if (g_r.postN < PLATEAU_MAX * RUN_BASELINE_POINTS) fromStats(g_r.post[g_r.postN++], st); }
// Measurement drive: sequence index 0 is the reference point, then the plateaus.
void onMeasPoint(const state::PointStats& st) {
  if (st.plateau == 0) {
    if (g_r.anchorPreN < RUN_POINTS_PER_PLATEAU) fromStats(g_r.anchorPre[g_r.anchorPreN++], st);
    return;
  }
  if (g_r.measN >= MEAS_N) return;
  Point& p = g_r.meas[g_r.measN++];
  fromStats(p, st);
  p.plateau = st.plateau - 1;
  if (!p.ok) warn("W03", "Point %.1f km/h #%u invalid: net dv/dt %.3f km/h/s", p.v, p.k + 1, p.dvdtNet);
}
void onAnchorPoint(const state::PointStats& st) { if (g_r.anchorN < RUN_POINTS_PER_PLATEAU) fromStats(g_r.anchor[g_r.anchorN++], st); }

void writeFiles();
float ctrlTargetNm();
void originText(const settings::WheelOrigin& o, char* out, size_t n);
void abortRun(const char* why);
void seqDone(bool completed);
void finishRun() { writeFiles(); go(S_REPORT); }

// Sequence over all plateaus, idle loss (one point per plateau).
bool startBaselineSeq(state::PointFn fn) {
  return state::startSequence(state::RUN_BASELINE, g_r.plateaus, g_r.plateauN, RUN_BASELINE_POINTS, false, fn, seqDone);
}

void anchorDrift(float& dNm, bool& have) {
  uint8_t n0 = 0, n1 = 0;
  const float m0 = meanM(g_r.anchorPre, g_r.anchorPreN, n0), m1 = meanM(g_r.anchor, g_r.anchorN, n1);
  have = n0 && n1; dNm = have ? m1 - m0 : NAN;
}

void seqDone(bool completed) {
  if (!completed) {
    if (g_step == S_BASE || g_step == S_PLATEAUS || g_step == S_ANCHOR || g_step == S_POST)
      abortRun(state::phase() == state::FAULT ? state::faultReason() : "stopped");
    return;
  }
  switch (g_step) {
    case S_BASE:
      if (fitAB(g_r.pre, g_r.preN, g_r.a0, g_r.b0)) settings::setBaseline(g_r.a0, g_r.b0);
      else warn("W12", "Idle-loss model not determinable from the pre-run");
      state::stopRun(); sub(3);   // brake, then lower the wheel
      break;
    case S_PLATEAUS:
      go(S_ANCHOR);
      if (!state::startSequence(state::RUN_MEASURE, &g_r.anchorKmh, 1, RUN_POINTS_PER_PLATEAU, false, onAnchorPoint, seqDone))
        abortRun(logger::lastNote());
      break;
    case S_ANCHOR: {
      float d; bool have; anchorDrift(d, have);
      if (have && fabsf(d) / rDrum() > RUN_ANCHOR_WARN_N)
        warn("W04", "Anchor drift %.3f N on F_roll between reference point before and after the plateaus", fabsf(d) / rDrum());
      go(S_STOP);
      g_r.stopMs = millis();
      state::stopRun();
      break;
    }
    case S_POST: {
      float worst = 0;
      for (uint8_t k = 0; k < g_r.plateauN; k++) { Drift d = drift(k); if (!isnan(d.n) && fabsf(d.n) > worst) worst = fabsf(d.n); }
      if (g_r.preN && worst > RUN_DRIFT_LIMIT_N) warn("W08", "Idle-loss drift %.3f N above %.2f N: discard series", worst, RUN_DRIFT_LIMIT_N);
      state::stopRun(); sub(3);
      break;
    }
    default: break;
  }
}

// ------------------------------------------------------------- System check
// Only what must be there NOW and can be checked now: controller, motor, cell, factor,
// arm, speed source, pressure, plateaus, wheel load. What later steps take care of
// themselves (zeroing, idle-loss model, wheel model, circumference) is not listed here --
// a list with "comes later" only confuses (2026-11-24).
void runChecks() {
  g_r.checkN = 0;
  const vesc::Values& v = vesc::values();
  if (!vesc::connected()) addCheck("Controller", -1, "no controller");
  else if (v.fault) addCheck("Controller", -1, "Fault: %s", vesc::faultText(v.fault));
  else addCheck("Controller", 1, "FW %u.%02u · %.1f V · no fault", vesc::fwMajor(), vesc::fwMinor(), v.vIn);
  addCheck("Motor at controller", (speedctl::motorOpen() || speedctl::estopInput()) ? -1 : 1,
           speedctl::motorOpen() ? "Disconnect detected" : speedctl::estopInput() ? "Emergency stop" : "Connected");
  if (!torque::isResponding()) addCheck("Load cell", -1, "HX711 not responding");
  else if (torque::isSaturated()) addCheck("Load cell", -1, "saturated");
  else {
    const float fN = torque::newtons();
    if (isnan(fN)) addCheck("Load cell", 1, "responding");
    else addCheck("Load cell", 1, "responding · %.1f %% of full scale", fabsf(fN) / LOADCELL_CAPACITY_N * 100.0f);
  }
  // The factor holds only for the conditions it was calibrated under (calib.h): cell arm,
  // calibration arm, HX711 rate and gain. Changed -> red; no record -> note.
  { char why[150]; const int8_t v = calib::validity(why, sizeof(why)); addCheck("Calibration", v, "%s", why); }
  addCheck("Cell arm", 1, "%.1f mm · L_cal %.1f mm", settings::torqueArmM() * 1000.0f, settings::calibArmM() * 1000.0f);
  // At the check the drum stands, so the live source is the controller; what counts is
  // whether marks and index are fitted (the header names the source of the run itself).
  addCheck("Revolutions", 1, "drum marks, %u per revolution (source while turning in the header)", static_cast<unsigned>(MARKS_PER_REV));
  if (tirepressure::source() == tirepressure::SRC_BLE && tirepressure::fresh())
    addCheck("Tire pressure", 1, "%s · %.2f bar · %.1f °C · battery %.0f %%", tirepressure::name(), tirepressure::bar(), tirepressure::tempC(), tirepressure::battery());
  else if (!isnan(g_r.meta.bar)) addCheck("Tire pressure", 1, "entry %.2f bar; AirSpy not reporting at the check (it wakes when the wheel turns)", g_r.meta.bar);
  else addCheck("Tire pressure", -1, "neither AirSpy nor entry: turn the wheel and Check again, or abort and enter it in step 1 (entries are kept)");
  if (!g_r.plateauN) addCheck("Plateaus", -1, "no measurement speeds");
  else {
    char l[48]; size_t p = 0;
    for (uint8_t i = 0; i < g_r.plateauN && p < sizeof(l) - 8; i++) p += snprintf(l + p, sizeof(l) - p, "%s%.1f", i ? " · " : "", g_r.plateaus[i]);
    addCheck("Plateaus", 1, "%s km/h, reference %.1f before and after", l, WARMUP_KMH);
  }
  addCheck("Wheel load", isnan(g_r.loadN) ? -1 : 1, isnan(g_r.loadN) ? "not set" : "%.1f N", g_r.loadN);
  // Storage (storage.h): without SD the run still works -- report to the flash,
  // but no raw data. Hence yellow, not red.
  { const storage::Info sd = storage::info(storage::SD_CARD);
    if (sd.state == storage::READY) addCheck("Storage", 1, "SD %d %% used · report and raw data", sd.pct);
    else addCheck("Storage", 0, "SD %s: report to flash, no raw data", storage::stateName(sd.state)); }
}

bool checksBlock() {
  for (uint8_t i = 0; i < g_r.checkN; i++) if (g_r.checks[i].res < 0) return true;
  return false;
}

// ------------------------------------------------------------- Report
void pf(Print& o, float v, uint8_t d) { if (isnan(v)) o.print("—"); else o.print(v, d); }

// Stand build (2026-11-26): everything about the mechanics and electronics that may change
// with a rebuild, so that a protocol can still be placed later. Settings with their value,
// constants with the firmware commit that carries them, and the builder's free text.
void writeBuild(Print& o) {
  const float u = settings::drumCircumferenceM();
  o.println("## Stand build");
  o.print("Stand revision: "); o.println(settings::standRev()[0] ? settings::standRev() : "not entered (Setup)");
  o.println();
  o.println("| Item | Value |"); o.println("|---|---|");
  o.print("| Firmware | commit " FW_COMMIT ", built " __DATE__ " " __TIME__ " |\n");
  o.print("| Drum | circumference "); o.print(u, 4); o.print(" m, Ø "); o.print(u / PI * 1000.0f, 1); o.print(" mm, J "); o.print(DRUM_INERTIA_KGM2, 3); o.println(" kg·m² (calculated) |");
  o.print("| Drum marks | "); o.print(MARKS_PER_REV); o.print(" per revolution, "); o.print(speed::pulsesPerRev()); o.print(" seen, revolution from ");
  o.print(speed::revSource() == speed::REV_INDEX_HALL ? "index Hall" : speed::revSource() == speed::REV_PATTERN ? "intermediate magnet" : "counting only (no index)");
  o.print(", echo filter "); o.print(MARK_ECHO_PCT); o.println(" % |");
  o.print("| Test-wheel pulses | "); o.print(WHEEL_MARKS_PER_REV); o.print(" per revolution, echo filter "); o.print(WHEEL_ECHO_PCT); o.println(" % |");
  o.print("| Load cell | "); o.print(LOADCELL_TYPE); o.print(", capacity "); o.print(LOADCELL_CAPACITY_N, 2); o.print(" N; "); o.print(LOADCELL_AMP); o.print(", nominal ");
  o.print(HX711_RATE_SPS); o.print(" SPS, measured ");
  { const uint32_t ms = g_r.endMs - g_r.startMs, n = g_r.readsEnd - g_r.readsStart;
    if (g_r.endMs > g_r.startMs && ms > 0) { o.print(n * 1000.0f / ms, 1); o.print(" values/s over the run"); } else o.print("—"); }
  o.println(" |");
  o.print("| Cell arm / calibration arm | "); o.print(settings::torqueArmM() * 1000.0f, 1); o.print(" mm / "); o.print(settings::calibArmM() * 1000.0f, 1); o.println(" mm |");
  o.print("| Wheel-load lever | empty (wheel, arm, fork at the axle) "); o.print(settings::loadEmptyKg(), 2); o.print(" kg, sliding weight "); o.print(settings::loadWeightKg(), 2);
  o.print(" kg, pivot to axle "); o.print(settings::loadArmM() * 1000.0f, 1); o.print(" mm, weight at "); if (g_r.meta.posMm) { o.print(g_r.meta.posMm); o.print(" mm"); } else o.print("—"); o.println(" |");
  o.print("| Motor | "); o.print(MOTOR_POLE_PAIRS); o.print(" pole pairs, kt "); o.print(MOTOR_KT_NM_PER_A, 3); o.print(" Nm/A (from the flux linkage), controller FW ");
  o.print(vesc::fwMajor()); o.print("."); o.print(vesc::fwMinor()); o.print(", relay brake "); o.print(RELAY_BRAKE_WIRED ? "wired" : "not wired"); o.println(" |");
  o.println();
}

void writeMd(Print& o) {
  char t[24];
  o.print("# Measurement run "); o.println(g_r.id);
  o.println();
  o.print("Tire: "); o.print(g_r.meta.tire); o.print(" · ETRTO: "); o.print(g_r.meta.etrto[0] ? g_r.meta.etrto : "—");
  o.print(" · Tube: "); o.print(g_r.meta.tube[0] ? g_r.meta.tube : "—"); o.print(" · Rim: "); o.print(g_r.meta.rim[0] ? g_r.meta.rim : "—");
  o.print(" · Hub: "); o.print(g_r.meta.hub[0] ? g_r.meta.hub : "—");
  { profiles::Hub h;
    if (profiles::hub(g_r.meta.hub, h)) {
      o.print(" ("); o.print(h.spokes); o.print(" spokes, "); o.print(h.axle); o.print(", brake "); o.print(h.brake);
      o.print(h.drive == 1 ? ", drive wheel" : h.drive == 0 ? ", no drive" : ""); if (h.colour.length()) { o.print(", "); o.print(h.colour); }
      o.print(")"); } }
  { profiles::Attr a;
    // Details only: the names are already in the line above (2026-11-26: printed twice).
    if (profiles::attr(profiles::TUBE, g_r.meta.tube, a) && a.a[2].length()) { o.print(" · tube material "); o.print(a.a[2]); }
    if (profiles::attr(profiles::RIM, g_r.meta.rim, a)) { o.print(" · rim inner width "); o.print(a.a[2]); o.print(" mm, BSD "); o.print(a.a[3]); o.print(" mm"); } }
  o.print(" · Wheel mass "); pf(o, settings::wheelMassKg() * 1000.0f, 0); o.print(" g · J_wheel ");
  if (wheel::jWheelMeasured()) { pf(o, settings::wheelJKgm2(), 4); o.print(" kg·m² (bifilar)"); } else o.print("estimated");
  o.print(" · Pressure cold: "); pf(o, g_r.meta.bar, 2); o.print(" bar · after warm-up: "); pf(o, g_r.barWarm, 2); o.print(" bar");
  o.print(" · Wheel load: "); pf(o, g_r.meta.loadKg, 1);
  o.print(" kg ("); pf(o, g_r.loadN, 1); o.print(" N), entered");
  if (g_r.meta.posMm) { o.print(" · sliding weight at "); o.print(g_r.meta.posMm); o.print(" mm"); }
  o.println("  ");
  if (isnan(g_r.meta.tempEndC)) g_r.meta.tempEndC = tirepressure::tempC();
  o.print("AirSpy temperature start: "); pf(o, g_r.meta.tempC, 1); o.print(" °C · end: "); pf(o, g_r.meta.tempEndC, 1);
  o.print(" °C · tire temperature after warm-up: "); pf(o, g_r.tTireWarm, 1); o.print(" °C · start ");
  fmtTime(g_r.meta.epoch, t, sizeof(t), true); o.print(t); o.print(" · end ");
  fmtTime(epochNow(), t, sizeof(t), true); o.print(t);
  o.print(" · duration "); o.print((g_r.endMs - g_r.startMs) / 60000); o.print(" min · firmware " __DATE__ " " __TIME__);
  o.print(" · controller FW "); o.print(vesc::fwMajor()); o.print("."); o.print(vesc::fwMinor());
  o.print(" · speed source "); o.println(revsrc::sourceName());
  o.print("Tire state: "); o.print(g_r.meta.tireNew == 1 ? "new" : g_r.meta.tireNew == 0 ? "already ridden" : "not stated");
  o.print(" · thickness "); if (isnan(g_r.meta.treadMm)) o.print("not measured"); else { pf(o, g_r.meta.treadMm, 2); o.print(" mm"); } o.println("  ");
  if (g_r.meta.note[0]) { o.print("Note: "); o.println(g_r.meta.note); }
  o.print("Raw data: "); o.println(g_r.rawFile[0] ? g_r.rawFile : "none (no SD card ready)");
  if (g_r.abortWhy[0]) { o.println(); o.print("**ABORTED: "); o.print(g_r.abortWhy); o.println("**"); }
  o.println();
  writeBuild(o);
  o.println("## Checks");
  o.println("| Check | Result | Value |"); o.println("|---|---|---|");
  for (uint8_t i = 0; i < g_r.checkN; i++) {
    const Check& c = g_r.checks[i];
    o.print("| "); o.print(c.name); o.print(" | "); o.print(c.res == 2 ? "recorded" : c.res > 0 ? "ok" : c.res == 0 ? "due, done" : "MISSING");
    o.print(" | "); o.print(c.val); o.println(" |");
  }
  o.print("| Zeroing, wheel lifted | ok | tare "); if (g_r.tareTaken) o.print(g_r.tareRaw); else o.print("—");
  o.print(" counts · peak-to-peak "); o.print(g_r.tarePp); o.print(" counts ("); pf(o, countsToFrollN(g_r.tarePp), 4); o.print(" N) · noise σ = "); pf(o, countsToFrollN(g_r.tareSigma), 4); o.print(" N on F_roll ("); pf(o, g_r.tareSigma, 0); o.println(" counts) |");
  if (g_r.tareAuto) {
    o.print("| Zero by the motor | both directions at "); pf(o, AZ_SPIN_KMH, 0); o.print(" km/h | ");
    for (uint8_t i = 0; i < AZ_POSITIONS; i++) { if (i) o.print(" · "); o.print(i % 2 ? "− " : "+ "); pf(o, g_r.azMean[i], 1); o.print(" counts over "); pf(o, g_r.azDeg[i] / 360.0f, 0); o.print(" rev"); }
    o.println(" |");
  }
  o.print(g_r.tareAuto ? "| Spread of the revolution means | " : "| Zero check after turning | "); o.print(isnan(g_r.tareCheckN) ? "not done" : (g_r.tareCheckOk ? "ok" : "DEVIATION (W13)"));
  o.print(" | "); if (!isnan(g_r.tareCheckN)) { pf(o, g_r.tareCheckN, 3); o.print(" N on F_roll, limit "); o.print(RUN_ZERO_LIMIT_N, 2); o.print(" N"); } o.println(" |");
  o.print("| Check weight | "); o.print(g_r.ctrlSkipped ? "skipped" : (fabsf(g_r.ctrlDev) < RUN_CTRL_OK_PCT ? "ok" : "warning (W10)"));
  o.print(" | "); if (g_r.ctrlSkipped) o.print("W01"); else { pf(o, g_r.ctrlNm, 3); o.print(" Nm · "); pf(o, g_r.ctrlDev, 2); o.print(" %"); } o.println(" |");
  o.print("| Load cell end stop | "); o.print(g_r.stopSkipped || isnan(g_r.stopPeakPct) ? "skipped" : ((g_r.stopPeakPct >= RUN_STOP_MIN_PCT && g_r.stopPeakPct <= RUN_STOP_MAX_PCT) ? "ok" : "OUTSIDE (W16)"));
  o.print(" | "); if (!isnan(g_r.stopPeakPct)) { o.print("peak "); pf(o, g_r.stopPeakPct, 0); o.print(" % FS, target "); o.print(RUN_STOP_MIN_PCT, 0); o.print(" to "); o.print(RUN_STOP_MAX_PCT, 0); o.print(" %"); } else o.print("W17"); o.println(" |");   // print, not printf: one %
  o.print("| Check weight after end stop | ");
  if (isnan(g_r.ctrlRep)) o.print("not read | |\n");
  else { o.print(fabsf(g_r.ctrlRep) < RUN_CTRL_OK_PCT ? "ok" : "CHANGED (W22)"); o.print(" | "); pf(o, g_r.ctrlRep, 3); o.print(" % against the first reading, limit "); pf(o, RUN_CTRL_OK_PCT, 1); o.println(" % |"); }
  o.print("| Zero after end stop | ");
  if (isnan(g_r.stopZeroN)) o.print("not read | |\n");
  else { o.print(fabsf(g_r.stopZeroN) <= RUN_ZERO_LIMIT_N ? "ok" : "SHIFTED (W21)"); o.print(" | "); pf(o, g_r.stopZeroN, 3); o.print(" N on F_roll, limit "); pf(o, RUN_ZERO_LIMIT_N, 2); o.println(" N |"); }
  o.print("| Zero point after the run | "); o.print(isnan(g_r.zeroEndN) ? "not checked" : (fabsf(g_r.zeroEndN) <= RUN_ZERO_LIMIT_N ? "ok" : "DRIFT (W14)"));
  o.print(" | "); if (!isnan(g_r.zeroEndN)) { pf(o, g_r.zeroEndN, 3); o.print(" N on F_roll"); } o.println(" |");
  o.println();
  o.println("## Calibration state");
  o.println("| Quantity | Value | Origin |"); o.println("|---|---|---|");
  o.print("| U drum | "); o.print(settings::drumCircumferenceM(), 4); o.println(" m | settings |");
  o.print("| L cell arm | "); o.print(settings::torqueArmM(), 4); o.print(" m | settings, at calibration "); pf(o, settings::armAtCalM(), 4); o.println(" m |");
  o.print("| L_cal calibration arm | "); o.print(settings::calibArmM(), 4); o.println(" m | settings (determines the torque chain) |");
  { char ct[24]; fmtTime(settings::calibEpoch(), ct, sizeof(ct), true);
    o.print("| N_per_count | "); o.print(settings::loadcellNPerCount(), 8); o.print(" N/count | ");
    o.print("calibration of "); o.print(settings::calibEpoch() ? ct : "unknown date");
    o.println(" |"); }
  calib::describe(o);
  o.print("| Check weight raw | ");
  if (isnan(g_r.ctrlRaw)) o.print("— | not read |\n");
  else {
    pf(o, g_r.ctrlRaw, 1); o.print(" counts above tare");
    if (!isnan(g_r.ctrlRaw2)) { o.print(", after the end stop "); pf(o, g_r.ctrlRaw2, 1); }
    // The factor this reading alone would give: target torque over (counts · cell arm).
    const float k = g_r.ctrlRaw != 0.0f ? ctrlTargetNm() / (g_r.ctrlRaw * settings::torqueArmM()) : NAN;
    o.print(" | step 5, "); o.print(settings::ctrlMassKg() * 1000.0f, 0); o.print(" g at "); o.print(settings::calibArmM() * 1000.0f, 0);
    o.print(" mm; gives "); pf(o, k, 8); o.println(" N/count |");
  }
  o.print("| J drum | "); o.print(DRUM_INERTIA_KGM2, 3); o.print(" kg·m² ± "); o.print(UNC_J_REL * 100, 0); o.println(" % | config.h, calculated; inertia correction per point |");
  o.print("| Tare | "); o.print(torque::tareOffset()); o.print(" counts | "); o.print(g_r.tareTaken ? "step 4 of this run" : "taken before the run (W20)"); o.println(" |");
  o.print("| A0 / B0 | "); pf(o, g_r.a0, 5); o.print(" Nm / "); pf(o, g_r.b0, 7); o.print(" | ");
  o.print(g_r.baseTaken ? "adopted (W02)" : "pre-run of this run");
  if (!isnan(g_r.a0Old)) { o.print(", before "); pf(o, g_r.a0Old, 5); o.print(" / "); pf(o, g_r.b0Old, 7); } o.println(" |");
  o.print("| A1 / B1 / J_wheel | ");
  if (g_r.wheelUsed) { o.print(settings::wheelA1(), 5); o.print(" / "); o.print(settings::wheelB1(), 8); o.print(" / "); pf(o, wheel::jWheelKgm2(), 4);
    { char org[140]; originText(settings::wheelOrigin(), org, sizeof(org));
      o.print(g_r.wheelFresh ? " kg·m² | pre-run of this run (wheel spin-down), J " : " kg·m² | stored, same wheel ("); if (!g_r.wheelFresh) { o.print(org); o.print("), J "); } } o.print(wheel::jWheelMeasured() ? "measured (bifilar)" : "ESTIMATED (WHEEL_J_FACTOR)"); o.println(" |"); }
  else o.println("— | not applied |");
  if (g_r.wheelFresh && g_r.prevSame && !isnan(g_r.a1Old)) { char org[140]; originText(g_r.woOld, org, sizeof(org));
    o.print("| A1 / B1 before | "); o.print(g_r.a1Old, 5); o.print(" / "); o.print(g_r.b1Old, 8); o.print(" | "); o.print(org);
    o.println(g_r.prevSame ? ", same wheel |" : ", other set-up |"); }
  o.print("| Wheel circumference | "); pf(o, settings::wheelCircM(), 4); o.println(" m | speed ratio |");
  o.print("| Type B relative | k "); o.print(UNC_K_REL, 4); o.print(" · L_cal "); o.print(UNC_LCAL_REL, 4); o.print(" · N "); o.print(UNC_N_REL, 4);
  o.print(" · p "); o.print(UNC_P_REL, 4); o.print(" · bearing "); o.print(UNC_BEARING_REL, 4); o.print(" · wheel ");
  o.print(g_r.wheelUsed ? (wheel::jWheelMeasured() ? UNC_WHEEL_REL : UNC_WHEEL_EST_REL) : 0.0f, 4); o.println(" | config.h |");
  o.println();
  if (g_r.preN) {
    o.println("## Idle loss pre-run");
    o.println("| v km/h | M Nm | M raw Nm | J·dω/dt Nm | σ_U Nm |"); o.println("|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.preN; i++) { const Point& p = g_r.pre[i]; o.print("| "); pf(o, p.v, 1); o.print(" | "); pf(o, p.m, 4); o.print(" | "); pf(o, p.mRaw, 4);
      o.print(" | "); pf(o, p.mInert, 4); o.print(" | "); pf(o, p.sd, 4); o.println(" |"); }
    o.println();
  }
  if (g_r.warmN) {
    o.print("## Warm-up "); o.print(WARMUP_KMH, 1); o.println(" km/h");
    o.println("| min | M Nm | P W | Crr corr. | p bar | T tire °C |"); o.println("|---|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.warmN; i++) { const Warm& w = g_r.warm[i]; o.print("| "); o.print(w.min); o.print(" | "); pf(o, w.m, 4); o.print(" | "); pf(o, w.p, 1);
      o.print(" | "); pf(o, w.crr, 5); o.print(" | "); pf(o, w.bar, 2); o.print(" | "); pf(o, w.tTire, 1); o.println(" |"); }
    if (g_r.warmN >= 6) { const float a = g_r.warm[g_r.warmN - 6].p, b = g_r.warm[g_r.warmN - 1].p;
      o.println(); o.print("Change in the last 5 min: "); pf(o, (b / a - 1.0f) * 100.0f, 1); o.println(" %"); }
    if (g_r.warmSettledMin) {
      o.println(); o.print("Ended after "); o.print(g_r.warmSettledMin); o.print(" min: settled (last ");
      o.print(WARMUP_STABLE_MIN); o.print(" minute values of M within "); o.print(WARMUP_STABLE_NM, 3);
      o.print(" Nm, tire temperature rise at most "); o.print(WARMUP_TTIRE_K_PER_MIN, 1); o.println(" K/min).");
    }
    o.println();
  }
  if (g_r.measN || g_r.anchorPreN) {
    o.println("## Measurement points");
    o.println("Order: reference point right after the warm-up, then the plateaus ascending, then the same reference point (anchor). M is corrected for J·dω/dt.");
    o.println();
    o.println("| Part | v km/h | Point | M Nm | M raw Nm | J·dω/dt Nm | dv/dt net | σ_U Nm | Ripple Nm | I_mot A | U_bus V | p bar | T tire | valid |");
    o.println("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    auto row = [&](const char* kind, const Point& p) {
      o.print("| "); o.print(kind); o.print(" | "); pf(o, p.v, 2); o.print(" | "); o.print(p.k + 1); o.print(" | "); pf(o, p.m, 4); o.print(" | "); pf(o, p.mRaw, 4);
      o.print(" | "); pf(o, p.mInert, 4); o.print(" | "); pf(o, p.dvdtNet, 3); o.print(" | "); pf(o, p.sd, 4); o.print(" | "); pf(o, p.ripple, 3);
      o.print(" | "); pf(o, p.imot, 2); o.print(" | "); pf(o, p.vin, 1); o.print(" | "); pf(o, p.bar, 2); o.print(" | "); pf(o, p.tTire, 1);
      o.print(" | "); o.print(p.ok ? "yes" : "no"); o.println(" |"); };
    for (uint8_t i = 0; i < g_r.anchorPreN; i++) row("Reference before", g_r.anchorPre[i]);
    for (uint8_t i = 0; i < g_r.measN; i++) row("Plateau", g_r.meas[i]);
    for (uint8_t i = 0; i < g_r.anchorN; i++) row("Reference after", g_r.anchor[i]);
    o.println();
    o.println("## Result per plateau");
    o.println("u(Crr) with k = 1; type A from the pooled spread of the points over all plateaus (degrees of freedom ν), type B from config.h, idle loss from the pre/post drift or the criterion.");
    o.println();
    o.println("| v km/h | n | M Nm | σ Nm | F_roll N | Crr raw | Crr corr. | u(Crr) k=1 | ν | u_A/M |"); o.println("|---|---|---|---|---|---|---|---|---|---|");
    for (uint8_t i = 0; i < g_r.plateauN; i++) { const PlStat s = plateauStat(i);
      o.print("| "); o.print(g_r.plateaus[i], 1); o.print(" | "); o.print(s.n); o.print("/"); o.print(s.all); o.print(" | "); pf(o, s.m, 4); o.print(" | "); pf(o, s.sd, 4);
      o.print(" | "); pf(o, s.froll, 3); o.print(" | "); pf(o, s.cRaw, 5); o.print(" | "); pf(o, s.cCorr, 5); o.print(" | ± "); pf(o, s.u, 5);
      o.print(" | "); o.print(s.dof); o.print(" | "); pf(o, isnan(s.uA) || isnan(s.m) ? NAN : s.uA / (s.m - lossNm(s.v)) * 100.0f, 2); o.println(" % |"); }
    o.println();
  }
  if (g_r.anchorPreN || g_r.anchorN) {
    o.print("## Anchor "); o.print(g_r.anchorKmh, 1); o.println(" km/h");
    float d; bool have; anchorDrift(d, have);
    if (have) { o.print("ΔM "); pf(o, d, 4); o.print(" Nm · ΔCrr "); pf(o, crrRaw(d), 5); o.print(" · "); pf(o, fabsf(d) / rDrum(), 3);
      o.print(" N on F_roll (limit "); o.print(RUN_ANCHOR_WARN_N, 2); o.println(" N), after minus before"); }
    else o.println("no valid comparison (reference before or after missing)");
    o.println();
  }
  o.println("## Idle loss pre/post");
  if (g_r.postN) {
    o.print("Mean of "); o.print(RUN_BASELINE_POINTS); o.println(" points per speed; spread = max − min within pre and post.");
    o.println(); o.println("| v km/h | pre Nm | spread | post Nm | spread | Δ on F_roll N |"); o.println("|---|---|---|---|---|---|");
    for (uint8_t k = 0; k < g_r.plateauN; k++) { const Drift d = drift(k); o.print("| "); pf(o, g_r.plateaus[k], 1); o.print(" | "); pf(o, d.pre, 4); o.print(" | "); pf(o, d.preSpread, 4);
      o.print(" | "); pf(o, d.post, 4); o.print(" | "); pf(o, d.postSpread, 4); o.print(" | "); pf(o, isnan(d.n) ? NAN : fabsf(d.n), 3); o.println(" |"); }
  } else o.println("Post-run not driven (W05).");
  o.println();
  o.println("## Warnings");
  if (!g_r.warnN) o.println("none");
  for (uint8_t i = 0; i < g_r.warnN; i++) { o.print("- "); o.println(g_r.warn[i]); }
  o.println();
  o.println("## Raw data");
  o.print(g_r.fileCsv); o.println(" · columns:kind,plateau,k,t_s,v_kmh,v_sd,m_Nm,m_raw_Nm,m_inert_Nm,sd_Nm,ripple_Nm,dvdt_net,i_mot,v_in,t_fet,t_mot,p_bar,t_tire,revs,real_revs,ok");
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
  o.print("# Measurement run "); o.print(g_r.id); o.print("  U="); o.print(settings::drumCircumferenceM(), 4);
  o.print(" r="); o.print(rDrum(), 4); o.print(" L="); o.print(settings::torqueArmM(), 4); o.print(" L_cal="); o.print(settings::calibArmM(), 4);
  o.print(" N_per_count="); o.print(settings::loadcellNPerCount(), 8); o.print(" load_N="); pf(o, g_r.loadN, 1);
  o.print(" tire_new="); o.print(g_r.meta.tireNew); o.print(" tire_thickness_mm="); pf(o, g_r.meta.treadMm, 2);   // 2026-11-26
  o.print(" tare_counts="); o.print(torque::tareOffset()); o.print(" ctrl_counts="); pf(o, g_r.ctrlRaw, 1);   // added 2026-11-26
  o.print(" A0="); pf(o, g_r.a0, 5); o.print(" B0="); pf(o, g_r.b0, 7);
  if (g_r.wheelUsed) { o.print(" A1="); o.print(settings::wheelA1(), 6); o.print(" B1="); o.print(settings::wheelB1(), 9);
    o.print(" U_wheel="); o.print(settings::wheelCircM(), 4); o.print(" J_wheel="); o.print(wheel::jWheelKgm2(), 5);
    o.print(" J_wheel_src="); o.print(wheel::jWheelMeasured() ? "meas" : "est"); }
  o.print(" J_drum="); o.print(DRUM_INERTIA_KGM2, 3);
  o.print(" unc_k="); o.print(UNC_K_REL, 4); o.print(" unc_lcal="); o.print(UNC_LCAL_REL, 4); o.print(" unc_n="); o.print(UNC_N_REL, 4);
  o.print(" unc_p="); o.print(UNC_P_REL, 4); o.print(" unc_bearing="); o.print(UNC_BEARING_REL, 4);
  o.print(" unc_rad="); o.print(g_r.wheelUsed ? (wheel::jWheelMeasured() ? UNC_WHEEL_REL : UNC_WHEEL_EST_REL) : 0.0f, 4);
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
  g_r.readsEnd = torque::readsTotal();
  if (!g_r.rawFile[0] && rawlog::fileName()[0]) {
    snprintf(g_r.rawFile, sizeof(g_r.rawFile), "%s (%s)", rawlog::fileName(), storage::name(rawlog::media()));
    rawNote("run end, protocol %s/%s/protocol.md", storage::ROOT_DIR, g_r.id);
  }
  // Into the run's session directory /vmrr/<id>/ (storage.h, since 2026-11-26), on the SD
  // card when it is ready, otherwise in the flash (storage::protocolMedia).
  char dir[40];
  snprintf(dir, sizeof(dir), "%s/%s", storage::ROOT_DIR, g_r.id);
  snprintf(g_r.fileMd, sizeof(g_r.fileMd), "%s/protocol.md", dir);
  snprintf(g_r.fileCsv, sizeof(g_r.fileCsv), "%s/points.csv", dir);
  const storage::Media m = storage::protocolMedia();
  g_r.fileMedia = static_cast<uint8_t>(m);
  fs::FS* fsp = storage::fs(m);
  if (m == storage::SD_CARD) storage::lockSd();
  if (fsp) storage::makeDir(*fsp, dir);
  File f = fsp ? fsp->open(g_r.fileMd, "w") : File();
  if (f) { writeMd(f); f.close(); } else { g_r.fileMd[0] = '\0'; logger::fail("Report not written: file system"); }
  f = fsp ? fsp->open(g_r.fileCsv, "w") : File();
  if (f) { writeCsv(f); f.close(); } else g_r.fileCsv[0] = '\0';
  if (m == storage::SD_CARD) storage::unlockSd();
  Serial.print("# Report ("); Serial.print(storage::name(m)); Serial.print("): "); Serial.print(g_r.fileMd);
  Serial.print("  Data: "); Serial.println(g_r.fileCsv);
}

void abortRun(const char* why) {
  if (g_step == S_ABORT || g_step == S_OFF) return;
  strncpy(g_r.abortWhy, why ? why : "aborted", sizeof(g_r.abortWhy) - 1);
  warn("W06", "Run aborted in step %u: %s", static_cast<unsigned>(g_step), g_r.abortWhy);
  go(S_ABORT);   // first, so that the sequence callback (seqDone) does not land here again
  autozero::stop();
  if (state::phase() != state::IDLE && state::phase() != state::FAULT) state::stopRun();
  state::wizardAck();
  torque::sampleStop();
  writeFiles();
}

// ------------------------------------------------------------- Start
// Reference point = warm-up speed; the plateaus follow in ascending order.
void buildSeq() {
  g_r.seqN = 0;
  g_r.seq[g_r.seqN++] = g_r.anchorKmh;
  for (uint8_t i = 0; i < g_r.plateauN && g_r.seqN < PLATEAU_MAX + 1; i++) g_r.seq[g_r.seqN++] = g_r.plateaus[i];
}
// Speeds of this run (2026-11-26): bit i = Setup plateau i is measured. Default all; only
// in step 1, and never none. Baseline pre/post, points and report all use this list.
void selectPlateaus(uint32_t mask) {
  const uint8_t n = settings::plateauCount();
  uint8_t k = 0;
  for (uint8_t i = 0; i < n; i++) if (mask & (1UL << i)) k++;
  if (!k) return;
  g_r.plateauN = 0;
  for (uint8_t i = 0; i < n; i++) if (mask & (1UL << i)) g_r.plateaus[g_r.plateauN++] = settings::plateauKmh(i);
  buildSeq();
}

void startRun() {
  g_r.~Record();
  new (&g_r) Record();   // no temporary on the main loop's stack
  g_r.startMs = millis();
  g_r.readsStart = torque::readsTotal();
  g_r.plateauN = settings::plateauCount();
  for (uint8_t i = 0; i < g_r.plateauN; i++) g_r.plateaus[i] = settings::plateauKmh(i);
  g_r.loadN = state::loadN();
  g_r.meta.loadKg = isnan(g_r.loadN) ? NAN : g_r.loadN / 9.80665f;
  // Prefill the header data of the last run (NVS): after an abort or restart, don't start from scratch.
  { const settings::RunMeta& rm = settings::runMeta();
    strncpy(g_r.meta.tire, rm.tire, sizeof(g_r.meta.tire) - 1);
    strncpy(g_r.meta.rim, rm.rim, sizeof(g_r.meta.rim) - 1);
    strncpy(g_r.meta.hub, rm.hub, sizeof(g_r.meta.hub) - 1);
    strncpy(g_r.meta.note, rm.note, sizeof(g_r.meta.note) - 1);
    strncpy(g_r.meta.etrto, rm.etrto, sizeof(g_r.meta.etrto) - 1);
    strncpy(g_r.meta.tube, rm.tube, sizeof(g_r.meta.tube) - 1);
    g_r.meta.tempC = rm.tempC; g_r.meta.posMm = rm.posMm;
    if (!isnan(rm.loadKg) && rm.loadKg > 0) { g_r.meta.loadKg = rm.loadKg; g_r.loadN = rm.loadKg * 9.80665f; }
    g_r.meta.bar = (tirepressure::source() == tirepressure::SRC_BLE && tirepressure::fresh()) ? tirepressure::bar() : rm.bar; }
  g_r.a0Old = settings::baselineA0(); g_r.b0Old = settings::baselineB0();
  g_r.anchorKmh = WARMUP_KMH;
  buildSeq();
  storage::sessionName(g_r.id, sizeof(g_r.id));   // also the name of the session directory
  go(S_META);
}

void setMeta(char key, const char* val) {
  Meta& m = g_r.meta;
  const float f = atof(val);
  switch (key) {
    case 't': strncpy(m.tire, val, sizeof(m.tire) - 1); break;
    case 'w': strncpy(m.rim, val, sizeof(m.rim) - 1); m.rim[sizeof(m.rim) - 1] = 0; break;     // rim
    case 'h': strncpy(m.hub, val, sizeof(m.hub) - 1); m.hub[sizeof(m.hub) - 1] = 0; break;     // hub
    case 'q': {   // wheel profile by index (profiles.h): takes over its four parts and the ETRTO
      if (g_step != S_META) break;   // the wheel is fixed once the run is past step 1
      profiles::Profile p;
      if (!profiles::get(atoi(val), p)) { g_r.profile = -1; break; }
      g_r.profile = atoi(val);
      snprintf(m.tire, sizeof(m.tire), "%s", p.tire.c_str()); snprintf(m.tube, sizeof(m.tube), "%s", p.tube.c_str());
      snprintf(m.rim, sizeof(m.rim), "%s", p.rim.c_str()); snprintf(m.hub, sizeof(m.hub), "%s", p.hub.c_str());
      snprintf(m.etrto, sizeof(m.etrto), "%s", profiles::etrto(p.tire.c_str()).c_str());
      key = 't';   // saved as the last run's header below
      break; }
    case 'n': strncpy(m.note, val, sizeof(m.note) - 1); break;
    case 'u': m.tireNew = val[0] == '1' ? 1 : val[0] == '0' ? 0 : -1; break;      // tire new / ridden
    case 'k': m.treadMm = (f > 0.1f && f < 20.0f) ? f : NAN; break;              // thickness mm
    case 'r': strncpy(m.etrto, val, sizeof(m.etrto) - 1); break;
    case 's': strncpy(m.tube, val, sizeof(m.tube) - 1); break;
    case 'b': m.bar = f > 0 ? f : NAN; if (f > 0) tirepressure::setManual(f); break;
    case 'l':   // measured (hanging scale): overrides the calculation; empty clears
      m.loadMeasKg = f > 0 ? f : NAN;
      if (f > 0) { m.loadKg = f; g_r.loadN = f * 9.80665f; state::setLoadN(g_r.loadN); }
      else if (!isnan(m.loadCalcKg)) { m.loadKg = m.loadCalcKg; g_r.loadN = m.loadKg * 9.80665f; state::setLoadN(g_r.loadN); }
      break;
    case 'p': { m.posMm = static_cast<int>(f);
      const float n = settings::loadFromPosN(f / 1000.0f);
      m.loadCalcKg = isnan(n) ? NAN : n / 9.80665f;
      if (!isnan(n) && isnan(m.loadMeasKg)) { m.loadKg = m.loadCalcKg; g_r.loadN = n; state::setLoadN(n); }
      break; }
    case 'v': if (g_step == S_META) selectPlateaus(static_cast<uint32_t>(strtoul(val, nullptr, 10))); break;   // speeds
    case 'd': m.epoch = static_cast<uint32_t>(strtoul(val, nullptr, 10)); m.epochMs = millis();
              fmtTime(m.epoch, g_r.id, sizeof(g_r.id), false); break;
    default: break;
  }
  if (key == 't' || key == 'w' || key == 'h' || key == 'n' || key == 'r' || key == 's' || key == 'b' || key == 'l' || key == 'p') {
    settings::RunMeta rm;
    strncpy(rm.tire, m.tire, sizeof(rm.tire)); strncpy(rm.rim, m.rim, sizeof(rm.rim)); strncpy(rm.hub, m.hub, sizeof(rm.hub)); strncpy(rm.note, m.note, sizeof(rm.note));
    strncpy(rm.etrto, m.etrto, sizeof(rm.etrto)); strncpy(rm.tube, m.tube, sizeof(rm.tube));
    rm.bar = m.bar; rm.tempC = m.tempC; rm.loadKg = m.loadKg; rm.posMm = m.posMm;
    settings::setRunMeta(rm);
  }
}

// Zero check after turning further and zero point after the run: the same calculation.
float zeroDeviationN() {
  const float m = sampleOffsetNm();
  return isnan(m) ? NAN : m / rDrum();
}
// Check-weight step readings (weight, weight again, zero): with the controller there they
// spin the drum both ways (autozero.h, builder 2026-11-27), else a plain average at rest.
void beginCtrlRead() {
  g_r.ctrlAuto = autozero::start(false);
  if (!g_r.ctrlAuto) torque::sampleBegin();
}
// true once the reading is complete; raw = mean counts, n = values (or readings).
bool ctrlReadDone(float& raw, uint32_t& n) {
  if (g_r.ctrlAuto) {
    const autozero::St a = autozero::state();
    if (a == autozero::FAILED) {
      warn("W13", "Reading by the motor stopped (%s), read at rest", autozero::why());
      autozero::stop(); g_r.ctrlAuto = false; torque::sampleBegin(); g_subMs = millis(); return false;
    }
    if (a != autozero::DONE) return false;
    raw = autozero::resultCounts(); n = AZ_POSITIONS;
    rawNote("motor_reading plus=%.1f minus=%.1f spread=%.1f", autozero::plusCounts(), autozero::minusCounts(), autozero::rangeCounts());
    autozero::stop(); g_r.ctrlAuto = false;
    return true;
  }
  if (inSubMs() < (g_sub == 4 ? RUN_ZERO_AVG_MS : RUN_CTRL_AVG_MS)) return false;
  torque::sampleStop();
  n = torque::sampleN(); raw = n ? torque::sampleMean() : NAN;
  return true;
}
float rawOffsetNm(float raw) {
  if (!settings::calibrated() || isnan(raw)) return NAN;
  return (raw - torque::tareOffset()) * settings::loadcellNPerCount() * settings::torqueArmM();
}
// Check weight: target torque and whether it is still hanging (live torque above half
// the target). Uncalibrated it counts as removed, otherwise there would be no Continue.
float ctrlTargetNm() { return settings::ctrlMassKg() * 9.80665f * settings::calibArmM(); }

// WHEEL MODEL IS PART OF EVERY RUN (builder, 2026-11-26): it depends on tire, tube and
// rim, so it is measured in step 6 together with the idle loss. Exception: a series on
// the same wheel at different pressures -- then the stored model may be taken, but only
// if it came from the same set-up (tire, tube, wheel, ETRTO; the pressure may differ).
// Stored model of THIS profile (profiles.h): same tire, tube, rim and hub.
bool storedWheelUsable() {
  profiles::Profile p;
  return profiles::get(g_r.profile, p) && p.hasModel() && !isnan(p.uWheel);
}
settings::RunMeta metaAsRunMeta() {
  settings::RunMeta m = {};
  snprintf(m.tire, sizeof(m.tire), "%s", g_r.meta.tire); snprintf(m.rim, sizeof(m.rim), "%s", g_r.meta.rim); snprintf(m.hub, sizeof(m.hub), "%s", g_r.meta.hub);
  snprintf(m.note, sizeof(m.note), "%s", g_r.meta.note); snprintf(m.etrto, sizeof(m.etrto), "%s", g_r.meta.etrto);
  snprintf(m.tube, sizeof(m.tube), "%s", g_r.meta.tube);
  m.bar = g_r.meta.bar; m.tempC = g_r.meta.tempC; m.loadKg = g_r.meta.loadKg; m.posMm = g_r.meta.posMm;
  return m;
}
// Origin of a profile's model, in the form the texts and the protocol use.
settings::WheelOrigin profileOrigin(const profiles::Profile& p) {
  settings::WheelOrigin o = {};
  snprintf(o.m.tire, sizeof(o.m.tire), "%s", p.tire.c_str()); snprintf(o.m.tube, sizeof(o.m.tube), "%s", p.tube.c_str());
  snprintf(o.m.rim, sizeof(o.m.rim), "%s", p.rim.c_str()); snprintf(o.m.hub, sizeof(o.m.hub), "%s", p.hub.c_str());
  o.m.bar = p.bar; snprintf(o.run, sizeof(o.run), "%s", p.run.c_str()); o.epoch = p.epoch; o.known = true;
  return o;
}
// "tire · tube · rim · hub, 4.50 bar, run 2026-11-25-183012" for texts and the protocol.
void originText(const settings::WheelOrigin& o, char* out, size_t n) {
  if (!o.known) { snprintf(out, n, "origin unknown (before 2026-11-26)"); return; }
  char bar[16] = "?"; if (!isnan(o.m.bar)) snprintf(bar, sizeof(bar), "%.2f", o.m.bar);
  snprintf(out, n, "%s \xc2\xb7 %s \xc2\xb7 %s \xc2\xb7 %s, %s bar, %s%s", o.m.tire[0] ? o.m.tire : "?", o.m.tube[0] ? o.m.tube : "no tube",
           o.m.rim[0] ? o.m.rim : "?", o.m.hub[0] ? o.m.hub : "?", bar, o.run[0] ? "run " : "spin-down wizard", o.run);
}
float liveNm() { const float f = torque::newtons(); return isnan(f) ? NAN : f * settings::torqueArmM(); }
bool  ctrlWeightOn() { const float m = liveNm(); return !isnan(m) && m > 0.5f * ctrlTargetNm(); }
// ------------------------------------------------------------- Buttons
// ok = main button, alt = secondary button, redo = Again.
// Buttons that start a drive or read the cell at rest: only when the stand is idle. Buttons
// pressed while the drum turns on purpose (lift the wheel during the spin-down ...) stay free.
// A press while the drum still turns is answered, not acted on, and the step stays.
bool needsStill() {
  switch (g_step) {
    case S_TARE:  return g_sub == 0 || g_sub == 2 || g_sub == 5;           // zero: spins or reads at rest
    case S_CTRL:  return g_sub == 0 || g_sub == 1 || g_sub == 7;           // readings of the cell at rest
    case S_BASE:  return g_sub == 1;                                       // starts the wizard from rest
    case S_READY: return true;                                             // starts the warm-up from rest
    case S_POST:  return g_sub == 0;                                       // post-run zero by the motor
    default: return false;                                                 // the rest may be pressed while the drum turns
  }
}
bool stillOrSay() {
  if (!needsStill() || state::phase() == state::IDLE) return true;
  logger::fail("Drum still turning: wait until it stands");
  return false;
}

void onOk() {
  if (!stillOrSay()) return;
  switch (g_step) {
    case S_META:
      // Temperature from the AirSpy (sits in the room at the valve); there has been no
      // separate room temperature since 2026-11-25.
      g_r.meta.tempC = tirepressure::tempC();
      if (tirepressure::source() == tirepressure::SRC_BLE && tirepressure::fresh()) g_r.meta.bar = tirepressure::bar();
      // Wheel profile (2026-11-26): chosen in step 1, everything about the wheel was
      // entered beforehand in Setup. Only a complete profile goes on.
      if (profiles::missing(g_r.profile).length()) break;
      { profiles::Profile p;
        profiles::get(g_r.profile, p);
        if (p.hasModel()) { g_r.a1Old = p.a1; g_r.b1Old = p.b1; g_r.woOld = profileOrigin(p); g_r.prevSame = true; }
        else { g_r.a1Old = g_r.b1Old = NAN; g_r.prevSame = false; }
        // Mass and J of THIS wheel into use; without a measured J none (estimated from the mass).
        settings::setWheelMassKg(p.mWheel);
        settings::setWheelJKgm2(isnan(p.jWheel) ? 0.0f : p.jWheel); }
      go(S_VISUAL); break;
    case S_VISUAL: runChecks(); go(S_CHECKS, 1); break;   // no noise window since 2026-11-26: zeroing measures it
    case S_CHECKS: if (g_sub == 1 && !checksBlock()) go(S_TARE); break;
    case S_TARE:
      // wheel lifted -> automatic zero with the drum motor (sub 5); by hand only as fallback
      if (g_sub == 0) { if (autozero::start(true)) sub(5); else { state::startTare(); sub(1); warn("W13", "Automatic zero not possible (%s), zeroed by hand", autozero::why()); } }
      else if (g_sub == 2 && (isnan(g_r.turnDeg) || g_r.turnReady)) { torque::sampleBegin(); sub(3); }   // turned, still -> measure
      else if (g_sub == 4) go(S_CTRL, 1);                              // result -> check weight
      break;
    case S_CTRL:
      // Order since 2026-11-26 (sub numbers kept, the order is new):
      //   1 hang weight -> 2 read -> 3 result (adopt?) -> 0 end stop WITH the weight on
      //   -> 6 read weight again -> 7 compare, remove weight -> 4 read zero -> 5 result.
      // The end stop is judged in % of the cell's full scale, i.e. with the factor the
      // weight has just confirmed or replaced. The second weight reading shows whether the
      // overload changed the sensitivity (W22), the zero afterwards whether it left an
      // offset (W21). Both compare raw counts, so an adopted factor does not matter.
      if (g_sub == 1) { beginCtrlRead(); sub(2); }                    // hanging -> measure
      else if (g_sub == 3) {                                           // result -> end stop, weight stays on
        if (fabsf(g_r.ctrlDev) >= RUN_CTRL_OK_PCT) warn("W10", "Check weight deviates %.2f %%, continued without recalibrating", g_r.ctrlDev);
        g_r.stopPeakPct = NAN; sub(0);
      }
      else if (g_sub == 0) {                                           // end stop reached and released
        if (isnan(g_r.stopPeakPct) || g_r.stopPeakPct < RUN_STOP_MIN_PCT || g_r.stopPeakPct > RUN_STOP_MAX_PCT)
          warn("W16", "End stop at %.0f %% FS, target %.0f to %.0f %%", g_r.stopPeakPct, RUN_STOP_MIN_PCT, RUN_STOP_MAX_PCT);
        beginCtrlRead();
        if (g_r.ctrlSkipped || isnan(g_r.ctrlRaw) || !ctrlWeightOn()) { g_r.stopZeroN = NAN; sub(4); }   // no weight: straight to the zero
        else sub(6);
      }
      else if (g_sub == 7) {                                           // compared -> zero, once the weight is off
        if (ctrlWeightOn()) { g_r.ctrlOnHint = true; break; }
        if (!isnan(g_r.ctrlRep) && fabsf(g_r.ctrlRep) >= RUN_CTRL_OK_PCT)
          warn("W22", "Check weight %+.2f %% after the end-stop press", g_r.ctrlRep);
        g_r.stopZeroN = NAN; beginCtrlRead(); sub(4);
      }
      else if (g_sub == 5 && !isnan(g_r.stopZeroN) && fabsf(g_r.stopZeroN) <= RUN_ZERO_LIMIT_N) go(S_BASE);
      break;
    case S_BASE:
      if (g_sub == 0) sub(1);                       // drive new -> confirm wheel placed
      else if (g_sub == 1) {                        // confirmed -> wizard: spin up, lift, both models
        g_r.preN = 0;
        state::startWizard(onPrePoint, seqDone, g_r.plateaus, g_r.plateauN);
        if (state::wizardStep() == 1) sub(2); else abortRun(logger::lastNote());
      } else if (g_sub == 4) go(S_WHEEL);           // wheel lowered
      break;
    case S_WHEEL:
      if ((g_r.wheelFresh || (g_r.baseTaken && storedWheelUsable())) && !isnan(settings::wheelCircM())) { g_r.wheelUsed = true; go(S_READY); }
      break;
    case S_READY:
      if (state::startHold(WARMUP_KMH)) { go(S_WARMUP); g_r.warmN = 0; g_r.warmSettledMin = 0; g_r.wSumM = g_r.wSumP = g_r.wSumC = 0; g_r.wCnt = 0; } else abortRun(logger::lastNote());
      break;
    case S_POST:
      // wheel lifted -> check the zero point: by the motor at several positions, the stand
      // not touched (since 2026-11-27); a single reading where that is not possible
      if (g_sub == 0) { g_r.postAuto = autozero::start(false); if (!g_r.postAuto) torque::sampleBegin(); sub(1); }
      break;
    case S_REPORT: case S_ABORT: go(S_OFF); break;
    default: break;
  }
}
void onAlt() {
  if (!stillOrSay()) return;
  switch (g_step) {
    case S_TARE:
      // Zero point already set since boot (tare at start-up or by hand): adopt it.
      if (g_sub == 0 && !isnan(torque::tareSigma())) { warn("W20", "Zeroing skipped: existing zero point %ld counts, angle check missing", torque::tareOffset()); go(S_CTRL, 1); }
      if (g_sub == 4) { warn("W13", "Zero point depends on angle: %.3f N on F_roll, continued anyway", g_r.tareCheckN); go(S_CTRL, 1); }
      if (g_sub == 5 && autozero::state() == autozero::FAILED) { g_r.tareAuto = false; state::startTare(); sub(1); }   // by hand instead
      break;
    case S_CTRL:
      if (g_sub == 0) { g_r.stopSkipped = true; g_r.stopPeakPct = NAN; warn("W17", "End-stop check skipped");
                        if (isnan(g_r.ctrlRaw)) go(S_BASE); else { g_r.ctrlRaw2 = NAN; g_r.ctrlRep = NAN; sub(7); } }   // still remove the weight
      else if (g_sub == 1) { g_r.ctrlSkipped = true; warn("W01", "Check weight skipped"); g_r.stopPeakPct = NAN; sub(0); }
      else if (g_sub == 5) { warn("W21", "Zero after end stop off by %.3f N on F_roll, continued anyway", g_r.stopZeroN); go(S_BASE); }
      // sub 3: "Calibrate" is handled in the UI (opens the calibration procedure, calib.h);
      // afterwards Again reads the check weight with the new factor.
      break;
    case S_BASE:
      if (g_sub == 0 && settings::haveBaseline() && storedWheelUsable()) {
        g_r.a0 = settings::baselineA0(); g_r.b0 = settings::baselineB0(); g_r.baseTaken = true;
        { profiles::Profile p; profiles::get(g_r.profile, p);   // the wheel model of this profile into use
          settings::setWheelModel(p.a1, p.b1); settings::setWheelCircM(p.uWheel);
          const settings::WheelOrigin o = profileOrigin(p); settings::setWheelOrigin(o.m, o.run, o.epoch); }
        if (settings::baselineAgeMs() == UINT32_MAX) warn("W02", "Idle-loss model of unknown age adopted");
        go(S_WHEEL);
      }
      break;
    case S_WARMUP:
      warn("W11", "Warm-up cut short after %lu min", static_cast<unsigned long>(inSubMs() / 60000));
      sub(1);   // -> plateaus
      break;
    case S_POST: if (g_sub == 0) { warn("W05", "Post-run skipped: idle-loss drift and zero point unchecked"); finishRun(); } break;
    default: break;
  }
}
// Back: one preparation step back, as long as nothing is driving. In the measurement
// drive there is no Back, only Abort.
bool canBack() {
  if (speedctl::running()) return false;
  switch (g_step) {
    case S_VISUAL: case S_CHECKS: case S_WHEEL: case S_READY: return true;
    case S_TARE: return g_sub != 1 && g_sub != 3 && !(g_sub == 5 && autozero::state() != autozero::FAILED);
    case S_CTRL: return g_sub != 2 && g_sub != 4 && g_sub != 6;
    case S_BASE: return g_sub == 0 || g_sub == 1 || g_sub == 4;
    default: return false;
  }
}
void onBack() {
  if (!canBack()) return;
  torque::sampleStop();
  switch (g_step) {
    case S_VISUAL: go(S_META); break;
    case S_CHECKS: go(S_VISUAL); break;
    case S_TARE:   runChecks(); go(S_CHECKS, 1); break;
    case S_CTRL:   if (g_sub == 1) go(S_TARE); else { g_r.stopPeakPct = NAN; sub(1); } break;
    case S_BASE:   if (g_sub == 1) sub(0); else go(S_CTRL, 1); break;
    case S_WHEEL:  go(S_BASE); break;
    case S_READY:  go(S_WHEEL); break;
    default: break;
  }
}
void onRedo() {
  if (!stillOrSay()) return;
  if (g_step == S_CHECKS && g_sub == 1) { runChecks(); sub(1); }   // after fixing things under Setup
  if (g_step == S_CTRL && g_sub == 3) sub(1);
  if (g_step == S_CTRL && g_sub == 0) { g_r.stopPeakPct = NAN; sub(0); }
  if (g_step == S_CTRL && g_sub == 7 && ctrlWeightOn()) { beginCtrlRead(); sub(6); }   // read the weight again
  if (g_step == S_CTRL && g_sub == 5) go(S_TARE);                        // zero shifted: zero again (then weight again)
  if (g_step == S_TARE && g_sub == 4) { g_r.tareCheckN = NAN; sub(0); }
  if (g_step == S_TARE && g_sub == 5 && autozero::state() == autozero::FAILED && autozero::start(true)) sub(5);
}

}  // namespace

// ------------------------------------------------------------- public
void begin(LiveFn live) {
  g_live = live;
  // storage::begin() mounts LittleFS (storage.h), no longer here: since 2026-11-25 an
  // unreadable flash is no longer formatted silently.
}

void service() {
  if (g_step == S_OFF || g_step == S_ABORT || g_step == S_REPORT) return;
  // A fault of the state machine aborts, whatever the step.
  if (state::phase() == state::FAULT) { abortRun(state::faultReason()); return; }

  switch (g_step) {
    case S_TARE:
      if (g_sub == 1 && state::phase() == state::IDLE && !torque::tareBusy() && inSubMs() > RUN_TARE_SETTLE_MS) {
        g_r.tareSigma = torque::tareSigma();
        g_r.tareRaw = torque::tareOffset(); g_r.tareTaken = true;
        { // peak-to-peak of the tare values: the last TARE_SAMPLES of the trace are exactly them
          static long tv[TARE_SAMPLES];
          const uint16_t n = torque::trace(tv, TARE_SAMPLES);
          long lo = n ? tv[0] : 0, hi = lo;
          for (uint16_t i = 1; i < n; i++) { if (tv[i] < lo) lo = tv[i]; if (tv[i] > hi) hi = tv[i]; }
          g_r.tarePp = hi - lo; }
        rawNote("tare raw_counts=%ld sigma_counts=%.1f pp_counts=%ld", g_r.tareRaw, g_r.tareSigma, g_r.tarePp);
        g_r.turnTacho0 = vesc::values().tacho; g_r.turnDeg = NAN; g_r.turnStandstillMs = 0; g_r.turnReady = false;
        sub(2);   // -> turn further
      }
      if (g_sub == 2) {
        // The controller's tachometer counts the Hall steps also when turned by hand.
        // Quarter turn reached and still for RUN_TURN_STANDSTILL_MS: the button appears.
        // The operator starts the reading -- no automatic start (2026-11-26: it started
        // while the drum was still creeping).
        if (vesc::connected()) {
          const int32_t d = vesc::values().tacho - g_r.turnTacho0;
          g_r.turnDeg = fabsf(static_cast<float>(d)) * 360.0f / VESC_TACHO_PER_REV;
          const bool still = revsrc::revPerSec() < STANDSTILL_RPS && fabsf(static_cast<float>(vesc::values().erpm)) < STANDSTILL_RPM * MOTOR_POLE_PAIRS;
          if (g_r.turnDeg >= RUN_TURN_DEG - RUN_TURN_TOL_DEG && still) {
            if (g_r.turnStandstillMs == 0) g_r.turnStandstillMs = millis();
            else if (millis() - g_r.turnStandstillMs >= RUN_TURN_STANDSTILL_MS) g_r.turnReady = true;
          } else { g_r.turnStandstillMs = 0; g_r.turnReady = false; }
        } else { g_r.turnDeg = NAN; g_r.turnReady = false; }
      }
      if (g_sub == 5 && autozero::state() == autozero::DONE) {
        g_r.tareAuto = true; g_r.tareTaken = true;
        for (uint8_t i = 0; i < AZ_POSITIONS; i++) { g_r.azMean[i] = autozero::mean(i); g_r.azDeg[i] = autozero::degTurned(i); }
        g_r.tareRaw = torque::tareOffset(); g_r.tareSigma = autozero::sigmaCounts();
        g_r.tarePp = lroundf(autozero::rangeCounts());
        g_r.tareCheckN = countsToFrollN(autozero::rangeCounts());   // range over the positions
        g_r.tareCheckOk = !isnan(g_r.tareCheckN) && g_r.tareCheckN <= RUN_ZERO_LIMIT_N;
        rawNote("tare auto raw_counts=%ld spread_counts=%.1f sigma_counts=%.1f plus=%.1f minus=%.1f", g_r.tareRaw,
                autozero::rangeCounts(), autozero::sigmaCounts(), autozero::plusCounts(), autozero::minusCounts());
        autozero::stop();
        sub(4);
      }
      if (g_sub == 3 && inSubMs() >= RUN_ZERO_AVG_MS) {
        torque::sampleStop();
        g_r.tareCheckN = zeroDeviationN();
        g_r.tareCheckOk = !isnan(g_r.tareCheckN) && fabsf(g_r.tareCheckN) <= RUN_ZERO_LIMIT_N;
        sub(4);
      }
      break;
    case S_CTRL:
      if (g_sub == 0) {   // end stop: track the peak of the cell force in % FS
        const float fN = torque::newtons();
        if (!isnan(fN)) { const float pct = fabsf(fN) / LOADCELL_CAPACITY_N * 100.0f; if (isnan(g_r.stopPeakPct) || pct > g_r.stopPeakPct) g_r.stopPeakPct = pct; }
      }
      { float raw = NAN; uint32_t n = 0;
        if ((g_sub == 2 || g_sub == 4 || g_sub == 6) && ctrlReadDone(raw, n)) {
          const float above = isnan(raw) ? NAN : raw - torque::tareOffset();
          if (g_sub == 2) {
            const float m = rawOffsetNm(raw);
            g_r.ctrlNm = m; g_r.ctrlDev = (m / ctrlTargetNm() - 1.0f) * 100.0f;
            g_r.ctrlRaw = above;
            rawNote("check_weight raw_above_tare=%.1f n=%lu", g_r.ctrlRaw, static_cast<unsigned long>(n));
            sub(3);
          } else if (g_sub == 6) {                                     // weight again, after the end-stop press
            g_r.ctrlRaw2 = above;
            g_r.ctrlRep = (isnan(g_r.ctrlRaw2) || isnan(g_r.ctrlRaw) || g_r.ctrlRaw == 0.0f) ? NAN : (g_r.ctrlRaw2 / g_r.ctrlRaw - 1.0f) * 100.0f;
            rawNote("check_weight_after_stop raw_above_tare=%.1f", g_r.ctrlRaw2);
            sub(7);
          } else {                                                     // zero after the end-stop press
            const float m = rawOffsetNm(raw);
            g_r.stopZeroN = isnan(m) ? NAN : m / rDrum();
            rawNote("zero_after_stop raw_above_tare=%.1f", above);
            sub(5);
          }
        } }
      break;
    case S_BASE:
      // Drum stands still AND the wheel spin-down has ended (otherwise lowering would cut the fit short).
      if (g_sub == 3 && state::phase() == state::IDLE && !wheel::spindownActive()) {
        state::wizardAck();
        // Fresh = set during THIS run (an old stored model is not fresh, finding 2026-11-26).
        g_r.wheelFresh = settings::haveWheelModel() && settings::wheelModelAgeMs() <= millis() - g_r.startMs;
        if (g_r.wheelFresh) {
          settings::setWheelOrigin(metaAsRunMeta(), g_r.id);
          profiles::setModel(g_r.profile, settings::wheelA1(), settings::wheelB1(), settings::wheelCircM(), g_r.meta.bar, g_r.id, settings::wheelOrigin().epoch);
        }
        if (!g_r.wheelFresh) warn("W19", "Wheel model not determinable from the spin-down (too few revolutions)");
        sub(4);   // -> lower the wheel
      }
      break;
    case S_WARMUP:
      if (g_sub == 0) {
        // one live point per minute; continue after WARMUP_MS
        const uint8_t minute = inSubMs() / 60000;
        // Track the live value: the minute point takes the last valid one of the minute,
        // not whatever the moment happens to give (at 30 km/h it was mostly not steady).
        // Since 2026-11-26 the minute value is the MEAN of all valid live values of that
        // minute: a single snapshot scattered by +-0.012 Nm, and five of them rarely lay
        // within WARMUP_STABLE_NM even with the tire settled.
        { LiveVals lv;
          if (g_live && g_live(lv)) {
            g_r.liveLast = lv; g_r.liveLastMs = millis();
            if (isfinite(lv.mNm) && isfinite(lv.pW)) { g_r.wSumM += lv.mNm; g_r.wSumP += lv.pW; g_r.wSumC += isfinite(lv.crr) ? lv.crr : 0.0; g_r.wCnt++; }
          } }
        if (minute >= 1 && g_r.warmN < WARM_N && (g_r.warmN == 0 || g_r.warm[g_r.warmN - 1].min < minute)) {
          Warm& w = g_r.warm[g_r.warmN++];
          w.min = minute; w.bar = tirepressure::bar(); w.tTire = tirepressure::tempC();
          if (g_r.wCnt) { w.m = g_r.wSumM / g_r.wCnt; w.p = g_r.wSumP / g_r.wCnt; w.crr = g_r.wSumC / g_r.wCnt; }
          else if (g_r.liveLastMs && millis() - g_r.liveLastMs < RUN_WARM_LIVE_MAX_MS) { w.m = g_r.liveLast.mNm; w.p = g_r.liveLast.pW; w.crr = g_r.liveLast.crr; }
          else { w.m = w.p = w.crr = NAN; }
          g_r.wSumM = g_r.wSumP = g_r.wSumC = 0; g_r.wCnt = 0;
          // One console line per minute: shows what the settle test sees.
          { float lo = INFINITY, hi = -INFINITY;
            for (uint8_t k = g_r.warmN > WARMUP_STABLE_MIN ? g_r.warmN - WARMUP_STABLE_MIN : 0; k < g_r.warmN; k++) { lo = fminf(lo, g_r.warm[k].m); hi = fmaxf(hi, g_r.warm[k].m); }
            Serial.printf("# Warm-up min %u: M %.4f Nm, P %.1f W, Crr %.5f, tire %.1f degC, span of last %u: %.4f Nm (limit %.3f)\n",
                          static_cast<unsigned>(minute), w.m, w.p, w.crr, w.tTire, static_cast<unsigned>(g_r.warmN < WARMUP_STABLE_MIN ? g_r.warmN : WARMUP_STABLE_MIN), hi - lo, WARMUP_STABLE_NM); }
          // Settled? Last WARMUP_STABLE_MIN minute values (calc/warmup.h).
          if (inSubMs() >= WARMUP_MIN_MS && g_r.warmN >= WARMUP_STABLE_MIN) {
            float p[WARMUP_STABLE_MIN], t[WARMUP_STABLE_MIN];
            for (uint8_t k = 0; k < WARMUP_STABLE_MIN; k++) {
              const Warm& x = g_r.warm[g_r.warmN - WARMUP_STABLE_MIN + k]; p[k] = x.m; t[k] = x.tTire;
            }
            if (calc::warmupSettled(p, t, WARMUP_STABLE_MIN, WARMUP_STABLE_NM, WARMUP_TTIRE_K_PER_MIN)) {
              g_r.warmSettledMin = minute;
              logger::note("Warm-up settled");
              sub(1);
            }
          }
        }
        if (g_sub == 0 && inSubMs() >= WARMUP_MS) sub(1);
        if (state::phase() != state::HOLD) { abortRun("Warm-up interrupted"); return; }
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
      if (g_sub == 1 && g_r.postAuto && autozero::state() == autozero::FAILED) {   // fall back to one reading
        warn("W14", "Automatic post-run zero stopped (%s), single reading", autozero::why());
        autozero::stop(); g_r.postAuto = false; torque::sampleBegin(); g_subMs = millis();
      }
      if (g_sub == 1 && (g_r.postAuto ? autozero::state() == autozero::DONE : inSubMs() >= RUN_ZERO_AVG_MS)) {
        if (g_r.postAuto) { g_r.zeroEndN = countsToFrollN(autozero::resultCounts() - torque::tareOffset()); autozero::stop(); }
        else { torque::sampleStop(); g_r.zeroEndN = zeroDeviationN(); }
        if (!isnan(g_r.zeroEndN) && fabsf(g_r.zeroEndN) > RUN_ZERO_LIMIT_N)
          warn("W14", "Zero point shifted by %.3f N on F_roll after the run: drift of cell, cable or preload", g_r.zeroEndN);
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
      if (g_step != S_OFF) { logger::fail("Run already active"); return false; }
      // No standstill needed to START (builder, 2026-11-27: the drum was still coasting, the
      // run refused, and there was no way out but a page refresh). The first steps are only
      // input; the steps that need the drum still check it themselves (needsStill()).
      startRun(); return true;
    case 'k': onOk(); return true;
    case 's': onAlt(); return true;
    case 'n': onRedo(); return true;
    case 'b': onBack(); return true;
    case 'x':
      if (g_step == S_OFF) return true;
      if (g_step == S_REPORT || g_step == S_ABORT) { go(S_OFF); return true; }
      abortRun("aborted by operator"); return true;
    case 'm':
      if (t[1] && t[2] == '=') { setMeta(t[1], t + 3); return true; }
      return false;
    default: return false;
  }
}

bool active() { return g_step != S_OFF; }
bool busy()   { return g_step != S_OFF && g_step != S_REPORT && g_step != S_ABORT; }
const char* id() { return g_step != S_OFF ? g_r.id : ""; }
uint8_t step() { return static_cast<uint8_t>(g_step); }

// ------------------------------------------------------------- JSON
namespace {
size_t jnum(char* b, size_t n, float v, uint8_t d) { return !isfinite(v) ? snprintf(b, n, "null") : snprintf(b, n, "%.*f", d, v); }
size_t jstr(char* b, size_t n, const char* s) { return calc::jsonStr(b, n, s); }   // " and \ escaped
#define J(...) do { if (p < len) p += snprintf(buf + p, len - p, __VA_ARGS__); } while (0)
#define JN(v, d) do { if (p < len) p += jnum(buf + p, len - p, (v), (d)); } while (0)
#define JS(s) do { if (p < len) p += jstr(buf + p, len - p, (s)); } while (0)
// [v, k, M, sd, (Crr raw, Crr corr,) ok, M_inert, net dv/dt]
void jpoint(char* buf, size_t len, size_t& p, const Point& x, bool withCrr) {
  J("["); JN(x.v, 1); J(",%u,", x.k + 1); JN(x.m, 4); J(","); JN(x.sd, 4); J(",");
  if (withCrr) { JN(crrRaw(x.m), 5); J(","); JN(crrKor(x.v, x.m), 5); J(","); }
  J("%s,", x.ok ? "true" : "false"); JN(x.mInert, 4); J(","); JN(x.dvdtNet, 3); J("]");
}
}  // namespace

void json(char* buf, size_t len) {
  size_t p = 0;
  // Buttons per step: ok = main button, alt = secondary button, redo = Again; "" = hidden.
  const char *ok = "", *alt = "", *redo = "", *title = "", *txt = "";
  const char* act = "";   // what to do now, shown large right above the buttons
  static char abuf[64];
  switch (g_step) {
    case S_META:   title = "Create run"; ok = "Next";   // refused in onOk without a complete profile
      if (g_r.profile >= 0 && profiles::missing(g_r.profile).length()) { static char tm[120]; snprintf(tm, sizeof(tm), "Profile incomplete: %s. Setup \xe2\x86\x92 Wheels.", profiles::missing(g_r.profile).c_str()); txt = tm; }
      break;
    case S_VISUAL: title = "Visual inspection"; txt = "Drum and weighing part free; Wheel secured; E-stop within reach"; ok = "Checked"; break;   // "; " = one bullet each in the UI
    case S_CHECKS: title = "System check";
      ok = checksBlock() ? "" : "Next"; redo = "Again";
      txt = checksBlock() ? "Fix what is marked red (Setup stays reachable), then Again." : "Everything the run needs now is there.";
      break;
    case S_TARE:   title = "Zeroing";
      if (g_sub == 0) { txt = "Then the stand zeroes by itself: the drum runs forwards and backwards, the cell is averaged over whole revolutions."; act = "Lift the wheel"; ok = "Wheel lifted";
                        if (!isnan(torque::tareSigma())) alt = "Use stored"; }
      else if (g_sub == 1) { static char ttxt[40]; snprintf(ttxt, sizeof(ttxt), "Hands off. %u values.", static_cast<unsigned>(TARE_SAMPLES)); txt = ttxt; }
      else if (g_sub == 2) { txt = isnan(g_r.turnDeg) ? "Turn the drum a quarter turn further by hand, let it swing out."
                                 : g_r.turnReady ? "Quarter turn reached, drum still. Hands off, then measure."
                                 : g_r.turnDeg >= RUN_TURN_DEG - RUN_TURN_TOL_DEG ? "Quarter turn reached. Wait until the drum is still."
                                 : "Turn the drum further by hand until the bar is full, then let go.";
                             ok = isnan(g_r.turnDeg) ? "Turned" : (g_r.turnReady ? "Measure" : ""); }
      else if (g_sub == 3) txt = "Hands off, measuring.";
      else if (g_sub == 5) {
        if (autozero::state() == autozero::FAILED) { static char ftxt[120]; snprintf(ftxt, sizeof(ftxt), "Automatic zero stopped: %s.", autozero::why()); txt = ftxt; redo = "Again"; alt = "By hand"; }
        else txt = "Hands off: the drum turns by itself.";
      }
      else { ok = g_r.tareCheckOk ? "Next" : ""; alt = g_r.tareCheckOk ? "" : "Next anyway"; redo = "Again";
             txt = g_r.tareCheckOk ? "" : "Zero point depends on the angle: is something holding the rotor (wheel on the drum, cable)?"; }
      break;
    case S_CTRL: {
      static char ctxt[640];
      const float lcal = settings::calibArmM() * 1000.0f;
      title = "Check weight";
      if (g_sub == 6) { title = "Check weight again"; txt = "Hands off, measuring 2 s."; }
      else if (g_sub == 7) { title = "Check weight again";
        const bool on = ctrlWeightOn();
        if (!on) g_r.ctrlOnHint = false;
        const bool same = !isnan(g_r.ctrlRep) && fabsf(g_r.ctrlRep) < RUN_CTRL_OK_PCT;
        snprintf(ctxt, sizeof(ctxt), "%s", isnan(g_r.ctrlRep) ? "" : same ? "Same as before the press." : "Changed by the press: the cell or the arm may be bent, or the weight moved.");
        txt = ctxt;
        act = on ? (g_r.ctrlOnHint ? "Weight still hanging: remove it" : "Remove the weight") : ""; ok = "Next"; redo = on ? "Again" : ""; }
      else if (g_sub == 4) { title = "Zero after end stop"; txt = "Hands off, measuring."; }
      else if (g_sub == 5) { title = "Zero after end stop";
        const bool zok = !isnan(g_r.stopZeroN) && fabsf(g_r.stopZeroN) <= RUN_ZERO_LIMIT_N;
        txt = zok ? "Zero unchanged: the overload left no bias." : "Zero shifted after the press: the cell may be bent or the arm is caught. Zero again, or continue with W21.";
        ok = zok ? "Next" : ""; alt = zok ? "" : "Next anyway"; redo = zok ? "" : "Zero again"; }
      else if (g_sub == 0) { title = "Load cell end stop";
        snprintf(ctxt, sizeof(ctxt), "Wheel lifted, check weight stays on. Push the arm by hand SLOWLY in the pull direction against the end stop until the bar stops rising, then let go and wait until the weight hangs still. The end stop must engage between %.0f and %.0f %% of the cell's full scale: below that it limits the measurement (and the check weight would already rest on it), above that it does not protect the cell (150 %% is the limit). If the value rises above %.0f %% without holding: let go.",
                 RUN_STOP_MIN_PCT, RUN_STOP_MAX_PCT, RUN_STOP_MAX_PCT);
        txt = ctxt; ok = "Released, still"; alt = "Skip"; redo = "Again"; }
      else if (g_sub == 1) { snprintf(ctxt, sizeof(ctxt), "Certified weight at the mark (Setup: calibration arm), plumb, without weighing hook or cord. The wheel stays lifted. Checks the whole chain of cell, arm and factor against the calibration. The arm must not rest on the end stop.");
                             snprintf(abuf, sizeof(abuf), "Hang the %.0f g weight at %.0f mm", settings::ctrlMassKg() * 1000.0f, lcal);
                             txt = ctxt; act = abuf; ok = "Weight hangs"; alt = "Skip"; }
      else if (g_sub == 2) txt = "Hands off, measuring 2 s.";
      else {
        const bool good = fabsf(g_r.ctrlDev) < RUN_CTRL_OK_PCT;
        const bool on = ctrlWeightOn();
        // Continue is offered as soon as the weight is demonstrably off (live torque) -- the
        // deviation never blocks, it warns (W10). Adopt makes today's value the calibration
        // (from the sample just taken; the weight may already be off).
        ok = "Next"; alt = good ? "" : "Calibrate"; redo = "Again";
        const char* hang = on ? " Leave the weight on: the end stop comes next." : " Weight is off: hang it again for the end-stop check, or Next without.";
        if (good) { snprintf(ctxt, sizeof(ctxt), "Matches the calibration.%s", hang); txt = ctxt; }
        else {
          snprintf(ctxt, sizeof(ctxt), "%+.1f %% against the stored factor. Either something is wrong today (weight not at the %.0f mm mark, hook or cord weighed, arm on the end stop, cable) -- then Again. Or the calibration is off -- then Calibrate (the full procedure), and Again afterwards. Next keeps the factor and records W10.%s",
                   g_r.ctrlDev, lcal, hang);
          txt = ctxt;
        }
      }
      break; }
    case S_BASE:   title = "Idle-loss model";
      if (g_sub == 0) { ok = "Measure new";
        // Stored only within a series on the same wheel (same tire, tube, rim).
        const bool st = settings::haveBaseline() && storedWheelUsable();
        alt = st ? "Use stored" : "";
        static char t6[320];
        if (st) { char o[140]; profiles::Profile pp; profiles::get(g_r.profile, pp); originText(profileOrigin(pp), o, sizeof(o));
          snprintf(t6, sizeof(t6), "Spin up with the wheel resting, lift at 40 km/h: idle loss and wheel model in one. Same wheel as the stored model (%s): stored allowed in a pressure series.", o); }
        else snprintf(t6, sizeof(t6), "Spin up with the wheel resting, lift at 40 km/h: idle loss and wheel model in one. Part of every run.");
        txt = t6; }
      else if (g_sub == 1) { title = "Wheel resting?"; txt = ctrlWeightOn() ? "" : "Wheel rests on the drum, sliding weight in position. The drum runs up to 40 km/h, then the wheel is lifted."; act = ctrlWeightOn() ? "Check weight still hanging: remove it" : ""; ok = ctrlWeightOn() ? "" : "Spin up"; }
      else if (g_sub == 2) {
        const uint8_t w = state::wizardStep();
        if (w == 1)      { title = "Spin-up with wheel"; txt = "The wheel stays down until the drum has run steadily at 40 km/h for 3 s."; }
        else if (w == 2) { title = "Lift wheel now"; txt = "Lift with the eccentric. Detected from the wheel/drum speed ratio."; }
        else             { title = "Idle-loss pre-run"; txt = "Wheel coasts freely (wheel model), drum runs the plateaus (idle-loss model). Do not touch the wheel."; } }
      else if (g_sub == 3) { title = "Stopping"; txt = wheel::spindownActive() ? "Wheel still coasting, do not touch -- the fit only ends below 5 km/h at the wheel." : ""; }
      else { title = "Lower wheel"; txt = "Drum still, wheel still. Wheel onto the drum."; ok = "Wheel lowered"; }
      break;
    case S_WHEEL:  title = "Wheel model";
      { static char t7[360]; const char* jt = wheel::jWheelMeasured() ? "J_wheel measured." : "J_wheel only estimated (0.85 m r\xc2\xb2): \xc2\xb1" "3.5 %. Bifilar pendulum: step 1.";
        char o[140]; originText(settings::wheelOrigin(), o, sizeof(o));
        if (g_r.wheelFresh) {
          // Against the model before, if that was the same set-up: loss at 30 km/h at the wheel.
          const float u = settings::wheelCircM(), w = isnan(u) ? NAN : (30.0f / 3.6f) / (u / (2.0f * PI));
          const float mNew = settings::wheelA1() + settings::wheelB1() * w * w, mOld = g_r.a1Old + g_r.b1Old * w * w;
          if (g_r.prevSame && !isnan(mOld) && mOld != 0.0f)
            snprintf(t7, sizeof(t7), "From this run. %s At 30 km/h %.4f Nm, previous model of this wheel %.4f Nm (%+.0f %%).", jt, mNew, mOld, (mNew / mOld - 1.0f) * 100.0f);
          else snprintf(t7, sizeof(t7), "From this run. %s", jt);
          ok = "Next"; }
        else if (g_r.baseTaken && storedWheelUsable()) { snprintf(t7, sizeof(t7), "Stored, same wheel: %s. %s", o, jt); ok = "Next"; }
        else snprintf(t7, sizeof(t7), "No wheel model from the spin-down (W19). \xe2\x86\x90 Back and measure again.");
        txt = t7; }
      break;
    case S_READY:  title = "Wheel placed"; txt = "Sliding weight in position, beam level, wheel centred, cold pressure noted."; ok = "Start measurement"; break;
    case S_WARMUP: title = "Warm-up"; alt = "Skip rest"; break;
    case S_PLATEAUS: title = (state::plateauIndex() == 0) ? "Reference point" : "Plateaus"; break;
    case S_ANCHOR: title = "Reference point after"; break;
    case S_STOP:   title = "Stopping"; break;
    case S_POST:   title = "Idle-loss post-run";
      if (g_sub == 0) { txt = "Then the zero point is checked first, then the idle loss is repeated warm."; act = "Lift the wheel"; ok = "Wheel lifted"; alt = "Skip"; }
      else if (g_sub == 1) { title = "Zero point"; txt = "Hands off, measuring."; }
      else if (g_sub == 3) title = "Stopping";
      break;
    case S_REPORT: title = "Report"; ok = "Close"; break;
    case S_ABORT:  title = "Aborted"; txt = g_r.abortWhy; ok = "Close"; break;
    default: break;
  }
  // The step name already stands in the outline item above; the title only goes out
  // when a sub-step has its own (2026-11-24).
  static const char* const BASE[] = {"", "Create run", "Visual inspection", "System check", "Zeroing", "End stop, check weight",
                                     "Idle-loss model", "Wheel model", "Wheel placed", "Warm-up", "Plateaus",
                                     "Reference point after", "Stopping", "Idle-loss post-run", "Report"};
  if (g_step >= 1 && g_step <= 14 && strcmp(title, BASE[g_step]) == 0) title = "";
  J("{\"step\":%u,\"sub\":%u,\"id\":", static_cast<unsigned>(g_step), static_cast<unsigned>(g_sub)); JS(g_r.id);
  J(",\"title\":"); JS(title); J(",\"txt\":"); JS(txt); J(",\"act\":"); JS(act);
  J(",\"ok\":"); JS(ok); J(",\"alt\":"); JS(alt); J(",\"redo\":"); JS(redo); J(",\"back\":%s", canBack() ? "true" : "false");
  J(",\"subMs\":%lu,\"warmMs\":%lu", static_cast<unsigned long>(inSubMs()), static_cast<unsigned long>(WARMUP_MS));
  { LiveVals lv; const bool ok = g_live && g_live(lv);   // live values for the display, NAN while not steady
    J(",\"lvP\":"); JN(ok ? lv.pW : NAN, 1); J(",\"lvCrr\":"); JN(ok ? lv.crr : NAN, 5); J(",\"lvM\":"); JN(ok ? lv.mNm : NAN, 4); }
  // PROGRESS where something is measured: fraction 0..1 and one line of text with it.
  { float prog = NAN; char ptxt[120] = "";
    const bool seqStep = (g_step == S_BASE && g_sub == 2) || g_step == S_PLATEAUS || g_step == S_ANCHOR || (g_step == S_POST && g_sub == 2);
    if (g_step == S_TARE && g_sub == 1) {
      // The tare ends after TARE_SAMPLES values OR after TARE_TIMEOUT_MS; the bar follows
      // whichever comes first, otherwise it jumps from 25 % to done with a slow cell.
      const float byCount = static_cast<float>(torque::tareCount()) / TARE_SAMPLES, byTime = static_cast<float>(inSubMs()) / TARE_TIMEOUT_MS;
      prog = byCount > byTime ? byCount : byTime;
      snprintf(ptxt, sizeof(ptxt), "Zeroing: %u of %u values", torque::tareCount(), static_cast<unsigned>(TARE_SAMPLES)); }
    else if (((g_step == S_TARE && g_sub == 5) || (g_step == S_POST && g_sub == 1 && g_r.postAuto)
              || (g_step == S_CTRL && (g_sub == 2 || g_sub == 4 || g_sub == 6) && g_r.ctrlAuto)) && autozero::state() != autozero::FAILED) {
      prog = (autozero::done() + (autozero::state() == autozero::READ ? 0.5f : 0.0f)) / AZ_POSITIONS;
      snprintf(ptxt, sizeof(ptxt), "Zero by the motor: %s", autozero::phaseText()); }
    else if (g_step == S_TARE && g_sub == 2 && !isnan(g_r.turnDeg)) { prog = g_r.turnDeg / RUN_TURN_DEG;
      snprintf(ptxt, sizeof(ptxt), g_r.turnReady ? "%.0f degrees, still" : g_r.turnDeg >= RUN_TURN_DEG - RUN_TURN_TOL_DEG ? "%.0f degrees, still moving" : "%.0f of %.0f degrees turned", g_r.turnDeg, RUN_TURN_DEG); }
    else if ((g_step == S_TARE && g_sub == 3) || (g_step == S_POST && g_sub == 1) || (g_step == S_CTRL && g_sub == 4)) { prog = static_cast<float>(inSubMs()) / RUN_ZERO_AVG_MS; snprintf(ptxt, sizeof(ptxt), "Reading zero point, %lu s", static_cast<unsigned long>(RUN_ZERO_AVG_MS / 1000)); }
    else if (g_step == S_CTRL && g_sub == 6) { prog = static_cast<float>(inSubMs()) / RUN_CTRL_AVG_MS; snprintf(ptxt, sizeof(ptxt), "Reading check weight again, %lu s", static_cast<unsigned long>(RUN_CTRL_AVG_MS / 1000)); }
    else if (g_step == S_CTRL && g_sub == 2) { prog = static_cast<float>(inSubMs()) / RUN_CTRL_AVG_MS; snprintf(ptxt, sizeof(ptxt), "Reading check weight, %lu s", static_cast<unsigned long>(RUN_CTRL_AVG_MS / 1000)); }
    else if (g_step == S_BASE && g_sub == 2 && state::wizardStep() < 3) { snprintf(ptxt, sizeof(ptxt), state::wizardStep() == 1 ? "Drum running up to %.0f km/h with wheel" : "Lift wheel now (%.0f km/h)", WIZ_KMH); }
    else if (seqStep) {
      const uint8_t n = state::seqLength(), pi = state::plateauIndex(), pp = state::pointsPerPlateau(), k = state::pointIndex();
      const state::Phase ph = state::phase();
      float within = 0.0f; const char* what = "";
      if (ph == state::SPINUP) { what = "spin-up"; within = 0.0f; }
      else if (ph == state::SETTLE) { what = "settling"; within = 0.5f * (static_cast<float>(state::settleElapsedMs()) / SETTLE_MS); if (within > 0.5f) within = 0.5f; }
      else if (ph == state::POINT) { what = "measuring"; within = 0.5f + 0.5f * ((k + static_cast<float>(state::revsInPoint()) / AVG_REVS_PER_POINT) / (pp ? pp : 1)); }
      if (n) prog = (pi + within) / n;
      const bool isRef = (g_step == S_PLATEAUS && pi == 0) || g_step == S_ANCHOR;
      if (ph == state::POINT) snprintf(ptxt, sizeof(ptxt), "%s %s %.1f km/h: point %u of %u, revolution %u of %u", isRef ? "Reference" : (g_step == S_PLATEAUS ? "Plateau" : "Idle"), what, speedctl::targetKmh(), k + 1, pp, state::revsInPoint() + 1, static_cast<unsigned>(AVG_REVS_PER_POINT));
      else if (ph == state::SETTLE) snprintf(ptxt, sizeof(ptxt), "%s %.1f km/h: settling, %lu s of at least %lu s", isRef ? "Reference" : (g_step == S_PLATEAUS ? "Plateau" : "Idle"), speedctl::targetKmh(), static_cast<unsigned long>(state::settleElapsedMs() / 1000), static_cast<unsigned long>(SETTLE_MS / 1000));
      else { const bool down = speedctl::slowingDown() || speedctl::kmh() > speedctl::targetKmh() + 0.5f;   // coming from above: braking
        snprintf(ptxt, sizeof(ptxt), "%s: %s to %.1f km/h (%u of %u)", g_step == S_PLATEAUS ? "Plateaus" : (g_step == S_ANCHOR ? "Reference" : "Idle"), down ? "slowing down" : "spin-up", speedctl::targetKmh(), pi + 1, n); }
    }
    J(",\"prog\":"); JN(isnan(prog) ? NAN : (prog < 0 ? 0 : (prog > 1 ? 1 : prog)), 3); J(",\"progTxt\":"); JS(ptxt); }
  // Live raw-value trace while zeroing (step 4) and at the zero point after the run
  // (step 13, sub 1): deviations from the trace mean in counts, plus the scale to N
  // on F_roll. The UI draws it with a scale, so that fine flutter is visible.
  // Weight hanging (step 5, sub 1/2/6): live curve too, against the expected reading.
  if (g_step == S_TARE || (g_step == S_POST && g_sub == 1) || (g_step == S_CTRL && (g_sub >= 4 || g_sub == 1 || g_sub == 2))) {
    static long tr[TRACE_N];
    const uint16_t n = torque::trace(tr, TRACE_N);
    double sum = 0;
    for (uint16_t i = 0; i < n; i++) sum += tr[i];
    const long mean = n ? lround(sum / n) : 0;
    J(",\"trMean\":%ld,\"trNow\":%ld,\"trNpc\":", mean, n ? tr[n - 1] : 0L); JN(countsToFrollN(1.0f), 7);
    J(",\"trTare\":%ld,\"trace\":[", torque::tareOffset());
    for (uint16_t i = 0; i < n; i++) J("%s%ld", i ? "," : "", tr[i] - mean);
    J("]");
  }
  J(",\"tareSigma\":"); JN(g_r.tareSigma, 1); J(",\"tareSigmaN\":"); JN(countsToFrollN(g_r.tareSigma), 4);
  J(",\"tareChk\":"); JN(g_r.tareCheckN, 3); J(",\"zeroEnd\":"); JN(g_r.zeroEndN, 3); J(",\"zeroLim\":%.3f", RUN_ZERO_LIMIT_N);
  J(",\"ctrlNm\":"); JN(g_r.ctrlNm, 3); J(",\"ctrlDev\":"); JN(g_r.ctrlDev, 2); J(",\"ctrlOn\":%s", ctrlWeightOn() ? "true" : "false");
  J(",\"turnDeg\":"); JN(g_r.turnDeg, 1); J(",\"turnTarget\":%.0f,\"turnMin\":%.0f", static_cast<double>(RUN_TURN_DEG), static_cast<double>(RUN_TURN_DEG - RUN_TURN_TOL_DEG));
  J(",\"ctrlTarget\":%.3f", ctrlTargetNm());
  J(",\"ctrlLive\":"); JN(liveNm(), 3);
  // Counts above tare the stored calibration expects for the check weight (null if not
  // calibrated), and the first reading of this run (reference for the reread, sub 6).
  J(",\"ctrlExpect\":"); JN(settings::calibrated() ? ctrlTargetNm() / (settings::loadcellNPerCount() * settings::torqueArmM()) : NAN, 1);
  J(",\"ctrlRaw\":"); JN(g_r.ctrlRaw, 1);
  // How old zero point and calibration are: zero point since it was set (ESP clock, so
  // at most since boot), calibration as local time from storage.
  { const uint32_t ta = settings::tareAgeMs();
    J(",\"tareAgeS\":"); if (ta == UINT32_MAX) J("null"); else J("%lu", static_cast<unsigned long>(ta / 1000));
    J(",\"calEpoch\":%lu,\"nowEpoch\":%lu", static_cast<unsigned long>(settings::calibEpoch()), static_cast<unsigned long>(epochNow())); }   // live with the stored factor, for the scale at the check weight
  J(",\"ctrlKg\":"); JN(isnan(g_r.ctrlNm) ? NAN : g_r.ctrlNm / 9.80665f / settings::calibArmM(), 3);
  J(",\"ctrlTargetKg\":%.3f,\"calArmMm\":%.1f", settings::ctrlMassKg(), settings::calibArmM() * 1000.0f);
  { const float fN = torque::newtons();
    J(",\"stopLive\":"); JN(isnan(fN) ? NAN : fabsf(fN) / LOADCELL_CAPACITY_N * 100.0f, 1);
    J(",\"stopPeak\":"); JN(g_r.stopPeakPct, 1); J(",\"stopZero\":"); JN(g_r.stopZeroN, 3); J(",\"ctrlRep\":"); JN(g_r.ctrlRep, 3);
    J(",\"stopMin\":%.0f,\"stopMax\":%.0f,\"stopBar\":%.0f", RUN_STOP_MIN_PCT, RUN_STOP_MAX_PCT, RUN_STOP_BAR_PCT); }
  J(",\"bar\":"); JN(tirepressure::bar(), 2); J(",\"barWarm\":"); JN(g_r.barWarm, 2); J(",\"tTire\":"); JN(tirepressure::tempC(), 1);
  J(",\"meta\":{\"t\":"); JS(g_r.meta.tire); J(",\"w\":"); JS(g_r.meta.rim); J(",\"h\":"); JS(g_r.meta.hub);
  J(",\"u\":%d,\"k\":", g_r.meta.tireNew); JN(g_r.meta.treadMm, 2);
  J(",\"q\":%d,\"qmiss\":", g_r.profile); JS(profiles::missing(g_r.profile).c_str()); J(",\"n\":"); JS(g_r.meta.note);
  J(",\"r\":"); JS(g_r.meta.etrto); J(",\"s\":"); JS(g_r.meta.tube);
  J(",\"b\":"); JN(g_r.meta.bar, 2); J(",\"c\":"); JN(g_r.meta.tempC, 1); J(",\"l\":"); JN(g_r.meta.loadMeasKg, 1); J(",\"lc\":"); JN(g_r.meta.loadCalcKg, 1); J(",\"p\":%d}", g_r.meta.posMm);
  J(",\"checks\":[");
  for (uint8_t i = 0; i < g_r.checkN; i++) { J("%s[", i ? "," : ""); JS(g_r.checks[i].name); J(",%d,", g_r.checks[i].res); JS(g_r.checks[i].val); J("]"); }
  J("],\"pre\":[");
  for (uint8_t i = 0; i < g_r.preN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.pre[i], false); }
  J("],\"a0\":"); JN(g_r.a0, 5); J(",\"b0\":"); JN(g_r.b0, 7); J(",\"a0Old\":"); JN(g_r.a0Old, 5);
  J(",\"post\":[");
  for (uint8_t k = 0; g_r.postN && k < g_r.plateauN; k++) { const Drift d = drift(k); J("%s[", k ? "," : ""); JN(g_r.plateaus[k], 1); J(","); JN(d.post, 4); J(",");
    JN(isnan(d.n) ? NAN : fabsf(d.n), 3); J("]"); }
  J("],\"warm\":[");
  for (uint8_t i = 0; i < g_r.warmN; i++) { J("%s[%u,", i ? "," : "", g_r.warm[i].min); JN(g_r.warm[i].p, 1); J(","); JN(g_r.warm[i].crr, 5); J(","); JN(g_r.warm[i].bar, 2); J("]"); }
  J("],\"anchorPre\":[");
  for (uint8_t i = 0; i < g_r.anchorPreN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.anchorPre[i], true); }
  J("],\"pts\":[");
  for (uint8_t i = 0; i < g_r.measN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.meas[i], true); }
  J("],\"stat\":[");
  if (g_r.measN) for (uint8_t i = 0; i < g_r.plateauN; i++) { const PlStat s = plateauStat(i);
    J("%s[%.1f,%u,%u,", i ? "," : "", g_r.plateaus[i], s.n, s.all); JN(s.m, 4); J(","); JN(s.sd, 4); J(","); JN(s.cRaw, 5); J(","); JN(s.cCorr, 5); J(","); JN(s.u, 5); J(",%u]", s.dof); }
  J("],\"anchor\":[");
  for (uint8_t i = 0; i < g_r.anchorN; i++) { J("%s", i ? "," : ""); jpoint(buf, len, p, g_r.anchor[i], true); }
  J("],\"anchorKmh\":"); JN(g_r.anchorKmh, 1);
  { float d; bool have; anchorDrift(d, have); J(",\"anchorDN\":"); JN(have ? d / rDrum() : NAN, 3); }
  J(",\"warn\":[");
  for (uint8_t i = 0; i < g_r.warnN; i++) { J("%s", i ? "," : ""); JS(g_r.warn[i]); }
  // URLs of the file browser (webui.cpp /fs/<medium><path>), null while not written.
  { const char* mn = storage::name(static_cast<storage::Media>(g_r.fileMedia));
    char u[72];
    J("],\"md\":"); if (g_r.fileMd[0]) { snprintf(u, sizeof(u), "/fs/%s%s", mn, g_r.fileMd); JS(u); } else J("null");
    J(",\"csv\":"); if (g_r.fileCsv[0]) { snprintf(u, sizeof(u), "/fs/%s%s", mn, g_r.fileCsv); JS(u); } else J("null");
    J(",\"dir\":"); if (g_r.fileMd[0]) { snprintf(u, sizeof(u), "/fs/%s%s/%s", mn, storage::ROOT_DIR, g_r.id); JS(u); } else J("null"); }
  J(",\"plateaus\":[");
  for (uint8_t i = 0; i < g_r.plateauN; i++) J("%s%.1f", i ? "," : "", g_r.plateaus[i]);
  J("],\"plAll\":[");   // all Setup speeds, for the choice in step 1
  for (uint8_t i = 0; i < settings::plateauCount(); i++) J("%s%.1f", i ? "," : "", settings::plateauKmh(i));
  J("]}");
  if (p >= len) { buf[len - 1] = '\0'; }
}
#undef J
#undef JN
#undef JS

}  // namespace run
