# Sicherheit

> Dieses Dokument beschreibt die Gefährdungen eines selbstgebauten Prüfstands und die
> vorgesehenen Schutzmaßnahmen. Es ersetzt keine Gefährdungsbeurteilung nach
> Arbeitsschutzrecht. Der Prüfstand ist ein privates Gerät und wird nur unter Aufsicht
> betrieben.

## 1 Die zentrale Zahl

Die Trommel ist ein Schwungrad. Mit J ≈ 2,3 kg·m² (geschätzt, **durch Auslaufversuch zu
bestätigen**):

| v an der Oberfläche | ω | gespeicherte Energie |
|---|---|---|
| 30 km/h | 17,5 rad/s | ≈ 350 J |
| 60 km/h | 34,9 rad/s | ≈ **1400 J** |
| 100 km/h | 58,2 rad/s | ≈ **3900 J** |

1400 J entsprechen ungefähr einem 1-kg-Körper mit 53 m/s. Ein sich lösendes Trommel-
segment wäre lebensgefährlich. Die Umfangsgeschwindigkeit beträgt bei 60 km/h 16,7 m/s.

## 2 Gefährdungen und Maßnahmen

| Gefährdung | Maßnahme |
|---|---|
| Berstende oder sich lösende Trommel | Schutzhaube aus Stahlblech oder Polycarbonat, besonders in der Rotationsebene. Sichtprüfung vor jedem Lauf. Festgelegte Höchstdrehzahl. |
| Einzug zwischen Trommel und Prüflaufrad | Haube geschlossen; keine losen Kleidungsstücke, keine langen Haare offen, keine Handschuhe. |
| Berührung der Trommeloberfläche im Lauf | Haube mit Verriegelung; niemals in den Prüfraum greifen, solange sich etwas dreht. |
| Langer Auslauf nach dem Abschalten | Gerechnet **13 Minuten** von 60 km/h bis Stillstand. Keine Handbremsung. Haube erst öffnen, wenn die Trommel steht. |
| Gelöstes Prüflaufrad | Laufrad formschlüssig gesichert, nicht nur geklemmt. |
| Reißender Zahnriemen | Riemen abgedeckt. |
| Elektrisch: 36–42 V, hohe Ströme | Sicherung im Leistungspfad. Nur isoliertes Werkzeug. Netzteil abdecken. |
| Überhitzung von Motor oder Regler | PTC im Motor auswerten, Strombegrenzung im Regler, Lauf nicht unbeaufsichtigt. |
| Umkippen des Aufbaus | Rahmen am Boden verankert oder ausreichend schwer. |

## 3 Not-Aus

**Der Not-Aus trennt den Leistungspfad in Hardware.** Ein Softwarebefehl allein zählt
nicht als Not-Aus:

- zwangsöffnender Pilzschalter in der Zuleitung zum Regler
- ohne Werkzeug erreichbar, auch wenn man von der Maschine zurückweicht
- Rückmeldung an den ESP32 nur zur **Anzeige**; die Trennung hängt nicht daran
- nach dem Auslösen läuft die Trommel frei aus — das ist beabsichtigt und dauert
  gerechnet rund 13 Minuten ([02-messkonzept](02-messkonzept.md) §4.3)

Zusätzlich, aber **nicht als Ersatz**: der Regler kann kontrolliert bremsen. Das ist der
normale Weg zum Anhalten und schont den Riemen.

**Mechanische Bremse.** Der gewählte Nabenmotor hat eine integrierte Trommelbremse. Die
ist über einen Bowdenzug zu bedienen und dient als Not- und Feststellbremse — der einzige
Bremspfad, der auch ohne Strom funktioniert. Zwei Bedingungen:

- Im Messbetrieb muss sie **vollständig gelöst** sein. Eine schleifende Bremse erzeugt
  ein temperaturabhängiges Schleppmoment, das die Leerlaufkennlinie zwischen Vor- und
  Nachlauf auseinanderlaufen lässt und damit die Messreihe wertlos macht.
- Beim Direktantrieb wirkt sie unmittelbar auf die Trommel, ohne Zwischenglied. Das ist
  der einzige Bremspfad, der auch bei Stromausfall funktioniert, und angesichts der
  13 Minuten freien Auslaufs der normale Weg zum Anhalten.

**Rückspeisung.** Rekuperatives Bremsen schiebt Energie in den Zwischenkreis zurück. An
einem Netzteil steigt dabei die Spannung bis zur Überspannungsabschaltung des Reglers.
Wer regenerativ bremsen will, braucht einen Bremswiderstand mit Chopper (1,4 kJ über 10 s
sind 140 W) oder eine Batterie als Puffer.

## 4 Automatische Abschaltkriterien (Firmware)

| Kriterium | Grenze | Reaktion |
|---|---|---|
| Keine Drehzahlpulse trotz Sollwert > 0 | `STALL_TIMEOUT_MS` = 2000 ms | FAULT, Antrieb aus |
| Riemenschlupf: n_Motor/i gegen n_Trommel | `SLIP_TOLERANCE` = 5 % | FAULT, Antrieb aus |
| Kraft an der Wägezelle | `MAX_FORCE_N` = 90 N | FAULT, Antrieb aus |
| Wägezelle gesättigt oder HX711 ohne Antwort | — | FAULT, Antrieb aus |
| Trommeldrehzahl über Grenze | `MAX_DRUM_RPM` = 333 1/min | FAULT, Antrieb aus |
| Motortemperatur (PTC) | Grenze **TBD**, Kennlinie unbekannt | FAULT, Antrieb aus |

FAULT ist rastend: Quittierung nur im Stillstand über die Konsole.

## 5 Freigabe höherer Drehzahlen

`MAX_DRUM_RPM` steht auf 333 1/min (60 km/h). Eine Erhöhung setzt voraus:

1. Trommel gewuchtet, Restunwucht dokumentiert
2. Nabenanbindung auf das Moment und die Fliehkräfte gerechnet
3. Rundlauf gemessen
4. Schrittweiser Hochlauf in 5-km/h-Stufen mit Beobachtung von Lauf und Geräusch,
   Haube geschlossen, Abstand halten
5. Eintrag hier im Dokument mit Datum und Begründung

Ohne diese Punkte bleibt es bei 60 km/h.

## 6 Checkliste vor jedem Lauf

- [ ] Haube geschlossen und verriegelt
- [ ] Not-Aus erreichbar und geprüft
- [ ] Trommel sichtgeprüft: Risse, Nabe, lose Teile
- [ ] Prüflaufrad gesichert
- [ ] Riemenspannung und Riemenabdeckung geprüft
- [ ] Nichts Loses im Prüfraum
- [ ] Zweite Person informiert oder anwesend
