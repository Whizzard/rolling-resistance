#include "tirepressure.h"

namespace tirepressure {
namespace {
float g_bar = NAN;
Source g_src = SRC_NONE;
}  // namespace

void begin() {
  // TODO BLE: sobald das Sensormodell feststeht, hier scannen und verbinden.
  // Vorgehen zur Ermittlung der UUIDs: mit einem BLE-Scanner die Services und
  // Characteristics des Sensors auslesen und in docs/04-hardware.md eintragen.
  // Bis dahin bleibt es bei der Handeingabe — ein geratenes Profil wäre wertlos.
}

void service() {
  // TODO BLE: Notifications verarbeiten und g_bar/g_src setzen.
}

float bar() { return g_bar; }
Source source() { return g_src; }

const char* sourceName() {
  switch (g_src) {
    case SRC_MANUAL: return "manual";
    case SRC_BLE:    return "ble";
    default:         return "none";
  }
}

void setManual(float b) {
  g_bar = b;
  g_src = SRC_MANUAL;
}

}  // namespace tirepressure
