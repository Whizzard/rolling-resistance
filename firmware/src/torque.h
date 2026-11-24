#pragma once
//
// Momentenmessung über die Pendelmaschine: HX711 + 5-kg-Wägezelle am Hebelarm.
//
// Kernformel (ohne Geschwindigkeit!):
//     F_roll = F · L / r
//     M      = F · L
//     P      = M · ω
//
#include <Arduino.h>

namespace torque {

void begin();

// Nicht blockierend. Holt einen Wert, wenn der HX711 bereit ist.
// Liefert true, wenn ein neuer Rohwert vorliegt.
bool update();

long  lastRaw();

// Gleitender Mittelwert über die letzten DISPLAY_AVG_N Rohwerte, samt Spitze-Spitze.
// NUR für die ANZEIGE — ein mit 80 Hz zappelnder Zahlenwert ist nicht lesbar
// (docs/10-firmware.md W5). Die MESSUNG mittelt davon unabhängig umdrehungssynchron
// über accumulate()/averageRaw(). Gefüllt wird in update(), also genau dann, wenn der
// HX711 wirklich einen neuen Wert geliefert hat — nicht nach der Uhr.
void displayStats(long& mean, long& ripple);
bool  isSaturated();      // Rohwert am Anschlag → Zelle überlastet oder Kabelbruch
bool  isResponding();     // HX711 liefert überhaupt Werte

// Skalierte Größen. Alle liefern NAN, solange LOADCELL_N_PER_COUNT nicht
// kalibriert ist — absichtlich, damit nie eine erfundene Zahl im Log landet.
float newtons();
float momentNm();
float rollForceN();                 // Tangentialkraft an der Trommeloberfläche
float powerW(float omega);

// Statische Tara im Stillstand: mittelt TARE_SAMPLES Rohwerte.
//
// NICHT BLOCKIEREND. tareBegin() kehrt sofort zurück, gesammelt wird in update(),
// und tareBusy() sagt, wann es vorbei ist. Die frühere Fassung hielt bis zu 3 s die
// Schleife auf — damit stand auch der Webserver, die Oberfläche sah einen Abriss,
// und es verstieß gegen docs/10-firmware.md G1 (kein delay() im Hauptpfad).
void     tareBegin();
bool     tareBusy();
uint16_t tareCount();      // 0..TARE_SAMPLES, für den Fortschritt
float    tareSigma();      // Standardabweichung der Rohwerte der letzten Tara, NAN vorher
long     tareOffset();

// --- Umdrehungssynchrone Mittelung ------------------------------------------
// Jeder Rohwert wird mit der Markennummer in einen Winkel-Bin einsortiert.
// Ein Messpunkt ist immer der Mittelwert über ganze Umdrehungen.

void accumulate(uint8_t markIndex);  // nach jedem update() aufrufen
void resetAccumulator();
uint32_t sampleCount();
double   sumRaw();                   // Summe der Rohwerte seit reset (fuer Teilmittel je Umdrehung)
float    averageRaw();               // Mittelwert der Rohwerte seit reset
float    averageN();                 // dito in Newton (NAN ohne Kalibrierung)
long     rippleRaw();                // Spitze-Spitze der Rohwerte seit reset (Welligkeit im Punkt)
float    binAverageRaw(uint8_t bin); // Winkel-Bin, für die Unrundheits-Diagnose
uint8_t  binCount();

// --- Stichprobe im Stillstand ------------------------------------------------
// Fuer Kontrollgewicht und Nullpruefungen (run.cpp): sampleBegin(), warten, dann
// sampleMean()/sampleSigma() ueber ALLE seither gelesenen Werte -- nicht den
// 40er-Anzeigering, der nur 0,5 s abdeckt (docs/11-review.md D5).
// Unabhaengig vom Umdrehungsakkumulator, der nur im Zustand POINT gefuellt wird.
void     sampleBegin();
void     sampleStop();
uint32_t sampleN();
float    sampleMean();               // Rohwert, NAN ohne Werte
float    sampleSigma();              // Rohwert je Einzelwert, NAN unter 2 Werten

}  // namespace torque
