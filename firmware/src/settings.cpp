#include "settings.h"
#include "config.h"
#include <Preferences.h>
#include "wallclock.h"

namespace settings {
namespace {

Preferences g_nvs;

// Keys are limited to 15 characters (NVS).
// Renamed from "pruefstand" on 2026-11-25; the values stored under it are gone with that
// (development, loss acceptable per the builder): redetermine calibration, circumference, models.
constexpr char NS[]      = "teststand";
constexpr char K_DRUM[]  = "drum_u";
constexpr char K_ARM[]   = "arm";
constexpr char K_CALIB[] = "calib_arm";
constexpr char K_NPC[]   = "n_per_count";
constexpr char K_NPC_T[] = "npc_epoch";   // local time of the calibration, 0 = unknown
uint32_t g_npcEpoch = 0;
constexpr char K_DIR[]   = "dir_sign";
constexpr char K_PLAT[]  = "plateaus";
constexpr char K_TARE[]  = "tare";
constexpr char K_LOAD[]  = "load_n";
constexpr char K_A0[] = "a0"; constexpr char K_B0[] = "b0";
constexpr char K_A1[] = "a1"; constexpr char K_B1[] = "b1";
constexpr char K_MWHEEL[] = "m_wheel";
constexpr char K_UWHEEL[] = "u_wheel";
constexpr char K_JWHEEL[] = "j_wheel";   // measured J_wheel, bifilar pendulum
constexpr char K_LD_EMPTY[] = "ld_empty"; constexpr char K_LD_WEIGHT[] = "ld_weight"; constexpr char K_LD_ARM[] = "ld_arm";
constexpr char K_RM_TIRE[] = "rm_tire"; constexpr char K_RM_WHEEL[] = "rm_wheel";   // holds the rim
constexpr char K_RM_HUB[] = "rm_hub"; constexpr char K_RM_NOTE[] = "rm_note";
constexpr char K_RM_ETRTO[] = "rm_etrto"; constexpr char K_RM_TUBE[] = "rm_tube";
constexpr char K_RM_BAR[] = "rm_bar"; constexpr char K_RM_TEMP[] = "rm_temp"; constexpr char K_RM_LOAD[] = "rm_load"; constexpr char K_RM_POS[] = "rm_pos";
constexpr char K_ARMCAL[] = "arm_at_cal"; // cell arm at calibration
constexpr char K_CTRLKG[] = "ctrl_kg";     // mass of the check weight (setup, 2026-11-25)
constexpr char K_WORG[] = "wm_org";        // origin of the wheel model (WheelOrigin as bytes, 2026-11-26)
constexpr uint8_t WORG_VER = 2;            // first byte of the blob; a different layout is ignored

// TIMESTAMP PER VALUE (since 2026-11-26): when it was last changed, local time of the
// browser in s (wallclock.h), in NVS under "t_<key>". For display in the
// setup. UINT32_MAX = never set, the default from config.h applies; 0 = set,
// but without a clock (console only, or saved before 2026-11-26).
const char* const STAMPED[] = {K_DRUM, K_ARM, K_CALIB, K_NPC, K_DIR, K_PLAT, K_TARE, K_LOAD,
                               K_A0, K_A1, K_MWHEEL, K_UWHEEL, K_JWHEEL, K_ARMCAL, K_CTRLKG,
                               K_LD_EMPTY, K_LD_WEIGHT, K_LD_ARM};
constexpr size_t STAMP_N = sizeof(STAMPED) / sizeof(STAMPED[0]);
uint32_t g_stamp[STAMP_N];

int stampIndex(const char* k) {
  for (size_t i = 0; i < STAMP_N; i++) if (strcmp(STAMPED[i], k) == 0) return static_cast<int>(i);
  return -1;
}
void stampKey(char* out, const char* k) { snprintf(out, 16, "t_%s", k); }   // NVS: at most 15 characters
// Call only with NVS open (for writing).
void stamp(const char* k) {
  const int i = stampIndex(k);
  if (i < 0) return;
  char tk[16]; stampKey(tk, k);
  g_stamp[i] = wallclock::now();
  g_nvs.putUInt(tk, g_stamp[i]);
}

float g_drum  = DRUM_CIRCUMFERENCE_M;
float g_arm   = TORQUE_ARM_M;
float g_cal   = CALIB_ARM_M;
float g_npc   = LOADCELL_N_PER_COUNT;
int8_t g_dir  = static_cast<int8_t>(VESC_DIRECTION_SIGN);

long    g_tare  = 0;
float   g_load  = LOAD_DEFAULT_N;
float   g_a0 = NAN, g_b0 = NAN, g_a1 = NAN, g_b1 = NAN, g_mWheel = WHEEL_MASS_DEFAULT_KG;
float   g_uWheel = NAN;
float   g_jWheel = NAN;
float   g_armCal = NAN;
float   g_ctrlKg = RUN_CTRL_MASS_KG;
RunMeta g_rm = {"", "", "", "", "", "", NAN, NAN, NAN, 0};
WheelOrigin g_wo = {};
float   g_ldEmpty = LOAD_EMPTY_KG, g_ldWeight = LOAD_WEIGHT_KG, g_ldArm = LOAD_ARM_A_M;
float   g_plat[PLATEAU_MAX];
uint8_t g_platN = 0;

// Two speeds count as the same if they differ by less than
// 0.05 km/h -- 28.8 should not stand next to 28.80001.
inline bool samePlateau(float a, float b) { return fabsf(a - b) < 0.05f; }
inline bool okPlateau(float v) { return v >= V_MIN_KMH && v <= V_MAX_KMH; }

void defaultPlateaus() {
  g_platN = 0;
  for (uint8_t i = 0; i < PLATEAU_COUNT && i < PLATEAU_MAX; i++) g_plat[g_platN++] = PLATEAUS_KMH[i];
}

// Sort and remove duplicates; insertion sort, the list is short.
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
  stamp(K_PLAT);
  g_nvs.end();
  return n == g_platN * sizeof(float);
}

// Plausibility limits. Deliberately wide: they are meant to catch typos by
// powers of ten, not to patronise the design.
inline bool okDrum(float m)  { return m > 0.30f && m < 6.00f; }
inline bool okArm(float m)   { return m > 0.005f && m < 1.00f; }
inline bool okNpc(float x)   { return x > 0.0f && x < 10.0f; }

}  // namespace

void begin() {
  for (size_t i = 0; i < STAMP_N; i++) g_stamp[i] = UINT32_MAX;
  // On the very first start the namespace does not exist yet, and a read-only
  // open then reports "nvs_open failed: NOT_FOUND" as an ERROR on the console.
  // It is caught, but a red error line at start that is no error at all costs
  // time searching next time. So open once for writing: that creates the
  // namespace without writing anything into it.
  if (g_nvs.begin(NS, false)) g_nvs.end();

  // Open read-only and close again at once -- the NVS handle should not stay
  // open for the whole runtime.
  // Check every key for existence first: a getFloat on a missing key logs
  // "nvs_get_blob len fail ... NOT_FOUND" as an ERROR on the console -- the same
  // false red line as above.
  if (g_nvs.begin(NS, true)) {
    if (g_nvs.isKey(K_DRUM))  g_drum = g_nvs.getFloat(K_DRUM,  DRUM_CIRCUMFERENCE_M);
    if (g_nvs.isKey(K_ARM))   g_arm  = g_nvs.getFloat(K_ARM,   TORQUE_ARM_M);
    if (g_nvs.isKey(K_CALIB)) g_cal  = g_nvs.getFloat(K_CALIB, CALIB_ARM_M);
    if (g_nvs.isKey(K_NPC))   g_npc  = g_nvs.getFloat(K_NPC,   LOADCELL_N_PER_COUNT);
    g_npcEpoch = g_nvs.getUInt(K_NPC_T, 0);
    if (g_nvs.isKey(K_DIR))   g_dir  = g_nvs.getChar(K_DIR, static_cast<int8_t>(VESC_DIRECTION_SIGN));
    if (g_nvs.isKey(K_TARE))  g_tare = g_nvs.getLong(K_TARE, 0);
    if (g_nvs.isKey(K_LOAD))  g_load = g_nvs.getFloat(K_LOAD, LOAD_DEFAULT_N);
    if (g_nvs.isKey(K_A0)) g_a0 = g_nvs.getFloat(K_A0, NAN);
    if (g_nvs.isKey(K_B0)) g_b0 = g_nvs.getFloat(K_B0, NAN);
    if (g_nvs.isKey(K_A1)) g_a1 = g_nvs.getFloat(K_A1, NAN);
    if (g_nvs.isKey(K_B1)) g_b1 = g_nvs.getFloat(K_B1, NAN);
    if (g_nvs.isKey(K_MWHEEL)) g_mWheel = g_nvs.getFloat(K_MWHEEL, NAN);
    if (g_nvs.isKey(K_UWHEEL)) g_uWheel = g_nvs.getFloat(K_UWHEEL, NAN);
    if (g_nvs.isKey(K_JWHEEL)) g_jWheel = g_nvs.getFloat(K_JWHEEL, NAN);
    if (g_nvs.isKey(K_ARMCAL)) g_armCal = g_nvs.getFloat(K_ARMCAL, NAN);
    if (g_nvs.isKey(K_CTRLKG)) g_ctrlKg = g_nvs.getFloat(K_CTRLKG, RUN_CTRL_MASS_KG);
    if (g_nvs.isKey(K_LD_EMPTY))  g_ldEmpty  = g_nvs.getFloat(K_LD_EMPTY,  LOAD_EMPTY_KG);
    if (g_nvs.isKey(K_LD_WEIGHT)) g_ldWeight = g_nvs.getFloat(K_LD_WEIGHT, LOAD_WEIGHT_KG);
    if (g_nvs.isKey(K_LD_ARM))    g_ldArm    = g_nvs.getFloat(K_LD_ARM,    LOAD_ARM_A_M);
    if (g_nvs.isKey(K_RM_TIRE))  g_nvs.getString(K_RM_TIRE, g_rm.tire, sizeof(g_rm.tire));
    if (g_nvs.isKey(K_RM_WHEEL)) g_nvs.getString(K_RM_WHEEL, g_rm.rim, sizeof(g_rm.rim));
    if (g_nvs.isKey(K_RM_HUB))   g_nvs.getString(K_RM_HUB, g_rm.hub, sizeof(g_rm.hub));
    if (g_nvs.isKey(K_RM_NOTE))  g_nvs.getString(K_RM_NOTE, g_rm.note, sizeof(g_rm.note));
    if (g_nvs.isKey(K_RM_ETRTO)) g_nvs.getString(K_RM_ETRTO, g_rm.etrto, sizeof(g_rm.etrto));
    if (g_nvs.isKey(K_RM_TUBE))  g_nvs.getString(K_RM_TUBE, g_rm.tube, sizeof(g_rm.tube));
    if (g_nvs.isKey(K_RM_BAR))   g_rm.bar = g_nvs.getFloat(K_RM_BAR, NAN);
    if (g_nvs.isKey(K_RM_TEMP))  g_rm.tempC = g_nvs.getFloat(K_RM_TEMP, NAN);
    if (g_nvs.isKey(K_RM_LOAD))  g_rm.loadKg = g_nvs.getFloat(K_RM_LOAD, NAN);
    if (g_nvs.isKey(K_RM_POS))   g_rm.posMm = g_nvs.getInt(K_RM_POS, 0);
    for (size_t i = 0; i < STAMP_N; i++) {
      char tk[16]; stampKey(tk, STAMPED[i]);
      g_stamp[i] = !g_nvs.isKey(STAMPED[i]) ? UINT32_MAX : (g_nvs.isKey(tk) ? g_nvs.getUInt(tk, 0) : 0);
    }
    if (g_nvs.isKey(K_WORG) && g_nvs.getBytesLength(K_WORG) == 1 + sizeof(WheelOrigin)) {
      uint8_t buf[1 + sizeof(WheelOrigin)];
      g_nvs.getBytes(K_WORG, buf, sizeof(buf));
      if (buf[0] == WORG_VER) memcpy(&g_wo, buf + 1, sizeof(g_wo));
    }
    const size_t bytes = g_nvs.isKey(K_PLAT) ? g_nvs.getBytesLength(K_PLAT) : 0;
    if (bytes > 0 && bytes % sizeof(float) == 0 && bytes <= sizeof(g_plat)) {
      g_nvs.getBytes(K_PLAT, g_plat, bytes);
      g_platN = bytes / sizeof(float);
      tidyPlateaus();
    }
    g_nvs.end();
  }
  if (g_platN == 0) defaultPlateaus();
  // A nonsense value that ended up in NVS must not poison the stand.
  if (!okDrum(g_drum)) g_drum = DRUM_CIRCUMFERENCE_M;
  if (!okArm(g_arm))   g_arm  = TORQUE_ARM_M;
  if (!okArm(g_cal))   g_cal  = CALIB_ARM_M;
  if (g_npc != 0.0f && !okNpc(g_npc)) g_npc = LOADCELL_N_PER_COUNT;
  if (g_dir != 1 && g_dir != -1) g_dir = static_cast<int8_t>(VESC_DIRECTION_SIGN);

  Serial.print("# Settings: drum "); Serial.print(g_drum, 4);
  Serial.print(" m, cell arm ");        Serial.print(g_arm, 4);
  Serial.print(" m, calibration arm "); Serial.print(g_cal, 4);
  Serial.print(" m, direction ");       Serial.print(g_dir);
  Serial.print(", calibration factor ");
  if (g_npc > 0.0f) Serial.println(g_npc, 8);
  else              Serial.println("NOT CALIBRATED");
  Serial.print("# Measuring speeds:");
  for (uint8_t i = 0; i < g_platN; i++) { Serial.print(' '); Serial.print(g_plat[i], 1); }
  Serial.println(" km/h");
}

namespace {
bool store2(const char* k1, float v1, const char* k2, float v2) {
  if (!g_nvs.begin(NS, false)) return false;
  const bool ok = g_nvs.putFloat(k1, v1) > 0 && g_nvs.putFloat(k2, v2) > 0;
  if (ok) stamp(k1);
  g_nvs.end();
  return ok;
}
bool store1(const char* k, float v) {
  if (!g_nvs.begin(NS, false)) return false;
  const bool ok = g_nvs.putFloat(k, v) > 0;
  if (ok) stamp(k);
  g_nvs.end();
  return ok;
}
// An NVS failure is a message of its own, not "implausible": the value applies in this
// session, it just does not survive a restart (finding B12, 2026-11-25).
void nvsFailed(const char* what) { Serial.print("# NVS: "); Serial.print(what); Serial.println(" not saved -- valid only until restart"); }
}  // namespace

// 0 = never set in this session (settings.h: age unknown then).
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
  if (!store2(K_A0, a0, K_B0, b0)) nvsFailed("no-load model");
  return true;
}
float wheelA1() { return g_a1; }
float wheelB1() { return g_b1; }
bool  haveWheelModel() { return !isnan(g_a1) && !isnan(g_b1); }
bool  setWheelModel(float a1, float b1) {
  if (isnan(a1) || isnan(b1) || fabsf(a1) > 5.0f || fabsf(b1) > 0.1f) return false;
  g_a1 = a1; g_b1 = b1; g_a1SetMs = millis();
  if (!store2(K_A1, a1, K_B1, b1)) nvsFailed("wheel model");
  // Origin: the inputs last entered for a run (saved while typing, so during a run they
  // are this run's). run.cpp overwrites it with the run id right after.
  setWheelOrigin(g_rm, "");
  return true;
}
float ctrlMassKg() { return g_ctrlKg; }
bool  setCtrlMassKg(float kg) {
  // Upper limit: on the calibration arm the weight must not overload the cell.
  if (!(kg > 0.0f && kg * 9.80665f * g_cal / g_arm <= LOADCELL_CAPACITY_N)) return false;
  g_ctrlKg = kg;
  return store1(K_CTRLKG, kg);
}
float wheelMassKg() { return g_mWheel; }
bool  setWheelMassKg(float kg) {
  if (!(kg > 0.3f && kg < 10.0f)) return false;
  g_mWheel = kg;
  return store1(K_MWHEEL, kg);
}

float wheelJKgm2() { return g_jWheel; }
uint32_t g_jSetMs = 0;
uint32_t wheelJSetMs() { return g_jSetMs; }
bool  setWheelJKgm2(float j) {
  if (j == 0.0f) {
    g_jWheel = NAN;
    if (!g_nvs.begin(NS, false)) return false;
    char tk[16]; stampKey(tk, K_JWHEEL);
    g_nvs.remove(K_JWHEEL); g_nvs.remove(tk); g_stamp[stampIndex(K_JWHEEL)] = UINT32_MAX;
    g_nvs.end();
    return true;
  }
  if (!(j > 0.01f && j < 2.0f)) return false;
  g_jWheel = j; g_jSetMs = millis() | 1;
  return store1(K_JWHEEL, j);
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
const WheelOrigin& wheelOrigin() { return g_wo; }
void setWheelOrigin(const RunMeta& m, const char* run, uint32_t epoch) {
  g_wo.m = m;
  snprintf(g_wo.run, sizeof(g_wo.run), "%s", run ? run : "");
  g_wo.epoch = epoch ? epoch : wallclock::now();
  g_wo.known = true;
  if (!g_nvs.begin(NS, false)) { nvsFailed("wheel model origin"); return; }
  uint8_t buf[1 + sizeof(WheelOrigin)];
  buf[0] = WORG_VER; memcpy(buf + 1, &g_wo, sizeof(g_wo));
  if (g_nvs.putBytes(K_WORG, buf, sizeof(buf)) != sizeof(buf)) nvsFailed("wheel model origin");
  g_nvs.end();
}
void setRunMeta(const RunMeta& m) {
  g_rm = m;
  if (!g_nvs.begin(NS, false)) return;
  g_nvs.putString(K_RM_TIRE, m.tire); g_nvs.putString(K_RM_WHEEL, m.rim); g_nvs.putString(K_RM_HUB, m.hub); g_nvs.putString(K_RM_NOTE, m.note);
  g_nvs.putString(K_RM_ETRTO, m.etrto); g_nvs.putString(K_RM_TUBE, m.tube);
  g_nvs.putFloat(K_RM_BAR, m.bar); g_nvs.putFloat(K_RM_TEMP, m.tempC); g_nvs.putFloat(K_RM_LOAD, m.loadKg);
  g_nvs.putInt(K_RM_POS, m.posMm);
  g_nvs.end();
}

float wheelCircM() { return g_uWheel; }
bool  setWheelCircM(float m) {
  if (!(m > 0.5f && m < 3.0f)) return false;
  g_uWheel = m;
  return store1(K_UWHEEL, m);
}

float loadN() { return g_load; }
bool setLoadN(float n) {
  if (!(n > 0.0f && n < 5000.0f)) return false;
  g_load = n;
  if (!g_nvs.begin(NS, false)) return false;
  const size_t k = g_nvs.putFloat(K_LOAD, n);
  if (k) stamp(K_LOAD);
  g_nvs.end();
  return k > 0;
}

long tareOffset() { return g_tare; }

bool setTareOffset(long counts) {
  g_tare = counts; g_tareSetMs = millis();
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putLong(K_TARE, counts);
  if (n) stamp(K_TARE);
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
  if (n) stamp(key);
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

uint32_t calibEpoch() { return g_npcEpoch; }

bool setLoadcellNPerCount(float x, uint32_t epoch) {
  if (!okNpc(x)) return false;
  g_npc = x;
  g_npcEpoch = epoch;
  // Until 2026-11-26 this was putUInt WITHOUT begin(): the time was never saved.
  if (g_nvs.begin(NS, false)) { g_nvs.putUInt(K_NPC_T, epoch); g_nvs.end(); }
  g_armCal = g_arm;   // the factor applies to THIS cell arm
  if (!store(K_ARMCAL, g_arm) || !store(K_NPC, x)) nvsFailed("calibration factor");
  return true;
}

bool setDirectionSign(int8_t sign) {
  if (sign != 1 && sign != -1) return false;
  g_dir = sign;
  if (!g_nvs.begin(NS, false)) return false;
  const size_t n = g_nvs.putChar(K_DIR, sign);
  if (n) stamp(K_DIR);
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
  g_mWheel = WHEEL_MASS_DEFAULT_KG;
  g_uWheel = NAN;
  g_jWheel = NAN;
  g_armCal = NAN;
  g_ctrlKg = RUN_CTRL_MASS_KG;
  g_rm = RunMeta{"", "", "", "", "", "", NAN, NAN, NAN, 0};
  g_wo = WheelOrigin{};
  g_ldEmpty = LOAD_EMPTY_KG; g_ldWeight = LOAD_WEIGHT_KG; g_ldArm = LOAD_ARM_A_M;
  defaultPlateaus();
  for (size_t i = 0; i < STAMP_N; i++) g_stamp[i] = UINT32_MAX;
}

size_t stampsJson(char* out, size_t n) {
  size_t p = 0;
  p += snprintf(out + p, n - p, "{");
  bool first = true;
  for (size_t i = 0; i < STAMP_N && p < n; i++) {
    if (g_stamp[i] == UINT32_MAX) continue;
    p += snprintf(out + p, n - p, "%s\"%s\":%lu", first ? "" : ",", STAMPED[i], static_cast<unsigned long>(g_stamp[i]));
    first = false;
  }
  if (p < n) p += snprintf(out + p, n - p, "}");
  return p;
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
