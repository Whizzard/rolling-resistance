#pragma once
//
// Alle physikalischen Konstanten des Prüfstands — und nur hier.
// Spiegelt docs/03-parameter.md. Änderungen immer in beiden Dateien.
//
// Herkunft jedes Werts steht im Kommentar:
//   [gem]   gemessen        [ber]  berechnet
//   [DB]    Datenblatt      [gesch] geschätzt
//   [TBD]   noch zu ermitteln — Methode steht in docs/03-parameter.md
//

#include <Arduino.h>

// ---------------------------------------------------------------- Geometrie

// [TBD] Maßband um die Trommel, 3 Messungen mitteln. 1 mm Fehler = 0,03 %.
constexpr float DRUM_CIRCUMFERENCE_M = 3.000f;

// [ber] U / 2π
constexpr float DRUM_RADIUS_M = DRUM_CIRCUMFERENCE_M / (2.0f * PI);

// [ber] So gewählt, dass der ungünstigste Fall samt Anfahren (4,20 Nm) bei etwa 60 N
// landet, also 61 % vom Endwert der 10-kg-Zelle. Kurzer Arm = größere Kraft =
// besseres SNR; die Grenze ist allein die Überlastreserve.
// Nach dem Bau mit bekanntem Gewicht nachmessen.
constexpr float TORQUE_ARM_M = 0.070f;

// [Festlegung] DIREKTANTRIEB: die Trommel sitzt auf dem Rotor des Nabenmotors,
// Motor- und Trommeldrehzahl sind identisch. Der Motor dreht mit 605 1/min allein
// schon 109 km/h an der Trommel, eine Übersetzung ist also nicht nötig — und jede
// Übersetzung läge AUSSERHALB der Pendelwiege und würde mitgemessen
// (docs/02-messkonzept.md §2.1). Die Konstante bleibt für den Fall, dass doch
// einmal ein Riemen dazwischenkommt.
constexpr float BELT_RATIO = 1.00f;

// ------------------------------------------------------- Drehzahlaufnehmer

constexpr uint8_t MARKS_PER_REV = 6;     // [Festlegung] gleichmäßig verteilt
constexpr uint32_t MIN_PULSE_US = 800;   // [ber] kürzester echter Abstand ist 18 ms
constexpr uint32_t STALL_TIMEOUT_MS = 2000;  // [ber] > 1,08 s bei 10 km/h
constexpr float SLIP_TOLERANCE = 0.05f;  // [gesch] n_Motor/i gegen n_Trommel

// Motor-Hallflanken pro Motorumdrehung, eine ausgewertete Phase = Polpaarzahl.
// [TBD] Polzahl des DAAO-Motors ausmessen: Motor langsam eine volle Umdrehung von
// Hand drehen und die Flanken zählen.
// Beim Direktantrieb ist das zugleich eine hochauflösende Trommeldrehzahlquelle;
// der Vergleich mit den Trommelmarken bleibt als Plausibilitätsprüfung sinnvoll
// (erkennt einen ausgefallenen Aufnehmer).
constexpr uint8_t MOTOR_PULSES_PER_REV = 15;

// ------------------------------------------------------------------ HX711

constexpr uint8_t HX711_GAIN_PULSES = 1;   // [DB] 1 = Kanal A / Gain 128
constexpr uint16_t HX711_RATE_SPS   = 80;  // [DB] RATE-Pin auf VCC
constexpr uint16_t TARE_SAMPLES     = 200; // [gesch] ≈ 2,5 s bei 80 SPS

// [DB] 10-kg-Zelle
constexpr float LOADCELL_CAPACITY_N = 98.07f;

// [TBD] KALIBRIERUNG AUSSTEHEND. Solange dieser Wert 0 ist, gibt die Firmware
// keine Kräfte und kein Crr aus, sondern nur Rohwerte. Das ist Absicht:
// ein erfundener Kalibrierfaktor wäre schlimmer als eine Lücke.
// Ermittlung: docs/05-pruefablauf.md §2.
constexpr float LOADCELL_N_PER_COUNT = 0.0f;

// -------------------------------------------------------------- Messbetrieb

constexpr float V_MIN_KMH = 10.0f;   // [Festlegung]
constexpr float V_MAX_KMH = 60.0f;   // [Festlegung] 100 erst nach Trommelfreigabe

// [Festlegung] Stützstellen. Baseline und Messung MÜSSEN dieselben benutzen,
// sonst kann die Auswertung nicht punktweise subtrahieren.
// 28,8 km/h ist absichtlich dabei: das ist die Referenzgeschwindigkeit der
// veröffentlichten Crr-Datenbank (200 1/min auf deren 77-cm-Trommel). Zusammen mit
// STANDARD_LOAD_N erlaubt dieser Punkt einen Plausibilitätsvergleich.
constexpr float PLATEAUS_KMH[] = {10, 15, 20, 25, 28.8f, 35, 40, 45, 50, 55, 60};
constexpr uint8_t PLATEAU_COUNT = sizeof(PLATEAUS_KMH) / sizeof(PLATEAUS_KMH[0]);

constexpr float RAMP_KMH_PER_S = 0.5f;        // [gesch] 2 min von 0 auf 60
constexpr uint32_t SETTLE_MS = 15000;         // [gesch] nach Plateauwechsel
constexpr uint8_t AVG_REVS_PER_POINT = 10;    // [gesch] ganze Umdrehungen
// [Literatur] Der Referenzprüfstand fährt 30 min warm, bevor gemessen wird.
// Crr sinkt mit der Reifentemperatur deutlich; ohne definierten Warmlauf sind
// Vergleiche zwischen Reifen nicht belastbar.
constexpr uint32_t WARMUP_MS = 1800000;

// [Festlegung] Standard-Auflast: 42,5 kg, wie in der veröffentlichten Crr-Datenbank.
// Nur ein Vorschlagswert für die Bedienung — die tatsächliche Last wird gemessen
// bzw. über den Befehl 'l<wert>' eingegeben und geht LINEAR in Crr ein.
constexpr float STANDARD_LOAD_N = 416.9f;

// [Literatur] Auslegungsobergrenze des Rollwiderstandsbeiwerts. Der langsamste
// vermessene MTB-Reifen liegt bei 0,0127, der beste Rennreifen bei 0,0025.
constexpr float CRR_DESIGN_MAX = 0.015f;

// ------------------------------------------------------ Sicherheitsgrenzen

constexpr float MAX_FORCE_N = 90.0f;          // 92 % vom Zellenendwert
constexpr float MAX_DRUM_RPM = 333.0f;        // entspricht 60 km/h
constexpr uint32_t FAULT_SETTLE_MS = 500;     // Entprellung der Fehlerkriterien

// ---------------------------------------------------------------- Pinbelegung
// Vorschlag; anpassen, sobald das ESP32-Board feststeht. Siehe docs/04-hardware.md.

constexpr uint8_t PIN_MARK       = 25;  // Drehzahlmarken Trommel (Interrupt)
constexpr uint8_t PIN_INDEX      = 26;  // Indexmarke Trommel (Interrupt)
constexpr uint8_t PIN_MOTOR_HALL = 27;  // Motor-Hall, eine Phase (Interrupt)
constexpr uint8_t PIN_HX711_DOUT = 32;
constexpr uint8_t PIN_HX711_SCK  = 33;
constexpr uint8_t PIN_I2C_SDA    = 21;
constexpr uint8_t PIN_I2C_SCL    = 22;
constexpr uint8_t PIN_DRIVE_TX   = 17;  // UART2 → Motorregler
constexpr uint8_t PIN_DRIVE_RX   = 16;  // UART2 ← Motorregler
constexpr uint8_t PIN_ESTOP_FB   = 34;  // nur Anzeige; Trennung erfolgt in Hardware
constexpr uint8_t PIN_MOTOR_PTC  = 35;  // PTC im Motor, analog (Kennlinie [TBD])

// --------------------------------------------------------------------- LCD

constexpr uint8_t LCD_I2C_ADDR = 0x27;  // [TBD] 0x27 oder 0x3F
constexpr uint8_t LCD_COLS = 20;
constexpr uint8_t LCD_ROWS = 4;

// ------------------------------------------------------------- Umrechnungen

constexpr float KMH_TO_MS = 1.0f / 3.6f;
constexpr float MS_TO_KMH = 3.6f;

// Trommelumdrehungen pro Sekunde ↔ Geschwindigkeit
inline float kmhFromRevPerSec(float rps) { return rps * DRUM_CIRCUMFERENCE_M * MS_TO_KMH; }
inline float revPerSecFromKmh(float kmh) { return kmh * KMH_TO_MS / DRUM_CIRCUMFERENCE_M; }
inline float omegaFromRevPerSec(float rps) { return rps * 2.0f * PI; }

// Ist der Kraftpfad kalibriert?
inline bool isCalibrated() { return LOADCELL_N_PER_COUNT > 0.0f; }
