#pragma once
//
// Tire pressure and tire temperature: SKS AIRSPY via BLE, manual entry as fallback.
//
// Protocol from the sister project ~/git/airspy (docs/20-protocol.md, measured against
// the sensor there): name "AIRSPY-<serial number>", service 0xFFF0, characteristic
// 0xFFF4 streams unsolicited at 2 Hz; 15 bytes per notification, float32 at offset 8
// and the field tag in byte 14 (0x2E pressure kPa), in the second half
// temperature °C (offset 0, tag byte 6 = 0x22) and battery % (offset 7).
// Constants in config.h (AIRSPY_*).
//
// THERE IS EXACTLY ONE SENSOR AT THE STAND: the first "AIRSPY-*" found is taken.
// Scanning, connecting and subscribing run in a TASK OF THEIR OWN on core 0 -- the
// NimBLE calls block for up to seconds, and the main loop must send the setpoint to
// the controller every 150 ms (VESC_ALIVE_MS). Values come across as 32-bit floats,
// whose writes are atomic on the ESP32.
//
// The sensor falls asleep about 10 minutes after it was last moved and then sends
// nothing: turn the wheel once before the run. A value older than
// AIRSPY_STALE_MS counts as old (fresh() == false), but stays readable.
//
// [INFERRED] The sensor delivers gauge pressure (manometer). The absolute offset is
// still open in the protocol project (§3.4) -- hence UNC_P_REL stays in the budget.
//
#include <Arduino.h>

namespace tirepressure {

enum Source : uint8_t { SRC_NONE = 0, SRC_MANUAL, SRC_BLE };

void begin();
void service();

float bar();          // pressure [bar]; NAN as long as nothing was set. BLE before manual entry.
float tempC();        // tire/sensor temperature [°C], BLE only; otherwise NAN
float battery();      // sensor battery [%], BLE only; otherwise NAN
Source source();
const char* sourceName();   // "none" | "manual" | "ble"
const char* name();         // "AIRSPY-14203" or ""
bool connected();           // connection up and notifications subscribed
bool fresh();               // last pressure value younger than AIRSPY_STALE_MS
uint32_t ageMs();           // age of the last pressure value, UINT32_MAX without a value

void setManual(float bar);  // manual entry; applies as long as no fresh BLE value is there

}  // namespace tirepressure
