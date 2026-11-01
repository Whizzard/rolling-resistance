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

// [TBD] Gebaut und gemessen: Ø 900 mm BLANK (2026-11-01). Mit 1,5 mm Tränenblech
// auf dem Umfang wird daraus Ø 903 mm, also U = 2,837 m — das ist der Wert hier.
// Er bleibt TBD, bis das Blech drauf ist und mit dem Maßband um die fertige
// Oberfläche nachgemessen wurde (3 Messungen mitteln). 1 mm Fehler = 0,035 %.
// Alle aus r abgeleiteten Momente verschieben sich mit diesem Wert.
constexpr float DRUM_CIRCUMFERENCE_M = 2.837f;

// [ber] U / 2π
constexpr float DRUM_RADIUS_M = DRUM_CIRCUMFERENCE_M / (2.0f * PI);

// [ber] Kurzer Arm = größere Kraft = besseres SNR; die Grenze ist die Überlastreserve.
// Mit CRR_DESIGN_MAX = 0,007 statt 0,015 ist der ungünstigste Fall halb so groß,
// deshalb wird der Arm von 0,070 auf 0,040 m VERKÜRZT — sonst blieben zwei Drittel
// des Messbereichs ungenutzt und die auf den Endwert bezogenen Zellenfehler
// (Kriechen, Nichtwiederholbarkeit) gingen voll ein. Herleitung docs/08-staender.md §5.2:
//   Auslegungsfall stationär 1,88 Nm → 47,0 N = 48 % vom Zellenendwert
//   samt Rampe              2,51 Nm → 62,7 N = 64 %
//   ΔCrr = 0,0005                            →  2,43 N
// Bohrungen am Hebelarm: 40,0 mm (Zelle), 80,0 mm (Kalibrierung), beide auf ±0,1 mm
// auszumessen. Anschlag bei 0,200 m. Nach dem Bau mit bekanntem Gewicht nachmessen.
constexpr float TORQUE_ARM_M = 0.040f;

// [Festlegung] DIREKTANTRIEB: die Trommel sitzt auf dem Rotor des Nabenmotors,
// Motor- und Trommeldrehzahl sind identisch. Der verbaute Hoverboard-Motor dreht mit
// etwa 480 1/min allein schon 84 km/h an der Trommel, eine Übersetzung ist also
// nicht nötig — und jede
// Übersetzung läge AUSSERHALB der Pendelwiege und würde mitgemessen
// (docs/09-messtheorie.md §1.6). Die Konstante bleibt für den Fall, dass doch
// einmal ein Riemen dazwischenkommt.
constexpr float BELT_RATIO = 1.00f;

// ------------------------------------------------------- Drehzahlaufnehmer

// [Festlegung] 8 geschraubte Magnete auf dem Teilkreis der 8 Abstandsklötze
// (docs/04-hardware.md §9). Mehr Marken bringen keine Genauigkeit — alles, was ins
// Ergebnis geht, wird index-getriggert über ganze Umdrehungen gemessen, dort kürzt
// sich die Markenteilung exakt heraus. Sie bringen nur Latenz auf dem
// trommelseitigen Pfad; die eigentliche Auflösung liefert der Motor-Hall.
constexpr uint8_t MARKS_PER_REV = 8;
constexpr uint32_t MIN_PULSE_US = 800;   // [ber] kürzester echter Abstand ist 21 ms
// [ber] Nicht 10 km/h ist der Grenzfall, sondern der Hochlauf: bei 8 Marken ist der
// Pulsabstand bei 10 km/h nur 132 ms, bei 1 km/h aber 1,32 s. 2000 ms bedeutet
// "langsamer als 0,64 km/h gilt als Stillstand" — die Rampe ist nach 1,3 s darüber.
constexpr uint32_t STALL_TIMEOUT_MS = 2000;
// [gesch] n_Motor/i gegen n_Trommel. Beim Direktantrieb gibt es keinen Schlupf —
// das ist eine PLAUSIBILITÄTSPRÜFUNG der beiden Aufnehmer, kein Schlupfkriterium.
constexpr float SLIP_TOLERANCE = 0.05f;

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

// [Festlegung] Auslegungsobergrenze des Rollwiderstandsbeiwerts. Der Prüfstand ist
// für schnelle Straßenreifen gedacht, nicht für MTB: der langsamste vorgesehene Reifen
// ist der Continental Contact Urban mit 18,4 W, also Crr 0,00552. 0,007 gibt darauf
// 27 % Luft. Die frühere 0,015 war die MTB-Grenze und hat den Hebelarm unnötig lang
// und damit das Signal unnötig klein gemacht.
constexpr float CRR_DESIGN_MAX = 0.007f;

// ------------------------------------------------------ Sicherheitsgrenzen

constexpr float MAX_FORCE_N = 90.0f;          // 92 % vom Zellenendwert
// [ber] ABGELEITET, nicht hart gesetzt: die Grenze bedeutet "60 km/h an der
// Trommeloberfläche" und muss dem Umfang folgen. Der alte feste Wert 333 galt für die
// geplante Ø-955-Trommel; an der gebauten Ø-933-Trommel hätte er schon bei 58,4 km/h
// ausgelöst, also mitten im obersten Messplateau.
// Bei U = 2,837 m sind es 353 1/min.
// Erhöhung von V_MAX_KMH nur nach docs/07-sicherheit.md §5.
constexpr float MAX_DRUM_RPM = V_MAX_KMH / 3.6f / DRUM_CIRCUMFERENCE_M * 60.0f;
constexpr uint32_t FAULT_SETTLE_MS = 500;     // Entprellung der Fehlerkriterien

// ---------------------------------------------------------------- Pinbelegung
// Board: ESP32-S3-DevKitC-1, Modul WROOM-1 N16R8. Begründung docs/04-hardware.md §4.
//
// Gesperrt auf diesem Board — NICHT benutzen:
//   GPIO 26…32  SPI-Flash
//   GPIO 33…37  Octal-PSRAM (das R8 im Namen)
//   GPIO 19, 20 USB-D−/D+
//   GPIO 0, 3, 45, 46  Strapping
//   GPIO 43, 44 UART0-Konsole
//   GPIO 38     RGB-LED des DevKits
//   GPIO 22…25  gibt es auf dem S3 nicht
// Die alte Belegung stammte vom klassischen ESP32 und lag mit 25/26/27/32/33/34/35/22
// fast vollständig auf gesperrten oder nicht existierenden Pins.
// Maßgeblich ist hardware/schaltplan.py — dort ist die gesamte Peripherie verdrahtet,
// auch die Teile, die die Firmware noch nicht anfasst.

constexpr uint8_t PIN_MARK       = 4;   // Drehzahlmarken Trommel (Interrupt), 8/U
constexpr uint8_t PIN_INDEX      = 5;   // Indexmarke Trommel (Interrupt), 1/U
constexpr uint8_t PIN_WHEEL      = 6;   // Drehzahl des Prüflaufrads (Interrupt)
constexpr uint8_t PIN_MOTOR_HALL = 7;   // Motor-Hall, eine Phase (Interrupt)
constexpr uint8_t PIN_HX711_DOUT = 15;  // Zelle 1, Moment
constexpr uint8_t PIN_HX711_SCK  = 16;  // gemeinsamer Takt beider HX711
constexpr uint8_t PIN_HX7112_DOUT = 17; // Zelle 2, Auflast — noch nicht bestückt
constexpr uint8_t PIN_I2C_SDA    = 8;   // S3-Vorgabebelegung für I²C
constexpr uint8_t PIN_I2C_SCL    = 9;
constexpr uint8_t PIN_SD_CS      = 10;  // microSD, Chip Select (SPI noch nicht belegt)
constexpr uint8_t PIN_DRIVE_TX   = 18;  // UART1 → Motorregler
constexpr uint8_t PIN_DRIVE_RX   = 21;  // UART1 ← Motorregler
constexpr uint8_t PIN_ESTOP_FB   = 2;   // nur Anzeige; Trennung erfolgt in Hardware
constexpr uint8_t PIN_BUTTON     = 14;  // Bedientaster
constexpr uint8_t PIN_STATUS_LED = 48;  // Status-LED (auf DevKitC-1 v1.0 die RGB-LED)
// PTC muss auf ADC1 liegen — ADC2 ist bei aktivem WLAN gesperrt. GPIO1 = ADC1_CH0.
// [TBD] Fühlertyp unbekannt — der Hall-Stecker hat eine 6. Ader, vermutlich ein NTC.
// Ausmessen nach docs/06-offene-fragen.md A5.
constexpr uint8_t PIN_MOTOR_PTC  = 1;   // Motortemperatur, analog

// PIN_WHEEL, PIN_HX7112_DOUT, PIN_SD_CS, PIN_BUTTON und PIN_STATUS_LED sind reserviert
// und in hardware/schaltplan.py verdrahtet, werden von der Firmware aber noch nicht
// benutzt. Sie stehen hier, damit die Belegung an genau einer Stelle gepflegt wird.

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
