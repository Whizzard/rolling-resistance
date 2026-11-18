//
// Testlauf-Variante — Hauptprogramm.
//
// Regler + Waegezelle, sonst nichts. Keine Hall-Geber an der Trommel, kein LCD,
// keine Zustandsmaschine, kein Messbetrieb mit Plateaus und Protokoll.
//
// Zweck: die ersten Testlaeufe fahren und dabei alles sehen, was es zu sehen gibt —
// Waegezelle, Geschwindigkeit, Temperaturen, Stroeme. Dazu eine Einrichtungsseite
// fuer Trommelumfang, Hebelarme und die Kalibrierung der Zelle.
//
// GESCHWINDIGKEIT kommt aus der Reglerdrehzahl, nicht von den Trommelmarken: die
// Hall-Geber an der Trommel sind noch nicht verdrahtet. Mit der am 2026-11-17
// gemessenen Polpaarzahl ist ERPM / 15 die Trommeldrehzahl, und das ist eine
// Rechnung, keine Schaetzung.
//
// DREI TOTMANNSCHALTUNGEN, unabhaengig voneinander:
//   1. Hier: bleibt der Bedienrechner laenger als LINK_TIMEOUT_MS aus, wird der
//      Antrieb freigegeben (Trommel laeuft aus).
//   2. Im Regler: bleibt laenger als timeout_msec kein Sollwert, stellt er selbst
//      ab. Greift auch dann, wenn DIESE Firmware stehenbleibt.
//   3. In Hardware: die Kurzschlussrelais am Not-Aus. Die einzige, die auch bei
//      Ausfall von Regler oder Rechner wirkt (docs/07-sicherheit.md §3).
//
#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "webui.h"
#include "webui_vesc_html.h"
#include "statusled.h"
#include "torque.h"
#include "speedctl.h"
#include "vesc/vesc.h"

namespace {

bool  g_linkWasAlive = true;
char  g_note[96] = "";          // letzte Rueckmeldung an die Oberflaeche

void note(const char* s) { strncpy(g_note, s, sizeof(g_note) - 1); g_note[sizeof(g_note)-1] = '\0'; }

void printHelp() {
  Serial.println("# Befehle:");
  Serial.println("#   v<kmh>  Zielgeschwindigkeit      s  STOP (bremst)");
  Serial.println("#   c       auslaufen lassen        t  Wägezelle nullen");
  Serial.println("#   k<kg>   kalibrieren mit Gewicht am Kalibrierarm");
  Serial.println("#   d<m>    Trommelumfang           a<m>  Zellenarm");
  Serial.println("#   b<m>    Kalibrierarm            r  Einstellungen zuruecksetzen");
  Serial.print("# Grenze: "); Serial.print(V_MAX_KMH, 0);
  Serial.println(" km/h (docs/07-sicherheit.md §5)");
}

// Kalibrierung: ein bekanntes Gewicht haengt am Kalibrierarm und erzeugt dort das
// Moment m·g·L_cal. Die Zelle sitzt am kuerzeren Arm L und sieht deshalb
//     F = m · g · L_cal / L
// Der Faktor ist diese Kraft geteilt durch den Rohwertzuwachs seit dem Nullabgleich.
void calibrate(float kg) {
  if (kg <= 0.0f) { note("Gewicht muss groesser null sein"); return; }
  if (!torque::isResponding()) { note("HX711 antwortet nicht"); return; }

  long mean, ripple;
  torque::displayStats(mean, ripple);
  const long delta = mean - torque::tareOffset();
  if (delta == 0) { note("kein Ausschlag — Gewicht wirklich angehaengt?"); return; }

  const float f = kg * 9.80665f * settings::calibArmM() / settings::torqueArmM();
  const float nPerCount = f / static_cast<float>(delta > 0 ? delta : -delta);

  if (!settings::setLoadcellNPerCount(nPerCount)) {
    note("Faktor unplausibel — nicht uebernommen");
    return;
  }
  snprintf(g_note, sizeof(g_note),
           "kalibriert: %.1f N auf %ld counts  ->  %.8f N/count", f, delta, nPerCount);
  Serial.print("# "); Serial.println(g_note);
  Serial.println("# In config.h und docs/03-parameter.md eintragen:");
  Serial.print("#   constexpr float LOADCELL_N_PER_COUNT = ");
  Serial.print(nPerCount, 8); Serial.println("f;");
}

// Dasselbe Befehlsvokabular fuer Konsole und Weboberflaeche (docs/10-firmware.md G5).
void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;
  const float arg = cmd.substring(1).toFloat();

  switch (cmd[0]) {
    case 'v':
      if (speedctl::setTargetKmh(arg)) {
        snprintf(g_note, sizeof(g_note), "Ziel %.0f km/h", arg);
      } else {
        note(speedctl::lastRefusal());
      }
      break;
    case 's':
      speedctl::stop();
      note("STOP — Bremsstrom 20 A");
      break;
    case 'c':
      speedctl::coast();
      note("freigegeben, Trommel laeuft aus");
      break;
    case 't':
      if (speedctl::running()) { note("erst anhalten"); break; }
      torque::tareStatic();
      snprintf(g_note, sizeof(g_note), "genullt bei %ld counts", torque::tareOffset());
      break;
    case 'k': calibrate(arg); break;
    case 'd':
      note(settings::setDrumCircumferenceM(arg) ? "Trommelumfang uebernommen"
                                                : "Trommelumfang unplausibel");
      break;
    case 'a':
      note(settings::setTorqueArmM(arg) ? "Zellenarm uebernommen"
                                        : "Zellenarm unplausibel");
      break;
    case 'b':
      note(settings::setCalibArmM(arg) ? "Kalibrierarm uebernommen"
                                       : "Kalibrierarm unplausibel");
      break;
    case 'r':
      if (speedctl::running()) { note("erst anhalten"); break; }
      settings::reset();
      note("Einstellungen auf die Vorgaben aus config.h zurueckgesetzt");
      break;
    case '?': printHelp(); note(""); break;
    default:  note("unbekannter Befehl"); break;
  }
}

// Eine Zahl oder null. Keine erfundenen Werte — was nicht gemessen ist, ist null,
// und die Oberflaeche zeigt dafuer einen Gedankenstrich.
void num(char* out, size_t n, float v, uint8_t nk) {
  if (isnan(v)) snprintf(out, n, "null");
  else          snprintf(out, n, "%.*f", nk, v);
}

void buildStateJson(char* buf, size_t len) {
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

  char sN[16], sPct[16], sKg[16], sM[16], sRoll[16];
  num(sN,   sizeof(sN),   fN,   2);
  num(sPct, sizeof(sPct), pct,  1);
  num(sKg,  sizeof(sKg),  kg,   3);
  num(sM,   sizeof(sM),   mNm,  3);
  num(sRoll,sizeof(sRoll),roll, 3);

  snprintf(buf, len,
    "{\"link\":%s,\"fault\":%u,\"faultTxt\":\"%s\",\"status\":%u,\"age\":%lu,"
    "\"fw\":\"%s\",\"note\":\"%s\","
    "\"kmh\":%.1f,\"soll\":%.0f,\"rampe\":%.1f,\"duty\":%.3f,\"faehrt\":%s,"
    "\"erpm\":%ld,\"rpm\":%.1f,"
    "\"vin\":%.1f,\"imot\":%.2f,\"iin\":%.2f,\"watt\":%.0f,"
    "\"tfet\":%.1f,\"tmot\":%.1f,"
    "\"counts\":%ld,\"ripple\":%ld,\"n\":%s,\"pct\":%s,\"kg\":%s,"
    "\"nm\":%s,\"roll\":%s,"
    "\"kal\":%s,\"saturiert\":%s,\"zelle\":%s,"
    "\"drumU\":%.4f,\"arm\":%.4f,\"calArm\":%.4f,\"npc\":%.8f,"
    "\"vmax\":%.0f,\"vmin\":%.0f,\"step\":%.0f,\"fsN\":%.1f}",
    ok ? "true" : "false",
    static_cast<unsigned>(v.fault), vesc::faultText(v.fault),
    static_cast<unsigned>(v.status), static_cast<unsigned long>(v.ageMs),
    vesc::fwKnown() ? String(String(vesc::fwMajor()) + "." + String(vesc::fwMinor())).c_str() : "",
    g_note,
    speedctl::kmh(), speedctl::targetKmh(), speedctl::rampKmh(),
    speedctl::duty(), speedctl::running() ? "true" : "false",
    static_cast<long>(ok ? v.erpm : 0),
    ok ? fabsf(static_cast<float>(v.erpm)) / MOTOR_POLE_PAIRS : 0.0f,
    ok ? v.vIn : 0.0f, ok ? v.currentMotor : 0.0f, ok ? v.currentIn : 0.0f,
    ok ? v.vIn * v.currentIn : 0.0f,
    ok ? v.tempFet : 0.0f, ok ? v.tempMotor : 0.0f,
    counts, ripple, sN, sPct, sKg, sM, sRoll,
    settings::calibrated() ? "true" : "false",
    torque::isSaturated() ? "true" : "false",
    torque::isResponding() ? "true" : "false",
    settings::drumCircumferenceM(), settings::torqueArmM(),
    settings::calibArmM(), settings::loadcellNPerCount(),
    V_MAX_KMH, V_MIN_KMH, SPEED_STEP_KMH, LOADCELL_CAPACITY_N);
}

void pollSerial() {
  static char line[32];
  static uint8_t n = 0;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[n] = '\0';
      if (n) handleCommand(String(line));
      n = 0;
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("# Reifenpruefstand — TESTLAUF-VARIANTE (Regler + Waegezelle)");

  settings::begin();
  statusled::begin();
  torque::begin();
  speedctl::begin();
  webui::begin(handleCommand, buildStateJson,
               WEBUI_VESC_HTML_GZ, WEBUI_VESC_HTML_GZ_LEN);
  vesc::begin();

  if (!settings::calibrated())
    Serial.println("# Waegezelle NICHT kalibriert — es gibt nur Rohwerte, keine Kraefte.");

  printHelp();
}

void loop() {
  statusled::service();
  webui::service();
  vesc::service();
  torque::update();
  speedctl::service();
  pollSerial();

  // Totmannschaltung: reisst die Bedienverbindung ab, wird der Antrieb freigegeben.
  // Nur die FLANKE auswerten, sonst wuerde jeder Durchlauf release() senden.
  const bool alive = webui::linkAlive();
  if (g_linkWasAlive && !alive && speedctl::running()) {
    speedctl::coast();
    note("Bedienverbindung weg — Antrieb freigegeben");
    Serial.println("# Bedienverbindung weg — Antrieb freigegeben");
  }
  g_linkWasAlive = alive;
}
