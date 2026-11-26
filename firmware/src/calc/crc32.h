#pragma once
// CRC-32 (IEEE 802.3, reflected, 0xEDB88320), as ZIP and zlib need it.
// Bitwise without a table: the run files are small, and this way it fits into the
// Arduino-free calculation core (host test against the catalogue value 0xCBF43926).
#include <cstddef>
#include <cstdint>

namespace calc {

// crc = 0 at the start; continue with the return value of the previous call.
inline uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n) {
  crc = ~crc;
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

}  // namespace calc
