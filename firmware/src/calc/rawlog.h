#pragma once
// RAW DATA FORMAT ("vmrr-raw" version 3, since 2026-11-27): encoding, line parsing, split
// into per-type CSVs, summary, metadata JSON and legend. Arduino-free. Host test in
// firmware/test/test_calc, counterpart for the computer: tools/rawlog.py.
// Description: docs/10-firmware.md §11.4.
//
// Builder 2026-11-27: one CSV stream for everything with the sensor type in front, no
// metadata header in it; the metadata as JSON separately, a legend as Markdown. Types are
// single letters ("D=drum, W=wheel, T=torque etc."); the edge channel is in the type.
//
//   raw_<session>.csv    one record per line, no header line:  type,t_us,fields...
//       S,0,1764000000,4294967040             sync, wall clock; first line, then every RAWLOG_SYNC_MS
//       T,1250,-81234                          torque load cell, raw counts
//       D,1310,0                               drum mark edge, echo flag (I index, W wheel, H motor Hall)
//       M,1400,-12345,678,-0.250,...           motor controller packet as received
//       C,1500,15.50,-0.0123,1,4               control: setpoint/duty/brake/phase on change
//       P,1600,4.250,-3.5,87                   tire pressure sensor; unknown = empty field
//       G,1700,512                             gap: ring was full, bytes dropped before this line
//       N,1800,"text"                          note, CSV-quoted
//   L is reserved for the axle load cells (planned), not yet written.
//   raw_<session>.json   metadata: version, stand, dimensions, calibration, columns
//   raw_<session>.md     legend (legend() below)
//
// TIME t_us on every line: microseconds since the start of recording (micros() of the
// ESP32 extended to 64 bit, so no overflow after 71 min). Edges from the ISRs arrive with
// the next round of the main loop and may lie a little BEFORE the line above them: sort
// by t_us if order matters.
//
// Raw values only, in the resolution they were recorded (load cell: counts; controller
// fields as it sends them, fixed decimals without float formatting). Conversion happens in
// the evaluation (CLAUDE.md: derived quantities never into the raw file).
// A line cut off by a power failure is the only damage: every line before it is valid.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace calc {
namespace rawcsv {

constexpr const char* FORMAT = "vmrr-raw";
constexpr int         VERSION = 3;
constexpr size_t      REC_MAX = 320;      // longest record line incl. \n (a note is cut to fit)
constexpr size_t      HEADER_MAX = 5120;  // the metadata JSON (about 3.4 kB in the host test, up to ~4 kB on the stand)
constexpr size_t      LEGEND_MAX = 3072;  // the legend Markdown
constexpr size_t      COLS_MAX = 12;      // type + t_us + 10 fields of vesc

enum Kind : uint8_t { K_NONE = 0, K_SYNC, K_DRUM, K_INDEX, K_WHEEL, K_MOTOR, K_HX, K_VESC, K_CTRL,
                     K_TIRE, K_GAP, K_NOTE, K_N };
// Letter in the file.
inline const char* kindKey(Kind k) {
  static const char* const K[K_N] = {"", "S", "D", "I", "W", "H", "T", "M", "C", "P", "G", "N"};
  return k < K_N ? K[k] : "";
}
// Short name (JSON "types", events.csv).
inline const char* kindName(Kind k) {
  static const char* const K[K_N] = {"", "sync", "drum", "index", "wheel", "motor_hall", "torque", "vesc",
                                     "control", "tire", "gap", "note"};
  return k < K_N ? K[k] : "";
}
inline Kind kindOf(const char* k, size_t n) {
  for (uint8_t i = 1; i < K_N; i++) {
    const char* s = kindKey(static_cast<Kind>(i));
    if (std::strlen(s) == n && std::memcmp(s, k, n) == 0) return static_cast<Kind>(i);
  }
  return K_NONE;
}
// Reserved letters: announced in the legend and the JSON, never written yet.
constexpr const char* RESERVED_KEY = "L";
constexpr const char* RESERVED_TEXT = "axle load cells (planned): reserved, not yet written";

// Edge channels (speed.h order) = the four edge types. Bit 7 of the raw channel byte =
// rejected as echo. The names are the channel column of the split edges.csv.
constexpr const char* EDGE_NAMES[4] = {"drum", "index", "wheel", "motor"};
inline bool isEdge(Kind k) { return k >= K_DRUM && k <= K_MOTOR; }

// ------------------------------------------------------------------ Columns
// The fields after type and t_us, in file order, with unit. One table for the split CSV
// headers, the "columns" of the JSON and the legend.
struct Col { const char* name; const char* unit; };
inline const Col* columns(Kind k, size_t* n) {
  static const Col sync[] = {{"unix_s", "s, local wall clock"}, {"micros", "us, micros() of the ESP32"}};
  static const Col edge[] = {{"echo", "1 = rejected as echo"}};
  static const Col hx[]   = {{"raw_counts", "HX711 counts"}};
  static const Col vesc[] = {{"erpm", "1/min electrical"}, {"tacho", "steps"}, {"duty", "0..1"},
                             {"i_motor_A", "A"}, {"i_in_A", "A"}, {"v_in_V", "V"}, {"t_fet_C", "degC"},
                             {"t_motor_C", "degC"}, {"fault", "code"}, {"status", "code"}};
  static const Col ctrl[] = {{"target_kmh", "km/h"}, {"duty_cmd", "0..1"}, {"brake", "1 = relays dropped out"},
                             {"phase", "index into ctrl_phases"}};
  static const Col tire[] = {{"p_tire_bar", "bar"}, {"t_tire_C", "degC"}, {"battery_pct", "%"}};
  static const Col gap[]  = {{"dropped_bytes", "B"}};
  static const Col note[] = {{"text", "CSV-quoted"}};
  if (isEdge(k)) { *n = 1; return edge; }
  switch (k) {
    case K_SYNC: *n = 2; return sync;  case K_HX: *n = 1; return hx;
    case K_VESC: *n = 10; return vesc; case K_CTRL: *n = 4; return ctrl;  case K_TIRE: *n = 3; return tire;
    case K_GAP: *n = 1; return gap;    case K_NOTE: *n = 1; return note;
    default: *n = 0; return nullptr;
  }
}
inline const char* kindText(Kind k) {
  switch (k) {
    case K_SYNC:  return "sync: time anchor, first line, then every sync_ms";
    case K_DRUM:  return "drum mark edge (marks_per_rev per drum revolution)";
    case K_INDEX: return "drum index edge (one per revolution)";
    case K_WHEEL: return "test wheel edge (wheel_marks_per_rev per revolution)";
    case K_MOTOR: return "motor Hall edge (motor_pole_pairs per motor revolution)";
    case K_HX:    return "torque load cell, HX711 raw";
    case K_VESC:  return "motor controller (VESC) packet as received";
    case K_CTRL:  return "control: speed controller, on change and with every sync";
    case K_TIRE:  return "tire pressure sensor, empty = unknown";
    case K_GAP:   return "gap: ring buffer was full, bytes of records dropped before this line";
    case K_NOTE:  return "note: message of the firmware";
    default:      return "";
  }
}

// ------------------------------------------------------------------ Encoding helpers
// Fixed point without float formatting: v / 10^dec, exactly as recorded.
inline int fix(char* o, size_t n, long v, int dec) {
  long d = 1; for (int i = 0; i < dec; i++) d *= 10;
  const bool neg = v < 0; const unsigned long a = neg ? 0ul - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
  if (dec == 0) return std::snprintf(o, n, "%s%lu", neg ? "-" : "", a);
  return std::snprintf(o, n, "%s%lu.%0*lu", neg ? "-" : "", a / d, dec, a % d);
}

// Quoted string, `q` doubled (CSV) or escaped with a backslash (JSON, q = 0); control
// characters become spaces. Leaves two bytes of room for "\n" + NUL. Returns the length,
// 0 if not even the quotes fit.
inline size_t quote(char* o, size_t n, const char* s, bool csv, size_t maxIn = 255) {
  if (n < 5) return 0;
  size_t k = 0; o[k++] = '"';
  for (size_t i = 0; s[i] && i < maxIn && k + 5 < n; i++) {
    const char c = s[i];
    if (c == '"') { o[k++] = csv ? '"' : '\\'; o[k++] = c; }
    else if (c == '\\' && !csv) { o[k++] = '\\'; o[k++] = c; }
    else o[k++] = (static_cast<unsigned char>(c) < 0x20) ? ' ' : c;
  }
  o[k++] = '"'; o[k] = '\0';
  return k;
}
inline size_t jstr(char* o, size_t n, const char* s) { return quote(o, n, s, false); }

// Builds one line or document. All appends are bounded; the result is 0 if the buffer
// was too small (nothing half-written is ever used).
struct Line {
  char* o; size_t cap; size_t n = 0; bool good = true;
  Line(char* out, size_t c) : o(out), cap(c) { if (cap) o[0] = '\0'; }
  void add(int w) { if (w < 0 || n + static_cast<size_t>(w) >= cap) { good = false; n = cap ? cap - 1 : 0; } else n += w; }
  void s(const char* t) { add(std::snprintf(o + n, cap - n, "%s", t)); }
  void i(long long v) { add(std::snprintf(o + n, cap - n, "%lld", v)); }
  void u(unsigned long long v) { add(std::snprintf(o + n, cap - n, "%llu", v)); }
  void f(long v, int dec) { add(fix(o + n, cap - n, v, dec)); }
  void q(const char* t, bool csv) { const size_t k = quote(o + n, cap - n, t ? t : "", csv); if (!k) good = false; else n += k; }
  void str(const char* t) { q(t, false); }
  void strOrNull(const char* t) { if (t && t[0]) str(t); else s("null"); }
  // Float with `sig` significant digits; NaN/inf become null (JSON has no NaN).
  void fl(double v, int sig) { if (!std::isfinite(v)) s("null"); else add(std::snprintf(o + n, cap - n, "%.*g", sig, v)); }
  void key(const char* k) { s(n && o[n - 1] != '{' ? ",\"" : "\""); s(k); s("\":"); }
  void keyTop(const char* k) { s(n && o[n - 1] != '{' ? ",\n  \"" : "\n  \""); s(k); s("\": "); }
  size_t end() { s("\n"); return good ? n : 0; }
};

// ------------------------------------------------------------------ Encoder
// Holds the time reference: micros() of the start and a 64-bit extension.
struct Encoder {
  uint32_t lastUs = 0;
  int64_t  lastT = 0;

  void begin(uint32_t us) { lastUs = us; lastT = 0; }
  int64_t t(uint32_t us) {
    const int32_t d = static_cast<int32_t>(us - lastUs);     // edges may be a little in the past
    const int64_t v = lastT + d;
    if (d > 0) { lastUs = us; lastT = v; }
    return v;
  }
  Line head(char* o, size_t n, uint32_t us, Kind k) {
    Line l(o, n); l.s(kindKey(k)); l.s(","); l.i(t(us)); return l;
  }

  size_t sync(char* o, size_t n, uint32_t us, uint32_t unixS) {
    Line l = head(o, n, us, K_SYNC); l.s(","); l.u(unixS); l.s(","); l.u(us); return l.end();
  }
  // ch = speed.h channel 0..3, bit 7 = echo. Another channel is not written (returns 0).
  size_t edge(char* o, size_t n, uint32_t us, uint8_t ch) {
    const uint8_t c = ch & 0x7F;
    if (c >= 4) return 0;
    Line l = head(o, n, us, static_cast<Kind>(K_DRUM + c));
    l.s(","); l.u((ch >> 7) & 1);
    return l.end();
  }
  size_t hx711(char* o, size_t n, uint32_t us, int32_t raw) {
    Line l = head(o, n, us, K_HX); l.s(","); l.i(raw); return l.end();
  }
  size_t vesc(char* o, size_t n, uint32_t us, int32_t erpm, int32_t tacho, int16_t duty1000,
              int32_t iMot100, int32_t iIn100, uint16_t vIn10, int16_t tFet10, int16_t tMot10,
              uint8_t fault, uint8_t status) {
    Line l = head(o, n, us, K_VESC);
    l.s(","); l.i(erpm); l.s(","); l.i(tacho); l.s(","); l.f(duty1000, 3);
    l.s(","); l.f(iMot100, 2); l.s(","); l.f(iIn100, 2); l.s(","); l.f(vIn10, 1);
    l.s(","); l.f(tFet10, 1); l.s(","); l.f(tMot10, 1);
    l.s(","); l.u(fault); l.s(","); l.u(status);
    return l.end();
  }
  size_t ctrl(char* o, size_t n, uint32_t us, int16_t target100, int16_t duty10000, uint8_t brake, uint8_t phase) {
    Line l = head(o, n, us, K_CTRL);
    l.s(","); l.f(target100, 2); l.s(","); l.f(duty10000, 4); l.s(","); l.u(brake); l.s(","); l.u(phase);
    return l.end();
  }
  // mbar 0xFFFF, temp INT16_MIN, battery 0xFF = unknown -> empty field
  size_t tire(char* o, size_t n, uint32_t us, uint16_t mbar, int16_t temp10, uint8_t batt) {
    Line l = head(o, n, us, K_TIRE);
    l.s(","); if (mbar != 0xFFFF) l.f(mbar, 3);
    l.s(","); if (temp10 != INT16_MIN) l.f(temp10, 1);
    l.s(","); if (batt != 0xFF) l.u(batt);
    return l.end();
  }
  size_t gap(char* o, size_t n, uint32_t us, uint32_t bytes) {
    Line l = head(o, n, us, K_GAP); l.s(","); l.u(bytes); return l.end();
  }
  size_t note(char* o, size_t n, uint32_t us, const char* s) {
    Line l = head(o, n, us, K_NOTE); l.s(","); l.q(s, true); return l.end();
  }
};

// ------------------------------------------------------------------ Parsing
// CSV fields of one line; a quoted field is returned WITH its quotes (raw, as in the file).
struct Fields {
  const char* p; const char* e; bool done = false; bool bad = false;
  Fields(const char* s, size_t n) : p(s), e(s + n) {}
  bool next(const char** v, size_t* vn, bool* quoted) {
    if (done) return false;
    const char* s = p;
    if (p < e && *p == '"') {
      p++;
      for (;;) {
        if (p >= e) { bad = true; break; }                  // quote never closed
        if (*p == '"') { if (p + 1 < e && p[1] == '"') { p += 2; continue; } p++; break; }
        p++;
      }
      *quoted = true;
      if (p < e && *p != ',') bad = true;                  // junk behind the closing quote
    } else {
      while (p < e && *p != ',') p++;
      *quoted = false;
    }
    *v = s; *vn = static_cast<size_t>(p - s);
    if (p < e && *p == ',') p++; else done = true;
    return true;
  }
};

struct Rec {
  Kind        kind = K_NONE;
  int64_t     t = 0;
  const char* body = nullptr;   // the fields after t_us (to the end of the line, without \n)
  size_t      bodyLen = 0;
};

inline long long parseInt(const char* s, size_t n, bool* ok) {
  long long v = 0; size_t i = 0; bool neg = false;
  if (i < n && s[i] == '-') { neg = true; i++; }
  if (i >= n) { *ok = false; return 0; }
  for (; i < n; i++) { if (s[i] < '0' || s[i] > '9') { *ok = false; return 0; } v = v * 10 + (s[i] - '0'); }
  *ok = true; return neg ? -v : v;
}

// One record line (without or with \n). false = not a valid record (unknown type, wrong
// field count, open quote). A line cut off at the end that still looks complete (hx with
// fewer digits) cannot be told apart here: the readers take the last line only with \n.
inline bool parseLine(const char* s, size_t n, Rec& r) {
  while (n && (s[n - 1] == '\n' || s[n - 1] == '\r')) n--;
  size_t a = 0; while (a < n && s[a] != ',') a++;
  if (a >= n) return false;
  r.kind = kindOf(s, a);
  if (r.kind == K_NONE) return false;
  size_t b = a + 1; while (b < n && s[b] != ',') b++;
  bool ok = false; r.t = parseInt(s + a + 1, b - a - 1, &ok);
  if (!ok || b >= n) return false;
  r.body = s + b + 1; r.bodyLen = n - b - 1;
  size_t want = 0; columns(r.kind, &want);
  Fields fs(r.body, r.bodyLen); const char* v; size_t vn; bool qd; size_t got = 0;
  while (fs.next(&v, &vn, &qd)) got++;
  return !fs.bad && got == want;
}

// Quoted CSV field (with its quotes) to plain text. Length.
inline size_t unquote(char* o, size_t cap, const char* v, size_t vn) {
  if (!cap) return 0;
  if (vn >= 2 && v[0] == '"') { v++; vn -= 2; }
  size_t k = 0;
  for (size_t i = 0; i < vn && k + 1 < cap; i++) {
    if (v[i] == '"' && i + 1 < vn && v[i + 1] == '"') i++;
    o[k++] = v[i];
  }
  o[k] = '\0';
  return k;
}

// ------------------------------------------------------------------ Split into per-type CSVs
// One CSV per type with a column header row; sync, gap and note share events.csv
// (t_us,event,value; sync only with unix_s).
inline const char* csvName(Kind k) {
  switch (k) {
    case K_DRUM: case K_INDEX: case K_WHEEL: case K_MOTOR: return "edges.csv";
    case K_HX: return "loadcell.csv"; case K_VESC: return "vesc.csv";
    case K_CTRL: return "control.csv"; case K_TIRE: return "tire.csv";
    case K_GAP: case K_NOTE: case K_SYNC: return "events.csv";
    default: return "";
  }
}
inline bool isEvent(Kind k) { return k == K_SYNC || k == K_GAP || k == K_NOTE; }
// Column header row (with \n) of the file for `file` (K_NOTE = events.csv). Length.
inline size_t csvHeader(char* o, size_t cap, Kind file) {
  Line l(o, cap);
  if (isEvent(file)) { l.s("t_us,event,value\n"); return l.good ? l.n : 0; }
  if (isEdge(file)) { l.s("t_us,channel,echo\n"); return l.good ? l.n : 0; }
  size_t nc = 0; const Col* c = columns(file, &nc);
  l.s("t_us");
  for (size_t i = 0; i < nc; i++) { l.s(","); l.s(c[i].name); }
  return l.end();
}
inline bool csvBelongs(Kind file, Kind rec) {
  if (isEvent(file)) return isEvent(rec);
  if (isEdge(file)) return isEdge(rec);
  return file == rec;
}
constexpr Kind CSV_FILES[] = {K_HX, K_DRUM, K_VESC, K_CTRL, K_TIRE, K_NOTE};   // K_DRUM = edges.csv
constexpr size_t CSV_FILE_N = sizeof(CSV_FILES) / sizeof(CSV_FILES[0]);

// Row (with \n) of the per-type CSV for a parsed record: the line without its type; for
// edges t_us,channel name,echo; for events t_us,type name,first field. Length, 0 = did not fit.
inline size_t csvLine(char* o, size_t n, const Rec& r) {
  Line l(o, n);
  l.i(r.t); l.s(",");
  if (isEdge(r.kind)) { l.s(EDGE_NAMES[r.kind - K_DRUM]); l.s(","); }
  if (isEvent(r.kind)) {
    l.s(kindName(r.kind)); l.s(",");
    Fields fs(r.body, r.bodyLen); const char* v; size_t vn = 0; bool qd;
    if (!fs.next(&v, &vn, &qd)) vn = 0;
    if (l.n + vn + 2 >= l.cap) return 0;
    std::memcpy(o + l.n, v, vn); l.n += vn; o[l.n] = '\0';
  } else {
    if (l.n + r.bodyLen + 2 >= l.cap) return 0;
    std::memcpy(o + l.n, r.body, r.bodyLen); l.n += r.bodyLen; o[l.n] = '\0';
  }
  return l.end();
}

// ------------------------------------------------------------------ Summary
// Overview for the viewer (webui.cpp /fs/...?info): lines and bytes per type, time span,
// edges per channel (and how many were rejected as echo), dropped bytes, wall clock at start.
struct Summary {
  uint32_t n[K_N] = {};
  uint64_t bytes[K_N] = {};
  int64_t  t0 = 0, t1 = 0;
  bool     any = false;
  uint32_t edges[4] = {}, rejected[4] = {};
  uint64_t gapBytes = 0;
  uint32_t unixStart = 0;
  uint32_t bad = 0;               // lines that are not records (cut off, foreign); counted by the caller
  void add(const Rec& r, size_t lineBytes) {
    n[r.kind]++; bytes[r.kind] += lineBytes;
    if (!any) { t0 = t1 = r.t; any = true; }
    if (r.t < t0) t0 = r.t;
    if (r.t > t1) t1 = r.t;
    Fields fs(r.body, r.bodyLen); const char* v; size_t vn; bool qd; bool ok;
    if (isEdge(r.kind)) {
      const int ch = r.kind - K_DRUM;
      edges[ch]++;
      if (fs.next(&v, &vn, &qd) && vn == 1 && v[0] == '1') rejected[ch]++;
    }
    if (r.kind == K_GAP && fs.next(&v, &vn, &qd)) { const long long g = parseInt(v, vn, &ok); if (ok) gapBytes += g; }
    if (r.kind == K_SYNC && !unixStart && fs.next(&v, &vn, &qd)) { const long long u = parseInt(v, vn, &ok); if (ok) unixStart = static_cast<uint32_t>(u); }
  }
};

// ------------------------------------------------------------------ Metadata (.json)
// Everything needed to evaluate the stream on its own: version, stand, dimensions,
// calibration, columns. Filled by rawlog.cpp from config.h / settings; the host test
// checks it is JSON. Top-level keys one per line.
struct CalRec {               // settings::CalRecord as stored; present = false when ver 0
  bool present = false;
  unsigned ver = 0, gainPulses = 0, sps = 0;
  uint32_t epoch = 0;
  float npc = NAN, armM = NAN, lcalM = NAN, spsMeas = NAN, linPctFS = NAN, hystPctFS = NAN, zeroPctFS = NAN;
  unsigned nLoads = 0, nPoints = 0;
  const char* method = "";
};
struct HeaderInfo {
  const char* csvFile = nullptr; const char* legendFile = nullptr;   // nullptr: written by headerFiles()
  const char* firmware = ""; const char* commit = ""; const char* standRev = "";
  const char* startLocal = ""; uint32_t unixStartLocal = 0;
  const char* runId = "";                                  // "" -> null
  unsigned marksPerRev = 0, wheelMarksPerRev = 0, motorPolePairs = 0;
  float drumCircM = NAN, torqueArmM = NAN, calibArmM = NAN, wheelCircM = NAN;
  int directionSign = 0;
  unsigned vescPollMs = 0, hx711RateSps = 0, hx711GainPulses = 0, syncMs = 0;
  float loadcellCapacityN = NAN, loadEmptyKg = NAN, loadWeightKg = NAN, loadArmM = NAN;
  float nPerCount = NAN; long tareOffset = 0;
  int validity = 0; const char* validityWhy = "";
  CalRec cal;
  const char* const* phases = nullptr; unsigned phaseN = 0;   // state::Phase names
};

// `{` and the file keys. The storage task writes this, then "," and header() without its `{`:
// the file name (collision suffix) is known only there.
inline void headerFilesInto(Line& l, const char* csvFile, const char* legendFile) {
  l.s("{"); l.keyTop("file"); l.str(csvFile); l.keyTop("legend"); l.str(legendFile);
}
inline size_t headerFiles(char* o, size_t cap, const char* csvFile, const char* legendFile) {
  Line l(o, cap); headerFilesInto(l, csvFile, legendFile); return l.good ? l.n : 0;
}

inline size_t header(char* o, size_t cap, const HeaderInfo& h) {
  Line l(o, cap);
  if (h.csvFile) headerFilesInto(l, h.csvFile, h.legendFile ? h.legendFile : "");
  else l.s("{");
  l.keyTop("format"); l.str(FORMAT); l.keyTop("version"); l.i(VERSION);
  l.keyTop("firmware"); l.str(h.firmware); l.keyTop("commit"); l.str(h.commit);
  l.keyTop("stand_rev"); l.str(h.standRev);
  l.keyTop("start_local"); l.str(h.startLocal); l.keyTop("unix_start_local"); l.u(h.unixStartLocal);
  l.keyTop("run_id"); l.strOrNull(h.runId);
  l.keyTop("layout"); l.str("one CSV record per line, no header line: type letter,t_us,fields; the fields per type in 'columns'");
  l.keyTop("time"); l.str("t_us = microseconds since the start of recording (micros() of the ESP32, extended to "
                          "64 bit). Edges may lie slightly before the line above them: sort by t_us. sync maps "
                          "t_us to local unix time.");
  l.keyTop("columns"); l.s("{");
  for (uint8_t k = 1; k < K_N; k++) {
    size_t nc = 0; const Col* c = columns(static_cast<Kind>(k), &nc);
    l.key(kindKey(static_cast<Kind>(k))); l.s("[[\"t_us\",\"us\"]");
    for (size_t i = 0; i < nc; i++) { l.s(",["); l.str(c[i].name); l.s(","); l.str(c[i].unit); l.s("]"); }
    l.s("]");
  }
  l.s("}");
  l.keyTop("types"); l.s("{");
  for (uint8_t k = 1; k < K_N; k++) {
    l.key(kindKey(static_cast<Kind>(k))); l.s("{"); l.key("name"); l.str(kindName(static_cast<Kind>(k)));
    l.key("description"); l.str(kindText(static_cast<Kind>(k))); l.s("}");
  }
  l.key(RESERVED_KEY); l.s("{"); l.key("name"); l.str("axle_load"); l.key("description"); l.str(RESERVED_TEXT); l.s("}");
  l.s("}");
  l.keyTop("echo"); l.str("edges D/I/W/H: echo 1 = rejected as echo/double pulse by the firmware, kept in the file");
  l.keyTop("ctrl_phases"); l.s("[");
  for (unsigned i = 0; i < h.phaseN; i++) { if (i) l.s(","); l.str(h.phases[i]); }
  l.s("]");
  l.keyTop("setup"); l.s("{");
  l.key("marks_per_rev"); l.u(h.marksPerRev); l.key("wheel_marks_per_rev"); l.u(h.wheelMarksPerRev);
  l.key("motor_pole_pairs"); l.u(h.motorPolePairs);
  l.key("drum_circumference_m"); l.fl(h.drumCircM, 7); l.key("torque_arm_m"); l.fl(h.torqueArmM, 7);
  l.key("calib_arm_m"); l.fl(h.calibArmM, 7);
  l.key("direction_sign"); l.i(h.directionSign); l.key("vesc_poll_ms"); l.u(h.vescPollMs);
  l.key("hx711_rate_sps"); l.u(h.hx711RateSps); l.key("hx711_gain_pulses"); l.u(h.hx711GainPulses);
  l.key("loadcell_capacity_n"); l.fl(h.loadcellCapacityN, 7);
  l.key("load_empty_kg"); l.fl(h.loadEmptyKg, 7); l.key("load_weight_kg"); l.fl(h.loadWeightKg, 7);
  l.key("load_arm_m"); l.fl(h.loadArmM, 7); l.key("wheel_circumference_m"); l.fl(h.wheelCircM, 7);
  l.key("sync_ms"); l.u(h.syncMs);
  l.s("}");
  l.keyTop("calibration"); l.s("{");
  l.key("loadcell_n_per_count"); l.fl(h.nPerCount, 7); l.key("tare_offset_counts"); l.i(h.tareOffset);
  l.key("validity"); l.i(h.validity); l.key("validity_text"); l.str(h.validityWhy);
  l.key("record");
  if (!h.cal.present) l.s("null");
  else {
    const CalRec& c = h.cal;
    l.s("{"); l.key("ver"); l.u(c.ver); l.key("gain_pulses"); l.u(c.gainPulses); l.key("sps"); l.u(c.sps);
    l.key("epoch_local"); l.u(c.epoch); l.key("npc"); l.fl(c.npc, 7); l.key("arm_m"); l.fl(c.armM, 7);
    l.key("lcal_m"); l.fl(c.lcalM, 7); l.key("sps_meas"); l.fl(c.spsMeas, 6);
    l.key("lin_pct_fs"); l.fl(c.linPctFS, 4); l.key("hyst_pct_fs"); l.fl(c.hystPctFS, 4);
    l.key("zero_pct_fs"); l.fl(c.zeroPctFS, 4);
    l.key("n_loads"); l.u(c.nLoads); l.key("n_points"); l.u(c.nPoints); l.key("method"); l.str(c.method);
    l.s("}");
  }
  l.s("}\n}");
  return l.end();
}

// ------------------------------------------------------------------ Legend (.md)
// base = "raw_<session>" (with collision suffix), origin = how the recording started.
// The same text is in docs/10-firmware.md §11.4.
inline size_t legend(char* o, size_t cap, const char* base, const char* origin) {
  Line l(o, cap);
  l.s("# Raw log "); l.s(base); l.s("\n\n");
  l.s(origin); l.s("\n\n`"); l.s(base);
  l.s(".csv`: everything the sensors of the stand delivered, one record per line, raw values "
      "(no derived quantities), no header line. Version, stand, dimensions and calibration: `");
  l.s(base); l.s(".json`. Format: `docs/10-firmware.md` §11.4 of the stand repository.\n\n"
      "Every line is `type,t_us,fields`, the type a single letter. `t_us` = µs since the start of the recording (64 bit, "
      "does not wrap). Edges may come a little out of order: sort by `t_us` where order matters. "
      "An empty field is unknown. Text is CSV-quoted.\n\n"
      "| Type | Fields after t_us | Content |\n|---|---|---|\n");
  for (uint8_t k = 1; k < K_N; k++) {
    size_t nc = 0; const Col* c = columns(static_cast<Kind>(k), &nc);
    l.s("| `"); l.s(kindKey(static_cast<Kind>(k))); l.s("` | ");
    for (size_t i = 0; i < nc; i++) { if (i) l.s(", "); l.s(c[i].name); l.s(" ("); l.s(c[i].unit); l.s(")"); }
    l.s(" | "); l.s(kindText(static_cast<Kind>(k))); l.s(" |\n");
  }
  l.s("| `"); l.s(RESERVED_KEY); l.s("` | | "); l.s(RESERVED_TEXT); l.s(" |\n");
  l.s("\nOne type: `grep '^T,' "); l.s(base); l.s(".csv`\n\n```python\nimport pandas as pd\n"
      "d = pd.read_csv(\""); l.s(base); l.s(".csv\", header=None, names=range(12))\n"
      "t = d[d[0] == \"T\"][[1, 2]].astype(int)   # t_us, raw_counts\n```\n\n"
      "One CSV per type with column names: `python3 tools/rawlog.py "); l.s(base);
  l.s(".csv`, or *CSV .zip* in the file browser of the stand.");
  return l.end();
}

}  // namespace rawcsv
}  // namespace calc
