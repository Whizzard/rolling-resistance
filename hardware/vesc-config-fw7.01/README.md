# Reglerkonfiguration, Firmware 7.01 (2026-11-17)

Zustand nach dem Firmware-Update von 5.02 auf **7.01** und dem Zurueckschreiben aller
am 2026-11-17 gemessenen Werte. **31 Felder gegengelesen, alle bestaetigt.**

## Das XML ist die maßgebliche Sicherung

`mcconf.xml` / `appconf.xml` adressieren **Feldnamen**, nicht Byte-Versaetze. Damit
ueberleben sie einen Firmwarewechsel — anders als die Binaerdateien, deren Signatur
sich mit jeder Version aendert (von 2211848314 auf 471314414 beim Sprung 5.02 → 7.01,
mcconf von 458 auf 493 Byte).

Lesen und Schreiben mit VESC Tool, ohne Oberflaeche:

    "VESC Tool" --offscreen --getMcConf  <datei>
    "VESC Tool" --offscreen --setMcConf  <datei>
    "VESC Tool" --offscreen --getAppConf <datei>
    "VESC Tool" --offscreen --setAppConf <datei>

Die `.bin`-Dateien liegen nur als Rohdatenbeleg daneben.

## Was aus Messungen stammt

| Feld | Wert | Herkunft |
|---|---|---|
| `si_motor_poles` | 30 | gemessen, 900 Tachoschritte auf 10 Umdrehungen |
| `foc_sensor_mode` | 2 (Hall) | Geber erkannt |
| `foc_hall_table__0…7` | 255, 68, 0, 34, 134, 102, 167, 255 | erkannt, exakte 60-Grad-Teilung |
| `foc_motor_r` | 0,238695 Ω | gemessen |
| `foc_motor_l` | 403,8 µH | gemessen, **Streuung 13 %, TBD** |
| `foc_motor_flux_linkage` | 17,2874 mWb | erkannt |
| `foc_observer_gain` | 3 346 112 | abgeleitet: g = 1e3/λ², Beziehung an zwei Datenpunkten geprueft |
| `si_gear_ratio` | 1 | Direktantrieb |
| `si_wheel_diameter` | 0,903 m | aus `DRUM_CIRCUMFERENCE_M` |

## Was diese Firmware neu kann — der Grund fuer das Update

| Feld | Werk | jetzt | Wirkung |
|---|---|---|---|
| `foc_short_ls_on_zero_duty` | 0 | **1** | echter Brueckenkurzschluss bei Tastverhaeltnis 0: dissipative Bremse **ohne** Bus, also auch am Netzteil. Auf 5.02 gab es das nicht, und das war die Ursache aller Bremsversuche, die mit Ueberspannung endeten. |
| `l_battery_regen_cut_start` | 1000 V | **42,5 V** | drosselt die Rueckspeisung ab, statt den Antrieb fallen zu lassen |
| `l_battery_regen_cut_end` | 1100 V | **44 V** | |

**Beides ist noch nicht erprobt.** Der Bremsversuch mit `foc_short_ls_on_zero_duty`
steht aus und braucht den Motor.

## Der eine Wert, der der Energiequelle folgen muss

`l_in_current_max` steht auf **4,5 A** — das ist die Grenze des Netzteils. **Mit Akku
gehoert er hoeher**, sonst deckelt er die Beschleunigung ohne Not (am 2026-11-17 hat
genau das den Hochlauf auf 100 km/h von 40 auf 60 s gestreckt).

`l_battery_cut_start/end` stehen auf 32/30 V, also 3,20/3,00 V je Zelle bei 10S. Am
Netzteil, das unter Last auf 33,8 V einbricht, ist das knapp — dort gehoeren 20/18 V
hin. Siehe `docs/07-sicherheit.md` §3.2c.

## Noch Werkseinstellung, absichtlich

`s_pid_kp` 0,004 / `ki` 0,004 / `kd` 0,0001 / `s_pid_min_erpm` 900 — der Drehzahlregler
ist **unbrauchbar eingestellt** und muss gegen die montierte Trommel abgeglichen
werden. `s_pid_min_erpm` = 900 blockiert ausserdem die unterste Pruefstufe (10 km/h
entsprechen 881 ERPM). Das sind Werkswerte der VESC-Firmware, nicht Reste der
vorgefundenen Fremdkonfiguration — nachgewiesen am frisch geflashten Regler.
Siehe `docs/06-offene-fragen.md` A15.

## Hall-Tabelle neu erkannt, 2026-11-23

Nach dem Neuverdrahten des Stands bestromte der Motor, drehte aber nicht: ERPM blieb
exakt 0, während der Strom auf der Rampe bis 6,4 A stieg. `hall_analyze` und
`foc_openloop` über den Durchgriff der Firmware zeigten Sensoren und Leistungsteil in
Ordnung — die Zuordnung Hall-Zustand zu Winkel passte nicht mehr, also waren Hall-
oder Phasenleitungen anders gesteckt als bei der Erkennung am 17.11.

`foc_sensors_detect_apply 10` (über `tools/konsole.py ":foc_sensors_detect_apply 10"`)
hat neu erkannt und gespeichert:

| | `foc_hall_table` 0…7 |
|---|---|
| 17.11. | 255, 68, 0, 34, 134, 102, 167, 255 |
| **23.11.** | **255, 2, 66, 33, 130, 166, 98, 255** |

`vesc-mcconf-fw7.01-2026-11-23.bin` ist die Konfiguration danach, roh wie vom Regler
(`COMM_GET_MCCONF`); gegenüber dem 17.11. unterscheiden sich nur die Tabelle und der
Block der Stromoffsets (Bytes 287–302), den der Regler bei jedem Start neu
kalibriert. `mcconf.xml` ist an den acht Tabellenwerten nachgeführt. Die Datei vom
17.11. bleibt als Stand vor der Umverdrahtung liegen.

Außerdem stand im Fehlerspeicher des Reglers ein `FAULT_CODE_DRV` vom Einschalten
(0 A, 40,5 V, 0 Zyklen) — einmalig, nicht aktiv. Beobachten.
