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
// INTEGRALMETHODE statt punktweiser Differenz (docs/12-methodology-review.md W8):
// aus J dw/dt = -(A1 + B1 w^2) folgt durch Integration ueber die Zeit
//     w(t) = w0 - (A1/J) t - (B1/J) * Int_0^t w^2 dt .
// Das ist LINEAR in den drei Unbekannten w0, a = A1/J, b = B1/J, mit den Regressoren
// t und I(t) = Int w^2 dt (Trapez ueber die Impulsabstaende). Keine Differenz zweier
// Perioden, also kein Rauschen aus der Zeitstempelaufloesung; der Fit nutzt jeden
// Impuls gleichwertig. Geloest werden die Normalgleichungen 3x3 (Cramer).
bool     g_sdActive = false;
uint32_t g_sdRevs = 0;
uint32_t g_sdPrevUs = 0;       // Zeitstempel des letzten Impulses
uint32_t g_sdStartUs = 0;
double   g_sdPrevW = 0.0;      // w aus dem letzten Intervall
double   g_sdI = 0.0;          // Int w^2 dt bisher
// Normalgleichungen: Summen ueber x = (1, t, I), y = w
double   g_sN = 0, g_sT = 0, g_sI = 0, g_sTT = 0, g_sTI = 0, g_sII = 0, g_sY = 0, g_sYT = 0, g_sYI = 0, g_sYY = 0;
uint32_t g_sdLastPulseMs = 0;
float    g_sdJ = NAN;
float    g_sdResidRms = NAN;   // Residuum des letzten Fits, rad/s
uint32_t g_sdFitN = 0;

static double det3(double a, double b, double c, double d, double e, double f, double g, double h, double i) {
  return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}

void spindownFinish() {
  g_sdActive = false;
  if (g_sdRevs < SPINDOWN_MIN_REVS || g_sN < 4) {
    Serial.print("# Auslauf verworfen: "); Serial.print(g_sdRevs);
    Serial.println(" Umdrehungen, zu wenige fuer einen Fit");
    return;
  }
  // [N sT sI; sT sTT sTI; sI sTI sII] * [w0 -a -b]^T = [sY sYT sYI]
  const double D = det3(g_sN, g_sT, g_sI, g_sT, g_sTT, g_sTI, g_sI, g_sTI, g_sII);
  if (fabs(D) < 1e-18) { Serial.println("# Auslauf verworfen: Normalgleichungen singulaer"); return; }
  const double w0 = det3(g_sY, g_sT, g_sI, g_sYT, g_sTT, g_sTI, g_sYI, g_sTI, g_sII) / D;
  const double ca = det3(g_sN, g_sY, g_sI, g_sT, g_sYT, g_sTI, g_sI, g_sYI, g_sII) / D;   // = -a
  const double cb = det3(g_sN, g_sT, g_sY, g_sT, g_sTT, g_sYT, g_sI, g_sTI, g_sYI) / D;   // = -b
  // Residuum aus den Summen: RSS = sYY - w0 sY - ca sYT - cb sYI
  const double rss = g_sYY - w0 * g_sY - ca * g_sYT - cb * g_sYI;
  g_sdResidRms = rss > 0 ? static_cast<float>(sqrt(rss / g_sN)) : 0.0f;
  g_sdFitN = static_cast<uint32_t>(g_sN);
  const float a1 = static_cast<float>(-ca * g_sdJ);
  const float b1 = static_cast<float>(-cb * g_sdJ);
  if (settings::setWheelModel(a1, b1)) {
    Serial.print("# Radverlustmodell gespeichert: A1="); Serial.print(a1, 5);
    Serial.print(" Nm  B1="); Serial.print(b1, 8); Serial.print(" Nm/(rad/s)^2  aus ");
    Serial.print(g_sdRevs); Serial.print(" Umdrehungen, Residuum ");
    Serial.print(g_sdResidRms, 3); Serial.print(" rad/s, w0="); Serial.print(w0, 2);
    Serial.print(", J_Rad="); Serial.print(g_sdJ, 4);
    Serial.println(isnan(settings::wheelJKgm2()) ? " kg m^2 (GESCHAETZT, WHEEL_J_FACTOR)" : " kg m^2 (gemessen)");
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

  // Auslauf: w aus dem Impulsabstand; je Impuls ein Punkt (t, I, w) fuer den Fit.
  if (g_sdActive) {
    g_sdLastPulseMs = millis();
    if (prevPulseUs) {
      const double dt = (lastUs - prevPulseUs) * 1e-6;
      if (dt > 1e-3) {
        const double w = 2.0 * PI / WHEEL_MARKS_PER_REV / dt;   // mittleres w dieses Intervalls
        // Unter SPINDOWN_MIN_KMH am Rad ist Schluss -- bis zum Stillstand dauert es
        // Minuten, und die langsamen Umdrehungen tragen kaum noch bei.
        const double uRad = isnan(g_circ) ? settings::wheelCircM() : g_circ;
        if (!isnan(uRad) && w * uRad / (2.0 * PI) * 3.6 < SPINDOWN_MIN_KMH) {
          spindownFinish();
          g_sdPrevUs = lastUs;
          g_head = (g_head + 1) % (N + 1); g_hist[g_head] = g_drumInt;
          if (g_filled < N + 1) g_filled++;
          return;
        }
        if (g_sdStartUs == 0) g_sdStartUs = prevPulseUs;
        // w gilt in der Intervallmitte; I = Int w^2 dt per Trapez bis dorthin.
        if (g_sdPrevW > 0.0) g_sdI += 0.5 * (g_sdPrevW * g_sdPrevW + w * w) * dt;
        const double t = (static_cast<double>(lastUs - g_sdStartUs) - 0.5e6 * dt) * 1e-6;
        g_sN++; g_sT += t; g_sI += g_sdI; g_sTT += t * t; g_sTI += t * g_sdI; g_sII += g_sdI * g_sdI;
        g_sY += w; g_sYT += w * t; g_sYI += w * g_sdI; g_sYY += w * w;
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
  // Gemessen (Bifilarpendel, settings) schlaegt geschaetzt.
  if (!isnan(settings::wheelJKgm2())) return settings::wheelJKgm2();
  const float m = settings::wheelMassKg();
  // Der GEMERKTE Umfang stammt aus konstanter Fahrt; der Livewert ist beim
  // Abheben vom Schlupf verfaelscht (1,70 statt 1,54 m am 2026-11-23).
  const float u = isnan(settings::wheelCircM()) ? g_circ : settings::wheelCircM();
  if (isnan(m) || isnan(u)) return NAN;
  const float r = u / (2.0f * PI);
  return WHEEL_J_FACTOR * m * r * r;
}
bool jRadMeasured() { return !isnan(settings::wheelJKgm2()); }
float spindownResidRadS() { return g_sdResidRms; }
uint32_t spindownFitN() { return g_sdFitN; }

bool spindownBegin() {
  g_sdJ = jRadKgm2();
  if (isnan(g_sdJ)) return false;
  g_sdActive = true;
  g_sdRevs = 0;
  g_sdPrevW = 0.0;
  g_sdPrevUs = 0;
  g_sdStartUs = 0;
  g_sdI = 0.0;
  g_sN = g_sT = g_sI = g_sTT = g_sTI = g_sII = g_sY = g_sYT = g_sYI = g_sYY = 0;
  g_sdResidRms = NAN; g_sdFitN = 0;
  g_sdLastPulseMs = millis();
  return true;
}
bool     spindownActive() { return g_sdActive; }
uint32_t spindownRevs()   { return g_sdRevs; }

bool     valid()          { return !isnan(g_circ); }
float    circumferenceM() { return g_circ; }
uint32_t revsUsed()       { return g_filled ? (g_filled - 1) / WHEEL_MARKS_PER_REV : 0; }

}  // namespace wheel
