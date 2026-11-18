#include "settings.h"
#include "config.h"
#include <Preferences.h>

namespace settings {
namespace {

Preferences g_nvs;

// Schluessel sind auf 15 Zeichen begrenzt (NVS).
constexpr char NS[]      = "pruefstand";
constexpr char K_DRUM[]  = "drum_u";
constexpr char K_ARM[]   = "arm";
constexpr char K_CALIB[] = "calib_arm";
constexpr char K_NPC[]   = "n_per_count";
constexpr char K_DIR[]   = "dir_sign";

float g_drum  = DRUM_CIRCUMFERENCE_M;
float g_arm   = TORQUE_ARM_M;
float g_cal   = CALIB_ARM_M;
float g_npc   = LOADCELL_N_PER_COUNT;
int8_t g_dir  = static_cast<int8_t>(VESC_DIRECTION_SIGN);

// Grenzen der Plausibilitaet. Absichtlich weit: sie sollen Tippfehler um
// Zehnerpotenzen abfangen, nicht die Konstruktion bevormunden.
inline bool okDrum(float m)  { return m > 0.30f && m < 6.00f; }
inline bool okArm(float m)   { return m > 0.005f && m < 1.00f; }
inline bool okNpc(float x)   { return x > 0.0f && x < 10.0f; }

}  // namespace

void begin() {
  // Beim allerersten Start gibt es den Namensraum noch nicht, und ein nur lesendes
  // Oeffnen meldet dann "nvs_open failed: NOT_FOUND" als FEHLER auf die Konsole.
  // Abgefangen wird das zwar, aber eine rote Fehlerzeile beim Start, die gar keiner
  // ist, kostet beim naechsten Mal Zeit mit Suchen. Deshalb einmal schreibend
  // oeffnen: das legt den Namensraum an, ohne etwas hineinzuschreiben.
  if (g_nvs.begin(NS, false)) g_nvs.end();

  // Nur lesend oeffnen und gleich wieder schliessen — der NVS-Handle soll nicht
  // ueber die ganze Laufzeit offen bleiben.
  if (g_nvs.begin(NS, true)) {
    g_drum = g_nvs.getFloat(K_DRUM,  DRUM_CIRCUMFERENCE_M);
    g_arm  = g_nvs.getFloat(K_ARM,   TORQUE_ARM_M);
    g_cal  = g_nvs.getFloat(K_CALIB, CALIB_ARM_M);
    g_npc  = g_nvs.getFloat(K_NPC,   LOADCELL_N_PER_COUNT);
    g_dir  = g_nvs.getChar(K_DIR, static_cast<int8_t>(VESC_DIRECTION_SIGN));
    g_nvs.end();
  }
  // Ein im NVS gelandeter Unsinnswert darf den Stand nicht vergiften.
  if (!okDrum(g_drum)) g_drum = DRUM_CIRCUMFERENCE_M;
  if (!okArm(g_arm))   g_arm  = TORQUE_ARM_M;
  if (!okArm(g_cal))   g_cal  = CALIB_ARM_M;
  if (g_npc != 0.0f && !okNpc(g_npc)) g_npc = LOADCELL_N_PER_COUNT;
  if (g_dir != 1 && g_dir != -1) g_dir = static_cast<int8_t>(VESC_DIRECTION_SIGN);

  Serial.print("# Einstellungen: Trommel "); Serial.print(g_drum, 4);
  Serial.print(" m, Zellenarm ");            Serial.print(g_arm, 4);
  Serial.print(" m, Kalibrierarm ");         Serial.print(g_cal, 4);
  Serial.print(" m, Drehrichtung ");         Serial.print(g_dir);
  Serial.print(", Kalibrierfaktor ");
  if (g_npc > 0.0f) Serial.println(g_npc, 8);
  else              Serial.println("NICHT KALIBRIERT");
}

float drumCircumferenceM() { return g_drum; }
float drumRadiusM()        { return g_drum / (2.0f * PI); }
float torqueArmM()         { return g_arm; }
float calibArmM()          { return g_cal; }
float loadcellNPerCount()  { return g_npc; }
bool  calibrated()         { return g_npc > 0.0f; }
int8_t directionSign()     { return g_dir; }

namespace {
bool store(const char* key, float value) {
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putFloat(key, value);
  g_nvs.end();
  return n > 0;
}
}  // namespace

bool setDrumCircumferenceM(float m) {
  if (!okDrum(m)) return false;
  g_drum = m;
  return store(K_DRUM, m);
}

bool setTorqueArmM(float m) {
  if (!okArm(m)) return false;
  g_arm = m;
  return store(K_ARM, m);
}

bool setCalibArmM(float m) {
  if (!okArm(m)) return false;
  g_cal = m;
  return store(K_CALIB, m);
}

bool setLoadcellNPerCount(float x) {
  if (!okNpc(x)) return false;
  g_npc = x;
  return store(K_NPC, x);
}

bool setDirectionSign(int8_t sign) {
  if (sign != 1 && sign != -1) return false;
  g_dir = sign;
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putChar(K_DIR, sign);
  g_nvs.end();
  return n > 0;
}

void reset() {
  if (g_nvs.begin(NS, false)) {
    g_nvs.clear();
    g_nvs.end();
  }
  g_drum = DRUM_CIRCUMFERENCE_M;
  g_arm  = TORQUE_ARM_M;
  g_cal  = CALIB_ARM_M;
  g_npc  = LOADCELL_N_PER_COUNT;
  g_dir  = static_cast<int8_t>(VESC_DIRECTION_SIGN);
}

bool anyOverridden() {
  return g_drum != DRUM_CIRCUMFERENCE_M || g_arm != TORQUE_ARM_M ||
         g_cal  != CALIB_ARM_M          || g_npc != LOADCELL_N_PER_COUNT ||
         g_dir  != static_cast<int8_t>(VESC_DIRECTION_SIGN);
}

}  // namespace settings
