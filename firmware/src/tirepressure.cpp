#include "tirepressure.h"
#include "config.h"
#include <NimBLEDevice.h>

namespace tirepressure {
namespace {

// State, written from the BLE task, read from the main loop.
// 32-bit writes are atomic on the ESP32; no further protection is needed.
volatile float    g_bleBar  = NAN;     // moving average over AIRSPY_AVG_N values
volatile float    g_bleTemp = NAN;
volatile float    g_bleBatt = NAN;
volatile uint32_t g_bleLastMs = 0;     // millis() of the last pressure value, 0 = never
volatile bool     g_connected = false;
char              g_name[24] = "";

float g_manualBar = NAN;

// Averaging, used only in the BLE task.
float   g_ring[AIRSPY_AVG_N];
uint8_t g_ringPos = 0, g_ringFill = 0;

// Sighting from the scan, handed from the scan callback to the task.
NimBLEAddress g_seenAddr;
volatile bool g_seen = false;
volatile uint32_t g_seenMs = 0;
char g_seenName[24] = "";

static float readF32(const uint8_t* p) { float f; memcpy(&f, p, 4); return f; }

// One notification: 15 bytes. Pressure in the first half, temperature and battery in
// the second. Runs in the context of the NimBLE host.
void onNotify(NimBLERemoteCharacteristic*, uint8_t* d, size_t len, bool) {
  if (len != 15) return;
  if (d[14] == AIRSPY_TAG_PRESSURE) {
    const float kPa = readF32(d + 8);
    if (kPa > -50.0f && kPa < 1500.0f) {
      g_ring[g_ringPos] = kPa / 100.0f;
      g_ringPos = (g_ringPos + 1) % AIRSPY_AVG_N;
      if (g_ringFill < AIRSPY_AVG_N) g_ringFill++;
      float sum = 0; for (uint8_t i = 0; i < g_ringFill; i++) sum += g_ring[i];
      g_bleBar = sum / g_ringFill;
      g_bleLastMs = millis();
    }
  } else if (d[6] == AIRSPY_TAG_TEMP && d[13] == AIRSPY_TAG_BATTERY) {
    g_bleTemp = readF32(d + 0);
    g_bleBatt = readF32(d + 7);
  }
}

class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    const std::string n = dev->getName();
    if (n.rfind(AIRSPY_NAME_PREFIX, 0) != 0) return;
    strncpy(g_seenName, n.c_str(), sizeof(g_seenName) - 1);
    g_seenAddr = dev->getAddress();
    g_seenMs = millis();
    g_seen = true;
  }
};
ScanCB g_scanCB;

class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*, int reason) override {
    g_connected = false;
    g_ringFill = g_ringPos = 0;    // never average across a gap
    Serial.print("# AirSpy: connection lost, reason "); Serial.println(reason);
  }
};
ClientCB g_clientCB;

NimBLEClient* g_client = nullptr;

void startScan() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) return;
  scan->setActiveScan(true);           // the name is in the scan response
  scan->setInterval(100);
  scan->setWindow(30);
  scan->setMaxResults(0);
  scan->start(0, false, true);
}

// Connect and subscribe. Blocks for up to AIRSPY_CONNECT_TIMEOUT_MS -- hence in
// its own task.
bool connectAndSubscribe() {
  if (!g_client) {
    g_client = NimBLEDevice::createClient();
    if (!g_client) return false;
    g_client->setClientCallbacks(&g_clientCB, false);
    g_client->setConnectTimeout(AIRSPY_CONNECT_TIMEOUT_MS);
  }
  if (g_client->isConnected()) return true;
  NimBLEDevice::getScan()->stop();
  Serial.print("# AirSpy: connecting to "); Serial.println(g_seenName);
  if (!g_client->connect(g_seenAddr, true)) { Serial.println("# AirSpy: connection failed"); return false; }
  NimBLERemoteService* svc = g_client->getService(NimBLEUUID(AIRSPY_SVC_UUID16));
  NimBLERemoteCharacteristic* chr = svc ? svc->getCharacteristic(NimBLEUUID(AIRSPY_CHR_UUID16)) : nullptr;
  if (!chr || !chr->canNotify() || !chr->subscribe(true, onNotify)) {
    Serial.println("# AirSpy: cannot subscribe to 0xFFF4");
    g_client->disconnect();
    return false;
  }
  // Request a slow connection interval, otherwise the coin cell is empty within a day.
  g_client->updateConnParams(AIRSPY_CONN_ITVL_MIN, AIRSPY_CONN_ITVL_MAX, 4, 600);
  strncpy(g_name, g_seenName, sizeof(g_name) - 1);
  g_connected = true;
  Serial.print("# AirSpy: "); Serial.print(g_name); Serial.println(" subscribed (0xFFF4, 2 Hz)");
  return true;
}

void bleTask(void*) {
  NimBLEDevice::init("");
  NimBLEDevice::setPower(3);
  NimBLEDevice::getScan()->setScanCallbacks(&g_scanCB, false);
  startScan();
  uint32_t nextTry = 0;
  for (;;) {
    if (g_connected) {
      // Connected but silent: the sensor is asleep or the connection is dead.
      if (g_bleLastMs && millis() - g_bleLastMs > AIRSPY_MUTE_MS && g_client && g_client->isConnected()) {
        Serial.println("# AirSpy: silent, disconnecting");
        g_client->disconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(AIRSPY_POLL_MS));
      continue;
    }
    // Only connect if the sensor was just heard -- then it answers at once.
    if (g_seen && millis() - g_seenMs < AIRSPY_SEEN_MS && millis() >= nextTry) {
      g_seen = false;
      if (!connectAndSubscribe()) nextTry = millis() + AIRSPY_RETRY_MS;
    }
    if (!g_connected) startScan();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

}  // namespace

void begin() {
  // Core 0, below the network task; the main loop runs on core 1.
  xTaskCreatePinnedToCore(bleTask, "airspy", 6144, nullptr, 1, nullptr, 0);
}

void service() {}

bool connected() { return g_connected; }
bool fresh() { return g_bleLastMs != 0 && millis() - g_bleLastMs < AIRSPY_STALE_MS; }
uint32_t ageMs() { return g_bleLastMs ? millis() - g_bleLastMs : UINT32_MAX; }
const char* name() { return g_name; }

float bar() {
  if (fresh()) return g_bleBar;
  if (!isnan(g_manualBar)) return g_manualBar;
  return isnan(g_bleBar) ? NAN : g_bleBar;   // old, but better than nothing; fresh() tells
}
float tempC()   { return g_bleLastMs ? g_bleTemp : NAN; }
float battery() { return g_bleLastMs ? g_bleBatt : NAN; }

Source source() {
  if (fresh()) return SRC_BLE;
  if (!isnan(g_manualBar)) return SRC_MANUAL;
  return g_bleLastMs ? SRC_BLE : SRC_NONE;
}

const char* sourceName() {
  switch (source()) {
    case SRC_MANUAL: return "manual";
    case SRC_BLE:    return "ble";
    default:         return "none";
  }
}

void setManual(float b) { g_manualBar = b; }

}  // namespace tirepressure
