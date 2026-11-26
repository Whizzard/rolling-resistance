#pragma once
//
// STORAGE: microSD (SPI2, FAT) and the internal flash (LittleFS), since 2026-11-25.
//
// Both are managed by ONE task on core 0 (next to WLAN and web server): it
// mounts, checks every STORAGE_CHECK_MS whether the card is still inserted, recomputes
// the usage, formats on request and writes the raw data (rawlog.h).
// The main loop touches no file system, except run.cpp at the end of a run.
//
// THE FLASH ONLY TAKES WHAT IS WRITTEN RARELY AND AT STANDSTILL (since 2026-11-26):
// settings and calibration (NVS, settings.h) and -- without SD -- the reports
// at the end of a run. Every write or erase in the flash locks the sensor ISRs on
// both cores (config.h, RAWLOG_FLASH_IDLE_MS). Never streaming data.
//
// STATES per medium (for UI and console):
//   ABSENT       no card (flash: always present, never ABSENT)
//   READY        mounted and writable
//   READONLY     mounted, write probe failed (card locked or worn out)
//   UNREADABLE   card answers, but no readable file system (unformatted,
//                exFAT, corrupted). Flash: LittleFS cannot be mounted.
//
// FORMATTING only out of UNREADABLE -- a readable file system is never erased
// from here. The UI asks for confirmation; the console does not ask.
// Flash only at standstill (config.h RAWLOG_FLASH_IDLE_MS: erasing locks the ISRs).
//
#include <Arduino.h>
#include <FS.h>

namespace storage {

enum Media : uint8_t { SD_CARD = 0, FLASH, MEDIA_N };
enum State : uint8_t { ABSENT = 0, READY, READONLY, UNREADABLE };

struct Info {
  State    state = ABSENT;
  uint64_t total = 0, used = 0;   // bytes; 0/0 while not mounted
  int8_t   pct   = -1;            // usage in %, -1 unknown
  bool     formatting = false;
};

void begin();                     // before run::begin(): run.cpp writes to /runs
Info info(Media m);               // from any task
fs::FS* fs(Media m);              // nullptr while not READY/READONLY
const char* name(Media m);        // "sd" | "flash" (URL and JSON)
const char* stateName(State s);   // "absent" | "ready" | "read-only" | "unreadable"

// Request formatting (main loop or console). false + reason if refused.
bool requestFormat(Media m, const char** why);

// Exclusive access to the SD for longer read streams (webui.cpp): the
// insertion check sends a command bypassing the FAT and must not land in the middle of
// a multi-block read.
void lockSd();
void unlockSd();

// Where a measurement run report goes: SD if ready, otherwise flash (decision
// 2026-11-26). Raw data goes ONLY to the SD (rawlog.h).
Media protocolMedia();

// Standstill, set from the main loop: only then is the flash usage
// computed and the flash formatted.
void setFlashWritable(bool ok);
bool flashWritable();

}  // namespace storage
