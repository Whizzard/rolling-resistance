#pragma once
// ZIP archive without compression ("stored"): local file headers, central directory,
// end record. Nothing more is needed to deliver report and data of a run as ONE
// file (/runs/<id>.zip). The headers are built here, the stream from the files
// comes from the caller (webui.cpp). Arduino-free, host test in
// firmware/test/test_calc.
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace calc {

inline void le16(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
inline void le32(uint8_t* p, uint32_t v) { le16(p, v & 0xFFFF); le16(p + 2, v >> 16); }

constexpr uint16_t ZIP_DOS_DATE_1980 = 0x0021;   // 1980-01-01: the earliest valid date
constexpr size_t   ZIP_LOCAL_FIXED = 30, ZIP_CENTRAL_FIXED = 46, ZIP_END_FIXED = 22;

inline size_t zipLocalHeader(uint8_t* out, const char* name, uint32_t crc, uint32_t size,
                             uint16_t dosTime = 0, uint16_t dosDate = ZIP_DOS_DATE_1980) {
  const uint16_t n = static_cast<uint16_t>(std::strlen(name));
  le32(out, 0x04034b50u); le16(out + 4, 20); le16(out + 6, 0); le16(out + 8, 0);
  le16(out + 10, dosTime); le16(out + 12, dosDate);
  le32(out + 14, crc); le32(out + 18, size); le32(out + 22, size);
  le16(out + 26, n); le16(out + 28, 0);
  std::memcpy(out + 30, name, n);
  return ZIP_LOCAL_FIXED + n;
}

inline size_t zipCentralHeader(uint8_t* out, const char* name, uint32_t crc, uint32_t size, uint32_t localOffset,
                               uint16_t dosTime = 0, uint16_t dosDate = ZIP_DOS_DATE_1980, uint16_t flags = 0) {
  const uint16_t n = static_cast<uint16_t>(std::strlen(name));
  le32(out, 0x02014b50u); le16(out + 4, 20); le16(out + 6, 20); le16(out + 8, flags); le16(out + 10, 0);
  le16(out + 12, dosTime); le16(out + 14, dosDate);
  le32(out + 16, crc); le32(out + 20, size); le32(out + 24, size);
  le16(out + 28, n); le16(out + 30, 0); le16(out + 32, 0); le16(out + 34, 0); le16(out + 36, 0);
  le32(out + 38, 0); le32(out + 42, localOffset);
  std::memcpy(out + 46, name, n);
  return ZIP_CENTRAL_FIXED + n;
}

inline size_t zipEndRecord(uint8_t* out, uint16_t entries, uint32_t cdSize, uint32_t cdOffset) {
  le32(out, 0x06054b50u); le16(out + 4, 0); le16(out + 6, 0);
  le16(out + 8, entries); le16(out + 10, entries);
  le32(out + 12, cdSize); le32(out + 16, cdOffset); le16(out + 20, 0);
  return ZIP_END_FIXED;
}

// STREAMED entries (bit 3, "data descriptor"): CRC and size come only AFTER
// the data. For content that is only created while sending (raw data -> CSV, webui.cpp):
// one pass instead of two, but without Content-Length (HTTP chunked).
constexpr uint16_t ZIP_FLAG_DESCRIPTOR = 0x0008;
constexpr size_t   ZIP_DESCRIPTOR = 16;

inline size_t zipLocalHeaderStreamed(uint8_t* out, const char* name) {
  const size_t h = zipLocalHeader(out, name, 0, 0);
  le16(out + 6, ZIP_FLAG_DESCRIPTOR);
  return h;
}
inline size_t zipDataDescriptor(uint8_t* out, uint32_t crc, uint32_t size) {
  le32(out, 0x08074b50u); le32(out + 4, crc); le32(out + 8, size); le32(out + 12, size);
  return ZIP_DESCRIPTOR;
}

// Total size of the archive, for Content-Length before the first byte.
inline uint32_t zipArchiveSize(const uint32_t* sizes, const size_t* nameLens, size_t n) {
  uint32_t total = ZIP_END_FIXED;
  for (size_t i = 0; i < n; i++) total += ZIP_LOCAL_FIXED + ZIP_CENTRAL_FIXED + 2 * nameLens[i] + sizes[i];
  return total;
}

}  // namespace calc
