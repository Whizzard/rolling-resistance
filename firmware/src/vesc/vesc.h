#pragma once
//
// Motorregler über UART — das Nötigste.
//
// Sollwert ist die DREHZAHL in ERPM, denn so rechnet der Regler, und er hat dafür
// einen eigenen PID-Regelkreis: ein Plateau hält er selbst, der ESP32 muss nicht
// nachregeln. Die mechanische Drehzahl wäre ERPM / Polpaarzahl — die ist aber nicht
// gemessen (MOTOR_PULSES_PER_REV ist TBD), deshalb wird hier nichts umgerechnet.
// Lieber ERPM anzeigen als eine erfundene Drehzahl.
//
// Gelesen werden neun Werte (VESC_POLL_MS): FET- und Motortemperatur, Motor- und
// Eingangsstrom, Aussteuerung, Drehzahl, Busspannung, Fehlercode und Statusbyte.
//
// Die Motortemperatur war anfangs ausgeklammert, weil der Regler ohne Fühler am
// TMP-Eingang dafür eine Phantasiezahl OHNE Fehlermeldung liefert. Seit A5 am
// 2026-11-17 beantwortet ist (10-k-NTC im Stator, dreifach belegt), ist der Wert
// echt und wird abgefragt.
//
// TOTMANN: Der Regler stellt ab, wenn länger als timeout_msec kein Sollwert kommt.
// Deshalb wird der Sollwert alle VESC_ALIVE_MS wiederholt, solange gefahren wird.
// ACHTUNG: Messwertabfragen setzen den Timeout NICHT zurück — nur Sollwerte.
// Wird nicht gefahren, wird auch nichts gesendet; läuft die Firmware dann fest,
// greift der Timeout des Reglers. Das ist die zweite, unabhängige Ebene.
//
#include <Arduino.h>

namespace vesc {

// Befehlskennungen, Zahlenwerte aus datatypes.h der VESC-Firmware.
enum : uint8_t {
  COMM_FW_VERSION          = 0,
  COMM_SET_DUTY            = 5,
  COMM_SET_CURRENT         = 6,
  COMM_SET_CURRENT_BRAKE   = 7,
  COMM_SET_RPM             = 8,
  COMM_TERMINAL_CMD        = 20,   // Textbefehl an das Terminal des Reglers
  COMM_PRINT               = 21,   // Textantwort des Terminals
  COMM_ALIVE               = 30,
  COMM_GET_VALUES_SELECTIVE = 50,
};

struct Values {
  bool     seen   = false;   // schon mindestens ein Paket empfangen
  uint32_t ageMs  = 0;       // seit dem letzten Paket
  float    tempFet = 0.0f;   // °C
  float    tempMotor = 0.0f; // °C, 10-k-NTC im Stator (A5)
  float    currentMotor = 0.0f;  // A, Mittel seit der letzten Abfrage
  float    currentIn    = 0.0f;  // A, dito. Auf Hardware 4.x GERECHNET, nicht gemessen
  float    duty   = 0.0f;    // −1 … +1
  int32_t  erpm   = 0;
  float    vIn    = 0.0f;    // V
  uint8_t  fault  = 0;
  uint8_t  status = 0;       // Bit0 Timeout des Reglers, Bit1 Kill-Switch
};

void begin();
void service();              // zyklisch aufrufen; nicht blockierend

const Values& values();
bool connected();            // ein Paket innerhalb der letzten Sekunde

// Sollwert. Wird auf ±VESC_ERPM_MAX begrenzt und alle VESC_ALIVE_MS wiederholt,
// bis release() kommt.
void setErpm(int32_t erpm);
int32_t targetErpm();
bool driving();
bool braking();              // SET_CURRENT_BRAKE liegt an (Befehl s)

// Tastverhaeltnis als Stellgroesse, −1 … +1. Wird wie der Drehzahlsollwert alle
// VESC_ALIVE_MS wiederholt.
//
// Dieser Weg wird gefahren, weil der Drehzahlregler des VESC fuer diesen Motor
// unbrauchbar eingestellt ist und s_pid_min_erpm = 900 die unterste Pruefstufe
// sperrt (docs/06-offene-fragen.md A15). Ueber das Tastverhaeltnis ist der Stand
// am 2026-11-17 bis 99,2 km/h gefahren worden, streng linear zur Leerlaufkennlinie.
void setDuty(float duty);
float targetDuty();

// Bremsen mit vorgegebenem Strom. Der EINSTIEG speist fuer Millisekunden zurueck;
// am Netzteil ab ~56 km/h Ueberspannung (docs/07-sicherheit.md §3.2d). Deshalb nur
// unter BRAKE_MAX_KMH rufen. Ein Tastverhaeltnis-0-Befehl waere in FOC kein
// Kurzschluss, sondern eine PI-Rampe mit Rueckspeisung -- darum gibt es ihn hier nicht.
void setBrakeCurrent(float amps);

// Antrieb stromlos, Trommel läuft frei aus. Das ist der normale Weg zum Anhalten
// und entspricht IDrive::stop() — KEIN Ersatz für den Not-Aus.
void release();

// Firmwarestand des Reglers. Entscheidet unter anderem, ob
// foc_short_ls_on_zero_duty existiert (erst ab 6.05).
bool fwKnown();
uint8_t fwMajor();
uint8_t fwMinor();
const char* hwName();        // "410" beim FSESC 4.20

// Fehlercode im Klartext. Unbekannte Codes werden als solche gemeldet und nicht
// auf einen falschen Namen abgebildet.
const char* faultText(uint8_t code);

// Der Regler meldet einen Fehlercode nur m_fault_stop_time_ms lang (500 ms), dann
// ist er wieder 0. Zu kurz fuer ein Auge -- deshalb bleibt der letzte Code hier
// stehen: lastFault() und wie lange das her ist. Jeder neue Fehler geht als
// "# VESC-FEHLER" auf die Konsole. Die Historie liefert das Terminal (":faults").
uint8_t  lastFault();
uint32_t lastFaultAgeMs();   // 0xFFFFFFFF, wenn nie einer kam
uint32_t faultCount();

// DURCHGRIFF fuer die Fehlersuche ueber die USB-Konsole (docs/04-hardware.md 14.1):
//   terminal(text)   schickt einen Textbefehl an das Terminal des Reglers, so wie
//                    das VESC-Tool es tut ("faults", "hall_analyze 5", ...). Die
//                    Antworten kommen als COMM_PRINT und werden als "# VESC> ..."
//                    auf die Konsole geschrieben.
//   raw(payload)     schickt eine beliebige Nutzlast (Kennung + Daten). Antworten,
//                    die der Treiber nicht selbst verarbeitet, erscheinen als
//                    "# V <hex>" -- so laesst sich z. B. COMM_GET_MCCONF (14) holen.
// Beides ist Werkzeug, kein Betriebsweg: die Firmware wertet nichts davon aus.
void terminal(const char* text);
void raw(const uint8_t* payload, size_t len);

}  // namespace vesc
