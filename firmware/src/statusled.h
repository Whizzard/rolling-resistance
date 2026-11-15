#pragma once
//
// Status-LED des DevKits — adressierbare RGB-LED (WS2812) an PIN_STATUS_LED.
//
// Beim Booten läuft genau einmal ein Regenbogen durch, danach bleibt die LED aus.
// Zweck ist ein Lebenszeichen beim Einschalten: man sieht ohne Konsole, dass der
// Chip läuft und die Firmware angekommen ist. Im Messbetrieb leuchtet nichts —
// eine mitlaufende LED am Prüfstand wäre nur Ablenkung.
//
#include <Arduino.h>

namespace statusled {

void begin();    // LED aus, Startzeitpunkt merken
void service();  // zyklisch aufrufen; nicht blockierend
bool busy();     // true, solange der Regenbogen noch läuft

}  // namespace statusled
