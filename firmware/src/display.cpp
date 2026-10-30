#include "display.h"
#include <Wire.h>
#include "config.h"
#include "speed.h"
#include "torque.h"
#include "drive.h"
#include "state.h"
#include "tirepressure.h"

namespace display {
namespace {

// PCF8574-Bitbelegung der üblichen Backpacks:
// P0=RS  P1=RW  P2=EN  P3=Backlight  P4..P7=D4..D7
constexpr uint8_t BIT_RS = 0x01;
constexpr uint8_t BIT_EN = 0x04;
constexpr uint8_t BIT_BL = 0x08;

constexpr uint8_t ROW_OFFSET[4] = {0x00, 0x40, 0x14, 0x54};

bool g_present = false;
Page g_page = PAGE_LIVE;
uint32_t g_lastMs = 0;

void expanderWrite(uint8_t data) {
  Wire.beginTransmission(LCD_I2C_ADDR);
  Wire.write(data | BIT_BL);
  Wire.endTransmission();
}

void pulseEnable(uint8_t data) {
  expanderWrite(data | BIT_EN);
  delayMicroseconds(1);
  expanderWrite(data & ~BIT_EN);
  delayMicroseconds(50);
}

void write4(uint8_t nibble) {
  expanderWrite(nibble);
  pulseEnable(nibble);
}

void send(uint8_t value, uint8_t mode) {
  write4((value & 0xF0) | mode);
  write4(((value << 4) & 0xF0) | mode);
}

void command(uint8_t c) { send(c, 0); }
void writeChar(uint8_t c) { send(c, BIT_RS); }

void setCursor(uint8_t col, uint8_t row) {
  if (row >= LCD_ROWS) row = LCD_ROWS - 1;
  command(0x80 | (col + ROW_OFFSET[row]));
}

void printPadded(const char* s) {
  uint8_t n = 0;
  while (*s && n < LCD_COLS) { writeChar(*s++); n++; }
  while (n < LCD_COLS) { writeChar(' '); n++; }
}

void row(uint8_t r, const char* s) {
  if (!g_present) return;
  setCursor(0, r);
  printPadded(s);
}

void renderLive() {
  char buf[24];
  snprintf(buf, sizeof(buf), "v %5.1f km/h", speed::kmh());
  row(0, buf);

  if (isCalibrated()) {
    snprintf(buf, sizeof(buf), "P %6.1f W", torque::powerW(speed::omega()));
    row(1, buf);
    snprintf(buf, sizeof(buf), "F %6.2f N", torque::rollForceN());
    row(2, buf);
  } else {
    row(1, "P    --  unkalib.");
    snprintf(buf, sizeof(buf), "raw %ld", torque::lastRaw());
    row(2, buf);
  }

  snprintf(buf, sizeof(buf), "%-10s U%lu", state::name(),
           static_cast<unsigned long>(speed::revCount()));
  row(3, buf);
}

void renderMeasure() {
  char buf[24];
  snprintf(buf, sizeof(buf), "Plateau %u/%u", state::plateauIndex() + 1, PLATEAU_COUNT);
  row(0, buf);
  snprintf(buf, sizeof(buf), "soll %4.1f ist %4.1f", drive().targetKmh(), speed::kmh());
  row(1, buf);
  snprintf(buf, sizeof(buf), "Umdr %u/%u", state::revsInPoint(), AVG_REVS_PER_POINT);
  row(2, buf);
  row(3, state::name());
}

void renderSetup() {
  char buf[24];
  snprintf(buf, sizeof(buf), "Last  %6.1f N", state::loadN());
  row(0, buf);
  snprintf(buf, sizeof(buf), "Druck %5.2f bar", tirepressure::bar());
  row(1, buf);
  snprintf(buf, sizeof(buf), "Arm %.3f m i %.2f", TORQUE_ARM_M, BELT_RATIO);
  row(2, buf);
  row(3, isCalibrated() ? "Zelle kalibriert" : "Zelle UNKALIBRIERT");
}

void renderDiag() {
  char buf[24];
  snprintf(buf, sizeof(buf), "Trommel %5.2f 1/s", speed::revPerSec());
  row(0, buf);
  snprintf(buf, sizeof(buf), "Motor   %5.2f 1/s", speed::motorRevPerSec());
  row(1, buf);
  snprintf(buf, sizeof(buf), "Schlupf %s", speed::slipDetected() ? "JA" : "nein");
  row(2, buf);
  snprintf(buf, sizeof(buf), "Antrieb %s", drive().name());
  row(3, buf);
}

}  // namespace

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.beginTransmission(LCD_I2C_ADDR);
  g_present = (Wire.endTransmission() == 0);
  if (!g_present) return;

  // HD44780-Initialisierung im 4-Bit-Modus
  delay(50);
  expanderWrite(0x00);
  delay(10);
  write4(0x30); delay(5);
  write4(0x30); delayMicroseconds(150);
  write4(0x30); delayMicroseconds(150);
  write4(0x20); delayMicroseconds(150);   // ab hier 4 Bit

  command(0x28);  // 4 Bit, 2 Zeilen (gilt auch für 4×20), 5x8
  command(0x0C);  // Display an, Cursor aus
  command(0x06);  // Eingabemodus: inkrementieren
  command(0x01);  // löschen
  delay(2);
}

bool present() { return g_present; }
void setPage(Page p) { g_page = p; }
void nextPage() { g_page = static_cast<Page>((g_page + 1) % PAGE_COUNT); }
Page page() { return g_page; }

void line(uint8_t r, const char* text) { row(r, text); }

void service() {
  if (!g_present) return;
  const uint32_t now = millis();
  if (now - g_lastMs < 250) return;
  g_lastMs = now;

  switch (g_page) {
    case PAGE_LIVE:    renderLive(); break;
    case PAGE_MEASURE: renderMeasure(); break;
    case PAGE_SETUP:   renderSetup(); break;
    case PAGE_DIAG:    renderDiag(); break;
    default: break;
  }
}

}  // namespace display
