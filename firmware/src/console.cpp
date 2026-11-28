extern "C" {
#include "heatshrink_encoder.h"
}
#include "relaytest.h"
#include "console.h"
#include "calib.h"
#include "autozero.h"


#include "config.h"
#include "profiles.h"
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
#include "calc/crc32.h"

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
// seconds. For debugging over USB -- tools/console.py records it.
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
  Serial.print(" actual=");  Serial.print(revsrc::kmh(), 1);
  Serial.print(" ramp=");    Serial.print(speedctl::rampKmh(), 1);
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
  Serial.print(" steady=");  Serial.print(live::steady() ? 1 : 0);
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
  Serial.println("#   t       tare load cell          k<kg>  calibrate (also web)   K<kg>  check weight");
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
  Serial.println("#   Fs / Ff format SD (also readable) / flash (only if unreadable) -- erases everything on it\n"
  "#   D       list /vmrr on the SD (sessions and their files); D<file> prints a text file below /vmrr");
  Serial.println("#   G?<dir> / G<path> <off> <len>  file transfer, for tools/console.py --get");
  Serial.println("#   Z / Zt / Zx  automatic zero by the motor, wheel lifted: measure / and set tare / stop");
  Serial.println("#   C0 / Cm<kg> / Cz / Ce / Ca / Cx  calibration: zero, load with mass, zero, evaluate, accept, cancel");
  Serial.println("#   X<n>    relay pulse test at standstill: n drop/pull-in cycles, edges and load cell per cycle");
  Serial.println("#   B<text> stand revision (free text into every protocol), B alone clears");
  Serial.println("#   n<A0> <B0>  set baseline model (m0 fits it itself)");
  Serial.println("#   w<A1> <B1>  set wheel loss model   y<kg>  wheel mass");
  Serial.println("#   Y<kgm2>  J_wheel MEASURED (bifilar pendulum), Y0 clears -> estimate 0.85 m r^2");
  Serial.println("#   P<T/U/R/H>| name | ... |  tire/tube/rim/hub entry as a row of /vmrr/wheels.md");
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

  // Measured read rate over the last call of i (HX711_RATE_SPS is what the RATE pin
  // should give; the main loop may read fewer).
  static uint32_t lastN = 0, lastMs = 0;
  const uint32_t nNow = torque::readsTotal(), msNow = millis();
  const float sps = (lastMs && msNow > lastMs) ? (nNow - lastN) * 1000.0f / (msNow - lastMs) : NAN;
  lastN = nNow; lastMs = msNow;
  Serial.print("# Cell     ");
  if (torque::isResponding()) {
    if (!isnan(sps)) { Serial.print(sps, 1); Serial.print(" values/s | "); }
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

  if (!settings::npcPlausible(nPerCount)) { fail("Factor implausible"); return; }
  if (!calib::singlePoint(nPerCount)) return;
  snprintf(g_note, sizeof(g_note),
           "calibrated: %.1f N over %ld counts  ->  %.8f N/count", f, delta, nPerCount);
  Serial.print("# "); Serial.println(g_note);
  Serial.println("# Enter in config.h and docs/03-parameters.md:");
  Serial.print("#   constexpr float LOADCELL_N_PER_COUNT = ");
  Serial.print(nPerCount, 8); Serial.println("f;");
}

// The same command vocabulary for console and web UI (docs/10-firmware.md G5).

// File transfer over USB (tools/console.py --get, 2026-11-26). Text lines that a
// line-based reader can pick out between other output (other tasks print too):
//   G?<dir>                  ->  "=L <size> <name>" per entry ("/" after directories), "=E"
//   G<path> <offset> <len>   ->  "=G <offset> <base64 of up to 384 bytes> <crc32 hex>" lines,
//                                then "=E <next offset> <file size>"
//   Gz<path> <offset> <len>  ->  the same range heatshrink-compressed (window 2^10, lookahead
//                                2^5, lib/heatshrink): "=Z <n> <base64> <crc32 hex>" lines of
//                                the compressed stream, then "=E <next offset> <file size>".
//                                Each request is compressed on its own, so a retry is simple.
// <path> lies below /vmrr on the SD. One request blocks the main loop for its length
// (at most GET_CHUNK_MAX bytes; native USB, the baud rate does not apply), so only at standstill
// and outside a measurement run; the client asks chunk by chunk and repeats a chunk
// whose CRC does not match.
namespace {
// 2026-11-26: 16 kB per request and 384 bytes per line instead of 3 kB / 48 bytes; the
// first version ran at 8 kB/s, mostly per-request and per-line overhead.
constexpr size_t GET_CHUNK_MAX = 16384;
constexpr size_t GET_LINE = 384;
constexpr size_t GET_READ = 8192;   // SD read block; larger blocks were not faster (2026-11-26)
constexpr size_t GET_ZCHUNK_MAX = 32768;   // raw bytes per compressed request (about 9 kB on the line)
const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
void b64(const uint8_t* p, size_t n, char* out) {
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    const uint32_t v = (uint32_t(p[i]) << 16) | (i + 1 < n ? uint32_t(p[i + 1]) << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
    out[o++] = B64[(v >> 18) & 63]; out[o++] = B64[(v >> 12) & 63];
    out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < n ? B64[v & 63] : '=';
  }
  out[o] = '\0';
}
// Compressed variant: raw range -> heatshrink -> lines of up to GET_LINE compressed bytes.
heatshrink_encoder g_hse;
uint8_t g_zLine[GET_LINE];
size_t g_zN = 0, g_zIdx = 0;
void zFlush() {
  if (!g_zN) return;
  static char ln[GET_LINE / 3 * 4 + 40];
  int k = snprintf(ln, sizeof(ln), "=Z %lu ", (unsigned long)g_zIdx++);
  b64(g_zLine, g_zN, ln + k); k += strlen(ln + k);
  k += snprintf(ln + k, sizeof(ln) - k, " %08lx\n", (unsigned long)calc::crc32(0, g_zLine, g_zN));
  Serial.write(reinterpret_cast<const uint8_t*>(ln), k);
  g_zN = 0;
}
void zDrain() {
  size_t got;
  do {
    got = 0;
    heatshrink_encoder_poll(&g_hse, g_zLine + g_zN, GET_LINE - g_zN, &got);
    g_zN += got;
    if (g_zN == GET_LINE) zFlush();
  } while (got);
}
void zPut(uint8_t* p, size_t n) {
  while (n) {
    size_t used = 0;
    heatshrink_encoder_sink(&g_hse, p, n, &used);
    p += used; n -= used;
    zDrain();
  }
}
void zEnd() {
  while (heatshrink_encoder_finish(&g_hse) == HSER_FINISH_MORE) zDrain();
  zFlush();
}

void fileGet(const String& cmd) {
  if (run::busy() || state::phase() != state::IDLE || revsrc::kmh() > 0.5f) { Serial.println("=X busy: only at standstill, outside a run"); return; }
  const bool list = cmd.length() > 1 && cmd[1] == '?';
  const bool z = cmd.length() > 1 && cmd[1] == 'z';
  String rest = cmd.substring(list || z ? 2 : 1); rest.trim();
  const int sp = rest.indexOf(' ');
  const String path = sp < 0 ? rest : rest.substring(0, sp);
  if (!path.startsWith(storage::ROOT_DIR) || path.indexOf("..") >= 0) { Serial.println("=X path must lie below /vmrr"); return; }
  // Formatting holds the SD lock for minutes: waiting here would stall the main loop.
  if (storage::info(storage::SD_CARD).formatting) { Serial.println("=X busy: SD formatting"); return; }
  storage::lockSd();
  fs::FS* f = storage::fs(storage::SD_CARD);
  File h = f ? f->open(path, "r") : File();
  if (!h) { storage::unlockSd(); Serial.println("=X not found"); return; }
  if (list) {
    if (!h.isDirectory()) { Serial.print("=L "); Serial.print((unsigned long)h.size()); Serial.print(' '); Serial.println(h.name()); }
    else for (File x = h.openNextFile(); x; x = h.openNextFile()) {
      Serial.print("=L "); Serial.print((unsigned long)(x.isDirectory() ? 0 : x.size())); Serial.print(' ');
      Serial.print(x.name()); Serial.println(x.isDirectory() ? "/" : "");
    }
    h.close(); storage::unlockSd(); Serial.println("=E"); return;
  }
  if (h.isDirectory()) { h.close(); storage::unlockSd(); Serial.println("=X is a directory"); return; }
  unsigned long off = 0, len = GET_CHUNK_MAX;
  if (sp >= 0) sscanf(rest.c_str() + sp + 1, "%lu %lu", &off, &len);
  if (len > (z ? GET_ZCHUNK_MAX : GET_CHUNK_MAX)) len = z ? GET_ZCHUNK_MAX : GET_CHUNK_MAX;
  const size_t size = h.size();
  if (off > size) off = size;
  h.seek(off);
  // Read in blocks. The card delivers about 48 kB/s over SPI at 10 MHz whatever the block
  // size (384 B, 8 kB and 32 kB measured on 2026-11-26): the SD limits the transfer.
  static uint8_t buf[GET_READ]; static char ln[GET_LINE / 3 * 4 + 40];
  size_t done = 0;
  uint32_t tRead = 0; const uint32_t t0 = micros();
  if (z) { heatshrink_encoder_reset(&g_hse); g_zN = 0; g_zIdx = 0; }
  while (done < len) {
    const size_t want = len - done < sizeof(buf) ? len - done : sizeof(buf);
    const uint32_t tr = micros();
    const size_t n = h.read(buf, want);
    tRead += micros() - tr;
    if (!n) break;
    if (z) { zPut(buf, n); done += n; continue; }
    for (size_t i = 0; i < n; i += GET_LINE) {
      const size_t m = n - i < GET_LINE ? n - i : GET_LINE;
      int k = snprintf(ln, sizeof(ln), "=G %lu ", off + done + i);
      b64(buf + i, m, ln + k); k += strlen(ln + k);
      k += snprintf(ln + k, sizeof(ln) - k, " %08lx\n", (unsigned long)calc::crc32(0, buf + i, m));
      Serial.write(reinterpret_cast<const uint8_t*>(ln), k);
    }
    done += n;
  }
  if (z) zEnd();
  h.close(); storage::unlockSd();
  // Behind the two numbers the client needs: time spent reading the SD and in total, ms.
  Serial.printf("=E %lu %lu %lu %lu\n", off + done, (unsigned long)size, (unsigned long)(tRead / 1000), (unsigned long)((micros() - t0) / 1000));
}
}  // namespace

void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;
  const float arg = cmd.substring(1).toFloat();

  // Pass-through to the controller, only sensible from the console (vesc.h).
  if (cmd[0] == ':') { vesc::terminal(cmd.c_str() + 1); return; }
  // Guided run (run.h). Whoever starts it is subject to the dead-man check.
  if (cmd == "Gb") { if (run::busy() || state::phase() != state::IDLE || storage::info(storage::SD_CARD).formatting) Serial.println("# SD bench: only at standstill"); else storage::sdBench(); return; }
  // Automatic zero by the drum motor (autozero.h): Z measures only, Zt also sets the tare, Zx stops.
  if (cmd[0] == 'Z') {
    if (cmd == "Zx") { autozero::stop(); note("Auto zero stopped"); return; }
    if (run::busy()) { fail("Not during a run"); return; }
    if (autozero::start(cmd == "Zt")) note("Auto zero: drum spins both ways, keep clear"); else fail(autozero::why());
    return;
  }
  if (cmd[0] == 'C' && cmd.length() > 1) { calib::command(cmd.c_str() + 1); return; }
  if (cmd[0] == 'G') { fileGet(cmd); return; }
  if (cmd[0] == 'X') { relaytest::start(static_cast<uint8_t>(constrain(cmd.substring(1).toInt(), 1, 20))); return; }
  if (cmd[0] == 'B') {   // stand revision, free text; "B" alone clears it
    if (settings::setStandRev(cmd.c_str() + 1)) note(cmd.length() > 1 ? "Stand revision set" : "Stand revision cleared");
    else fail("Stand revision too long");
    return;
  }
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
    case 'P': {   // P<T|U|R|H>| name | ... |  list entry as a wheels.md row (profiles.h)
      const char* why = "";
      if (profiles::addRow(cmd.length() > 1 ? cmd[1] : 0, cmd.c_str() + (cmd.length() > 1 ? 2 : 1), &why)) note("Parts entry set");
      else fail(why);
      break; }
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
    case 'D': {   // list /vmrr on the SD, two levels (storage.h layout); not during a run
      if (run::busy()) { fail("Not during a run"); break; }
      if (storage::info(storage::SD_CARD).formatting) { fail("SD formatting"); break; }   // holds the lock for minutes
      storage::lockSd();
      fs::FS* f = storage::fs(storage::SD_CARD);
      File root = f ? f->open(storage::ROOT_DIR) : File();
      if (!root || !root.isDirectory()) { storage::unlockSd(); fail("No /vmrr on the SD"); break; }
      if (cmd.length() > 1) {   // D<name>: print a text file below /vmrr, at most 16 kB
        String path = String(storage::ROOT_DIR) + "/" + cmd.substring(1);
        File t = f->open(path, "r");
        if (!t || t.isDirectory() || t.size() > 16384) { if (t) t.close(); storage::unlockSd(); fail("No such text file"); break; }
        while (t.available()) { String ln = t.readStringUntil('\n'); Serial.print("# | "); Serial.println(ln); }
        t.close(); storage::unlockSd(); break;
      }
      Serial.print("# SD "); Serial.print(storage::info(storage::SD_CARD).fsType); Serial.print(" "); Serial.println(storage::ROOT_DIR);
      for (File d = root.openNextFile(); d; d = root.openNextFile()) {
        Serial.print("#   "); Serial.print(d.name()); Serial.println(d.isDirectory() ? "/" : "");
        if (!d.isDirectory()) continue;
        for (File x = d.openNextFile(); x; x = d.openNextFile()) {
          Serial.print("#     "); Serial.print(x.name()); Serial.print("  "); Serial.print(static_cast<unsigned long>(x.size())); Serial.println(" B");
        }
      }
      storage::unlockSd();
      break;
    }
    case 'F': {   // format (storage.h): Fs SD (also readable, not during a run), Ff flash (only unreadable)
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
  static uint16_t n = 0;   // was uint8_t until 2026-11-26: lines over 255 characters wrapped
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
