#pragma once
//
// Drehzahlerfassung der Trommel.
//
// Kernidee: die Geschwindigkeit wird IMMER aus dem Zeitfenster über eine ganze
// Umdrehung berechnet, also t[n] - t[n-MARKS_PER_REV]. Damit fallen Teilungsfehler
// der Marken exakt heraus — mehr Marken erfordern deshalb keine Teilungsgenauigkeit,
// liefern aber eine schnellere Aktualisierung.
//
#include <Arduino.h>

namespace speed {

void begin();
// Zyklisch aus der Hauptschleife (revsrc::service): wertet die Zeitstempel der
// Trommelmarken aus -- Umdrehungen, Markennummer, Muster des Zwischenmagneten.
void service();

// Trommel
float revPerSec();        // 0, solange kein vollständiges Fenster vorliegt
float kmh();
float omega();            // rad/s
uint32_t markCount();     // seit begin()
uint32_t revCount();      // Indexmarken seit begin()

// Von der Zustandsmaschine konsumiert: true genau einmal pro Indexmarke.
bool consumeIndexFlag();

// Motor (für die Riemenschlupf-Überwachung)
float motorRevPerSec();

// Prüflaufrad, WHEEL_MARKS_PER_REV Magnete an den Speichen. Dasselbe Verfahren
// wie an der Trommel: Fenster über eine ganze Umdrehung. 0, solange kein
// vollständiges Fenster vorliegt oder das Rad steht.
float wheelRevPerSec();
// Impulszähler des Laufrads und Zeitstempel des letzten Impulses, als Paar.
void wheelPulses(uint32_t& count, uint32_t& lastUs);

// true, wenn die Trommeldrehzahl aus Motordrehzahl/i um mehr als SLIP_TOLERANCE
// abweicht. Liefert false, solange eine der beiden Quellen noch kein Fenster hat.
bool slipDetected();

// true, wenn seit STALL_TIMEOUT_MS keine Marke mehr kam.
bool stalled();

// Nummer der zuletzt gesehenen Marke innerhalb der Umdrehung (0..MARKS_PER_REV-1),
// für die winkelaufgelöste Auswertung.
uint8_t markIndexInRev();

// WOHER DIE UMDREHUNG KOMMT (docs/04-hardware.md §8.6):
//   REV_COUNT       nur die 8 Marken: jede 8. Flanke, Marke 0 ist die erste gesehene
//   REV_PATTERN     Zwischenmagnet in einer 45°-Luecke erkannt (zwei kurze Intervalle):
//                   fester Nullpunkt, Zaehlung rastet jede Umdrehung neu ein
//   REV_INDEX_HALL  eigener Index-Hall an PIN_INDEX
// Das Muster wird automatisch erkannt (MARK_PATTERN_INDEX); ohne Zwischenmagnet
// bleibt es bei REV_COUNT.
enum RevSource : uint8_t { REV_COUNT = 0, REV_PATTERN, REV_INDEX_HALL };
RevSource revSource();
uint8_t   pulsesPerRev();     // 8, oder 9 mit erkanntem Zwischenmagneten
uint32_t  patternResyncs();   // Muster kam an unerwarteter Stelle: Zaehlung neu gesetzt
uint32_t  missedPulses();     // zu lange Intervalle (verschluckte Flanke) oder Muster ausgeblieben
uint32_t  glitchPulses();     // doppelt-kurz mitten in der Umdrehung: Stoerimpuls

// Verdrahtungstest (docs/10-firmware.md D4/D5): lebt jeder Kanal? Je Geber ein
// eigener Impulszähler, der die Messzähler nicht berührt und darum jederzeit
// genullt werden darf, dazu die Zeit seit dem letzten Impuls und der Pegel am Pin.
// Trommel oder Laufrad von Hand drehen, Zähler müssen mitlaufen.
enum Channel : uint8_t { CH_MARK = 0, CH_INDEX, CH_WHEEL, CH_MOTOR, CH_N };
struct ChannelState {
  uint32_t count;     // Impulse seit begin() bzw. resetCounters()
  uint32_t sinceMs;   // seit dem letzten Impuls; 0xFFFFFFFF, wenn noch keiner kam
  bool     low;       // Pegel am Pin: low = Magnet vor dem Geber
};
const char*  channelName(Channel ch);   // "Marken" | "Index" | "Laufrad" | "Motor"
ChannelState channel(Channel ch);
void resetCounters();

}  // namespace speed
