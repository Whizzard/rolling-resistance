//
// Fahrrad-Reifenpruefstand — Hauptprogramm.
//
// EIN Pfad: Motorregler ueber UART, Waegezelle am HX711, Zustandsmaschine mit
// Plateaus und Protokoll, Bedienung ueber WLAN. Die frueheren zwei Varianten
// (Stub-Antrieb mit LCD und die Testlauf-Variante) sind am 2026-11-20
// zusammengelegt worden — was den Motor nie gefahren hat, ist weg.
// Begruendung: docs/04-hardware.md §14.
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
#include "webui_html.h"
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

namespace {

bool  g_linkWasAlive = true;
char  g_note[96] = "";          // letzte Rueckmeldung an die Oberflaeche

// Meldungen: note() ist ein Hinweis (Konsole und Oberflaeche, dort unauffaellig),
// fail() ein Fehler -- NUR die zeigt die Oberflaeche in der Meldezeile. Seit
// 2026-11-23: "Ziel 15 km/h" stand dort dauerhaft, obwohl die Kachel Soll das
// laengst zeigte; die Zeile ist fuer das reserviert, was der Bediener wissen muss.
bool g_noteBad = false;
uint32_t g_noteMs = 0;
void note(const char* s) { strncpy(g_note, s, sizeof(g_note) - 1); g_note[sizeof(g_note)-1] = '\0'; g_noteBad = false; g_noteMs = millis(); }
void fail(const char* s) { note(s); g_noteBad = true; }

// DER Fehlerzustand fuer die Oberflaeche: ein Text oder nichts. Die Seite fragt
// nur ab, ob da etwas steht, und zeigt es rot. Reihenfolge = Wichtigkeit.
// Befehlsfehler (fail) verschwinden nach fuenf Sekunden von selbst.
const char* currentError() {
  static char buf[96];
  const vesc::Values& v = vesc::values();
  if (!vesc::connected())            return "Kein Regler";
  if (v.fault != 0) { snprintf(buf, sizeof(buf), "Regler: %s", vesc::faultText(v.fault)); return buf; }
  if (speedctl::estopInput())        return "Not-Aus";
  if (speedctl::motorOpen())         return "Trennung erkannt";
  if (state::phase() == state::FAULT) {
    const char* r = state::faultReason();
    return strncmp(r, "FAULT: ", 7) == 0 ? r + 7 : r;
  }
  if (torque::isSaturated())         return "Waegezelle am Anschlag";
  if (!torque::isResponding())       return "HX711 antwortet nicht";
  if (v.tempMotor < MOTOR_TEMP_OPEN_C) return "Motorfuehler offen";
  if (speedctl::running() && speedctl::rampKmh() > 3.0f && speedctl::kmh() < 1.0f)
                                     return "Trommel dreht nicht";
  if (g_noteBad && g_note[0] && millis() - g_noteMs < 5000) return g_note;
  return "";
}

// Meldungen der Zustandsmaschine und der Regelung (logger::note) sind Fehler, wenn
// sie so anfangen.
bool looksBad(const char* s) {
  static const char* const BAD[] = {"Abbruch", "FAULT", "Start abgelehnt", "Tara nur",
                                    "Quittierung nur", "nicht", "kein"};
  for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++)
    if (strncmp(s, BAD[i], strlen(BAD[i])) == 0) return true;
  return false;
}

// Live-Telemetrie auf die Konsole (Befehl j<s>): eine Zeile alle 200 ms, s Sekunden
// lang. Fuer das Fehlersuchen ueber USB -- tools/konsole.py schreibt sie mit.
uint32_t g_liveUntil = 0, g_liveLast = 0;

// Laengste Schleifenrunde seit der letzten Abfrage. Der Webserver kann die
// Hauptschleife blockieren, und ab 300 ms schlaegt der Totmann des Reglers zu,
// ab 1 s gibt speedctl frei -- deshalb wird das gemessen, nicht vermutet.
uint32_t g_loopMaxMs = 0, g_loopPrevMs = 0, g_vescAgeMax = 0;

void liveTelemetry() {
  const uint32_t now = millis();
  if (now >= g_liveUntil || now - g_liveLast < 200) return;
  g_liveLast = now;
  const vesc::Values& v = vesc::values();
  Serial.print("# J ");      Serial.print(now);
  Serial.print(" ist=");     Serial.print(speedctl::kmh(), 1);
  Serial.print(" rampe=");   Serial.print(speedctl::rampKmh(), 1);
  Serial.print(" duty=");    Serial.print(speedctl::duty(), 3);
  Serial.print(" erpm=");    Serial.print(v.erpm);
  Serial.print(" imot=");    Serial.print(v.currentMotor, 2);
  Serial.print(" iin=");     Serial.print(v.currentIn, 2);
  Serial.print(" vin=");     Serial.print(v.vIn, 1);
  Serial.print(" fault=");   Serial.print(v.fault);
  Serial.print(" phase=");   Serial.print(state::name());
  long mean = 0, ripple = 0;
  torque::displayStats(mean, ripple);
  Serial.print(" counts=");  Serial.print(mean);
  Serial.print(" loopmax="); Serial.print(g_loopMaxMs);
  Serial.print(" vage=");    Serial.println(g_vescAgeMax);
  g_loopMaxMs = 0; g_vescAgeMax = 0;
}

void printHelp() {
  Serial.println("# Befehle:");
  Serial.println("#   v<kmh>  Zielgeschwindigkeit      s  STOP (bremst)");
  Serial.println("#   c       auslaufen lassen        q  Fehler quittieren");
  Serial.println("#   m0      Leerlauf (Rad abgehoben)       m1  Messlauf");
  Serial.println("#   h<kmh>  Messgeschwindigkeit dazu       h-<kmh>  entfernen");
  Serial.println("#   t       Waegezelle nullen       k<kg>  kalibrieren");
  Serial.println("#   l<N>    Auflast                 p<bar> Reifendruck");
  Serial.println("#   d<m>    Trommelumfang           a<m>   Zellenarm");
  Serial.println("#   b<m>    Kalibrierarm            z      Einstellungen zuruecksetzen");
  Serial.println("#   e       Drehrichtung einlernen (Trommel von Hand drehen); e1 / e-1 direkt");
  Serial.println("#   x       Losbrechmoment messen (Tastverhaeltnis steigt langsam)");
  Serial.println("#   i       Zustand: Regler, Waegezelle, Geber, Bedienverbindung");
  Serial.println("#   g       Geberzaehler nullen (Verdrahtungstest)");
  Serial.println("#   j<s>    Live-Telemetrie, alle 200 ms, s Sekunden (Vorgabe 5)");
  Serial.println("#   :<text> Terminalbefehl an den Regler (faults, hall_analyze 5, ...)");
  Serial.println("#   ><hex>  Rohpaket an den Regler, Antwort als Hex (z. B. >0e = mcconf)");
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
  if (speedctl::estopInput())
    Serial.println("#          NOT-AUS ausgeloest (Hilfskontakt).");
  if (speedctl::motorOpen())
    Serial.println("#          KEIN MOTOR AM REGLER: Stellwert lag an, aber kein Motorstrom."
                   " Not-Aus ausgeloest oder Motor abgeklemmt. Antrieb freigegeben.");

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

  // Verdrahtungstest: Trommel von Hand drehen, die Zaehler muessen mitlaufen.
  // Ein Kanal ohne Impulse ist entweder nicht verdrahtet oder sieht keinen
  // Magneten -- am Pegel allein ist das nicht zu unterscheiden, weil der
  // Pull-up beides high zieht.
  Serial.print("# Geber    ");
  for (uint8_t i = 0; i < speed::CH_N; i++) {
    const speed::ChannelState c = speed::channel(static_cast<speed::Channel>(i));
    if (i) Serial.print(" | ");
    Serial.print(speed::channelName(static_cast<speed::Channel>(i)));
    Serial.print(' '); Serial.print(c.count);
    if (c.count) { Serial.print(" vor "); Serial.print(c.sinceMs); Serial.print(" ms"); }
    else         { Serial.print(" KEINE IMPULSE"); }
    Serial.print(c.low ? " low" : " high");
  }
  Serial.println();

  // Laufradumfang: integriert ueber ein Fenster stetiger Fahrt (wheel.h).
  {
    Serial.print("# Laufrad  Trommel "); Serial.print(revsrc::revPerSec() * 60.0f, 1);
    Serial.print(" 1/min | Rad ");       Serial.print(speed::wheelRevPerSec() * 60.0f, 1);
    Serial.print(" 1/min | Umfang ");
    if (wheel::valid()) {
      Serial.print(wheel::circumferenceM() * 1000.0f, 0);
      Serial.print(" mm aus "); Serial.print(wheel::revsUsed()); Serial.println(" Radumdrehungen");
    } else {
      Serial.print("\u2014 ("); Serial.print(wheel::revsUsed());
      Serial.print(" von "); Serial.print(WHEEL_CIRC_WINDOW_REVS);
      Serial.println(" Radumdrehungen)");
    }
  }

  Serial.print("# Messung  ");
  Serial.print(state::name());
  if (state::phase() == state::FAULT) {
    Serial.print(" \u2014 "); Serial.print(state::faultReason());
    Serial.print(" (quittieren: q)");
  }
  if (state::runMode() != state::RUN_NONE) {
    Serial.print(state::runMode() == state::RUN_BASELINE ? " Leerlauf" : " Messlauf");
    Serial.print(" Plateau "); Serial.print(state::plateauIndex() + 1);
    Serial.print('/');          Serial.print(settings::plateauCount());
  }
  Serial.print(" | Geschwindigkeiten");
  for (uint8_t i = 0; i < settings::plateauCount(); i++) {
    Serial.print(' '); Serial.print(settings::plateauKmh(i), 1);
  }
  Serial.print(" km/h | Auflast ");
  if (isnan(state::loadN())) Serial.println("nicht gesetzt");
  else { Serial.print(state::loadN(), 1); Serial.println(" N"); }

  Serial.print("# Schleife laengste Runde "); Serial.print(g_loopMaxMs);
  Serial.print(" ms, aelteste Reglerwerte "); Serial.print(g_vescAgeMax);
  Serial.println(" ms seit letzter Abfrage");
  g_loopMaxMs = 0; g_vescAgeMax = 0;

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
  if (kg <= 0.0f) { fail("Gewicht muss groesser null sein"); return; }
  if (!torque::isResponding()) { fail("HX711 antwortet nicht"); return; }

  long mean, ripple;
  torque::displayStats(mean, ripple);
  const long delta = mean - torque::tareOffset();
  if (delta == 0) { fail("kein Ausschlag — Gewicht wirklich angehaengt?"); return; }

  const float f = kg * 9.80665f * settings::calibArmM() / settings::torqueArmM();
  const float nPerCount = f / static_cast<float>(delta > 0 ? delta : -delta);

  if (!settings::setLoadcellNPerCount(nPerCount)) {
    fail("Faktor unplausibel — nicht uebernommen");
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

  // Durchgriff zum Regler, nur von der Konsole sinnvoll (vesc.h).
  if (cmd[0] == ':') { vesc::terminal(cmd.c_str() + 1); return; }
  if (cmd[0] == '>') {
    uint8_t p[vesc::PAYLOAD_MAX];
    size_t n = 0;
    for (unsigned i = 1; i + 1 < cmd.length() && n < sizeof(p); i += 2) {
      char h[3] = {cmd[i], cmd[i + 1], 0};
      p[n++] = static_cast<uint8_t>(strtoul(h, nullptr, 16));
    }
    if (n) vesc::raw(p, n); else fail("Hexnutzlast fehlt");
    return;
  }

  switch (cmd[0]) {
    case 'v':
      if (speedctl::setTargetKmh(arg)) note("");      // Soll zeigt die Kachel
      else                             fail(speedctl::lastRefusal());
      break;
    case 's':
      state::stopRun();            // bremst und beendet einen laufenden Lauf
      note("STOP — Bremsstrom 20 A");
      break;
    case 'c':
      state::coastRun();           // gibt frei und beendet einen laufenden Lauf
      note("freigegeben, Trommel laeuft aus");
      break;
    case 't':
      if (speedctl::running()) { fail("erst anhalten"); break; }
      state::startTare();          // nicht blockierend, Fortschritt in tareCount()
      note("Tara laeuft — nicht anfassen");
      break;

    // --- Laeufe. 'm' allein wird abgelehnt: toFloat("") ist 0 und waere
    //     stillschweigend ein Leerlauf. Siehe docs/05-pruefablauf.md B4/B6.
    case 'm':
      if (cmd.length() < 2) { fail("m0 = Leerlauf, m1 = Messlauf"); break; }
      if (cmd[1] == '0')      state::startBaseline();
      else if (cmd[1] == '1') state::startMeasure();
      else                  { fail("m0 = Leerlauf, m1 = Messlauf"); break; }
      // Die Zustandsmaschine meldet Ablehnungen ueber logger::note auf die
      // Konsole; die Oberflaeche soll denselben Grund sehen.
      if (state::phase() == state::SPINUP)
        snprintf(g_note, sizeof(g_note), "%s: %u Geschwindigkeiten",
                 cmd[1] == '0' ? "Leerlauf" : "Messlauf",
                 static_cast<unsigned>(settings::plateauCount()));
      else
        fail(logger::lastNote());
      break;
    // --- Messgeschwindigkeiten: h25 fuegt 25 km/h ein, h-25 nimmt sie heraus.
    //     Die Liste bleibt sortiert, settings.cpp kuemmert sich darum.
    case 'h':
      if (cmd.length() < 2) { fail("h<kmh> dazu, h-<kmh> entfernen"); break; }
      if (arg < 0.0f) {
        if (settings::removePlateau(-arg)) note("Geschwindigkeit entfernt");
        else                               fail("nicht in der Liste");
      } else if (settings::addPlateau(arg)) {
        snprintf(g_note, sizeof(g_note), "%.1f km/h aufgenommen", arg); g_noteBad = false;
      } else {
        snprintf(g_note, sizeof(g_note),
                 "abgelehnt: %.0f..%.0f km/h, hoechstens %u, keine Doppelten",
                 V_MIN_KMH, V_MAX_KMH, static_cast<unsigned>(PLATEAU_MAX));
        g_noteBad = true;
      }
      break;
    case 'q':
      state::clearFault();
      break;
    case 'k': calibrate(arg); break;
    case 'd':
      if (settings::setDrumCircumferenceM(arg)) note("Trommelumfang uebernommen");
      else                                       fail("Trommelumfang unplausibel");
      break;
    case 'a':
      if (settings::setTorqueArmM(arg)) note("Zellenarm uebernommen");
      else                               fail("Zellenarm unplausibel");
      break;
    case 'b':
      if (settings::setCalibArmM(arg)) note("Kalibrierarm uebernommen");
      else                               fail("Kalibrierarm unplausibel");
      break;
    case 'l':
      if (arg > 0.0f) {
        state::setLoadN(arg);
        snprintf(g_note, sizeof(g_note), "Auflast %.1f N", arg);
      } else {
        fail("Auflast muss groesser null sein");
      }
      break;
    case 'p':
      if (arg > 0.0f) {
        tirepressure::setManual(arg);
        snprintf(g_note, sizeof(g_note), "Reifendruck %.2f bar", arg);
      } else {
        note("Druck muss groesser null sein");
      }
      break;
    case 'z':
      if (speedctl::running()) { note("erst anhalten"); break; }
      settings::reset();
      note("Einstellungen auf die Vorgaben aus config.h zurueckgesetzt");
      break;
    case 'e':
      if (speedctl::running()) { note("erst anhalten"); break; }
      // e1 / e-1 setzen das Vorzeichen direkt -- wenn man die Richtung gesehen hat
      // und nicht erst von Hand drehen will. e allein lernt an.
      if (cmd.length() > 1 && (arg == 1.0f || arg == -1.0f)) {
        settings::setDirectionSign(static_cast<int8_t>(arg));
        snprintf(g_note, sizeof(g_note), "Drehrichtung %+d gesetzt", static_cast<int>(arg));
        break;
      }
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
    case 'g': speed::resetCounters(); note("Geberzaehler genullt"); break;
    case 'j':
      g_liveUntil = millis() + static_cast<uint32_t>((arg > 0.0f ? arg : 5.0f) * 1000.0f);
      g_liveLast = 0;
      break;
    case '?': printHelp(); note(""); break;
    default:  fail("unbekannter Befehl"); break;
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

  // Verdrahtungstest der Geber: je Kanal [Impulse, ms seit dem letzten, low].
  // ms ist null, solange noch kein Impuls kam.
  char sGeber[128];   // 4 x maximal 30 Zeichen
  size_t gp = 0;
  for (uint8_t i = 0; i < speed::CH_N; i++) {
    const speed::ChannelState c = speed::channel(static_cast<speed::Channel>(i));
    char sMs[12];
    if (c.count) snprintf(sMs, sizeof(sMs), "%lu", static_cast<unsigned long>(c.sinceMs));
    else         snprintf(sMs, sizeof(sMs), "null");
    gp += snprintf(sGeber + gp, sizeof(sGeber) - gp, "%s[%lu,%s,%s]",
                   i ? "," : "", static_cast<unsigned long>(c.count), sMs,
                   c.low ? "true" : "false");
  }

  // Laufrad: Drehzahlen und integrierter Umfang (wheel.h). Intern m und 1/s;
  // 1/min und mm entstehen erst hier, an der Ausgabegrenze.
  const float nDrum  = revsrc::revPerSec();
  const float nWheel = speed::wheelRevPerSec();
  char sURad[16];
  num(sURad, sizeof(sURad), wheel::circumferenceM() * 1000.0f, 0);

  // Messgeschwindigkeiten als JSON-Liste, sortiert wie in den Einstellungen.
  char sPlat[PLATEAU_MAX * 7 + 2];
  size_t pp = 0;
  for (uint8_t i = 0; i < settings::plateauCount(); i++)
    pp += snprintf(sPlat + pp, sizeof(sPlat) - pp, "%s%.1f", i ? "," : "",
                   settings::plateauKmh(i));
  if (pp == 0) sPlat[0] = '\0';
  char sLoad[16];
  num(sLoad, sizeof(sLoad), state::loadN(), 1);

  // Live: Verlustleistung am Reifen und Crr -- ROH, also samt Trommelverlusten;
  // die Auswertung zieht den Leerlauf ab (docs/09-messtheorie.md). Ohne
  // Kalibrierung null, ohne Auflast kein Crr.
  const float pW  = isnan(mNm) ? NAN : mNm * revsrc::omega();
  const float crr = (isnan(roll) || isnan(state::loadN()) || state::loadN() <= 0.0f)
                    ? NAN : roll / state::loadN();
  char sPW[16], sCrr[16];
  num(sPW,  sizeof(sPW),  pW,  1);
  num(sCrr, sizeof(sCrr), crr, 4);

  char sN[16], sPct[16], sKg[16], sM[16], sRoll[16];
  num(sN,   sizeof(sN),   fN,   2);
  num(sPct, sizeof(sPct), pct,  1);
  num(sKg,  sizeof(sKg),  kg,   3);
  num(sM,   sizeof(sM),   mNm,  3);
  num(sRoll,sizeof(sRoll),roll, 3);

  snprintf(buf, len,
    "{\"build\":\"" __DATE__ " " __TIME__ "\","
    "\"link\":%s,\"fault\":%u,\"faultTxt\":\"%s\",\"status\":%u,\"age\":%lu,"
    "\"fw\":\"%s\",\"note\":\"%s\",\"noteBad\":%s,\"bremst\":%s,"
    "\"kmh\":%.1f,\"soll\":%.0f,\"rampe\":%.1f,\"duty\":%.3f,\"faehrt\":%s,"
    "\"erpm\":%ld,\"rpm\":%.1f,"
    "\"vin\":%.1f,\"imot\":%.2f,\"iin\":%.2f,\"watt\":%.0f,"
    "\"tfet\":%.1f,\"tmot\":%s,\"tmotOffen\":%s,"
    "\"counts\":%ld,\"raw\":%ld,\"tare\":%ld,\"ripple\":%ld,\"n\":%s,\"pct\":%s,\"kg\":%s,"
    "\"nm\":%s,\"roll\":%s,\"pw\":%s,\"crr\":%s,\"err\":\"%s\","
    "\"kal\":%s,\"saturiert\":%s,\"zelle\":%s,"
    "\"dir\":%d,\"lernt\":%s,\"brk\":%s,\"brkDuty\":%.4f,\"brkAmps\":%.1f,"
    "\"up\":%lu,\"geber\":[%s],"
    "\"nTrommel\":%.1f,\"nRad\":%.1f,\"uRad\":%s,\"uRadN\":%lu,\"uRadMin\":%lu,"
    "\"plateaus\":[%s],\"phase\":\"%s\",\"run\":%u,\"plateau\":%u,"
    "\"sfault\":\"%s\",\"load\":%s,\"motorOffen\":%s,\"notaus\":%s,"
    "\"drumU\":%.4f,\"arm\":%.4f,\"calArm\":%.4f,\"npc\":%.8f,"
    "\"vmax\":%.0f,\"vmin\":%.0f,\"step\":%.0f,\"fsN\":%.1f}",
    ok ? "true" : "false",
    static_cast<unsigned>(v.fault), vesc::faultText(v.fault),
    static_cast<unsigned>(v.status), static_cast<unsigned long>(v.ageMs),
    vesc::fwKnown() ? String(String(vesc::fwMajor()) + "." + String(vesc::fwMinor())).c_str() : "",
    g_note, g_noteBad ? "true" : "false", vesc::braking() ? "true" : "false",
    speedctl::kmh(), speedctl::targetKmh(), speedctl::rampKmh(),
    speedctl::duty(), speedctl::running() ? "true" : "false",
    static_cast<long>(ok ? v.erpm : 0),
    ok ? fabsf(static_cast<float>(v.erpm)) / MOTOR_POLE_PAIRS : 0.0f,
    ok ? v.vIn : 0.0f, ok ? v.currentMotor : 0.0f, ok ? v.currentIn : 0.0f,
    ok ? v.vIn * v.currentIn : 0.0f,
    ok ? v.tempFet : 0.0f, sTmot, tmotOffen ? "true" : "false",
    counts, mean, torque::tareOffset(), ripple, sN, sPct, sKg, sM, sRoll, sPW, sCrr, currentError(),
    settings::calibrated() ? "true" : "false",
    torque::isSaturated() ? "true" : "false",
    torque::isResponding() ? "true" : "false",
    static_cast<int>(settings::directionSign()),
    speedctl::learning() ? "true" : "false",
    speedctl::breakingAway() ? "true" : "false",
    speedctl::breakawayDuty(), speedctl::breakawayAmps(),
    static_cast<unsigned long>(millis()), sGeber,
    nDrum * 60.0f, nWheel * 60.0f, sURad,
    static_cast<unsigned long>(wheel::revsUsed()),
    static_cast<unsigned long>(WHEEL_CIRC_WINDOW_REVS),
    sPlat, state::name(), static_cast<unsigned>(state::runMode()),
    static_cast<unsigned>(state::plateauIndex()), state::faultReason(), sLoad,
    speedctl::motorOpen() ? "true" : "false", speedctl::estopInput() ? "true" : "false",
    settings::drumCircumferenceM(), settings::torqueArmM(),
    settings::calibArmM(), settings::loadcellNPerCount(),
    V_MAX_KMH, V_MIN_KMH, SPEED_STEP_KMH, LOADCELL_CAPACITY_N);
}

void pollSerial() {
  // 1100 Zeichen: eine Reglerkonfiguration als Hex (>0d + 493 Byte) muss hineinpassen.
  static char line[1100];
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
  Serial.println("# Fahrrad-Reifenpruefstand — Firmware");

  settings::begin();
  statusled::begin();
  torque::begin();
  revsrc::begin();
  wheel::begin();
  speedctl::begin();
  logger::begin();
  tirepressure::begin();
  state::begin();
  webui::begin(handleCommand, buildStateJson,
               WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
  vesc::begin();

  if (!settings::calibrated())
    Serial.println("# Waegezelle NICHT kalibriert — es gibt nur Rohwerte, keine Kraefte.");

  printHelp();
}

void loop() {
  {
    const uint32_t now = millis();
    if (g_loopPrevMs && now - g_loopPrevMs > g_loopMaxMs) g_loopMaxMs = now - g_loopPrevMs;
    g_loopPrevMs = now;
    if (vesc::values().ageMs > g_vescAgeMax) g_vescAgeMax = vesc::values().ageMs;
  }
  // Interne Meldungen (Zustandsmaschine, Regelung) auch in die Oberflaeche.
  {
    static char seen[96] = "";
    if (strcmp(logger::lastNote(), seen) != 0) {
      strncpy(seen, logger::lastNote(), sizeof(seen) - 1);
      if (looksBad(seen)) fail(seen); else note(seen);
    }
  }
  statusled::service();
  webui::service();
  vesc::service();

  // Jeder neue Rohwert der Zelle wird im Messzustand in den umdrehungssynchronen
  // Akkumulator einsortiert. Das muss HIER stehen, wo update() gerufen wird:
  // nur dieser Aufruf weiß, dass gerade ein neuer Wert angekommen ist.
  if (torque::update() && state::phase() == state::POINT) {
    torque::accumulate(revsrc::markIndexInRev());
  }

  revsrc::service();
  wheel::service();
  speedctl::service();
  state::service();
  pollSerial();
  liveTelemetry();

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
