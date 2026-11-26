#pragma once
// RAW DATA FORMAT (.rrl), version 1 — encoding and decoding, Arduino-free.
// Host test in firmware/test/test_calc, counterpart for the computer: tools/rawlog.py.
// Description: docs/10-firmware.md §11.
//
// File:
//   header   "RRL1" | u16 text length | text (key=value per line, ASCII)
//   records  u8 type | time interval | fixed-length payload (TEXT: u8 length + bytes)
//
// TIME: micros() of the ESP32 (1 µs, the finest unit that ISRs and main loop
// share). Every record carries the interval to the PREVIOUS record as a signed
// zigzag varint (1–5 bytes): the edges from the ISRs only arrive with the
// next round of the main loop and then lie in time BEFORE records that
// are already written. From this the decoder computes t_us from the start of recording
// (64 bit, no overflow after 71 min like micros()).
//
// SYNC carries the absolute time (u32 micros, u32 Unix time or 0) and no interval.
// It is at the start and every RAWLOG_SYNC_MS; from there the file is readable again
// even after a lost chunk.
//
// Integers little endian. Raw values only: load cell counts, edges of the
// Hall sensors, the controller's fields in its own resolution. Conversion happens
// in the evaluation (CLAUDE.md: derived quantities never into the raw file).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace calc {
namespace rrl {

constexpr char     MAGIC[4] = {'R', 'R', 'L', '1'};
constexpr size_t   HEADER_FIXED = 6;     // magic + u16 text length

enum Type : uint8_t {
  T_SYNC  = 1,   // u32 abs_us, u32 unix_s                         (no time interval)
  T_EDGE  = 2,   // u8 channel (0 mark, 1 index, 2 test wheel, 3 motor), bit 7 = rejected as echo
  T_HX711 = 3,   // i24 raw value cell 1
  T_VESC  = 4,   // i32 erpm, i32 tacho, i16 duty*1000, i32 iMot*100, i32 iIn*100,
                 // u16 vIn*10, i16 tFet*10, i16 tMot*10, u8 fault, u8 status
  T_CTRL  = 5,   // i16 target km/h*100, i16 duty*10000, u8 brake, u8 phase
  T_TIRE  = 6,   // u16 pressure mbar, i16 temperature *10 °C, u8 battery %
  T_GAP   = 7,   // u32 dropped bytes (buffer was full)
  T_TEXT  = 8,   // u8 length, text
  T_N
};

// Payload length per type; 0 = variable (TEXT).
constexpr uint8_t PAYLOAD[T_N] = {0, 8, 1, 3, 26, 6, 5, 4, 0};
constexpr size_t  REC_MAX = 1 + 5 + 1 + 255;

// ------------------------------------------------------------------ Encoding
inline void put16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
inline void put32(uint8_t* p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }
inline uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t get32(const uint8_t* p) { return get16(p) | (static_cast<uint32_t>(get16(p + 2)) << 16); }

inline size_t putVarint(uint8_t* p, int32_t dt) {
  uint32_t z = (static_cast<uint32_t>(dt) << 1) ^ static_cast<uint32_t>(dt >> 31);   // zigzag
  size_t n = 0;
  while (z >= 0x80) { p[n++] = static_cast<uint8_t>(z | 0x80); z >>= 7; }
  p[n++] = static_cast<uint8_t>(z);
  return n;
}

// Header into out (at least HEADER_FIXED + strlen(text)). Length of the header.
inline size_t header(uint8_t* out, const char* text) {
  const size_t n = std::strlen(text);
  std::memcpy(out, MAGIC, 4);
  put16(out + 4, static_cast<uint16_t>(n));
  std::memcpy(out + 6, text, n);
  return HEADER_FIXED + n;
}

// Holds the encoder's time reference. prevUs is micros() of the last record.
struct Encoder {
  uint32_t prevUs = 0;

  size_t sync(uint8_t* out, uint32_t absUs, uint32_t unixS) {
    out[0] = T_SYNC; put32(out + 1, absUs); put32(out + 5, unixS);
    prevUs = absUs;
    return 9;
  }
  // Record head (type + interval); the caller writes the payload behind it.
  size_t head(uint8_t* out, Type t, uint32_t us) {
    out[0] = t;
    const size_t n = 1 + putVarint(out + 1, static_cast<int32_t>(us - prevUs));
    prevUs = us;
    return n;
  }
  size_t edge(uint8_t* out, uint32_t us, uint8_t ch) {
    size_t n = head(out, T_EDGE, us); out[n++] = ch; return n;
  }
  size_t hx711(uint8_t* out, uint32_t us, int32_t raw) {
    size_t n = head(out, T_HX711, us);
    out[n++] = raw & 0xFF; out[n++] = (raw >> 8) & 0xFF; out[n++] = (raw >> 16) & 0xFF;
    return n;
  }
  size_t vesc(uint8_t* out, uint32_t us, int32_t erpm, int32_t tacho, int16_t duty1000,
              int32_t iMot100, int32_t iIn100, uint16_t vIn10, int16_t tFet10, int16_t tMot10,
              uint8_t fault, uint8_t status) {
    size_t n = head(out, T_VESC, us);
    put32(out + n, static_cast<uint32_t>(erpm)); n += 4;
    put32(out + n, static_cast<uint32_t>(tacho)); n += 4;
    put16(out + n, static_cast<uint16_t>(duty1000)); n += 2;
    put32(out + n, static_cast<uint32_t>(iMot100)); n += 4;
    put32(out + n, static_cast<uint32_t>(iIn100)); n += 4;
    put16(out + n, vIn10); n += 2;
    put16(out + n, static_cast<uint16_t>(tFet10)); n += 2;
    put16(out + n, static_cast<uint16_t>(tMot10)); n += 2;
    out[n++] = fault; out[n++] = status;
    return n;
  }
  size_t ctrl(uint8_t* out, uint32_t us, int16_t target100, int16_t duty10000, uint8_t brake, uint8_t phase) {
    size_t n = head(out, T_CTRL, us);
    put16(out + n, static_cast<uint16_t>(target100)); n += 2;
    put16(out + n, static_cast<uint16_t>(duty10000)); n += 2;
    out[n++] = brake; out[n++] = phase;
    return n;
  }
  size_t tire(uint8_t* out, uint32_t us, uint16_t mbar, int16_t temp10, uint8_t batt) {
    size_t n = head(out, T_TIRE, us);
    put16(out + n, mbar); n += 2; put16(out + n, static_cast<uint16_t>(temp10)); n += 2; out[n++] = batt;
    return n;
  }
  size_t gap(uint8_t* out, uint32_t us, uint32_t bytes) {
    size_t n = head(out, T_GAP, us); put32(out + n, bytes); return n + 4;
  }
  size_t text(uint8_t* out, uint32_t us, const char* s) {
    size_t n = head(out, T_TEXT, us);
    size_t k = std::strlen(s); if (k > 255) k = 255;
    out[n++] = static_cast<uint8_t>(k); std::memcpy(out + n, s, k);
    return n + k;
  }
};

// ------------------------------------------------------------------ Decoding
struct Rec {
  Type     type;
  int64_t  tUs;            // from the first SYNC
  const uint8_t* p;        // payload (points into the caller's buffer)
  uint8_t  len;            // payload length (TEXT: without length byte)
};

enum Status : uint8_t { OK = 0, NEED_MORE, BAD };

struct Decoder {
  bool     started = false;
  int64_t  t = 0;           // µs since the first SYNC
  uint32_t lastAbs = 0;
  uint32_t unixStart = 0;   // Unix time of the first SYNC, 0 = unknown

  // Read one record from p[0..n). OK: *used bytes consumed, r filled.
  // NEED_MORE: incomplete (end of file or refill buffer). BAD: unknown type.
  Status next(const uint8_t* p, size_t n, Rec& r, size_t* used) {
    if (n < 1) return NEED_MORE;
    const uint8_t ty = p[0];
    if (ty == 0 || ty >= T_N) return BAD;
    size_t i = 1;
    if (ty == T_SYNC) {
      if (n < 9) return NEED_MORE;
      const uint32_t abs = get32(p + 1);
      if (!started) { started = true; t = 0; unixStart = get32(p + 5); }
      else          t += static_cast<int32_t>(abs - lastAbs);
      lastAbs = abs;
      r.type = T_SYNC; r.tUs = t; r.p = p + 1; r.len = 8; *used = 9;
      return OK;
    }
    uint32_t z = 0; int shift = 0;
    for (;;) {
      if (i >= n) return NEED_MORE;
      const uint8_t b = p[i++];
      z |= static_cast<uint32_t>(b & 0x7F) << shift;
      if (!(b & 0x80)) break;
      shift += 7; if (shift > 28) return BAD;
    }
    const int32_t dt = static_cast<int32_t>((z >> 1) ^ (0u - (z & 1)));
    size_t len = PAYLOAD[ty];
    if (ty == T_TEXT) { if (i >= n) return NEED_MORE; len = p[i++]; }
    if (i + len > n) return NEED_MORE;
    lastAbs += static_cast<uint32_t>(dt);
    t += dt;
    r.type = static_cast<Type>(ty); r.tUs = t; r.p = p + i; r.len = static_cast<uint8_t>(len);
    *used = i + len;
    return OK;
  }
};

// Check the header; returns the length of the whole header or 0 if it is not an .rrl.
inline size_t parseHeader(const uint8_t* p, size_t n, const char** text, size_t* textLen) {
  if (n < HEADER_FIXED || std::memcmp(p, MAGIC, 4) != 0) return 0;
  const size_t k = get16(p + 4);
  if (n < HEADER_FIXED + k) return 0;
  *text = reinterpret_cast<const char*>(p + 6); *textLen = k;
  return HEADER_FIXED + k;
}

// ------------------------------------------------------------------ CSV
// One CSV per record type. Column names with unit where there is one (CLAUDE.md).
// The values stay in the resolution in which they were recorded.
inline const char* csvName(Type t) {
  switch (t) {
    case T_EDGE:  return "edges.csv";
    case T_HX711: return "loadcell.csv";
    case T_VESC:  return "vesc.csv";
    case T_CTRL:  return "control.csv";
    case T_TIRE:  return "tire.csv";
    case T_GAP: case T_TEXT: case T_SYNC: return "events.csv";
    default:      return "";
  }
}
inline const char* csvHeader(Type t) {
  switch (t) {
    case T_EDGE:  return "t_us,channel,echo\n";
    case T_HX711: return "t_us,raw_counts\n";
    case T_VESC:  return "t_us,erpm,tacho,duty,i_motor_A,i_in_A,v_in_V,t_fet_C,t_motor_C,fault,status\n";
    case T_CTRL:  return "t_us,target_kmh,duty_cmd,brake,phase\n";
    case T_TIRE:  return "t_us,p_tire_bar,t_tire_C,battery_pct\n";
    default:      return "t_us,event,value\n";
  }
}
// Which records go into which file: the three event types share events.csv.
inline bool csvBelongs(Type file, Type rec) {
  if (file == T_TEXT) return rec == T_TEXT || rec == T_GAP || rec == T_SYNC;
  return file == rec;
}

inline int i24(const uint8_t* p) {
  int32_t v = p[0] | (p[1] << 8) | (p[2] << 16);
  if (v & 0x800000) v |= ~0xFFFFFF;
  return v;
}

// Fixed point without float formatting: v / 10^dec, exactly as recorded.
inline int fix(char* o, size_t n, long v, int dec) {
  long d = 1; for (int i = 0; i < dec; i++) d *= 10;
  const bool neg = v < 0; const unsigned long a = neg ? 0ul - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
  if (dec == 0) return std::snprintf(o, n, "%s%lu", neg ? "-" : "", a);
  return std::snprintf(o, n, "%s%lu.%0*lu", neg ? "-" : "", a / d, dec, a % d);
}

// One CSV line (with \n). Length, 0 = does not belong in this file.
inline size_t csvLine(char* o, size_t n, const Rec& r) {
  int k = std::snprintf(o, n, "%lld,", static_cast<long long>(r.tUs));
  if (k < 0 || static_cast<size_t>(k) >= n) return 0;
  const uint8_t* p = r.p;
  char* q = o + k; size_t m = n - k;
  auto add = [&](int w) { if (w > 0 && static_cast<size_t>(w) < m) { q += w; m -= w; } };
  auto sep = [&]() { if (m > 1) { *q++ = ','; m--; } };
  switch (r.type) {
    case T_EDGE:  add(std::snprintf(q, m, "%u,%u", p[0] & 0x7F, (p[0] >> 7) & 1)); break;
    case T_HX711: add(std::snprintf(q, m, "%d", i24(p))); break;
    case T_VESC:
      add(std::snprintf(q, m, "%ld,%ld,", static_cast<long>(static_cast<int32_t>(get32(p))),
                        static_cast<long>(static_cast<int32_t>(get32(p + 4)))));
      add(fix(q, m, static_cast<int16_t>(get16(p + 8)), 3));   sep();
      add(fix(q, m, static_cast<int32_t>(get32(p + 10)), 2));  sep();
      add(fix(q, m, static_cast<int32_t>(get32(p + 14)), 2));  sep();
      add(fix(q, m, get16(p + 18), 1));                         sep();
      add(fix(q, m, static_cast<int16_t>(get16(p + 20)), 1));  sep();
      add(fix(q, m, static_cast<int16_t>(get16(p + 22)), 1));  sep();
      add(std::snprintf(q, m, "%u,%u", p[24], p[25]));
      break;
    case T_CTRL:
      add(fix(q, m, static_cast<int16_t>(get16(p)), 2));      sep();
      add(fix(q, m, static_cast<int16_t>(get16(p + 2)), 4));  sep();
      add(std::snprintf(q, m, "%u,%u", p[4], p[5]));
      break;
    case T_TIRE:
      add(fix(q, m, get16(p), 3)); sep();
      add(fix(q, m, static_cast<int16_t>(get16(p + 2)), 1)); sep();
      add(std::snprintf(q, m, "%u", p[4]));
      break;
    case T_SYNC:  add(std::snprintf(q, m, "sync,%lu", static_cast<unsigned long>(get32(p + 4)))); break;
    case T_GAP:   add(std::snprintf(q, m, "gap_bytes,%lu", static_cast<unsigned long>(get32(p)))); break;
    case T_TEXT: {
      add(std::snprintf(q, m, "note,\""));
      for (uint8_t i = 0; i < r.len && m > 3; i++) {
        const char c = static_cast<char>(p[i]);
        if (c == '"') { *q++ = '"'; m--; }
        *q++ = (c == '\n' || c == '\r') ? ' ' : c; m--;
      }
      *q++ = '"'; m--;
      break;
    }
    default: return 0;
  }
  if (m < 2) return 0;
  *q++ = '\n'; *q = '\0';
  return static_cast<size_t>(q - o);
}

// The files in the archive, in this order.
constexpr Type CSV_FILES[] = {T_HX711, T_EDGE, T_VESC, T_CTRL, T_TIRE, T_TEXT};
constexpr size_t CSV_FILE_N = sizeof(CSV_FILES) / sizeof(CSV_FILES[0]);

// ------------------------------------------------------------------ Summary
// Overview of a raw file for the viewer (webui.cpp /fs/...?info, since 2026-11-26): records
// and bytes per type, time span, edges per channel (and how many were rejected as echo),
// dropped bytes. Feed every decoded record with the bytes it used.
struct Summary {
  uint32_t n[T_N] = {};
  uint64_t bytes[T_N] = {};
  int64_t  t0 = 0, t1 = 0;      // first and last time stamp, µs from the first SYNC
  bool     any = false;
  uint32_t edges[4] = {}, rejected[4] = {};
  uint64_t gapBytes = 0;
  void add(const Rec& r, size_t used) {
    n[r.type]++;
    bytes[r.type] += used;
    if (!any) { t0 = t1 = r.tUs; any = true; }
    if (r.tUs < t0) t0 = r.tUs;
    if (r.tUs > t1) t1 = r.tUs;
    if (r.type == T_EDGE && r.len >= 1) { const uint8_t ch = r.p[0] & 0x7F; if (ch < 4) { edges[ch]++; if (r.p[0] & 0x80) rejected[ch]++; } }
    if (r.type == T_GAP && r.len >= 4) gapBytes += get32(r.p);
  }
};

inline const char* typeName(Type t) {
  switch (t) {
    case T_SYNC: return "sync"; case T_EDGE: return "edge"; case T_HX711: return "hx711";
    case T_VESC: return "vesc"; case T_CTRL: return "ctrl"; case T_TIRE: return "tire";
    case T_GAP: return "gap"; case T_TEXT: return "text"; default: return "?";
  }
}

}  // namespace rrl
}  // namespace calc
