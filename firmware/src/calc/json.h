#pragma once
// JSON string literal with quotes: " and \ escaped, line break to
// space. Writes at most n bytes including the null byte and returns the length.
#include <cstddef>

namespace calc {

inline size_t jsonStr(char* b, size_t n, const char* s) {
  size_t p = 0;
  if (p < n) b[p++] = '"';
  for (; *s && p + 3 < n; s++) {
    if (*s == '"' || *s == '\\') b[p++] = '\\';
    if (*s == '\n') { b[p++] = ' '; continue; }
    b[p++] = *s;
  }
  if (p < n) b[p++] = '"';
  if (p < n) b[p] = '\0';
  return p;
}

}  // namespace calc
