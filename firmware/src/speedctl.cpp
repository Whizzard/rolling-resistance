#include "speedctl.h"
#include "config.h"
#include "settings.h"
#include "vesc/vesc.h"
#include "revsrc.h"
#include "logger.h"

namespace speedctl {
namespace {

float    g_target = 0.0f;   // gewuenschte km/h
float    g_ramp   = 0.0f;   // gerampter innerer Sollwert
float    g_duty   = 0.0f;   // Betrag des Stellwerts, Vorzeichen kommt aus config
bool     g_run    = false;
bool     g_fresh  = true;   // beim naechsten Durchlauf aus der Istdrehzahl vorbesetzen
uint32_t g_lastMs = 0;
const char* g_refusal = "";
bool     g_learn  = false;  // Drehrichtung wird gerade angelernt
uint32_t g_learnStart = 0;
bool     g_brk    = false;  // Losbrechmessung laeuft
float    g_brkDuty = 0.0f;  // Ergebnis: Tastverhaeltnis beim Losbrechen
float    g_brkAmps = 0.0f;
bool     g_motorOpen = false;   // Befund "kein Motor am Regler", rastend
float    g_out = 0.0f;          // zuletzt gesendeter Stellwert (I + P + D)
bool     g_braking = false;     // Abstieg: Bremsstrom statt Stellwert
uint32_t g_openSince = 0;

// Mechanische Drehzahl der Trommel aus der Reglerdrehzahl. Vorzeichenbehaftet:
// in Messrichtung ist ERPM negativ (VESC_DIRECTION_SIGN).
float rpmSigned() {
  return static_cast<float>(vesc::values().erpm) / MOTOR_POLE_PAIRS;
}

// km/h in Messrichtung, also positiv, wenn die Trommel richtig herum dreht.
float kmhSigned() {
  return kmhFromRevPerSec(rpmSigned() / 60.0f) * settings::directionSign();
}

float rpmFromKmh(float kmh) { return revPerSecFromKmh(kmh) * 60.0f; }

// Jede Freigabe, die die Regelung VON SICH AUS macht, wird gemeldet: auf die
// Konsole und ueber logger::lastNote() in die Oberflaeche. Vorher geschah das
// still, und zwei abgebrochene Starts am 2026-11-23 liessen sich nicht erklaeren.
void dropout(const char* why) {
  g_run = false;
  g_braking = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  vesc::release();
  g_refusal = why;
  logger::note(why);
}
// Im Stillstand ohne Sollwert verfaellt der Befund von selbst, samt Meldung.
void clearMotorOpenIfStopped() {
  if (g_motorOpen && !g_run && fabsf(rpmSigned()) < 5.0f) {
    g_motorOpen = false;
    logger::note("");
  }
}

// Leerlaufdrehzahl bei Vollaussteuerung und der MOMENTANEN Busspannung.
//
// Dass die Spannung eingeht, ist nicht Feinschliff: am 2026-11-17 brach das Netzteil
// unter Last von 40,0 auf 33,8 V ein, und eine auf 40 V gerechnete Klemme liess den
// Hochlauf dann bei 94 km/h haengen.
float noLoadRpm() {
  const float v = vesc::values().vIn;
  return MOTOR_NO_LOAD_RPM_PER_V * (v > 10.0f ? v : 10.0f);
}

}  // namespace

void begin() {
  g_target = g_ramp = g_duty = 0.0f;
  g_run = false;
  g_fresh = true;
  g_lastMs = millis();
  g_refusal = "";
  g_motorOpen = false;
  g_openSince = 0;
  if (ESTOP_FB_WIRED) pinMode(PIN_ESTOP_FB, INPUT_PULLUP);
}

bool setTargetKmh(float kmh) {
  g_refusal = "";
  g_motorOpen = false;
  g_openSince = 0;
  g_braking = false;

  if (kmh <= 0.0f) { coast(); return true; }

  if (kmh > V_MAX_KMH) {
    g_refusal = "ueber V_MAX_KMH — Freigabe nach docs/07-sicherheit.md §5 fehlt";
    return false;
  }

  // Richtungswechsel im Lauf ist ein Bremsbefehl (docs/07-sicherheit.md §3.3).
  if (kmhSigned() < -1.0f) {
    g_refusal = "Trommel dreht rueckwaerts — erst Stillstand abwarten";
    return false;
  }

  if (!g_run) g_fresh = true;   // Anfahren: Stellwert aus der Istdrehzahl vorbesetzen
  g_target = kmh;
  g_run    = true;
  return true;
}

void service() {
  const uint32_t now = millis();
  if (now - g_lastMs < SPEEDCTL_PERIOD_MS) return;
  const float dt = (now - g_lastMs) / 1000.0f;
  g_lastMs = now;

  if (g_learn) {
    // Vorzeichenfrei messen: welches Vorzeichen hat die Reglerdrehzahl, wenn
    // der Mensch in Messrichtung dreht? Genau das ist directionSign().
    const float roh = kmhFromRevPerSec(rpmSigned() / 60.0f);
    if (fabsf(roh) >= SPEEDCTL_LEARN_MIN_KMH) {
      const int8_t neu = (roh > 0.0f) ? 1 : -1;
      g_learn = false;
      g_refusal = settings::setDirectionSign(neu)
                    ? "" : "Vorzeichen nicht gespeichert";
      return;
    }
    if (now - g_learnStart > SPEEDCTL_LEARN_TIMEOUT_MS) {
      g_learn = false;
      g_refusal = "nichts gedreht \u2014 Anlernen abgebrochen";
    }
    return;
  }

  if (g_brk) {
    if (!vesc::connected()) { g_brk = false; dropout("Abbruch: keine Messwerte vom Regler"); return; }
    const float ist = kmhSigned();
    if (fabsf(ist) >= BREAKAWAY_DETECT_KMH) {
      g_brkDuty = g_duty;
      g_brkAmps = vesc::values().currentMotor;
      g_brk = false;
      coast();
      return;
    }
    g_duty += BREAKAWAY_RATE_PER_S * dt;
    if (g_duty > BREAKAWAY_DUTY_MAX) {
      g_brk = false;
      g_brkDuty = 0.0f;          // nicht losgebrochen -- keine erfundene Zahl
      g_brkAmps = vesc::values().currentMotor;
      coast();
      g_refusal = "nicht losgebrochen bis zur Obergrenze";
      return;
    }
    vesc::setDuty(g_duty * settings::directionSign());
    return;
  }

  if (!g_run) { clearMotorOpenIfStopped(); return; }

  // Ohne Messwerte vom Regler wird nicht gestellt — blind das Tastverhaeltnis
  // hochzufahren waere genau der Fehler, gegen den die Vorsteuerung gebaut ist.
  if (!vesc::connected()) {
    dropout("Abbruch: seit ueber 1 s keine Messwerte vom Regler (Schleife blockiert?)");
    return;
  }

  const float istKmh = kmhSigned();

  // Herunterfahren: bremsen statt rekuperieren (config.h SPEEDCTL_BRAKE_*).
  if (istKmh > g_target + SPEEDCTL_BRAKE_ABOVE_KMH) g_braking = true;
  if (g_braking) {
    if (istKmh > g_target + SPEEDCTL_BRAKE_UNTIL_KMH) {
      vesc::setBrakeCurrent(VESC_BRAKE_CURRENT_STOP);
      g_out = 0.0f;
      return;
    }
    g_braking = false;
    g_fresh = true;              // auf dem Sollwert wieder anknuepfen
  }

  if (g_fresh) {
    // Dort anknuepfen, wo die Trommel dreht. Siehe Regel 1 in speedctl.h.
    g_fresh = false;
    g_ramp  = istKmh > 0.0f ? istKmh : 0.0f;
    g_duty  = rpmFromKmh(g_ramp) / noLoadRpm();
    if (g_duty < 0.0f) g_duty = 0.0f;
  }

  // Sollwert rampen, vor dem Ziel abflachend (config.h SPEEDCTL_APPROACH_*)
  if (g_ramp < g_target) {
    float k = (g_target - g_ramp) / SPEEDCTL_APPROACH_KMH;
    if (k > 1.0f) k = 1.0f;
    if (k < SPEEDCTL_APPROACH_MIN) k = SPEEDCTL_APPROACH_MIN;
    g_ramp += SPEEDCTL_RAMP_KMH_PER_S * k * dt;
    if (g_ramp > g_target) g_ramp = g_target;
    // ... und nie weiter als SPEEDCTL_LEAD_KMH vor der Trommel (config.h).
    const float lead = (istKmh > 0.0f ? istKmh : 0.0f) + SPEEDCTL_LEAD_KMH;
    if (g_ramp > lead && lead < g_target) g_ramp = lead;
  } else if (g_ramp > g_target) {
    g_ramp -= SPEEDCTL_RAMP_KMH_PER_S * dt;
    if (g_ramp < g_target) g_ramp = g_target;
  }

  // Klemme aus der Leerlaufkennlinie des Sollwerts (Regel 3)
  float klemme = rpmFromKmh(g_ramp) / noLoadRpm() * SPEEDCTL_CLAMP_FACTOR
                 + SPEEDCTL_CLAMP_OFFSET;
  // Anfahrfenster: steht die Trommel noch, darf der Stellwert bis zum Losbrechen
  // hoch -- sonst wartet sie unter Strom, bis die Rampe es erlaubt (config.h).
  if (istKmh < 1.0f && g_ramp > 1.0f && klemme < SPEEDCTL_START_DUTY)
    klemme = SPEEDCTL_START_DUTY;
  if (klemme > 0.95f) klemme = 0.95f;

  // Trimm als PI-D (config.h), Aenderung je Runde asymmetrisch begrenzt (Regel 2).
  // g_duty ist der I-Anteil samt Vorbesetzung; P und D kommen je Runde obendrauf
  // und werden nicht integriert.
  const float err  = g_ramp - istKmh;
  const float dvdtNow = revsrc::dvdtKmhS();
  // D nur auf die Beschleunigung UEBER die Rampe hinaus: solange die Rampe
  // steigt, ist Beschleunigen gewollt, und ein D dagegen vergroesserte nur
  // den Nachlauf und damit das Ueberschwingen (2,1 km/h am 2026-11-23).
  const float wanted = (g_ramp < g_target - 0.01f) ? SPEEDCTL_RAMP_KMH_PER_S : 0.0f;
  const float dTerm = isnan(dvdtNow) ? 0.0f : -SPEEDCTL_KD * (dvdtNow - wanted);
  const float pTerm = SPEEDCTL_KPP * err;
  float       step = SPEEDCTL_KI * err * dt;
  const float up   = SPEEDCTL_DUTY_UP_PER_S   * dt;
  const float down = SPEEDCTL_DUTY_DOWN_PER_S * dt;
  if (step >  up)   step =  up;
  if (step < -down) step = -down;

  // Anti-Windup: liegt der Stellwert an der Klemme und die Abweichung draengt
  // weiter nach oben, integriert der I-Anteil nicht -- sonst steht beim
  // Erreichen des Sollwerts ein Ueberschuss, der erst abgebaut werden muss.
  const bool saturated = (g_duty >= klemme - 1e-4f) && err > 0.0f;
  if (!saturated) g_duty += step;
  if (g_duty > klemme) g_duty = klemme;
  if (g_duty < 0.0f)   g_duty = 0.0f;
  float out = g_duty + pTerm + dTerm;
  if (out > klemme) out = klemme;
  if (out < 0.0f)   out = 0.0f;

  // Not-Aus erkennen (speedctl.h, config.h ESTOP_*): Stellwert da, kein Strom,
  // UND die Drehzahl liegt weit unter der Leerlaufdrehzahl zu diesem Stellwert.
  // Nur Strom: Fehlalarm im Leerlauf (0,2 A bei 28 km/h). Nur Stillstand: die
  // Hall-Geber melden die auslaufende Trommel noch 25 s (beides 2026-11-23).
  const float imot = fabsf(vesc::values().currentMotor);
  const float rpmNoLoad = g_duty * noLoadRpm();
  const bool  lagging = rpmNoLoad > 0.0f &&
                        fabsf(rpmSigned()) < ESTOP_SPEED_RATIO * rpmNoLoad;
  const float dvdt = revsrc::dvdtKmhS();
  const bool  rampingDown = g_ramp > g_target + 0.1f;
  const bool  decel = !isnan(dvdt) && dvdt < -ESTOP_DECEL_KMH_S && !rampingDown;
  if (out > STALL_DUTY_MIN && imot < ESTOP_IMOT_MAX_A && (lagging || decel)) {
    const uint32_t now = millis();
    if (g_openSince == 0) {
      g_openSince = now;
    } else if (now - g_openSince >= ESTOP_DETECT_MS) {
      g_motorOpen = true;
      dropout("Abbruch: kein Motorstrom bei Stellwert — Not-Aus ausgeloest oder Motor abgeklemmt");
      return;
    }
  } else {
    g_openSince = 0;
  }

  g_out = out;
  vesc::setDuty(out * settings::directionSign());
}

bool motorOpen()  { return g_motorOpen; }
bool estopInput() { return ESTOP_FB_WIRED && digitalRead(PIN_ESTOP_FB) == LOW; }

float targetKmh() { return g_target; }
float rampKmh()   { return g_run ? g_ramp : 0.0f; }
float kmh()       { const float k = kmhSigned(); return k > 0.0f ? k : 0.0f; }
// Auch waehrend der Losbrechmessung liegt ein Stellwert an -- ohne g_brk hier
// zeigte die Anzeige durchgehend 0, und genau diese Zahl ist das Messergebnis.
float duty()      { return g_run ? g_out : (g_brk ? g_duty : 0.0f); }
bool  running()   { return g_run; }
bool  braking()   { return g_run && g_braking; }

void stop() {
  g_run = false;
  g_braking = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  vesc::setBrakeCurrent(VESC_BRAKE_CURRENT_STOP);
}

void coast() {
  g_run = false;
  g_braking = false;
  g_target = g_ramp = g_duty = g_out = 0.0f;
  g_fresh = true;
  vesc::release();
}

void learn() {
  // Nur im Stillstand und nur mit freigegebenem Antrieb: wer von Hand dreht,
  // soll nicht gegen ein Haltemoment arbeiten.
  coast();
  g_learn = true;
  g_learnStart = millis();
  g_refusal = "";
}

bool learning() { return g_learn; }

void breakaway() {
  coast();
  g_duty = 0.0f;
  g_brkDuty = g_brkAmps = 0.0f;
  g_brk = true;
  g_refusal = "";
}

bool  breakingAway()  { return g_brk; }
float breakawayDuty() { return g_brkDuty; }
float breakawayAmps() { return g_brkAmps; }

const char* lastRefusal() { return g_refusal; }

}  // namespace speedctl
