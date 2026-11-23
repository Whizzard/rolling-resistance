#include "logger.h"
#include "config.h"
#include "settings.h"
#include "torque.h"

namespace logger {
namespace {

void printFloat(float v, uint8_t decimals) {
  if (isnan(v)) Serial.print("");   // leeres Feld statt erfundener Zahl
  else Serial.print(v, decimals);
}

}  // namespace

void begin() { Serial.begin(115200); }

void header() {
  Serial.println();
  Serial.println("# Fahrrad-Reifenpruefstand — Rohdaten");
  Serial.print("# U=");  Serial.print(settings::drumCircumferenceM(), 4);
  Serial.print(" m  r="); Serial.print(settings::drumRadiusM(), 4);
  Serial.print(" m  L="); Serial.print(settings::torqueArmM(), 4);
  Serial.print(" m  i="); Serial.print(BELT_RATIO, 3);
  Serial.print("  N_per_count="); Serial.println(settings::loadcellNPerCount(), 8);
  if (!isCalibrated()) {
    Serial.println("# WARNUNG: Waegezelle unkalibriert — Kraftspalten bleiben leer.");
  }
  Serial.println("t_ms,state,rev,v_kmh,f_raw,f_N,m_Nm,p_W,crr,load_N,p_tire_bar,note");
}

void point(const char* stateName, uint32_t rev, float v_kmh, float raw, float f_N,
           float m_Nm, float p_W, float crr, float load_N, float bar,
           const char* noteText) {
  Serial.print(millis());      Serial.print(',');
  Serial.print(stateName);     Serial.print(',');
  Serial.print(rev);           Serial.print(',');
  printFloat(v_kmh, 2);        Serial.print(',');
  printFloat(raw, 1);          Serial.print(',');
  printFloat(f_N, 4);          Serial.print(',');
  printFloat(m_Nm, 5);         Serial.print(',');
  printFloat(p_W, 3);          Serial.print(',');
  printFloat(crr, 6);          Serial.print(',');
  printFloat(load_N, 2);       Serial.print(',');
  printFloat(bar, 2);          Serial.print(',');
  Serial.println(noteText ? noteText : "");
}

void angleBins() {
  Serial.print("# bins_raw");
  for (uint8_t i = 0; i < torque::binCount(); i++) {
    Serial.print(',');
    const float v = torque::binAverageRaw(i);
    if (!isnan(v)) Serial.print(v, 1);
  }
  Serial.println();
}

namespace { char g_last[96] = ""; }

void note(const char* text) {
  Serial.print("# ");
  Serial.println(text);
  strncpy(g_last, text, sizeof(g_last) - 1);
  g_last[sizeof(g_last) - 1] = '\0';
}

const char* lastNote() { return g_last; }

}  // namespace logger
