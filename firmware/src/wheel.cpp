#include "wheel.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "revsrc.h"

namespace wheel {
namespace {

double   g_drumInt = 0.0;    // Trommelumdrehungen, aufintegriert seit begin()
uint32_t g_lastUs = 0;

bool     g_have0 = false;    // Fensteranfang gesetzt
uint32_t g_pulse0 = 0;       // Impulszähler am Fensteranfang
double   g_int0 = 0.0;       // Trommelintegral am Fensteranfang
uint32_t g_lastPulse = 0;    // zuletzt gesehener Impulszähler
uint32_t g_lastPulseMs = 0;
float    g_rpsMin = 0.0f, g_rpsMax = 0.0f;   // Trommel im Fenster
uint32_t g_revs = 0;         // Radumdrehungen im laufenden Fenster
float    g_circ = NAN;       // letzter gueltiger Umfang
uint32_t g_circRevs = 0;     // ... und aus wie vielen Umdrehungen er stammt

// Neues Fenster auf diesem Impuls. Der letzte gueltige Wert bleibt stehen, bis
// das neue Fenster einen besseren liefert -- die Anzeige soll nicht blinken.
void restart(uint32_t pulses, double integ, float rps) {
  g_have0 = true;
  g_pulse0 = pulses;
  g_int0 = integ;
  g_rpsMin = g_rpsMax = rps;
  g_revs = 0;
}

// Rad steht: Fenster und Wert verfallen.
void clear() {
  g_have0 = false;
  g_revs = 0;
  g_circ = NAN;
  g_circRevs = 0;
}

}  // namespace

void begin() {
  g_drumInt = 0.0;
  g_lastUs = micros();
  uint32_t last;
  speed::wheelPulses(g_lastPulse, last);
  clear();
}

void service() {
  // Trommel integrieren, jede Runde.
  const uint32_t nowUs = micros();
  const float rps = revsrc::revPerSec();
  g_drumInt += static_cast<double>(rps) * static_cast<double>(nowUs - g_lastUs) * 1e-6;
  g_lastUs = nowUs;

  uint32_t pulses, lastUs;
  speed::wheelPulses(pulses, lastUs);

  if (pulses == g_lastPulse) {
    // Kein neuer Impuls. Steht das Rad, ist das Fenster hinfällig.
    if (g_have0 && millis() - g_lastPulseMs > WHEEL_CIRC_GAP_MS) clear();
    return;
  }
  g_lastPulse = pulses;
  g_lastPulseMs = millis();

  if (!g_have0 || rps < 0.2f) { restart(pulses, g_drumInt, rps); return; }

  // Stetigkeit: ändert sich die Trommeldrehzahl im Fenster zu stark, ist das
  // Verhältnis nicht mehr sauber — neu beginnen, auf diesem Impuls.
  if (rps < g_rpsMin) g_rpsMin = rps;
  if (rps > g_rpsMax) g_rpsMax = rps;
  const float mean = 0.5f * (g_rpsMin + g_rpsMax);
  if (mean > 0.0f && (g_rpsMax - g_rpsMin) / mean > WHEEL_CIRC_STEADY) {
    restart(pulses, g_drumInt, rps);
    return;
  }

  const uint32_t revs = (pulses - g_pulse0) / WHEEL_MARKS_PER_REV;
  g_revs = revs;
  if (revs >= WHEEL_CIRC_MIN_REVS) {
    g_circ = settings::drumCircumferenceM()
           * static_cast<float>((g_drumInt - g_int0) / static_cast<double>(revs));
    g_circRevs = revs;
  }
}

bool     valid()          { return !isnan(g_circ); }
float    circumferenceM() { return g_circ; }
uint32_t revsUsed()       { return valid() ? g_circRevs : g_revs; }

}  // namespace wheel
