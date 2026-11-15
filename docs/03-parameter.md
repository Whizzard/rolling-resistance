# Parameter

Spiegelt `firmware/src/config.h` 1:1. Jede Änderung hier **und** dort.

Herkunft: `gem` = gemessen · `ber` = berechnet · `DB` = Datenblatt · `gesch` = geschätzt ·
`TBD` = noch zu ermitteln.

## 1 Geometrie

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| U | `DRUM_CIRCUMFERENCE_M` | **2,931** | m | gem | Über die fertige Oberfläche gemessen, Ø 933 mm ([09-messtheorie](09-messtheorie.md) §1.5). Der frühere Wert 3,000 galt der geplanten Ø-955-Trommel und war 2,4 % zu groß. Nach jeder Änderung an der Oberfläche neu messen: Maßband um die Trommel, 3 Messungen mitteln. 1 mm Fehler = 0,03 %. |
| r | `DRUM_RADIUS_M` | **0,4665** | m | ber | U / 2π |
| L | `TORQUE_ARM_M` | **0,040** | m | ber | Von 0,070 **verkürzt**, weil `CRR_DESIGN_MAX` auf 0,007 gesunken ist — sonst blieben zwei Drittel des Messbereichs ungenutzt. Auslegungsfall stationär 1,88 Nm → 47,0 N = 48 % FS; samt Rampe 2,51 Nm → 62,7 N = 64 %; ΔCrr 0,0005 → 2,43 N. Herleitung [08-staender](08-staender.md) §5.2. Bohrungen 40,0 mm (Zelle) und 80,0 mm (Kalibrierung), je ±0,1 mm. Nach dem Bau mit bekanntem Gewicht nachmessen (§2). |
| i | `BELT_RATIO` | 1,00 | – | Festlegung | **Direktantrieb**: Trommel auf dem Motorrotor, keine Übersetzung. Konstante bleibt für den Fall eines späteren Riemens. |
| J | — | **2,07** | kg·m² | ber | Aus der Geometrie, [09-messtheorie](09-messtheorie.md) §9.1. Der frühere Wert 2,3 galt der Ø-955-Trommel. Geht **nicht** ins Ergebnis ein (gemessen wird stationär, dω/dt = 0), sondern nur in Rampenkorrektur, Hebelarmauslegung und die Energiebilanz. Nachmessen per Auslaufversuch mit bekannter Zusatzmasse. |

## 2 Momentenmessung

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| — | `LOADCELL_CAPACITY_N` | 98,07 | N | DB | 10 kg |
| — | `LOADCELL_N_PER_COUNT` | **TBD** | N/count | TBD | Kalibrierung: Wiegeteil in Messlage, bekanntes Gewicht am Kalibrierpunkt anhängen, Rohwerte vorher/nachher mitteln, dividieren. Mindestens 3 Gewichte über den Bereich, Linearität prüfen. |
| — | `HX711_GAIN_PULSES` | 1 | – | DB | 1 = Kanal A Gain 128, 3 = Kanal B Gain 32 |
| — | `HX711_RATE_SPS` | 80 | 1/s | DB | RATE-Pin auf VCC. Bei 60 km/h ≈ 14 Samples pro Umdrehung. |
| — | `TARE_SAMPLES` | 200 | – | gesch | statische Tara, ≈ 2,5 s bei 80 SPS |

> Solange `LOADCELL_N_PER_COUNT` TBD ist, gibt die Firmware keine Crr-Werte aus, sondern
> nur Rohwerte. Das ist Absicht.

## 3 Drehzahlaufnehmer

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| k | `MARKS_PER_REV` | 8 | – | Festlegung | geschraubte Magnete auf dem Teilkreis der 8 Abstandsklötze, [04-hardware](04-hardware.md) §9. Mehr Marken bringen keine Genauigkeit, nur Latenz. |
| — | `MIN_PULSE_US` | 800 | µs | ber | Kürzester Abstand: 60 km/h, 8 Marken → 22,5 ms. Bei 100 km/h → 13,5 ms. 800 µs filtert sicher nur Störer. |
| — | `STALL_TIMEOUT_MS` | 2000 | ms | ber | Grenzfall ist nicht 10 km/h (135 ms bei 8 Marken), sondern der Hochlauf: bei 1 km/h sind es 1,35 s. 2000 ms bedeutet *langsamer als 0,68 km/h gilt als Stillstand*; die Rampe ist nach 1,4 s darüber. |
| — | `SLIP_TOLERANCE` | 0,05 | – | gesch | zulässige relative Abweichung n_Motor/i gegen n_Trommel |
| — | `MOTOR_PULSES_PER_REV` | 15 | – | TBD | Hallflanken einer Phase pro Motorumdrehung = Polpaarzahl. Ermittlung: Motor von Hand eine volle Umdrehung drehen und Flanken zählen. |

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
| kv | **14,0** | 1/min/V | gem | 589 1/min bei 42 V |
| kt | 0,68 | Nm/A | ber | 9,55 / kv |
| Masse | 2,26 | kg | Angebot | geht in die Eigenfrequenz des Wiegeteils ein |
| Hallsensoren | ja, 3 Stück auf einer Platine im Stator | – | Foto des geöffneten Motors | 3 Flachstecker (gelb/grün/blau) + weißer JST mit 6 Adern, Belegung in [04-hardware](04-hardware.md) §11 |
| Temperaturfühler | **TBD** | – | – | weiße Ader am Hall-Stecker, führt ins Wickelpaket — Typ ausmessen, A5 |
| **Mechanische Bremse** | **nein** | – | – | **kein stromloser Bremspfad**, siehe [07-sicherheit](07-sicherheit.md) §3 und A9 |
| Polzahl | **TBD** | – | TBD | von Hand eine Umdrehung drehen, Hallflanken zählen |
| Phasenwiderstand | **TBD** | Ω | TBD | Vierleitermessung zwischen je zwei Phasen |

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

## 5 Messbetrieb

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `V_MIN_KMH` | 10 | km/h | Festlegung | |
| `V_MAX_KMH` | 60 | km/h | Festlegung | 100 km/h erst nach Trommelfreigabe |
| `PLATEAUS_KMH` | 10,15,20,25,**28,8**,35,…,60 | km/h | Festlegung | 11 Stützstellen. 28,8 km/h ist die Referenzgeschwindigkeit der veröffentlichten Crr-Datenbank — zusammen mit 42,5 kg Last ein direkt vergleichbarer Punkt. |
| `RAMP_KMH_PER_S` | 0,5 | km/h/s | gesch | 2 min von 0 auf 60 |
| `SETTLE_MS` | 15000 | ms | gesch | Einschwingen nach Plateauwechsel; nach ersten Messungen prüfen |
| `AVG_REVS_PER_POINT` | 10 | – | gesch | bei 60 km/h ≈ 1,8 s, bei 10 km/h ≈ 10,8 s |
| `WARMUP_MS` | 1800000 | ms | Literatur | **30 min**, wie beim Referenzprüfstand. Crr sinkt mit der Reifentemperatur deutlich. |

## 6 Prüfling und Last

| Größe | Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|---|
| Standard-Auflast | `STANDARD_LOAD_N` | 416,9 | N | Literatur | 42,5 kg — der De-facto-Standard der veröffentlichten Crr-Datenbank. Nur Vorschlagswert; die tatsächliche Last wird über `l<wert>` eingegeben. |
| Auflast-Mechanik | — | Hebelwaage | – | Entwurf | Laufgewicht 10 kg, `N = m_Rad·g + W·b/a` mit a = 0,200 m, b = 0,30…0,90 m → 147…441 N. Entwurf und Abgleichvorschrift: [08-staender](08-staender.md) §5. Geht **linear** in Crr ein → auf 1 % genau nötig. |
| Auslegungs-Crr | `CRR_DESIGN_MAX` | **0,007** | – | Festlegung | Es werden schnelle Straßenreifen gemessen, keine MTB-Reifen. Langsamster vorgesehener Reifen ist der Continental Contact Urban mit 0,00552, darauf 27 % Luft. Siehe Tabelle unten. |
| Reifendruck | — | Eingabe | bar | – | Handeingabe, später BLE |
| Reifentemperatur | — | **TBD** | °C | TBD | Sensor noch nicht festgelegt |
| Motortemperatur | — | **TBD** | °C | TBD | 6. Ader am Hall-Stecker ausmessen (A5). Fühler hängt am Eingang `TMP` des Reglers, Auslesung über UART — [04-hardware](04-hardware.md) §11.4 |

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
> anheben, sondern auch `TORQUE_ARM_M` zurück auf 0,070 m setzen — sonst läuft die
> Wägezelle bei Crr 0,0127 samt Anfahrmoment über ihren Endwert. Beide Werte gehören
> zusammen.

## 7 Sicherheitsgrenzen

| Konstante | Wert | Einheit | Begründung |
|---|---|---|---|
| `MAX_FORCE_N` | 90 | N | 92 % vom Zellenendwert → Abschaltung vor der Sättigung |
| `MAX_DRUM_RPM` | **abgeleitet** → 341 bei U = 2,931 m | 1/min | `V_MAX_KMH / 3,6 / U · 60`. Nicht hart gesetzt: die Grenze bedeutet *60 km/h an der Trommeloberfläche* und muss dem Umfang folgen — der alte feste Wert 333 galt der Ø-955-Trommel und hätte an der gebauten Ø-933-Trommel schon bei 58,4 km/h ausgelöst, mitten im obersten Messplateau. Erhöhung von `V_MAX_KMH` erst nach Freigabe in [07-sicherheit](07-sicherheit.md) §5. |
| `STALL_TIMEOUT_MS` | 2000 | ms | siehe §3 |
| `FAULT_SETTLE_MS` | 500 | ms | Entprellung der Fehlerkriterien |

## 8 Status-LED

Die RGB-LED des DevKits ist eine adressierbare **WS2812**, kein einfacher Ausgang; sie
wird über `neopixelWrite()` angesteuert, das intern RMT benutzt.

| Konstante | Wert | Einheit | Herkunft | Anmerkung |
|---|---|---|---|---|
| `PIN_STATUS_LED` | 48 | – | Festlegung | **TBD**: auf DevKitC-1 v1.0 sitzt die LED an GPIO 48, auf v1.1 an GPIO 38. Der Arduino-Core nimmt für den S3 ebenfalls 48 (`RGB_BUILTIN`). Bleibt die LED dunkel, ist es das andere Board — dann hier auf 38 ändern, sonst nichts. |
| `BOOT_RAINBOW_MS` | 2500 | ms | Festlegung | Dauer des Regenbogens beim Booten. Ein voller Farbkreis, danach bleibt die LED **aus**. |
| `BOOT_RAINBOW_STEP_MS` | 12 | ms | Festlegung | ≈ 200 Schritte über die Dauer — flüssig, ohne den Bus zu belasten. |
| `STATUS_LED_DIV` | 6 | – | Festlegung | Helligkeitsteiler. Die LED blendet bei voller Aussteuerung auf dem Tisch. |

Beide Pins nacheinander zu bespielen geht **nicht**: `neopixelWrite()` initialisiert RMT
einmalig für den Pin des ersten Aufrufs und wechselt danach nicht mehr. Deshalb ein
Pin, und im Zweifel umstellen statt beides zu versuchen.
