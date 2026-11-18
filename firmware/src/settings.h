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

// false, solange nicht kalibriert. Dann liefert torque:: keine Kräfte, nur Rohwerte.
bool calibrated();

// Setzer. Liefern false und ändern nichts, wenn der Wert unplausibel ist —
// ein vertippter Trommelumfang von 28,37 m soll nicht stillschweigend gelten.
bool setDrumCircumferenceM(float m);
bool setTorqueArmM(float m);
bool setCalibArmM(float m);
bool setLoadcellNPerCount(float nPerCount);

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
