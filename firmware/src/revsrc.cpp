#include "revsrc.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "speedctl.h"

namespace revsrc {
namespace {

Source   g_src = SRC_NONE;
uint32_t g_lastMarkCount = 0;
uint32_t g_lastMarkMs = 0;

// Gerechnete Umdrehungen aus der Reglerdrehzahl. g_frac trägt den Bruchteil
// zwischen zwei Durchläufen mit, sonst gingen bei jedem Aufruf die Reste verloren.
float    g_frac = 0.0f;
uint32_t g_revs = 0;
bool     g_indexFlag = false;
uint32_t g_lastIntegMs = 0;

// Geschwindigkeitsverlauf für dv/dt. Acht Proben über DVDT_WINDOW_MS, Steigung
// über die Ausgleichsgerade — eine Differenz aus nur zwei Proben rauscht zu stark,
// und genau diese Zahl entscheidet, ob ein Messpunkt gültig ist (L9/L10).
constexpr uint8_t DVDT_N = 8;
float    g_vHist[DVDT_N];
uint32_t g_tHist[DVDT_N];
uint8_t  g_dvdtCount = 0;
uint8_t  g_dvdtHead = 0;
uint32_t g_lastSampleMs = 0;

void sampleDvdt(uint32_t now, float v) {
  const uint32_t spacing = DVDT_WINDOW_MS / DVDT_N;
  if (g_lastSampleMs != 0 && (now - g_lastSampleMs) < spacing) return;
  g_lastSampleMs = now;
  g_vHist[g_dvdtHead] = v;
  g_tHist[g_dvdtHead] = now;
  g_dvdtHead = (g_dvdtHead + 1) % DVDT_N;
  if (g_dvdtCount < DVDT_N) g_dvdtCount++;
}

}  // namespace

void begin() {
  speed::begin();
  g_src = SRC_NONE;
  g_lastMarkCount = speed::markCount();
  g_lastMarkMs = 0;
  g_frac = 0.0f;
  g_revs = 0;
  g_indexFlag = false;
  g_lastIntegMs = millis();
  g_dvdtCount = 0;
  g_dvdtHead = 0;
  g_lastSampleMs = 0;
}

void service() {
  const uint32_t now = millis();

  // Quellenwahl. Marken gelten, solange sie zählen; dieselbe Grenze, mit der die
  // Zustandsmaschine Stillstand trotz Sollwert erkennt.
  const uint32_t mc = speed::markCount();
  if (mc != g_lastMarkCount) {
    g_lastMarkCount = mc;
    g_lastMarkMs = now;
    g_src = SRC_MARKS;
  } else if (g_src == SRC_MARKS && (now - g_lastMarkMs) > STALL_TIMEOUT_MS) {
    g_src = SRC_VESC;   // Marken stehen oder sind nicht verdrahtet
  } else if (g_src == SRC_NONE) {
    g_src = SRC_VESC;
  }

  if (g_src != SRC_MARKS) {
    // Umdrehungen aus der Reglerdrehzahl integrieren.
    const uint32_t dt = now - g_lastIntegMs;
    if (dt > 0) {
      g_lastIntegMs = now;
      const float u = settings::drumCircumferenceM();
      const float rps = (u > 0.01f) ? (speedctl::kmh() / 3.6f / u) : 0.0f;
      g_frac += rps * (static_cast<float>(dt) / 1000.0f);
      while (g_frac >= 1.0f) {
        g_frac -= 1.0f;
        g_revs++;
        g_indexFlag = true;
      }
    }
  } else {
    g_lastIntegMs = now;
  }

  sampleDvdt(now, kmh());
}

Source source() { return g_src; }

const char* sourceName() {
  switch (g_src) {
    case SRC_MARKS: return "marken";
    case SRC_VESC:  return "regler";
    default:        return "keine";
  }
}

bool angleReference() { return g_src == SRC_MARKS; }

float revPerSec() {
  if (g_src == SRC_MARKS) return speed::revPerSec();
  const float u = settings::drumCircumferenceM();
  return (u > 0.01f) ? (speedctl::kmh() / 3.6f / u) : 0.0f;
}

float kmh() {
  return (g_src == SRC_MARKS) ? speed::kmh() : speedctl::kmh();
}

float omega() { return 2.0f * PI * revPerSec(); }

uint32_t revCount() {
  return (g_src == SRC_MARKS) ? speed::revCount() : g_revs;
}

bool consumeIndexFlag() {
  if (g_src == SRC_MARKS) return speed::consumeIndexFlag();
  const bool f = g_indexFlag;
  g_indexFlag = false;
  return f;
}

uint8_t markIndexInRev() {
  return (g_src == SRC_MARKS) ? speed::markIndexInRev() : MARKS_PER_REV;
}

float dvdtKmhS() {
  if (g_dvdtCount < 3) return NAN;

  // Ausgleichsgerade v = a + b·t über das Fenster, b ist dv/dt.
  double sumT = 0, sumV = 0, sumTT = 0, sumTV = 0;
  const uint32_t t0 = g_tHist[(g_dvdtHead + DVDT_N - g_dvdtCount) % DVDT_N];
  for (uint8_t i = 0; i < g_dvdtCount; i++) {
    const uint8_t k = (g_dvdtHead + DVDT_N - g_dvdtCount + i) % DVDT_N;
    const double t = static_cast<double>(g_tHist[k] - t0) / 1000.0;
    const double v = g_vHist[k];
    sumT += t; sumV += v; sumTT += t * t; sumTV += t * v;
  }
  const double n = g_dvdtCount;
  const double den = n * sumTT - sumT * sumT;
  if (fabs(den) < 1e-9) return NAN;
  return static_cast<float>((n * sumTV - sumT * sumV) / den);
}

}  // namespace revsrc
