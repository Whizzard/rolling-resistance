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

// ---- Auslauf
bool     g_sdActive = false;
uint32_t g_sdRevs = 0;
uint32_t g_sdPrevUs = 0;       // Zeitstempel des vorletzten Impulses
double   g_sdPrevW = 0.0;      // w aus dem letzten Intervall
double   g_sdLastCirc = NAN;   // Umfang, mit dem J gerechnet wird
double   g_sdN = 0, g_sdSx = 0, g_sdSy = 0, g_sdSxx = 0, g_sdSxy = 0;
uint32_t g_sdLastPulseMs = 0;
float    g_sdJ = NAN;

void spindownFinish() {
  g_sdActive = false;
  const double den = g_sdN * g_sdSxx - g_sdSx * g_sdSx;
  if (g_sdRevs < SPINDOWN_MIN_REVS || g_sdN < 3 || fabs(den) < 1e-12) {
    Serial.print("# Auslauf verworfen: "); Serial.print(g_sdRevs);
    Serial.println(" Umdrehungen, zu wenige fuer einen Fit");
    return;
  }
  const float b1 = static_cast<float>((g_sdN * g_sdSxy - g_sdSx * g_sdSy) / den);
  const float a1 = static_cast<float>((g_sdSy - b1 * g_sdSx) / g_sdN);
  if (settings::setWheelModel(a1, b1)) {
    Serial.print("# Radverlustmodell gespeichert: A1="); Serial.print(a1, 5);
    Serial.print(" Nm  B1="); Serial.print(b1, 8); Serial.print(" Nm/(rad/s)^2  aus ");
    Serial.print(g_sdRevs); Serial.print(" Umdrehungen, J_Rad=");
    Serial.print(g_sdJ, 4); Serial.println(" kg m^2");
  } else {
    Serial.println("# Radverlustmodell unplausibel, nicht gespeichert");
  }
}

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
    // Kein neuer Impuls. Steht das Rad, verfaellt das Fenster -- und ein
    // laufender Auslauf ist zu Ende.
    if (g_filled && millis() - g_lastPulseMs > WHEEL_CIRC_GAP_MS) clear();
    if (g_sdActive && millis() - g_sdLastPulseMs > WHEEL_CIRC_GAP_MS) spindownFinish();
    return;
  }
  // Mehrere Impulse seit der letzten Runde (kommt bei 1 kHz Schleife nicht vor,
  // waere aber nicht trennbar) -- dann zaehlt nur der juengste.
  const uint32_t prevPulseUs = g_sdPrevUs;
  g_lastPulse = pulses;
  g_lastPulseMs = millis();

  // Auslauf: w aus dem Impulsabstand, dw/dt aus zwei Intervallen.
  if (g_sdActive) {
    g_sdLastPulseMs = millis();
    if (prevPulseUs) {
      const double dt = (lastUs - prevPulseUs) * 1e-6;
      if (dt > 1e-3) {
        const double w = 2.0 * PI / WHEEL_MARKS_PER_REV / dt;
        if (g_sdPrevW > 0.0) {
          const double dwdt = (w - g_sdPrevW) / dt;          // ueber dieses Intervall
          const double m = -static_cast<double>(g_sdJ) * dwdt; // Verlustmoment
          const double wm = 0.5 * (w + g_sdPrevW);
          g_sdN++; g_sdSx += wm * wm; g_sdSy += m; g_sdSxx += wm * wm * wm * wm; g_sdSxy += wm * wm * m;
        }
        g_sdPrevW = w;
        g_sdRevs++;
      }
    }
  }
  g_sdPrevUs = lastUs;

  g_head = (g_head + 1) % (N + 1);
  g_hist[g_head] = g_drumInt;
  if (g_filled < N + 1) g_filled++;

  if (g_filled < N + 1) { g_circ = NAN; return; }
  const double drumRevs = g_hist[g_head] - g_hist[(g_head + 1) % (N + 1)];   // aeltester
  g_circ = settings::drumCircumferenceM()
         * static_cast<float>(drumRevs / static_cast<double>(WHEEL_CIRC_WINDOW_REVS));
  // Merken, hoechstens alle 10 s und nur bei mehr als 1 mm Aenderung -- der NVS
  // soll nicht bei jeder Umdrehung beschrieben werden.
  // ... und nur auf stetiger Fahrt: in der Rampe hinkt das Trommelintegral den
  // Impulsen nach, und 1428 statt 1540 mm landeten so im Speicher (2026-11-23).
  static uint32_t lastStoreMs = 0;
  const float dvdt = revsrc::dvdtKmhS();
  if (!isnan(dvdt) && fabsf(dvdt) < 0.3f && millis() - lastStoreMs > 10000 &&
      (isnan(settings::wheelCircM()) || fabsf(settings::wheelCircM() - g_circ) > 0.001f)) {
    settings::setWheelCircM(g_circ);
    lastStoreMs = millis();
  }
}

float jRadKgm2() {
  const float m = settings::wheelMassKg();
  const float u = isnan(g_circ) ? settings::wheelCircM() : g_circ;   // live, sonst gemerkt
  if (isnan(m) || isnan(u)) return NAN;
  const float r = u / (2.0f * PI);
  return WHEEL_J_FACTOR * m * r * r;
}

bool spindownBegin() {
  g_sdJ = jRadKgm2();
  if (isnan(g_sdJ)) return false;
  g_sdActive = true;
  g_sdRevs = 0;
  g_sdPrevW = 0.0;
  g_sdPrevUs = 0;
  g_sdN = g_sdSx = g_sdSy = g_sdSxx = g_sdSxy = 0;
  g_sdLastPulseMs = millis();
  return true;
}
bool     spindownActive() { return g_sdActive; }
uint32_t spindownRevs()   { return g_sdRevs; }

bool     valid()          { return !isnan(g_circ); }
float    circumferenceM() { return g_circ; }
uint32_t revsUsed()       { return g_filled ? (g_filled - 1) / WHEEL_MARKS_PER_REV : 0; }

}  // namespace wheel
