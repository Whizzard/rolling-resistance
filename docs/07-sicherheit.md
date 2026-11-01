# Sicherheit

> Dieses Dokument beschreibt die Gefährdungen eines selbstgebauten Prüfstands und die
> vorgesehenen Schutzmaßnahmen. Es ersetzt keine Gefährdungsbeurteilung nach
> Arbeitsschutzrecht. Der Prüfstand ist ein privates Gerät und wird nur unter Aufsicht
> betrieben.

## 1 Die zentrale Zahl

Die Trommel ist ein Schwungrad. Mit J = 2,07 kg·m² (aus der Geometrie, **durch
Auslaufversuch zu bestätigen**), U = 2,931 m:

| v an der Oberfläche | ω | E = ½Jω² | |
|---|---|---|---|
| 30 km/h | 17,9 rad/s | 330 J | 0,09 Wh |
| **60 km/h** | **35,7 rad/s** | **1321 J** | **0,37 Wh** |
| 100 km/h | 59,5 rad/s | 3670 J | 1,02 Wh |

Als Energie ist das wenig — eine AA-Zelle hat das Siebenfache. Die Gefahr liegt nicht
in der Menge, sondern darin, wie schnell sie frei wird: 1321 J sind die ganze Trommel
(17 kg) mit **45 km/h** geradeaus, oder dieselben 17 kg aus **7,9 m Höhe** fallend.
Die Umfangsgeschwindigkeit beträgt bei 60 km/h 16,7 m/s; ein sich lösendes
Trommelsegment wäre lebensgefährlich.

## 2 Gefährdungen und Maßnahmen

| Gefährdung | Maßnahme |
|---|---|
| Berstende oder sich lösende Trommel | Schutzhaube aus Stahlblech oder Polycarbonat, besonders in der Rotationsebene. Sichtprüfung vor jedem Lauf. Festgelegte Höchstdrehzahl. |
| Einzug zwischen Trommel und Prüflaufrad | Haube geschlossen; keine losen Kleidungsstücke, keine langen Haare offen, keine Handschuhe. |
| Berührung der Trommeloberfläche im Lauf | Haube mit Verriegelung; niemals in den Prüfraum greifen, solange sich etwas dreht. |
| Langer Auslauf nach dem Abschalten | **3 bis 6 Minuten** von 60 km/h bis Stillstand, allein durch das Rastmoment des Motors ([09-messtheorie](09-messtheorie.md) §8.9). Keine Handbremsung. Haube erst öffnen, wenn die Trommel steht. |
| **Kein Bremspfad vorhanden** | **Offen, siehe §3.** Der verbaute Hoverboard-Nabenmotor hat keine mechanische Bremse, und A8 (Kurzschlussschütz) ist zurückgestellt. Derzeit lässt sich die Trommel nur auslaufen lassen. |
| Gelöstes Prüflaufrad | Laufrad formschlüssig gesichert, nicht nur geklemmt. |
| Elektrisch: 36–42 V, hohe Ströme | Sicherung im Leistungspfad. Nur isoliertes Werkzeug. Netzteil abdecken. |
| Überhitzung von Motor oder Regler | Strombegrenzung im Regler, Lauf nicht unbeaufsichtigt. **Der verbaute Motor hat keinen Temperaturfühler** — NTC auf der Statorachse nachrüsten (A5). Im Auslegungsfall werden 67 W von 350 W gebraucht, das Risiko ist gering, aber unüberwacht. |
| Umkippen des Aufbaus | Rahmen am Boden verankert oder ausreichend schwer. |

## 3 Not-Aus

**Der Not-Aus trennt den Leistungspfad in Hardware.** Ein Softwarebefehl allein zählt
nicht als Not-Aus:

- zwangsöffnender Pilzschalter in der Zuleitung zum Regler
- ohne Werkzeug erreichbar, auch wenn man von der Maschine zurückweicht
- Rückmeldung an den ESP32 nur zur **Anzeige**; die Trennung hängt nicht daran
- nach dem Auslösen läuft die Trommel frei aus — das ist beabsichtigt und dauert
  **3 bis 6 Minuten** ([09-messtheorie](09-messtheorie.md) §8.9)

Zusätzlich, aber **nicht als Ersatz**: der Regler kann kontrolliert bremsen. Das ist der
normale Weg zum Anhalten.

### Es gibt derzeit keinen stromlosen Bremspfad

> **Offener Punkt.** Der verbaute Motor ist ein **Hoverboard-Antriebsrad 6,5″,
> 36 V / 350 W** — der hat **keine** mechanische Bremse. Die frühere Annahme einer
> integrierten Trommelbremse stammte vom Scooter-Motor, der nicht verbaut wurde.
> Gleichzeitig ist A8 (Kurzschlussschütz) zurückgestellt.
>
> Damit bleibt nach einem Not-Aus **nur der freie Auslauf von 3 bis 6 Minuten**. Es
> gibt keine Möglichkeit, die Trommel schneller oder im stromlosen Zustand
> anzuhalten, und keine Feststellbremse für Arbeiten am Aufbau.

Solange das so ist, gilt:

- Die Haube bleibt nach jedem Lauf zu, bis die Trommel **sichtbar** steht. Nicht
  nach Gefühl, nicht nach Uhr.
- Vor Arbeiten an Trommel oder Laufrad die Trommel **mechanisch blockieren**, etwa
  mit einem Holzkeil zwischen Rahmen und Speiche. Eine stehende Trommel ist nicht
  gesichert.
- Der Not-Aus trennt den Antrieb, er hält nichts an.

Nachzurüsten ist eins von beidem: **Bandbremse oder Bremssattel auf einer Scheibe
an der Trommelwelle** (A9), oder das Kurzschlussschütz aus A8. Erst dann greift die
Freigabe höherer Drehzahlen nach §5.

**Rückspeisung.** Rekuperatives Bremsen schiebt Energie in den Zwischenkreis zurück. An
einem Netzteil steigt dabei die Spannung bis zur Überspannungsabschaltung des Reglers.
Wer regenerativ bremsen will, braucht einen Bremswiderstand mit Chopper (1,4 kJ über 10 s
sind 140 W) oder eine Batterie als Puffer.

## 4 Automatische Abschaltkriterien (Firmware)

| Kriterium | Grenze | Reaktion |
|---|---|---|
| Keine Drehzahlpulse trotz Sollwert > 0 | `STALL_TIMEOUT_MS` = 2000 ms | FAULT, Antrieb aus |
| Aufnehmer unplausibel: n_Motor gegen n_Trommel | `SLIP_TOLERANCE` = 5 % | FAULT, Antrieb aus |
| Kraft an der Wägezelle | `MAX_FORCE_N` = 90 N | FAULT, Antrieb aus |
| Wägezelle gesättigt oder HX711 ohne Antwort | — | FAULT, Antrieb aus |
| Trommeldrehzahl über Grenze | `MAX_DRUM_RPM` = **341** 1/min | FAULT, Antrieb aus |
| Motortemperatur | **entfällt vorerst** — der verbaute Motor hat keinen Fühler (A5) | – |

FAULT ist rastend: Quittierung nur im Stillstand über die Konsole.

Beim Direktantrieb kann es keinen Riemenschlupf geben — Motor und Trommel sitzen auf
derselben Welle. Der Vergleich bleibt trotzdem sinnvoll, aber als
**Plausibilitätsprüfung der beiden Aufnehmer**: weicht n_Motor von n_Trommel ab, ist
ein Geber ausgefallen.

## 5 Freigabe höherer Drehzahlen

`MAX_DRUM_RPM` entspricht 341 1/min (60 km/h an der Trommeloberfläche, U = 2,931 m).

Die Grenze ist in `config.h` **nicht fest eingetragen**, sondern aus `V_MAX_KMH` und
`DRUM_CIRCUMFERENCE_M` abgeleitet. Das ist Absicht: der frühere feste Wert 333 galt der
geplanten Ø-955-Trommel und hätte an der gebauten Ø-933-Trommel schon bei 58,4 km/h
ausgelöst. Wer den Umfang nachmisst, bekommt die passende Grenze automatisch.

**Für dieses Dokument heißt das:** Eine Änderung von `DRUM_CIRCUMFERENCE_M` verschiebt
eine Sicherheitsgrenze. Sie ist hier genauso nachzuziehen wie eine Änderung von
`V_MAX_KMH` selbst.

Eine Erhöhung von `V_MAX_KMH` setzt voraus:

1. Trommel gewuchtet, Restunwucht dokumentiert
2. Nabenanbindung auf das Moment und die Fliehkräfte gerechnet
3. Rundlauf gemessen
4. Schrittweiser Hochlauf in 5-km/h-Stufen mit Beobachtung von Lauf und Geräusch,
   Haube geschlossen, Abstand halten
5. Eintrag hier im Dokument mit Datum und Begründung

Ohne diese Punkte bleibt es bei 60 km/h.

## 5a Trommel auswuchten

Breite 100 mm bei 933 mm Durchmesser, also L/D = 0,11. Unter 0,2 genügt
**Einebenen-Wuchten** (statisch) — die beiden Korrekturebenen fallen praktisch
zusammen.

### Zulässige Restunwucht

Nach ISO 21940 mit Gütestufe **G 6.3**, m ≈ 17 kg, ω = 35,7 rad/s bei 60 km/h:

```
e_zul = G / ω     = 6,3 / 35,7      = 0,176 mm
U_zul = e_zul · m = 0,176 mm · 17 kg = 3000 g·mm
```

Am Radius 466 mm sind das **6,4 g am Umfang**. Das ist das Ziel.

### Der Ausrolltest taugt nur ohne Motor

Die Trommel bleibt stehen, sobald das Schwerkraftmoment der Unwucht das
Reibmoment unterschreitet: `m_min = T_reib / (g · r)`.

| Lagerung | T_reib | kleinste erkennbare Unwucht am Umfang |
|---|---|---|
| **mit Nabenmotor, Rastmoment 0,15 Nm** | 0,15 Nm | **33 g** |
| mit stark rastendem Stator | 0,30 Nm | 66 g |
| gedichtete Rillenkugellager, ohne Motor | ≈ 0,02 Nm | 4,4 g |
| offene Lager oder Schneiden | ≈ 0,002 Nm | 0,44 g |

**Mit angeflanschtem Motor ist der Test wertlos.** Das Rastmoment hält die Trommel
in jeder beliebigen Stellung fest — sie besteht den Test, egal wie unwuchtig sie
ist, und zwar um Faktor 5 bis 10 unter der nötigen Auflösung.

Also: Trommel vom Motor trennen und die Welle auf zwei waagerecht ausgerichtete
Schneiden oder zwei Lagerpaare legen.

### Eigene Auflösung nachweisen

Wenn die Trommel in jeder Stellung steht, ist damit **nicht** bewiesen, dass sie
gewuchtet ist — nur, dass die Reibung größer ist als die Restunwucht. Deshalb
gegenprüfen: **ein bekanntes Prüfgewicht von 5 g am Umfang ankleben.** Dreht sich
die Trommel daraufhin, liegt die Auflösung unter 5 g und das Ergebnis trägt.
Bewegt sie sich nicht, ist die Reibung zu hoch und das Wuchtergebnis wertlos.

Ausgleich über Schrauben mit Scheiben in die Multiplexscheibe, Radius notieren.
**Nicht auf den Teilkreis der Markenmagnete** — Stahl neben den Hall-Gebern.

### Gegenprobe im Lauf

Die Wägezelle misst die Restunwucht selbst mit: im umdrehungssynchron gemittelten
Momentenverlauf ist die **1. Ordnung** ein direktes Maß dafür. Beim stufenweisen
Hochlauf (§5) mitschreiben.

Für die Messung selbst ist Unwucht unkritisch: die 1. Ordnung liegt bei 60 km/h bei
5,7 Hz, weit unter der Eigenfrequenz des Wiegeteils von 26…44 Hz
([09-messtheorie](09-messtheorie.md) §3.5), und die umdrehungssynchrone Mittelung
löscht alle Vielfachen der Drehfrequenz exakt. Das Wuchten dient der **Sicherheit
und den Lagern**, nicht der Messgenauigkeit.

## 6 Checkliste vor jedem Lauf

- [ ] Haube geschlossen und verriegelt
- [ ] Not-Aus erreichbar und geprüft
- [ ] Trommel sichtgeprüft: Risse, Nabe, lose Teile, Markenmagnete fest
- [ ] Prüflaufrad gesichert
- [ ] Blockierkeil entfernt
- [ ] Nichts Loses im Prüfraum
- [ ] Zweite Person informiert oder anwesend
- [ ] **Zeit für 3 bis 6 Minuten Auslauf eingeplant** — es gibt keine Bremse

## 7 Nach jedem Lauf

- [ ] Antrieb aus, Trommel **sichtbar** zum Stillstand gekommen
- [ ] Erst dann Haube öffnen
- [ ] Vor Arbeiten am Aufbau: Blockierkeil zwischen Rahmen und Speiche
