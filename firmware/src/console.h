#pragma once
// Serial console and messages: command parser (the same vocabulary as the web
// UI, docs/10-firmware.md G5), help, status line, live telemetry, calibration
// command. And THE error text for the UI (currentError).
// Part of main.cpp until 2026-11-25.
#include <Arduino.h>

namespace console {

// note(): notice (console and UI, unobtrusive there). fail(): error --
// ONLY that one is shown by the UI in the message line, for five seconds.
void note(const char* s);
void fail(const char* s);
const char* lastNote();
bool lastNoteBad();
const char* currentError();      // text or "" -- order = importance

void handleCommand(const String& cmd);
void pollSerial();               // lines from the serial port to handleCommand
void printHelp();
void telemetry();                // command j<s>: one line every 200 ms

// Loop statistics from loop(), for status line and telemetry.
void loopRound(uint32_t dtMs, uint32_t vescAgeMs);
void loopSection(const char* name, uint32_t ms);

}  // namespace console
