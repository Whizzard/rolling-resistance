#include "state.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "revsrc.h"
#include "torque.h"
#include "speedctl.h"
#include "vesc/vesc.h"
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
  speedctl::stop();
  logger::note(reason);
}

// Abschaltkriterien. Siehe docs/07-sicherheit.md §4.
void checkFaults() {
  const char* reason = nullptr;

  // Stillstand trotz Sollwert. Ohne Trommelmarken kann speed::stalled() das nicht
  // sehen, und eine aus der Reglerdrehzahl gerechnete Umdrehung "stallt" nie —
  // deshalb hier über Stellwert und Drehzahl. Der Befund vom 2026-11-18: 0,045
  // Stellwert, 2,5 A Motorstrom, Trommel still. Das ist Kommutierung.
  const bool stillstand =
      speedctl::running() && speedctl::rampKmh() > 1.0f &&
      speedctl::duty() > STALL_DUTY_MIN && revsrc::kmh() < 1.0f;

  if (torque::isSaturated())        reason = "FAULT: Waegezelle gesaettigt";
  else if (!torque::isResponding()) reason = "FAULT: HX711 antwortet nicht";
  else if (isCalibrated() && fabsf(torque::newtons()) > MAX_FORCE_N)
                                    reason = "FAULT: Kraftgrenze ueberschritten";
  else if (revsrc::revPerSec() * 60.0f > MAX_DRUM_RPM)
                                    reason = "FAULT: Drehzahlgrenze ueberschritten";
  else if (!vesc::connected())      reason = "FAULT: kein Regler";
  else if (vesc::values().fault != 0) reason = "FAULT: Regler meldet Fehler";
  else if (stillstand)              reason = "FAULT: treibt, Trommel dreht nicht";
  // Riemenschlupf und fehlende Drehzahlpulse prüfen nur, wenn die Trommelmarken
  // wirklich zählen. Gegen die eigene Rechenquelle wären beide tautologisch.
  else if (revsrc::angleReference() && speedctl::rampKmh() > 1.0f && speed::stalled())
                                    reason = "FAULT: keine Drehzahlpulse";
  else if (revsrc::angleReference() && speed::slipDetected())
                                    reason = "FAULT: Riemenschlupf";
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
  if (!speedctl::setTargetKmh(settings::plateauKmh(idx))) {
    enterFault(speedctl::lastRefusal());
    return;
  }
  g_phase = SPINUP;
}

void startRun(RunMode mode) {
  if (g_phase != IDLE) {
    logger::note("Start abgelehnt: nicht im Zustand IDLE");
    return;
  }
  if (mode == RUN_MEASURE && isnan(g_loadN)) {
    logger::note("Start abgelehnt: Auflast nicht gesetzt (Befehl 'l<N>')");
    return;
  }
  if (settings::plateauCount() == 0) {
    logger::note("Start abgelehnt: keine Messgeschwindigkeiten (Befehl 'h<kmh>')");
    return;
  }
  g_run = mode;
  logger::note(mode == RUN_BASELINE ? "Leerlauf — Laufrad abgehoben"
                                    : "Messlauf");
  // Woher die Drehzahl kommt, steht EINMAL je Lauf im Protokoll — nicht je Punkt.
  if (!revsrc::angleReference()) {
    logger::note("Drehzahlquelle: Regler (ERPM/Polpaare). Keine Winkelreferenz, "
                 "die Winkel-Bins bleiben leer.");
  }
  gotoPlateau(0);
}

void finishPoint() {
  const float v = revsrc::kmh();
  const float raw = torque::averageRaw();
  const float fN = torque::averageN();
  const float mNm = isnan(fN) ? NAN : fN * settings::torqueArmM();
  const float pW = isnan(mNm) ? NAN : mNm * revsrc::omega();
  const float fRoll = isnan(mNm) ? NAN : mNm / settings::drumRadiusM();

  // Crr nur im Messlauf und nur mit bekannter Auflast. Im BASELINE-Lauf ist es
  // sinnlos; die Subtraktion macht die Auswertung, nicht die Firmware.
  float crr = NAN;
  if (g_run == RUN_MEASURE && !isnan(fRoll) && !isnan(g_loadN) && g_loadN > 0.0f) {
    crr = fRoll / g_loadN;
  }

  logger::point(name(), revsrc::revCount(), v, raw, fN, mNm, pW, crr,
                g_loadN, tirepressure::bar(),
                g_run == RUN_BASELINE ? "baseline" : "measure");
  if (revsrc::angleReference()) logger::angleBins();

  if (g_plateau + 1 < settings::plateauCount()) {
    gotoPlateau(g_plateau + 1);
  } else {
    logger::note("Lauf beendet — Trommel laeuft aus");
    speedctl::coast();   // frei auslaufen, nicht bremsen (docs/05-pruefablauf.md §B8)
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
  // Die Kraftwerte liest und akkumuliert die Hauptschleife — sie ruft
  // torque::update() und weiß damit als Einzige, wann ein neuer Rohwert da ist.
  //
  // Indexmarke zählt die Umdrehungen des laufenden Messpunkts.
  const bool indexSeen = revsrc::consumeIndexFlag();

  if (g_phase != IDLE && g_phase != FAULT && g_phase != TARE) checkFaults();

  switch (g_phase) {
    case IDLE:
    case FAULT:
      break;

    case TARE:
      if (!torque::tareBusy()) {
        logger::note("Statische Tara gesetzt");
        g_phase = IDLE;
      }
      break;

    case SPINUP:
      if (fabsf(revsrc::kmh() - speedctl::rampKmh()) < SPEED_TOLERANCE_KMH) {
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
      } else if (fabsf(revsrc::kmh() - speedctl::rampKmh()) > SPEED_TOLERANCE_KMH * 2) {
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
      if (revsrc::revPerSec() < 0.05f) {
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
  if (revsrc::revPerSec() > 0.05f) { logger::note("Tara nur im Stillstand"); return; }
  torque::tareBegin();
  g_phase = TARE;
}

void stopRun() {
  speedctl::stop();
  g_run = RUN_NONE;
  if (g_phase != FAULT) g_phase = COAST;
}

void coastRun() {
  speedctl::coast();
  g_run = RUN_NONE;
  if (g_phase != FAULT) g_phase = COAST;
}

void clearFault() {
  if (g_phase != FAULT) return;
  if (revsrc::revPerSec() > 0.05f) {
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
