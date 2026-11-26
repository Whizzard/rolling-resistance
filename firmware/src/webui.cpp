#include "profiles.h"
#include "webui.h"
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
  // charset MUST be included: without it iOS reads the page as Latin-1 and "Prüflaufrad"
  // becomes "PrÃ¼flaufrad". The <meta charset> in the document comes too late when the
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
  char buf[4096];   // 2026-11-26: + change timestamps of the settings (about 450 bytes)
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

// Reports from the flash: /runs lists, /runs/<file> delivers for download,
// /runs/<file>?view shows it as text in the browser.
//
// Download means application/octet-stream: text/markdown with Content-Disposition
// was still shown in the window by Safari on the iPad (2026-11-24, iOS 12), and from
// the view the file cannot be passed on via AirDrop. An unknown type is offered
// via the share sheet; on the Mac it lands in the Downloads folder.
// /runs/<id>.zip: report and data of a run as one archive, without compression
// (calc/zip.h). Two passes per file: first CRC and size (Content-Length
// comes before the first byte), then the stream in 512-byte chunks.
// Since 2026-11-26 reports are on the SD or (without SD) in the flash, under
// the same path /runs/<id>.*. The SD is searched first.
fs::FS* runFs(const String& path) {
  fs::FS* sd = storage::fs(storage::SD_CARD);
  if (sd && sd->exists(path)) return sd;
  fs::FS* fl = storage::fs(storage::FLASH);
  if (fl && fl->exists(path)) return fl;
  return nullptr;
}

void sendRunZip(const String& base) {
  const char* EXT[] = {".md", ".csv"};
  String   path[2]; String name[2]; uint32_t crc[2] = {0, 0}; uint32_t size[2] = {0, 0}; size_t nlen[2] = {0, 0};
  size_t n = 0;
  uint8_t buf[512];
  for (const char* e : EXT) {
    const String p = base + e;
    fs::FS* fsp = runFs(p);
    if (!fsp) continue;
    File f = fsp->open(p, "r");
    if (!f) continue;
    uint32_t c = 0, sz = 0;
    while (f.available()) { const size_t k = f.read(buf, sizeof(buf)); c = calc::crc32(c, buf, k); sz += k; }
    f.close();
    path[n] = p; name[n] = p.substring(p.lastIndexOf('/') + 1); crc[n] = c; size[n] = sz; nlen[n] = name[n].length(); n++;
  }
  if (n == 0) { g_server.send(404, "text/plain", "not found"); return; }
  const String zipName = base.substring(base.lastIndexOf('/') + 1) + ".zip";
  g_server.setContentLength(calc::zipArchiveSize(size, nlen, n));
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + zipName + "\"");
  g_server.send(200, "application/zip", "");
  WiFiClient c = g_server.client();
  uint8_t hdr[calc::ZIP_CENTRAL_FIXED + 64];
  uint32_t offset[2] = {0, 0}, pos = 0;
  for (size_t i = 0; i < n; i++) {
    offset[i] = pos;
    const size_t h = calc::zipLocalHeader(hdr, name[i].c_str(), crc[i], size[i]);
    c.write(hdr, h); pos += h;
    fs::FS* fsp = runFs(path[i]);
    File f = fsp ? fsp->open(path[i], "r") : File();
    if (f) { while (f.available()) { const size_t k = f.read(buf, sizeof(buf)); c.write(buf, k); pos += k; } f.close(); }
  }
  const uint32_t cdStart = pos;
  for (size_t i = 0; i < n; i++) { const size_t h = calc::zipCentralHeader(hdr, name[i].c_str(), crc[i], size[i], offset[i]); c.write(hdr, h); pos += h; }
  const size_t e = calc::zipEndRecord(hdr, static_cast<uint16_t>(n), pos - cdStart, cdStart);
  c.write(hdr, e);
}

struct SdLock { SdLock() { storage::lockSd(); } ~SdLock() { storage::unlockSd(); } };

void handleRuns() {
  g_other++;
  SdLock lk;   // reports can be on the SD
  const String uri = g_server.uri();
  if (uri.startsWith("/runs/") && uri.endsWith(".zip") && uri.indexOf("..") < 0) { sendRunZip(uri.substring(0, uri.length() - 4)); return; }
  if (uri == "/runs" || uri == "/runs/") {
    String html = "<!doctype html><meta charset=utf-8><title>Runs</title>"
                  "<body style='font-family:system-ui;padding:16px'><h2>Runs</h2><ul>";
    for (uint8_t m = 0; m < storage::MEDIA_N; m++) {
      fs::FS* fsp = storage::fs(static_cast<storage::Media>(m));
      if (!fsp) continue;
      const char* where = m == storage::SD_CARD ? " (SD)" : " (Flash)";
      File dir = fsp->open("/runs");
      if (dir) { File f = dir.openNextFile(); while (f) {
          String fn = String(f.name()); fn = fn.substring(fn.lastIndexOf('/') + 1);
          html += "<li><a href='/runs/" + fn + "'>" + fn + "</a> " + String(f.size()) + " B" + where + " &middot; <a href='/runs/" + fn + "?view'>view</a>";
          if (fn.endsWith(".md")) html += " &middot; <a href='/runs/" + fn.substring(0, fn.length() - 3) + ".zip'>report + data as .zip</a>";
          html += "</li>";
          f = dir.openNextFile(); } }
    }
    html += "</ul>";
    g_server.send(200, "text/html; charset=utf-8", html);
    return;
  }
  fs::FS* fsp = uri.indexOf("..") >= 0 ? nullptr : runFs(uri);
  if (!fsp) { g_server.send(404, "text/plain", "not found"); return; }
  File f = fsp->open(uri, "r");
  if (!f) { g_server.send(404, "text/plain", "not found"); return; }
  const String name = uri.substring(uri.lastIndexOf('/') + 1);
  g_server.sendHeader("Cache-Control", "no-store");
  if (g_server.hasArg("view")) {
    g_server.streamFile(f, "text/plain; charset=utf-8");
  } else {
    g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    g_server.streamFile(f, "application/octet-stream");
  }
  f.close();
}

// ---------------------------------------------------------- Raw data (/raw)
//
//   /raw                          list of both media
//   /raw/<sd|flash>/<file>.rrl    the raw file as it is
//   /raw/<sd|flash>/<file>.zip    the same as CSV, one file per sensor, converted
//                                 while sending (calc/rawlog.h). Streamed ZIP with
//                                 data descriptor and HTTP chunked: one read pass
//                                 per CSV, no intermediate buffer, no Content-Length.
// If the file is not a readable .rrl, it goes into the archive unchanged.
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

// One CSV from the raw file: one pass, only the records of this type.
// Returns the number of rows; bad = file does not end cleanly (power loss,
// unknown record) -- everything before it is valid.
uint32_t csvPass(File& in, size_t hdrLen, calc::rrl::Type which, ZipOut& z, bool& bad) {
  static uint8_t ib[2048];
  static char line[400];
  size_t have = 0;
  uint32_t rows = 0;
  bad = false;
  in.seek(hdrLen);
  calc::rrl::Decoder d;
  z.data(calc::rrl::csvHeader(which));
  for (;;) {
    const size_t got = in.read(ib + have, sizeof(ib) - have);
    have += got;
    size_t off = 0;
    for (;;) {
      calc::rrl::Rec r; size_t used = 0;
      const calc::rrl::Status st = d.next(ib + off, have - off, r, &used);
      if (st == calc::rrl::NEED_MORE) break;
      if (st == calc::rrl::BAD) { bad = true; return rows; }
      off += used;
      if (!calc::rrl::csvBelongs(which, r.type)) continue;
      const size_t k = calc::rrl::csvLine(line, sizeof(line), r);
      if (k) { z.data(reinterpret_cast<const uint8_t*>(line), k); rows++; }
    }
    memmove(ib, ib + off, have - off);
    have -= off;
    if (got == 0) { if (have) bad = true; return rows; }   // remainder = truncated last record
  }
}

void sendRawZip(fs::FS& f, const String& path) {
  File in = f.open(path, "r");
  if (!in) { g_server.send(404, "text/plain", "not found"); return; }
  const String file = path.substring(path.lastIndexOf('/') + 1);
  const String base = file.substring(0, file.length() - 4);

  static uint8_t hb[calc::rrl::HEADER_FIXED + 1024];
  const size_t hn = in.read(hb, sizeof(hb));
  const char* text = nullptr; size_t textLen = 0;
  const size_t hdrLen = calc::rrl::parseHeader(hb, hn, &text, &textLen);

  g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + base + ".zip\"");
  g_server.send(200, "application/zip", "");

  static ZipOut z;
  z = ZipOut();
  ZipEntry e[calc::rrl::CSV_FILE_N + 2];
  size_t ne = 0;
  char name[64];

  if (!hdrLen) {   // not an .rrl: into the archive unchanged
    zipBegin(z, e[ne], file.c_str());
    in.seek(0);
    static uint8_t b[1024];
    for (size_t k; (k = in.read(b, sizeof(b))) > 0;) z.data(b, k);
    zipEnd(z, e[ne++]);
    in.close();
    zipFinish(z, e, ne);
    return;
  }

  uint32_t rows[calc::rrl::CSV_FILE_N];
  bool anyBad = false;
  for (size_t i = 0; i < calc::rrl::CSV_FILE_N; i++) {
    const calc::rrl::Type t = calc::rrl::CSV_FILES[i];
    snprintf(name, sizeof(name), "%s_%s", base.c_str(), calc::rrl::csvName(t));
    zipBegin(z, e[ne], name);
    bool bad = false;
    rows[i] = csvPass(in, hdrLen, t, z, bad);
    anyBad |= bad;
    zipEnd(z, e[ne++]);
  }
  // Finally the header of the raw file (constants for conversion) and the row counts.
  snprintf(name, sizeof(name), "%s_info.txt", base.c_str());
  zipBegin(z, e[ne], name);
  z.data(reinterpret_cast<const uint8_t*>(text), textLen);
  char l[96];
  snprintf(l, sizeof(l), "source_file=%s\nsource_bytes=%lu\n", file.c_str(), static_cast<unsigned long>(in.size()));
  z.data(l);
  for (size_t i = 0; i < calc::rrl::CSV_FILE_N; i++) {
    snprintf(l, sizeof(l), "rows_%s=%lu\n", calc::rrl::csvName(calc::rrl::CSV_FILES[i]), static_cast<unsigned long>(rows[i]));
    z.data(l);
  }
  z.data(anyBad ? "end=truncated (last record incomplete or unknown; everything before it is valid)\n" : "end=clean\n");
  zipEnd(z, e[ne++]);
  in.close();
  zipFinish(z, e, ne);
}

void listRaw(String& html, storage::Media m) {
  const storage::Info si = storage::info(m);
  html += "<h3>" + String(m == storage::SD_CARD ? "SD" : "Flash") + " &middot; " + storage::stateName(si.state);
  if (si.pct >= 0) html += " &middot; " + String(si.pct) + " %";
  html += "</h3><ul>";
  fs::FS* f = storage::fs(m);
  if (f) {
    File dir = f->open("/raw");
    if (dir) { File x = dir.openNextFile(); while (x) {
        String fn = String(x.name());
        fn = fn.substring(fn.lastIndexOf('/') + 1);
        const String url = "/raw/" + String(storage::name(m)) + "/" + fn;
        const bool writing = rawlog::active() && rawlog::media() == m && String(rawlog::fileName()).endsWith(fn);
        html += "<li>" + fn + " " + String(static_cast<uint32_t>(x.size() / 1024)) + " kB";
        if (writing) html += " &middot; <b>recording</b>";
        if (fn.endsWith(".rrl")) html += " &middot; <a href='" + url.substring(0, url.length() - 4) + ".zip'>CSV (.zip)</a>";
        html += " &middot; <a href='" + url + "'>raw</a></li>";
        x = dir.openNextFile(); } }
  }
  html += "</ul>";
}

void handleRaw() {
  g_other++;
  const String uri = g_server.uri();
  if (uri == "/raw" || uri == "/raw/") {
    String html = "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                  "<title>Raw data</title><body style='font-family:system-ui;padding:16px'><h2>Raw data</h2>";
    storage::lockSd(); listRaw(html, storage::SD_CARD); storage::unlockSd();
    listRaw(html, storage::FLASH);
    g_server.send(200, "text/html; charset=utf-8", html);
    return;
  }
  // /raw/<medium>/<file>
  const int slash = uri.indexOf('/', 5);
  if (slash < 0 || uri.indexOf("..") >= 0) { g_server.send(404, "text/plain", "not found"); return; }
  const String medium = uri.substring(5, slash);
  const storage::Media m = medium == "sd" ? storage::SD_CARD : medium == "flash" ? storage::FLASH : storage::MEDIA_N;
  if (m == storage::MEDIA_N) { g_server.send(404, "text/plain", "not found"); return; }
  const String name = uri.substring(slash + 1);
  const bool zip = name.endsWith(".zip");
  const String path = "/raw/" + (zip ? name.substring(0, name.length() - 4) + ".rrl" : name);

  if (m == storage::SD_CARD) storage::lockSd();
  fs::FS* f = storage::fs(m);
  if (!f || !f->exists(path)) g_server.send(404, "text/plain", "not found");
  else if (zip) sendRawZip(*f, path);
  else {
    File in = f->open(path, "r");
    g_server.sendHeader("Cache-Control", "no-store");
    g_server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    g_server.streamFile(in, "application/octet-stream");
    in.close();
  }
  if (m == storage::SD_CARD) storage::unlockSd();
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
  if (g_server.uri().startsWith("/runs/")) { handleRuns(); return; }
  if (g_server.uri().startsWith("/raw/")) { handleRaw(); return; }
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
  g_server.on("/runs", handleRuns);
  g_server.on("/raw", handleRaw);

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
