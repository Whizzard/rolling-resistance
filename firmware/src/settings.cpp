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
constexpr char K_PLAT[]  = "plateaus";
constexpr char K_TARE[]  = "tare";
constexpr char K_LOAD[]  = "load_n";

float g_drum  = DRUM_CIRCUMFERENCE_M;
float g_arm   = TORQUE_ARM_M;
float g_cal   = CALIB_ARM_M;
float g_npc   = LOADCELL_N_PER_COUNT;
int8_t g_dir  = static_cast<int8_t>(VESC_DIRECTION_SIGN);

long    g_tare  = 0;
float   g_load  = LOAD_DEFAULT_N;
float   g_plat[PLATEAU_MAX];
uint8_t g_platN = 0;

// Zwei Geschwindigkeiten gelten als dieselbe, wenn sie sich um weniger als
// 0,05 km/h unterscheiden — 28,8 soll nicht neben 28,80001 stehen.
inline bool samePlateau(float a, float b) { return fabsf(a - b) < 0.05f; }
inline bool okPlateau(float v) { return v >= V_MIN_KMH && v <= V_MAX_KMH; }

void defaultPlateaus() {
  g_platN = 0;
  for (uint8_t i = 0; i < PLATEAU_COUNT && i < PLATEAU_MAX; i++) g_plat[g_platN++] = PLATEAUS_KMH[i];
}

// Sortieren und Doppelte entfernen; Einfuegesortierung, die Liste ist kurz.
void tidyPlateaus() {
  for (uint8_t i = 1; i < g_platN; i++) {
    const float v = g_plat[i];
    int8_t j = i - 1;
    while (j >= 0 && g_plat[j] > v) { g_plat[j + 1] = g_plat[j]; j--; }
    g_plat[j + 1] = v;
  }
  uint8_t n = 0;
  for (uint8_t i = 0; i < g_platN; i++) {
    if (!okPlateau(g_plat[i])) continue;
    if (n > 0 && samePlateau(g_plat[n - 1], g_plat[i])) continue;
    g_plat[n++] = g_plat[i];
  }
  g_platN = n;
}

bool storePlateaus() {
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putBytes(K_PLAT, g_plat, g_platN * sizeof(float));
  g_nvs.end();
  return n == g_platN * sizeof(float);
}

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
  // Jeden Schluessel erst auf Existenz pruefen: ein getFloat auf einen
  // fehlenden Schluessel protokolliert "nvs_get_blob len fail ... NOT_FOUND"
  // als FEHLER auf die Konsole -- dieselbe falsche rote Zeile wie oben.
  if (g_nvs.begin(NS, true)) {
    if (g_nvs.isKey(K_DRUM))  g_drum = g_nvs.getFloat(K_DRUM,  DRUM_CIRCUMFERENCE_M);
    if (g_nvs.isKey(K_ARM))   g_arm  = g_nvs.getFloat(K_ARM,   TORQUE_ARM_M);
    if (g_nvs.isKey(K_CALIB)) g_cal  = g_nvs.getFloat(K_CALIB, CALIB_ARM_M);
    if (g_nvs.isKey(K_NPC))   g_npc  = g_nvs.getFloat(K_NPC,   LOADCELL_N_PER_COUNT);
    if (g_nvs.isKey(K_DIR))   g_dir  = g_nvs.getChar(K_DIR, static_cast<int8_t>(VESC_DIRECTION_SIGN));
    if (g_nvs.isKey(K_TARE))  g_tare = g_nvs.getLong(K_TARE, 0);
    if (g_nvs.isKey(K_LOAD))  g_load = g_nvs.getFloat(K_LOAD, LOAD_DEFAULT_N);
    const size_t bytes = g_nvs.isKey(K_PLAT) ? g_nvs.getBytesLength(K_PLAT) : 0;
    if (bytes > 0 && bytes % sizeof(float) == 0 && bytes <= sizeof(g_plat)) {
      g_nvs.getBytes(K_PLAT, g_plat, bytes);
      g_platN = bytes / sizeof(float);
      tidyPlateaus();
    }
    g_nvs.end();
  }
  if (g_platN == 0) defaultPlateaus();
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
  Serial.print("# Messgeschwindigkeiten:");
  for (uint8_t i = 0; i < g_platN; i++) { Serial.print(' '); Serial.print(g_plat[i], 1); }
  Serial.println(" km/h");
}

float loadN() { return g_load; }
bool setLoadN(float n) {
  if (!(n > 0.0f && n < 5000.0f)) return false;
  g_load = n;
  if (!g_nvs.begin(NS, false)) return false;
  const size_t k = g_nvs.putFloat(K_LOAD, n);
  g_nvs.end();
  return k > 0;
}

long tareOffset() { return g_tare; }

bool setTareOffset(long counts) {
  g_tare = counts;
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putLong(K_TARE, counts);
  g_nvs.end();
  return n > 0;
}

uint8_t plateauCount() { return g_platN; }
float plateauKmh(uint8_t i) { return i < g_platN ? g_plat[i] : 0.0f; }

bool addPlateau(float kmh) {
  if (!okPlateau(kmh) || g_platN >= PLATEAU_MAX) return false;
  for (uint8_t i = 0; i < g_platN; i++)
    if (samePlateau(g_plat[i], kmh)) return false;
  g_plat[g_platN++] = kmh;
  tidyPlateaus();
  return storePlateaus();
}

bool removePlateau(float kmh) {
  for (uint8_t i = 0; i < g_platN; i++) {
    if (!samePlateau(g_plat[i], kmh)) continue;
    for (uint8_t j = i; j + 1 < g_platN; j++) g_plat[j] = g_plat[j + 1];
    g_platN--;
    return storePlateaus();
  }
  return false;
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
  g_tare = 0;
  g_load = LOAD_DEFAULT_N;
  defaultPlateaus();
}

bool anyOverridden() {
  bool plat = g_platN != PLATEAU_COUNT;
  for (uint8_t i = 0; !plat && i < g_platN; i++)
    plat = !samePlateau(g_plat[i], PLATEAUS_KMH[i]);
  return g_drum != DRUM_CIRCUMFERENCE_M || g_arm != TORQUE_ARM_M ||
         g_cal  != CALIB_ARM_M          || g_npc != LOADCELL_N_PER_COUNT ||
         g_dir  != static_cast<int8_t>(VESC_DIRECTION_SIGN) || plat;
}

}  // namespace settings
