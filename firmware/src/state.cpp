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
#include "wheel.h"

namespace state {
namespace {

Phase g_phase = IDLE;
RunMode g_run = RUN_NONE;
uint8_t g_plateau = 0;
uint8_t g_revs = 0;
uint32_t g_settleStartMs = 0;
uint32_t g_faultSinceMs = 0;
uint32_t g_faultStillMs = 0;   // seit wann die Trommel im FAULT steht
float g_loadN = NAN;
const char* g_faultReason = "";

constexpr float SPEED_TOLERANCE_KMH = 0.5f;

// Leerlaufmodell M_leer(w) = A0 + B0 w^2: waehrend eines Leerlauflaufs werden die
// Messpunkte (w^2, M) als Regressionssummen gesammelt, am Ende wird gefittet und
// gespeichert (settings::setBaseline). Keine Punktliste noetig.
struct Fit { double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
  void add(double x, double y) { n++; sx += x; sy += y; sxx += x * x; sxy += x * y; }
  bool solve(float& a, float& b) const {
    const double den = n * sxx - sx * sx;
    if (n < 3 || fabs(den) < 1e-12) return false;
    b = static_cast<float>((n * sxy - sx * sy) / den);
    a = static_cast<float>((sy - b * sx) / n);
    return true;
  }
};
Fit g_fit;

bool     g_wizard = false;      // Assistent aktiv (ueber alle Phasen hinweg)
bool     g_wizardDone = false;  // Leerlauflauf des Assistenten beendet
bool     g_wizardAborted = false;
uint32_t g_wizSteadyMs = 0, g_wizSlipMs = 0;
float    g_wizRatioRef = 0.0f;  // Rad/Trommel-Verhaeltnis mit aufliegendem Rad

void enterFault(const char* reason) {
  g_faultReason = reason;
  g_phase = FAULT;
  g_run = RUN_NONE;
  if (g_wizard) g_wizardAborted = true;
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

  // Kurz, die Oberflaeche zeigt den Text so wie er ist.
  if (torque::isSaturated())        reason = "FAULT: Waegezelle am Anschlag";
  else if (!torque::isResponding()) reason = "FAULT: HX711 antwortet nicht";
  else if (isCalibrated() && fabsf(torque::newtons()) > MAX_FORCE_N)
                                    reason = "FAULT: Kraftgrenze";
  else if (revsrc::revPerSec() * 60.0f > MAX_DRUM_RPM)
                                    reason = "FAULT: Drehzahlgrenze";
  else if (!vesc::connected())      reason = "FAULT: kein Regler";
  else if (vesc::values().fault != 0) {
    static char rbuf[64];
    snprintf(rbuf, sizeof(rbuf), "FAULT: Regler: %s", vesc::faultText(vesc::values().fault));
    reason = rbuf;
  }
  else if (speedctl::estopInput())  reason = "FAULT: Not-Aus";
  else if (speedctl::motorOpen())   reason = "FAULT: Trennung erkannt";
  else if (stillstand)              reason = "FAULT: Trommel dreht nicht";
  // Riemenschlupf und fehlende Drehzahlpulse prüfen nur, wenn die Trommelmarken
  // wirklich zählen. Gegen die eigene Rechenquelle wären beide tautologisch.
  else if (revsrc::angleReference() && speedctl::rampKmh() > 1.0f && speed::stalled())
                                    reason = "FAULT: keine Drehzahlimpulse";
  else if (revsrc::angleReference() && speed::slipDetected())
                                    reason = "FAULT: Riemenschlupf";
  else if (!webui::linkAlive())     reason = "FAULT: Bedienung weg";

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
  if (g_phase != IDLE && g_phase != WIZ_LIFT) {
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
  g_fit = Fit();
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
  if (g_run == RUN_BASELINE && !isnan(mNm)) {
    const double w = revsrc::omega();
    g_fit.add(w * w, mNm);
  }

  if (g_plateau + 1 < settings::plateauCount()) {
    gotoPlateau(g_plateau + 1);
  } else {
    if (g_run == RUN_BASELINE) {
      float a0, b0;
      if (g_fit.solve(a0, b0) && settings::setBaseline(a0, b0)) {
        Serial.print("# Leerlaufmodell gespeichert: A0="); Serial.print(a0, 4);
        Serial.print(" Nm  B0="); Serial.print(b0, 7); Serial.println(" Nm/(rad/s)^2");
        Serial.println("# In config.h und docs/03-parameter.md eintragen.");
      } else {
        logger::note("Leerlaufmodell nicht gespeichert: zu wenige Punkte oder unplausibel");
      }
    }
    if (g_wizard) g_wizardDone = true;
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
  g_loadN = settings::loadN();   // Vorgabe LOAD_DEFAULT_N oder der gespeicherte Wert
}

void service() {
  // Die Kraftwerte liest und akkumuliert die Hauptschleife — sie ruft
  // torque::update() und weiß damit als Einzige, wann ein neuer Rohwert da ist.
  //
  // Indexmarke zählt die Umdrehungen des laufenden Messpunkts.
  const bool indexSeen = revsrc::consumeIndexFlag();

  if (g_phase != IDLE && g_phase != FAULT && g_phase != TARE) checkFaults();   // auch WIZ_*

  switch (g_phase) {
    case IDLE:
      break;

    case FAULT:
      // Haelt, bis die Trommel steht; dann von selbst wieder IDLE. Ein rastender
      // Fehler, den jemand quittieren muss, hat am Stand nur genervt (2026-11-23).
      // Waehrend die Trommel noch dreht, bleibt er stehen und sichtbar.
      if (revsrc::revPerSec() < 0.05f) {
        if (g_faultStillMs == 0) g_faultStillMs = millis();
        else if (millis() - g_faultStillMs >= 2000) {
          g_faultReason = "";
          g_faultSinceMs = 0;
          g_faultStillMs = 0;
          g_phase = IDLE;
          logger::note("");
        }
      } else {
        g_faultStillMs = 0;
      }
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

    case WIZ_SPINUP:
      // Mit aufliegendem Rad auf WIZ_KMH, dann 3 s ruhig -> Abheben erlaubt.
      if (fabsf(revsrc::kmh() - WIZ_KMH) < 1.0f) {
        if (g_wizSteadyMs == 0) g_wizSteadyMs = millis();
        else if (millis() - g_wizSteadyMs > 3000 && speed::wheelRevPerSec() > 0.5f) {
          g_wizRatioRef = speed::wheelRevPerSec() / revsrc::revPerSec();
          g_wizSlipMs = 0;
          g_phase = WIZ_LIFT;
          logger::note("Rad jetzt abheben");
        }
      } else {
        g_wizSteadyMs = 0;
      }
      break;

    case WIZ_LIFT: {
      // Rad dreht langsamer, als es ueber die Trommel muesste -> abgehoben.
      const float ratio = (revsrc::revPerSec() > 0.5f)
          ? speed::wheelRevPerSec() / revsrc::revPerSec() / g_wizRatioRef : 1.0f;
      if (ratio < WIZ_SLIP_RATIO) {
        if (g_wizSlipMs == 0) g_wizSlipMs = millis();
        else if (millis() - g_wizSlipMs > 500) {
          logger::note("Rad abgehoben: Auslauf und Leerlauf laufen");
          wheel::spindownBegin();
          g_fit = Fit();
          g_run = RUN_BASELINE;
          gotoPlateau(0);             // Leerlauflauf, Rampe zum ersten Plateau
        }
      } else {
        g_wizSlipMs = 0;
      }
      break;
    }
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
    case WIZ_SPINUP: return "WIZ_SPINUP";
    case WIZ_LIFT:   return "WIZ_LIFT";
    case FAULT:  return "FAULT";
  }
  return "?";
}

void startBaseline() { startRun(RUN_BASELINE); }

void startWizard() {
  if (g_phase != IDLE) { logger::note("Start abgelehnt: nicht im Zustand IDLE"); return; }
  if (isnan(wheel::jRadKgm2())) {
    logger::note("Start abgelehnt: Laufradmasse oder Umfang fehlen"); return;
  }
  if (settings::plateauCount() == 0) { logger::note("Start abgelehnt: keine Messgeschwindigkeiten"); return; }
  if (!speedctl::setTargetKmh(WIZ_KMH)) { logger::note(speedctl::lastRefusal()); return; }
  g_wizard = true;
  g_wizardDone = false;
  g_wizardAborted = false;
  g_wizSteadyMs = 0;
  g_phase = WIZ_SPINUP;
  logger::note("Assistent: Trommel faehrt hoch, Rad bleibt aufgelegt");
}

uint8_t wizardStep() {
  if (!g_wizard) return 0;
  if (g_wizardAborted) return 9;
  if (g_phase == WIZ_SPINUP) return 1;
  if (g_phase == WIZ_LIFT) return 2;
  if (g_wizardDone && !wheel::spindownActive()) return 4;
  return 3;
}

void wizardAck() { g_wizard = false; g_wizardDone = false; g_wizardAborted = false; }
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
  if (g_wizard && !g_wizardDone) g_wizardAborted = true;
  if (g_phase != FAULT) g_phase = COAST;
}

void coastRun() {
  speedctl::coast();
  g_run = RUN_NONE;
  if (g_wizard && !g_wizardDone) g_wizardAborted = true;
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

void setLoadN(float n) { g_loadN = n; settings::setLoadN(n); }
float loadN() { return g_loadN; }

uint8_t plateauIndex() { return g_plateau; }
uint8_t revsInPoint() { return g_revs; }
const char* faultReason() { return g_faultReason; }

}  // namespace state
