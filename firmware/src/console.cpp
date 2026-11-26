#include "console.h"


#include "config.h"
#include "calc/linfit.h"
#include "calc/loss.h"
#include "calc/json.h"
#include "settings.h"
#include "webui.h"
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

namespace console {
namespace {

char  g_note[96] = "";          // last feedback to the UI

// Messages: note() is a notice (console and UI, unobtrusive there),
// fail() an error -- ONLY those are shown by the UI in the message line. Since
// 2026-11-23: "Target 15 km/h" stood there permanently although the Target tile
// had long been showing it; the line is reserved for what the operator must know.
bool g_noteBad = false;
uint32_t g_noteMs = 0;


}  // namespace

void note(const char* s) { strncpy(g_note, s, sizeof(g_note) - 1); g_note[sizeof(g_note)-1] = '\0'; g_noteBad = false; g_noteMs = millis(); }
void fail(const char* s) { note(s); g_noteBad = true; }

// THE error state for the UI: a text or nothing. The page only checks whether
// something is there and shows it in red. Order = importance.
// Command errors (fail) disappear by themselves after five seconds.
const char* currentError() {
  static char buf[96];
  const vesc::Values& v = vesc::values();
  if (!vesc::connected())            return "No controller";
  // Show a controller fault for 30 s -- the controller itself clears it after 500 ms.
  if (vesc::lastFaultAgeMs() < FAULT_SHOW_MS) {
    snprintf(buf, sizeof(buf), "Controller: %s", vesc::faultText(vesc::lastFault()));
    return buf;
  }
  if (speedctl::estopInput())        return "E-stop";
  if (speedctl::motorOpen())         return "Motor disconnected";
  if (state::phase() == state::FAULT) {
    const char* r = state::faultReason();
    return strncmp(r, "FAULT: ", 7) == 0 ? r + 7 : r;
  }
  if (torque::isSaturated())         return "Load cell saturated";
  if (!torque::isResponding())       return "HX711 not responding";
  if (v.tempMotor < MOTOR_TEMP_OPEN_C) return "Motor sensor open";
  if (speedctl::stalledUnderDuty())  return "Drum not turning";
  if (g_noteBad && g_note[0] && millis() - g_noteMs < NOTE_SHOW_MS) return g_note;
  return "";
}

// Live telemetry to the console (command j<s>): one line every 200 ms, for s
// seconds. For debugging over USB -- tools/konsole.py records it.
uint32_t g_liveUntil = 0, g_liveLast = 0;

// Longest loop round since the last query. The web server can block the main
// loop, and from 300 ms the controller's dead-man timeout trips, from 1 s
// speedctl releases -- that is why it is measured, not assumed.
uint32_t g_loopMaxMs = 0, g_vescAgeMax = 0;
uint32_t    g_sectMaxMs = 0;          // longest loop section of the reporting period
const char* g_sectMaxName = "-";

void telemetry() {
  const uint32_t now = millis();
  if (now >= g_liveUntil || now - g_liveLast < CONSOLE_LIVE_PERIOD_MS) return;
  g_liveLast = now;
  const vesc::Values& v = vesc::values();
  Serial.print("# J ");      Serial.print(now);
  Serial.print(" ist=");     Serial.print(revsrc::kmh(), 1);
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
  const live::Result lv = live::result();
  Serial.print(" konst=");   Serial.print(live::steady() ? 1 : 0);
  Serial.print(" pw=");      if (isnan(lv.pW)) Serial.print("-"); else Serial.print(lv.pW, 1);
  Serial.print(" crr=");     if (isnan(lv.crr)) Serial.print("-"); else Serial.print(lv.crr, 4);
  Serial.print(" loopmax="); Serial.print(g_loopMaxMs);
  Serial.print(" sect=");    Serial.print(g_sectMaxName); Serial.print(':'); Serial.print(g_sectMaxMs);
  Serial.print(" vage=");    Serial.println(g_vescAgeMax);
  g_loopMaxMs = 0; g_vescAgeMax = 0; g_sectMaxMs = 0; g_sectMaxName = "-";
}

void printHelp() {
  Serial.println("# Commands:");
  Serial.println("#   R / Rk / Rs / Rn / Rx / Rm<k>=<v>  guided run (run.h)");
  Serial.println("#   v<kmh>  target speed             s  STOP (relay brake)");
  Serial.println("#   c       let it coast down        q  acknowledge fault");
  Serial.println("#   m0      baseline run (wheel lifted)    m1  measurement run");
  Serial.println("#   h<kmh>  add measurement speed          h-<kmh>  remove");
  Serial.println("#   t       tare load cell          k<kg>  calibrate (console only)   K<kg>  check weight");
  Serial.println("#   l<N>    wheel load              p<bar> tire pressure");
  Serial.println("#   d<m>    drum circumference      a<m>   cell arm");
  Serial.println("#   b<m>    calibration arm         z      reset settings");
  Serial.println("#   e       learn direction (turn the drum by hand); e1 / e-1 directly");
  Serial.println("#   x       measure breakaway torque (duty cycle rises slowly)");
  Serial.println("#   ed      detect drive direction (slow start, load cell must count positive)");
  Serial.println("#   i       status: controller, load cell, sensors, UI link");
  Serial.println("#   g       reset sensor counters (wiring test)");
  Serial.println("#   j<s>    live telemetry, every 200 ms, s seconds (default 5)");
  Serial.println("#   L1 / L0 raw data on / off (SD only; automatic in a measurement run)");
  Serial.println("#   Fs / Ff format SD / flash -- ONLY if unreadable, erases everything on it");
  Serial.println("#   n<A0> <B0>  set baseline model (m0 fits it itself)");
  Serial.println("#   w<A1> <B1>  set wheel loss model   y<kg>  wheel mass");
  Serial.println("#   Y<kgm2>  J_wheel MEASURED (bifilar pendulum), Y0 clears -> estimate 0.85 m r^2");
  Serial.println("#   Ae<kg> Aw<kg> Aa<m>  lever balance: empty weight, sliding weight, wheel arm   Ap<m>  position -> wheel load");
  Serial.println("#   u       assistant spin-down + baseline (drum up, wheel lifted); uq acknowledges");
  Serial.println("#   :<text> terminal command to the controller (faults, hall_analyze 5, ...)");
  Serial.println("#   ><hex>  raw packet to the controller, reply as hex (e.g. >0e = mcconf)");
  Serial.print("# Limit: "); Serial.print(V_MAX_KMH, 0);
  Serial.println(" km/h (docs/07-safety.md §5)");
}

// Status to the console. The UI shows all of this too, but when connecting at
// the stand you stand in front of it with a screwdriver, not with the phone --
// and when one of the two connections is missing, the browser is exactly what
// you do not have.
void printStatus() {
  const vesc::Values& v = vesc::values();
  long mean = 0, ripple = 0;
  torque::displayStats(mean, ripple);

  Serial.println("# ---- Status ----");

  for (uint8_t m = 0; m < storage::MEDIA_N; m++) {
    const storage::Info si = storage::info(static_cast<storage::Media>(m));
    Serial.print(m == storage::SD_CARD ? "# SD       " : "# Flash    ");
    Serial.print(storage::stateName(si.state));
    if (si.pct >= 0) {
      Serial.print(", "); Serial.print(si.pct); Serial.print(" % of ");
      Serial.print(static_cast<uint32_t>(si.total / (1024 * 1024))); Serial.print(" MB");
    }
    Serial.println();
  }
  Serial.print("# Raw data ");
  if (rawlog::active()) {
    Serial.print(storage::name(rawlog::media())); Serial.print(rawlog::fileName());
    Serial.print(", "); Serial.print(rawlog::bytesWritten()); Serial.print(" B written, ");
    Serial.print(rawlog::bytesPending()); Serial.print(" B in ring, ");
    Serial.print(rawlog::bytesDropped()); Serial.println(" B dropped");
  } else Serial.println("off");

  Serial.print("# Controller ");
  if (vesc::connected()) {
    Serial.print("OK");
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
    if (v.tempMotor < MOTOR_TEMP_OPEN_C) Serial.print("SENSOR OPEN");
    else { Serial.print(v.tempMotor, 1); Serial.print(" C"); }
    Serial.print(" | Fault "); Serial.print(vesc::faultText(v.fault));
    // The timeout bit is the normal case at STANDSTILL and not a malfunction: the
    // setpoint is only repeated while driving (vesc.h). When not driving, the
    // controller's own timeout expires -- and that is how it should be, because
    // a mere heartbeat would have kept the controller armed without a setpoint
    // behind it.
    if (v.status & 0x01)
      Serial.print(speedctl::running() ? " | TIMEOUT WHILE RUNNING!"
                                       : " | Timeout (expected at standstill)");
    if (v.status & 0x02) Serial.print(" | KILL-SWITCH");
    Serial.println();
    Serial.print("#          Drives only above the controller's l_battery_cut_end;"
                 " the limits are in hardware/vesc-config-fw7.01/mcconf.xml");
    Serial.println();
  } else {
    Serial.print("MISSING — no reply on TX GPIO ");
    Serial.print(PIN_DRIVE_TX);
    Serial.print(" / RX GPIO ");  Serial.print(PIN_DRIVE_RX);
    Serial.print(" at ");         Serial.print(VESC_BAUD);
    Serial.println(" Bd");
    if (v.seen) {
      Serial.print("#          has replied before, last packet ");
      Serial.print(v.ageMs); Serial.println(" ms ago");
    } else {
      Serial.println("#          has never replied. Check: TX/RX swapped?"
                     " app_to_use = UART? COMM ground?");
    }
  }

  Serial.print("# Drive    ");
  if (speedctl::breakingAway()) {
    Serial.print("breakaway measurement | duty ");
    Serial.print(speedctl::duty(), 4);
    Serial.print(" | Imot ");  Serial.print(v.currentMotor, 1);
    Serial.print(" A | actual "); Serial.print(revsrc::kmh(), 1);
    Serial.println(" km/h");
  } else if (speedctl::learning()) {
    Serial.println("learning direction — turn the drum in measuring direction");
  } else {
    Serial.print(speedctl::running() ? "running" : "stopped");
    Serial.print(" | relay "); Serial.print(brake::stateName());
    Serial.print(" | target ");  Serial.print(speedctl::targetKmh(), 0);
    Serial.print(" | ramp "); Serial.print(speedctl::rampKmh(), 1);
    Serial.print(" | actual ");   Serial.print(revsrc::kmh(), 1);
    Serial.print(" km/h | duty "); Serial.print(speedctl::duty(), 3);
    Serial.print(" | direction "); Serial.print(settings::directionSign());
    Serial.println();
  }

  // Setpoint is applied, drum does not turn: the controller has lost
  // commutation. With the uneven Hall table this happens below
  // foc_hall_interp_erpm, where the raw table applies (docs/04-hardware.md 11.2).
  if (speedctl::stalledUnderDuty())
    Serial.println("#          WARNING duty applied, drum not turning."
                   " Commutation lost — re-detect the Hall table.");
  if (vesc::faultCount()) {
    Serial.print("#          Controller faults since boot: "); Serial.print(vesc::faultCount());
    Serial.print(", last "); Serial.print(vesc::faultText(vesc::lastFault()));
    Serial.print(" "); Serial.print(vesc::lastFaultAgeMs() / 1000); Serial.println(" s ago");
  }
  if (speedctl::estopInput())
    Serial.println("#          EMERGENCY STOP triggered (auxiliary contact).");
  if (speedctl::motorOpen())
    Serial.println("#          NO MOTOR ON THE CONTROLLER: duty was applied, but no motor current."
                   " Emergency stop triggered or motor disconnected. Drive released.");

  // The open temperature sensor is also a suspicion on the Hall sensors: TMP and
  // the three Hall wires are in the SAME 6-pin connector (docs/04-hardware.md
  // 11.5). On 2026-11-18 exactly that was the case -- connector not plugged in,
  // -82 C at the sensor, and the motor only twitched.
  if (vesc::connected() && v.tempMotor < MOTOR_TEMP_OPEN_C)
    Serial.println("#          WARNING sensor connector: TMP open. The Hall wires"
                   " are in the same connector, so they are suspect too.");

  Serial.print("# Cell     ");
  if (torque::isResponding()) {
    Serial.print("OK | "); Serial.print(mean);
    Serial.print(" counts | peak-to-peak "); Serial.print(ripple);
    Serial.print(" | zero ");  Serial.print(torque::tareOffset());
    if (torque::isSaturated()) Serial.print(" | SATURATED");
    if (settings::calibrated()) {
      Serial.print(" | factor "); Serial.print(settings::loadcellNPerCount(), 8);
      Serial.print(" N/count | force "); Serial.print(torque::newtons(), 2); Serial.print(" N");
    } else {
      Serial.print(" | not calibrated");
    }
    Serial.print(" | arms "); Serial.print(settings::torqueArmM() * 1000.0f, 1);
    Serial.print(" / ");       Serial.print(settings::calibArmM() * 1000.0f, 1); Serial.print(" mm");
    Serial.println();
  } else {
    Serial.print("MISSING — HX711 not responding on DOUT GPIO ");
    Serial.print(PIN_HX711_DOUT);
    Serial.print(" / SCK GPIO "); Serial.println(PIN_HX711_SCK);
    Serial.println("#          Check: VCC and VDD supplied? GND common?"
                   " DOUT/SCK swapped?");
  }

  // Wiring test: turn the drum by hand, the counters must follow.
  // A channel without pulses is either not wired or sees no magnet --
  // the level alone cannot tell the two apart, because the pull-up pulls
  // both high.
  Serial.print("# Sensors  ");
  for (uint8_t i = 0; i < speed::CH_N; i++) {
    const speed::ChannelState c = speed::channel(static_cast<speed::Channel>(i));
    if (i) Serial.print(" | ");
    Serial.print(speed::channelName(static_cast<speed::Channel>(i)));
    Serial.print(' '); Serial.print(c.count);
    if (i == speed::CH_MARK && speed::markEchoes()) { Serial.print(" (echoes dropped "); Serial.print(speed::markEchoes()); Serial.print(')'); }
    if (c.count) { Serial.print(" "); Serial.print(c.sinceMs); Serial.print(" ms ago"); }
    else         { Serial.print(" NO PULSES"); }
    Serial.print(c.low ? " low" : " high");
  }
  Serial.println();
  // Evaluation of the drum marks (speed.h RevSource): pattern of the intermediate
  // magnet and the counters that show whether it is detected cleanly or interferes.
  static const char* const SRC[] = {"8 marks counted", "intermediate magnet detected", "index Hall"};
  Serial.printf("# Marks    %s, %u pulses/rev | re-locked %lu | missed %lu | glitches %lu | echoes %lu | revolutions %lu\n",
                SRC[speed::revSource()], static_cast<unsigned>(speed::pulsesPerRev()),
                static_cast<unsigned long>(speed::patternResyncs()), static_cast<unsigned long>(speed::missedPulses()),
                static_cast<unsigned long>(speed::glitchPulses()), static_cast<unsigned long>(speed::markEchoes()),
                static_cast<unsigned long>(speed::revCount()));
  { float r; uint8_t h; uint16_t si; uint32_t se, rs; speed::patternDiag(r, h, si, se, rs);
    Serial.printf("#          pattern: mean %.0f us | hits %u | since intermediate pulse %u | seen %lu | reset %lu\n",
                  r, static_cast<unsigned>(h), static_cast<unsigned>(si), static_cast<unsigned long>(se), static_cast<unsigned long>(rs)); }
  // Last 18 intervals in degrees, 9 of them = 360: shows where the intermediate magnet sits.
  float deg[18];
  const uint8_t nd = speed::recentIntervalsDeg(deg, 18, MARKS_PER_REV + 1);
  if (nd) {
    Serial.print("#          intervals (9 = 360 deg):");
    for (uint8_t k = 0; k < nd; k++) Serial.printf(" %.1f", deg[k]);
    Serial.println();
  }

  // Test wheel circumference: integrated over a window of steady running (wheel.h).
  {
    Serial.print("# Wheel    drum "); Serial.print(revsrc::revPerSec() * 60.0f, 1);
    Serial.print(" 1/min | wheel ");       Serial.print(speed::wheelRevPerSec() * 60.0f, 1);
    Serial.print(" 1/min | circumference ");
    if (wheel::valid()) {
      Serial.print(wheel::circumferenceM() * 1000.0f, 0);
      Serial.print(" mm from "); Serial.print(wheel::revsUsed()); Serial.println(" wheel revolutions");
    } else {
      Serial.print("— [computed "); Serial.print(wheel::circumferenceRawM() * 1000.0f, 0); Serial.print(" mm] (");
      Serial.print(wheel::revsUsed());
      Serial.print(" of "); Serial.print(WHEEL_CIRC_WINDOW_REVS);
      Serial.println(" wheel revolutions)");
    }
  }

  Serial.print("# Models   baseline ");
  if (settings::haveBaseline()) {
    Serial.print("A0="); Serial.print(settings::baselineA0(), 4);
    Serial.print(" B0="); Serial.print(settings::baselineB0(), 7);
  } else Serial.print("MISSING (m0 with wheel lifted)");
  Serial.print(" | wheel ");
  if (settings::haveWheelModel()) {
    Serial.print("A1="); Serial.print(settings::wheelA1(), 5);
    Serial.print(" B1="); Serial.print(settings::wheelB1(), 8);
  } else Serial.print("MISSING (y<kg>, then u)");
  Serial.print(" | wheel mass "); Serial.print(settings::wheelMassKg(), 3);
  Serial.print(" kg | stored circumference "); Serial.print(settings::wheelCircM() * 1000.0f, 0);
  Serial.print(" mm | J_wheel ");
  if (isnan(wheel::jWheelKgm2())) Serial.println("unknown");
  else { Serial.print(wheel::jWheelKgm2(), 4); Serial.println(" kg m^2"); }

  Serial.print("# Measure  ");
  Serial.print(state::name());
  if (state::phase() == state::FAULT) {
    Serial.print(" — "); Serial.print(state::faultReason());
    Serial.print(" (acknowledge: q)");
  }
  if (state::runMode() != state::RUN_NONE) {
    Serial.print(state::runMode() == state::RUN_BASELINE ? " baseline run" : " measurement run");
    Serial.print(" plateau "); Serial.print(state::plateauIndex() + 1);
    Serial.print('/');          Serial.print(settings::plateauCount());
  }
  Serial.print(" | speeds");
  for (uint8_t i = 0; i < settings::plateauCount(); i++) {
    Serial.print(' '); Serial.print(settings::plateauKmh(i), 1);
  }
  Serial.print(" km/h | wheel load ");
  if (isnan(state::loadN())) Serial.println("not set");
  else { Serial.print(state::loadN(), 1); Serial.println(" N"); }

  Serial.print("# Loop     longest round "); Serial.print(g_loopMaxMs);
  Serial.print(" ms (longest section "); Serial.print(g_sectMaxName); Serial.print(' '); Serial.print(g_sectMaxMs);
  Serial.print(" ms), oldest controller values "); Serial.print(g_vescAgeMax);
  Serial.println(" ms since last query");
  g_loopMaxMs = 0; g_vescAgeMax = 0; g_sectMaxMs = 0; g_sectMaxName = "-";

  Serial.print("# UI       ");
  Serial.print(webui::linkAlive() ? "browser connected" : "no browser");
  Serial.print(" | WLAN \""); Serial.print(WEBUI_SSID);
  Serial.print("\" | http://"); Serial.print(WEBUI_HOSTNAME);
  Serial.print(".local/ or http://");
  for (uint8_t i = 0; i < 4; ++i) {
    Serial.print(WEBUI_AP_IP[i]);
    if (i < 3) Serial.print('.');
  }
  Serial.println("/");

  if (speedctl::breakawayDuty() > 0.0f) {
    Serial.print("# Breakaway at duty ");
    Serial.print(speedctl::breakawayDuty(), 4);
    Serial.print(", Imot ");  Serial.print(speedctl::breakawayAmps(), 1);
    Serial.print(" A, torque ");
    Serial.print(speedctl::breakawayAmps() * MOTOR_KT_NM_PER_A, 2);
    Serial.print(" Nm | running friction for comparison ");
    Serial.print(DRUM_LOSS_CONST_NM, 3); Serial.println(" Nm");
  }

  Serial.print("# Last message: ");
  Serial.println(g_note[0] ? g_note : "—");
}

// Calibration: a known weight hangs on the calibration arm and produces the
// torque m·g·L_cal there. The cell sits on the shorter arm L and therefore sees
//     F = m · g · L_cal / L
// The factor is this force divided by the raw-value increase since taring.
void calibrate(float kg) {
  if (kg <= 0.0f) { fail("Weight must be > 0"); return; }
  if (!torque::isResponding()) { fail("HX711 not responding"); return; }

  long mean, ripple;
  torque::displayStats(mean, ripple);
  const long delta = mean - torque::tareOffset();
  if (delta == 0) { fail("No deflection"); return; }

  const float f = kg * 9.80665f * settings::calibArmM() / settings::torqueArmM();
  const float nPerCount = f / static_cast<float>(delta > 0 ? delta : -delta);

  if (!settings::setLoadcellNPerCount(nPerCount)) {
    fail("Factor implausible");
    return;
  }
  snprintf(g_note, sizeof(g_note),
           "calibrated: %.1f N over %ld counts  ->  %.8f N/count", f, delta, nPerCount);
  Serial.print("# "); Serial.println(g_note);
  Serial.println("# Enter in config.h and docs/03-parameters.md:");
  Serial.print("#   constexpr float LOADCELL_N_PER_COUNT = ");
  Serial.print(nPerCount, 8); Serial.println("f;");
}

// The same command vocabulary for console and web UI (docs/10-firmware.md G5).
void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;
  const float arg = cmd.substring(1).toFloat();

  // Pass-through to the controller, only sensible from the console (vesc.h).
  if (cmd[0] == ':') { vesc::terminal(cmd.c_str() + 1); return; }
  // Guided run (run.h). Whoever starts it is subject to the dead-man check.
  if (cmd[0] == 'R') {
    if (cmd.length() == 1) webui::setWebOperator(webui::inWebCommand());
    if (!run::command(cmd.c_str() + 1)) fail(logger::lastNote()[0] ? logger::lastNote() : "Run refused");
    return;
  }
  if (cmd[0] == '>') {
    uint8_t p[vesc::PAYLOAD_MAX];
    size_t n = 0;
    for (unsigned i = 1; i + 1 < cmd.length() && n < sizeof(p); i += 2) {
      char h[3] = {cmd[i], cmd[i + 1], 0};
      p[n++] = static_cast<uint8_t>(strtoul(h, nullptr, 16));
    }
    if (n) vesc::raw(p, n); else fail("Hex payload missing");
    return;
  }

  switch (cmd[0]) {
    case 'v':
      webui::setWebOperator(webui::inWebCommand());   // whoever starts is subject to the dead-man check
      if (speedctl::setTargetKmh(arg)) note("");      // the tile shows the target
      else                             fail(speedctl::lastRefusal());
      break;
    case 's':
      state::stopRun();            // brakes (relay) and ends a running run
      note("STOP");
      break;
    case 'c':
      state::coastRun();           // releases and ends a running run
      note("Coasting");
      break;
    case 't':
      if (speedctl::running()) { fail("Stop first"); break; }
      state::startTare();          // non-blocking, progress in tareCount()
      note("Taring, hands off");
      break;

    // --- Runs. 'm' alone is refused: toFloat("") is 0 and would silently
    //     be a baseline run. See docs/05-test-procedure.md B4/B6.
    case 'm':
      if (cmd.length() < 2) { fail("m0 = baseline, m1 = measure"); break; }
      webui::setWebOperator(webui::inWebCommand());
      if (cmd[1] == '0')      state::startBaseline();
      else if (cmd[1] == '1') state::startMeasure();
      else                  { fail("m0 = baseline, m1 = measure"); break; }
      // The state machine reports refusals via logger::note to the
      // console; the UI should see the same reason.
      if (state::phase() == state::SPINUP)
        snprintf(g_note, sizeof(g_note), "%s: %u speeds",
                 cmd[1] == '0' ? "Baseline run" : "Measurement run",
                 static_cast<unsigned>(settings::plateauCount()));
      else
        fail(logger::lastNote());
      break;
    // --- Measurement speeds: h25 adds 25 km/h, h-25 removes it.
    //     The list stays sorted, settings.cpp takes care of that.
    case 'h':
      if (cmd.length() < 2) { fail("h<kmh> add, h-<kmh> remove"); break; }
      if (arg < 0.0f) {
        if (settings::removePlateau(-arg)) note("Speed removed");
        else                               fail("Not in list");
      } else if (settings::addPlateau(arg)) {
        snprintf(g_note, sizeof(g_note), "%.1f km/h added", arg); g_noteBad = false;
      } else {
        snprintf(g_note, sizeof(g_note),
                 "Refused: %.0f..%.0f km/h, max %u, no duplicates",
                 V_MIN_KMH, V_MAX_KMH, static_cast<unsigned>(PLATEAU_MAX));
        g_noteBad = true;
      }
      break;
    case 'q':
      state::clearFault();
      break;
    case 'k': calibrate(arg); break;
    case 'K':   // check weight (setup); checked and applied during the measurement run
      if (settings::setCtrlMassKg(arg)) note("Check weight set");
      else                               fail("Check weight implausible");
      break;
    case 'd':
      if (settings::setDrumCircumferenceM(arg)) note("Drum circumference set");
      else                                       fail("Drum circumference implausible");
      break;
    case 'a':
      if (settings::setTorqueArmM(arg)) note("Cell arm set");
      else                               fail("Cell arm implausible");
      break;
    case 'b':
      if (settings::setCalibArmM(arg)) note("Calibration arm set");
      else                               fail("Calibration arm implausible");
      break;
    case 'l':
      if (arg > 0.0f) {
        state::setLoadN(arg);
        snprintf(g_note, sizeof(g_note), "Wheel load %.1f N", arg);
      } else {
        fail("Load must be > 0");
      }
      break;
    case 'p':
      if (arg > 0.0f) {
        tirepressure::setManual(arg);
        snprintf(g_note, sizeof(g_note), "Tire pressure %.2f bar", arg);
      } else {
        note("Pressure must be > 0");
      }
      break;
    case 'z':
      if (speedctl::running()) { note("Stop first"); break; }
      settings::reset();
      note("Settings reset to config.h defaults");
      break;
    case 'e':
      if (speedctl::running()) { note("Stop first"); break; }
      // e1 / e-1 set the sign directly -- when the direction has been seen
      // and you do not want to turn by hand first. e alone learns it.
      if (cmd.length() > 1 && (arg == 1.0f || arg == -1.0f)) {
        settings::setDirectionSign(static_cast<int8_t>(arg));
        snprintf(g_note, sizeof(g_note), "Direction %+d set", static_cast<int>(arg));
        break;
      }
      if (cmd == "ed") {   // detect: slow start, cell must count positive (speedctl.h)
        const char* why = "";
        if (speedctl::detectDirection(&why)) note("Detecting direction, keep clear"); else note(why);
        break;
      }
      speedctl::learn();
      note("Turn drum by hand");
      Serial.println("# Drive released. Turn the drum in MEASURING DIRECTION,"
                     " at least 2 km/h.");
      break;
    case 'x':
      if (speedctl::running()) { note("Stop first"); break; }
      speedctl::breakaway();
      note("Slow test, keep clear");
      Serial.println("# Breakaway measurement: duty cycle rises at 0.01/s,"
                     " stops at 0.15 or as soon as the drum moves.");
      break;
    // --- Loss models and test wheel (docs/10-firmware.md §8)
    case 'n': case 'w': {
      // n<A0> <B0> or w<A1> <B1>: two numbers
      const int sp = cmd.indexOf(' ');
      if (sp < 2) { fail(cmd[0] == 'n' ? "n<A0> <B0>" : "w<A1> <B1>"); break; }
      const float a = cmd.substring(1, sp).toFloat(), b = cmd.substring(sp + 1).toFloat();
      const bool ok = cmd[0] == 'n' ? settings::setBaseline(a, b) : settings::setWheelModel(a, b);
      if (ok) note("Model set"); else fail("Model implausible");
      break;
    }
    case 'y':
      if (settings::setWheelMassKg(arg)) note("Wheel mass set");
      else                                fail("Wheel mass implausible");
      break;
    case 'Y':
      if (settings::setWheelJKgm2(arg)) note(arg == 0.0f ? "J_wheel cleared, estimate used" : "J_wheel set");
      else                               fail("J_wheel implausible (0.01 to 2 kg m^2)");
      break;
    case 'A': {   // lever balance: Ae<kg> empty weight, Aw<kg> sliding weight, Aa<m> wheel arm, Ap<m> position -> wheel load
      const float v = cmd.length() > 2 ? cmd.substring(2).toFloat() : 0.0f;
      bool ok = false;
      switch (cmd.length() > 1 ? cmd[1] : '?') {
        case 'e': ok = settings::setLoadEmptyKg(v);  break;
        case 'w': ok = settings::setLoadWeightKg(v); break;
        case 'a': ok = settings::setLoadArmM(v);     break;
        case 'p': { const float n = settings::loadFromPosN(v);
          if (!isnan(n)) { state::setLoadN(n); snprintf(g_note, sizeof(g_note), "Wheel load at %.0f mm: %.1f N (%.1f kg)", v * 1000.0f, n, n / 9.80665f); return; }
          break; }
        default: break;
      }
      if (ok) note("Lever balance set"); else fail("Lever balance value implausible");
      break; }
    case 'u':
      if (cmd.length() > 1 && cmd[1] == 'q') { state::wizardAck(); break; }   // uq: acknowledge step 4
      webui::setWebOperator(webui::inWebCommand());
      state::startWizard();
      if (state::phase() != state::WIZ_SPINUP) fail(logger::lastNote());
      break;
    case 'i': printStatus(); break;
    case 'L':   // raw data (rawlog.h): L1 on, L0 off
      if (arg > 0.0f) { if (rawlog::start()) note("Raw data on"); else fail(logger::lastNote()); }
      else rawlog::stop();
      break;
    case 'F': {   // format, only an UNREADABLE medium (storage.h): Fs SD, Ff flash
      const char m = cmd.length() > 1 ? cmd[1] : '?';
      if (m != 's' && m != 'f') { fail("Fs or Ff"); break; }
      const char* why = "";
      if (storage::requestFormat(m == 's' ? storage::SD_CARD : storage::FLASH, &why)) note("Formatting");
      else fail(why);
      break;
    }
    case 'g': speed::resetCounters(); note("Sensor counters reset"); break;
    case 'j':
      g_liveUntil = millis() + static_cast<uint32_t>((arg > 0.0f ? arg : CONSOLE_LIVE_DEFAULT_S) * 1000.0f);
      g_liveLast = 0;
      break;
    case '?': printHelp(); note(""); break;
    default:  fail("Unknown command"); break;
  }
}

void pollSerial() {
  // 1100 characters: a controller configuration as hex (>0d + 493 bytes) must fit.
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

// From loop(): duration of a round and age of the controller values; longest section.
void loopRound(uint32_t dtMs, uint32_t vescAgeMs) {
  if (dtMs > g_loopMaxMs) g_loopMaxMs = dtMs;
  if (vescAgeMs > g_vescAgeMax) g_vescAgeMax = vescAgeMs;
}
void loopSection(const char* name, uint32_t ms) { if (ms > g_sectMaxMs) { g_sectMaxMs = ms; g_sectMaxName = name; } }
const char* lastNote() { return g_note; }
bool lastNoteBad() { return g_noteBad; }

}  // namespace console
