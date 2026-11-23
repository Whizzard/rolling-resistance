# Parameter

Spiegelt `firmware/src/config.h` 1:1. Jede Änderung hier **und** dort.

Herkunft: `gem` = gemessen · `ber` = berechnet · `DB` = Datenblatt · `gesch` = geschätzt ·
`TBD` = noch zu ermitteln.

## 1 Geometrie

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| U | `DRUM_CIRCUMFERENCE_M` | **2,842** | m | gem | **2026-11-23 mit dem Maßband um die fertige Oberfläche: 2842 mm**, Ø 904,6 mm. Hier stand zwischendurch 2,931 (Ø 933), während `config.h` 2,837 führte — beides ersetzt ([09-messtheorie](09-messtheorie.md) §1.5). Der frühere Wert 3,000 galt der geplanten Ø-955-Trommel und war 2,4 % zu groß. Nach jeder Änderung an der Oberfläche neu messen: Maßband um die Trommel, 3 Messungen mitteln. 1 mm Fehler = 0,03 %. |
| r | `DRUM_RADIUS_M` | **0,4665** | m | ber | U / 2π |
| L | `TORQUE_ARM_M` | **0,084** | m | **gem** | **Am Aufbau 2026-11-23: 84 mm** Trommelachse bis Waage (Ansage, Messschieber-Genauigkeit noch nicht bestätigt). Vorher 0,080 geplant. Gilt für die **5-kg-Zelle**. Auslegungsfall stationär 1,88 Nm → 23,5 N = 48 % FS; samt Rampe 2,51 Nm → 31,4 N = 64 %; ΔCrr 0,0005 → 1,22 N — dieselben Ausnutzungen wie zuvor mit 10 kg bei 0,040 m: halbe Zelle, doppelter Arm. Herleitung [08-staender](08-staender.md) §5.2. Bohrungen 80,0 mm (Zelle) und 160,0 mm (Kalibrierung), je ±0,1 mm. Verbaut sind laut Ansage etwa 80 mm — **ausmessen**, der Arm geht linear in jedes Moment ein (§2). |
| J | `DRUM_INERTIA_KGM2` | **1,94** | kg·m² | ber | Aus der Geometrie der **Ø-903-Trommel**, passend zu `DRUM_CIRCUMFERENCE_M` = 2,837 in `config.h`. **Achtung, offener Widerspruch:** die Zeile `J` weiter unten in diesem Abschnitt nennt **2,07** für die Ø-933-Trommel, und `DRUM_CIRCUMFERENCE_M` steht in diesem Dokument als 2,931. Welche Trommel gilt, ist nicht entschieden — siehe [06-offene-fragen](06-offene-fragen.md) A14. Bis dahin ist `config.h` in sich stimmig (2,837 / 1,94) und dieses Dokument nicht. |
| — | `DRUM_LOSS_CONST_NM` | **0,472** | Nm | **gem** | Konstanter Anteil des Leerlaufverlustmoments: Lager- und Motorreibung. **Kürzt sich in der Pendelmessung heraus**, weil Trommellager und Motorreibung beide innerhalb der Wiege liegen. |
| — | `DRUM_LOSS_OMEGA2_NM` | **3,178·10⁻⁴** | Nm·s² | **gem** | Faktor des ω²-Anteils: **Luftwiderstand der Trommel**, der Teil, der sich *nicht* heraus kürzt. Beide Werte am 2026-11-17 aus dem **freien Auslauf** bestimmt: 97 → 29 km/h in 140 s, acht Stützstellen, M = J·dω/dt, Anpassung M = a + b·ω². Bei 60 km/h ergibt das 0,43 Nm Luftwiderstand — [09-messtheorie](09-messtheorie.md) nennt dafür 0,27 Nm, der gemessene Wert ist **1,6-fach größer**. Gegen ein Nutzsignal von 1,32 Nm (Crr 0,007 bei 417 N) sind das 33 % statt 20 %, bei 100 km/h sogar 91 %. Neu aufnehmen, sobald sich an Trommeloberfläche oder Einhausung etwas ändert. |
| L_kal | `CALIB_ARM_M` | **0,330** | m | Festlegung | Hebelarm des **Kalibriergewichts**. **Seit 2026-11-23: 330 mm** für das geeichte 1-kg-Gewicht — 38,5 N an der Zelle, 79 % der 5-kg-Zelle, deckt den Messbereich nach oben ab; Bohrung noch nicht gesetzt, nach dem Bohren den gemessenen Abstand eintragen. Vorher 0,160 geplant als zweite Bohrung im Arm. Das 2:1-Verhältnis zum Zellenarm ist Absicht: ein Gewicht dort erzeugt das doppelte Moment und damit die doppelte Kraft an der Zelle, also lässt sich mit handlichen Gewichten bis in den oberen Messbereich kalibrieren. Beide Arme sind **zur Laufzeit überschreibbar** (`settings.h`) — sie werden am Aufbau ausgemessen. Wie `TORQUE_ARM_M` auf ±0,1 mm nachzumessen. |
| i | `BELT_RATIO` | 1,00 | – | Festlegung | **Direktantrieb**: Trommel auf dem Motorrotor, keine Übersetzung. Konstante bleibt für den Fall eines späteren Riemens. |
| J | — | **2,07** | kg·m² | ber | Aus der Geometrie, [09-messtheorie](09-messtheorie.md) §9.1. Der frühere Wert 2,3 galt der Ø-955-Trommel. Geht **nicht** ins Ergebnis ein (gemessen wird stationär, dω/dt = 0), sondern nur in Rampenkorrektur, Hebelarmauslegung und die Energiebilanz. Nachmessen per Auslaufversuch mit bekannter Zusatzmasse. |

## 2 Momentenmessung

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| — | `LOADCELL_CAPACITY_N` | 49,03 | N | Ansage 2026-11-16 | **5 kg**. Die Unterlagen nannten bis dahin 10 kg — Typenschild nachlesen, dann auf DB setzen. |
| — | `LOADCELL_N_PER_COUNT` | **0,00002668** | N/count | **gem** | **Kalibriert 2026-11-23:** geeichtes 1-kg-Gewicht bei 330 mm, Zellenarm 84 mm → 38,53 N an der Zelle, Faktor über die Einrichtungsseite. Ein Gewicht, die Linearität über den Bereich steht noch aus. Vorschrift: Wiegeteil in Messlage, bekanntes Gewicht am Kalibrierpunkt anhängen, Rohwerte vorher/nachher mitteln, dividieren. Mindestens 3 Gewichte über den Bereich, Linearität prüfen. |
| — | `HX711_GAIN_PULSES` | 1 | – | DB | 1 = Kanal A Gain 128, 3 = Kanal B Gain 32 |
| — | `HX711_RATE_SPS` | 80 | 1/s | DB | RATE-Pin auf VCC. Bei 60 km/h ≈ 14 Samples pro Umdrehung. |
| — | `TARE_SAMPLES` | 200 | – | gesch | statische Tara, ≈ 2,5 s bei 80 SPS |
| `TARE_TIMEOUT_MS` | 5000 | ms | Festlegung | Abbruch der Tara, wenn nicht genug Werte kommen. Die Tara läuft seit dem 2026-11-20 **nicht blockierend** (`tareBegin`/`tareBusy`/`tareCount`); ohne Abbruch würde ein stummer HX711 den Zustand `TARE` auf Dauer besetzen. |
| — | `DISPLAY_AVG_N` | 40 | – | Festlegung | Gleitender Mittelwert für die **Anzeige**, 0,5 s bei 80 SPS. Ein mit 80 Hz zappelnder Zahlenwert ist weder lesbar noch aussagekräftig ([10-firmware](10-firmware.md) W5, D6). Hat mit der umdrehungssynchronen Mittelung der **Messung** (`AVG_REVS_PER_POINT`) nichts zu tun — die beiden laufen unabhängig. |

> Seit dem 2026-11-23 ist der Faktor gesetzt. Solange `LOADCELL_N_PER_COUNT` TBD wäre, gäbe die Firmware keine Crr-Werte aus, sondern
> nur Rohwerte. Das ist Absicht.

## 3 Drehzahlaufnehmer

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| k | `MARKS_PER_REV` | 8 | – | Festlegung | geschraubte Magnete auf dem Teilkreis der 8 Abstandsklötze, [04-hardware](04-hardware.md) §9. Mehr Marken bringen keine Genauigkeit, nur Latenz. |
| — | `WHEEL_MARKS_PER_REV` | **1** | – | Festlegung | Magnete an den Speichen des Prüflaufrads, Geber am Ausfallende, [09-messtheorie](09-messtheorie.md) §4. Verbaut ist seit 2026-11-22 **ein** Magnet. Drehzahl über ein Fenster von einer ganzen Umdrehung; mehr Magnete brächten keine Genauigkeit, nur eine schnellere Aktualisierung. Der Wert muss zum Rad passen, sonst sind Drehzahl und Laufradumfang um diesen Faktor falsch. |
| — | `WHEEL_CIRC_WINDOW_REVS` | 10 | – | Festlegung | gleitendes Fenster für den Laufradumfang, in Radumdrehungen; bei jedem Impuls neu gerechnet ([04-hardware](04-hardware.md) §14.1). Bei 15 km/h sind das rund 4 s. |
| — | `WHEEL_CIRC_GAP_MS` | 3000 | ms | gesch | ohne Radimpuls gilt das Rad als stehend, das Fenster verfällt |
| — | `MIN_PULSE_US` | 800 | µs | ber | Kürzester Abstand: 60 km/h, 8 Marken → 22,5 ms. Bei 100 km/h → 13,5 ms. 800 µs filtert sicher nur Störer. |
| — | `STALL_TIMEOUT_MS` | 2000 | ms | ber | Grenzfall ist nicht 10 km/h (135 ms bei 8 Marken), sondern der Hochlauf: bei 1 km/h sind es 1,35 s. 2000 ms bedeutet *langsamer als 0,68 km/h gilt als Stillstand*; die Rampe ist nach 1,4 s darüber. |
| — | `SLIP_TOLERANCE` | 0,05 | – | gesch | zulässige relative Abweichung n_Motor/i gegen n_Trommel |
| p | `MOTOR_POLE_PAIRS` | **15** | – | **gem** | **Gemessen 2026-11-17** über die Hall-Geber: bei 10 markierten Handumdrehungen stieg der Tachozähler des Reglers um genau **900** Schritte, ohne einen Rückwärtsschritt. 6 Schritte je elektrischer Umdrehung → 900/10/6 = 15 Polpaare (30 Pole). Die Skalierung „6 je elektrischer Umdrehung" ist am selben Datensatz gegen das ERPM-Integral geprüft (6,31; der Überschuss ist Zittern). Ein erster Versuch **sensorlos** gab 16,5 und war nicht reproduzierbar — der Lagebeobachter wackelt bei Handgeschwindigkeit; erst mit Hall-Gebern ist der Zähler eine Messung. Der Regler stand bis dahin auf `si_motor_poles` = 14 — das ist die **Werkseinstellung** der VESC-Firmware, nachgewiesen am frisch auf 7.01 geflashten Regler (dort steht wieder 14). Die Firmware **ermittelt** die Polzahl nicht, sie wird eingetippt; ohne Eintrag bleibt sie auf 14 stehen, und jede daraus abgeleitete mechanische Drehzahl ist falsch. |
| — | `MOTOR_PULSES_PER_REV` | **15** | – | ber | = `MOTOR_POLE_PAIRS`. Hallflanken einer Phase pro Motorumdrehung. Beim Direktantrieb zugleich eine hochauflösende Trommeldrehzahlquelle (90 Flanken je Umdrehung über alle drei Phasen). |

## 4 Antrieb

**Verbaut ist das Hoverboard-Antriebsrad 6,5″, 36 V / 350 W** (beschafft 2026-10-28),
dazu ein **FSESC 4.20 / 50 A** als Regler ([04-hardware](04-hardware.md) §10).

| Größe | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| Bauform | Hoverboard-Nabenmotor 6,5″ | – | Angebot | |
| Nennspannung / -leistung | 36 / 350 | V / W | Angebot | |
| Leerlaufdrehzahl bei 42 V | **589** | 1/min | gem | nackte Trommel über 100 km/h, 2026-11-03 |
| Dauermoment `P/ω` | **6,96** | Nm | ber | Faktor 2,8 über dem Auslegungsfall von 2,51 Nm |
| n → Trommel bei 36 V | 505 1/min → **86 km/h** | – | ber | für 60 km/h sind 353 1/min nötig, also 43 % Reserve |
| kv | **14,0** | 1/min/V | gem | 589 1/min bei 42 V — **das ist eine Systemgröße**, gemessen mit montierter Trommel und damit gegen deren Luftwiderstand, keine Motorkonstante. Die Motorkonstante steht in §4.1: 19,1 1/min je Busvolt am freien Motor. |
| kt | 0,68 | Nm/A | ber | 9,55 / kv — **überholt**, weil aus der Systemgröße kv gerechnet. Gemessen sind **0,389 Nm/A**, siehe §4.1. |
| Masse | 2,26 | kg | Angebot | geht in die Eigenfrequenz des Wiegeteils ein |
| Hallsensoren | ja, 3 Stück auf einer Platine im Stator | – | Foto des geöffneten Motors | 3 Flachstecker (gelb/grün/blau) + weißer JST mit 6 Adern, Belegung in [04-hardware](04-hardware.md) §11 |
| Temperaturfühler | **10-k-NTC** | – | **gem** | Weiße Ader am Hall-Stecker, führt ins Wickelpaket. Am 2026-11-17 dreifach belegt (A5 erledigt): offene Klemme −75,5 °C, 1,6 K Abstand zum bordeigenen FET-Fühler, monotoner Anstieg unter Heißluft gegen flache Referenz. `m_motor_temp_sens_type` = NTC 10K. Offen bleibt nur der **Grenzwert**. |
| **Mechanische Bremse** | **nein** | – | – | Gebremst wird über **Kurzschluss der drei Phasen** per Relais (verbaut und gemessen: 3,25 Nm) oder über den Regler mit `VESC_BRAKE_CURRENT_STOP`. Die Relais wirken stromlos, siehe [07-sicherheit](07-sicherheit.md) §3. |
| Polzahl | **30** (15 Polpaare) | – | **gem** | 2026-11-17 über die Hall-Geber ausgezählt, siehe `MOTOR_POLE_PAIRS` in §3 |
| Phasenwiderstand | **0,2387** | Ω | **gem** | siehe `MOTOR_R_OHM` in §4.1 |

### Nicht verbaut: Scooter-Motor (DAAO)

Die folgende Tabelle war die Alternative und ist nur noch als Vergleich interessant.
Angaben vom Typenschild.

| Größe | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| Hersteller | DAAO Electric Co. Ltd | – | Typenschild | |
| Typ | 23328N36A28G001 | – | Typenschild | |
| Nennspannung | 36 | V | Typenschild | |
| Nennleistung | 350 | W | Typenschild | |
| Spitzenleistung | 800 | W | Verkäufer | Anfahrreserve für die Schwungmasse |
| **Nenndrehzahl** | **605** | **1/min** | **Typenschild** | die Auslegungszahl |
| Reifen | 9.0 × 2.125 | Zoll | Reifenflanke | Vollgummi |
| Radumfang | 0,718 | m | ber | Ø 9,0″ = 228,6 mm |
| kv am Rad | 16,8 | 1/min/V | ber | 605 / 36 |
| kt | 0,568 | Nm/A | ber | 9,55 / kv |
| Nennmoment | 5,52 | Nm | ber | 350 W / 63,4 rad/s |
| Spitzenmoment | ≈ 12,6 | Nm | ber | 800 W bei Nenndrehzahl |
| Moment an der Trommel | 5,52 | Nm | ber | 5,52 · i, **i = 1,00** (Direktantrieb) |
| Mechanische Bremse | Trommelbremse | – | Beschreibung | **der verbaute Hoverboard-Motor hat sie nicht** |
| Stecker | Julet 9-polig | – | Foto | 3 Phasen + 5 Hall + PTC erwartet — **mit dem Messgerät verifizieren** |
| Hallsensoren | ja | – | Steckerbelegung | 9-poliger Julet, erwartet 3 Phasen + 5 Hall + PTC |
| Temperaturfühler | PTC | – | Beschreibung | weißes Kabel; Kennlinie **TBD** |
| Bremse | Trommelbremse integriert | – | Beschreibung | mechanischer Bremspfad, siehe [07-sicherheit](07-sicherheit.md) |
| Polzahl | **TBD** | – | TBD | Motor von Hand eine Umdrehung drehen, Hallflanken zählen |
| Phasenwiderstand | **TBD** | Ω | TBD | Vierleitermessung zwischen je zwei Phasen |
| Masse | **TBD** | kg | TBD | wiegen — geht in die Eigenfrequenz des Wiegeteils ein |

### Auswahlkriterium für einen Nabenmotor

Falls der Motor einmal ersetzt werden muss: **die Nennleistung allein sagt nichts.**
Zwei Motoren mit gleichem Typenschild liefern unterschiedliches Moment, je nach
Nenndrehzahl. Zu prüfen sind zwei Größen:

```
T_dauer = P_nenn / (2π · n_nenn / 60)   ≥ 2,51 Nm   (Auslegungsfall samt Rampe, §6)
kv      = n_nenn / U_nenn               = 14…20 1/min/V
```

Das kv-Fenster ergibt sich aus der Trommeldrehzahl: 341 1/min bei 60 km/h, geteilt durch
36 V Busspannung, sind 9,25 1/min/V als absolute Untergrenze — praktisch 14 bis 20 mit
Spannungsreserve für den Strom. Angebote mit 100 kv oder mehr sind Motoren zum
Herunterübersetzen, kein Direktantrieb.

Daraus die Mindestleistung:

Die folgende Tabelle ist noch mit der **alten** MTB-Auslegung (3,53 Nm) gerechnet und
damit um Faktor 1,4 konservativ. Für die heutige Auslegung mit 2,51 Nm genügen 71 % der
genannten Werte — die Empfehlung bleibt trotzdem stehen, weil Typenschildangaben
billiger Motoren optimistisch sind.

| n_nenn bei 36 V | ω | Minimum (alt, 3,53 Nm) | mit 50 % Reserve |
|---|---|---|---|
| 400 1/min | 41,9 rad/s | 148 W | 222 W |
| 500 1/min | 52,4 rad/s | 185 W | 277 W |
| **605 1/min** (verbauter Motor) | 63,4 rad/s | **224 W** | **336 W** |
| 700 1/min | 73,3 rad/s | 259 W | 388 W |

Praktisch: **250 W Minimum, 350 W richtig.** Unter 400 1/min Nenndrehzahl wird es
unabhängig von der Leistung eng, weil dann kaum Spannungsreserve über den für 60 km/h
nötigen 341 1/min bleibt.

Der Grund für den scheinbar großen Abstand zwischen den 123 W mechanischem Bedarf (§4 in
[09-messtheorie](09-messtheorie.md)) und den geforderten 224 W Nennleistung: Der Prüfstand
ist **momentenbegrenzt, nicht leistungsbegrenzt**. Bei 341 1/min läuft der Motor mit 55 %
seiner Nenndrehzahl und kann höchstens 55 % seiner Nennleistung abgeben.

> Typenschildangaben billiger Motoren sind optimistisch; die thermische Dauerleistung
> liegt häufig darunter. Die Tabelle gilt für 42,5 kg Auflast und die alte Crr-Grenze
> 0,015. Nur
> Rennreifen (Crr ≤ 0,008) kämen mit 2,06 Nm und damit 200 W aus; 100 kg Auflast
> verdoppeln alles.

### Geschwindigkeitsreserve

Beim **Direktantrieb** ist die Geschwindigkeit nicht spannungsbegrenzt. Trommel- und
Motordrehzahl sind identisch, 605 1/min Nenndrehzahl bei 36 V ergeben an der Trommel
bereits **109 km/h** — fast das Doppelte der zugelassenen 60 km/h.

| | n_Motor = n_Trommel | v an der Trommel |
|---|---|---|
| Nenndrehzahl bei 36 V | 605 1/min | 109 km/h |
| **zugelassen** | **341 1/min** | **60 km/h** |

Die Grenze ist also **mechanisch**, nicht elektrisch: Wuchtgüte und Festigkeit des
Holzaufbaus, siehe [07-sicherheit](07-sicherheit.md) §5 und A2 in
[06-offene-fragen](06-offene-fragen.md). Eine höhere Busspannung bringt nichts, solange
diese Freigabe nicht vorliegt.

Umgekehrt heißt das: Bei 341 1/min läuft der Motor mit 55 % seiner Nenndrehzahl, und die
nutzbare Leistung ist entsprechend gedeckelt — der Prüfstand ist momenten-, nicht
drehzahlbegrenzt.

### Alternative: Hoverboard-Nabenmotor 6,5″

TY36VH25B350W, 36 V / 350 W, 2,26 kg. Nenndrehzahl ≈ 480 1/min (**abgeleitet** aus der
üblichen Hoverboard-Endgeschwindigkeit, steht nicht auf dem Typenschild — die 605 1/min
des DAAO-Motors sind dagegen aufgedruckt).

| | Hoverboard 6,5″ | Scooter 9,5″ (DAAO) |
|---|---|---|
| Dauermoment `P/ω_nenn` | **6,96 Nm** | 5,52 Nm |
| Hallsensoren | ja — 3 dicke Phasen (gelb/grün/blau) + weißer JST mit 5 dünnen Adern | ja |
| n_nenn → Trommel max | 480 1/min → 86 km/h | 605 1/min → 109 km/h |
| Temperaturfühler | vermutlich ja — weiße Ader ins Wickelpaket, Typ **TBD** (A5) | PTC |
| Mechanische Bremse | **nein** | Trommelbremse |
| Spitzenlast | unbekannt | 800 W |
| Masse | 2,26 kg | schwerer, **TBD** |
| Lochkreis für den Trommeladapter | 6,5″ | 9,5″, steifer |

Beide erfüllen selbst die alte Anforderung von 3,53 Nm mit Reserve, die heutige von
2,51 Nm erst recht. Die frühere Einschätzung, die
„6-adrige Kabelpeitsche" mache Hallsensoren zweifelhaft, hat sich an den Angebotsfotos
erledigt: es sind 8 Leiter, die Beschreibung zählte nur den Hall-Strang.

Übrig bleibt als Unterschied die **mechanische Bremse**, und die ist bei 13 Minuten
freiem Auslauf (siehe [09-messtheorie](09-messtheorie.md) §8.9) kein Komfortmerkmal.
Nachrüstbar ist sie aber: Bandbremse oder Bremssattel auf einer Scheibe an der
Trommelwelle.

**Entscheidungsregel:** Ist das Hoverboard-Rad deutlich billiger, nimm es und plane eine
Bremse an der Trommel ein. Liegen die Preise nah beieinander, nimm den Scooter-Motor und
spare dir die beiden Nebenbaustellen Bremse und Temperaturfühler.

Die Achsfrage A6 — beidseitige Lagerung — gilt für beide gleichermaßen und entscheidet
über den Direktantrieb, nicht über die Motorwahl.

## 4.1 Motor, elektrisch gemessen

Alles am **2026-11-17** am verbauten Motor ermittelt, Regler FSESC 4.20 (damals noch
Firmware 5.02).

Im Regler stand eine **fremde Konfiguration**. Welche Werte darin wirklich fremd waren,
ließ sich erst nach dem Flashen auf 7.01 trennen — der Werkszustand ist seither bekannt:

| | vorgefunden | Werk | |
|---|---|---|---|
| `si_motor_poles` | 14 | **14** | Werkseinstellung |
| `si_wheel_diameter` | 0,083 m | **0,083 m** | Werkseinstellung |
| `s_pid_kp` / `ki` / `kd` / `min_erpm` | 0,004 / 0,004 / 0,0001 / 900 | **gleich** | Werkseinstellung |
| `si_gear_ratio` | 2,769 | 3,0 | von Hand gesetzt |
| R / L / λ | 0,0147 Ω / 2,02 µH / 1,65 mWb | 0,015 / 7 µH / 2,45 mWb | **Erkennung an einem anderen Motor** |
| `foc_openloop_rpm` | 700 | 1500 | von Hand gesetzt |

Es lief also eine Motorerkennung an einem fremden Motor, und zwei Werte wurden
eingetippt. *Frühere Fassungen dieses Dokuments nannten pauschal alles
„Skateboard-Konfiguration" — das war zu weit gegriffen.*

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| λ | `MOTOR_FLUX_LINKAGE_WB` | **0,017287** | Wb | **gem** | Vom Regler erkannt (`foc_detect_apply_all` mit 50 W Verlustgrenze). Scheitelwert der Strangverkettung. Gegenprobe: 763 1/min ergeben damit 20,7 V Strangspitzenspannung, und 40 V Bus liefern über Raumzeigermodulation höchstens 40/√3 = 23,1 V — Verhältnis 0,90, passend zur Vollaussteuerung. |
| kt | `MOTOR_KT_NM_PER_A` | **0,389** | Nm/A | ber | kt = 3/2 · p · λ für eine dreiphasige PMSM in feldorientierter Regelung. Gegenprobe am Hochlauf: 3 A ergaben 1,17 Nm, und die Trommel beschleunigte wie aus `DRUM_INERTIA_KGM2` erwartet. |
| R | `MOTOR_R_OHM` | **0,2387** | Ω | **gem** | `measure_res_ind` bei **direkter** Verdrahtung: 237,2 mΩ; die Erkennung trug 238,7 mΩ ein. **Mit** dem Kurzschlussrelaisblock im Pfad 251,9 mΩ (Mittel aus vier Läufen, Streuung ±1,5 mΩ), also 14,7 mΩ Kontaktwiderstand. Das ist ein Wirkungsgradposten und erscheint **nicht** an der Wägezelle — elektrische Verluste sind kein mechanisches Reaktionsmoment. |
| — | `MOTOR_NO_LOAD_RPM_PER_V` | **19,1** | 1/min/V | **gem** | Leerlaufdrehzahl je Busvolt, am **freien** Motor über zehn Stützstellen von Tastverhältnis 0,50 bis 0,95 bei 40 V, streng linear (11 384 bis 11 450 ERPM je Einheit Tastverhältnis). Extrapoliert 11 450 ERPM = **763 1/min** bei Vollaussteuerung, an der Trommel 130 km/h. **Nicht** zu verwechseln mit den „kv = 14" aus [06-offene-fragen](06-offene-fragen.md) A13: das ist die Endgeschwindigkeit des Systems **mit** Trommel, also gegen deren Luftwiderstand — eine Systemgröße, keine Motorkonstante. |
| L | — | *(offen)* | H | **TBD** | `measure_res_ind` streute über vier Läufe von **305 bis 377 µH**, die Erkennung trug 403,8 µH ein. Bei ±13 % Streuung darf hier keine Zahl stehen. Braucht ein anderes Verfahren (LCR-Messbrücke bei 1 kHz, Rotor in definierter Lage). |

## 5 Messbetrieb

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `V_MIN_KMH` | 10 | km/h | Festlegung | |
| `V_MAX_KMH` | 60 | km/h | Festlegung | 100 km/h erst nach Trommelfreigabe |
| `PLATEAUS_KMH` | 15, **28,8**, 40, 50, 60 | km/h | Festlegung | **Vorgabe** der Messgeschwindigkeiten; die gültige Liste liegt seit 2026-11-23 in den Einstellungen (NVS) und wird über *Einrichtung → Messung* gepflegt, sortiert, mit `h<kmh>` / `h-<kmh>`. 28,8 km/h ist die Referenzgeschwindigkeit der veröffentlichten Crr-Datenbank — zusammen mit 42,5 kg Last ein direkt vergleichbarer Punkt; 60 km/h ist `V_MAX_KMH`. Leerlauf und Messlauf fahren dieselbe Liste. |
| `PLATEAU_MAX` | 16 | – | Festlegung | Platz für Messgeschwindigkeiten in den Einstellungen |
| `RAMP_KMH_PER_S` | 0,5 | km/h/s | gesch | 2 min von 0 auf 60 |
| `SETTLE_MS` | 15000 | ms | gesch | Einschwingen nach Plateauwechsel; nach ersten Messungen prüfen |
| `AVG_REVS_PER_POINT` | 10 | – | gesch | bei 60 km/h ≈ 1,8 s, bei 10 km/h ≈ 10,8 s |
| `WARMUP_MS` | 1800000 | ms | Literatur | **30 min**, wie beim Referenzprüfstand. Crr sinkt mit der Reifentemperatur deutlich. |
| `WARMUP_KMH` | 28,8 | km/h | Festlegung | Geschwindigkeit, bei der die Warmlaufzeit zählt. Identisch mit `PLATEAUS_KMH[1]` und aus demselben Grund: es ist die Stützstelle der veröffentlichten Datenbank. Vorher stand die Zahl nur im Fließtext. |
| `WARMUP_TOL_KMH` | 1,0 | km/h | gesch | Band um `WARMUP_KMH`, innerhalb dessen Zeit angerechnet wird. Breiter als `SPEED_TOLERANCE_KMH`, weil hier nicht gemessen, sondern gewärmt wird. |
| `DVDT_WINDOW_MS` | 1000 | ms | Festlegung | Fenster für dv/dt. `revsrc.cpp` legt acht Proben darin ab und fittet eine Gerade — eine Differenz aus zwei Proben rauscht zu stark für eine Gültigkeitsentscheidung. |
| `DVDT_CONST_KMH_PER_S` | 0,010 | km/h/s | **ber** | Grenze für „konstant". Ein Rest an Beschleunigung erzeugt über `M = J·dω/dt` ein Scheinmoment: 1 % vom Signal eines guten Rennreifens sind 0,012 Nm, über `J` = 1,94 kg·m² also 0,0062 rad/s² und damit 0,010 km/h/s ([10-firmware](10-firmware.md) §7.1, L9). Punkte außerhalb werden als ungültig markiert, nicht verworfen. |
| `DVDT_STRICT_KMH_PER_S` | 0,002 | km/h/s | **ber** | Dieselbe Rechnung für 0,2 % Fehler (L11), wählbar für schärfere Läufe. |

## 6 Prüfling und Last

| Größe | Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|---|
| Auflast, Vorgabe | `LOAD_DEFAULT_N` | **494,3** | N | gem | **50,4 kg, Stand vom 2026-11-23:** 7,2 kg leer (Arm und Rad, am Rad gewogen) plus 10,4 kg bei 84 cm über den Radarm von 20,2 cm, Hebel 4,16. Vorgabe für die Einstellung, am Stand änderbar; für 416,9 N müsste das Gewicht bei 68,6 cm hängen. |
| Standard-Auflast | `STANDARD_LOAD_N` | 416,9 | N | Literatur | 42,5 kg — der De-facto-Standard der veröffentlichten Crr-Datenbank. Nur Vorschlagswert; die tatsächliche Last wird über `l<wert>` eingegeben. |
| Auflast-Mechanik | — | Hebelwaage | – | Entwurf | Laufgewicht 10 kg, `N = m_Rad·g + W·b/a` mit a = 0,200 m, b = 0,30…0,90 m → 147…441 N. Entwurf und Abgleichvorschrift: [08-staender](08-staender.md) §5. Geht **linear** in Crr ein → auf 1 % genau nötig. |
| Auslegungs-Crr | `CRR_DESIGN_MAX` | **0,007** | – | Festlegung | Es werden schnelle Straßenreifen gemessen, keine MTB-Reifen. Langsamster vorgesehener Reifen ist der Continental Contact Urban mit 0,00552, darauf 27 % Luft. Siehe Tabelle unten. |
| Reifendruck | — | Eingabe | bar | – | Handeingabe, später BLE |
| Reifentemperatur | — | **TBD** | °C | TBD | Sensor noch nicht festgelegt |
| Motortemperatur | — | **vorhanden** | °C | **gem** | 10-k-NTC im Stator, A5 am 2026-11-17 beantwortet. Fühler hängt am Eingang `TMP` des **Reglers**, Auslesung über UART — [04-hardware](04-hardware.md) §11.4. Offen ist nur noch der **Abschaltgrenzwert**, siehe [07-sicherheit](07-sicherheit.md) §4. |

### Gemessene Crr-Werte als Auslegungsgrundlage

Einheitlich bei 42,5 kg und 28,8 km/h erhoben; Momente mit r = 0,4665 m gerechnet und
**ohne** Leerlauf und Laufradverluste (die addieren rund 0,51 Nm, siehe
[08-staender](08-staender.md) §5.1)
([Quelle](https://www.bicyclerollingresistance.com/the-test)):

| Reifen | Leistung | Crr | Moment an unserer Trommel |
|---|---|---|---|
| Vittoria Corsa Speed (bester Rennreifen) | ~11 W | ~0,0025 | 0,49 Nm |
| typischer guter Rennreifen | 13–17 W | 0,0035–0,0040 | 0,68–0,78 Nm |
| [Schwalbe Marathon Plus](https://www.bicyclerollingresistance.com/tour-reviews/schwalbe-marathon-plus-2015) | 25,5 W | 0,00764 | 1,49 Nm |
| **Continental Contact Urban — langsamster vorgesehener Reifen** | **18,4 W** | **0,00552** | **1,07 Nm** |
| **Auslegungsgrenze `CRR_DESIGN_MAX`** | | **0,007** | **1,36 Nm** |
| [Schwalbe Hans Dampf TrailStar, langsamster MTB](https://www.bicyclerollingresistance.com/mtb-reviews) — *nicht vorgesehen* | 42,2 W | ≈ 0,0127 | 2,47 Nm |

Der MTB-Crr ist selbst gerechnet, weil die Quelle dort nur Watt ausweist:
`42,2 W / 8,0 m/s / (42,5 · 9,81 N) = 0,0127`.

> Achtung bei höheren Lasten: Bei 100 kg Auflast (Lastenrad, E-Bike) ergibt Crr 0,0127
> allein für den Reifen 5,95 Nm und überschreitet damit das Dauermoment des Motors.
> Die Auslegung gilt für 42,5–50 kg.

> **Wer doch einmal MTB-Reifen messen will**, muss nicht nur `CRR_DESIGN_MAX` wieder
> anheben, sondern auch `TORQUE_ARM_M` verlängern — sonst läuft die Wägezelle bei
> Crr 0,0127 samt Anfahrmoment über ihren Endwert. Mit der 5-kg-Zelle läge der
> langsamste MTB-Reifen (2,98 Nm) bei 0,080 m schon bei 76 %. Beide Werte gehören
> zusammen.

## 7 Sicherheitsgrenzen

| Konstante | Wert | Einheit | Begründung |
|---|---|---|---|
| `MAX_FORCE_N` | 45 | N | 92 % vom Zellenendwert der 5-kg-Zelle → Abschaltung vor der Sättigung |
| `MAX_DRUM_RPM` | **abgeleitet** → 341 bei U = 2,931 m | 1/min | `V_MAX_KMH / 3,6 / U · 60`. Nicht hart gesetzt: die Grenze bedeutet *60 km/h an der Trommeloberfläche* und muss dem Umfang folgen — der alte feste Wert 333 galt der Ø-955-Trommel und hätte an der gebauten Ø-933-Trommel schon bei 58,4 km/h ausgelöst, mitten im obersten Messplateau. Erhöhung von `V_MAX_KMH` erst nach Freigabe in [07-sicherheit](07-sicherheit.md) §5. |
| `STALL_TIMEOUT_MS` | 2000 | ms | siehe §3. Dient zugleich als Grenze, ab der `revsrc` von den Trommelmarken auf die Reglerdrehzahl umschaltet. |
| `STALL_DUTY_MIN` | 0,05 | – | gesch | Stellwert, ab dem „treibt, Trommel dreht nicht" ein Befund ist und kein Rauschen. Am 2026-11-18 lagen 0,045 Stellwert und 2,5 A Motorstrom an, während die Trommel stillstand — das ist Kommutierung, nicht Last ([04-hardware](04-hardware.md) §11). Ohne eigene Trommelmarken ist das der einzige Weg, Stillstand trotz Sollwert zu erkennen. |
| `FAULT_SETTLE_MS` | 500 | ms | Entprellung der Fehlerkriterien |
| `ESTOP_IMOT_MAX_A` | 0,3 | A | gesch | Motorstrom, unter dem bei anliegendem Stellwert **kein Motor am Regler** hängt (Not-Aus ausgelöst oder abgeklemmt). Mit Motor zog der stehende Antrieb am 2026-11-23 bei 0,11 Stellwert 6,6 A; nach ausgelöstem Not-Aus aus 60 km/h lag Imot bei −0,2…+0,1 A, während die Geber noch 25 s Drehzahl meldeten — deshalb keine Drehzahlbedingung. |
| `ESTOP_SPEED_RATIO` | 0,6 | – | gesch | Not-Aus gilt erst, wenn die Drehzahl unter diesem Anteil der Leerlaufdrehzahl zum Stellwert liegt — zusammen mit dem fehlenden Strom. Im Leerlauf bei 28 km/h lag der Strom bei 0,2 A, aber die Drehzahl bei 100 % ([07-sicherheit](07-sicherheit.md) §4). |
| `ESTOP_DECEL_KMH_S` | 0,5 | km/h/s | gesch | zweiter Weg: Trommel wird ohne Strom schneller als das langsamer, und der Sollwert sinkt nicht — ein angeschlossener Motor zöge sofort Strom. Nach dem Not-Aus aus 28 km/h etwa 3 km/h/s, die Regelung pendelt mit 0,1…0,2. |
| `ESTOP_DETECT_MS` | 1000 | ms | gesch | so lange muss der Befund stehen |
| `ESTOP_FB_WIRED` | false | – | TBD | Hilfskontakt des Not-Aus an `PIN_ESTOP_FB` — noch nicht verdrahtet. Kontakt soll nach Masse ziehen, wenn ausgelöst. |
| `LINK_TIMEOUT_MS` | 2000 | ms | **Totmannschaltung.** Solange ein Lauf fährt, muss der Bedienrechner sich melden; bleibt er länger aus, bricht der Lauf ab. Dieselbe Größenordnung wie `STALL_TIMEOUT_MS`. Die Oberfläche fragt 5×/s ab, es dürfen also zehn Abfragen ausfallen. Gilt **nicht**, solange nie ein Client da war — dann wird über die Konsole gefahren. [07-sicherheit](07-sicherheit.md) §4 |

Die WLAN-Konstanten `WEBUI_*` stehen in [04-hardware](04-hardware.md) §13.1, weil sie
zum Aufbau gehören und nicht zur Physik.

## 8 Motorregler über UART

Nur für die Reglervariante (`env:esp32s3-vesc`), siehe
[04-hardware](04-hardware.md) §14. Die Hauptfirmware benutzt sie noch nicht.

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `VESC_BAUD` | 115200 | Bd | DB | Vorgabe der VESC-Firmware |
| `VESC_POLL_MS` | 200 | ms | Festlegung | Abfrage der Messwerte. Das Intervall **ist** zugleich das Mittelungsfenster: der Regler liefert Ströme als Mittelwert seit der letzten Abfrage und setzt den Akkumulator beim Lesen zurück. |
| `VESC_ALIVE_MS` | 150 | ms | Festlegung | Wiederholung des Sollwerts. Der Regler stellt ab, wenn länger als `timeout_msec` nichts kommt (dort auf 300 ms eingestellt); 150 ms lässt zwei Ausfälle zu. **Messwertabfragen setzen den Timeout nicht zurück** — nur Sollwerte und `COMM_ALIVE`. |
| `VESC_ERPM_MAX` | **abgeleitet** → 5295 | ERPM | ber | `MAX_DRUM_RPM · MOTOR_POLE_PAIRS`. Seit die Polpaarzahl **gemessen** ist (§3), folgt die Kappung direkt aus `V_MAX_KMH` und zieht automatisch mit. Vorher stand hier 2400, abgeleitet aus den `si_motor_poles` = 14 des Reglers unter der Annahme, das sei der kleinste plausible Wert — es waren 30 Pole. Der Antrieb ist **nicht** der begrenzende Faktor: am freien Motor 763 1/min bei Vollaussteuerung, an der Trommel 130 km/h, für 60 km/h genügt Tastverhältnis 0,46. Kein Ersatz für `MAX_DRUM_RPM`, sondern eine Kappung davor. |
| `VESC_ERPM_STEP_SMALL` | 100 | ERPM | Festlegung | Feineinstellung über ± |
| `VESC_ERPM_STEP_LARGE` | 1000 | ERPM | Festlegung | grobe Annäherung |
| `VESC_DIRECTION_SIGN` | **+1** | – | **gem** | Vorzeichen, mit dem Sollwerte an den Regler gehen, damit die Trommel in **Messrichtung** dreht. **Seit 2026-11-23 +1:** nach dem Neuverdrahten lagen Hall- oder Phasenleitungen anders, die Hall-Tabelle wurde neu erkannt, und mit dem alten −1 lief die Trommel rückwärts — am Stand gesehen. Vom 2026-11-17 bis dahin −1, bei damals geprüfter Phasenzuordnung (Relaisblock 1:1). **Nach jeder Neuerkennung der Hall-Tabelle das Vorzeichen prüfen** (`e` von Hand drehen, oder `e1` / `e-1` direkt). Steht bewusst **hier** und nicht als `m_invert_direction` im Regler: dort wäre die Festlegung nicht versioniert und ginge bei jeder Neuerkennung verloren. Zwei Fallstricke: zwei vertauschte Phasen im Relaisblock drehen die Richtung **und** entwerten die Hall-Tabelle (der Motor brummt dann stromführend ohne Moment und meldet **keinen** Fehler, [04-hardware](04-hardware.md) §10.6); und ein Richtungswechsel im Lauf ist ein **Bremsbefehl** ([07-sicherheit](07-sicherheit.md) §3.3). |
| `VESC_BRAKE_CURRENT_STOP` | **20,0** | A | **gem** / Festlegung | Der **einzige** Bremsstrom. Festlegung des Erbauers: es wird immer der schnellstmögliche Stopp gefahren, nie ein langsamer — Rückgewinnung ist damit kein Auswahlkriterium. Gemessen 50 → 5 km/h in **13,1 s**; 15 A wären mit 13,9 s kaum langsamer und energieneutral, aber die Vorgabe lautet schnellstmöglich. Über 20 A ist die Bremse gesättigt. **Nur mit Akku am Bus** — am Netzteil Abwurf nach 0,61 s mit Fehlercode 1, weil der Regler bei hoher Drehzahl zunächst **zurückspeist**. Vollständige Kurve in [07-sicherheit](07-sicherheit.md) §3.2b, die widerlegte Gegenannahme in §3.2c. |

## 8.2 Geschwindigkeitsregelung der Trommel

Gilt für die **Testlauf-Variante** (`env:esp32s3-vesc`). Gefahren wird über das
**Tastverhältnis**, nicht über den Drehzahlregler des VESC: der ist für diesen Motor
unbrauchbar eingestellt, und `s_pid_min_erpm` = 900 sperrt die unterste Prüfstufe
(10 km/h sind 881 ERPM) — [06-offene-fragen](06-offene-fragen.md) A15.

Die Vorsteuerung kommt aus `MOTOR_NO_LOAD_RPM_PER_V` (§4.1) mal der **gemessenen**
Busspannung, darüber liegt ein langsamer Trimm. So ist der Stand am 2026-11-17 bis
99,2 km/h gefahren worden.

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `SPEEDCTL_RAMP_KMH_PER_S` | 2,0 | km/h/s | Festlegung | Rampe des inneren Sollwerts: 30 s von 0 auf 60. Langsam genug, dass ein Fehlgriff auffällt, bevor es schnell wird. |
| `SPEEDCTL_KP` | 0,02 | 1/(km/h·s) | Festlegung | Trimmverstärkung. Die Vorsteuerung trägt die Hauptlast; der Trimm holt nur Reibung und Spannungsabfall nach. |
| `SPEEDCTL_DUTY_UP_PER_S` | 0,30 | 1/s | Festlegung | Höchste Änderung des Tastverhältnisses nach **oben**. |
| `SPEEDCTL_DUTY_DOWN_PER_S` | 0,06 | 1/s | Festlegung | …und nach **unten**, ein Fünftel davon. **Die Asymmetrie ist der Kern:** herunterstellen ist ein Bremsbefehl. Ein Tastverhältnis unter dem, was die momentane Drehzahl braucht, lässt den Regler zurückspeisen — am Netzteil endet das in Überspannung ([07-sicherheit](07-sicherheit.md) §3.3). |
| `SPEEDCTL_CLAMP_FACTOR` | 1,08 | – | Festlegung | Klemme gegen Weglaufen des Stellwerts, als Vielfaches der Leerlaufkennlinie des **Sollwerts**. |
| `SPEEDCTL_CLAMP_OFFSET` | 0,02 | – | Festlegung | Zuschlag derselben Klemme für die Reibung. Ohne diese Klemme integriert der Trimm bis an den Anschlag, solange die Stromgrenze die Beschleunigung bestimmt — und die Trommel schießt über den Sollwert hinaus, sobald der Strom frei wird. Am 2026-11-17 genau so passiert: Stellwert 0,95 bei tatsächlichen 0,26. |
| `SPEEDCTL_PERIOD_MS` | 20 | ms | Festlegung | Takt des Regelkreises. Schneller bringt nichts — die Drehzahl kommt mit `VESC_POLL_MS` = 200 ms herein. |
| `CMD_MAX_AGE_MS` | 2000 | ms | **Sicherheitsgrenze** | Hoechstalter eines Befehls aus der Oberflaeche. Jeder Befehl traegt den Zeitstempel mit, den die Oberflaeche zuletzt gesehen hat; was aelter ist, wird mit **409** abgewiesen. **STOP und Auslaufen sind ausgenommen** — ein verspaetetes STOP ist harmlos, ein verworfenes nicht. Grund: am 2026-11-18 kam beim Seitenaufbau ein alter `v20` als Browser-Wiederholung erneut an und setzte den Antrieb in Gang ([07-sicherheit](07-sicherheit.md) §6). |
| `BREAKAWAY_RATE_PER_S` | 0,01 | 1/s | Festlegung | Anstieg des Tastverhaeltnisses in der Losbrechmessung. Langsam genug, dass der Moment des Losbrechens ablesbar ist. |
| `BREAKAWAY_DUTY_MAX` | 0,15 | – | Festlegung | Obergrenze der Messung. Bricht sie bis dahin nicht los, wird abgebrochen und **kein** Wert gemeldet — statt einen zu erfinden. |
| `BREAKAWAY_DETECT_KMH` | 0,5 | km/h | Festlegung | Ab hier gilt die Trommel als bewegt, die Messung endet sofort. |
| `SPEEDCTL_LEARN_MIN_KMH` | 2,0 | km/h | Festlegung | Ab dieser Drehzahl gilt das Anlernen der Drehrichtung als eindeutig. Darunter koennte das Vorzeichen aus Rauschen stammen. |
| `SPEEDCTL_LEARN_TIMEOUT_MS` | 20000 | ms | Festlegung | Wird in dieser Zeit nichts gedreht, bricht das Anlernen ab, statt stillschweigend zu warten. |
| `MOTOR_TEMP_OPEN_C` | **−20** | °C | **gem** | Darunter ist der Motorfuehler **offen**, nicht kalt. Am 2026-11-17 gemessen: offene Klemme −75,5 °C, angeschlossen 19,4 °C ([06-offene-fragen](06-offene-fragen.md) A5). Am 2026-11-18 zeigte eine nicht gesteckte Sensorleitung −82 °C. Weil TMP und die drei Hall-Adern im **selben** Stecker liegen, ist der offene Fuehler zugleich der brauchbare Indikator fuer die Hall-Geber — der Regler liefert die rohen Hall-Zustaende nicht mit. |
| `SPEED_STEP_KMH` | 5 | km/h | Festlegung | Schrittweite der Bedienknöpfe. Das Raster reicht von `V_MIN_KMH` bis 100; Stufen über `V_MAX_KMH` zeigt die Oberfläche **gesperrt** an, weil die Freigabe nach [07-sicherheit](07-sicherheit.md) §5 fehlt. |

> **Drei dieser Werte stehen nicht aus Geschmack dort.** Vorbesetzung des Stellwerts
> aus der gemessenen Drehzahl, asymmetrische Änderungsrate und die Klemme sind jeweils
> die Antwort auf einen Fehler, der am 2026-11-17 am laufenden Aufbau passiert ist.
> Die Begründungen stehen vollständig in `firmware/src/speedctl.h`.

## 8.1 Status-LED

Die RGB-LED des DevKits ist eine adressierbare **WS2812**, kein einfacher Ausgang; sie
wird über `neopixelWrite()` angesteuert, das intern RMT benutzt.

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `PIN_STATUS_LED` | 48 | – | **gem** | Am verbauten Board bestätigt (2026-11-15). Auf DevKitC-1 v1.1 säße die LED an GPIO 38 — dieses Board hat sie an 48, wie auch der Arduino-Core für den S3 annimmt (`RGB_BUILTIN`). |
| `STATUS_LED_DIV` | 6 | – | Festlegung | Helligkeitsteiler. Die LED blendet bei voller Aussteuerung auf dem Tisch. |
| `STATUS_BLINK_MS` | 700 | ms | Festlegung | Halbe Periode des roten Blinkens, solange auf die Bedienverbindung gewartet wird. **Grün stetig** = ein Bedienrechner meldet sich, **rot blinkend** = es wartet keiner. Damit ist die Totmannschaltung aus drei Metern ablesbar. |

Beide Pins nacheinander zu bespielen geht **nicht**: `neopixelWrite()` initialisiert RMT
einmalig für den Pin des ersten Aufrufs und wechselt danach nicht mehr. Deshalb ein
Pin, und im Zweifel umstellen statt beides zu versuchen.
