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
// [gem 2026-11-23] 2842 mm mit dem Massband um die fertige Oberflaeche.
constexpr float DRUM_CIRCUMFERENCE_M = 2.842f;

// [ber] U / 2π
constexpr float DRUM_RADIUS_M = DRUM_CIRCUMFERENCE_M / (2.0f * PI);

// [ber] Traegheitsmoment der Trommel aus der Geometrie (docs/09-messtheorie.md).
// Bestimmt die Hochlaufzeit und die Energie, die im Notfall abzufuehren ist:
// bei 100 km/h sind es 3,7 kJ.
constexpr float DRUM_INERTIA_KGM2 = 1.94f;

// [gem] Leerlaufverlustmoment der Trommel, am 2026-11-17 aus dem FREIEN AUSLAUF
// bestimmt: 97 -> 29 km/h in 140 s, acht Stuetzstellen, M = I * dw/dt. Anpassung
//     M_verlust = DRUM_LOSS_CONST_NM + DRUM_LOSS_OMEGA2_NM * w^2     (w in rad/s)
//
// Der konstante Anteil ist Lager- und Motorreibung. Er KUERZT SICH in der Pendel-
// messung heraus, weil Trommellager und Motorreibung beide innerhalb der Wiege
// liegen. Uebrig bleibt der w^2-Anteil: der Luftwiderstand der Trommel.
//
// Bei 60 km/h sind das 0,43 Nm. docs/09-messtheorie.md nennt dafuer 0,27 Nm — der
// gemessene Wert ist 1,6-fach groesser. Gegen ein Nutzsignal von 1,32 Nm (Crr 0,007
// bei 417 N Auflast) sind das 33 % statt 20 %, bei 100 km/h sogar 91 %. Abziehbar
// ist er, weil er sich je Stufe leerlaufend mitmessen laesst — aber er muss
// gemessen werden, nicht geschaetzt.
constexpr float DRUM_LOSS_CONST_NM  = 0.472f;
constexpr float DRUM_LOSS_OMEGA2_NM = 3.178e-4f;

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
// [gem 2026-11-23] 84 mm, Trommelachse bis Waage, am Aufbau angesagt.
constexpr float TORQUE_ARM_M = 0.084f;

// [Festlegung] Hebelarm des KALIBRIERGEWICHTS, die zweite Bohrung im Arm. Das
// 2:1-Verhaeltnis zum Zellenarm ist Absicht: ein Gewicht am laengeren Arm erzeugt das
// doppelte Moment und damit die doppelte Kraft an der Zelle, also laesst sich mit
// handlichen Gewichten bis in den oberen Messbereich kalibrieren.
//
// Beide Arme sind zur Laufzeit ueberschreibbar (settings.h) — sie werden AM AUFBAU
// ausgemessen, und ein Kalibrierlauf, der zum Uebersetzen zwingt, wird nicht gemacht.
// [Festlegung 2026-11-23] 330 mm: mit dem geeichten 1-kg-Gewicht 38,5 N an der
// Zelle, 79 % der 5-kg-Zelle -- deckt den Messbereich (48...64 %) nach oben ab und
// laesst Reserve. Bohrung noch nicht gesetzt; nach dem Bohren den Abstand Achse bis
// Lochmitte messen und den gemessenen Wert eintragen.
constexpr float CALIB_ARM_M = 0.330f;

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
// [Festlegung] Magnete an den Speichen des Prüflaufrads, Geber am Ausfallende
// (docs/09-messtheorie.md §4). Die Laufraddrehzahl wird wie die Trommel über ein
// Fenster von einer ganzen Umdrehung gerechnet; mehr Magnete brächten keine
// Genauigkeit, nur eine schnellere Aktualisierung. Verbaut ist seit 2026-11-22
// EIN Magnet — der Wert muss zum Rad passen, sonst stimmen Drehzahl und
// Reifendurchmesser um genau diesen Faktor nicht.
constexpr uint8_t WHEEL_MARKS_PER_REV = 1;
// Laufradumfang aus dem Drehzahlverhältnis (wheel.h): gleitendes Fenster der
// letzten WHEEL_CIRC_WINDOW_REVS Radumdrehungen, bei jedem Impuls neu gerechnet.
constexpr uint8_t  WHEEL_CIRC_WINDOW_REVS = 10;   // [Festlegung 2026-11-23]
constexpr uint32_t WHEEL_CIRC_GAP_MS      = 3000; // [gesch] ohne Impuls: Rad steht, Fenster weg
constexpr uint32_t MIN_PULSE_US = 800;   // [ber] kürzester echter Abstand ist 21 ms
// [ber] Nicht 10 km/h ist der Grenzfall, sondern der Hochlauf: bei 8 Marken ist der
// Pulsabstand bei 10 km/h nur 132 ms, bei 1 km/h aber 1,32 s. 2000 ms bedeutet
// "langsamer als 0,64 km/h gilt als Stillstand" — die Rampe ist nach 1,3 s darüber.
constexpr uint32_t STALL_TIMEOUT_MS = 2000;
// [gesch] n_Motor/i gegen n_Trommel. Beim Direktantrieb gibt es keinen Schlupf —
// das ist eine PLAUSIBILITÄTSPRÜFUNG der beiden Aufnehmer, kein Schlupfkriterium.
constexpr float SLIP_TOLERANCE = 0.05f;

// [gem] Polpaarzahl, am 2026-11-17 über die Hall-Geber ausgezählt: bei 10
// markierten Handumdrehungen stieg der Tachozähler des Reglers um genau 900
// Schritte, ohne einen einzigen Rückwärtsschritt. 6 Schritte je elektrischer
// Umdrehung ergeben 900 / 10 / 6 = 15 Polpaare, also 30 Pole.
//
// Die Skalierung "6 Schritte je elektrischer Umdrehung" ist am selben Datensatz
// gegengerechnet: das Integral von |ERPM| über die Zeit ergab 6,31 Schritte je
// elektrischer Umdrehung, der Überschuss ist Zittern.
//
// Ein erster Versuch im SENSORLOSEN Betrieb gab 16,5 und war nicht reproduzierbar —
// der Lagebeobachter wackelt bei Handgeschwindigkeit. Erst mit Hall-Gebern ist der
// Tachozähler eine Messung. Der Regler stand bis dahin auf si_motor_poles = 14 — das
// ist die WERKSEINSTELLUNG der VESC-Firmware, nachgewiesen am frisch auf 7.01
// geflashten Regler (dort steht wieder 14). Die Firmware ERMITTELT die Polzahl nicht,
// sie wird eingetippt; ohne Eintrag bleibt sie auf 14, und jede daraus abgeleitete
// mechanische Drehzahl ist falsch.
constexpr uint8_t MOTOR_POLE_PAIRS = 15;

// Motor-Hallflanken pro Motorumdrehung, eine ausgewertete Phase = Polpaarzahl.
// Beim Direktantrieb ist das zugleich eine hochauflösende Trommeldrehzahlquelle;
// der Vergleich mit den Trommelmarken bleibt als Plausibilitätsprüfung sinnvoll
// (erkennt einen ausgefallenen Aufnehmer).
constexpr uint8_t MOTOR_PULSES_PER_REV = MOTOR_POLE_PAIRS;

// --------------------------------------------------- Motor, elektrisch
// Am 2026-11-17 am verbauten Motor gemessen, Regler FSESC 4.20 mit Firmware 5.02.
// Vorher stand im Regler eine fremde Konfiguration: Uebersetzung und Oeffnungsdrehzahl
// von Hand gesetzt, R/L/Flussverkettung aus einer Erkennung an einem ANDEREN Motor.
// Polzahl, Raddurchmesser und die Drehzahlreglergewinne darin waren dagegen nur die
// Werkseinstellungen — siehe docs/03-parameter.md §4.1.

// [gem] Flussverkettung, vom Regler erkannt (foc_detect_apply_all mit 50 W
// Verlustgrenze). Scheitelwert der Strangverkettung.
constexpr float MOTOR_FLUX_LINKAGE_WB = 0.017287f;

// [ber] Momentenkonstante einer dreiphasigen PMSM in feldorientierter Regelung:
//     kt = 3/2 * Polpaare * Flussverkettung = 0,389 Nm/A
// Gegenprobe am Hochlauf: mit 3 A ergaben sich 1,17 Nm, und die Trommel
// beschleunigte damit so, wie es DRUM_INERTIA erwarten laesst.
constexpr float MOTOR_KT_NM_PER_A = 1.5f * MOTOR_POLE_PAIRS * MOTOR_FLUX_LINKAGE_WB;

// [gem] Strangwiderstand bei DIREKTER Verdrahtung: measure_res_ind gab 237,2 mOhm,
// die Erkennung trug 238,7 mOhm ein. Mit dem Kurzschlussrelaisblock im Pfad sind es
// 251,9 mOhm (Mittel aus vier Laeufen, Streuung +/- 1,5 mOhm), also 14,7 mOhm
// Kontaktwiderstand. Das ist ein Wirkungsgradposten und erscheint NICHT an der
// Waegezelle — elektrische Verluste sind kein mechanisches Reaktionsmoment.
constexpr float MOTOR_R_OHM = 0.2387f;

// [gem] Leerlaufdrehzahl je Busvolt, am FREIEN Motor ueber zehn Stuetzstellen von
// Tastverhaeltnis 0,50 bis 0,95 bei 40 V, streng linear (11 384 bis 11 450 ERPM je
// Einheit Tastverhaeltnis).
//
// NICHT zu verwechseln mit den "kv = 14", die in docs/06-offene-fragen.md unter A13
// stehen: das ist die Endgeschwindigkeit des Systems MIT Trommel, also gegen deren
// Luftwiderstand — eine Systemgroesse, keine Motorkonstante. Die Induktivitaet ist
// hier bewusst nicht aufgefuehrt: measure_res_ind streute ueber vier Laeufe von
// 305 bis 377 uH, die Erkennung trug 403,8 uH ein. Dafuer braucht es ein anderes
// Verfahren, bevor eine Zahl hier etwas behaupten darf. [TBD]
constexpr float MOTOR_NO_LOAD_RPM_PER_V = 19.1f;

// ------------------------------------------------------------------ HX711

constexpr uint8_t HX711_GAIN_PULSES = 1;   // [DB] 1 = Kanal A / Gain 128
constexpr uint16_t HX711_RATE_SPS   = 80;  // [DB] RATE-Pin auf VCC
constexpr uint16_t TARE_SAMPLES     = 200; // [gesch] ≈ 2,5 s bei 80 SPS
// [Festlegung] Abbruch der Tara, wenn nicht genug Werte kommen. 200 Werte sind bei
// 80 SPS rund 2,5 s; 5000 ms lassen Luft und verhindern, dass ein stummer HX711 den
// Zustand TARE auf Dauer besetzt.
constexpr uint32_t TARE_TIMEOUT_MS  = 5000;

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
// [gem 2026-11-23] Geeichtes 1-kg-Gewicht bei 330 mm, Zellenarm 84 mm, also
// 38,53 N an der Zelle; Faktor ueber die Oberflaeche ermittelt (Befehl k). Ein
// einzelnes Gewicht -- die Linearitaet ueber den Bereich ist damit noch nicht
// geprueft (docs/03-parameter.md §2).
constexpr float LOADCELL_N_PER_COUNT = 0.00002668f;

// -------------------------------------------------------------- Messbetrieb

constexpr float V_MIN_KMH = 10.0f;   // [Festlegung]
constexpr float V_MAX_KMH = 60.0f;   // [Festlegung] 100 erst nach Trommelfreigabe

// [Festlegung] Stützstellen, VORGABE. Die gültige Liste liegt seit 2026-11-23 in
// den Einstellungen (settings::plateauKmh, NVS) und wird über die Oberfläche
// gepflegt; diese Werte gelten, solange dort nichts anderes steht oder nach
// einem Zurücksetzen. Baseline und Messung MÜSSEN dieselbe Liste benutzen,
// sonst kann die Auswertung nicht punktweise subtrahieren — deshalb ist es
// EINE Liste für beide Läufe.
// 28,8 km/h ist absichtlich dabei: das ist die Referenzgeschwindigkeit der
// veröffentlichten Crr-Datenbank (200 1/min auf deren 77-cm-Trommel). Zusammen mit
// STANDARD_LOAD_N erlaubt dieser Punkt einen Plausibilitätsvergleich. 60 km/h ist
// V_MAX_KMH, das obere Ende des freigegebenen Bereichs.
constexpr float PLATEAUS_KMH[] = {15, 28.8f, 40, 50, 60};
constexpr uint8_t PLATEAU_COUNT = sizeof(PLATEAUS_KMH) / sizeof(PLATEAUS_KMH[0]);
constexpr uint8_t PLATEAU_MAX = 16;           // Platz in den Einstellungen

constexpr float RAMP_KMH_PER_S = 0.5f;        // [gesch] 2 min von 0 auf 60
constexpr uint32_t SETTLE_MS = 15000;         // [gesch] nach Plateauwechsel
constexpr uint8_t AVG_REVS_PER_POINT = 10;    // [gesch] ganze Umdrehungen
// [Literatur] Der Referenzprüfstand fährt 30 min warm, bevor gemessen wird.
// Crr sinkt mit der Reifentemperatur deutlich; ohne definierten Warmlauf sind
// Vergleiche zwischen Reifen nicht belastbar.
constexpr uint32_t WARMUP_MS = 1800000;
// [Festlegung] Geschwindigkeit, bei der der Warmlauf zählt, und das Band darum.
// WARMUP_KMH ist PLATEAUS_KMH[1] — dieselbe Stützstelle, aus demselben Grund.
constexpr float WARMUP_KMH = 28.8f;
constexpr float WARMUP_TOL_KMH = 1.0f;        // [gesch] Band, in dem Zeit zählt

// [Festlegung] Fenster, über das dv/dt gerechnet wird. Bei 5 Hz Anzeige sind das
// fünf Stützstellen; revsrc.cpp legt acht Proben darin ab und fittet eine Gerade.
constexpr uint32_t DVDT_WINDOW_MS = 1000;
// [ber] "Konstant" ist kein Augenmaß, sondern eine Grenze: ein Rest an
// Beschleunigung erzeugt über M = J·dω/dt ein Scheinmoment. 1 % vom Signal eines
// guten Rennreifens sind 0,012 Nm, über J = 1,94 kg·m² also 0,0062 rad/s² und damit
// 0,010 km/h/s (docs/10-firmware.md §7.1). Die scharfe Variante ist 0,2 %.
constexpr float DVDT_CONST_KMH_PER_S = 0.010f;
constexpr float DVDT_STRICT_KMH_PER_S = 0.002f;

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
// [gesch] Stellwert, ab dem "treibt und dreht nicht" ein Befund ist und kein
// Rauschen. Am 2026-11-18 lag bei 0,045 Stellwert und 2,5 A Motorstrom die Trommel
// still — das ist Kommutierung, nicht Last (docs/04-hardware.md §11). Ohne eigene
// Trommelmarken ist das der einzige Weg, Stillstand trotz Sollwert zu erkennen.
constexpr float STALL_DUTY_MIN = 0.05f;
// NOT-AUS ERKENNEN. Der manuelle Not-Aus trennt den Motor ueber drei Relais vom
// Regler und schliesst ihn kurz; Regler, ESP32 und Bedienung bleiben am Netz. Es
// gibt keine Rueckmeldung — erkennbar ist der Fall am STROM: mit Motor zieht ein
// stehender Antrieb bei anliegendem Stellwert mehrere Ampere (6,6 A bei 0,11 am
// 2026-11-23), ohne Motor nur das Messrauschen des Reglers.
// [gesch] Pruefen: Not-Aus ausloesen, v15, Imot in der Live-Telemetrie (Befehl j)
// ablesen; er muss unter dieser Grenze bleiben.
constexpr float    ESTOP_IMOT_MAX_A = 0.3f;
// Der Strom allein reicht NICHT: bei 28 km/h konstant zog der Motor nur 0,2 A
// (2026-11-23), das ist Leerlauf, kein Not-Aus. Dazu muss die Drehzahl deutlich
// unter der liegen, die der Stellwert an Spannung vorgibt (Leerlaufkennlinie
// MOTOR_NO_LOAD_RPM_PER_V): ein angeschlossener Motor, der langsamer dreht als
// seine Spannung verlangt, zieht zwingend Strom. Nach dem Not-Aus laeuft die
// Trommel aus, der Stellwert bleibt, die Luecke waechst -- aus 28 km/h nach
// wenigen Sekunden. [gesch] 0,6: unter 60 % der Leerlaufdrehzahl zum Stellwert.
constexpr float    ESTOP_SPEED_RATIO = 0.6f;
// Zweiter, schnellerer Weg: die Trommel wird LANGSAMER, ohne dass Strom fliesst,
// und der Sollwert wird nicht gerade abgesenkt. Ein angeschlossener Motor zieht bei
// sinkender Drehzahl unter konstantem Stellwert sofort Strom. Gemessen nach dem
// Not-Aus aus 28 km/h: etwa 3 km/h/s; die Regelung pendelt mit 0,1...0,2 km/h/s.
constexpr float    ESTOP_DECEL_KMH_S = 0.5f;   // [gesch]
constexpr uint32_t ESTOP_DETECT_MS  = 1000;   // [gesch] so lange muss der Befund stehen
// [TBD] Hilfskontakt des Not-Aus an PIN_ESTOP_FB — noch nicht verdrahtet. Sobald er
// liegt: true setzen. Der Kontakt soll nach Masse ziehen, wenn ausgeloest (interner
// Pull-up; unverdrahtet liest der Pin "nicht ausgeloest").
constexpr bool     ESTOP_FB_WIRED   = false;

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

// PIN_HX7112_DOUT, PIN_SD_CS, PIN_BTN_*, PIN_BRAKE_RELEASE und PIN_STATUS_LED
// sind reserviert und in hardware/schaltplan.py verdrahtet, werden von der
// Firmware aber noch nicht benutzt. Sie stehen hier, damit die Belegung an genau
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
// [Festlegung] mDNS-Name → http://tire.local/. Kurz, weil er von Hand getippt wird.
//
// WAS WORAUF ANTWORTET, damit das nicht durcheinandergeht:
//   tire.local   → mDNS (Bonjour). Apple und Windows fragen fuer ".local"
//                  AUSSCHLIESSLICH per mDNS und NIE den DNS-Server des Netzes.
//   jeder andere → der eigene DNS-Server, der alles auf WEBUI_AP_IP beantwortet
//                  Name        (webui.cpp: g_dns.start(53, "*", AP_IP)).
//   4.3.2.1      → direkt, ohne Namensaufloesung. Der Weg, der immer geht.
//
// ANDROID kann im Browser kein mDNS — dort fuehrt "tire.local" ins Leere. Deshalb
// steht im QR-Code die IP und nicht der Name (docs/04-hardware.md §13.4).
constexpr char     WEBUI_HOSTNAME[] = "tire";

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

// --------------------------------------------------------- Motorregler (VESC)
// Nur fuer die Reglervariante (env:esp32s3-vesc). Die Hauptfirmware benutzt sie
// noch nicht.

constexpr uint32_t VESC_BAUD = 115200;   // [DB] Vorgabe der VESC-Firmware

// [Festlegung] Abfrage der Messwerte. Das Intervall IST zugleich das Mittelungs-
// fenster: der Regler liefert Stroeme als Mittelwert seit der letzten Abfrage und
// setzt den Akkumulator beim Lesen zurueck. 200 ms = 5 Hz, dieselbe Rate wie die
// Anzeige (docs/10-firmware.md W5).
constexpr uint32_t VESC_POLL_MS = 200;

// [Festlegung] Wiederholung des Sollwerts. Der Regler stellt ab, wenn laenger als
// timeout_msec nichts kommt — Vorgabe dort 1000 ms, eingestellt werden 300 ms.
// 150 ms laesst zwei Ausfaelle zu, bevor es greift. ACHTUNG: Messwertabfragen
// setzen den Timeout NICHT zurueck, nur Sollwerte und COMM_ALIVE.
constexpr uint32_t VESC_ALIVE_MS = 150;

// SICHERHEITSGRENZE. Abgeleitet, seit die Polpaarzahl gemessen ist.
//
// Der Regler rechnet in ELEKTRISCHEN Umdrehungen; die mechanische Drehzahl ist
// ERPM / Polpaarzahl. Mit MOTOR_POLE_PAIRS = 15 (gemessen, siehe oben) folgt die
// Kappung direkt aus MAX_DRUM_RPM und zieht damit automatisch mit V_MAX_KMH nach:
//
//     353 1/min * 15 = 5295 ERPM
//
// Der Antrieb ist NICHT der begrenzende Faktor. Am 2026-11-17 am freien Motor
// gemessen: 10 877 ERPM bei Tastverhältnis 0,95 und 40 V, über zehn Stützstellen
// linear, extrapoliert 11 450 ERPM = 763 1/min bei Vollaussteuerung. An der Trommel
// wären das 130 km/h; für 60 km/h genügt Tastverhältnis 0,46. Die Grenze ist eine
// Festlegung aus docs/07-sicherheit.md, keine Eigenschaft der Maschine.
//
// Vorher stand hier 2400, abgeleitet aus den si_motor_poles = 14 des Reglers unter
// der Annahme, das sei der kleinste plausible Wert. Es waren 30 Pole.
//
// Das ist KEIN Ersatz fuer MAX_DRUM_RPM, sondern eine Kappung davor.
constexpr int32_t VESC_ERPM_MAX =
    static_cast<int32_t>(MAX_DRUM_RPM * MOTOR_POLE_PAIRS);

// DREHRICHTUNG. [gem] Am 2026-11-17 am Aufbau festgestellt und zu bestaetigen.
//
// Mit der geprueften Phasenzuordnung (Relaisblock 1:1 wie bei direkter Verdrahtung)
// und der gemessenen Hall-Tabelle dreht die Trommel in MESSRICHTUNG, wenn der Regler
// NEGATIVE ERPM bekommt. Jeder Sollwert wird daher mit diesem Vorzeichen versehen.
//
// Warum nicht `m_invert_direction` im Regler: dann steckt die Festlegung in einer
// Konfiguration, die bei jeder Neuerkennung oder jedem Zurueckspielen des Backups
// verlorengehen kann — und sie waere nicht versioniert. Hier steht sie im Repo.
//
// ACHTUNG, zwei Fallstricke, beide am 2026-11-17 ausgeloest:
//   1. Werden im Relaisblock zwei Phasen getauscht, dreht sich die Richtung um, und
//      die Hall-Tabelle passt nicht mehr — der Motor brummt dann mit Strom, aber ohne
//      Moment, und meldet KEINEN Fehler (docs/04-hardware.md §10.6).
//   2. Ein Richtungswechsel im Lauf ist ein BREMSBEFEHL und speist zurueck. Immer
//      erst Stillstand abwarten (docs/07-sicherheit.md §3.3).
// [gem 2026-11-23] +1 seit dem Neuverdrahten: Fallstrick 1 ist genau so eingetreten,
// die Hall-Tabelle wurde neu erkannt (hardware/vesc-config-fw7.01/README.md), und mit
// dem alten -1 lief die Trommel RUECKWAERTS. Nach jeder Neuerkennung der Hall-Tabelle
// das Vorzeichen pruefen: Befehl e (von Hand drehen) oder e1 / e-1 direkt setzen.
constexpr int32_t VESC_DIRECTION_SIGN = +1;

// Unterhalb dieser Motortemperatur ist der Fuehler OFFEN, nicht kalt. Am
// 2026-11-17 gemessen: offene Klemme meldet -75,5 C, angeschlossen 19,4 C
// (docs/06-offene-fragen.md A5). Am 2026-11-18 hat eine nicht gesteckte
// Sensorleitung -82 C gezeigt -- und weil TMP und die drei Hall-Geber im
// SELBEN Stecker liegen, ist das zugleich der Verdacht auf die Hall-Geber.
constexpr float MOTOR_TEMP_OPEN_C = -20.0f;

// Hoechstalter eines Befehls aus der Oberflaeche.
//
// GRUND, am 2026-11-18 am Stand passiert: Befehle gingen als GET, und ein GET
// gilt als wiederholbar -- iOS schickt ihn nach einem Verbindungsabriss von
// selbst noch einmal. Beim Seitenaufbau riss die Verbindung ab, und ein alter
// "v20" kam Sekunden spaeter erneut an und setzte den Antrieb in Gang. Jeder
// Befehl traegt deshalb jetzt den Zeitstempel mit, den die Oberflaeche zuletzt
// gesehen hat; was aelter ist, wird verworfen.
//
// STOP und Auslaufen sind davon AUSGENOMMEN. Ein verspaetetes STOP ist
// harmlos, ein verworfenes waere es nicht.
constexpr uint32_t CMD_MAX_AGE_MS = 2000;

// Losbrechmessung (speedctl::breakaway). Das Losbrechmoment der Trommel ist
// NICHT gemessen -- DRUM_LOSS_CONST_NM ist die LAUFreibung aus dem Auslauf und
// taugt dafuer nicht. Die Messung hebt das Tastverhaeltnis langsam an, bis sich
// die Trommel bewegt, und bricht dann sofort ab.
constexpr float BREAKAWAY_RATE_PER_S  = 0.01f;   // Anstieg des Tastverhaeltnisses
constexpr float BREAKAWAY_DUTY_MAX    = 0.15f;   // Obergrenze, dann Abbruch
constexpr float BREAKAWAY_DETECT_KMH  = 0.5f;    // ab hier gilt sie als bewegt

// Anlernen der Drehrichtung von Hand (speedctl::learn).
constexpr float    SPEEDCTL_LEARN_MIN_KMH    = 2.0f;
constexpr uint32_t SPEEDCTL_LEARN_TIMEOUT_MS = 20000;

// BREMSSTROEME. [gem] Am 2026-11-17 als Kurve aufgenommen: je Stufe aus 50 km/h
// gebremst (SET_CURRENT_BRAKE), Bremszeit bis 5 km/h und zurueckgewonnene Energie
// gemessen. Verfuegbar waren 911 J.
//
//   3 A  31,4 s  +487 J     10 A  14,9 s  +223 J
//   6 A  19,6 s  +409 J     15 A  13,9 s   +36 J
//                           20 A  13,1 s  -140 J
//
// Bremsleistung und Rueckgewinnung schliessen sich aus, und der Grund steckt in der
// Arbeitsweise von SET_CURRENT_BRAKE: der Regler beginnt KURZGESCHLOSSEN und speist
// erst dann aktiv zurueck, wenn der Kurzschlussstrom den Sollwert UEBERSTEIGT.
// Unterhalb davon muss er den Kurzschlussstrom herunterregeln — das geht nur ueber
// den Bus. Oberhalb muss er Strom zusaetzlich treiben, das KOSTET Busleistung.
//
// Daraus folgt die wichtigste Konsequenz fuer den Betrieb: ein GROSSER Bremsstrom ist
// netzteilfreundlich, ein kleiner nicht. Siehe docs/07-sicherheit.md §3.2a.

// [Festlegung 2026-11-17] Es wird IMMER der schnellstmoegliche Stopp gefahren, nie ein
// langsamer. Rueckgewinnung ist damit kein Auswahlkriterium — sie faellt an oder nicht.
// Deshalb gibt es nur EINEN Bremsstrom.
//
// 20 A statt 15 A, obwohl 15 A energieneutral waeren: der Unterschied in der Bremszeit
// ist klein (13,1 gegen 13,9 s), aber "schnellstmoeglich" ist die Vorgabe. Mehr als
// 20 A bringt nichts mehr, die Bremse ist dort gesaettigt.
//
// NUR MIT AKKU AM BUS. Am Netzteil wirft der Regler ab: am 2026-11-17 mit 20 A aus
// 40 km/h nach 0,61 s Fehlercode 1 (Ueberspannung). Bei hoher Drehzahl liegt der
// Sollwert unter dem Kurzschlussstrom, dort speist der Regler ZURUECK — erst bei
// kleiner Drehzahl entnimmt er. Die Nettobilanz von -140 J verdeckt diesen
// Anfangsstoss, und ein Netzteil kann ihn nicht aufnehmen.
// docs/07-sicherheit.md §3.2b und §3.2c.
constexpr float VESC_BRAKE_CURRENT_STOP = 20.0f;

// ------------------------------------------- Geschwindigkeitsregelung der Trommel
//
// Der Drehzahlregler des VESC ist fuer diesen Motor unbrauchbar eingestellt, und
// s_pid_min_erpm = 900 sperrt die unterste Pruefstufe (881 ERPM bei 10 km/h) —
// docs/06-offene-fragen.md A15. Gefahren wird deshalb ueber das TASTVERHAELTNIS,
// mit Vorsteuerung aus der Leerlaufkennlinie und langsamem Trimm darueber.
// Am 2026-11-17 so bis 99,2 km/h erprobt.

// [Festlegung] Rampe des inneren Sollwerts. 2 km/h je Sekunde sind 30 s von 0 auf 60
// und langsam genug, dass ein Fehlgriff auffaellt, bevor es schnell wird.
constexpr float SPEEDCTL_RAMP_KMH_PER_S = 2.0f;

// [Festlegung] Trimmverstaerkung: Tastverhaeltnis je (km/h Abweichung · Sekunde).
// Die Vorsteuerung traegt die Hauptlast, der Trimm holt nur Reibung und
// Spannungsabfall nach.
constexpr float SPEEDCTL_KP = 0.02f;

// [Festlegung] Hoechste Aenderung des Tastverhaeltnisses je Sekunde.
//
// ACHTUNG, die Asymmetrie ist der Kern: HERUNTERstellen ist ein BREMSBEFEHL. Ein
// Tastverhaeltnis unter dem, was die momentane Drehzahl braucht, laesst den Regler
// zurueckspeisen — am Netzteil endet das in Ueberspannung (docs/07-sicherheit.md
// §3.3). Deshalb darf der Stellwert nur ein Fuenftel so schnell fallen wie steigen.
constexpr float SPEEDCTL_DUTY_UP_PER_S   = 0.30f;
constexpr float SPEEDCTL_DUTY_DOWN_PER_S = 0.06f;

// [Festlegung] Klemme gegen Weglaufen des Stellwerts. Solange die Stromgrenze die
// Beschleunigung bestimmt, bleibt die Regelabweichung gross und der Trimm wuerde bis
// an den Anschlag integrieren — die Trommel schiesst dann ueber den Sollwert hinaus,
// sobald der Strom frei wird. Am 2026-11-17 genau so passiert.
// Obergrenze ist die Leerlaufkennlinie des SOLLWERTS plus Reibungszuschlag.
constexpr float SPEEDCTL_CLAMP_FACTOR = 1.08f;
constexpr float SPEEDCTL_CLAMP_OFFSET = 0.02f;

// [Festlegung] Takt des Regelkreises. Schneller bringt nichts: die Drehzahl kommt
// mit VESC_POLL_MS = 200 ms herein.
constexpr uint32_t SPEEDCTL_PERIOD_MS = 20;

// [Festlegung] Schrittweite der Bedienknoepfe.
constexpr float SPEED_STEP_KMH = 5.0f;

constexpr int32_t VESC_ERPM_STEP_SMALL = 100;   // [Festlegung] Feineinstellung
constexpr int32_t VESC_ERPM_STEP_LARGE = 1000;  // [Festlegung] grobe Annaeherung

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

inline float omegaFromRevPerSec(float rps) { return rps * 2.0f * PI; }

// kmhFromRevPerSec(), revPerSecFromKmh() und isCalibrated() stehen in settings.h.
// Sie haengen am Trommelumfang bzw. am Kalibrierfaktor, und die sind zur Laufzeit
// ueberschreibbar — hier koennten sie nur den Vorgabewert sehen und damit still
// etwas anderes rechnen als der Rest der Firmware.
