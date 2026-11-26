#pragma once
//
// Control panel over WLAN.
//
// The test stand sets up its own access point and serves the UI itself -- no
// internet, no router, no app. On connecting it opens by itself (captive
// portal): a DNS server answers every query with its own address, and unknown
// paths are redirected to "/". That makes the operating system's connectivity
// check fail and it shows the page.
//
// The UI is the same file that also runs as a mock-up in the browser:
// firmware/web/index.html, gzip-baked into src/webui_html.h. If /api/state
// answers, the test stand is in charge; otherwise the page simulates itself.
//
// DEAD-MAN CHECK: as long as a run is driving, the operator device must report in.
// If it stays silent longer than LINK_TIMEOUT_MS, that is a shutdown criterion
// like any other (docs/07-safety.md §4) -- state.cpp queries linkAlive().
//
// OWN TASK, since 2026-11-23: the web server of the Arduino library BLOCKS when a
// browser opens a connection but sends no request -- iOS does that
// pre-emptively, and the library then waits a fixed 5 s (HTTP_MAX_DATA_WAIT). In
// the main loop that meant: 2.8 s standstill, the controller's dead-man timeout
// (300 ms) tripped, and the speed control released because no measurements came
// for over 1 s. Two starts aborted that way. That is why DNS, web server and JSON
// building now run in their own task on core 0, next to the WLAN. Commands from
// the browser go via a queue to the main loop and are executed there -- the
// control stays single-threaded, the network can no longer stall it.
//
// The JSON building reads values that the main loop writes. These are
// single 32-bit quantities, which are atomic on the ESP32; an inconsistent
// pair (such as counter and timestamp of a sensor) is possible and costs at
// most one display for 200 ms. NOTHING is actuated from this task.
//
#include <Arduino.h>

namespace webui {

// Command receiver. It is deliberately the same parser as for the serial
// console (docs/10-firmware.md G5), so that there is only ONE command vocabulary.
typedef void (*CommandFn)(const String&);

// Builds the reply to /api/state. The respective firmware variant supplies it;
// that way the web server knows nothing about the measuring chain or the drive
// and is the same for all variants. The buffer is given, the function writes JSON into it.
typedef void (*StateJsonFn)(char* buf, size_t len);

// htmlGz points to the gzip-baked UI in PROGMEM. It also comes from
// outside, so that each variant can serve its own page.
// Second JSON supplier for /api/run (guided run, run.h): own path
// and own, larger buffer, so that /api/state stays small and fast.
// Files under /runs/ come from LittleFS (reports), /runs lists them.
void begin(CommandFn handler, StateJsonFn state, StateJsonFn runJson,
           const uint8_t* htmlGz, size_t htmlGzLen);
// Call from the main loop: executes the commands that the web server has
// queued. The network is no longer served here.
void service();

// false as soon as a client was there and then stays silent longer than LINK_TIMEOUT_MS.
// As long as no client was ever there, true -- the stand can also be driven
// from the console without WLAN.
bool linkAlive();
// Who is driving? The dead-man check only applies when the drive was started
// from the browser. Whoever starts from the console is the operator -- a
// sleeping phone must not kill their run (2026-11-23, several times).
bool inWebCommand();           // true while a browser command is being executed
void setWebOperator(bool web); // remember at start
bool webOperator();

bool clientSeen();
uint32_t msSinceClient();

}  // namespace webui
