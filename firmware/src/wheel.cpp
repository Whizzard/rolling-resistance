#include "wheel.h"
#include "config.h"
#include "settings.h"
#include "speed.h"
#include "revsrc.h"

namespace wheel {
namespace {

double   g_drumInt = 0.0;    // Trommelumdrehungen, aufintegriert seit begin()
uint32_t g_lastUs = 0;

// Gleitendes Fenster: das Trommelintegral an den letzten WHEEL_CIRC_WINDOW_REVS + 1
// Radimpulsen (bei einem Magneten je Umdrehung ist ein Impuls eine Umdrehung).
constexpr uint8_t N = WHEEL_CIRC_WINDOW_REVS * WHEEL_MARKS_PER_REV;
double   g_hist[N + 1];
uint8_t  g_head = 0;         // juengster Eintrag
uint32_t g_filled = 0;       // Eintraege seit dem letzten Stillstand
uint32_t g_lastPulse = 0;    // zuletzt gesehener Impulszaehler
uint32_t g_lastPulseMs = 0;
float    g_circ = NAN;

void clear() {
  g_filled = 0;
  g_circ = NAN;
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
    // Kein neuer Impuls. Steht das Rad, verfaellt das Fenster.
    if (g_filled && millis() - g_lastPulseMs > WHEEL_CIRC_GAP_MS) clear();
    return;
  }
  // Mehrere Impulse seit der letzten Runde (kommt bei 1 kHz Schleife nicht vor,
  // waere aber nicht trennbar) -- dann zaehlt nur der juengste.
  g_lastPulse = pulses;
  g_lastPulseMs = millis();

  g_head = (g_head + 1) % (N + 1);
  g_hist[g_head] = g_drumInt;
  if (g_filled < N + 1) g_filled++;

  if (g_filled < N + 1) { g_circ = NAN; return; }
  const double drumRevs = g_hist[g_head] - g_hist[(g_head + 1) % (N + 1)];   // aeltester
  g_circ = settings::drumCircumferenceM()
         * static_cast<float>(drumRevs / static_cast<double>(WHEEL_CIRC_WINDOW_REVS));
}

bool     valid()          { return !isnan(g_circ); }
float    circumferenceM() { return g_circ; }
uint32_t revsUsed()       { return g_filled ? (g_filled - 1) / WHEEL_MARKS_PER_REV : 0; }

}  // namespace wheel
