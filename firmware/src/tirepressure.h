#pragma once
//
// Reifendruck und Reifentemperatur: SKS AIRSPY ueber BLE, Handeingabe als Rueckfall.
//
// Protokoll aus dem Schwesterprojekt ~/git/airspy (docs/20-protocol.md, dort gegen
// den Sensor gemessen): Name "AIRSPY-<Seriennummer>", Dienst 0xFFF0, Characteristic
// 0xFFF4 streamt unaufgefordert mit 2 Hz; je Notification 15 Byte, float32 an Offset 8
// und das Feldkennzeichen in Byte 14 (0x2E Druck kPa), in der zweiten Haelfte
// Temperatur °C (Offset 0, Kennzeichen Byte 6 = 0x22) und Batterie % (Offset 7).
// Konstanten in config.h (AIRSPY_*).
//
// AM STAND GIBT ES GENAU EINEN SENSOR: der erste gefundene "AIRSPY-*" wird genommen.
// Scannen, Verbinden und Abonnieren laufen in einer EIGENEN TASK auf Kern 0 -- die
// NimBLE-Aufrufe blockieren bis zu Sekunden, und die Hauptschleife muss alle 150 ms
// den Sollwert an den Regler schicken (VESC_ALIVE_MS). Werte kommen als 32-Bit-Floats
// herueber, deren Schreiben auf dem ESP32 atomar ist.
//
// Der Sensor schlaeft rund 10 Minuten nach dem letzten Bewegen ein und sendet dann
// nichts mehr: vor dem Lauf das Rad einmal drehen. Ein Wert, der aelter als
// AIRSPY_STALE_MS ist, gilt als alt (fresh() == false), bleibt aber lesbar.
//
// [INFERRED] Der Sensor liefert Relativdruck (Manometer). Der absolute Offset ist im
// Protokollprojekt noch offen (§3.4) -- deshalb bleibt UNC_P_REL im Budget.
//
#include <Arduino.h>

namespace tirepressure {

enum Source : uint8_t { SRC_NONE = 0, SRC_MANUAL, SRC_BLE };

void begin();
void service();

float bar();          // Druck [bar]; NAN, solange nichts gesetzt wurde. BLE vor Handeingabe.
float tempC();        // Reifen-/Sensortemperatur [°C], nur BLE; sonst NAN
float battery();      // Batterie des Sensors [%], nur BLE; sonst NAN
Source source();
const char* sourceName();   // "none" | "manual" | "ble"
const char* name();         // "AIRSPY-14203" oder ""
bool connected();           // Verbindung steht und Notifications sind abonniert
bool fresh();               // letzter Druckwert juenger als AIRSPY_STALE_MS
uint32_t ageMs();           // Alter des letzten Druckwerts, UINT32_MAX ohne Wert

void setManual(float bar);  // Handeingabe; gilt, solange kein frischer BLE-Wert da ist

}  // namespace tirepressure
