# Parameter

Spiegelt `firmware/src/config.h` 1:1. Jede Änderung hier **und** dort.

Herkunft: `gem` = gemessen · `ber` = berechnet · `DB` = Datenblatt · `gesch` = geschätzt ·
`TBD` = noch zu ermitteln.

## 1 Geometrie

| Symbol | Konstante | Wert | Einheit | Herkunft | Ermittlung |
|---|---|---|---|---|---|
| U | `DRUM_CIRCUMFERENCE_M` | 3,000 | m | TBD | Maßband um die Trommel, 3 Messungen mitteln. 1 mm Fehler = 0,03 % — unkritisch. |
| r | `DRUM_RADIUS_M` | 0,4775 | m | ber | U / 2π |
| L | `TORQUE_ARM_M` | 0,070 | m | ber | So gewählt, dass der ungünstigste Fall samt Anfahren (4,20 Nm) bei 60 N liegt = 61 % vom Zellenendwert. Nach dem Bau mit bekanntem Gewicht nachmessen (§2). |
| i | `BELT_RATIO` | 1,00 | – | Festlegung | **Direktantrieb**: Trommel auf dem Motorrotor, keine Übersetzung. Konstante bleibt für den Fall eines späteren Riemens. |
| J | — | 2,3 | kg·m² | gesch | **Auslaufversuch**, siehe [02-messkonzept](02-messkonzept.md) §5. Nur für die optionale Rampenkorrektur nötig. |

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
| — | `STALL_TIMEOUT_MS` | 2000 | ms | ber | > 1,08 s (langsamster Puls bei 10 km/h, 1 Marke) mit Reserve |
| — | `SLIP_TOLERANCE` | 0,05 | – | gesch | zulässige relative Abweichung n_Motor/i gegen n_Trommel |
| — | `MOTOR_PULSES_PER_REV` | 15 | – | TBD | Hallflanken einer Phase pro Motorumdrehung = Polpaarzahl. Ermittlung: Motor von Hand eine volle Umdrehung drehen und Flanken zählen. |

## 4 Antrieb

Nabenmotor aus einem Bird-One-E-Scooter. Angaben vom Typenschild.

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
| Moment an der Trommel | 9,94 | Nm | ber | 5,52 · i, bei i = 1,80 |
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
T_dauer = P_nenn / (2π · n_nenn / 60)   ≥ 3,53 Nm   (Auslegungsfall, §6)
kv      = n_nenn / U_nenn               = 14…20 1/min/V
```

Das kv-Fenster ergibt sich aus der Trommeldrehzahl: 333 1/min bei 60 km/h, geteilt durch
36 V Busspannung, sind 9,25 1/min/V als absolute Untergrenze — praktisch 14 bis 20 mit
Spannungsreserve für den Strom. Angebote mit 100 kv oder mehr sind Motoren zum
Herunterübersetzen, kein Direktantrieb.

Daraus die Mindestleistung:

| n_nenn bei 36 V | ω | Minimum (3,53 Nm) | mit 50 % Reserve |
|---|---|---|---|
| 400 1/min | 41,9 rad/s | 148 W | 222 W |
| 500 1/min | 52,4 rad/s | 185 W | 277 W |
| **605 1/min** (verbauter Motor) | 63,4 rad/s | **224 W** | **336 W** |
| 700 1/min | 73,3 rad/s | 259 W | 388 W |

Praktisch: **250 W Minimum, 350 W richtig.** Unter 400 1/min Nenndrehzahl wird es
unabhängig von der Leistung eng, weil dann kaum Spannungsreserve über den für 60 km/h
nötigen 333 1/min bleibt.

Der Grund für den scheinbar großen Abstand zwischen den 123 W mechanischem Bedarf (§4 in
[02-messkonzept](02-messkonzept.md)) und den geforderten 224 W Nennleistung: Der Prüfstand
ist **momentenbegrenzt, nicht leistungsbegrenzt**. Bei 333 1/min läuft der Motor mit 55 %
seiner Nenndrehzahl und kann höchstens 55 % seiner Nennleistung abgeben.

> Typenschildangaben billiger Motoren sind optimistisch; die thermische Dauerleistung
> liegt häufig darunter. Die Tabelle gilt für 42,5 kg Auflast und Crr 0,015. Nur
> Rennreifen (Crr ≤ 0,008) kämen mit 2,06 Nm und damit 200 W aus; 100 kg Auflast
> verdoppeln alles.

### Geschwindigkeitsreserve über die Busspannung

Die Übersetzung ist auf maximales Moment ausgelegt und begrenzt die Trommel bei 36 V auf
rund 60 km/h. Mehr kommt über die Spannung, nicht über eine andere Übersetzung — das
Moment bleibt dabei erhalten:

| Busspannung | n_Motor | n_Trommel | v |
|---|---|---|---|
| 36 V | 605 1/min | 336 1/min | 60,5 km/h |
| 48 V | 807 1/min | 448 1/min | 80,7 km/h |
| 60 V | 1008 1/min | 560 1/min | 100,8 km/h |

Eine Erhöhung setzt die Freigabe der Trommeldrehzahl voraus
([07-sicherheit](07-sicherheit.md) §5).

### Alternative: Hoverboard-Nabenmotor 6,5″

TY36VH25B350W, 36 V / 350 W, 2,26 kg. Nenndrehzahl ≈ 480 1/min (**abgeleitet** aus der
üblichen Hoverboard-Endgeschwindigkeit, steht nicht auf dem Typenschild — die 605 1/min
des DAAO-Motors sind dagegen aufgedruckt).

| | Hoverboard 6,5″ | Scooter 9,5″ (DAAO) |
|---|---|---|
| Dauermoment `P/ω_nenn` | **6,96 Nm** | 5,52 Nm |
| Hallsensoren | ja — 3 dicke Phasen (gelb/grün/blau) + weißer JST mit 5 dünnen Adern | ja |
| n_nenn → Trommel max | 480 1/min → 86 km/h | 605 1/min → 109 km/h |
| Temperaturfühler | nein (NTC nachrüstbar) | PTC |
| Mechanische Bremse | **nein** | Trommelbremse |
| Spitzenlast | unbekannt | 800 W |
| Masse | 2,26 kg | schwerer, **TBD** |
| Lochkreis für den Trommeladapter | 6,5″ | 9,5″, steifer |

Beide erfüllen die Anforderung von 3,53 Nm mit Reserve. Die frühere Einschätzung, die
„6-adrige Kabelpeitsche" mache Hallsensoren zweifelhaft, hat sich an den Angebotsfotos
erledigt: es sind 8 Leiter, die Beschreibung zählte nur den Hall-Strang.

Übrig bleibt als Unterschied die **mechanische Bremse**, und die ist bei 13 Minuten
freiem Auslauf (siehe [02-messkonzept](02-messkonzept.md) §4.3) kein Komfortmerkmal.
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
| Auslegungs-Crr | `CRR_DESIGN_MAX` | 0,015 | – | Literatur | siehe Tabelle unten |
| Reifendruck | — | Eingabe | bar | – | Handeingabe, später BLE |
| Reifentemperatur | — | **TBD** | °C | TBD | Sensor noch nicht festgelegt |
| Motortemperatur | — | PTC im Motor | °C | TBD | Kennlinie des PTC unbekannt, siehe §4 |

### Gemessene Crr-Werte als Auslegungsgrundlage

Einheitlich bei 42,5 kg und 28,8 km/h erhoben
([Quelle](https://www.bicyclerollingresistance.com/the-test)):

| Reifen | Leistung | Crr | Moment an unserer Trommel |
|---|---|---|---|
| Vittoria Corsa Speed (bester Rennreifen) | ~11 W | ~0,0025 | 0,50 Nm |
| typischer guter Rennreifen | 13–17 W | 0,0035–0,0040 | 0,70–0,80 Nm |
| [Schwalbe Marathon Plus](https://www.bicyclerollingresistance.com/tour-reviews/schwalbe-marathon-plus-2015) | 25,5 W | 0,00764 | 1,52 Nm |
| [Schwalbe Hans Dampf TrailStar, langsamster MTB](https://www.bicyclerollingresistance.com/mtb-reviews) | 42,2 W | ≈ 0,0127 | 2,53 Nm |
| **Auslegungsgrenze** | | **0,015** | **2,99 Nm** |

Der MTB-Crr ist selbst gerechnet, weil die Quelle dort nur Watt ausweist:
`42,2 W / 8,0 m/s / (42,5 · 9,81 N) = 0,0127`.

> Achtung bei höheren Lasten: Bei 100 kg Auflast (Lastenrad, E-Bike) ergibt Crr 0,0127
> allein für den Reifen 5,95 Nm und überschreitet damit das Dauermoment des Motors.
> Die Auslegung gilt für 42,5–50 kg.

## 7 Sicherheitsgrenzen

| Konstante | Wert | Einheit | Begründung |
|---|---|---|---|
| `MAX_FORCE_N` | 90 | N | 92 % vom Zellenendwert → Abschaltung vor der Sättigung |
| `MAX_DRUM_RPM` | 333 | 1/min | entspricht 60 km/h; Erhöhung erst nach Freigabe in [07-sicherheit](07-sicherheit.md) |
| `STALL_TIMEOUT_MS` | 2000 | ms | siehe §3 |
| `FAULT_SETTLE_MS` | 500 | ms | Entprellung der Fehlerkriterien |
