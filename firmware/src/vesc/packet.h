#pragma once
//
// Packet framing of the VESC protocol.
//
//   1…255 byte payload:     0x02  len[1]          payload  crc_hi crc_lo  0x03
//   256…65535 bytes:        0x03  len_hi len_lo   payload  crc_hi crc_lo  0x03
//
// CRC16-CCITT/XMODEM: polynomial 0x1021, initial value 0x0000, MSB first, no final XOR,
// computed EXCLUSIVELY over the payload (not over start and length bytes),
// transmitted big-endian. All multi-byte numbers in the protocol are big-endian.
//
// The first payload byte is always the command id; answers echo it.
//
// The controller rejects the long form if the payload fits into the short one
// ("A shorter packet should use less length bytes") -- so frame() chooses itself,
// and Reader rejects the long form with a short length as well. Both sides must
// share the same idea of what a valid frame is.
//
// DELIBERATELY WITHOUT Arduino.h: this way this part can be compiled on the host and
// checked against known byte sequences before anything is connected.
// See firmware/test/test_packet/test_packet.cpp.
//
#include <cstddef>
#include <cstdint>

namespace vesc {

// Largest payload the controller accepts (PACKET_MAX_PL_LEN of the firmware).
constexpr size_t PAYLOAD_MAX = 512;

// Largest complete frame: long form (3) + payload + checksum (2) + end (1).
constexpr size_t FRAME_MAX = PAYLOAD_MAX + 6;

// Checksum over the payload. The check value for "123456789" is 0x31C3 -- that is the
// catalogue value of CRC-16/XMODEM and thus a cross-check independent of the project.
uint16_t crc16(const uint8_t* data, size_t len);

// Frames payload into out. Returns the total length, or 0 if out is too small
// or the payload would be 0 or larger than PAYLOAD_MAX.
size_t frame(const uint8_t* payload, size_t len, uint8_t* out, size_t outCap);

// Receiver. Feed bytes one at a time, never blocking -- the ESP32 serves the
// web server alongside.
//
// Buffered and RE-EVALUATED as needed, not as a pure state machine: a state
// machine that falls for a corrupted length byte consumes the following real
// packet as supposed payload and cannot get it back.
// Here, on an error the front byte is discarded and evaluation restarts from the
// next position -- so a noise byte costs at most one packet.
class Reader {
 public:
  // true as soon as a complete, checksum-verified packet is available.
  // payload() stays valid until the next feed().
  bool feed(uint8_t b);

  const uint8_t* payload() const { return m_rx + m_plOff; }
  size_t payloadLen() const { return m_plLen; }

  void reset();

  // Discarded bytes since start -- a measure of line quality, for the log.
  uint32_t dropped() const { return m_dropped; }

 private:
  enum class Try : uint8_t { INCOMPLETE, BAD, GOOD };

  Try tryDecode(size_t& frameLen) const;
  void dropFront(size_t k);

  uint8_t  m_rx[FRAME_MAX] = {0};
  size_t   m_n = 0;          // bytes in the buffer
  size_t   m_plOff = 0;      // start of the payload in the buffer
  size_t   m_plLen = 0;      // length of the payload
  size_t   m_pending = 0;    // frame length to discard after success
  uint32_t m_dropped = 0;
};

}  // namespace vesc
