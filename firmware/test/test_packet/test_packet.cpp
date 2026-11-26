// Test of the VESC packet framing — runs on the COMPUTER, not on the board.
//
//     cd firmware && pio test -e native -f test_packet
//
// or by hand:    c++ -std=c++17 -Wall -Wextra -Ifirmware/src firmware/test/test_packet/test_packet.cpp \
//                 firmware/src/vesc/packet.cpp -o /tmp/test_packet && /tmp/test_packet
//
// Purpose: frame and checksum are right BEFORE anything is connected. A wrong
// CRC on the connected controller looks like a wiring fault and costs
// hours.
//
// The anchor value is 0x31C3 for "123456789" — that is the catalogue check value of
// CRC-16/XMODEM and therefore independent of this project. If it matches, the
// checksum is right; if it does not, the rest is of no use.
#include "vesc/packet.h"

#include <cstdio>
#include <cstring>

static int g_fails = 0;

static void check(bool ok, const char* was) {
  std::printf("%-58s %s\n", was, ok ? "ok" : "FAILED");
  if (!ok) g_fails++;
}

int main() {
  using namespace vesc;

  // ---------------------------------------------------------------- Checksum
  const char* chk = "123456789";
  check(crc16(reinterpret_cast<const uint8_t*>(chk), 9) == 0x31C3,
        "CRC-16/XMODEM(\"123456789\") == 0x31C3");
  check(crc16(nullptr, 0) == 0x0000, "CRC over 0 bytes is 0");

  // ----------------------------------------------------------- Frame, short
  // COMM_FW_VERSION is a single null byte. The CRC over it is 0x0000.
  const uint8_t fw[] = {0x00};
  uint8_t out[32];
  size_t n = frame(fw, 1, out, sizeof(out));
  const uint8_t want[] = {0x02, 0x01, 0x00, 0x00, 0x00, 0x03};
  check(n == 6 && std::memcmp(out, want, 6) == 0,
        "COMM_FW_VERSION frames to 02 01 00 00 00 03");

  // Too small an output buffer must not write, but report 0.
  check(frame(fw, 1, out, 5) == 0, "too small buffer reports 0");
  check(frame(fw, 0, out, sizeof(out)) == 0, "payload 0 is rejected");

  // ----------------------------------------------------------- Frame, long
  static uint8_t big[300];
  for (size_t i = 0; i < sizeof(big); i++) big[i] = static_cast<uint8_t>(i);
  static uint8_t outBig[320];
  n = frame(big, sizeof(big), outBig, sizeof(outBig));
  check(n == sizeof(big) + 6 && outBig[0] == 0x03 && outBig[1] == 0x01 &&
            outBig[2] == 0x2C,
        "300 bytes take the long form 03 01 2C");

  // ------------------------------------------------------------- Round trip
  Reader r;
  bool done = false;
  for (size_t i = 0; i < 6; i++) done = r.feed(want[i]);
  check(done && r.payloadLen() == 1 && r.payload()[0] == 0x00,
        "round trip short: payload arrives unchanged");

  r.reset();
  done = false;
  for (size_t i = 0; i < n; i++) done = r.feed(outBig[i]);
  check(done && r.payloadLen() == sizeof(big) &&
            std::memcmp(r.payload(), big, sizeof(big)) == 0,
        "round trip long: 300 bytes arrive unchanged");

  // ------------------------------------------------- Junk before the packet
  r.reset();
  done = false;
  const uint8_t muell[] = {0xFF, 0xAA, 0x00};
  for (uint8_t b : muell) r.feed(b);
  for (size_t i = 0; i < 6; i++) done = r.feed(want[i]);
  check(done && r.payloadLen() == 1, "junk before does not swallow the packet");
  check(r.dropped() == 3, "the three junk bytes are counted");

  // ------------------------------------------- started bad frame
  // The tricky case: a corrupted length byte makes the receiver wait for a payload
  // that does not exist, and in doing so it consumes the FOLLOWING real packet.
  // What is expected is not that nothing is lost — no implementation can do that —
  // but that it recovers: one noise byte may cost at most one packet.
  r.reset();
  r.feed(0x02); r.feed(0x05); r.feed(0x11);   // 0x02 0x05 promises 5 payload bytes
  int decoded = 0;
  for (int rep = 0; rep < 2; rep++)
    for (size_t i = 0; i < 6; i++)
      if (r.feed(want[i])) decoded++;
  check(decoded == 2, "after a bad frame both following packets are read");

  // ----------------------------------------------------- wrong checksum
  r.reset();
  uint8_t kaputt[6];
  std::memcpy(kaputt, want, 6);
  kaputt[3] ^= 0x01;
  done = false;
  for (size_t i = 0; i < 6; i++) done = r.feed(kaputt[i]);
  check(!done, "wrong checksum yields no packet");

  // ------------------------------------------- long form with short length
  // The controller rejects this; so do we, otherwise both sides have different
  // ideas of what a valid frame is.
  r.reset();
  done = false;
  r.feed(0x03); r.feed(0x00); r.feed(0x01); r.feed(0x00);
  r.feed(0x00); r.feed(0x00); done = r.feed(0x03);
  check(!done, "long form with length < 256 is rejected");

  std::printf("\n%s\n", g_fails ? "THERE ARE FAILURES" : "all checks passed");
  return g_fails ? 1 : 0;
}
