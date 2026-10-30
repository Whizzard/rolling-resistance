#pragma once
//
// HD44780 4×20 über PCF8574-I²C-Backpack.
// Eigene Minimalimplementierung, damit der Standard-Build ohne externe
// Bibliotheken auskommt.
//
#include <Arduino.h>

namespace display {

enum Page : uint8_t { PAGE_LIVE = 0, PAGE_MEASURE, PAGE_SETUP, PAGE_DIAG, PAGE_COUNT };

void begin();
bool present();          // false, wenn unter LCD_I2C_ADDR niemand antwortet
void setPage(Page p);
void nextPage();
Page page();

// Zyklisch aufrufen; aktualisiert höchstens alle 250 ms, damit das LCD den Bus
// nicht blockiert.
void service();

// Freitext in eine Zeile (0..3), wird bei der nächsten Seitenaktualisierung
// überschrieben.
void line(uint8_t row, const char* text);

}  // namespace display
