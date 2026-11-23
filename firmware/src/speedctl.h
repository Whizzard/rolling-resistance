#pragma once
//
// Geschwindigkeitsregelung der Trommel über das Tastverhältnis des Motorreglers.
//
// WARUM NICHT DER DREHZAHLREGLER DES VESC: der ist für diesen Motor unbrauchbar
// eingestellt (Sollwert 2400 ERPM → 780 ERPM erreicht), und `s_pid_min_erpm` = 900
// sperrt die unterste Prüfstufe, weil 10 km/h nur 881 ERPM entsprechen.
// docs/06-offene-fragen.md A15.
//
// STATTDESSEN: Vorsteuerung aus der am 2026-11-17 gemessenen Leerlaufkennlinie
// (MOTOR_NO_LOAD_RPM_PER_V je Busvolt, über zehn Stützstellen streng linear), darüber
// ein langsamer Trimm. So ist der Stand bis 99,2 km/h gefahren worden.
//
// DREI REGELN, JEDE AM 2026-11-17 TEUER GELERNT:
//
//   1. Der Stellwert wird beim Anfahren aus der GEMESSENEN Drehzahl vorbesetzt.
//      Bei null anzufangen, während die Trommel dreht, ist ein Bremsbefehl — der
//      Bus stand dabei in einer halben Sekunde bei 53,3 V.
//   2. Herunterstellen ist langsamer erlaubt als Hochstellen, aus demselben Grund.
//   3. Der Stellwert wird gegen die Leerlaufkennlinie des Sollwerts geklemmt, sonst
//      läuft der Trimm an den Anschlag, solange die Stromgrenze die Beschleunigung
//      bestimmt — und die Trommel schießt über, sobald der Strom frei wird.
//
#include <Arduino.h>

namespace speedctl {

void begin();

// Zyklisch aufrufen, nicht blockierend. Rechnet höchstens alle SPEEDCTL_PERIOD_MS.
void service();

// Zielgeschwindigkeit an der Trommeloberfläche. 0 gibt den Antrieb frei
// (Trommel läuft aus). Wird auf V_MAX_KMH begrenzt — die Oberfläche zeigt Stufen
// darüber als gesperrt an, siehe docs/07-sicherheit.md §5.
//
// Liefert false, wenn der Wunsch abgelehnt wurde: über V_MAX_KMH, oder die Trommel
// dreht noch spürbar in die Gegenrichtung (dann erst Stillstand abwarten).
bool setTargetKmh(float kmh);

float targetKmh();     // was gewünscht ist
float rampKmh();       // was der Regler gerade anstrebt, gerampt
float kmh();           // Istwert, aus der Reglerdrehzahl
float duty();          // Stellwert, der gesendet wird
bool  running();
bool  braking();       // Abstieg laeuft gerade ueber Bremsstrom

// Schnellstmöglicher Stopp: Bremsen mit VESC_BRAKE_CURRENT_STOP.
// ACHTUNG: speist bei hoher Drehzahl zurück. Mit Akku am Bus geprüft; am Netzteil
// wirft der Regler ab und die Trommel läuft aus — der Fehlerfall ist zugleich der
// Rückfallweg (docs/07-sicherheit.md §3.2c).
void stop();

// Antrieb freigeben, Trommel läuft aus. Immer möglich, auch ohne Akku.
void coast();

// Grund der letzten Ablehnung, für die Oberfläche. "" wenn alles gut ging.
const char* lastRefusal();

// NOT-AUS. Zwei Wege, beide nur Erkennung — die Trennung macht die Hardware:
//   motorOpen()  Befund: Stellwert ueber STALL_DUTY_MIN, Motorstrom unter
//                ESTOP_IMOT_MAX_A UND Drehzahl unter ESTOP_SPEED_RATIO mal der
//                Leerlaufdrehzahl zum Stellwert, laenger als ESTOP_DETECT_MS. Ein
//                angeschlossener Motor kann nicht stromlos hinter seiner Spannung
//                zurueckbleiben; nach dem Not-Aus laeuft die Trommel aus und die
//                Luecke waechst (Begruendung in config.h).
//                Dann wird der Antrieb freigegeben, der Befund bleibt stehen, bis ein
//                neuer Sollwert kommt. Ohne Motor am Regler ist ein anliegender
//                Stellwert sinnlos — und wird der Not-Aus zurueckgedreht, soll der
//                Motor nicht mit vollem Stellwert anspringen.
//   estopInput() der Hilfskontakt an PIN_ESTOP_FB, nur wenn ESTOP_FB_WIRED.
bool motorOpen();              // rastend bis Stillstand oder neuer Sollwert
bool estopInput();

// Drehrichtung von Hand anlernen.
//
// Welches Vorzeichen der Reglerdrehzahl die MESSRICHTUNG ist, haengt an der
// Phasenreihenfolge und aendert sich, sobald jemand am Motorkabel oder an den
// Bremsrelais umklemmt. Statt es in config.h festzuschreiben wird es gemessen:
// Antrieb freigeben, Trommel von Hand in Messrichtung drehen, Vorzeichen in den
// NVS.
//
// Das ersetzt NICHT die Hall-Erkennung des Reglers. Die Kommutierung -- welcher
// Hall-Zustand zu welchem Winkel gehoert -- kann nur der Regler messen, weil
// dafuer Strom eingepraegt werden muss. Hier geht es allein um die
// Vorzeichenkonvention darueber.
void learn();
bool learning();

// Losbrechmessung: Tastverhaeltnis langsam anheben, bis sich die Trommel regt,
// dann sofort freigeben. Liefert die Zahl, die in docs/03-parameter.md fehlt --
// und beantwortet, ob die Klemme beim Anfahren ueberhaupt Reserve braucht.
void breakaway();
bool breakingAway();
float breakawayDuty();      // Tastverhaeltnis beim Losbrechen, 0 = noch keins
float breakawayAmps();      // Motorstrom in diesem Moment

}  // namespace speedctl
