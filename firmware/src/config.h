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

// [TBD] Kurzer Arm = größere Kraft = besseres SNR; die Grenze ist die Überlastreserve.
// Die Zelle ist eine 5-kg-Zelle (49,03 N). Bei L = 0,040 m läge der Auslegungsfall bei
// 96 % und der Fall samt Rampe bei 128 % — die Zelle würde im NORMALBETRIEB überfahren,
// nicht erst im Fehlerfall. Mit L = 0,080 m stimmt die Auslegung wieder:
//   Auslegungsfall stationär 1,88 Nm → 23,5 N = 48 % vom Zellenendwert
//   samt Rampe              2,51 Nm → 31,4 N = 64 %
//   ΔCrr = 0,0005                            →  1,22 N
// Das sind exakt dieselben Ausnutzungen wie in der früheren 10-kg-Rechnung bei 0,040 m:
// halbe Zelle, doppelter Arm. Die Begründung in docs/08-staender.md §5.2 trägt also
// unverändert, nur liegt die Überlastgrenze woanders.
// Verbaut sind laut Ansage vom 2026-11-16 etwa 80 mm. Der Arm geht LINEAR in jedes
// Moment ein — vor der ersten Messung die Bohrung auf ±0,1 mm ausmessen und den Wert
// hier eintragen; bis dahin bleibt er TBD. Die Kalibrierbohrung wandert mit nach außen
// auf 160 mm, damit das 2:1-Verhältnis erhalten bleibt. Anschlag bei 0,200 m.
constexpr float TORQUE_ARM_M = 0.080f;

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

// [Festlegung] Gleitender Mittelwert für die ANZEIGE, nicht für die Messung. Der
// HX711 liefert HX711_RATE_SPS Werte je Sekunde; ein mit 80 Hz zappelnder Zahlenwert
// ist weder lesbar noch aussagekräftig (docs/10-firmware.md W5, D6). 40 Werte sind
// 0,5 s bei 80 SPS. Die MESSUNG mittelt davon unabhängig umdrehungssynchron über
// AVG_REVS_PER_POINT — die beiden Mittelungen haben nichts miteinander zu tun.
constexpr uint8_t DISPLAY_AVG_N = 40;

// [Ansage 2026-11-16] 5-kg-Zelle, 5 · 9,80665 = 49,03 N. Die Unterlagen nannten bis
// dahin 10 kg; das war falsch und hätte zusammen mit dem 40-mm-Arm die Zelle beim
// Anfahren zerstört. Typenschild noch nachlesen und die Herkunft auf [DB] setzen.
constexpr float LOADCELL_CAPACITY_N = 49.03f;

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

constexpr float MAX_FORCE_N = 45.0f;          // 92 % vom Zellenendwert (5 kg)
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
constexpr uint8_t PIN_STATUS_LED = 48;  // adressierbare RGB-LED des DevKits, siehe unten

// Steuerkreuz: fünf Kontakte gegen Masse, interner Pull-up. Entprellung in Software,
// kein RC im Signalpfad (docs/10-firmware.md B1…B5). Bauform ist ein
// Fünfwege-Tastschalter — ein Bauteil, fünf Kontakte, gemeinsame Masse.
constexpr uint8_t PIN_BTN_UP     = 11;  // Sollwert hoch
constexpr uint8_t PIN_BTN_DOWN   = 12;  // Sollwert runter
constexpr uint8_t PIN_BTN_IN     = 13;  // rein — Schrittweite 1/5 km/h
constexpr uint8_t PIN_BTN_OUT    = 14;  // raus
constexpr uint8_t PIN_BTN_OK     = 39;  // Mitte, bestätigen

// MOSFET in der Spulenmasse der Bremsrelais (docs/04-hardware.md §9).
// high = Relais angezogen = Bremse gelöst. Pulldown am Gate, beim Reset also gebremst.
constexpr uint8_t PIN_BRAKE_RELEASE = 40;

// KEIN PIN_MOTOR_PTC: der Temperaturfühler des Motors hängt am Eingang TMP des
// REGLERS, nicht am ESP32. Der Regler begrenzt selbst, der ESP32 liest den Wert über
// UART (docs/04-hardware.md §11.4). Ein resistiver Fühler verträgt nur EINEN Pull-up,
// also gibt es nur eine Auswertestelle. GPIO 1 bleibt damit frei.
// [TBD] Welcher Fühlertyp es ist, bleibt offen (A5) — das entscheidet nur, welche
// Kennlinie im VESC-Tool eingestellt wird, nicht mehr, wo die Ader hingeht.

// PIN_WHEEL, PIN_HX7112_DOUT, PIN_SD_CS, PIN_BTN_*, PIN_BRAKE_RELEASE und
// PIN_STATUS_LED sind reserviert und in hardware/schaltplan.py verdrahtet, werden von
// der Firmware aber noch nicht benutzt. Sie stehen hier, damit die Belegung an genau
// einer Stelle gepflegt wird.
//
// Frei bleiben damit GPIO 1, 41, 42 und 47. Die microSD braucht davon noch drei
// (SCK/MOSI/MISO), CS liegt auf PIN_SD_CS — danach ist GPIO 1 der einzige freie Pin.

// ------------------------------------------------------------- Bedienfeld WLAN
// Der Prüfstand spannt einen eigenen Accesspoint auf und liefert die Oberfläche
// selbst aus. Kein Internet, kein Router. Begründung docs/04-hardware.md §13.
constexpr char     WEBUI_SSID[]    = "Pruefstand";  // [Festlegung]
// [Festlegung] WPA2 statt offen. Nicht aus Geheimhaltung, sondern weil sonst jeder in
// Reichweite den Motor fahren kann. Eingetippt wird es nie — es steckt im QR-Code
// (hardware/qr-wlan.png). Mindestens 8 Zeichen, sonst bleibt der AP offen.
constexpr char     WEBUI_PASSWORD[] = "***REMOVED***";
constexpr uint8_t  WEBUI_CHANNEL   = 6;             // [Festlegung]
constexpr uint8_t  WEBUI_MAX_CLIENTS = 4;           // [Festlegung]
constexpr uint16_t WEBUI_PORT      = 80;            // [Festlegung] Captive Portal braucht 80
constexpr char     WEBUI_HOSTNAME[] = "pruefstand"; // [Festlegung] mDNS → pruefstand.local

// Adresse des Accesspoints. NICHT 192.168.4.1 — und das ist kein Geschmacksfrage:
// Androids NetworkMonitor verwirft die Verbindungsprüfung, sobald der DNS einen
// Prüf-Host auf eine PRIVATE Adresse auflöst, und sendet die HTTP-Anfrage erst gar
// nicht ab (hasPrivateIpAddress(): 10/8, 172.16/12, 192.168/16, 169.254/16 →
// CaptivePortalProbeResult.PRIVATE_IP → failed). Ergebnis: "Verbunden, kein
// Internet", kein Portal, keine Anmeldeaufforderung. Deshalb setzt auch das
// arduino-esp32-Beispiel 8.8.4.4 und WLED 4.3.2.1.
// [Festlegung] Belegt fremden Adressraum — unschön, aber ohne Internet am Stand
// folgenlos, und die einzige Art, auf Android überhaupt erkannt zu werden.
constexpr uint8_t  WEBUI_AP_IP[4] = {4, 3, 2, 1};

// KEIN Captive Portal. Begründung in docs/04-hardware.md §13: das Portalfenster ist
// kein Browser, es schließt beim Wegwischen, iOS trennt danach das Netz, und Android
// zeigt es bei privaten Adressen gar nicht erst. Stattdessen beantwortet der Stand die
// Verbindungsprüfung so, wie das Betriebssystem es erwartet — dann gilt das Netz als in
// Ordnung, die Verbindung bleibt stehen, und nichts springt auf. Aufgerufen wird die
// Oberfläche über den aufgeklebten QR-Code (hardware/qr-ui.png) oder jede beliebige
// Adresse im Browser; der DNS beantwortet alles mit der eigenen Adresse.

// SICHERHEITSGRENZE. Totmannschaltung: solange ein Lauf fährt, muss der
// Bedienrechner sich melden. Bleibt er länger aus, bricht der Lauf ab — ein
// Abschaltkriterium wie jedes andere, docs/07-sicherheit.md §4.
// [Festlegung] 2000 ms, dieselbe Größenordnung wie STALL_TIMEOUT_MS. Die
// Oberfläche fragt 5×/s ab, es dürfen also zehn Abfragen ausfallen, bevor es greift.
// Gilt NICHT, solange nie ein Client da war: dann wird über die Konsole gefahren.
constexpr uint32_t LINK_TIMEOUT_MS = 2000;

// --------------------------------------------------------------- Status-LED
// Die RGB-LED des DevKits ist eine adressierbare WS2812, kein einfacher Ausgang —
// angesteuert über neopixelWrite(), das intern RMT benutzt.
//
// Sie zeigt den Zustand der Bedienverbindung: grün stetig = ein Bedienrechner
// meldet sich, rot blinkend = es wird gewartet. Damit ist die Totmannschaltung aus
// drei Metern ablesbar.
//
// [gem] GPIO 48 am verbauten Board bestätigt (2026-11-15, LED leuchtet). Auf
// DevKitC-1 v1.1 säße die LED an GPIO 38 — dieses Board ist also v1.0, oder zumindest
// eine Bestückung mit der LED an 48. Damit ist die frühere TBD erledigt.
// Hinweis für den Fall eines Boardwechsels: beide Pins nacheinander zu bespielen geht
// NICHT, neopixelWrite() initialisiert RMT einmalig für den Pin des ersten Aufrufs.
constexpr uint8_t  STATUS_LED_DIV       = 6;     // [Festlegung] Helligkeit teilen; die
                                                 // LED blendet sonst auf dem Tisch
// Die LED zeigt den Zustand der BEDIENVERBINDUNG:
// gruen = ein Bedienrechner meldet sich, rot blinkend = es wird gewartet.
// Damit sieht man die Totmannschaltung aus drei Metern Entfernung.
constexpr uint32_t STATUS_BLINK_MS      = 700;   // [Festlegung] halbe Periode rot

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
