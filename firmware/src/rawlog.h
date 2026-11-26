#pragma once
//
// RAW DATA RECORDING, since 2026-11-25. Format: calc/rawlog.h, docs/10-firmware.md §11.
//
// Everything the sensors deliver, timestamped in µs: every edge of the four
// Hall sensors (from the ISRs), every load cell value (80 SPS), every measurement packet
// of the controller (speed, tacho, currents, voltage, temperatures), the tire pressure, plus
// setpoint/duty/brake/phase on every change and the messages.
//
// WHERE: ONLY to the SD, into /raw/ (since 2026-11-26; until then the flash as fallback).
// Without a ready SD no recording: writing to the flash locks the sensor ISRs.
// DATA PATH: the main loop encodes into a ring in PSRAM (RAWLOG_BUF_BYTES),
// the storage task writes it out in RAWLOG_CHUNK_BYTES chunks. The main loop
// never waits for a file system.
// If the ring runs full, data is dropped and a GAP record with the byte count is entered.
//
// WHEN: by hand (console L1/L0, UI) or automatically while a
// guided run is active (run::active()).
//
#include <Arduino.h>
#include "storage.h"

namespace rawlog {

void begin();                        // from storage::begin()
void service(bool loadcellFresh);    // main loop, after torque::update()
void writerService();                // ONLY from the storage task
void mediaLost(storage::Media m);    // ONLY from the storage task

bool start();                        // by hand; false with reason in logger::lastNote()
void stop();
bool active();                       // requested or file open
bool manual();
bool draining();                     // stopped, ring still being written
storage::Media media();
const char* fileName();              // "/vmrr/<session>/raw_<session>.rrl" or ""
uint32_t bytesWritten();
uint32_t bytesPending();             // still in the ring
uint32_t bytesDropped();             // dropped since start

void text(const char* s);            // message as TEXT record (main loop)

}  // namespace rawlog
