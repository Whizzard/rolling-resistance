#include "state.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "torque.h"
#include "drive.h"
#include "logger.h"
#include "tirepressure.h"
#include "webui.h"

namespace state {
namespace {

Phase g_phase = IDLE;
RunMode g_run = RUN_NONE;
uint8_t g_plateau = 0;
uint8_t g_revs = 0;
uint32_t g_settleStartMs = 0;
uint32_t g_faultSinceMs = 0;
float g_loadN = NAN;
const char* g_faultReason = "";

constexpr float SPEED_TOLERANCE_KMH = 0.5f;

void enterFault(const char* reason) {
  g_faultReason = reason;
  g_phase = FAULT;
  g_run = RUN_NONE;
  drive().stop();
  logger::note(reason);
}

// Abschaltkriterien. Siehe docs/07-sicherheit.md §4.
void checkFaults() {
  const char* reason = nullptr;

  if (torque::isSaturated())        reason = "FAULT: Waegezelle gesaettigt";
  else if (!torque::isResponding()) reason = "FAULT: HX711 antwortet nicht";
  else if (isCalibrated() && fabsf(torque::newtons()) > MAX_FORCE_N)
                                    reason = "FAULT: Kraftgrenze ueberschritten";
  else if (speed::revPerSec() * 60.0f > MAX_DRUM_RPM)
                                    reason = "FAULT: Drehzahlgrenze ueberschritten";
  else if (drive().targetKmh() > 1.0f && speed::stalled())
                                    reason = "FAULT: keine Drehzahlpulse";
  else if (speed::slipDetected())   reason = "FAULT: Riemenschlupf";
  else if (drive().isFaulted())     reason = "FAULT: Leistungsstufe";
  else if (!webui::linkAlive())     reason = "FAULT: Bedienverbindung weg";

  if (reason == nullptr) {
    g_faultSinceMs = 0;
    return;
  }
  // Entprellen: kurzzeitige Ausreißer sollen nicht abschalten.
  const uint32_t now = millis();
  if (g_faultSinceMs == 0) { g_faultSinceMs = now; return; }
  if (now - g_faultSinceMs >= FAULT_SETTLE_MS) enterFault(reason);
}

void gotoPlateau(uint8_t idx) {
  g_plateau = idx;
  drive().setTargetKmh(PLATEAUS_KMH[idx]);
  g_phase = SPINUP;
}

void startRun(RunMode mode) {
  if (g_phase != IDLE) {
    logger::note("Start abgelehnt: nicht im Zustand IDLE");
    return;
  }
  if (mode == RUN_MEASURE && isnan(g_loadN)) {
    logger::note("Start abgelehnt: Auflast nicht gesetzt (Befehl 'l<wert>')");
    return;
  }
  g_run = mode;
  logger::note(mode == RUN_BASELINE ? "Lauf BASELINE (Laufrad abgehoben)"
                                    : "Lauf MEASURE");
  gotoPlateau(0);
}

void finishPoint() {
  const float v = speed::kmh();
  const float raw = torque::averageRaw();
  const float fN = torque::averageN();
  const float mNm = isnan(fN) ? NAN : fN * settings::torqueArmM();
  const float pW = isnan(mNm) ? NAN : mNm * speed::omega();
  const float fRoll = isnan(mNm) ? NAN : mNm / settings::drumRadiusM();

  // Crr nur im Messlauf und nur mit bekannter Auflast. Im BASELINE-Lauf ist es
  // sinnlos; die Subtraktion macht die Auswertung, nicht die Firmware.
  float crr = NAN;
  if (g_run == RUN_MEASURE && !isnan(fRoll) && !isnan(g_loadN) && g_loadN > 0.0f) {
    crr = fRoll / g_loadN;
  }

  logger::point(name(), speed::revCount(), v, raw, fN, mNm, pW, crr,
                g_loadN, tirepressure::bar(),
                g_run == RUN_BASELINE ? "baseline" : "measure");
  logger::angleBins();

  if (g_plateau + 1 < PLATEAU_COUNT) {
    gotoPlateau(g_plateau + 1);
  } else {
    logger::note("Lauf beendet — Trommel laeuft aus");
    drive().stop();
    g_run = RUN_NONE;
    g_phase = COAST;
  }
}

}  // namespace

void begin() {
  g_phase = IDLE;
  g_run = RUN_NONE;
}

void service() {
  drive().service();

  // Neue Kraftwerte einsammeln und im Messzustand akkumulieren.
  if (torque::update() && g_phase == POINT) {
    torque::accumulate(speed::markIndexInRev());
  }

  // Indexmarke zählt die Umdrehungen des laufenden Messpunkts.
  const bool indexSeen = speed::consumeIndexFlag();

  if (g_phase != IDLE && g_phase != FAULT && g_phase != TARE) checkFaults();

  switch (g_phase) {
    case IDLE:
    case FAULT:
      break;

    case TARE:
      torque::tareStatic();
      logger::note("Statische Tara gesetzt");
      g_phase = IDLE;
      break;

    case SPINUP:
      if (fabsf(speed::kmh() - drive().targetKmh()) < SPEED_TOLERANCE_KMH) {
        g_settleStartMs = millis();
        g_phase = SETTLE;
      }
      break;

    case SETTLE:
      // Nach dem Plateauwechsel einschwingen lassen: Restbeschleunigung der
      // Schwungmasse und Schwingungen des Wiegeteils klingen hier ab.
      if (millis() - g_settleStartMs >= SETTLE_MS) {
        torque::resetAccumulator();
        g_revs = 0;
        g_phase = POINT;
      } else if (fabsf(speed::kmh() - drive().targetKmh()) > SPEED_TOLERANCE_KMH * 2) {
        g_phase = SPINUP;   // wieder ausgeschert
      }
      break;

    case POINT:
      if (indexSeen) {
        g_revs++;
        if (g_revs >= AVG_REVS_PER_POINT) finishPoint();
      }
      break;

    case COAST:
      if (speed::revPerSec() < 0.05f) {
        logger::note("Trommel steht");
        g_phase = IDLE;
      }
      break;
  }
}

Phase phase() { return g_phase; }
RunMode runMode() { return g_run; }

const char* name() {
  switch (g_phase) {
    case IDLE:   return "IDLE";
    case TARE:   return "TARE";
    case SPINUP: return "SPINUP";
    case SETTLE: return "SETTLE";
    case POINT:  return "POINT";
    case COAST:  return "COAST";
    case FAULT:  return "FAULT";
  }
  return "?";
}

void startBaseline() { startRun(RUN_BASELINE); }
void startMeasure()  { startRun(RUN_MEASURE); }

void startTare() {
  if (g_phase != IDLE) { logger::note("Tara nur im Stillstand"); return; }
  if (speed::revPerSec() > 0.05f) { logger::note("Tara nur im Stillstand"); return; }
  g_phase = TARE;
}

void stopRun() {
  drive().stop();
  g_run = RUN_NONE;
  if (g_phase != FAULT) g_phase = COAST;
}

void clearFault() {
  if (g_phase != FAULT) return;
  if (speed::revPerSec() > 0.05f) {
    logger::note("Quittierung nur im Stillstand");
    return;
  }
  g_faultReason = "";
  g_faultSinceMs = 0;
  g_phase = IDLE;
  logger::note("Fehler quittiert");
}

void setLoadN(float n) { g_loadN = n; }
float loadN() { return g_loadN; }

uint8_t plateauIndex() { return g_plateau; }
uint8_t revsInPoint() { return g_revs; }
const char* faultReason() { return g_faultReason; }

}  // namespace state
