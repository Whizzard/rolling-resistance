#pragma once
//
// Werte, die am Stand ermittelt werden und deshalb zur Laufzeit liegen müssen.
//
// WARUM NICHT IN config.h: Trommelumfang, Hebelarm und Kalibrierfaktor werden
// AM AUFBAU gemessen, nicht am Schreibtisch. Ein Kalibrierlauf, der zum Übersetzen
// zwingt, wird nicht gemacht — und ein nicht gemachter Kalibrierlauf ist die Quelle
// erfundener Zahlen, gegen die CLAUDE.md geschrieben ist.
//
// DIE PROJEKTREGEL BLEIBT TROTZDEM GEWAHRT: config.h liefert die Vorgaben, und die
// Oberfläche zeigt nach jeder Kalibrierung die fertige Zeile zum Eintragen in
// config.h und docs/03-parameter.md an. Der NVS ist der Arbeitsspeicher, das Repo
// bleibt die Quelle der Wahrheit. Wer das Board tauscht, verliert den NVS — und
// merkt es, weil die Firmware dann wieder "nicht kalibriert" meldet.
//
#include <Arduino.h>
#include "config.h"

namespace settings {

// Lädt aus dem NVS. Fehlt ein Wert, gilt die Vorgabe aus config.h.
void begin();

float drumCircumferenceM();
float drumRadiusM();            // abgeleitet, U / 2π
float torqueArmM();             // Hebelarm der Wägezelle
float calibArmM();              // Hebelarm des Kalibriergewichts
float loadcellNPerCount();

// Welches Vorzeichen der Reglerdrehzahl die MESSRICHTUNG ist. +1 oder -1.
//
// Das haengt an der Phasenreihenfolge und aendert sich, sobald jemand am
// Motorkabel oder an den Bremsrelais umklemmt. Es gehoert deshalb hierher und
// nicht in config.h: anlernbar durch Drehen von Hand, siehe speedctl::learn().
int8_t directionSign();

// false, solange nicht kalibriert. Dann liefert torque:: keine Kräfte, nur Rohwerte.
bool calibrated();

// Setzer. Liefern false und ändern nichts, wenn der Wert unplausibel ist —
// ein vertippter Trommelumfang von 28,37 m soll nicht stillschweigend gelten.
bool setDrumCircumferenceM(float m);
bool setTorqueArmM(float m);
bool setCalibArmM(float m);
bool setLoadcellNPerCount(float nPerCount);
bool setDirectionSign(int8_t sign);   // nur +1 oder -1

// Messgeschwindigkeiten: die Plateaus, die Leerlauf- und Messlauf der Reihe
// nach abfahren. Vorgabe PLATEAUS_KMH aus config.h, gepflegt über die
// Oberfläche, im NVS. Die Liste ist IMMER aufsteigend sortiert und ohne
// Doppelte; addPlateau fügt an der richtigen Stelle ein.
uint8_t plateauCount();
float   plateauKmh(uint8_t i);           // 0 ausserhalb der Liste
bool    addPlateau(float kmh);           // false: unplausibel, voll oder schon drin
bool    removePlateau(float kmh);        // false: nicht in der Liste

// Auflast auf dem Laufrad, Vorgabe LOAD_DEFAULT_N, am Stand änderbar (Befehl l).
float loadN();
bool  setLoadN(float n);

// Verlustmodelle, beide im NVS (config.h, docs/09-messtheorie.md Gl. 11/12):
//   Leerlauf  M_leer(w)     = A0 + B0 w^2        [Nm], w Trommel in rad/s
//   Laufrad   M_Rad(w_Rad)  = A1 + B1 w_Rad^2    [Nm am Rad]
// NAN = nicht ermittelt. Dann wird nicht korrigiert, und Crr heisst "roh".
float baselineA0();  float baselineB0();  bool setBaseline(float a0, float b0);
float wheelA1();     float wheelB1();     bool setWheelModel(float a1, float b1);
bool  haveBaseline();  bool haveWheelModel();
// Laufradmasse mit Reifen [kg]; daraus J_Rad = WHEEL_J_FACTOR m r_Rad^2. NAN = unbekannt.
float wheelMassKg(); bool setWheelMassKg(float kg);
// Letzter gemessener Laufradumfang [m] (wheel.h), ueberlebt den Stillstand und den
// Neustart -- der Assistent braucht ihn, bevor sich etwas dreht. NAN = nie gemessen.
float wheelCircM(); bool setWheelCircM(float m);

// Statische Tara der Wägezelle (Rohwert bei leerem Arm). Im NVS, damit sie einen
// Neustart überlebt -- vorher war sie nach jedem Flashen weg, und 241 000 counts
// wurden als 6,4 N angezeigt (2026-11-23). 0 = nie genullt.
long tareOffset();
bool setTareOffset(long counts);

// Zurück auf die Vorgaben aus config.h, NVS wird geleert.
void reset();

// true, wenn mindestens ein Wert vom Vorgabewert abweicht.
bool anyOverridden();

}  // namespace settings

// --------------------------------------------------------- Umrechnungen
// Stehen hier und nicht in config.h, weil sie am Trommelumfang bzw. am
// Kalibrierfaktor haengen — beides zur Laufzeit ueberschreibbar.

inline float kmhFromRevPerSec(float rps) {
  return rps * settings::drumCircumferenceM() * MS_TO_KMH;
}
inline float revPerSecFromKmh(float kmh) {
  return kmh * KMH_TO_MS / settings::drumCircumferenceM();
}
inline bool isCalibrated() { return settings::calibrated(); }
