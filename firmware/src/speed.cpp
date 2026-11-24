#include "speed.h"
#include "config.h"
#include "settings.h"

namespace speed {
namespace {

// ---------------------------------------------------------------- Trommel
// Die ISR legt NUR Zeitstempel ab (CLAUDE.md: ISRs kurz). Alles, was daraus eine
// Umdrehung, eine Markennummer oder das Muster des Zwischenmagneten macht, rechnet
// service() in der Hauptschleife aus dem Ring.
constexpr uint8_t BUF = 16;   // >= Impulse je Umdrehung (8 oder 9) + 1

volatile uint32_t g_markUs[BUF] = {0};
volatile uint32_t g_markCount = 0;      // Impuls n liegt bei g_markUs[n % BUF]
volatile uint32_t g_lastMarkUs = 0;

volatile uint32_t g_revCount = 0;
volatile bool     g_indexFlag = false;
volatile bool     g_indexSeen = false;  // eigener Index-Hall verdrahtet
volatile bool     g_idxResync = false;  // Index kam, naechste Marke ist Marke 0

// Auswertung (nur service()):
uint32_t g_seen = 0;                    // bis hierher verarbeitete Impulse
uint8_t  g_markInRev = 0;               // zuletzt gesehene REGULAERE Marke, 0..MARKS_PER_REV-1
uint8_t  g_pulseInRev = 0;              // Impulse seit Marke 0, inkl. Zwischenimpuls
uint8_t  g_pulsesPerRev = MARKS_PER_REV;
bool     g_pattern = false;             // Zwischenmagnet bestaetigt
bool     g_synced = false;              // Marke 0 schon einmal aus dem Muster gesetzt
uint8_t  g_patternHits = 0;
uint16_t g_sinceIndex = 0xFFFF;         // Impulse seit dem letzten Zwischenimpuls
float    g_refUs = 0.0f;                // laufendes Mittel der regulaeren Intervalle
bool     g_prevShort = false;
uint32_t g_resyncs = 0, g_missed = 0, g_glitches = 0;

volatile uint32_t g_motorLastUs = 0;
volatile uint32_t g_motorPeriodUs = 0;

// Laufrad: Ringpuffer wie an der Trommel.
constexpr uint8_t WBUF = WHEEL_MARKS_PER_REV + 1;
volatile uint32_t g_wheelUs[WBUF] = {0};
volatile uint8_t  g_wheelHead = 0;
volatile uint32_t g_wheelCount = 0;

// Verdrahtungstest: eigene Zähler je Kanal (nullbar) und der letzte Zeitstempel.
// Gezählt wird, was auch die Messung sieht — also hinter dem Störfilter.
volatile uint32_t g_chCount[CH_N]  = {0};
volatile uint32_t g_chLastUs[CH_N] = {0};
volatile uint32_t g_wheelLastUs = 0;
constexpr uint8_t CH_PIN[CH_N] = {PIN_MARK, PIN_INDEX, PIN_WHEEL, PIN_MOTOR_HALL};
const char* const CH_NAME[CH_N] = {"Marken", "Index", "Laufrad", "Motor"};

void IRAM_ATTR tick(Channel ch, uint32_t nowUs) {
  g_chCount[ch]++;
  g_chLastUs[ch] = nowUs;
}

void IRAM_ATTR onMark() {
  const uint32_t now = micros();
  // Störimpulse und Prellen ausfiltern. Der kürzeste echte Abstand liegt beim
  // Zwischenmagneten (22,5°) bei 60 km/h bei 10,6 ms, bei 100 km/h bei 6,4 ms.
  if (now - g_lastMarkUs < MIN_PULSE_US) return;
  g_lastMarkUs = now;
  g_markUs[g_markCount % BUF] = now;
  g_markCount++;
  tick(CH_MARK, now);
}

void IRAM_ATTR onIndex() {
  g_indexSeen = true;
  g_idxResync = true;   // Index definiert den Winkelnullpunkt: naechste Marke ist Marke 0
  g_revCount++;
  g_indexFlag = true;
  tick(CH_INDEX, micros());
}

void IRAM_ATTR onMotorHall() {
  const uint32_t now = micros();
  const uint32_t dt = now - g_motorLastUs;
  if (dt < 100) return;  // Störfilter; bei 480 1/min und 15 Flanken sind es 8,3 ms
  g_motorPeriodUs = dt;
  g_motorLastUs = now;
  tick(CH_MOTOR, now);
}

void IRAM_ATTR onWheel() {
  const uint32_t now = micros();
  if (now - g_wheelLastUs < MIN_PULSE_US) return;
  g_wheelLastUs = now;
  g_wheelHead = (g_wheelHead + 1) % WBUF;
  g_wheelUs[g_wheelHead] = now;
  g_wheelCount++;
  tick(CH_WHEEL, now);
}

void resetPattern() {
  g_pattern = false; g_synced = false; g_patternHits = 0; g_sinceIndex = 0xFFFF;
  g_pulsesPerRev = MARKS_PER_REV; g_refUs = 0.0f; g_prevShort = false; g_pulseInRev = 0;
}

inline void newRev() { g_revCount++; g_indexFlag = true; g_pulseInRev = 0; }

}  // namespace

void begin() {
  pinMode(PIN_MARK, INPUT_PULLUP);
  pinMode(PIN_INDEX, INPUT_PULLUP);
  pinMode(PIN_WHEEL, INPUT_PULLUP);
  pinMode(PIN_MOTOR_HALL, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_MARK), onMark, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_INDEX), onIndex, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_WHEEL), onWheel, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_MOTOR_HALL), onMotorHall, FALLING);
}

// Umdrehungen und Markennummer aus den Zeitstempeln. Drei Faelle:
//   Index-Hall verdrahtet:  Index zaehlt die Umdrehung, naechste Marke ist Marke 0.
//   Zwischenmagnet erkannt: zwei kurze Intervalle (je ~22,5°) -- der Impuls dazwischen
//                           ist der Zwischenimpuls, der danach Marke 0. 9 Impulse je
//                           Umdrehung, 8 regulaere Marken; das Muster rastet die Zaehlung
//                           jede Umdrehung neu ein (verlorene oder zusaetzliche Impulse
//                           werden gezaehlt und korrigiert).
//   nur 8 Marken:           jede MARKS_PER_REV-te Flanke ist eine Umdrehung, Marke 0 ist
//                           die erste gesehene.
// Bestaetigt wird das Muster erst, wenn es MARK_PATTERN_CONFIRM Mal im Abstand von genau
// MARKS_PER_REV + 1 Impulsen kam -- ein einzelner Stoerimpuls erzeugt auch zwei kurze
// Intervalle, aber nicht periodisch.
void service() {
  noInterrupts();
  const uint32_t cnt = g_markCount;
  interrupts();
  if (stalled()) { if (g_pattern || g_refUs > 0) resetPattern(); }
  if (g_seen + BUF <= cnt) g_seen = cnt - 1;   // zu weit zurueck: aufholen, Rest verwerfen
  while (g_seen < cnt) {
    const uint32_t n = g_seen;
    const uint32_t t = g_markUs[n % BUF];
    const bool havePrev = n >= 1 && (n - 1) + BUF > cnt;
    const uint32_t dt = havePrev ? t - g_markUs[(n - 1) % BUF] : 0;
    g_seen++;

    if (g_idxResync) { g_idxResync = false; g_markInRev = 0; g_pulseInRev = 0; g_prevShort = false; continue; }

    const bool classify = havePrev && g_refUs > 0.0f && dt < 1000000UL;
    const bool shortDt  = classify && dt < MARK_SHORT_RATIO * g_refUs;
    const bool longDt   = classify && dt > MARK_MISSED_RATIO * g_refUs;
    if (havePrev && !shortDt && !longDt && dt < 1000000UL) g_refUs = g_refUs > 0 ? 0.8f * g_refUs + 0.2f * dt : dt;
    if (longDt) g_missed++;
    if (g_sinceIndex < 0xFFFF) g_sinceIndex++;

    // Zwei kurze Intervalle hintereinander: Impuls n-1 war der Zwischenimpuls, n ist Marke 0.
    if (MARK_PATTERN_INDEX && !g_indexSeen && shortDt && g_prevShort) {
      g_prevShort = false;
      if (!g_pattern) {
        g_patternHits = (g_sinceIndex == MARKS_PER_REV + 1) ? g_patternHits + 1 : 1;
        g_sinceIndex = 0;
        if (g_patternHits >= MARK_PATTERN_CONFIRM) { g_pattern = true; g_pulsesPerRev = MARKS_PER_REV + 1; }
        else { g_markInRev = (g_markInRev + 1) % MARKS_PER_REV; if (g_markInRev == 0) newRev(); continue; }
      } else {
        // Nahe der erwarteten Stelle? Sonst ein Stoerimpuls, der zwei Haelften macht.
        if (g_synced && g_pulseInRev < MARKS_PER_REV - 1) { g_glitches++; g_markInRev = (g_markInRev + 1) % MARKS_PER_REV; g_pulseInRev++; continue; }
        if (g_synced && (g_markInRev != MARKS_PER_REV - 1 || g_pulseInRev != MARKS_PER_REV)) g_resyncs++;
        g_sinceIndex = 0;
      }
      g_synced = true;
      g_markInRev = 0;
      newRev();
      continue;
    }
    g_prevShort = shortDt;

    if (g_pattern) {
      g_pulseInRev++;
      if (shortDt) continue;                       // erster kurzer: Zwischenimpuls, keine Marke
      g_markInRev = (g_markInRev + 1) % MARKS_PER_REV;
      if (g_markInRev == 0) { g_missed++; newRev(); }   // Muster ausgeblieben: nach Zaehlung
    } else {
      g_markInRev = (g_markInRev + 1) % MARKS_PER_REV;
      if (g_markInRev == 0 && !g_indexSeen) newRev();
    }
  }
}

float revPerSec() {
  noInterrupts();
  const uint32_t cnt = g_markCount;
  const uint32_t lastUs = g_lastMarkUs;
  interrupts();
  const uint8_t ppr = g_pulsesPerRev;
  if (cnt <= ppr) return 0.0f;
  const uint32_t tNew = g_markUs[(cnt - 1) % BUF];
  const uint32_t tOld = g_markUs[(cnt - 1 - ppr) % BUF];

  const uint32_t span = tNew - tOld;   // eine volle Umdrehung, unabhaengig von der Teilung
  if (span == 0) return 0.0f;

  // Steht die Trommel gerade? Dann ist der letzte Zeitstempel alt.
  if (micros() - lastUs > span * 2 + 100000UL) return 0.0f;

  return 1000000.0f / static_cast<float>(span);
}

float kmh() { return kmhFromRevPerSec(revPerSec()); }
float omega() { return omegaFromRevPerSec(revPerSec()); }

uint32_t markCount() { return g_markCount; }
uint32_t revCount() { return g_revCount; }

bool consumeIndexFlag() {
  if (!g_indexFlag) return false;
  g_indexFlag = false;
  return true;
}

uint8_t  markIndexInRev() { return g_markInRev; }
uint8_t  pulsesPerRev()   { return g_pulsesPerRev; }
RevSource revSource() {
  if (g_indexSeen) return REV_INDEX_HALL;
  if (g_pattern)   return REV_PATTERN;
  return REV_COUNT;
}
uint32_t patternResyncs() { return g_resyncs; }
uint32_t missedPulses()   { return g_missed; }
uint32_t glitchPulses()   { return g_glitches; }

float motorRevPerSec() {
  noInterrupts();
  const uint32_t period = g_motorPeriodUs;
  const uint32_t last = g_motorLastUs;
  interrupts();

  if (period == 0) return 0.0f;
  if (micros() - last > 500000UL) return 0.0f;  // steht
  return 1000000.0f / (static_cast<float>(period) * MOTOR_PULSES_PER_REV);
}

void wheelPulses(uint32_t& count, uint32_t& lastUs) {
  noInterrupts();
  count = g_wheelCount;
  lastUs = g_wheelLastUs;
  interrupts();
}

float wheelRevPerSec() {
  if (g_wheelCount <= WHEEL_MARKS_PER_REV) return 0.0f;

  noInterrupts();
  const uint8_t head = g_wheelHead;
  const uint32_t tNew = g_wheelUs[head];
  const uint32_t tOld = g_wheelUs[(head + WBUF - WHEEL_MARKS_PER_REV) % WBUF];
  const uint32_t lastUs = g_wheelLastUs;
  interrupts();

  const uint32_t span = tNew - tOld;   // eine volle Radumdrehung
  if (span == 0) return 0.0f;
  if (micros() - lastUs > span * 2 + 100000UL) return 0.0f;   // steht
  return 1000000.0f / static_cast<float>(span);
}

bool slipDetected() {
  const float drum = revPerSec();
  const float motor = motorRevPerSec();
  if (drum < 0.2f || motor < 0.2f) return false;  // zu langsam für eine Aussage

  const float expected = motor / BELT_RATIO;
  return fabsf(expected - drum) / expected > SLIP_TOLERANCE;
}

bool stalled() {
  noInterrupts();
  const uint32_t last = g_lastMarkUs;
  interrupts();
  return (micros() - last) > STALL_TIMEOUT_MS * 1000UL;
}

const char* channelName(Channel ch) { return CH_NAME[ch]; }

ChannelState channel(Channel ch) {
  noInterrupts();
  const uint32_t count = g_chCount[ch];
  const uint32_t last  = g_chLastUs[ch];
  interrupts();
  ChannelState s;
  s.count   = count;
  s.sinceMs = count ? (micros() - last) / 1000UL : 0xFFFFFFFFUL;
  s.low     = digitalRead(CH_PIN[ch]) == LOW;
  return s;
}

void resetCounters() {
  noInterrupts();
  for (uint8_t i = 0; i < CH_N; i++) { g_chCount[i] = 0; g_chLastUs[i] = 0; }
  interrupts();
  g_resyncs = g_missed = g_glitches = 0;
}

}  // namespace speed
