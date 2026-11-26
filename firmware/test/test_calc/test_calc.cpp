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

// ------------------------------------------------------------ Raw data (.rrl)
// Times deliberately out of order (edges arrive after the controller packet) and across
// the overflow of micros(); the expectation is computed by hand.
static void test_rrl_roundtrip() {
  using namespace calc::rrl;
  uint8_t b[512]; size_t n = header(b, "k=v\n");
  Encoder e;
  const uint32_t t0 = 0xFFFFFF00u;                      // 256 µs before the overflow
  n += e.sync(b + n, t0, 1764000000u);
  n += e.vesc(b + n, t0 + 1000, -12345, 678, -250, 1234, -56, 361, 254, -15, 0, 2);
  n += e.edge(b + n, t0 + 400, 0x80 | 0);               // older than the record before
  n += e.hx711(b + n, t0 + 2000, -8388608);             // smallest 24-bit value
  n += e.ctrl(b + n, t0 + 2000, 1550, -123, 1, 4);
  n += e.tire(b + n, t0 + 70000000u, 4250, -35, 87);    // 70 s later, after the overflow
  n += e.text(b + n, t0 + 70000001u, "a\"b");
  n += e.gap(b + n, t0 + 70000002u, 99);

  const char* text; size_t tl;
  const size_t h = parseHeader(b, n, &text, &tl);
  TEST_ASSERT_EQUAL_UINT(10u, h);
  TEST_ASSERT_EQUAL_MEMORY("k=v\n", text, 4);

  Decoder d; Rec r; size_t used; size_t off = h; char line[200];
  const int64_t expT[] = {0, 1000, 400, 2000, 2000, 70000000, 70000001, 70000002};
  const char* expL[] = {
    "0,sync,1764000000\n",
    "1000,-12345,678,-0.250,12.34,-0.56,36.1,25.4,-1.5,0,2\n",
    "400,0,1\n",
    "2000,-8388608\n",
    "2000,15.50,-0.0123,1,4\n",
    "70000000,4.250,-3.5,87\n",
    "70000001,note,\"a\"\"b\"\n",
    "70000002,gap_bytes,99\n"};
  for (int i = 0; i < 8; i++) {
    TEST_ASSERT_EQUAL(OK, d.next(b + off, n - off, r, &used));
    off += used;
    TEST_ASSERT_EQUAL_INT64(expT[i], r.tUs);
    TEST_ASSERT_TRUE(csvLine(line, sizeof(line), r) > 0);
    TEST_ASSERT_EQUAL_STRING(expL[i], line);
  }
  TEST_ASSERT_EQUAL_UINT(n, off);
  TEST_ASSERT_EQUAL(NEED_MORE, d.next(b + off, 0, r, &used));
  // truncated record: reading on asks for more, not nonsense
  Decoder d2; size_t o2 = h; d2.next(b + o2, n - o2, r, &used); o2 += used;
  TEST_ASSERT_EQUAL(NEED_MORE, d2.next(b + o2, 5, r, &used));   // VESC record is 29 bytes
  uint8_t junk = 0xEE;
  TEST_ASSERT_EQUAL(BAD, d2.next(&junk, 1, r, &used));
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_catalog_value);
  RUN_TEST(test_zip_stored_archive);
  RUN_TEST(test_zip_streamed_entry);
  RUN_TEST(test_rrl_roundtrip);
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
  RUN_TEST(test_fmt_time);
  RUN_TEST(test_json_str_escapes);
  RUN_TEST(test_json_str_truncates_safely);
  return UNITY_END();
}
