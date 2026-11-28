#include "profiles.h"
#include "webui.h"
#include "run.h"
#include "wallclock.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

#include "calc/crc32.h"
#include "calc/zip.h"
#include "calc/rawlog.h"

#include "storage.h"
#include "rawlog.h"

#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace webui {
namespace {

// Commands from the web server to the main loop (webui.h: own task).
struct CmdMsg { char text[160]; };   // run metadata (Rm...) needs room
QueueHandle_t g_cmdQueue = nullptr;
TaskHandle_t  g_task = nullptr;

WebServer     g_server(WEBUI_PORT);
DNSServer     g_dns;
CommandFn     g_cmd     = nullptr;
StateJsonFn   g_state   = nullptr;
StateJsonFn   g_runJson = nullptr;
const uint8_t* g_html   = nullptr;
size_t        g_htmlLen = 0;

const IPAddress AP_IP(WEBUI_AP_IP[0], WEBUI_AP_IP[1], WEBUI_AP_IP[2], WEBUI_AP_IP[3]);

bool     g_seen     = false;   // has a client been there at all?
bool     g_inWebCmd = false;
bool     g_webOperator = false;
uint32_t g_lastSeen = 0;

void touch() { g_seen = true; g_lastSeen = millis(); }

// --------------------------------------------------------------- Log
// Purpose is tracing at the USB port: which address does the phone query
// (that shows whether the connectivity check takes effect), who connects,
// and is the UI's cyclic polling running. /api/state is NOT logged
// individually -- 5 lines per second would be unreadable; a count instead.
uint32_t g_polls = 0, g_cmds = 0, g_other = 0, g_lastSummary = 0;

void logReq(const char* what) {
  Serial.print("# HTTP ");
  Serial.print(g_server.method() == HTTP_GET ? "GET " : "??? ");
  Serial.print(g_server.uri());
  Serial.print("  <- ");
  Serial.print(g_server.client().remoteIP());
  Serial.print("  ");
  Serial.print(what);
  const String ua = g_server.header("User-Agent");
  if (ua.length()) { Serial.print("  UA="); Serial.print(ua.substring(0, 70)); }
  Serial.println();
}

void summary() {
  const uint32_t now = millis();
  if (now - g_lastSummary < HTTP_SUMMARY_MS) return;
  g_lastSummary = now;
  if (!g_polls && !g_cmds && !g_other) return;
  Serial.print("# HTTP 5 s: ");
  Serial.print(g_polls);  Serial.print(" x /api/state, ");
  Serial.print(g_cmds);   Serial.print(" x /api/cmd, ");
  Serial.print(g_other);  Serial.print(" x other, clients ");
  Serial.print(WiFi.softAPgetStationNum());
  Serial.print(", Link ");
  Serial.println(linkAlive() ? "up" : "LOST");
  g_polls = g_cmds = g_other = 0;
}

void onWiFiEvent(arduino_event_id_t ev, arduino_event_info_t info) {
  if (ev == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.print("# WLAN: client connected, now ");
    Serial.println(WiFi.softAPgetStationNum());
  } else if (ev == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    Serial.print("# WLAN: client disconnected, now ");
    Serial.println(WiFi.softAPgetStationNum());
  } else if (ev == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
    Serial.println("# WLAN: address assigned via DHCP");
  }
}

// Tiny page for the portal window. It tries to hand over to Safari and
// loads the real UI afterwards if that does not work. No button: both
// happen by themselves.
// --------------------------------------------- Redirect to the name
//
// Whoever types a foreign name ends up here via the wildcard DNS and should
// then see the memorable name in the address bar.
//
// TWO THINGS ARE NEVER REDIRECTED, both for good reason:
//
//   1. The IP. It is the path that depends on NOTHING -- no DNS, no mDNS.
//      Bending it to a name would make the last resort, of all things,
//      depend on name resolution.
//   2. The host names of the connectivity checks. A redirect there is exactly
//      the "captive portal" signal -- and then iOS starts discarding the network
//      again after a short time (docs/04-hardware.md §13.2). The check paths do
//      have their own handlers, but an unknown path on a check host
//      would otherwise end up here.

bool isIpLiteral(const String& h) {
  if (h.length() == 0) return true;      // without a Host header better do nothing
  for (size_t i = 0; i < h.length(); i++) {
    const char c = h[i];
    if (c == ':') break;                 // cut off the port
    if ((c < '0' || c > '9') && c != '.') return false;
  }
  return true;
}

bool isProbeHost(const String& h) {
  static const char* const HOSTS[] = {
    "captive.apple.com", "apple.com", "www.apple.com",          // iOS, macOS
    "connectivitycheck.gstatic.com", "clients3.google.com",     // Android
    "www.msftconnecttest.com", "www.msftncsi.com",              // Windows
    "detectportal.firefox.com",
  };
  for (size_t i = 0; i < sizeof(HOSTS) / sizeof(HOSTS[0]); i++)
    if (h.equalsIgnoreCase(HOSTS[i])) return true;
  return false;
}

// true if redirected -- the caller is then done.
bool redirectedToName() {
  const String host = g_server.hostHeader();
  if (isIpLiteral(host)) return false;
  if (isProbeHost(host)) return false;
  if (host.endsWith(".local") || host.endsWith(".local.")) return false;  // already there

  const String target = String("http://") + WEBUI_HOSTNAME + ".local/";
  Serial.print("# "); Serial.print(host); Serial.print(" -> "); Serial.println(target);
  g_server.sendHeader("Location", target);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(302, "text/plain", "");
  return true;
}

void sendIndex() {
  logReq("-> UI");
  g_server.sendHeader("Content-Encoding", "gzip");
  g_server.sendHeader("Cache-Control", "no-store");
  // charset MUST be included: without it iOS reads the page as Latin-1 and "±0.5 °C"
  // becomes "Â±0.5 Â°C". The <meta charset> in the document comes too late when the
  // header claims something else.
  g_server.send_P(200, "text/html; charset=utf-8",
                  reinterpret_cast<const char*>(g_html), g_htmlLen);
}

void handleState() {
  touch();
  g_polls++;
  if (g_server.hasArg("now")) wallclock::set(strtoul(g_server.arg("now").c_str(), nullptr, 10));

  // The variant knows the content, not the web server.
  // 1.5 kB: about 700 bytes for load cell, drive, temperatures, currents and
  // settings, plus since 2026-11-22 the sensor wiring test and the
  // test wheel (together a good 150 bytes). Too small, and snprintf truncates silently --
  // the page would get broken JSON.
  // Since 2026-11-24 also: tire pressure, tire temperature, sensor name, speed source.
  // Since 2026-11-25 also: storage and raw data (about 200 bytes).
  char buf[5632];   // 2026-11-26: + calibration procedure (up to about 900), + change timestamps of the settings (about 450 bytes), stand revision and commit (up to 350)
  buf[0] = '\0';
  if (g_state) g_state(buf, sizeof(buf));
  if (buf[0] == '\0') snprintf(buf, sizeof(buf), "{}");

  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", buf);
}

// Guided run: own buffer, up to 48 measurement points plus reference points,
// each point with scatter, inertia correction and dv/dt.
void handleRun() {
  touch();
  g_other++;
  static char buf[12288];
  buf[0] = '\0';
  if (g_runJson) g_runJson(buf, sizeof(buf));
  if (buf[0] == '\0') snprintf(buf, sizeof(buf), "{}");
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", buf);
}

// Wheel profiles (profiles.h): GET = everything as JSON; with op= an edit from Setup,
// answered with the new state or {"err":"..."}.
//   op=ren&l=t|s|r|h&o=<old>&n=<new>   op=del&l=..&n=<name>   op=etrto&n=<tire>&e=<etrto>
//   op=pdel&i=<index>   op=pclr&i=<index> (wheel model only)
void handleProfiles() {
  touch();
  g_other++;
  String out;
  if (g_server.hasArg("op")) {
    const String op = g_server.arg("op");
    const String ls = g_server.arg("l");
    const profiles::List l = ls == "s" ? profiles::TUBE : ls == "r" ? profiles::RIM : ls == "h" ? profiles::HUB : profiles::TIRE;
    const char* why = "Unknown operation";
    bool ok = false;
    if (op == "ren")        ok = profiles::rename(l, g_server.arg("o").c_str(), g_server.arg("n").c_str(), &why);
    else if (op == "del")   ok = profiles::remove(l, g_server.arg("n").c_str(), &why);
    else if (op == "etrto") ok = profiles::setEtrto(g_server.arg("n").c_str(), g_server.arg("e").c_str(), &why);
    else if (op == "pdel")  ok = profiles::removeProfile(g_server.arg("i").toInt(), &why);
    else if (op == "pclr")  ok = profiles::clearModel(g_server.arg("i").toInt(), &why);
    else if (op == "pset") {   // create (i=-1) or change: t s r h e(ETRTO) m(kg) bd bl bn bt j
      auto f = [&](const char* k) { const String v = g_server.arg(k); return v.length() ? v.toFloat() : NAN; };
      ok = profiles::save(g_server.hasArg("i") ? g_server.arg("i").toInt() : -1,
                          g_server.arg("t").c_str(), g_server.arg("e").c_str(), g_server.arg("s").c_str(),
                          g_server.arg("r").c_str(), g_server.arg("h").c_str(), f("m"),
                          f("bd"), f("bl"), f("bn"), f("bt"), f("j"), &why) >= 0;
    }
    else if (op == "attr") {   // l=s|r n(name) a0..a3: tube or rim details
      profiles::Attr at;
      for (uint8_t k = 0; k < profiles::ATTR_N; k++) { char key[4]; snprintf(key, sizeof(key), "a%u", k); at.a[k] = g_server.arg(key); }
      ok = profiles::setAttr(l, g_server.arg("n").c_str(), at, &why);
    }
    else if (op == "hub") {    // n(name) sp ax br dr(1/0) co
      profiles::Hub h;
      h.spokes = g_server.arg("sp").toInt(); h.axle = g_server.arg("ax"); h.brake = g_server.arg("br");
      h.drive = g_server.arg("dr").length() ? static_cast<int8_t>(g_server.arg("dr").toInt()) : -1; h.colour = g_server.arg("co");
      ok = profiles::setHub(g_server.arg("n").c_str(), h, &why);
    }
    if (!ok) {
      char b[96]; snprintf(b, sizeof(b), "{\"err\":\"%s\"}", why);
      g_server.sendHeader("Cache-Control", "no-store");
      g_server.send(200, "application/json; charset=utf-8", b);
      return;
    }
  }
  profiles::json(out);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", out);
}

struct SdLock { SdLock() { storage::lockSd(); } ~SdLock() { storage::unlockSd(); } };


// ---------------------------------------------------------- Raw data
//
// A raw stream (raw_<session>.csv, calc/rawlog.h) split into one CSV per sensor with column
// names, plus its .json and .md, converted while sending (/fs/...?csv). Streamed ZIP with
// data descriptor and HTTP chunked: one read pass per CSV, no intermediate buffer, no
// Content-Length.
// If the first line is not a sync record, the file goes into the archive unchanged.
//
// While streaming, this task stands still -- /api/state waits. So fetch larger
// files at standstill. Reading from the FLASH also briefly locks the cache per
// access and with it the sensor ISRs (config.h, RAWLOG_FLASH_IDLE_MS).

struct ZipOut {
  uint8_t  buf[1436];
  size_t   n = 0;
  uint32_t pos = 0;           // byte in the archive
  uint32_t crc = 0, size = 0; // of the current entry
  void flush() { if (n) { g_server.sendContent(reinterpret_cast<const char*>(buf), n); n = 0; touch(); } }
  void raw(const uint8_t* p, size_t k) {
    while (k) { const size_t m = (sizeof(buf) - n < k) ? sizeof(buf) - n : k;
      memcpy(buf + n, p, m); n += m; p += m; k -= m; pos += m; if (n == sizeof(buf)) flush(); }
  }
  void data(const uint8_t* p, size_t k) { crc = calc::crc32(crc, p, k); size += k; raw(p, k); }
  void data(const char* s) { data(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
};

struct ZipEntry { char name[64]; uint32_t crc, size, offset; };

void zipBegin(ZipOut& z, ZipEntry& e, const char* name) {
  strncpy(e.name, name, sizeof(e.name) - 1); e.name[sizeof(e.name) - 1] = '\0';
  e.offset = z.pos;
  uint8_t h[calc::ZIP_LOCAL_FIXED + sizeof(e.name)];
  z.raw(h, calc::zipLocalHeaderStreamed(h, e.name));
  z.crc = 0; z.size = 0;
}
void zipEnd(ZipOut& z, ZipEntry& e) {
  e.crc = z.crc; e.size = z.size;
  uint8_t d[calc::ZIP_DESCRIPTOR];
  z.raw(d, calc::zipDataDescriptor(d, e.crc, e.size));
}
void zipFinish(ZipOut& z, const ZipEntry* e, size_t n) {
  const uint32_t cd = z.pos;
  uint8_t h[calc::ZIP_CENTRAL_FIXED + sizeof(e->name)];
  for (size_t i = 0; i < n; i++)
    z.raw(h, calc::zipCentralHeader(h, e[i].name, e[i].crc, e[i].size, e[i].offset, 0,
                                    calc::ZIP_DOS_DATE_1980, calc::ZIP_FLAG_DESCRIPTOR));
  z.raw(h, calc::zipEndRecord(h, static_cast<uint16_t>(n), z.pos - cd, cd));
  z.flush();
  g_server.sendContent("", 0);   // end of the chunked stream
}

// Reads a file line by line through one fixed buffer in 2 kB blocks. The buffer holds
// the longest record line (calc::rawcsv::REC_MAX) many times over; a longer line is handed out
// cut short with complete = false and skipped up to its \n. lock = take the SD lock per block
// (otherwise the caller holds it for the whole pass).
struct LineReader {
  static constexpr size_t CAP = 2048 + 2 * calc::rawcsv::REC_MAX;
  File& in; bool lock;
  uint8_t* b;
  size_t have = 0, off = 0;
  bool eof = false, skip = false;
  uint32_t blocks = 0;
  LineReader(File& f, bool sdLock) : in(f), lock(sdLock) { static uint8_t buf[CAP]; b = buf; }
  // Next line incl. its \n; false at the end of the file. complete = false: no \n (cut off
  // at the end) or longer than the buffer.
  bool next(const char** p, size_t* n, bool* complete) {
    for (;;) {
      if (skip) {
        const uint8_t* nl = static_cast<const uint8_t*>(memchr(b + off, '\n', have - off));
        if (nl) { off = static_cast<size_t>(nl - b) + 1; skip = false; } else off = have;
      }
      if (!skip) {
        const uint8_t* nl = static_cast<const uint8_t*>(memchr(b + off, '\n', have - off));
        if (nl) {
          *p = reinterpret_cast<const char*>(b + off); *n = static_cast<size_t>(nl - b) + 1 - off;
          off += *n; *complete = true; return true;
        }
        if (off == 0 && have == CAP) {   // too long for the buffer
          *p = reinterpret_cast<const char*>(b); *n = have; *complete = false;
          off = have; skip = true; return true;
        }
      }
      if (eof) {
        if (!skip && off < have) {
          *p = reinterpret_cast<const char*>(b + off); *n = have - off; *complete = false;
          off = have; return true;
        }
        return false;
      }
      memmove(b, b + off, have - off); have -= off; off = 0;
      size_t want = CAP - have; if (want > 2048) want = 2048;
      if (lock) storage::lockSd();
      const size_t got = in.read(b + have, want);
      if (lock) storage::unlockSd();
      if (!got) eof = true;
      have += got;
      if (++blocks % 64 == 0) touch();   // a long pass must not look like a lost web link
    }
  }
};

// A raw stream starts with a complete sync record (rawlog.cpp writes it first).
bool rawStart(LineReader& lr) {
  const char* p; size_t n; bool complete; calc::rawcsv::Rec r;
  return lr.next(&p, &n, &complete) && complete && calc::rawcsv::parseLine(p, n, r) && r.kind == calc::rawcsv::K_SYNC;
}

// <stem>.<ext> next to a raw <stem>.csv, whole into buf (at most cap - 1 bytes). Length, 0 = none.
size_t readSibling(fs::FS& f, const String& csvPath, const char* ext, char* buf, size_t cap, bool lock) {
  buf[0] = '\0';
  if (!csvPath.endsWith(".csv")) return 0;
  const String p = csvPath.substring(0, csvPath.length() - 4) + ext;
  if (lock) storage::lockSd();
  File x = f.exists(p) ? f.open(p, "r") : File();
  size_t n = 0;
  if (x) { for (size_t k; n < cap - 1 && (k = x.read(reinterpret_cast<uint8_t*>(buf) + n, cap - 1 - n)) > 0;) n += k; x.close(); }
  if (lock) storage::unlockSd();
  buf[n] = '\0';
  return n;
}

// One CSV from the raw stream: one pass, only the lines of this type.
// Returns the number of rows; bad = a line that is not a valid record (the last one cut
// off by a power loss, as a rule) -- every other line is still taken.
uint32_t csvPass(File& in, calc::rawcsv::Kind which, ZipOut& z, bool& bad) {
  static char line[calc::rawcsv::REC_MAX + 64];
  uint32_t rows = 0;
  bad = false;
  in.seek(0);
  LineReader lr(in, false);
  if (calc::rawcsv::csvHeader(line, sizeof(line), which)) z.data(line);
  const char* p; size_t n; bool complete;
  while (lr.next(&p, &n, &complete)) {
    calc::rawcsv::Rec r;
    if (!complete || !calc::rawcsv::parseLine(p, n, r)) { bad = true; continue; }
    if (!calc::rawcsv::csvBelongs(which, r.kind)) continue;
    const size_t k = calc::rawcsv::csvLine(line, sizeof(line), r);
    if (k) { z.data(reinterpret_cast<const uint8_t*>(line), k); rows++; }
  }
  return rows;
}

// Download name: inside a session directory <session>_<file>, so files from several runs
// do not overwrite each other in the Downloads folder.
String downloadName(const String& path) {
  const int s = path.lastIndexOf('/');
  const String file = path.substring(s + 1);
  const int s2 = s > 0 ? path.lastIndexOf('/', s - 1) : -1;
  const String parent = s2 >= 0 ? path.substring(s2 + 1, s) : String("");
  if (file.indexOf(parent) >= 0) return file;   // raw_<session>.csv: already unique
  return path.startsWith(String(storage::ROOT_DIR) + "/") && parent.length() && parent != "vmrr" ? parent + "_" + file : file;
}

void sendRawZip(fs::FS& f, const String& path) {
  File in = f.open(path, "r");
  if (!in) { g_server.send(404, "text/plain", "not found"); return; }
  const String file = path.substring(path.lastIndexOf('/') + 1);
  const String dl = downloadName(path);
  const String base = dl.endsWith(".csv") ? dl.substring(0, dl.length() - 4) : dl;
  bool raw = false;
  { LineReader lr(in, false); raw = rawStart(lr); }

  g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + base + ".zip\"");
  g_server.send(200, "application/zip", "");

  static ZipOut z;
  z = ZipOut();
  ZipEntry e[calc::rawcsv::CSV_FILE_N + 3];
  size_t ne = 0;
  char name[64];

  if (!raw) {   // not a raw stream: into the archive unchanged
    zipBegin(z, e[ne], file.c_str());
    in.seek(0);
    static uint8_t b[1024];
    for (size_t k; (k = in.read(b, sizeof(b))) > 0;) z.data(b, k);
    zipEnd(z, e[ne++]);
    in.close();
    zipFinish(z, e, ne);
    return;
  }

  uint32_t rows[calc::rawcsv::CSV_FILE_N];
  bool anyBad = false;
  for (size_t i = 0; i < calc::rawcsv::CSV_FILE_N; i++) {
    const calc::rawcsv::Kind t = calc::rawcsv::CSV_FILES[i];
    snprintf(name, sizeof(name), "%s_%s", base.c_str(), calc::rawcsv::csvName(t));
    zipBegin(z, e[ne], name);
    bool bad = false;
    rows[i] = csvPass(in, t, z, bad);
    anyBad |= bad;
    zipEnd(z, e[ne++]);
  }
  // Then the metadata and the legend as they are on the card, and the row counts.
  static char side[calc::rawcsv::HEADER_MAX + 1];
  for (const char* ext : {".json", ".md"}) {
    const size_t k = readSibling(f, path, ext, side, sizeof(side), false);
    if (!k) continue;
    snprintf(name, sizeof(name), "%s%s", base.c_str(), ext);
    zipBegin(z, e[ne], name);
    z.data(reinterpret_cast<const uint8_t*>(side), k);
    zipEnd(z, e[ne++]);
  }
  snprintf(name, sizeof(name), "%s_info.txt", base.c_str());
  zipBegin(z, e[ne], name);
  char l[96];
  snprintf(l, sizeof(l), "source_file=%s\nsource_bytes=%lu\n", file.c_str(), static_cast<unsigned long>(in.size()));
  z.data(l);
  for (size_t i = 0; i < calc::rawcsv::CSV_FILE_N; i++) {
    snprintf(l, sizeof(l), "rows_%s=%lu\n", calc::rawcsv::csvName(calc::rawcsv::CSV_FILES[i]), static_cast<unsigned long>(rows[i]));
    z.data(l);
  }
  z.data(anyBad ? "end=truncated (a line incomplete or unknown, as a rule the last one; all other lines are valid)\n" : "end=clean\n");
  zipEnd(z, e[ne++]);
  in.close();
  zipFinish(z, e, ne);
}

// ---------------------------------------------------------- File browser (since 2026-11-26)
//
//   GET /api/files?m=sd|flash&p=/vmrr       one directory as JSON (Setup -> Storage)
//   GET /fs/<sd|flash><path>                 a file for download (application/octet-stream,
//                                            see below), file name <session>_<file>
//   GET /fs/<sd|flash><path>?view            the same as text/plain inline (the UI renders .md)
//   GET /fs/<sd|flash><raw_*.csv>?csv        raw stream split into one CSV per sensor (+ .json, .md), as .zip
//   GET /fs/<sd|flash><dir>?zip              all files of a directory (not recursive) as .zip
//
// Download is application/octet-stream: text/markdown with Content-Disposition was still
// shown in the window by Safari on the iPad (2026-11-24, iOS 12), and from the view the
// file cannot be passed on via AirDrop.
//
// SD ACCESS AND THE RAW LOG: the storage task writes the raw data under the same SD lock.
// A plain download therefore takes the lock per 1.4 kB chunk, never for the whole file.
// The two .zip variants read the whole file under one lock: refused while a run or a raw
// log is active on that medium (409).

void appendJsonString(String& o, const String& v) {
  o += '"';
  for (size_t i = 0; i < v.length(); i++) {
    const char c = v[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (static_cast<uint8_t>(c) < 0x20) { char h[8]; snprintf(h, sizeof(h), "\\u%04x", c); o += h; }
    else o += c;
  }
  o += '"';
}

bool fsMedia(const String& n, storage::Media& m) {
  if (n == "sd") { m = storage::SD_CARD; return true; }
  if (n == "flash") { m = storage::FLASH; return true; }
  return false;
}
bool pathOk(const String& p) { return p.length() && p[0] == '/' && p.indexOf("..") < 0 && p.indexOf('"') < 0; }

void handleFiles() {
  g_other++;
  storage::Media m;
  String p = g_server.hasArg("p") ? g_server.arg("p") : String(storage::ROOT_DIR);
  while (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
  if (!fsMedia(g_server.arg("m"), m) || !pathOk(p)) { g_server.send(400, "text/plain", "bad request"); return; }
  const storage::Info si = storage::info(m);
  // While formatting the storage task holds the SD lock for seconds; waiting for it here
  // would stall the whole web task, /api/state included (2026-11-26).
  if (si.formatting) { g_server.send(503, "text/plain", "formatting"); return; }
  String o = "{\"m\":\"" + String(storage::name(m)) + "\",\"p\":";
  appendJsonString(o, p);
  o += ",\"state\":" + String(static_cast<unsigned>(si.state)) + ",\"fs\":\"" + si.fsType + "\",\"pct\":" + String(si.pct)
     + ",\"mb\":" + String(static_cast<unsigned long>(si.total >> 20))
     + ",\"rec\":";
  appendJsonString(o, rawlog::active() && rawlog::media() == m ? String(rawlog::fileName()) : String(""));
  o += ",\"d\":[";
  if (m == storage::SD_CARD) storage::lockSd();
  fs::FS* f = storage::fs(m);
  File dir = f ? f->open(p) : File();
  bool first = true, found = dir && dir.isDirectory();
  if (found) {
    for (File x = dir.openNextFile(); x; x = dir.openNextFile()) {
      String fn = String(x.name()); fn = fn.substring(fn.lastIndexOf('/') + 1);
      if (fn.startsWith(".")) continue;
      if (!first) o += ",";
      first = false;
      o += "{\"n\":"; appendJsonString(o, fn);
      o += ",\"dir\":" + String(x.isDirectory() ? 1 : 0) + ",\"s\":" + String(static_cast<unsigned long>(x.size()))
         + ",\"t\":" + String(static_cast<unsigned long>(x.getLastWrite())) + "}";
      if (o.length() > 24000) break;   // a few hundred entries at most per answer
    }
  }
  if (m == storage::SD_CARD) storage::unlockSd();
  o += "],\"ok\":" + String(found ? "true" : "false") + "}";
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", o);
}

void sendPlain(storage::Media m, const String& path, bool view) {
  const bool sd = m == storage::SD_CARD;
  if (sd) storage::lockSd();
  fs::FS* f = storage::fs(m);
  File in = f ? f->open(path, "r") : File();
  const size_t size = in ? in.size() : 0;
  if (sd) storage::unlockSd();
  if (!in || in.isDirectory()) { if (in) in.close(); g_server.send(404, "text/plain", "not found"); return; }
  // Small text for the viewer in one piece through the normal send() path (2026-11-26: a
  // 713 B info.md came out empty in the viewer on the iPad with the streamed variant).
  if (view && size <= 16384) {
    String body;
    body.reserve(size + 1);
    if (sd) storage::lockSd();
    static uint8_t tb[512];
    for (;;) { const size_t k = in.read(tb, sizeof(tb)); if (!k) break; body.concat(reinterpret_cast<const char*>(tb), k); }
    in.close();
    if (sd) storage::unlockSd();
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.send(200, "text/plain; charset=utf-8", body);
    return;
  }
  g_server.setContentLength(size);
  g_server.sendHeader("Cache-Control", "no-store");
  if (!view) g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + downloadName(path) + "\"");
  g_server.send(200, view ? "text/plain; charset=utf-8" : "application/octet-stream", "");
  WiFiClient c = g_server.client();
  static uint8_t b[1436];
  for (size_t done = 0; done < size;) {
    if (sd) storage::lockSd();
    const size_t k = in.read(b, sizeof(b));
    if (sd) storage::unlockSd();
    if (!k || c.write(b, k) != k) break;
    done += k;
    touch();
  }
  if (sd) storage::lockSd();
  in.close();
  if (sd) storage::unlockSd();
}

// Overview of a raw file as JSON for the viewer (?info, since 2026-11-26): metadata (.json next to it),
// time span, lines and bytes per kind, edges per channel, dropped bytes, the first and
// last note messages. One pass through the file on the ESP (a 10 MB file takes some
// seconds); the SD lock is taken per 2 kB block, so a running raw log is not held up.
void sendRawInfo(storage::Media m, const String& path) {
  const bool sd = m == storage::SD_CARD;
  if (sd) storage::lockSd();
  fs::FS* f = storage::fs(m);
  File in = f ? f->open(path, "r") : File();
  const size_t size = in ? in.size() : 0;
  if (sd) storage::unlockSd();
  if (!in) { g_server.send(404, "text/plain", "not found"); return; }
  // metadata from <stem>.json next to it (shown as tables by the viewer); "" if missing
  static char hdr[calc::rawcsv::HEADER_MAX + 1];
  readSibling(*f, path, ".json", hdr, sizeof(hdr), sd);
  bool raw = false;
  { LineReader lr0(in, sd); raw = rawStart(lr0); }
  String o = "{\"size\":" + String(static_cast<unsigned long>(size)) + ",\"hdrLen\":0,\"hdr\":";
  appendJsonString(o, String(hdr));
  if (!raw) {
    o += ",\"ok\":false}";
    if (sd) storage::lockSd();
    in.close();
    if (sd) storage::unlockSd();
    g_server.send(200, "application/json; charset=utf-8", o);
    return;
  }
  if (sd) storage::lockSd();
  in.seek(0);
  if (sd) storage::unlockSd();
  LineReader lr(in, sd);
  calc::rawcsv::Summary sm;
  constexpr uint8_t TXT_FIRST = 30, TXT_LAST = 10;
  static char txt[TXT_FIRST + TXT_LAST][72];
  static int64_t txtT[TXT_FIRST + TXT_LAST];
  uint32_t txtN = 0;
  bool lastBad = false;
  const char* p; size_t n; bool complete;
  while (lr.next(&p, &n, &complete)) {
    calc::rawcsv::Rec r;
    lastBad = !complete || !calc::rawcsv::parseLine(p, n, r);
    if (lastBad) { sm.bad++; continue; }
    sm.add(r, n);
    if (r.kind == calc::rawcsv::K_NOTE) {
      const uint32_t slot = txtN < TXT_FIRST ? txtN : TXT_FIRST + (txtN - TXT_FIRST) % TXT_LAST;
      calc::rawcsv::unquote(txt[slot], sizeof(txt[0]), r.body, r.bodyLen);
      txtT[slot] = r.t;
      txtN++;
    }
  }
  if (sd) storage::lockSd();
  in.close();
  if (sd) storage::unlockSd();
  // end: clean, truncated (only the last line is broken: power loss or still recording),
  // bad (broken lines in between as well)
  const char* end = !sm.bad ? "clean" : (sm.bad == 1 && lastBad) ? "truncated" : "bad";
  char b[128];
  snprintf(b, sizeof(b), ",\"ok\":true,\"unix\":%lu,\"t0\":%lld,\"t1\":%lld,\"end\":\"%s\",\"bad\":%lu,\"gap\":%llu",
           static_cast<unsigned long>(sm.unixStart), static_cast<long long>(sm.t0), static_cast<long long>(sm.t1),
           end, static_cast<unsigned long>(sm.bad), static_cast<unsigned long long>(sm.gapBytes));
  o += b;
  o += ",\"types\":[";
  bool first = true;
  for (uint8_t t = 1; t < calc::rawcsv::K_N; t++) {
    if (!sm.n[t]) continue;
    snprintf(b, sizeof(b), "%s{\"t\":\"%s\",\"n\":%lu,\"b\":%llu}", first ? "" : ",", calc::rawcsv::kindKey(static_cast<calc::rawcsv::Kind>(t)),
             static_cast<unsigned long>(sm.n[t]), static_cast<unsigned long long>(sm.bytes[t]));
    o += b; first = false;
  }
  o += "],\"edges\":[";
  for (uint8_t c = 0; c < 4; c++) {
    snprintf(b, sizeof(b), "%s[%lu,%lu]", c ? "," : "", static_cast<unsigned long>(sm.edges[c]), static_cast<unsigned long>(sm.rejected[c]));
    o += b;
  }
  o += "],\"textN\":" + String(static_cast<unsigned long>(txtN)) + ",\"text\":[";
  const uint32_t shown = txtN < TXT_FIRST + TXT_LAST ? txtN : TXT_FIRST + TXT_LAST;
  for (uint32_t i = 0; i < shown; i++) {
    // order: the first ones, then the ring of the last ones from its oldest entry
    uint32_t slot = i;
    if (i >= TXT_FIRST && txtN > TXT_FIRST + TXT_LAST) slot = TXT_FIRST + (txtN - TXT_LAST - TXT_FIRST + (i - TXT_FIRST)) % TXT_LAST;
    snprintf(b, sizeof(b), "%s[%lld,", i ? "," : "", static_cast<long long>(txtT[slot]));
    o += b;
    appendJsonString(o, String(txt[slot]));
    o += "]";
  }
  o += "]}";
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", o);
}

void sendDirZip(fs::FS& f, const String& dirPath) {
  File dir = f.open(dirPath);
  if (!dir || !dir.isDirectory()) { g_server.send(404, "text/plain", "not found"); return; }
  const String base = dirPath.substring(dirPath.lastIndexOf('/') + 1);
  g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + base + ".zip\"");
  g_server.send(200, "application/zip", "");
  static ZipOut z;
  z = ZipOut();
  static ZipEntry e[32];
  size_t ne = 0;
  static uint8_t b[1024];
  for (File x = dir.openNextFile(); x && ne < 32; x = dir.openNextFile()) {
    if (x.isDirectory()) continue;
    String fn = String(x.name()); fn = fn.substring(fn.lastIndexOf('/') + 1);
    if (fn.startsWith(".")) continue;
    zipBegin(z, e[ne], (base + "/" + fn).c_str());
    for (size_t k; (k = x.read(b, sizeof(b))) > 0;) z.data(b, k);
    zipEnd(z, e[ne++]);
  }
  zipFinish(z, e, ne);
}

void handleFs() {
  g_other++;
  // /fs/<medium><path>
  const String uri = g_server.uri();
  const int slash = uri.indexOf('/', 4);
  storage::Media m;
  if (slash < 0 || !fsMedia(uri.substring(4, slash), m)) { g_server.send(404, "text/plain", "not found"); return; }
  String path = uri.substring(slash);
  while (path.length() > 1 && path.endsWith("/")) path.remove(path.length() - 1);
  if (!pathOk(path)) { g_server.send(404, "text/plain", "not found"); return; }
  if (storage::info(m).formatting) { g_server.send(503, "text/plain", "formatting"); return; }
  if (g_server.hasArg("info")) { sendRawInfo(m, path); return; }
  const bool zip = g_server.hasArg("zip"), csv = g_server.hasArg("csv");
  if (!zip && !csv) { sendPlain(m, path, g_server.hasArg("view")); return; }
  if ((rawlog::active() && rawlog::media() == m) || (m == storage::SD_CARD && run::busy())) {
    g_server.send(409, "text/plain", "Not while a run or raw log is active"); return;
  }
  SdLock lk;
  fs::FS* f = storage::fs(m);
  if (!f || !f->exists(path)) { g_server.send(404, "text/plain", "not found"); return; }
  if (zip) sendDirZip(*f, path);
  else sendRawZip(*f, path);
}

// Accept a command -- but only if it is fresh.
//
// The UI appends to every command the timestamp it last saw from
// /api/state. A browser retry carries an old one and is thrown out
// because of it. See CMD_MAX_AGE_MS in config.h -- this is not
// theoretical, it happened on 2026-11-18 on the running stand.
void handleCmd() {
  touch();
  g_cmds++;
  const String c = g_server.arg("c");
  const bool stopping = c.length() && (c[0] == 's' || c[0] == 'c');

  if (!stopping) {
    const String t = g_server.arg("t");
    const uint32_t ageMs = t.length()
        ? (millis() - static_cast<uint32_t>(strtoul(t.c_str(), nullptr, 10)))
        : 0xFFFFFFFFu;
    if (ageMs > CMD_MAX_AGE_MS) {
      Serial.print("# CMD '"); Serial.print(c);
      Serial.print("' DISCARDED, ");
      if (t.length()) { Serial.print(ageMs); Serial.println(" ms old"); }
      else            Serial.println("without timestamp");
      g_server.sendHeader("Cache-Control", "no-store");
      g_server.send(409, "text/plain", "stale");
      return;
    }
  }

  Serial.print("# CMD '"); Serial.print(c); Serial.println("'");
  // Do not execute here -- queue it. The main loop picks it up in service().
  bool queued = false;
  if (c.length() && g_cmdQueue) {
    CmdMsg m;
    strncpy(m.text, c.c_str(), sizeof(m.text) - 1);
    m.text[sizeof(m.text) - 1] = '\0';
    queued = xQueueSend(g_cmdQueue, &m, 0) == pdTRUE;
  }
  g_server.sendHeader("Cache-Control", "no-store");
  if (queued || c.length() == 0) g_server.send(200, "text/plain", "ok");
  else                           g_server.send(503, "text/plain", "queue full");
}

// The network task: DNS, web server, summary. May block.
void webTask(void*) {
  for (;;) {
    g_dns.processNextRequest();
    g_server.handleClient();
    summary();
    vTaskDelay(1);   // one tick, so that WLAN and idle task on core 0 get their turn
  }
}

// Every unknown address shows the UI. The DNS answers everything with its own
// address anyway -- whatever someone types, they want to come here.
void handleNotFound() {
  if (g_server.uri().startsWith("/fs/")) { handleFs(); return; }
  g_other++;
  if (redirectedToName()) return;
  sendIndex();
}

// Connectivity check. In the portal phase redirect it (the phone opens the page
// by itself), afterwards answer it (the network counts as fine and stays up).
// The connectivity check gets exactly what it expects. Then the network counts as
// fine, no window pops up, and -- the actual point -- the phone keeps the
// connection instead of discarding it after a short time.
// The length must match to the byte, otherwise iOS keeps checking endlessly.
void probe(const char* body, int code, const char* type) {
  g_other++;
  logReq("check -> ok");
  g_server.send(code, type, body);
}
// The checkers expect exactly these strings; a wrong length makes iOS
// keep checking endlessly (tzapu/WiFiManager #868).
void handleProbeApple() { probe(
  "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>\n", 200, "text/html"); }
void handleProbe204()   { probe("", 204, "text/plain"); }
void handleProbeMsft()  { probe("Microsoft Connect Test", 200, "text/plain"); }

}  // namespace

void begin(CommandFn handler, StateJsonFn state, StateJsonFn runJson,
           const uint8_t* htmlGz, size_t htmlGzLen) {
  g_cmd     = handler;
  g_state   = state;
  g_runJson = runJson;
  g_html    = htmlGz;
  g_htmlLen = htmlGzLen;

  WiFi.persistent(false);
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  const char* pw = (sizeof(WEBUI_PASSWORD) > 8) ? WEBUI_PASSWORD : nullptr;  // <8 → open
  const bool ok = WiFi.softAP(WEBUI_SSID, pw, WEBUI_CHANNEL, 0, WEBUI_MAX_CLIENTS);
  Serial.print("# softAP(\""); Serial.print(WEBUI_SSID);
  Serial.print("\", channel "); Serial.print(WEBUI_CHANNEL);
  Serial.print(pw ? ", WPA2" : ", OPEN");
  Serial.print(") = "); Serial.print(ok ? "ok" : "FAILED");
  Serial.print("  IP "); Serial.print(WiFi.softAPIP());
  Serial.print("  MAC "); Serial.println(WiFi.softAPmacAddress());

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", AP_IP);            // every query to ourselves

  if (MDNS.begin(WEBUI_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEBUI_PORT);
    Serial.print("# mDNS: http://"); Serial.print(WEBUI_HOSTNAME); Serial.println(".local/");
  }

  // "/" is decided at run time: in the portal window the hand-over attempt,
  // otherwise the UI right away. "/ui" is always the UI.
  g_server.on("/", []() { if (!redirectedToName()) sendIndex(); });
  g_server.on("/ui", sendIndex);   // always the UI, never redirected
  g_server.on("/api/state", handleState);
  // POST only. A GET repeated by the browser thus ends up in onNotFound and
  // shows the UI -- instead of starting the drive.
  g_server.on("/api/cmd", HTTP_POST, handleCmd);
  g_server.on("/api/run", handleRun);
  g_server.on("/api/profiles", handleProfiles);
  g_server.on("/api/files", handleFiles);

  g_server.on("/hotspot-detect.html", handleProbeApple);     // iOS, macOS
  g_server.on("/library/test/success.html", handleProbeApple);
  g_server.on("/generate_204", handleProbe204);              // Android
  g_server.on("/gen_204", handleProbe204);
  g_server.on("/connecttest.txt", handleProbeMsft);          // Windows
  g_server.on("/ncsi.txt", handleProbeMsft);

  const char* collect[] = {"User-Agent", "Host"};
  g_server.collectHeaders(collect, 2);
  g_server.onNotFound(handleNotFound);
  g_server.begin();
  Serial.print("# Control panel: http://"); Serial.print(AP_IP);
  Serial.println("/  — no portal, network counts as fine immediately");

  // Network into its own task (webui.h). Core 0 is the WLAN core; the
  // main loop runs on core 1 and stays free of network waits.
  g_cmdQueue = xQueueCreate(8, sizeof(CmdMsg));
  const BaseType_t ok2 = xTaskCreatePinnedToCore(webTask, "webui", 12288, nullptr,
                                                 1, &g_task, 0);
  Serial.print("# Web server task on core 0: ");
  Serial.println(ok2 == pdPASS ? "running" : "START FAILED");
}

void service() {
  // Only fetch commands; the network is served by its own task.
  if (!g_cmdQueue) return;
  CmdMsg m;
  while (xQueueReceive(g_cmdQueue, &m, 0) == pdTRUE) {
    g_inWebCmd = true;
    if (g_cmd) g_cmd(String(m.text));
    g_inWebCmd = false;
  }
}

bool linkAlive() {
  if (!g_seen) return true;             // without an operator device the console rules
  if (!g_webOperator) return true;      // console is driving: the phone does not matter
  return (millis() - g_lastSeen) < LINK_TIMEOUT_MS;
}

bool inWebCommand() { return g_inWebCmd; }
void setWebOperator(bool web) { g_webOperator = web; }
bool webOperator() { return g_webOperator; }

bool clientSeen() { return g_seen; }
uint32_t msSinceClient() { return g_seen ? millis() - g_lastSeen : 0; }

}  // namespace webui
