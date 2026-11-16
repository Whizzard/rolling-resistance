# Sicherheit

Privates Gerät, offener Aufbau **ohne Schutzhaube**, nur vom Erbauer und nur unter
Aufsicht betrieben. Keine Gefährdungsbeurteilung nach Arbeitsschutzrecht.

## 1 Energie der Trommel

J = 2,07 kg·m² (aus der Geometrie, durch Auslaufversuch zu bestätigen), U = 2,931 m.

| v an der Oberfläche | ω | E = ½Jω² | |
|---|---|---|---|
| 30 km/h | 17,9 rad/s | 330 J | 0,09 Wh |
| **60 km/h** | **35,7 rad/s** | **1321 J** | **0,37 Wh** |
| 100 km/h | 59,5 rad/s | 3670 J | 1,02 Wh |

1321 J entsprechen 17 kg mit 45 km/h. Umfangsgeschwindigkeit bei 60 km/h: 16,7 m/s.

## 2 Gefährdungen und Maßnahmen

Ohne Haube tragen Drehzahlgrenze, Wuchtgüte und Sichtprüfung die Last.

| Gefährdung | Maßnahme |
|---|---|
| Berstende oder sich lösende Trommel | Drehzahlgrenze §5, Wuchten §6, Sichtprüfung vor jedem Lauf. Nicht in der Rotationsebene aufhalten. |
| Gelöster Markenmagnet | 2 g bei 16,7 m/s = 0,28 J. Schutzbrille. Magnetschrauben vor jedem Lauf prüfen. |
| Einzug zwischen Trommel und Prüflaufrad | Keine losen Kleidungsstücke, keine langen Haare offen, keine Handschuhe. Nicht in den Spalt greifen. |
| Berührung der Trommeloberfläche im Lauf | Definierter Standplatz seitlich, außerhalb der Rotationsebene. |
| **Kein Bremspfad** | §3. Trommel lässt sich nur auslaufen lassen, 3 bis 6 Minuten. |
| Gelöstes Prüflaufrad | Formschlüssig gesichert, nicht nur geklemmt. |
| Elektrisch: 36–42 V | Sicherung im Leistungspfad, isoliertes Werkzeug, Netzteil abgedeckt. |
| Überhitzung von Motor oder Regler | Strombegrenzung im Regler, Lauf nicht unbeaufsichtigt. Temperaturfühler siehe A5. |
| Umkippen des Aufbaus | Rahmen am Boden verankert oder ausreichend schwer. |

## 3 Not-Aus und Bremse

Der Not-Aus trennt den Leistungspfad **in Hardware**. Ein Softwarebefehl zählt nicht.

- zwangsöffnender Pilzschalter in der Zuleitung zum Regler
- ohne Werkzeug erreichbar, auch im Zurückweichen
- Rückmeldung an den ESP32 nur zur Anzeige
- danach läuft die Trommel frei aus

**Gebremst wird über Kurzschluss der drei Phasen** — Schaltung in
[04-hardware](04-hardware.md) §9. Gemessen: **25 s** aus über 400 1/min. Zwei Pfade,
beide in dieselbe Richtung: Bus weg → gebremst, GPIO low → gebremst, Reset → gebremst.

**Eine Feststellbremse gibt es nicht.** Eine stehende Trommel ist nicht gesichert:

- Nach jedem Lauf warten, bis die Trommel **sichtbar** steht.
- Vor Arbeiten am Aufbau Blockierkeil zwischen Rahmen und Speiche.

Solange die Relais nicht verbaut sind, bleibt nur der freie Auslauf von 3 bis 6 Minuten.

**Rückspeisung.** Rekuperatives Bremsen treibt die Zwischenkreisspannung am Netzteil
bis zur Überspannungsabschaltung. Nötig wären Bremswiderstand mit Chopper (1,4 kJ
über 10 s = 140 W) oder eine Batterie als Puffer.

## 4 Automatische Abschaltkriterien (Firmware)

| Kriterium | Grenze | Reaktion |
|---|---|---|
| Keine Drehzahlpulse trotz Sollwert > 0 | `STALL_TIMEOUT_MS` = 2000 ms | FAULT, Antrieb aus |
| n_Motor gegen n_Trommel unplausibel | `SLIP_TOLERANCE` = 5 % | FAULT, Antrieb aus |
| Kraft an der Wägezelle | `MAX_FORCE_N` = 45 N | FAULT, Antrieb aus |
| Wägezelle gesättigt oder HX711 ohne Antwort | — | FAULT, Antrieb aus |
| Trommeldrehzahl über Grenze | `MAX_DRUM_RPM` = 341 1/min | FAULT, Antrieb aus |
| Motortemperatur | **TBD**, siehe A5 | FAULT, Antrieb aus |
| **Bedienverbindung abgerissen** | `LINK_TIMEOUT_MS` = 2000 ms | FAULT, Antrieb aus |

FAULT ist rastend: Quittierung nur im Stillstand über die Konsole oder das Bedienfeld.

**Zur Bedienverbindung.** Solange ein Lauf fährt, muss sich der Bedienrechner melden —
reißt die Verbindung ab, bricht der Lauf ab und die Trommel läuft aus. Drei Festlegungen
dazu:

- Maßgeblich ist ein **Herzschlag**, nicht die TCP-Verbindung: ein weggelegtes oder
  eingeschlafenes Telefon merkt TCP erst nach Minuten. Jede Abfrage von `/api/state`
  zählt als Lebenszeichen, die Oberfläche fragt 5× je Sekunde.
- Das Kriterium ist **erst scharf, nachdem ein Client da war** (`webui::clientSeen()`).
  Ohne Bedienfeld lässt sich der Stand weiterhin allein über die serielle Konsole
  fahren — sonst wäre jeder Konsolenlauf sofort ein Fehlerfall.
- Es ist **rastend** wie jedes andere Kriterium. Ein Lauf, der nach einem
  Verbindungsabriss von selbst weiterfährt, sobald das WLAN wiederkommt, wäre die
  schlechtere Eigenschaft.

Die Status-LED zeigt denselben Zustand: grün stetig = Verbindung steht, rot blinkend =
es wird gewartet.

Beim Direktantrieb gibt es keinen Schlupf; der Drehzahlvergleich ist eine
Plausibilitätsprüfung der beiden Aufnehmer.

## 5 Freigabe höherer Drehzahlen

`MAX_DRUM_RPM` = 341 1/min entspricht 60 km/h bei U = 2,931 m. Der Wert ist in
`config.h` aus `V_MAX_KMH` und `DRUM_CIRCUMFERENCE_M` **abgeleitet**, nicht fest
eingetragen — eine Änderung des Umfangs verschiebt damit eine Sicherheitsgrenze und
ist hier nachzuziehen.

Eine Erhöhung von `V_MAX_KMH` setzt voraus:

1. Trommel gewuchtet, Restunwucht dokumentiert (§6)
2. Nabenanbindung auf Moment und Fliehkräfte gerechnet
3. Rundlauf gemessen
4. Bremspfad vorhanden (§3)
5. Schrittweiser Hochlauf in 5-km/h-Stufen, Lauf und Geräusch beobachtet, Abstand
6. Eintrag hier mit Datum und Begründung

Ohne diese Punkte bleibt es bei 60 km/h.

## 6 Trommel auswuchten

L/D = 100/933 = 0,11, also genügt **Einebenen-Wuchten**.

**Ziel:** ISO 21940 Gütestufe G 6.3 bei m = 17 kg und ω = 35,7 rad/s.

```
e_zul = G / ω     = 6,3 / 35,7       = 0,176 mm
U_zul = e_zul · m = 0,176 mm · 17 kg = 3000 g·mm
```

Am Radius 466 mm sind das **6,4 g am Umfang**.

**Ablauf.** Trommel vom Motor trennen, Welle auf zwei waagerechte Schneiden oder
Lagerpaare legen. Schwere Stelle dreht nach unten. Ausgleich über Schrauben mit
Scheiben in die Multiplexscheibe, Radius notieren — **nicht auf den Teilkreis der
Markenmagnete**, dort stört Stahl die Hall-Geber.

**Auflösung nachweisen.** Dass die Trommel in jeder Stellung steht, heißt nur, dass
die Reibung größer ist als die Restunwucht. Prüfgewicht 5 g am Umfang ankleben:
dreht sie sich, liegt die Auflösung unter 5 g. Bewegt sie sich nicht, ist die
Reibung zu hoch und das Ergebnis wertlos.

Kleinste erkennbare Unwucht am Umfang, `m_min = T_reib / (g · r)`:

| Lagerung | T_reib | m_min |
|---|---|---|
| mit Nabenmotor (Rastmoment) | 0,15…0,30 Nm | 33…66 g |
| gedichtete Rillenkugellager | ≈ 0,02 Nm | 4,4 g |
| offene Lager oder Schneiden | ≈ 0,002 Nm | 0,44 g |

Mit angeflanschtem Motor ist der Test also wertlos.

**Gegenprobe im Lauf.** Die 1. Ordnung im umdrehungssynchron gemittelten
Momentenverlauf ist ein direktes Maß für die Restunwucht. Beim Hochlauf nach §5
mitschreiben. Für die Messung selbst ist Unwucht unkritisch — 5,7 Hz bei 60 km/h
liegen weit unter der Eigenfrequenz des Wiegeteils, und die Mittelung löscht alle
Vielfachen der Drehfrequenz.

## 7 Checkliste vor jedem Lauf

- [ ] Not-Aus erreichbar und geprüft
- [ ] Trommel sichtgeprüft: Risse, Nabe, lose Teile, Magnetschrauben fest
- [ ] Prüflaufrad gesichert
- [ ] Blockierkeil entfernt
- [ ] Nichts Loses im Prüfraum, keine losen Kleidungsstücke
- [ ] Schutzbrille
- [ ] Zeit für 3 bis 6 Minuten Auslauf eingeplant

## 8 Nach jedem Lauf

- [ ] Antrieb aus, Trommel **sichtbar** zum Stillstand gekommen
- [ ] Vor Arbeiten am Aufbau: Blockierkeil zwischen Rahmen und Speiche
