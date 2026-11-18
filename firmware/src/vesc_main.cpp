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
  Serial.println("#   i       Zustand: Regler, Waegezelle, Bedienverbindung");
  Serial.println("#   l       Drehrichtung anlernen (Trommel von Hand drehen)");
  Serial.println("#   x       Losbrechmoment messen (Tastverhaeltnis steigt langsam)");
  Serial.print("# Grenze: "); Serial.print(V_MAX_KMH, 0);
  Serial.println(" km/h (docs/07-sicherheit.md §5)");
}

// Zustand auf die Konsole. Die Oberflaeche zeigt all das auch, aber beim
// Anschliessen am Stand steht man mit dem Schraubendreher davor und nicht mit dem
// Telefon — und wenn eine der beiden Verbindungen fehlt, ist der Browser genau
// das, was man gerade nicht hat.
void printStatus() {
  const vesc::Values& v = vesc::values();
  long mean = 0, ripple = 0;
  torque::displayStats(mean, ripple);

  Serial.println("# ---- Zustand ----");

  Serial.print("# Regler   ");
  if (vesc::connected()) {
    Serial.print("DA");
    if (vesc::fwKnown()) {
      Serial.print(", FW "); Serial.print(vesc::fwMajor());
      Serial.print('.');     Serial.print(vesc::fwMinor());
      Serial.print(", HW "); Serial.print(vesc::hwName());
    }
    Serial.print(" | "); Serial.print(v.vIn, 1);    Serial.print(" V");
    Serial.print(" | "); Serial.print(v.erpm);      Serial.print(" ERPM");
    Serial.print(" | Imot "); Serial.print(v.currentMotor, 1);
    Serial.print(" A | Iin "); Serial.print(v.currentIn, 1);
    Serial.print(" A | Duty "); Serial.print(v.duty, 3);
    Serial.print(" | FET "); Serial.print(v.tempFet, 1); Serial.print(" C");
    Serial.print(" | Motor ");
    if (v.tempMotor < MOTOR_TEMP_OPEN_C) Serial.print("FUEHLER OFFEN");
    else { Serial.print(v.tempMotor, 1); Serial.print(" C"); }
    Serial.print(" | Fehler "); Serial.print(vesc::faultText(v.fault));
    // Das Timeout-Bit ist im STILLSTAND der Normalfall und keine Stoerung: der
    // Sollwert wird nur wiederholt, solange gefahren wird (vesc.h). Wird nicht
    // gefahren, laeuft der reglereigene Timeout ab -- und genau so soll es sein,
    // denn ein blosser Herzschlag haette den Regler scharf gehalten, ohne dass
    // ein Sollwert dahinterstand.
    if (v.status & 0x01)
      Serial.print(speedctl::running() ? " | TIMEOUT IM LAUF!"
                                       : " | Timeout (erwartet im Stillstand)");
    if (v.status & 0x02) Serial.print(" | KILL-SWITCH");
    Serial.println();
    Serial.print("#          Fahren erst ueber l_battery_cut_end des Reglers;"
                 " die Grenzen stehen in hardware/vesc-config-fw7.01/mcconf.xml");
    Serial.println();
  } else {
    Serial.print("FEHLT — keine Antwort auf TX GPIO ");
    Serial.print(PIN_DRIVE_TX);
    Serial.print(" / RX GPIO ");  Serial.print(PIN_DRIVE_RX);
    Serial.print(" bei ");        Serial.print(VESC_BAUD);
    Serial.println(" Bd");
    if (v.seen) {
      Serial.print("#          schon einmal geantwortet, letztes Paket vor ");
      Serial.print(v.ageMs); Serial.println(" ms");
    } else {
      Serial.println("#          noch nie geantwortet. Pruefen: TX/RX vertauscht?"
                     " app_to_use = UART? COMM-Masse?");
    }
  }

  Serial.print("# Antrieb  ");
  if (speedctl::breakingAway()) {
    Serial.print("Losbrechmessung | Stellwert ");
    Serial.print(speedctl::duty(), 4);
    Serial.print(" | Imot ");  Serial.print(v.currentMotor, 1);
    Serial.print(" A | Ist "); Serial.print(speedctl::kmh(), 1);
    Serial.println(" km/h");
  } else if (speedctl::learning()) {
    Serial.println("lernt Drehrichtung \u2014 Trommel in Messrichtung drehen");
  } else {
    Serial.print(speedctl::running() ? "faehrt" : "steht");
    Serial.print(" | Soll ");  Serial.print(speedctl::targetKmh(), 0);
    Serial.print(" | Rampe "); Serial.print(speedctl::rampKmh(), 1);
    Serial.print(" | Ist ");   Serial.print(speedctl::kmh(), 1);
    Serial.print(" km/h | Stellwert "); Serial.print(speedctl::duty(), 3);
    Serial.print(" | Richtung "); Serial.print(settings::directionSign());
    Serial.println();
  }

  // Sollwert steht an, Trommel dreht nicht: der Regler hat die Kommutierung
  // verloren. Mit der unebenen Hall-Tabelle passiert das unterhalb von
  // foc_hall_interp_erpm, wo die rohe Tabelle gilt (docs/04-hardware.md 11.2).
  if (speedctl::running() && speedctl::rampKmh() > 3.0f && speedctl::kmh() < 1.0f)
    Serial.println("#          ACHTUNG Sollwert steht an, Trommel dreht nicht."
                   " Kommutierung verloren \u2014 Hall-Tabelle erneut erkennen.");

  // Der offene Fuehler ist zugleich der Verdacht auf die Hall-Geber: TMP und die
  // drei Hall-Adern liegen im SELBEN 6-poligen Stecker (docs/04-hardware.md
  // 11.5). Am 2026-11-18 war genau das der Fall -- Stecker nicht gesteckt,
  // -82 C am Fuehler, und der Motor zuckte nur.
  if (vesc::connected() && v.tempMotor < MOTOR_TEMP_OPEN_C)
    Serial.println("#          ACHTUNG Sensorstecker: TMP offen. Die Hall-Adern"
                   " liegen im selben Stecker, also auch verdaechtig.");

  Serial.print("# Zelle    ");
  if (torque::isResponding()) {
    Serial.print("DA | "); Serial.print(mean);
    Serial.print(" counts | Spitze-Spitze "); Serial.print(ripple);
    Serial.print(" | Null ");  Serial.print(torque::tareOffset());
    if (torque::isSaturated()) Serial.print(" | AM ANSCHLAG");
    if (!settings::calibrated()) Serial.print(" | nicht kalibriert");
    Serial.println();
  } else {
    Serial.print("FEHLT — HX711 antwortet nicht auf DOUT GPIO ");
    Serial.print(PIN_HX711_DOUT);
    Serial.print(" / SCK GPIO "); Serial.println(PIN_HX711_SCK);
    Serial.println("#          Pruefen: VCC und VDD versorgt? GND gemeinsam?"
                   " DOUT/SCK vertauscht?");
  }

  Serial.print("# Bedienung ");
  Serial.print(webui::linkAlive() ? "Browser verbunden" : "kein Browser");
  Serial.print(" | WLAN \""); Serial.print(WEBUI_SSID);
  Serial.print("\" | http://"); Serial.print(WEBUI_HOSTNAME);
  Serial.print(".local/ oder http://");
  for (uint8_t i = 0; i < 4; ++i) {
    Serial.print(WEBUI_AP_IP[i]);
    if (i < 3) Serial.print('.');
  }
  Serial.println("/");

  if (speedctl::breakawayDuty() > 0.0f) {
    Serial.print("# Losbruch bei Stellwert ");
    Serial.print(speedctl::breakawayDuty(), 4);
    Serial.print(", Imot ");  Serial.print(speedctl::breakawayAmps(), 1);
    Serial.print(" A, Moment ");
    Serial.print(speedctl::breakawayAmps() * MOTOR_KT_NM_PER_A, 2);
    Serial.print(" Nm | Laufreibung zum Vergleich ");
    Serial.print(DRUM_LOSS_CONST_NM, 3); Serial.println(" Nm");
  }

  Serial.print("# Letzte Meldung: ");
  Serial.println(g_note[0] ? g_note : "\u2014");
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
    case 'l':
      if (speedctl::running()) { note("erst anhalten"); break; }
      speedctl::learn();
      note("Trommel jetzt von Hand in Messrichtung drehen");
      Serial.println("# Antrieb freigegeben. Trommel in MESSRICHTUNG drehen,"
                     " mindestens 2 km/h.");
      break;
    case 'x':
      if (speedctl::running()) { note("erst anhalten"); break; }
      speedctl::breakaway();
      note("Losbrechmessung laeuft \u2014 Trommel nicht anfassen");
      Serial.println("# Losbrechmessung: Tastverhaeltnis steigt mit 0,01/s,"
                     " Abbruch bei 0,15 oder sobald sie sich regt.");
      break;
    case 'i': printStatus(); break;
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

  // Offene Klemme ist keine Temperatur. Der Wert geht als null heraus, damit die
  // Oberflaeche einen Gedankenstrich zeigt, und der Grund als eigenes Merkmal.
  const bool tmotOffen = ok && v.tempMotor < MOTOR_TEMP_OPEN_C;
  char sTmot[16];
  num(sTmot, sizeof(sTmot), (!ok || tmotOffen) ? NAN : v.tempMotor, 1);

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
    "\"tfet\":%.1f,\"tmot\":%s,\"tmotOffen\":%s,"
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
    ok ? v.tempFet : 0.0f, sTmot, tmotOffen ? "true" : "false",
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
