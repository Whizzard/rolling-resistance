#pragma once
// Unix time -> "YYYY-MM-DD HH:MM" or "YYYY-MM-DD-HHMM" (file name). Howard
// Hinnant's civil_from_days; the browser already sends the time as local time in
// seconds. t = 0 yields an em dash.
#include <cstdint>
#include <cstdio>

namespace calc {

inline void fmtTime(uint32_t t, char* out, size_t n, bool withTime) {
  if (!t) { snprintf(out, n, "\xe2\x80\x94"); return; }
  const uint32_t days = t / 86400, rem = t % 86400;
  const int64_t z = static_cast<int64_t>(days) + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t y = static_cast<int64_t>(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  const int64_t yy = y + (m <= 2 ? 1 : 0);
  if (withTime) snprintf(out, n, "%04lld-%02lu-%02lu %02lu:%02lu", static_cast<long long>(yy),
                         static_cast<unsigned long>(m), static_cast<unsigned long>(d),
                         static_cast<unsigned long>(rem / 3600), static_cast<unsigned long>(rem % 3600 / 60));
  else snprintf(out, n, "%04lld-%02lu-%02lu-%02lu%02lu", static_cast<long long>(yy),
                static_cast<unsigned long>(m), static_cast<unsigned long>(d),
                static_cast<unsigned long>(rem / 3600), static_cast<unsigned long>(rem % 3600 / 60));
}

}  // namespace calc
