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
constexpr char K_A0[] = "a0"; constexpr char K_B0[] = "b0";
constexpr char K_A1[] = "a1"; constexpr char K_B1[] = "b1";
constexpr char K_MRAD[] = "m_rad";
constexpr char K_URAD[] = "u_rad";
constexpr char K_JRAD[] = "j_rad";       // gemessenes J_Rad, Bifilarpendel
constexpr char K_LD_EMPTY[] = "ld_empty"; constexpr char K_LD_WEIGHT[] = "ld_weight"; constexpr char K_LD_ARM[] = "ld_arm";
constexpr char K_RM_TIRE[] = "rm_tire"; constexpr char K_RM_WHEEL[] = "rm_wheel"; constexpr char K_RM_NOTE[] = "rm_note";
constexpr char K_RM_BAR[] = "rm_bar"; constexpr char K_RM_TEMP[] = "rm_temp"; constexpr char K_RM_LOAD[] = "rm_load"; constexpr char K_RM_POS[] = "rm_pos";
constexpr char K_ARMCAL[] = "arm_at_cal"; // Zellenarm bei der Kalibrierung

float g_drum  = DRUM_CIRCUMFERENCE_M;
float g_arm   = TORQUE_ARM_M;
float g_cal   = CALIB_ARM_M;
float g_npc   = LOADCELL_N_PER_COUNT;
int8_t g_dir  = static_cast<int8_t>(VESC_DIRECTION_SIGN);

long    g_tare  = 0;
float   g_load  = LOAD_DEFAULT_N;
float   g_a0 = NAN, g_b0 = NAN, g_a1 = NAN, g_b1 = NAN, g_mRad = WHEEL_MASS_DEFAULT_KG;
float   g_uRad = NAN;
float   g_jRad = NAN;
float   g_armCal = NAN;
RunMeta g_rm = {"", "", "", NAN, NAN, NAN, 0};
float   g_ldEmpty = LOAD_EMPTY_KG, g_ldWeight = LOAD_WEIGHT_KG, g_ldArm = LOAD_ARM_A_M;
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
    if (g_nvs.isKey(K_A0)) g_a0 = g_nvs.getFloat(K_A0, NAN);
    if (g_nvs.isKey(K_B0)) g_b0 = g_nvs.getFloat(K_B0, NAN);
    if (g_nvs.isKey(K_A1)) g_a1 = g_nvs.getFloat(K_A1, NAN);
    if (g_nvs.isKey(K_B1)) g_b1 = g_nvs.getFloat(K_B1, NAN);
    if (g_nvs.isKey(K_MRAD)) g_mRad = g_nvs.getFloat(K_MRAD, NAN);
    if (g_nvs.isKey(K_URAD)) g_uRad = g_nvs.getFloat(K_URAD, NAN);
    if (g_nvs.isKey(K_JRAD)) g_jRad = g_nvs.getFloat(K_JRAD, NAN);
    if (g_nvs.isKey(K_ARMCAL)) g_armCal = g_nvs.getFloat(K_ARMCAL, NAN);
    if (g_nvs.isKey(K_LD_EMPTY))  g_ldEmpty  = g_nvs.getFloat(K_LD_EMPTY,  LOAD_EMPTY_KG);
    if (g_nvs.isKey(K_LD_WEIGHT)) g_ldWeight = g_nvs.getFloat(K_LD_WEIGHT, LOAD_WEIGHT_KG);
    if (g_nvs.isKey(K_LD_ARM))    g_ldArm    = g_nvs.getFloat(K_LD_ARM,    LOAD_ARM_A_M);
    if (g_nvs.isKey(K_RM_TIRE))  g_nvs.getString(K_RM_TIRE, g_rm.tire, sizeof(g_rm.tire));
    if (g_nvs.isKey(K_RM_WHEEL)) g_nvs.getString(K_RM_WHEEL, g_rm.wheel, sizeof(g_rm.wheel));
    if (g_nvs.isKey(K_RM_NOTE))  g_nvs.getString(K_RM_NOTE, g_rm.note, sizeof(g_rm.note));
    if (g_nvs.isKey(K_RM_BAR))   g_rm.bar = g_nvs.getFloat(K_RM_BAR, NAN);
    if (g_nvs.isKey(K_RM_TEMP))  g_rm.tempC = g_nvs.getFloat(K_RM_TEMP, NAN);
    if (g_nvs.isKey(K_RM_LOAD))  g_rm.loadKg = g_nvs.getFloat(K_RM_LOAD, NAN);
    if (g_nvs.isKey(K_RM_POS))   g_rm.posMm = g_nvs.getInt(K_RM_POS, 0);
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

namespace {
bool store2(const char* k1, float v1, const char* k2, float v2) {
  if (!g_nvs.begin(NS, false)) return false;
  const bool ok = g_nvs.putFloat(k1, v1) > 0 && g_nvs.putFloat(k2, v2) > 0;
  g_nvs.end();
  return ok;
}
bool store1(const char* k, float v) {
  if (!g_nvs.begin(NS, false)) return false;
  const bool ok = g_nvs.putFloat(k, v) > 0;
  g_nvs.end();
  return ok;
}
}  // namespace

// 0 = in dieser Sitzung nie gesetzt (settings.h: Alter dann unbekannt).
uint32_t g_a0SetMs = 0, g_a1SetMs = 0, g_tareSetMs = 0;
static uint32_t ageOf(uint32_t t) { return t ? millis() - t : UINT32_MAX; }
uint32_t baselineAgeMs()   { return ageOf(g_a0SetMs); }
uint32_t wheelModelAgeMs() { return ageOf(g_a1SetMs); }
uint32_t tareAgeMs()       { return ageOf(g_tareSetMs); }
float baselineA0() { return g_a0; }
float baselineB0() { return g_b0; }
bool  haveBaseline() { return !isnan(g_a0) && !isnan(g_b0); }
bool  setBaseline(float a0, float b0) {
  if (isnan(a0) || isnan(b0) || fabsf(a0) > 20.0f || fabsf(b0) > 0.1f) return false;
  g_a0 = a0; g_b0 = b0; g_a0SetMs = millis();
  return store2(K_A0, a0, K_B0, b0);
}
float wheelA1() { return g_a1; }
float wheelB1() { return g_b1; }
bool  haveWheelModel() { return !isnan(g_a1) && !isnan(g_b1); }
bool  setWheelModel(float a1, float b1) {
  if (isnan(a1) || isnan(b1) || fabsf(a1) > 5.0f || fabsf(b1) > 0.1f) return false;
  g_a1 = a1; g_b1 = b1; g_a1SetMs = millis();
  return store2(K_A1, a1, K_B1, b1);
}
float wheelMassKg() { return g_mRad; }
bool  setWheelMassKg(float kg) {
  if (!(kg > 0.3f && kg < 10.0f)) return false;
  g_mRad = kg;
  return store1(K_MRAD, kg);
}

float wheelJKgm2() { return g_jRad; }
bool  setWheelJKgm2(float j) {
  if (j == 0.0f) { g_jRad = NAN; if (!g_nvs.begin(NS, false)) return false; g_nvs.remove(K_JRAD); g_nvs.end(); return true; }
  if (!(j > 0.01f && j < 2.0f)) return false;
  g_jRad = j;
  return store1(K_JRAD, j);
}
float armAtCalM() { return g_armCal; }

float loadEmptyKg()  { return g_ldEmpty; }
float loadWeightKg() { return g_ldWeight; }
float loadArmM()     { return g_ldArm; }
bool setLoadEmptyKg(float kg)  { if (!(kg >= 0.0f && kg < 100.0f)) return false; g_ldEmpty = kg;  return store1(K_LD_EMPTY, kg); }
bool setLoadWeightKg(float kg) { if (!(kg >= 0.0f && kg < 100.0f)) return false; g_ldWeight = kg; return store1(K_LD_WEIGHT, kg); }
bool setLoadArmM(float m)      { if (!(m > 0.02f && m < 2.0f)) return false;      g_ldArm = m;    return store1(K_LD_ARM, m); }
float loadFromPosN(float posM) {
  if (isnan(posM) || posM < 0.0f || posM > 3.0f || g_ldArm <= 0.0f) return NAN;
  return (g_ldEmpty + g_ldWeight * posM / g_ldArm) * 9.80665f;
}

const RunMeta& runMeta() { return g_rm; }
void setRunMeta(const RunMeta& m) {
  g_rm = m;
  if (!g_nvs.begin(NS, false)) return;
  g_nvs.putString(K_RM_TIRE, m.tire); g_nvs.putString(K_RM_WHEEL, m.wheel); g_nvs.putString(K_RM_NOTE, m.note);
  g_nvs.putFloat(K_RM_BAR, m.bar); g_nvs.putFloat(K_RM_TEMP, m.tempC); g_nvs.putFloat(K_RM_LOAD, m.loadKg);
  g_nvs.putInt(K_RM_POS, m.posMm);
  g_nvs.end();
}

float wheelCircM() { return g_uRad; }
bool  setWheelCircM(float m) {
  if (!(m > 0.5f && m < 3.0f)) return false;
  g_uRad = m;
  return store1(K_URAD, m);
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
  g_tare = counts; g_tareSetMs = millis();
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
  g_armCal = g_arm;   // der Faktor gilt fuer DIESEN Zellenarm
  store(K_ARMCAL, g_arm);
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
  g_a0 = g_b0 = g_a1 = g_b1 = NAN;
  g_mRad = WHEEL_MASS_DEFAULT_KG;
  g_uRad = NAN;
  g_jRad = NAN;
  g_armCal = NAN;
  g_rm = RunMeta{"", "", "", NAN, NAN, NAN, 0};
  g_ldEmpty = LOAD_EMPTY_KG; g_ldWeight = LOAD_WEIGHT_KG; g_ldArm = LOAD_ARM_A_M;
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
