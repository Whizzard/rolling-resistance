//
// Bicycle tire test stand -- main program.
//
// ONE path: motor controller over UART, load cell on the HX711, state machine with
// plateaus and log, operation over WLAN. The earlier two variants
// (stub drive with LCD and the test-run variant) were merged on 2026-11-20 --
// whatever never drove the motor is gone.
// Reasoning: docs/04-hardware.md §14.
//
// SPEED comes from the controller speed, not from the drum marks: the
// Hall sensors on the drum are not wired yet. With the number of pole pairs measured
// on 2026-11-17, ERPM / 15 is the drum speed, and that is a calculation, not an
// estimate.
//
// THREE DEAD-MAN SWITCHES, independent of each other:
//   1. Here: if the operator computer stays away longer than LINK_TIMEOUT_MS, the
//      drive is released (drum coasts).
//   2. In the controller: if no setpoint arrives for longer than timeout_msec, it shuts
//      off by itself. Also works when THIS firmware hangs.
//   3. In hardware: the short-circuit relays on the emergency stop. The only one that
//      also works if the controller or computer fails (docs/07-safety.md §3).
//
#include "calib.h"
#include "relaytest.h"
#include "profiles.h"
#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "webui.h"
#include "webui_html.h"
#include "console.h"
#include "brake.h"
#include "live.h"
#include "statejson.h"
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

namespace {

bool g_linkWasAlive = true;

}  // namespace

void setup() {
  // USB CDC: if no computer is reading, the transmit buffer fills up, and every
  // print() blocks until the timeout. Measured with a 20 ms timeout: 266 ms per round
  // without a reader, and on 2026-11-24 a round of 980 ms at the end of a
  // measurement point (logger::point and the angle bins are over 30 single prints) --
  // the controller counted as silent for 1 s, the control shut off. Hence: large buffer
  // and timeout 0. With a reader the buffer never fills, without one data is discarded.
  Serial.setTxBufferSize(8192);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  delay(200);
  Serial.println();
  Serial.println("# Bicycle tire test stand -- firmware");

  settings::begin();
  brake::begin();      // energise the relays as soon as the firmware runs (brake.h)
  statusled::begin();
  torque::begin();
  revsrc::begin();
  wheel::begin();
  speedctl::begin();
  logger::begin();
  tirepressure::begin();
  state::begin();
  storage::begin();    // flash + SD + storage task, BEFORE run::begin (writes to /runs)
  profiles::begin();   // wheel profiles from the flash (needs storage)
  run::begin(live::forRun);
  webui::begin(console::handleCommand, statejson::build, run::json,
               WEBUI_HTML_GZ, WEBUI_HTML_GZ_LEN);
  vesc::begin();

  if (!settings::calibrated())
    Serial.println("# Load cell NOT calibrated -- raw values only, no forces.");

  console::printHelp();
}

void loop() {
  {
    static uint32_t prevMs = 0;
    const uint32_t now = millis();
    if (prevMs) console::loopRound(now - prevMs, vesc::values().ageMs);
    prevMs = now;
  }
  // Internal messages (state machine, control) also into the UI.
  {
    // As large as logger's buffer and compared over its full length: with 96 bytes a
    // longer note (W18, 114 characters) never matched its truncated copy and was written
    // to the raw log on every loop round -- 42029 times in run 2026-11-26-1651, 90 s of
    // raw data lost to the overflow (2026-11-26).
    static char seen[128] = "";
    if (strncmp(logger::lastNote(), seen, sizeof(seen) - 1) != 0) {
      strncpy(seen, logger::lastNote(), sizeof(seen) - 1);
      if (logger::lastNoteBad()) console::fail(seen); else console::note(seen);
      rawlog::text(seen);
    }
  }
  // Every section is timed; the longest of the reporting period appears by
  // name in the status line ('i'). That way a blocker can be found without guessing.
#define TIMED(name, call) do { const uint32_t t0_ = millis(); call; const uint32_t dt_ = millis() - t0_; \
    console::loopSection(name, dt_); } while (0)
  TIMED("statusled", statusled::service());
  TIMED("webui", webui::service());
  TIMED("vesc", vesc::service());

  // In the measuring state every new raw value of the cell is sorted into the
  // revolution-synchronous accumulator. This must be HERE, where update() is called:
  // only this call knows that a new value has just arrived.
  TIMED("torque", {
    const bool fresh = torque::update();
    if (fresh && state::phase() == state::POINT) torque::accumulate(revsrc::markIndexInRev());
    live::service(fresh);
    profiles::service();      // pending profile changes, written at standstill
    rawlog::service(fresh);   // raw data: edges, cell, controller, pressure, setpoints
  });

  TIMED("revsrc", revsrc::service());
  TIMED("wheel", wheel::service());
  TIMED("speedctl", speedctl::service());
  TIMED("brake", brake::service());
  relaytest::service();
  calib::service();
  TIMED("state", state::service());
  TIMED("run", run::service());
  TIMED("serial", console::pollSerial());
  TIMED("telemetry", console::telemetry());
#undef TIMED

  // Dead-man switch: if the operator connection breaks, the drive is released.
  // Evaluate only the EDGE, otherwise every pass would send release().
  const bool alive = webui::linkAlive();
  if (g_linkWasAlive && !alive && speedctl::running()) {
    speedctl::coast();
    console::note("Link lost, drive released");
    Serial.println("# Operator link lost -- drive released");
  }
  g_linkWasAlive = alive;
}
