#include "tirepressure.h"
#include "config.h"
#include <NimBLEDevice.h>

namespace tirepressure {
namespace {

// Zustand, aus der BLE-Task geschrieben, aus der Hauptschleife gelesen.
// 32-Bit-Schreibzugriffe sind auf dem ESP32 atomar; mehr Schutz braucht es nicht.
volatile float    g_bleBar  = NAN;     // gleitendes Mittel ueber AIRSPY_AVG_N Werte
volatile float    g_bleTemp = NAN;
volatile float    g_bleBatt = NAN;
volatile uint32_t g_bleLastMs = 0;     // millis() des letzten Druckwerts, 0 = nie
volatile bool     g_connected = false;
char              g_name[24] = "";

float g_manualBar = NAN;

// Mittelung, nur in der BLE-Task benutzt.
float   g_ring[AIRSPY_AVG_N];
uint8_t g_ringPos = 0, g_ringFill = 0;

// Sichtung aus dem Scan, von der Scan-Callback an die Task uebergeben.
NimBLEAddress g_seenAddr;
volatile bool g_seen = false;
volatile uint32_t g_seenMs = 0;
char g_seenName[24] = "";

static float readF32(const uint8_t* p) { float f; memcpy(&f, p, 4); return f; }

// Eine Notification: 15 Byte. Druck in der ersten Haelfte, Temperatur und Batterie in
// der zweiten. Laeuft im Kontext des NimBLE-Hosts.
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
    g_ringFill = g_ringPos = 0;    // nie ueber eine Luecke mitteln
    Serial.print("# AirSpy: Verbindung weg, Grund "); Serial.println(reason);
  }
};
ClientCB g_clientCB;

NimBLEClient* g_client = nullptr;

void startScan() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  if (scan->isScanning()) return;
  scan->setActiveScan(true);           // der Name steht in der Scan-Antwort
  scan->setInterval(100);
  scan->setWindow(30);
  scan->setMaxResults(0);
  scan->start(0, false, true);
}

// Verbinden und abonnieren. Blockiert bis zu AIRSPY_CONNECT_TIMEOUT_MS -- deshalb in
// der eigenen Task.
bool connectAndSubscribe() {
  if (!g_client) {
    g_client = NimBLEDevice::createClient();
    if (!g_client) return false;
    g_client->setClientCallbacks(&g_clientCB, false);
    g_client->setConnectTimeout(AIRSPY_CONNECT_TIMEOUT_MS);
  }
  if (g_client->isConnected()) return true;
  NimBLEDevice::getScan()->stop();
  Serial.print("# AirSpy: verbinde mit "); Serial.println(g_seenName);
  if (!g_client->connect(g_seenAddr, true)) { Serial.println("# AirSpy: Verbindung fehlgeschlagen"); return false; }
  NimBLERemoteService* svc = g_client->getService(NimBLEUUID(AIRSPY_SVC_UUID16));
  NimBLERemoteCharacteristic* chr = svc ? svc->getCharacteristic(NimBLEUUID(AIRSPY_CHR_UUID16)) : nullptr;
  if (!chr || !chr->canNotify() || !chr->subscribe(true, onNotify)) {
    Serial.println("# AirSpy: 0xFFF4 nicht abonnierbar");
    g_client->disconnect();
    return false;
  }
  // Langsames Verbindungsintervall erbitten, sonst ist die Knopfzelle in einem Tag leer.
  g_client->updateConnParams(AIRSPY_CONN_ITVL_MIN, AIRSPY_CONN_ITVL_MAX, 4, 600);
  strncpy(g_name, g_seenName, sizeof(g_name) - 1);
  g_connected = true;
  Serial.print("# AirSpy: "); Serial.print(g_name); Serial.println(" abonniert (0xFFF4, 2 Hz)");
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
      // Verbunden, aber stumm: der Sensor schlaeft oder die Verbindung ist tot.
      if (g_bleLastMs && millis() - g_bleLastMs > 6 * AIRSPY_STALE_MS && g_client && g_client->isConnected()) {
        Serial.println("# AirSpy: stumm, trenne");
        g_client->disconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    // Nur verbinden, wenn der Sensor gerade gehoert wurde -- dann antwortet er sofort.
    if (g_seen && millis() - g_seenMs < 3000 && millis() >= nextTry) {
      g_seen = false;
      if (!connectAndSubscribe()) nextTry = millis() + 2000;
    }
    if (!g_connected) startScan();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

}  // namespace

void begin() {
  // Kern 0, unter der Netzwerk-Task; die Hauptschleife laeuft auf Kern 1.
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
  return isnan(g_bleBar) ? NAN : g_bleBar;   // alt, aber besser als nichts; fresh() sagt es
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
