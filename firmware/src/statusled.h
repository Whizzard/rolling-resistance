#pragma once
//
// Status-LED des DevKits — adressierbare RGB-LED (WS2812) an PIN_STATUS_LED.
//
// Sie zeigt den Zustand der BEDIENVERBINDUNG, sonst nichts:
//   grün, stetig  — ein Bedienrechner meldet sich innerhalb LINK_TIMEOUT_MS
//   rot, blinkend — es wartet noch keiner, oder er ist weggeblieben
//
// Damit ist die Totmannschaltung aus drei Metern ablesbar, ohne auf einen Schirm
// zu sehen.
//
#include <Arduino.h>

namespace statusled {

void begin();    // LED aus
void service();  // zyklisch aufrufen; nicht blockierend
bool busy();     // immer false; bleibt für den Aufrufer erhalten

}  // namespace statusled
