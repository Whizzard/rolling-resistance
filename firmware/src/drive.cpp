#include "drive.h"
#include "config.h"
#include "speed.h"

namespace {

// Gemeinsame Rampenlogik: der Sollwert folgt dem Zielwert mit RAMP_KMH_PER_S.
// Begründung für die Rampenbegrenzung: ein schneller Sollwertsprung erzeugt über
// J·dω/dt ein Scheinmoment, das die Messung verfälscht (docs/09-messtheorie.md §1.7).
class RampedDrive : public IDrive {
 public:
  void setTargetKmh(float kmh) override {
    m_target = constrain(kmh, 0.0f, V_MAX_KMH);
  }
  float targetKmh() const override { return m_target; }

  void stop() override {
    m_target = 0.0f;
    m_ramped = 0.0f;
    applyKmh(0.0f);
  }

  void service() override {
    const uint32_t now = millis();
    if (m_lastMs == 0) m_lastMs = now;
    const float dt = (now - m_lastMs) / 1000.0f;
    m_lastMs = now;

    const float step = RAMP_KMH_PER_S * dt;
    if (m_ramped < m_target) m_ramped = min(m_target, m_ramped + step);
    else if (m_ramped > m_target) m_ramped = max(m_target, m_ramped - step);

    applyKmh(m_ramped);
  }

 protected:
  // Trommelgeschwindigkeit → Motordrehzahl
  static float motorRpmFor(float kmh) {
    return revPerSecFromKmh(kmh) * 60.0f * BELT_RATIO;
  }
  virtual void applyKmh(float kmh) = 0;

  float m_target = 0.0f;
  float m_ramped = 0.0f;
  uint32_t m_lastMs = 0;
};

// --------------------------------------------------------------- Stub

class StubDrive : public RampedDrive {
 public:
  void begin() override {}
  bool isFaulted() const override { return false; }
  const char* name() const override { return "stub"; }

 protected:
  void applyKmh(float kmh) override { m_applied = kmh; }

 private:
  float m_applied = 0.0f;
};

#if defined(DRIVE_SIMPLEFOC)
// TODO: SimpleFOC einbinden, sobald die Verdrahtung ESP32 ↔ Shield steht.
// Grenzen der Stufe beachten: ~2 A dauerhaft bei 24 V. Bei kt ≈ 0,72 Nm/A sind
// das rund 1,4 Nm am Motor bzw. 2,0 Nm an der Trommel — genug für die
// Software-Inbetriebnahme, nicht für den Messbetrieb unter voller Auflast.
class SimpleFocDrive : public RampedDrive {
 public:
  void begin() override {}
  bool isFaulted() const override { return false; }
  const char* name() const override { return "simplefoc"; }

 protected:
  void applyKmh(float kmh) override { (void)motorRpmFor(kmh); }
};
#endif

// --------------------------------------------------------------- Auswahl

#if defined(DRIVE_SIMPLEFOC)
SimpleFocDrive g_drive;
#else
StubDrive g_drive;
#endif

}  // namespace

IDrive& drive() { return g_drive; }
