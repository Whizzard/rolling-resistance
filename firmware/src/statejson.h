#pragma once
// State as JSON for /api/state (web interface). A number or null, never an
// invented one; strings escaped. Part of main.cpp until 2026-11-25.
#include <Arduino.h>

namespace statejson {
void build(char* buf, size_t len);
}
