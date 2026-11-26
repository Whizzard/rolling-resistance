#include "storage.h"
#include "config.h"
#include "rawlog.h"
#include "run.h"
#include "wallclock.h"
#include "calc/timefmt.h"

#include <LittleFS.h>
#include <vfs_api.h>

#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <esp_heap_caps.h>
#include <soc/spi_struct.h>
#include "diskio_sdmmc.h"
#include "diskio_impl.h"
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
  if (s == ABSENT || s == UNREADABLE) { i.total = i.used = 0; i.pct = -1; i.fsType = ""; }
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
  i.fsType = f->fs_type == FS_FAT12 ? "FAT12" : f->fs_type == FS_FAT16 ? "FAT16"
           : f->fs_type == FS_FAT32 ? "FAT32" : "exFAT";
  setInfo(SD_CARD, i);
}

// The driver raises the clock after the card's init only in fixed steps (20, 26, 40,
// 52 MHz, sdmmc_init_host_frequency); a limit below 20 MHz falls back to the probing
// clock of 400 kHz. So until 2026-11-26 the card ran at 400 kHz (about 45 kB/s, measured
// in the SPI clock register). Set the wanted clock explicitly after every init.
void sdSetClock(sdmmc_card_t* c) {
  if (!c) return;
  const esp_err_t e = sdspi_host_set_card_clk(c->host.slot, SD_SPI_KHZ);
  if (e != ESP_OK) Serial.printf("# SD: clock %lu kHz not set, %s\n", (unsigned long)SD_SPI_KHZ, esp_err_to_name(e));
}

volatile uint16_t g_sdMounts = 0;
void sdAfterMount() {
  g_sdMounted = true;
  g_sdMounts++;
  sdSetClock(g_card);
  if (g_card) Serial.printf("# SD: card max %u kHz, host %d kHz, bus width %d\n",
                            (unsigned)g_card->max_freq_khz, g_card->host.max_freq_khz, (int)g_card->log_bus_width);
  // What the card says about itself (CID/CSD), for judging its age and class.
  if (g_card) Serial.printf("# SD: \"%.8s\" maker 0x%02x oem 0x%04x rev %d.%d, made %d-%02d, %llu MB, %s, CSD speed %d kHz\n",
                            g_card->cid.name, g_card->cid.mfg_id, g_card->cid.oem_id, g_card->cid.revision >> 4, g_card->cid.revision & 15,
                            2000 + ((g_card->cid.date >> 4) & 0xff), g_card->cid.date & 15,
                            (unsigned long long)g_card->csd.capacity * g_card->csd.sector_size / (1024 * 1024),
                            g_card->ocr & (1UL << 30) ? "SDHC/SDXC" : "SDSC", g_card->csd.tr_speed / 1000);
  g_sdImpl->mountpoint(SD_MOUNT);
  setState(SD_CARD, sdWriteProbe() ? READY : READONLY);
  if (getInfo(SD_CARD).state == READY) {
    if (!g_sdFs.exists(ROOT_DIR)) g_sdFs.mkdir(ROOT_DIR);
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

// Result of the last format per medium, for the UI ("" = none yet, "ok", or the reason).
const char* g_fmtResult[MEDIA_N] = {"", ""};
volatile uint16_t g_fmtDone[MEDIA_N] = {0, 0};   // finished formats since boot, for the UI

// Progress while formatting (2026-11-26: a 64 GB card took minutes without a sign of
// life). f_mkfs has no callback, so for its duration the card's FatFs driver is replaced
// by a thin one that counts the sectors written. Expected: two FATs plus root directory
// and reserved area; the FATs are almost everything. Console line every 2 s, UI via
// formatPct()/formatSec().
volatile uint8_t  g_fmtPct = 0;
volatile uint32_t g_fmtT0 = 0, g_fmtSecs = 0;
uint32_t g_fmtExpect = 1, g_fmtWritten = 0, g_fmtLastPrint = 0;
DSTATUS fmtInit(BYTE)   { return 0; }
DSTATUS fmtStatus(BYTE) { return 0; }
DRESULT fmtRead(BYTE, BYTE* b, uint32_t s, unsigned n) { return sdmmc_read_sectors(g_card, b, s, n) == ESP_OK ? RES_OK : RES_ERROR; }
DRESULT fmtWrite(BYTE, const BYTE* b, uint32_t s, unsigned n) {
  if (sdmmc_write_sectors(g_card, b, s, n) != ESP_OK) return RES_ERROR;
  g_fmtWritten += n;
  const uint32_t pct = static_cast<uint32_t>(100ULL * g_fmtWritten / g_fmtExpect);
  g_fmtPct = pct > 99 ? 99 : pct;
  if (millis() - g_fmtLastPrint >= 2000) {
    g_fmtLastPrint = millis();
    Serial.printf("# SD: formatting %u %%, %lu MB written, %lu s\n", (unsigned)g_fmtPct,
                  (unsigned long)(g_fmtWritten / 2048), (unsigned long)((millis() - g_fmtT0) / 1000));
  }
  return RES_OK;
}
DRESULT fmtIoctl(BYTE, BYTE cmd, void* b) {
  switch (cmd) {
    case CTRL_SYNC: return RES_OK;
    case GET_SECTOR_COUNT: *static_cast<DWORD*>(b) = g_card->csd.capacity; return RES_OK;
    case GET_SECTOR_SIZE:  *static_cast<WORD*>(b) = g_card->csd.sector_size; return RES_OK;
    case GET_BLOCK_SIZE:   *static_cast<DWORD*>(b) = 1; return RES_OK;
    default: return RES_OK;   // CTRL_TRIM and the rest: nothing to do
  }
}
const ff_diskio_impl_t FMT_IO = {fmtInit, fmtStatus, fmtRead, fmtWrite, fmtIoctl};

// Format the card. Until 2026-11-26 this only re-mounted with format_if_mount_failed,
// which does nothing on a READABLE card (the mount succeeds) and still said "formatted".
// Now: FatFs f_mkfs on the card's drive, FAT type chosen by size (FAT32 above about 2 GB),
// allocation unit 16 kB as at mounting, then a fresh mount. An unreadable card that does
// not mount at all still goes through format_if_mount_failed. Under the SD lock: the web
// task must not read in between. Takes a few seconds on a 4 GB card (FAT tables).
void sdFormat() {
  Serial.println("# SD: formatting ...");
  if (g_sdLock) xSemaphoreTake(g_sdLock, portMAX_DELAY);
  // One path for a readable and an unreadable card (until 2026-11-26 the unreadable one
  // went through format_if_mount_failed: no progress, 4 kB pieces): unmount, address the
  // card directly without a file system, f_mkfs through the counting driver, mount anew.
  sdUnmount();
  FRESULT fr = FR_NOT_READY;
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.max_freq_khz = SD_SPI_KHZ;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
  slot.host_id = SD_HOST;
  sdspi_dev_handle_t dev = -1;
  sdmmc_card_t* card = static_cast<sdmmc_card_t*>(calloc(1, sizeof(sdmmc_card_t)));
  BYTE pdrv = 0xFF;
  // 32 kB work buffer: f_mkfs writes the FATs in pieces of this size (4 kB until
  // 2026-11-26), DMA-capable because it goes straight to the SPI driver.
  const UINT workLen = 32 * 1024;
  void* work = heap_caps_malloc(workLen, MALLOC_CAP_DMA);
  if (card && work && sdspi_host_init() == ESP_OK && sdspi_host_init_device(&slot, &dev) == ESP_OK) {
    host.slot = dev;
    if (sdmmc_card_init(&host, card) != ESP_OK) Serial.println("# SD: card does not answer");
    else if (ff_diskio_get_drive(&pdrv) != ESP_OK) Serial.println("# SD: no free drive number");
    else {
      sdSetClock(card);
      g_card = card;   // the counting driver works on g_card
      const uint64_t sectors = static_cast<uint64_t>(card->csd.capacity);
      const uint32_t auSec = 16 * 1024 / 512;
      const uint32_t fatSec = static_cast<uint32_t>((sectors / auSec * 4 + 511) / 512);
      g_fmtExpect = fatSec + auSec + 64;   // ONE FAT: f_mkfs writes 1 (measured 2026-11-26, 64 GB: 14 MB)
      g_fmtWritten = 0; g_fmtPct = 0; g_fmtT0 = g_fmtLastPrint = millis();
      Serial.printf("# SD: %llu MB card, clusters 16 kB, about %lu MB of FAT tables to write\n",
                    (unsigned long long)(sectors / 2048), (unsigned long)(g_fmtExpect / 2048));
      ff_diskio_register(pdrv, &FMT_IO);
      char drv[3] = {static_cast<char>('0' + pdrv), ':', 0};
      fr = f_mkfs(drv, FM_ANY, 16 * 1024, work, workLen);
      ff_diskio_unregister(pdrv);
      g_fmtSecs = (millis() - g_fmtT0) / 1000;
      Serial.printf("# SD: f_mkfs %s after %lu s, %lu MB written\n", fr == FR_OK ? "done" : "FAILED",
                    (unsigned long)g_fmtSecs, (unsigned long)(g_fmtWritten / 2048));
      g_card = nullptr;
    }
  } else Serial.println("# SD: no memory or SPI device for formatting");
  if (dev >= 0) sdspi_host_remove_device(dev);
  if (work) heap_caps_free(work);
  free(card);
  if (fr != FR_OK) Serial.printf("# SD: f_mkfs result %d\n", static_cast<int>(fr));
  const esp_err_t e = fr == FR_OK ? sdMount(false) : ESP_FAIL;
  g_fmtResult[SD_CARD] = fr == FR_OK ? (e == ESP_OK ? "ok" : "formatted, but does not mount") : "format failed";
  if (e == ESP_OK) { Serial.println("# SD: formatted"); sdAfterMount(); }
  else { Serial.println("# SD: formatting failed"); setState(SD_CARD, ABSENT); }
  if (g_sdLock) xSemaphoreGive(g_sdLock);
}

// ------------------------------------------------------------------ Flash
// The label "spiffs" is the one from the partition table (default_16MB.csv).
bool flashMount() {
  if (!LittleFS.begin(false, "/littlefs", 6, "spiffs")) return false;
  if (!LittleFS.exists(ROOT_DIR)) LittleFS.mkdir(ROOT_DIR);
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
  i.fsType = "LittleFS";
  setInfo(FLASH, i);
}

void flashFormat() {
  Serial.println("# Flash: formatting ...");
  rawlog::mediaLost(FLASH);
  LittleFS.end();
  const bool ok = LittleFS.format() && flashMount();
  Serial.println(ok ? "# Flash: formatted" : "# Flash: formatting failed");
  g_fmtResult[FLASH] = ok ? "ok" : "format failed";
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
      g_fmtDone[m]++;
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
  if (i.state == ABSENT) { *why = "No card"; return false; }
  if (m == FLASH && i.state != UNREADABLE) { *why = "Flash readable, not formatted (holds the profiles)"; return false; }
  if (rawlog::active() && rawlog::media() == m) { *why = "Recording active"; return false; }
  if (m == SD_CARD && run::busy()) { *why = "Run active"; return false; }
  if (m == FLASH && !g_flashWritable) { *why = "Stop first"; return false; }
  g_fmtResult[m] = "";
  g_formatReq[m] = true;
  return true;
}

void sessionName(char* out, size_t n) {
  const uint32_t t = wallclock::now();
  if (t) { char s[24]; calc::fmtTime(t, s, sizeof(s), false); snprintf(out, n, "%s%02lu", s, static_cast<unsigned long>(t % 60)); }
  else snprintf(out, n, "up%lu", static_cast<unsigned long>(millis() / 1000));
}

bool makeDir(fs::FS& f, const char* path) {
  if (!f.exists(ROOT_DIR)) f.mkdir(ROOT_DIR);
  if (!f.exists(path)) f.mkdir(path);
  return f.exists(path);
}

uint16_t sdMounts() { return g_sdMounts; }
uint8_t formatPct() { return g_fmtPct; }
uint32_t formatSec() { return g_fmtT0 ? (millis() - g_fmtT0) / 1000 : 0; }
const char* formatResult(Media m) { return m < MEDIA_N ? g_fmtResult[m] : ""; }
uint16_t formatsDone(Media m) { return m < MEDIA_N ? g_fmtDone[m] : 0; }

Media protocolMedia() {
  return getInfo(SD_CARD).state == READY ? SD_CARD : FLASH;
}

// SD read benchmark past the file system (console Gb, 2026-11-26): 128 sectors (64 kB)
// from the middle of the card, once as one multi-sector read and once sector by sector.
// Separates a slow card or bus from a slow FAT/VFS path. Read only.
void sdBench() {
  if (!g_card || !g_sdMounted) { Serial.println("# SD bench: no card"); return; }
  constexpr size_t N = 128;
  uint8_t* b = static_cast<uint8_t*>(heap_caps_malloc(N * 512, MALLOC_CAP_DMA));
  if (!b) { Serial.println("# SD bench: no memory"); return; }
  const size_t start = static_cast<size_t>(g_card->csd.capacity) / 2;
  lockSd();
  uint32_t t = micros();
  const esp_err_t e1 = sdmmc_read_sectors(g_card, b, start, N);
  const uint32_t tMulti = micros() - t;
  t = micros();
  esp_err_t e2 = ESP_OK;
  for (size_t i = 0; i < N && e2 == ESP_OK; i++) e2 = sdmmc_read_sectors(g_card, b, start + N + i, 1);
  const uint32_t tSingle = micros() - t;
  // SPI clock actually set: the register holds the last transaction's device setting.
  const uint32_t ck = GPSPI2.clock.val;
  const bool equ = GPSPI2.clock.clk_equ_sysclk;
  const float khz = equ ? 80000.0f : 80000.0f / ((GPSPI2.clock.clkdiv_pre + 1) * (GPSPI2.clock.clkcnt_n + 1));
  unlockSd();
  heap_caps_free(b);
  Serial.printf("# SD bench: SPI clock register 0x%08lx -> %.0f kHz (from 80 MHz)\n", (unsigned long)ck, khz);
  Serial.printf("# SD bench: 64 kB as one read %lu ms (%.0f kB/s, %s), as 128 single sectors %lu ms (%.0f kB/s, %s)\n",
                (unsigned long)(tMulti / 1000), 64000.0 / (tMulti / 1000.0 + 1e-3), esp_err_to_name(e1),
                (unsigned long)(tSingle / 1000), 64000.0 / (tSingle / 1000.0 + 1e-3), esp_err_to_name(e2));
}

void lockSd()   { if (g_sdLock) xSemaphoreTake(g_sdLock, portMAX_DELAY); }
void unlockSd() { if (g_sdLock) xSemaphoreGive(g_sdLock); }

void setFlashWritable(bool ok) { g_flashWritable = ok; }
bool flashWritable() { return g_flashWritable; }

}  // namespace storage
