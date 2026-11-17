//
// Fahrrad-Reifenprüfstand — Hauptprogramm
//
// Bedienung über die serielle Konsole (115200 Bd):
//
//   t          statische Tara (nur im Stillstand)
//   b          Leerlaufkennlinie fahren — Laufrad ABGEHOBEN
//   m          Messlauf fahren — Laufrad aufgelegt
//   s          Lauf beenden, Trommel läuft aus
//   c          Fehler quittieren (nur im Stillstand)
//   l<wert>    Auflast in N setzen, z. B. l490
//   p<wert>    Reifendruck in bar setzen, z. B. p6.5
//   n          Anzeigeseite weiterschalten
//   ?          Hilfe und Status
//
#include <Arduino.h>
#include "config.h"
#include "speed.h"
#include "torque.h"
#include "drive.h"
#include "display.h"
#include "logger.h"
#include "state.h"
#include "statusled.h"
#include "webui.h"
#include "webui_html.h"
#include "tirepressure.h"

namespace {

void printHelp() {
  logger::note("Befehle: t=Tara b=Baseline m=Messung s=Stop c=Quittieren");
  logger::note("         l<N>=Auflast p<bar>=Druck n=Seite ?=Hilfe");
  logger::note("         v<kmh>=Sollwert (nur IDLE) d=herunterfahren");
  Serial.print("# Zustand: ");       Serial.print(state::name());
  Serial.print("  Antrieb: ");       Serial.print(drive().name());
  Serial.print("  Zelle: ");         Serial.println(isCalibrated() ? "kalibriert"
                                                                   : "UNKALIBRIERT");
  if (state::phase() == state::FAULT) {
    Serial.print("# "); Serial.println(state::faultReason());
  }
}

void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;

  switch (cmd[0]) {
    case 't': state::startTare(); break;
    case 'b': state::startBaseline(); break;
    case 'm': state::startMeasure(); break;
    case 's': state::stopRun(); break;
    case 'c': state::clearFault(); break;
    case 'n': display::nextPage(); break;
    case '?': printHelp(); break;

    // Testmodus: Sollwert direkt vorgeben bzw. geregelt herunterfahren.
    // Nur im Stillstandszustand IDLE — waehrend eines Laufs fuehrt die
    // Zustandsmaschine den Sollwert, da darf niemand dazwischenfunken.
    // TODO: der Testmodus hat noch keinen eigenen Zustand in state::Phase.
    case 'v': {
      if (state::phase() != state::IDLE) { logger::note("Sollwert nur im Zustand IDLE"); break; }
      const float v = cmd.substring(1).toFloat();
      if (v < 0.0f || v > V_MAX_KMH) { logger::note("Sollwert ausserhalb 0..V_MAX_KMH"); break; }
      drive().setTargetKmh(v);
      Serial.print("# Sollwert = "); Serial.print(v, 1); Serial.println(" km/h");
      break;
    }
    case 'd': drive().setTargetKmh(0.0f); logger::note("Sollwert 0 — faehrt herunter"); break;

    case 'l': {
      const float v = cmd.substring(1).toFloat();
      if (v > 0.0f) {
        state::setLoadN(v);
        Serial.print("# Auflast = "); Serial.print(v, 2); Serial.println(" N");
      } else {
        logger::note("Auflast muss > 0 sein");
      }
      break;
    }
    case 'p': {
      const float v = cmd.substring(1).toFloat();
      if (v > 0.0f) {
        tirepressure::setManual(v);
        Serial.print("# Reifendruck = "); Serial.print(v, 2); Serial.println(" bar");
      } else {
        logger::note("Druck muss > 0 sein");
      }
      break;
    }
    default:
      logger::note("Unbekannter Befehl — '?' fuer Hilfe");
      break;
  }
}

// Antwort auf /api/state. Steht hier, weil nur die Hauptfirmware weiß, welche
// Module es gibt — der Webserver ist dadurch für beide Firmwarevarianten derselbe.
//
// Keine erfundenen Zahlen: was die Firmware nicht kennt, ist null, und die Oberfläche
// zeigt dafür einen Gedankenstrich.
//   crr  braucht die Kalibrierung UND die Leerlaufkennlinie
//   tmp  kommt später über UART vom Regler (docs/04-hardware.md §11.4)
//   mh, wh  speed.h führt für Motor-Hall und Laufrad keinen Zähler
void buildStateJson(char* buf, size_t len) {
  long mean = 0, ripple = 0;
  torque::displayStats(mean, ripple);

  snprintf(buf, len,
    "{\"ph\":\"%s\",\"v\":%.2f,\"vs\":%.2f,\"rpm\":%.0f,"
    "\"raw\":%ld,\"rip\":%ld,\"cal\":%s,\"crr\":null,\"tmp\":null,"
    "\"mk\":%lu,\"ix\":%lu,\"mh\":0,\"wh\":0,"
    "\"pi\":%u,\"rev\":%u,\"load\":%.1f,\"man\":%s,\"flt\":\"%s\"}",
    state::name(), speed::kmh(), drive().targetKmh(), speed::revPerSec() * 60.0f,
    mean, ripple, isCalibrated() ? "true" : "false",
    static_cast<unsigned long>(speed::markCount()),
    static_cast<unsigned long>(speed::revCount()),
    state::plateauIndex(), state::revsInPoint(),
    state::loadN(),
    (state::phase() == state::IDLE && drive().targetKmh() > 0.05f) ? "true" : "false",
    state::faultReason());
}

void pollSerial() {
  static String buf;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      handleCommand(buf);
      buf = "";
    } else if (buf.length() < 32) {
      buf += c;
    }
  }
}

}  // namespace

void setup() {
  logger::begin();
  delay(200);

  statusled::begin();   // Lebenszeichen; laeuft waehrend der uebrigen Initialisierung
  webui::begin(handleCommand, buildStateJson, WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);

  speed::begin();
  torque::begin();
  drive().begin();
  display::begin();
  tirepressure::begin();
  state::begin();

  logger::header();
  if (!display::present()) {
    logger::note("LCD nicht gefunden — Betrieb nur ueber die Konsole");
  }
  printHelp();
}

void loop() {
  statusled::service();
  webui::service();
  pollSerial();
  tirepressure::service();
  state::service();
  display::service();
}
