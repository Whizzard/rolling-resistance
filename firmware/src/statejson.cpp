#include "calib.h"
#include "build_id.h"
#include "statejson.h"


#include "config.h"
#include "calc/linfit.h"
#include "calc/loss.h"
#include "calc/json.h"
#include "settings.h"
#include "webui.h"
#include "console.h"
#include "live.h"
#include "brake.h"
#include "statusled.h"
#include "torque.h"
#include "speedctl.h"
#include "revsrc.h"
#include "speed.h"
#include "state.h"
#include "logger.h"
#include "tirepressure.h"
#include "vesc/vesc.h"
#include "vesc/packet.h"
#include "wheel.h"
#include "run.h"
#include "storage.h"
#include "rawlog.h"

namespace statejson {
namespace {

// A number or null. No invented values — what is not measured is null, and the
// user interface shows a dash for it.
void num(char* out, size_t n, float v, uint8_t nk) {
  if (!isfinite(v)) snprintf(out, n, "null");   // inf too: JSON does not know it, JSON.parse fails
  else          snprintf(out, n, "%.*f", nk, v);
}

}  // namespace

void build(char* buf, size_t len) {
  const vesc::Values& v = vesc::values();
  const bool ok = vesc::connected();

  long mean = 0, ripple = 0;
  torque::displayStats(mean, ripple);
  const long counts = mean - torque::tareOffset();

  const float fN   = torque::newtons();
  const float pct  = isnan(fN) ? NAN : fabsf(fN) / LOADCELL_CAPACITY_N * 100.0f;
  const float kg   = isnan(fN) ? NAN : fN / 9.80665f;
  const float mNm  = torque::momentNm();
  const float roll = torque::rollForceN();

  // An open terminal is not a temperature. The value goes out as null so the user
  // interface shows a dash, and the reason as a separate flag.
  const bool tmotOpen = ok && v.tempMotor < MOTOR_TEMP_OPEN_C;
  char sTmot[16];
  num(sTmot, sizeof(sTmot), (!ok || tmotOpen) ? NAN : v.tempMotor, 1);

  // Wiring test of the sensors: per channel [pulses, ms since the last one, low].
  // ms is null as long as no pulse has come yet.
  char sSensors[128];   // 4 x at most 30 characters
  size_t gp = 0;
  for (uint8_t i = 0; i < speed::CH_N; i++) {
    const speed::ChannelState c = speed::channel(static_cast<speed::Channel>(i));
    char sMs[12];
    if (c.count) snprintf(sMs, sizeof(sMs), "%lu", static_cast<unsigned long>(c.sinceMs));
    else         snprintf(sMs, sizeof(sMs), "null");
    gp += snprintf(sSensors + gp, sizeof(sSensors) - gp, "%s[%lu,%s,%s]",
                   i ? "," : "", static_cast<unsigned long>(c.count), sMs,
                   c.low ? "true" : "false");
  }

  // Wheel: speeds and integrated circumference (wheel.h). Internally m and 1/s;
  // 1/min and mm only arise here, at the output boundary.
  const float nDrum  = revsrc::revPerSec();
  const float nWheel = speed::wheelRevPerSec();
  char sUWheel[16], sUWheelMem[16];
  num(sUWheel, sizeof(sUWheel), wheel::circumferenceM() * 1000.0f, 0);
  num(sUWheelMem, sizeof(sUWheelMem), settings::wheelCircM() * 1000.0f, 0);

  // Measurement speeds as a JSON list, ordered as in the settings.
  char sPlat[PLATEAU_MAX * 7 + 2];
  size_t pp = 0;
  for (uint8_t i = 0; i < settings::plateauCount(); i++)
    pp += snprintf(sPlat + pp, sizeof(sPlat) - pp, "%s%.1f", i ? "," : "",
                   settings::plateauKmh(i));
  if (pp == 0) sPlat[0] = '\0';
  char sLoad[16];
  num(sLoad, sizeof(sLoad), state::loadN(), 1);

  // Live only at steady speed, averaged (liveResult). Without idle-loss model "raw".
  const live::Result lv = live::result();
  char sPW[16], sCrr[16];
  num(sPW,  sizeof(sPW),  lv.pW,  1);
  num(sCrr, sizeof(sCrr), lv.crr, 4);
  char sA0[16], sB0[16], sA1[16], sB1[16], sMWheel[16], sJ[16];
  num(sA0, sizeof(sA0), settings::baselineA0(), 4);
  num(sB0, sizeof(sB0), settings::baselineB0(), 7);
  num(sA1, sizeof(sA1), settings::wheelA1(), 5);
  num(sB1, sizeof(sB1), settings::wheelB1(), 8);
  num(sMWheel, sizeof(sMWheel), settings::wheelMassKg(), 3);
  num(sJ, sizeof(sJ), wheel::jWheelKgm2(), 4);

  // Tire pressure and temperature (tirepressure.h): BLE before manual entry, age in ms.
  char sBar[16], sTTire[16], sPBatt[16], sPAge[16];
  num(sBar, sizeof(sBar), tirepressure::bar(), 2);
  num(sTTire, sizeof(sTTire), tirepressure::tempC(), 1);
  num(sPBatt, sizeof(sPBatt), tirepressure::battery(), 0);
  if (tirepressure::ageMs() == UINT32_MAX) snprintf(sPAge, sizeof(sPAge), "null");
  else snprintf(sPAge, sizeof(sPAge), "%lu", static_cast<unsigned long>(tirepressure::ageMs()));
  char sJMeas[16];
  num(sJMeas, sizeof(sJMeas), settings::wheelJKgm2(), 4);
  char sLd[96];
  snprintf(sLd, sizeof(sLd), "\"ldEmpty\":%.3f,\"ldWeight\":%.3f,\"ldArm\":%.4f", settings::loadEmptyKg(), settings::loadWeightKg(), settings::loadArmM());
  // Drum marks: where the revolution comes from (speed.h RevSource) and counters of the evaluation.
  char sMark[80];
  snprintf(sMark, sizeof(sMark), "\"mSrc\":%u,\"mPpr\":%u,\"mRes\":%lu,\"mMiss\":%lu,\"mGl\":%lu",
           static_cast<unsigned>(speed::revSource()), static_cast<unsigned>(speed::pulsesPerRev()),
           static_cast<unsigned long>(speed::patternResyncs()), static_cast<unsigned long>(speed::missedPulses()),
           static_cast<unsigned long>(speed::glitchPulses()));

  char sN[16], sPct[16], sKg[16], sM[16], sRoll[16];
  num(sN,   sizeof(sN),   fN,   2);
  num(sPct, sizeof(sPct), pct,  1);
  num(sKg,  sizeof(sKg),  kg,   3);
  num(sM,   sizeof(sM),   mNm,  3);
  num(sRoll,sizeof(sRoll),roll, 3);

  // Strings escaped (calc/json.h): a " in a message or in the BLE name of the
  // pressure sensor would otherwise break the JSON and freeze the user interface.
  // Speeds via num(): since 2026-11-25 they can be NAN (controller gone), and %.1f
  // would write "nan", which JSON.parse rejects.
  char sNote[200], sErr[200], sPName[80], sFault[120], sKmh[16], sNDrum[16], sNWheel[16];
  calc::jsonStr(sNote, sizeof(sNote), console::lastNote());
  calc::jsonStr(sErr, sizeof(sErr), console::currentError());
  calc::jsonStr(sPName, sizeof(sPName), tirepressure::name());
  calc::jsonStr(sFault, sizeof(sFault), state::faultReason());
  num(sKmh, sizeof(sKmh), revsrc::kmh(), 1);
  num(sNDrum, sizeof(sNDrum), nDrum * 60.0f, 1);
  num(sNWheel, sizeof(sNWheel), nWheel * 60.0f, 1);
  snprintf(buf, len,
    "{\"build\":\"" __DATE__ " " __TIME__ "\","
    "\"link\":%s,\"fault\":%u,\"faultTxt\":\"%s\",\"status\":%u,\"age\":%lu,"
    "\"fw\":\"%s\",\"note\":%s,\"noteBad\":%s,\"braking\":%s,"
    "\"kmh\":%s,\"target\":%.1f,\"ramp\":%.1f,\"duty\":%.3f,\"driving\":%s,"
    "\"erpm\":%ld,\"rpm\":%.1f,"
    "\"vin\":%.1f,\"imot\":%.2f,\"iin\":%.2f,\"watt\":%.0f,"
    "\"tfet\":%.1f,\"tmot\":%s,\"tmotOpen\":%s,"
    "\"counts\":%ld,\"raw\":%ld,\"rawNow\":%ld,\"tare\":%ld,\"ripple\":%ld,\"n\":%s,\"pct\":%s,\"kg\":%s,"
    "\"nm\":%s,\"roll\":%s,\"pw\":%s,\"crr\":%s,\"corr\":%s,\"steady\":%s,"
    "\"a0\":%s,\"b0\":%s,\"a1\":%s,\"b1\":%s,\"mWheel\":%s,\"jWheel\":%s,\"jMeas\":%s,\"spindown\":%s,\"spindownN\":%lu,"
    "\"bar\":%s,\"tTire\":%s,\"pSrc\":\"%s\",\"pName\":%s,\"pBatt\":%s,\"pAge\":%s,\"pFresh\":%s,\"pConn\":%s,\"revSrc\":\"%s\","
    "\"relayBrake\":\"%s\",\"wiz\":%u,\"wizKmh\":%.0f,\"taring\":%s,\"descending\":%s,\"uWheelMem\":%s,"
    "\"err\":%s,"
    "\"cal\":%s,\"saturated\":%s,\"cellOk\":%s,"
    "\"dir\":%d,\"learning\":%s,\"brk\":%s,\"brkDuty\":%.4f,\"brkAmps\":%.1f,\"dirDet\":%s,\"dirRes\":%d,\"dirDN\":%.3f,"
    "\"up\":%lu,\"sensors\":[%s],%s,%s,"
    "\"nDrum\":%s,\"nWheel\":%s,\"uWheel\":%s,\"uWheelN\":%lu,\"uWheelMin\":%lu,"
    "\"plateaus\":[%s],\"phase\":\"%s\",\"run\":%u,\"plateau\":%u,"
    "\"sfault\":%s,\"load\":%s,\"motorOpen\":%s,\"estop\":%s,"
    "\"drumU\":%.4f,\"arm\":%.4f,\"npc\":%.8f,"
    "\"vmax\":%.0f,\"vmin\":%.0f,\"step\":%.0f,\"fsN\":%.1f,\"maxN\":%.1f,\"mMin\":%.2f,\"mMax\":%.2f,\"stopMin\":%.0f,\"stopMax\":%.0f,\"stopBar\":%.0f,\"rs\":%u}",
    ok ? "true" : "false",
    static_cast<unsigned>(v.fault), vesc::faultText(v.fault),
    static_cast<unsigned>(v.status), static_cast<unsigned long>(v.ageMs),
    vesc::fwKnown() ? String(String(vesc::fwMajor()) + "." + String(vesc::fwMinor())).c_str() : "",
    sNote, console::lastNoteBad() ? "true" : "false", vesc::braking() ? "true" : "false",
    sKmh, speedctl::targetKmh(), speedctl::rampKmh(),
    speedctl::duty(), speedctl::running() ? "true" : "false",
    static_cast<long>(ok ? v.erpm : 0),
    ok ? fabsf(static_cast<float>(v.erpm)) / MOTOR_POLE_PAIRS : 0.0f,
    ok ? v.vIn : 0.0f, ok ? v.currentMotor : 0.0f, ok ? v.currentIn : 0.0f,
    ok ? v.vIn * v.currentIn : 0.0f,
    ok ? v.tempFet : 0.0f, sTmot, tmotOpen ? "true" : "false",
    counts, mean, torque::lastRaw(), torque::tareOffset(), ripple, sN, sPct, sKg, sM, sRoll, sPW, sCrr,
    lv.corrected ? "true" : "false", live::steady() ? "true" : "false",
    sA0, sB0, sA1, sB1, sMWheel, sJ, sJMeas, wheel::spindownActive() ? "true" : "false",
    static_cast<unsigned long>(wheel::spindownRevs()),
    sBar, sTTire, tirepressure::sourceName(), sPName, sPBatt, sPAge,
    tirepressure::fresh() ? "true" : "false", tirepressure::connected() ? "true" : "false", revsrc::sourceName(),
    brake::stateName(), static_cast<unsigned>(state::wizardStep()), WIZ_KMH, torque::tareBusy() ? "true" : "false",
    speedctl::slowingDown() ? "true" : "false", sUWheelMem,
    sErr,
    settings::calibrated() ? "true" : "false",
    torque::isSaturated() ? "true" : "false",
    torque::isResponding() ? "true" : "false",
    static_cast<int>(settings::directionSign()),
    speedctl::learning() ? "true" : "false",
    speedctl::breakingAway() ? "true" : "false",
    speedctl::breakawayDuty(), speedctl::breakawayAmps(),
    speedctl::detectingDirection() ? "true" : "false", speedctl::directionResult(),
    isnan(speedctl::directionDeltaN()) ? 0.0 : static_cast<double>(speedctl::directionDeltaN()),
    static_cast<unsigned long>(millis()), sSensors, sMark, sLd,
    sNDrum, sNWheel, sUWheel,
    static_cast<unsigned long>(wheel::revsUsed()),
    static_cast<unsigned long>(WHEEL_CIRC_WINDOW_REVS),
    sPlat, state::name(), static_cast<unsigned>(state::runMode()),
    static_cast<unsigned>(state::plateauIndex()), sFault, sLoad,
    speedctl::motorOpen() ? "true" : "false", speedctl::estopInput() ? "true" : "false",
    settings::drumCircumferenceM(), settings::torqueArmM(),
    settings::loadcellNPerCount(),
    V_MAX_KMH, V_MIN_KMH, SPEED_STEP_KMH, LOADCELL_CAPACITY_N, MAX_FORCE_N, MEAS_TORQUE_MIN_NM, MEAS_TORQUE_MAX_NM, RUN_STOP_MIN_PCT, RUN_STOP_MAX_PCT, RUN_STOP_BAR_PCT,
    static_cast<unsigned>(run::step()));

  // Storage and raw data (storage.h, rawlog.h), since 2026-11-25. Appended instead of in
  // the big call above: there every argument counts, here not.
  // sd/fl: 0 missing, 1 ready, 2 read-only, 3 unreadable; Pct null = unknown.
  const size_t used = strlen(buf);
  if (used < 2 || buf[used - 1] != '}') return;
  const storage::Info sd = storage::info(storage::SD_CARD), fl = storage::info(storage::FLASH);
  char sdPct[8], flPct[8];
  if (sd.pct >= 0) snprintf(sdPct, sizeof(sdPct), "%d", sd.pct); else snprintf(sdPct, sizeof(sdPct), "null");
  if (fl.pct >= 0) snprintf(flPct, sizeof(flPct), "%d", fl.pct); else snprintf(flPct, sizeof(flPct), "null");
  snprintf(buf + used - 1, len - used + 1,
    ",\"sto\":{\"sd\":%u,\"sdPct\":%s,\"sdMB\":%lu,\"sdFmt\":%s,\"sdFs\":\"%s\",\"sdFmtRes\":\"%s\",\"sdFmtN\":%u,\"sdFmtPct\":%u,\"sdFmtSec\":%lu,\"fl\":%u,\"flPct\":%s,\"flMB\":%lu,\"flFmt\":%s,\"flFs\":\"%s\","
    "\"rec\":%s,\"recMan\":%s,\"recOn\":\"%s\",\"recKB\":%lu,\"recPend\":%lu,\"recDrop\":%lu}}",
    static_cast<unsigned>(sd.state), sdPct, static_cast<unsigned long>(sd.total >> 20), sd.formatting ? "true" : "false", sd.fsType, storage::formatResult(storage::SD_CARD), static_cast<unsigned>(storage::formatsDone(storage::SD_CARD)),
    static_cast<unsigned>(storage::formatPct()), static_cast<unsigned long>(storage::formatSec()),
    static_cast<unsigned>(fl.state), flPct, static_cast<unsigned long>(fl.total >> 20), fl.formatting ? "true" : "false", fl.fsType,
    rawlog::active() ? "true" : "false", rawlog::manual() ? "true" : "false",
    rawlog::active() ? storage::name(rawlog::media()) : "",
    static_cast<unsigned long>(rawlog::bytesWritten() / 1024), static_cast<unsigned long>(rawlog::bytesPending() / 1024),
    static_cast<unsigned long>(rawlog::bytesDropped()));

  // Time of the last change per setting (settings::stampsJson), since 2026-11-26.
  const size_t u2 = strlen(buf);
  if (u2 < 2 || buf[u2 - 1] != '}' || len - u2 < 8) return;
  int k = snprintf(buf + u2 - 1, len - u2 + 1, ",\"chg\":");
  if (k <= 0) return;
  const size_t at = u2 - 1 + k;
  const size_t w = settings::stampsJson(buf + at, len - at);
  if (at + w + 2 <= len) { buf[at + w] = '}'; buf[at + w + 1] = '\0'; }
  else { buf[u2 - 1] = '}'; buf[u2] = '\0'; return; }   // does not fit: better without than broken JSON

  // Stand revision and firmware commit (Setup), since 2026-11-26.
  char rev[2 * STAND_REV_LEN + 8];
  calc::jsonStr(rev, sizeof(rev), settings::standRev());
  const size_t u3 = strlen(buf);
  const int r = snprintf(buf + u3 - 1, len - u3 + 1, ",\"rev\":%s,\"commit\":\"" FW_COMMIT "\"}", rev);
  if (r <= 0 || static_cast<size_t>(r) >= len - u3 + 1) { buf[u3 - 1] = '}'; buf[u3] = '\0'; return; }

  // Calibration (calib.h), since 2026-11-26: validity of the stored factor always, the
  // procedure while it is open.
  char why[160], whyJ[200];
  const int8_t cv = calib::validity(why, sizeof(why));
  calc::jsonStr(whyJ, sizeof(whyJ), why);
  const size_t u4 = strlen(buf);
  const int q = snprintf(buf + u4 - 1, len - u4 + 1, ",\"calV\":%d,\"calWhy\":%s}", cv, whyJ);
  if (q <= 0 || static_cast<size_t>(q) >= len - u4 + 1) { buf[u4 - 1] = '}'; buf[u4] = '\0'; return; }
  const size_t u5 = strlen(buf);
  const size_t c = calib::json(buf + u5 - 1, len - u5 - 1);
  if (c > 0 && u5 - 1 + c + 2 <= len) { buf[u5 - 1 + c] = '}'; buf[u5 + c] = '\0'; }
  else { buf[u5 - 1] = '}'; buf[u5] = '\0'; }
}

}  // namespace statejson
