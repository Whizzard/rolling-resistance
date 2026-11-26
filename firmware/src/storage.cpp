#include "storage.h"
#include "config.h"
#include "rawlog.h"

#include <LittleFS.h>
#include <vfs_api.h>

#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include "diskio_sdmmc.h"
#include "ff.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

namespace storage {
namespace {

constexpr const char* SD_MOUNT = "/sd";
constexpr spi_host_device_t SD_HOST = SPI2_HOST;

portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
Info g_info[MEDIA_N];
volatile bool g_formatReq[MEDIA_N] = {false, false};
volatile bool g_flashWritable = false;

SemaphoreHandle_t g_sdLock = nullptr;
bool g_busOk = false;
sdmmc_card_t* g_card = nullptr;
bool g_sdMounted = false;

// Arduino FS on top of the /sd mounted by ESP-IDF: the same File interface
// as LittleFS, so that webui.cpp and rawlog.cpp need not distinguish.
std::shared_ptr<VFSImpl> g_sdImpl = std::make_shared<VFSImpl>();
fs::FS g_sdFs(g_sdImpl);

void setInfo(Media m, const Info& i) {
  portENTER_CRITICAL(&g_mux); g_info[m] = i; portEXIT_CRITICAL(&g_mux);
}
Info getInfo(Media m) {
  portENTER_CRITICAL(&g_mux); const Info i = g_info[m]; portEXIT_CRITICAL(&g_mux); return i;
}
void setState(Media m, State s) {
  Info i = getInfo(m);
  if (i.state != s) {
    Serial.print("# Storage "); Serial.print(name(m)); Serial.print(": "); Serial.println(stateName(s));
  }
  i.state = s;
  if (s == ABSENT || s == UNREADABLE) { i.total = i.used = 0; i.pct = -1; }
  setInfo(m, i);
}

int8_t pctOf(uint64_t used, uint64_t total) {
  if (!total) return -1;
  const uint64_t p = (used * 100 + total / 2) / total;
  return static_cast<int8_t>(p > 100 ? 100 : p);
}

// ------------------------------------------------------------------ SD
bool busInit() {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_SD_MOSI;
  bus.miso_io_num = PIN_SD_MISO;
  bus.sclk_io_num = PIN_SD_SCK;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = 4096;
  const esp_err_t e = spi_bus_initialize(SD_HOST, &bus, SPI_DMA_CH_AUTO);
  if (e != ESP_OK) { Serial.print("# SD: SPI bus not free, "); Serial.println(esp_err_to_name(e)); }
  return e == ESP_OK;
}

// ESP_OK mounted; ESP_FAIL card answers, but no FAT; otherwise no card.
esp_err_t sdMount(bool format) {
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_HOST;
  host.max_freq_khz = SD_SPI_KHZ;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
  slot.host_id = SD_HOST;
  esp_vfs_fat_sdmmc_mount_config_t mc = {};
  mc.format_if_mount_failed = format;
  mc.max_files = 4;
  mc.allocation_unit_size = 16 * 1024;
  return esp_vfs_fat_sdspi_mount(SD_MOUNT, &host, &slot, &mc, &g_card);
}

void sdUnmount() {
  if (!g_sdMounted) return;
  rawlog::mediaLost(SD_CARD);
  g_sdImpl->mountpoint(nullptr);
  esp_vfs_fat_sdcard_unmount(SD_MOUNT, g_card);
  g_card = nullptr;
  g_sdMounted = false;
}

bool sdWriteProbe() {
  File f = g_sdFs.open("/.probe", FILE_WRITE);
  if (!f) return false;
  const bool ok = f.write(reinterpret_cast<const uint8_t*>("1"), 1) == 1;
  f.close();
  g_sdFs.remove("/.probe");
  return ok;
}

void sdStats() {
  Info i = getInfo(SD_CARD);
  if (!g_sdMounted) return;
  char drv[4] = {static_cast<char>('0' + ff_diskio_get_pdrv_card(g_card)), ':', 0, 0};
  FATFS* f = nullptr; DWORD freeClust = 0;
  if (f_getfree(drv, &freeClust, &f) != FR_OK || !f) return;
  const uint64_t clust = static_cast<uint64_t>(f->csize) * 512u;   // SD: sector 512 bytes
  i.total = static_cast<uint64_t>(f->n_fatent - 2) * clust;
  i.used  = i.total - static_cast<uint64_t>(freeClust) * clust;
  i.pct   = pctOf(i.used, i.total);
  setInfo(SD_CARD, i);
}

void sdAfterMount() {
  g_sdMounted = true;
  g_sdImpl->mountpoint(SD_MOUNT);
  setState(SD_CARD, sdWriteProbe() ? READY : READONLY);
  if (getInfo(SD_CARD).state == READY) {
    if (!g_sdFs.exists("/raw"))  g_sdFs.mkdir("/raw");
    if (!g_sdFs.exists("/runs")) g_sdFs.mkdir("/runs");
  }
  sdStats();
}

void sdCheck() {
  if (!g_busOk) return;
  if (g_sdMounted) {
    // CMD13 (status) bypassing the FAT: therefore only when nobody is reading.
    if (xSemaphoreTake(g_sdLock, 0) != pdTRUE) return;
    const esp_err_t e = sdmmc_get_status(g_card);
    xSemaphoreGive(g_sdLock);
    if (e == ESP_OK) return;
    Serial.println("# SD: removed");
    sdUnmount();
    setState(SD_CARD, ABSENT);
    return;
  }
  const esp_err_t e = sdMount(false);
  if (e == ESP_OK) { Serial.println("# SD: mounted"); sdAfterMount(); }
  else if (e == ESP_FAIL) setState(SD_CARD, UNREADABLE);
  else setState(SD_CARD, ABSENT);
}

void sdFormat() {
  Serial.println("# SD: formatting ...");
  sdUnmount();
  const esp_err_t e = sdMount(true);
  if (e == ESP_OK) { Serial.println("# SD: formatted"); sdAfterMount(); }
  else { Serial.print("# SD: formatting failed, "); Serial.println(esp_err_to_name(e)); setState(SD_CARD, ABSENT); }
}

// ------------------------------------------------------------------ Flash
// The label "spiffs" is the one from the partition table (default_16MB.csv).
bool flashMount() {
  if (!LittleFS.begin(false, "/littlefs", 6, "spiffs")) return false;
  if (!LittleFS.exists("/runs")) LittleFS.mkdir("/runs");
  return true;
}

// For usedBytes() LittleFS walks through all blocks: many short flash reads,
// each locks the cache and with it the sensor ISRs for a few µs. Therefore only at
// standstill; while driving the usage does not change anyway (ring, see above).
void flashStats() {
  Info i = getInfo(FLASH);
  if (i.state != READY || !g_flashWritable) return;
  i.total = LittleFS.totalBytes();
  i.used  = LittleFS.usedBytes();
  i.pct   = pctOf(i.used, i.total);
  setInfo(FLASH, i);
}

void flashFormat() {
  Serial.println("# Flash: formatting ...");
  rawlog::mediaLost(FLASH);
  LittleFS.end();
  const bool ok = LittleFS.format() && flashMount();
  Serial.println(ok ? "# Flash: formatted" : "# Flash: formatting failed");
  setState(FLASH, ok ? READY : UNREADABLE);
  flashStats();
}

void beginFormat(Media m, bool on) {
  Info i = getInfo(m); i.formatting = on; setInfo(m, i);
}

// ------------------------------------------------------------------ Task
void task(void*) {
  uint32_t lastCheck = 0, lastStats = 0;
  for (;;) {
    const uint32_t now = millis();
    for (uint8_t m = 0; m < MEDIA_N; m++) {
      if (!g_formatReq[m]) continue;
      beginFormat(static_cast<Media>(m), true);
      if (m == SD_CARD) sdFormat(); else flashFormat();
      beginFormat(static_cast<Media>(m), false);
      g_formatReq[m] = false;
    }
    if (now - lastCheck >= STORAGE_CHECK_MS) { lastCheck = now; sdCheck(); }
    rawlog::writerService();
    if (now - lastStats >= STORAGE_STATS_MS) {
      lastStats = now;
      sdStats();
      flashStats();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void begin() {
  g_sdLock = xSemaphoreCreateMutex();
  // Flash first and here, not in the task: run::begin() needs /runs immediately.
  // NO formatting on failure (until 2026-11-25 LittleFS.begin(true)): an
  // unreadable flash is reported and only formatted after confirmation.
  g_flashWritable = true;   // at boot the drum stands still; from now on the main loop sets it
  if (flashMount()) { setState(FLASH, READY); flashStats(); }
  else {
    setState(FLASH, UNREADABLE);
    Serial.println("# Flash: LittleFS unreadable — format under Setup > Storage (or console Ff)");
  }
  const Info fi = getInfo(FLASH);
  if (fi.state == READY) {
    Serial.print("# Flash: "); Serial.print(static_cast<uint32_t>(fi.used / 1024)); Serial.print(" of ");
    Serial.print(static_cast<uint32_t>(fi.total / 1024)); Serial.println(" kB used");
  }
  g_busOk = busInit();
  if (g_busOk) sdCheck();
  rawlog::begin();
  xTaskCreatePinnedToCore(task, "storage", 8192, nullptr, 1, nullptr, 0);
}

Info info(Media m) { return getInfo(m); }

fs::FS* fs(Media m) {
  const State s = getInfo(m).state;
  if (s != READY && s != READONLY) return nullptr;
  return m == SD_CARD ? &g_sdFs : static_cast<fs::FS*>(&LittleFS);
}

const char* name(Media m) { return m == SD_CARD ? "sd" : "flash"; }

const char* stateName(State s) {
  switch (s) {
    case READY:      return "ready";
    case READONLY:   return "read-only";
    case UNREADABLE: return "unreadable";
    default:         return "absent";
  }
}

bool requestFormat(Media m, const char** why) {
  if (m >= MEDIA_N) { *why = "Medium?"; return false; }
  const Info i = getInfo(m);
  if (i.formatting || g_formatReq[m]) { *why = "Already formatting"; return false; }
  if (i.state != UNREADABLE) { *why = i.state == ABSENT ? "No card" : "Readable, not formatted"; return false; }
  if (rawlog::active() && rawlog::media() == m) { *why = "Recording active"; return false; }
  if (m == FLASH && !g_flashWritable) { *why = "Stop first"; return false; }
  g_formatReq[m] = true;
  return true;
}

Media protocolMedia() {
  return getInfo(SD_CARD).state == READY ? SD_CARD : FLASH;
}

void lockSd()   { if (g_sdLock) xSemaphoreTake(g_sdLock, portMAX_DELAY); }
void unlockSd() { if (g_sdLock) xSemaphoreGive(g_sdLock); }

void setFlashWritable(bool ok) { g_flashWritable = ok; }
bool flashWritable() { return g_flashWritable; }

}  // namespace storage
