#include "packet.h"

namespace vesc {

// CRC16-CCITT/XMODEM, bitwise instead of by table: a 512-byte table saves nothing
// worth mentioning here (a packet rarely has more than 80 bytes), and bitwise is
// verifiable without having to trust a table.
uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0x0000;
  for (size_t i = 0; i < len; i++) {
    crc ^= static_cast<uint16_t>(data[i]) << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
  }
  return crc;
}

size_t frame(const uint8_t* payload, size_t len, uint8_t* out, size_t outCap) {
  if (payload == nullptr || out == nullptr) return 0;
  if (len == 0 || len > PAYLOAD_MAX) return 0;

  // Short form wherever it suffices -- otherwise the controller rejects the long form.
  const bool longForm = (len > 255);
  const size_t total = len + (longForm ? 6 : 5);
  if (outCap < total) return 0;

  size_t i = 0;
  if (longForm) {
    out[i++] = 0x03;
    out[i++] = static_cast<uint8_t>(len >> 8);
    out[i++] = static_cast<uint8_t>(len & 0xFF);
  } else {
    out[i++] = 0x02;
    out[i++] = static_cast<uint8_t>(len);
  }

  for (size_t k = 0; k < len; k++) out[i++] = payload[k];

  const uint16_t crc = crc16(payload, len);
  out[i++] = static_cast<uint8_t>(crc >> 8);
  out[i++] = static_cast<uint8_t>(crc & 0xFF);
  out[i++] = 0x03;
  return i;
}

// ---------------------------------------------------------------------- Reader

void Reader::reset() {
  m_n = 0;
  m_plOff = 0;
  m_plLen = 0;
  m_pending = 0;
}

void Reader::dropFront(size_t k) {
  if (k >= m_n) { m_n = 0; return; }
  for (size_t i = 0; i + k < m_n; i++) m_rx[i] = m_rx[i + k];
  m_n -= k;
}

// Evaluates the buffer from position 0 without changing it.
Reader::Try Reader::tryDecode(size_t& frameLen) const {
  if (m_n < 1) return Try::INCOMPLETE;

  // Check the start byte first: otherwise a single noise byte would remain as a
  // supposed frame start until a second one arrives.
  const bool longForm = (m_rx[0] == 0x03);
  if (m_rx[0] != 0x02 && !longForm) return Try::BAD;

  const size_t hdr = longForm ? 3 : 2;
  if (m_n < hdr) return Try::INCOMPLETE;

  const size_t len = longForm
      ? (static_cast<size_t>(m_rx[1]) << 8) | m_rx[2]
      : m_rx[1];

  if (len == 0 || len > PAYLOAD_MAX) return Try::BAD;
  if (longForm && len < 256) return Try::BAD;   // the controller rejects that as well

  const size_t total = hdr + len + 3;           // + crc[2] + end byte
  if (m_n < total) return Try::INCOMPLETE;

  if (m_rx[total - 1] != 0x03) return Try::BAD;

  const uint16_t crc = (static_cast<uint16_t>(m_rx[hdr + len]) << 8) | m_rx[hdr + len + 1];
  if (crc16(m_rx + hdr, len) != crc) return Try::BAD;

  frameLen = total;
  return Try::GOOD;
}

bool Reader::feed(uint8_t b) {
  // Discard the last reported frame only now -- this keeps payload() valid
  // between two feed() calls and it does not have to be copied.
  if (m_pending) {
    dropFront(m_pending);
    m_pending = 0;
    m_plLen = 0;
  }

  // Overflow can only occur when a plausible but wrong length waits for a frame
  // that never comes. Then sacrifice one byte at the front instead of getting stuck.
  if (m_n >= FRAME_MAX) {
    dropFront(1);
    m_dropped++;
  }
  m_rx[m_n++] = b;

  for (;;) {
    size_t frameLen = 0;
    switch (tryDecode(frameLen)) {
      case Try::INCOMPLETE:
        return false;

      case Try::BAD:
        dropFront(1);
        m_dropped++;
        if (m_n == 0) return false;
        continue;                       // retry from the next position

      case Try::GOOD:
        m_plOff   = (m_rx[0] == 0x03) ? 3 : 2;
        m_plLen   = frameLen - m_plOff - 3;
        m_pending = frameLen;
        return true;
    }
  }
}

}  // namespace vesc
