#pragma once
//
// Abstraktion der Leistungsstufe.
//
// Der Tausch der Leistungsstufe berührt nur diese Datei und drive.cpp — das ist
// der ganze Zweck der Abstraktion. Drei Implementierungen sind vorgesehen:
//
//   StubDrive      Trockentest ohne Hardware (Standard-Build)
//   SimpleFocDrive Inbetriebnahme am vorhandenen SimpleFOCShield v2.
//                  Nur ~2 A / 24 V → unter Last realistisch 15–25 km/h.
//   VescDrive      Vollausbau über UART. Noch nicht implementiert, weil der
//                  Regler nicht feststeht (VESC-Klasse oder umgeflashte
//                  Hoverboard-Platine sprechen unterschiedliche Frames).
//
#include <Arduino.h>

class IDrive {
 public:
  virtual ~IDrive() {}
  virtual void begin() = 0;

  // Sollgeschwindigkeit der TROMMELOBERFLÄCHE in km/h. Die Implementierung
  // rechnet selbst auf Motordrehzahl um (Trommelumfang und BELT_RATIO).
  virtual void setTargetKmh(float kmh) = 0;
  virtual float targetKmh() const = 0;

  // Antrieb stromlos, Trommel läuft frei aus. Das ist der normale Weg zum
  // Anhalten; die Trommel braucht danach lange. Kein Ersatz für den Not-Aus.
  virtual void stop() = 0;

  // Kontrolliertes Bremsen, falls die Leistungsstufe es kann.
  // Default: wie stop().
  virtual void brake() { stop(); }

  virtual bool isFaulted() const = 0;
  virtual const char* name() const = 0;

  // Zyklisch aufrufen (Rampe, Reglerkommunikation).
  virtual void service() = 0;
};

// Liefert die zur Build-Konfiguration passende Instanz.
IDrive& drive();
