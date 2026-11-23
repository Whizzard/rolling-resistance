#pragma once
//
// GEFUEHRTER MESSLAUF ("Messung" auf der Messseite), seit 2026-11-23.
//
// Vierzehn Schritte, Begruendung in docs/11-messablauf-review.md §3:
//
//   A Vorbereitung (Haende)      1 Lauf anlegen  2 Sichtpruefung  3 Systempruefung
//                                4 Nullen  5 Kontrollgewicht  6 Leerlaufmodell
//                                7 Radmodell  8 Rad aufgelegt
//   B Messfahrt (allein)         9 Warmlauf  10 Plateaus  11 Ankerpunkt  12 Anhalten
//   C Abschluss                  13 Nachlauf Leerlauf (optional)  14 Protokoll
//
// Alles, was Haende braucht, liegt vor dem Warmlauf. Danach faehrt der Stand ohne
// Bediener, und am Ende liegen Protokoll (.md) und Daten (.csv) im Flash
// (LittleFS, /runs/<kennung>.*), abrufbar ueber die Oberflaeche.
//
// Der Lauf sitzt UEBER der Zustandsmaschine (state.h): er startet Sequenzen und
// bekommt jeden Messpunkt mit Streuung zurueck. Alles, was waehrend des Laufs
// abweicht, wird eine nummerierte Warnung (W01 ...) im Protokoll, nichts davon
// verschwindet in der Konsole.
//
// Befehle (Konsole und Oberflaeche, Praefix 'R'):
//   R          starten             Rk  Hauptknopf des Schritts (Weiter, Bestaetigt, Neu)
//   Rs         Nebenknopf (Ueberspringen, Uebernehmen)
//   Rn         Nochmal (Kontrollgewicht)
//   Rx         abbrechen und schliessen
//   Rm<k>=<v>  Metadaten: t Reifen, w Laufrad, b Druck bar, c Raum °C,
//              l Auflast kg, p Position Laufgewicht mm, n Notiz, d Unixzeit s
//
#include <Arduino.h>

namespace run {

// Live-Werte liefert main.cpp (Verlustleistung und Crr bei Konstanz).
struct LiveVals { float mNm, pW, crr; bool korr; };
typedef bool (*LiveFn)(LiveVals& out);

void begin(LiveFn live);
void service();                       // zyklisch, nicht blockierend

bool command(const char* text);       // alles nach dem 'R'; false = abgelehnt
bool active();                        // Schritt 1..14 oder Abbruch sichtbar
uint8_t step();                       // 0 aus, 1..14, 99 abgebrochen

// Zustand des Laufs fuer /api/run, JSON in den Puffer.
void json(char* buf, size_t len);

}  // namespace run
