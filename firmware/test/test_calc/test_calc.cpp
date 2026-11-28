// Host tests of the calculation cores in src/calc/ -- run on the COMPUTER (pio test -e native).
//
// The expected values are computed by hand or independently, not with the
// formula under test: the fit gets an exact line, the spin-down a curve integrated
// with known A1/B1/J, the time known calendar dates.
#include <unity.h>

#include <cmath>
#include <cstring>

#include <cstdio>

#include "calc/crc32.h"
#include "calc/json.h"
#include "calc/zip.h"
#include "calc/rawlog.h"
#include "calc/linfit.h"
#include "calc/loss.h"
#include "calc/warmup.h"
#include "calc/spindown.h"
#include "calc/timefmt.h"

void setUp() {}
void tearDown() {}

// ------------------------------------------------------------ LinFit
static void test_linfit_exact_line() {
  calc::LinFit f;
  for (int x = 0; x < 10; x++) f.add(x, 2.0 + 3.0 * x);
  float a, b;
  TEST_ASSERT_TRUE(f.solve(a, b));
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.0f, b);
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, a);
}
static void test_linfit_too_few_points() {
  calc::LinFit f; f.add(0, 1); f.add(1, 2);
  float a, b;
  TEST_ASSERT_FALSE(f.solve(a, b));                 // three points required
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 1.0f, static_cast<float>(f.slope(2)));   // allowed with two
}
static void test_linfit_degenerate_x() {
  calc::LinFit f;
  for (int i = 0; i < 5; i++) f.add(4.0, i);        // all x equal: no slope can be determined
  TEST_ASSERT_TRUE(std::isnan(f.slope()));
  float a, b;
  TEST_ASSERT_FALSE(f.solve(a, b));
}
static void test_linfit_min_den_guard() {
  calc::LinFit f;                                   // t over only 0.1 s: variance T^2/12 = 0.00083 n^2
  for (int i = 0; i < 20; i++) f.add(i * 0.005, 10.0 + i);
  TEST_ASSERT_TRUE(std::isnan(f.slope(10, 0.02 * 20 * 20)));   // the limit from state.cpp applies
  TEST_ASSERT_FALSE(std::isnan(f.slope(10, 0.0)));
}

// ------------------------------------------------------------ Scatter
static void test_sd_from_sums() {
  // 1, 2, 3, 4: mean 2.5, sample variance 5/3
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, std::sqrt(5.0f / 3.0f), calc::sdFromSums(10.0, 30.0, 4));
  TEST_ASSERT_TRUE(std::isnan(calc::sdFromSums(5.0, 25.0, 1)));
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, calc::sdFromSums(6.0, 12.0, 3));   // three times 2: scatter 0
}

// ------------------------------------------------------------ Loss model
static void test_loss_idle_only() {
  // A0 + B0 w^2 at w = 10: 0.1 + 0.001 * 100 = 0.2
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, calc::lossNm(10.0f, 0.45f, 0.1f, 0.001f, true, 1, 1, false, NAN));
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, calc::lossNm(10.0f, 0.45f, 0.1f, 0.001f, false, 1, 1, false, NAN));
}
static void test_loss_wheel_via_power_balance() {
  // Independent: wheel loss as power at the wheel, divided by omega of the drum.
  const float w = 12.0f, rDrum = 0.45f, uWheel = 1.54f, a1 = 0.012f, b1 = 7.1e-5f;
  const float rWheel = uWheel / (2.0f * static_cast<float>(M_PI));
  const float wWheel = w * rDrum / rWheel;                        // same surface speed
  const float pWheel = (a1 + b1 * wWheel * wWheel) * wWheel;      // W
  const float expected = pWheel / w;                               // Nm at the drum shaft
  TEST_ASSERT_FLOAT_WITHIN(1e-6f, expected, calc::lossNm(w, rDrum, 0, 0, false, a1, b1, true, uWheel));
}
static void test_loss_wheel_needs_circumference() {
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, calc::lossNm(12.0f, 0.45f, 0, 0, false, 0.01f, 1e-5f, true, NAN));
  TEST_ASSERT_FLOAT_WITHIN(1e-9f, 0.0f, calc::lossNm(12.0f, 0.45f, 0, 0, false, 0.01f, 1e-5f, true, 0.0f));
}

// ------------------------------------------------------------ Spin-down fit
static void test_spindown_recovers_a1_b1() {
  // dw/dt = -(A + B w^2)/J, Euler with a small step; samples as at the wheel: one per revolution.
  const double A = 0.012, B = 7.1e-5, J = 0.11;
  double w = 49.0, t = 0.0, I = 0.0;
  calc::SpindownSums s;
  double nextSample = 0.0;
  while (w > 5.0) {
    if (t >= nextSample) { s.add(t, I, w); nextSample += 2.0 * M_PI / w; }
    const double dt = 1e-4;
    I += w * w * dt;
    w -= (A + B * w * w) / J * dt;
    t += dt;
  }
  float a1, b1, resid; double w0;
  TEST_ASSERT_TRUE(s.solve(J, a1, b1, w0, resid));
  TEST_ASSERT_FLOAT_WITHIN(0.01f * A, A, a1);     // 1 %
  TEST_ASSERT_FLOAT_WITHIN(0.01f * B, B, b1);
  TEST_ASSERT_FLOAT_WITHIN(0.05, 49.0, w0);
  TEST_ASSERT_TRUE(resid < 0.01f);
}
static void test_spindown_negative_a_constrained() {
  // A loss rising faster than w^2 (here w^3) makes the free A + B w^2 fit put A < 0;
  // solvePositive must refit with A = 0 and keep B positive.
  const double J = 0.0697;
  double w = 38.0, t = 0.0, I = 0.0;
  calc::SpindownSums s;
  double nextSample = 0.0;
  while (w > 6.0) {
    if (t >= nextSample) { s.add(t, I, w); nextSample += 2.0 * M_PI / w; }
    const double dt = 1e-4;
    I += w * w * dt;
    w -= (1.5e-6 * w * w * w) / J * dt;
    t += dt;
  }
  float a1, b1, resid; double w0; int forced;
  TEST_ASSERT_TRUE(s.solve(J, a1, b1, w0, resid));
  TEST_ASSERT_TRUE(a1 < 0.0f);                       // the free fit is unphysical
  TEST_ASSERT_TRUE(s.solvePositive(J, a1, b1, w0, resid, forced));
  TEST_ASSERT_EQUAL_INT(1, forced);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, a1);
  TEST_ASSERT_TRUE(b1 > 0.0f);
  // Only a safety net: the B-only model is coarse for such a curve (here 0.024 against
  // 0.054 Nm at 33 rad/s), but physical and of the right order.
  TEST_ASSERT_TRUE(b1 * 33.0f * 33.0f > 0.3f * 0.054f && b1 * 33.0f * 33.0f < 3.0f * 0.054f);
}
static void test_spindown_singular() {
  calc::SpindownSums s;
  for (int i = 0; i < 6; i++) s.add(1.0, 2.0, 3.0);   // the same point six times
  float a1, b1, resid; double w0;
  TEST_ASSERT_FALSE(s.solve(0.1, a1, b1, w0, resid));
  calc::SpindownSums e;
  e.add(0, 0, 10); e.add(1, 100, 9);
  TEST_ASSERT_FALSE(e.solve(0.1, a1, b1, w0, resid));  // two points, three unknowns
}

// ------------------------------------------------------------ Time, JSON
static void test_fmt_time() {
  char b[32];
  calc::fmtTime(0, b, sizeof(b), true);
  TEST_ASSERT_EQUAL_STRING("\xe2\x80\x94", b);
  calc::fmtTime(951868800u, b, sizeof(b), true);   // 2000-03-01 00:00 UTC
  TEST_ASSERT_EQUAL_STRING("2000-03-01 00:00", b);
  calc::fmtTime(1709210040u, b, sizeof(b), true);   // leap day
  TEST_ASSERT_EQUAL_STRING("2024-02-29 12:34", b);
  calc::fmtTime(1795615380u, b, sizeof(b), false);  // file name form
  TEST_ASSERT_EQUAL_STRING("2026-11-25-1403", b);
}
static void test_json_str_escapes() {
  char b[32];
  const size_t n = calc::jsonStr(b, sizeof(b), "a\"b\\c\nd");
  TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c d\"", b);
  TEST_ASSERT_EQUAL_UINT(std::strlen(b), n);
}
static void test_json_str_truncates_safely() {
  char b[6];
  calc::jsonStr(b, sizeof(b), "abcdefgh");
  TEST_ASSERT_TRUE(std::strlen(b) < sizeof(b));   // always null-terminated, never overflowed
  TEST_ASSERT_EQUAL_CHAR('"', b[0]);
}

// ------------------------------------------------------------ CRC-32, ZIP
static void test_crc32_catalog_value() {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, calc::crc32(0, reinterpret_cast<const uint8_t*>("123456789"), 9));
  // the same in two pieces
  uint32_t c = calc::crc32(0, reinterpret_cast<const uint8_t*>("1234"), 4);
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, calc::crc32(c, reinterpret_cast<const uint8_t*>("56789"), 5));
  TEST_ASSERT_EQUAL_HEX32(0u, calc::crc32(0, nullptr, 0));
}
static void test_zip_stored_archive() {
  // One file "a.txt" with "hello": 30+5 + 5 + 46+5 + 22 = 113 bytes, signatures in place.
  const char* data = "hello";
  const uint32_t crc = calc::crc32(0, reinterpret_cast<const uint8_t*>(data), 5);
  uint8_t z[256]; size_t p = 0;
  p += calc::zipLocalHeader(z + p, "a.txt", crc, 5);
  std::memcpy(z + p, data, 5); p += 5;
  const uint32_t cd = p;
  p += calc::zipCentralHeader(z + p, "a.txt", crc, 5, 0);
  p += calc::zipEndRecord(z + p, 1, p - cd, cd);
  const uint32_t sizes[] = {5}; const size_t nl[] = {5};
  TEST_ASSERT_EQUAL_UINT32(113u, calc::zipArchiveSize(sizes, nl, 1));
  TEST_ASSERT_EQUAL_UINT(113u, p);
  TEST_ASSERT_EQUAL_HEX8(0x50, z[0]); TEST_ASSERT_EQUAL_HEX8(0x4b, z[1]); TEST_ASSERT_EQUAL_HEX8(0x03, z[2]); TEST_ASSERT_EQUAL_HEX8(0x04, z[3]);
  TEST_ASSERT_EQUAL_HEX8(0x01, z[cd + 2]); TEST_ASSERT_EQUAL_HEX8(0x02, z[cd + 3]);
  TEST_ASSERT_EQUAL_HEX8(0x05, z[p - 22 + 2]); TEST_ASSERT_EQUAL_HEX8(0x06, z[p - 22 + 3]);
  TEST_ASSERT_EQUAL_MEMORY("hello", z + 35, 5);
  // for checking with a real unzipper (python3 -m zipfile -t /tmp/rr_test.zip)
  if (FILE* f = std::fopen("/tmp/rr_test.zip", "wb")) { std::fwrite(z, 1, p, f); std::fclose(f); }
}

// ------------------------------------------------------------ Raw data (raw_*.csv)
// Times deliberately out of order (an edge arrives after the controller packet) and across
// the overflow of micros(); the expected lines are written by hand. The stream also goes to
// /tmp/rr_test_raw.csv for tools/rawlog.py and python3 csv.
static void test_rawcsv_records() {
  using namespace calc::rawcsv;
  char b[2048]; size_t n = 0;
  Encoder e;
  const uint32_t t0 = 0xFFFFFF00u;                      // 256 µs before the overflow
  e.begin(t0);
  size_t len[10]; int k = 0;
  len[k] = e.sync(b + n, sizeof(b) - n, t0, 1764000000u); n += len[k++];
  len[k] = e.vesc(b + n, sizeof(b) - n, t0 + 1000, -12345, 678, -250, 1234, -56, 361, 254, -15, 0, 2); n += len[k++];
  len[k] = e.edge(b + n, sizeof(b) - n, t0 + 400, 0x80 | 0);   // older than the line before
  n += len[k++];
  len[k] = e.edge(b + n, sizeof(b) - n, t0 + 1500, 3); n += len[k++];   // motor Hall
  len[k] = e.hx711(b + n, sizeof(b) - n, t0 + 2000, -8388608); n += len[k++];   // smallest 24-bit value
  len[k] = e.ctrl(b + n, sizeof(b) - n, t0 + 2000, 1550, -123, 1, 4); n += len[k++];
  len[k] = e.tire(b + n, sizeof(b) - n, t0 + 70000000u, 4250, -35, 87); n += len[k++];   // 70 s later
  len[k] = e.tire(b + n, sizeof(b) - n, t0 + 70000000u, 0xFFFF, INT16_MIN, 0xFF); n += len[k++];
  len[k] = e.note(b + n, sizeof(b) - n, t0 + 70000001u, "a\"b\\c, d\ne"); n += len[k++];
  len[k] = e.gap(b + n, sizeof(b) - n, t0 + 70000002u, 99); n += len[k++];
  b[n] = '\0';
  const char* expLines =
    "S,0,1764000000,4294967040\n"
    "M,1000,-12345,678,-0.250,12.34,-0.56,36.1,25.4,-1.5,0,2\n"
    "D,400,1\n"
    "H,1500,0\n"
    "T,2000,-8388608\n"
    "C,2000,15.50,-0.0123,1,4\n"
    "P,70000000,4.250,-3.5,87\n"
    "P,70000000,,,\n"
    "N,70000001,\"a\"\"b\\c, d e\"\n"
    "G,70000002,99\n";
  TEST_ASSERT_EQUAL_STRING(expLines, b);

  // split into the per-type CSVs
  const char* expCsv[] = {
    "0,sync,1764000000\n",
    "1000,-12345,678,-0.250,12.34,-0.56,36.1,25.4,-1.5,0,2\n",
    "400,drum,1\n",
    "1500,motor,0\n",
    "2000,-8388608\n",
    "2000,15.50,-0.0123,1,4\n",
    "70000000,4.250,-3.5,87\n",
    "70000000,,,\n",
    "70000001,note,\"a\"\"b\\c, d e\"\n",
    "70000002,gap,99\n"};
  const Kind expK[] = {K_SYNC, K_VESC, K_DRUM, K_MOTOR, K_HX, K_CTRL, K_TIRE, K_TIRE, K_NOTE, K_GAP};
  Summary sm; size_t off = 0; char line[200];
  for (int i = 0; i < 10; i++) {
    Rec r;
    TEST_ASSERT_TRUE(parseLine(b + off, len[i], r));
    TEST_ASSERT_EQUAL_UINT8(expK[i], r.kind);
    TEST_ASSERT_TRUE(csvLine(line, sizeof(line), r) > 0);
    TEST_ASSERT_EQUAL_STRING(expCsv[i], line);
    sm.add(r, len[i]);
    off += len[i];
  }
  TEST_ASSERT_EQUAL_UINT(n, off);
  { Rec r; size_t o8 = 0; for (int i = 0; i < 8; i++) o8 += len[i];
    TEST_ASSERT_TRUE(parseLine(b + o8, len[8], r)); TEST_ASSERT_EQUAL_UINT8(K_NOTE, r.kind);
    char t[40]; unquote(t, sizeof(t), r.body, r.bodyLen); TEST_ASSERT_EQUAL_STRING("a\"b\\c, d e", t); }
  TEST_ASSERT_EQUAL_UINT32(1, sm.n[K_DRUM]); TEST_ASSERT_EQUAL_UINT32(1, sm.n[K_MOTOR]); TEST_ASSERT_EQUAL_UINT32(2, sm.n[K_TIRE]);
  TEST_ASSERT_EQUAL_UINT32(1, sm.edges[3]); TEST_ASSERT_EQUAL_UINT32(0, sm.rejected[3]);
  TEST_ASSERT_EQUAL_INT64(0, sm.t0); TEST_ASSERT_EQUAL_INT64(70000002, sm.t1);
  TEST_ASSERT_EQUAL_UINT32(1, sm.edges[0]); TEST_ASSERT_EQUAL_UINT32(1, sm.rejected[0]);
  TEST_ASSERT_EQUAL_UINT64(99, sm.gapBytes);
  TEST_ASSERT_EQUAL_UINT32(1764000000u, sm.unixStart);
  uint64_t tot = 0; for (int i = 0; i < K_N; i++) tot += sm.bytes[i];
  TEST_ASSERT_EQUAL_UINT64(n, tot);

  char h[160];
  TEST_ASSERT_TRUE(csvHeader(h, sizeof(h), K_VESC) > 0);
  TEST_ASSERT_EQUAL_STRING("t_us,erpm,tacho,duty,i_motor_A,i_in_A,v_in_V,t_fet_C,t_motor_C,fault,status\n", h);
  csvHeader(h, sizeof(h), K_NOTE); TEST_ASSERT_EQUAL_STRING("t_us,event,value\n", h);
  csvHeader(h, sizeof(h), K_DRUM); TEST_ASSERT_EQUAL_STRING("t_us,channel,echo\n", h);
  TEST_ASSERT_TRUE(csvBelongs(K_DRUM, K_WHEEL)); TEST_ASSERT_FALSE(csvBelongs(K_DRUM, K_HX));

  // cut off and foreign lines are not records
  Rec r;
  TEST_ASSERT_FALSE(parseLine(b + len[0], 30, r));                       // vesc with 5 of 10 fields
  TEST_ASSERT_FALSE(parseLine("N,5,\"open", 9, r));                     // quote never closed
  TEST_ASSERT_FALSE(parseLine("T,5", 3, r));                             // no field
  TEST_ASSERT_FALSE(parseLine("D,5,mark,0", 10, r));                     // edge with a channel column
  TEST_ASSERT_FALSE(parseLine("L,5,1", 5, r));                           // reserved, not written yet
  TEST_ASSERT_FALSE(parseLine("bogus,5,1", 9, r));
  TEST_ASSERT_FALSE(parseLine("kind,plateau,v_kmh", 18, r));            // points.csv header
  TEST_ASSERT_FALSE(parseLine("measure,1,28.80", 15, r));
  // a line that does not fit is refused, not written half
  char small[20];
  TEST_ASSERT_EQUAL_UINT(0, e.vesc(small, sizeof(small), t0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0));
  TEST_ASSERT_EQUAL_UINT(0, e.edge(small, sizeof(small), t0, 5));        // unknown channel: not written
  // a long note is cut, but stays a valid record
  { char big[400]; std::memset(big, '"', 300); big[300] = '\0';
    char o[REC_MAX]; const size_t m = e.note(o, sizeof(o), t0, big);
    TEST_ASSERT_TRUE(m > 0 && m < sizeof(o)); TEST_ASSERT_EQUAL_CHAR('\n', o[m - 1]);
    TEST_ASSERT_TRUE(parseLine(o, m, r)); }
  if (FILE* f = std::fopen("/tmp/rr_test_raw.csv", "wb")) { std::fwrite(b, 1, n, f); std::fclose(f); }
}

// Metadata JSON with NaN as null and an escaped stand revision, and the legend; written to
// /tmp/rr_test_raw.json and .md and checked with python3 json there.
static void test_rawcsv_meta() {
  using namespace calc::rawcsv;
  static const char* const ph[] = {"IDLE", "TARE"};
  HeaderInfo h;
  h.csvFile = "rr_test_raw.csv"; h.legendFile = "rr_test_raw.md";
  h.firmware = "Nov 27 2026 10:00:00"; h.commit = "abc1234"; h.standRev = "rev \"B\"\\2\nnew cell";
  h.startLocal = "2026-11-27 10:00:00"; h.unixStartLocal = 1795773600u; h.runId = "";
  h.marksPerRev = 8; h.wheelMarksPerRev = 1; h.motorPolePairs = 15;
  h.drumCircM = 0.4523f; h.torqueArmM = 0.1f; h.wheelCircM = NAN;
  h.directionSign = -1; h.vescPollMs = 200; h.hx711RateSps = 80; h.hx711GainPulses = 1; h.syncMs = 1000;
  h.loadcellCapacityN = 49.03f; h.loadEmptyKg = 1.0f; h.loadWeightKg = 2.0f; h.loadArmM = 0.3f;
  h.nPerCount = NAN; h.tareOffset = -81234; h.validity = -1; h.validityWhy = "not calibrated: Setup \xE2\x86\x92 Calibrate";
  h.cal.present = true; h.cal.ver = 1; h.cal.npc = 1.2345e-5f; h.cal.linPctFS = NAN; h.cal.method = "single point (k)";
  h.phases = ph; h.phaseN = 2;
  static char b[HEADER_MAX];
  const size_t n = header(b, sizeof(b), h);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NULL(std::strstr(b, "nan"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"stand_rev\": \"rev \\\"B\\\"\\\\2 new cell\""));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"run_id\": null"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"loadcell_n_per_count\":null"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"wheel_circumference_m\":null"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"drum_circumference_m\":0.4523,"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"npc\":1.2345e-05,"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"T\":[[\"t_us\",\"us\"],[\"raw_counts\",\"HX711 counts\"]]"));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"D\":{\"name\":\"drum\","));
  TEST_ASSERT_NOT_NULL(std::strstr(b, "\"L\":{\"name\":\"axle_load\""));
  // the storage task's way: file keys + "," + header without its "{" gives the same text
  { HeaderInfo h2 = h; h2.csvFile = nullptr; static char c[HEADER_MAX]; const size_t cn = header(c, sizeof(c), h2);
    char fk[160]; const size_t fn = headerFiles(fk, sizeof(fk), "rr_test_raw.csv", "rr_test_raw.md");
    TEST_ASSERT_TRUE(cn > 0 && fn > 0);
    static char j[HEADER_MAX + 200]; std::snprintf(j, sizeof(j), "%s,%s", fk, c + 1);
    TEST_ASSERT_EQUAL_STRING(b, j); }
  char small[200]; TEST_ASSERT_EQUAL_UINT(0, header(small, sizeof(small), h));   // too small: refused
  static char md[LEGEND_MAX];
  const size_t mn = legend(md, sizeof(md), "rr_test_raw", "Recorded by the guided run `2026-11-27-100000`.");
  TEST_ASSERT_TRUE(mn > 0);
  TEST_ASSERT_NOT_NULL(std::strstr(md, "| `M` | erpm (1/min electrical), tacho (steps), duty (0..1),"));
  TEST_ASSERT_NOT_NULL(std::strstr(md, "| `L` | | axle load cells"));
  TEST_ASSERT_NOT_NULL(std::strstr(md, "grep '^T,' rr_test_raw.csv"));
  if (FILE* f = std::fopen("/tmp/rr_test_raw.json", "wb")) { std::fwrite(b, 1, n, f); std::fclose(f); }
  if (FILE* f = std::fopen("/tmp/rr_test_raw.md", "wb")) { std::fwrite(md, 1, mn, f); std::fclose(f); }
  std::printf("json %u B, md %u B\n", static_cast<unsigned>(n), static_cast<unsigned>(mn));
}

// Streamed entry (data descriptor): written to /tmp and checked there by
// python3 -m zipfile -t, like the non-streamed one above.
static void test_zip_streamed_entry() {
  uint8_t z[256]; size_t p = 0;
  p += calc::zipLocalHeaderStreamed(z + p, "b.csv");
  TEST_ASSERT_EQUAL_HEX8(0x08, z[6]);
  std::memcpy(z + p, "x,y\n", 4); p += 4;
  const uint32_t crc = calc::crc32(0, reinterpret_cast<const uint8_t*>("x,y\n"), 4);
  p += calc::zipDataDescriptor(z + p, crc, 4);
  const size_t cd = p;
  p += calc::zipCentralHeader(z + p, "b.csv", crc, 4, 0, 0, calc::ZIP_DOS_DATE_1980, calc::ZIP_FLAG_DESCRIPTOR);
  p += calc::zipEndRecord(z + p, 1, p - cd, cd);
  TEST_ASSERT_EQUAL_UINT(30u + 5 + 4 + 16 + 46 + 5 + 22, p);
  if (FILE* f = std::fopen("/tmp/rr_test_stream.zip", "wb")) { std::fwrite(z, 1, p, f); std::fclose(f); }
}

void test_warmup_settled() {
  // Minute values of M (Nm) from run 2026-11-26-1542: minutes 13-17 and 1-5.
  const float flat[5] = {0.3698f, 0.3618f, 0.3758f, 0.3649f, 0.3612f};
  const float rising[5] = {0.4285f, 0.3883f, 0.3834f, 0.3809f, 0.3680f};
  const float gap[5] = {0.37f, NAN, 0.37f, 0.37f, 0.37f};
  const float tFlat[5] = {21.0f, 21.1f, 21.1f, 21.2f, 21.3f};
  const float tWarming[5] = {20.0f, 21.0f, 22.0f, 23.0f, 24.0f};
  const float nanT[5] = {NAN, NAN, NAN, NAN, NAN};
  TEST_ASSERT_TRUE(calc::warmupSettled(flat, tFlat, 5, 0.025f, 0.2f));
  TEST_ASSERT_TRUE(calc::warmupSettled(flat, nanT, 5, 0.025f, 0.2f));     // no sensor: power alone
  TEST_ASSERT_FALSE(calc::warmupSettled(rising, tFlat, 5, 0.025f, 0.2f));  // 0.06 Nm spread
  TEST_ASSERT_FALSE(calc::warmupSettled(flat, tWarming, 5, 0.025f, 0.2f)); // tire still warming 1 K/min
  TEST_ASSERT_FALSE(calc::warmupSettled(gap, tFlat, 5, 0.025f, 0.2f));     // a missing minute
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_catalog_value);
  RUN_TEST(test_zip_stored_archive);
  RUN_TEST(test_zip_streamed_entry);
  RUN_TEST(test_rawcsv_records);
  RUN_TEST(test_rawcsv_meta);
  RUN_TEST(test_linfit_exact_line);
  RUN_TEST(test_linfit_too_few_points);
  RUN_TEST(test_linfit_degenerate_x);
  RUN_TEST(test_linfit_min_den_guard);
  RUN_TEST(test_sd_from_sums);
  RUN_TEST(test_loss_idle_only);
  RUN_TEST(test_loss_wheel_via_power_balance);
  RUN_TEST(test_loss_wheel_needs_circumference);
  RUN_TEST(test_spindown_recovers_a1_b1);
  RUN_TEST(test_spindown_singular);
  RUN_TEST(test_spindown_negative_a_constrained);
  RUN_TEST(test_fmt_time);
  RUN_TEST(test_json_str_escapes);
  RUN_TEST(test_json_str_truncates_safely);
  RUN_TEST(test_warmup_settled);
  return UNITY_END();
}
