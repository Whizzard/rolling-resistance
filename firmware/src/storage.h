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
// FORMATTING: the SD card also when it is readable (since 2026-11-26, wanted by the
// builder: a fresh FAT32 card), never while a run or a raw log uses it. The flash
// only out of UNREADABLE -- it holds the wheel profiles. The UI asks for
// confirmation; the console does not ask.
//
// FILE SYSTEMS: SD = FAT12/16/32 (ESP-IDF FatFs as built into the Arduino core
// 2.0.14: exFAT switched off, FF_FS_EXFAT 0). Flash = LittleFS.
//
// LAYOUT (since 2026-11-26), the same on both media:
//   /vmrr/<session>/            one directory per run or manual raw log;
//                               <session> = run id = local time YYYY-MM-DD-HHMMSS,
//                               "up<s>" while the clock is not set
//     raw_<session>.csv/.json/.md  raw log: stream, metadata, legend (SD only);
//                               raw_<session>-b.* ... after a name clash
//     protocol.md, points.csv   report and averaged points of a run
// Older files in /raw and /runs (before 2026-11-26) stay where they are.
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
  const char* fsType = "";        // "FAT32", "FAT16", "FAT12", "exFAT", "LittleFS"; "" unknown
};

constexpr const char* ROOT_DIR = "/vmrr";
// Name of a new session directory: local time YYYY-MM-DD-HHMMSS, "up<s>" without clock.
void sessionName(char* out, size_t n);
// mkdir for ROOT_DIR and one level below it; true if it exists afterwards.
bool makeDir(fs::FS& f, const char* path);

void begin();                     // before run::begin(): run.cpp writes to /runs
Info info(Media m);               // from any task
fs::FS* fs(Media m);              // nullptr while not READY/READONLY
const char* name(Media m);        // "sd" | "flash" (URL and JSON)
const char* stateName(State s);   // "absent" | "ready" | "read-only" | "unreadable"

// Request formatting (main loop or console). false + reason if refused.
bool requestFormat(Media m, const char** why);
const char* formatResult(Media m);
uint8_t  formatPct();    // SD format progress 0..99 while formatting (sectors written / expected)
uint32_t formatSec();    // seconds since the last SD format started
uint16_t sdMounts();   // counts every successful SD mount (also after formatting): "a new card file system"
uint16_t formatsDone(Media m);       // counts up when a format has finished (UI waits for it)   // last format: "" none/running, "ok", or why it failed

// Exclusive access to the SD for longer read streams (webui.cpp): the
// insertion check sends a command bypassing the FAT and must not land in the middle of
// a multi-block read.
void lockSd();
void sdBench();   // console Gb: raw sector read speed, past the file system
void unlockSd();

// Where a measurement run report goes: SD if ready, otherwise flash (decision
// 2026-11-26). Raw data goes ONLY to the SD (rawlog.h).
Media protocolMedia();

// Standstill, set from the main loop: only then is the flash usage
// computed and the flash formatted.
void setFlashWritable(bool ok);
bool flashWritable();

}  // namespace storage
