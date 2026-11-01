# Anforderungen

Legende: **M** = muss, **S** = soll, **K** = kann (später).

## 1 Funktional

| # | Prio | Anforderung |
|---|---|---|
| F1 | M | Trommel (U = 3,000 m) wird motorisch auf eine einstellbare Solldrehzahl geregelt. |
| F2 | M | Geschwindigkeitsbereich 10–60 km/h an der Trommeloberfläche, geregelt auf ±0,5 km/h. Eine Stützstelle liegt auf 28,8 km/h (Referenz der publizierten Crr-Datenbank). |
| F3 | M | Ein Laufrad mit Prüfreifen wird mit definierter, bekannter Auflast gegen die Trommel gepresst. |
| F4 | M | Das Reaktionsmoment des Antriebs wird kontinuierlich gemessen. |
| F5 | M | Aus Moment und Geometrie werden Rollwiderstandskraft, Verlustleistung und Crr berechnet. |
| F6 | M | Messung läuft auf **Geschwindigkeits-Plateaus** mit Haltezeit, nicht während der Rampe (Begründung: [09-messtheorie](09-messtheorie.md), Fallstrick 1). |
| F7 | M | Leerlaufkennlinie P_leer(v) bei abgehobenem Laufrad wird aufgenommen und abgezogen. |
| F8 | M | Alle Messpunkte werden als CSV protokolliert (seriell; SD optional). |
| F9 | S | Anzeige von v, P, Crr, Zustand auf 4×20-Zeichen-LCD. |
| F10 | S | Auflast wird eingegeben; später gemessen. |
| F11 | K | Reifenluftdruck wird per BLE-Sensor gelesen; bis dahin Handeingabe. |
| F12 | K | Reifen- und Umgebungstemperatur werden mitgeloggt. |

## 2 Messtechnisch

| # | Prio | Anforderung |
|---|---|---|
| M1 | M | Auflösung so, dass ΔCrr = 0,0005 zwischen zwei Reifen sicher unterscheidbar ist (0,21 N Tangentialkraft bei 42,5 kg). Schärfer noch: die Geschwindigkeitsabhängigkeit beträgt nur 11,6 % zwischen 14 und 36 km/h, was 1–2 % Genauigkeit je Messpunkt verlangt. |
| M2 | M | Mittelung immer über eine **ganze Zahl von Trommelumdrehungen**, getriggert auf die Indexmarke. |
| M3 | M | Zwei getrennte Nullpunkte: statischer Zellen-Nullpunkt im Stillstand **und** geschwindigkeitsabhängige Leerlaufkennlinie. |
| M4 | M | Leerlaufkennlinie vor **und** nach jeder Messreihe; Auswertung interpoliert zwischen beiden (Lagerreibung ändert sich mit der Temperatur). |
| M5 | M | Kein Element mit Haftreibung oder Hysterese im Momentenpfad. |
| M6 | M | **30 Minuten Warmlauf** vor der ersten Messung, wie beim kommerziellen Referenzprüfstand; ohne ihn sind Vergleiche wertlos. |
| M7 | S | Winkelaufgelöste Kraftauswertung zur Diagnose von Trommelnaht und Unrundheit. |
| M8 | S | A/B/A-Protokoll: Referenzreifen am Anfang und am Ende jeder Sitzung. |

## 3 Mechanik

Diese vier sind später teuer nachzurüsten und gehören in die erste Konstruktion:

1. **Laufrad abhebbar ohne Geometrieänderung** (Anschlag, Nocken oder Exzenter).
   Sonst ist die Leerlaufkennlinie nicht unter identischen Bedingungen messbar.
2. **Drehpunkt des Laufradarms so legen, dass die Tangentialkraft die Auflast nicht
   verändert** — Drehpunkt in Richtung der Reibkraft bzw. auf Latschhöhe.
3. **Momentenabgriff ohne zusätzliches Lager.** Der Stator trägt sich auf den
   motoreigenen Lagern ab und ist nur über den Hebelarm gehalten; damit kürzt sich die
   Rotor-Stator-Reibung exakt heraus ([08-staender](08-staender.md) §7.2). Zuleitungen
   treten axial auf der Achse aus. Kalibrierbohrung bei 80 mm, Flagge der Lichtschranke
   und **beidseitige Endanschläge bei 200 mm** — ohne sie zerstört schon das
   Spitzenmoment des Motors die Zelle. Platz für den Wirbelstromdämpfer einplanen.
4. **Motor-Hallsignale an den ESP32 führen.** Beim Direktantrieb sind Motor- und
   Trommeldrehzahl identisch; der Vergleich mit den Trommelmarken ist dann eine
   Plausibilitätsprüfung, die einen ausgefallenen Aufnehmer erkennt. Kommt doch ein
   Riemen dazwischen, wird daraus die Schlupfüberwachung.

Weiter:

| # | Prio | Anforderung |
|---|---|---|
| X1 | M | **Direktantrieb**: Trommel auf dem Motorrotor, Statorachse **beidseitig** in Stehlagern auf dem Wiegerahmen, Trommel symmetrisch zur Motorebene (kein Biegemoment auf die Motorlager). Lässt sich die Achse nicht beidseitig lagern, bekommt die Trommel eigene Lager und einen Zahnriemen. |
| X2 | M | Trommel gewuchtet; zulässige Höchstdrehzahl schriftlich festgelegt ([07-sicherheit](07-sicherheit.md)). |
| X3 | M | Wägezelle **nur auf Zug** angebunden (Schaltzug, 12 N vorgespannt). Das Vorzeichen kehrt beim Bremsen und im Auslauf um — dann wird der Zug schlaff und die Last geht in den Anschlag, nicht in die Zelle. Begründung und Maße: [08-staender](08-staender.md) §7.3. |
| X4 | S | Nur falls doch ein Riemen nötig wird: HTD/GT-Profil, nicht T-Profil, wegen der Anregung des Wiegeteils. |
| X5 | M | Eine nachgerüstete Bremse muss im Messbetrieb vollständig gelöst sein — ein Schleppmoment driftet mit der Temperatur und zerstört die Leerlaufkennlinie. Der verbaute Hoverboard-Motor hat keine eingebaute Bremse. |

## 4 Betrieb und Sicherheit

| # | Prio | Anforderung |
|---|---|---|
| B1 | M | Hardware-Not-Aus, der die Motorleistung **galvanisch** trennt — nicht per Software. |
| B2 | M | Offener Aufbau **ohne Schutzhaube** (Festlegung 2026-11-01). Stattdessen: Drehzahlgrenze, Wuchtgüte, Sichtprüfung, Schutzbrille, Standplatz außerhalb der Rotationsebene. |
| B3 | M | Automatische Abschaltung bei: ausbleibenden Drehzahlpulsen, unplausiblem Drehzahlvergleich Motor/Trommel, Überschreiten der Kraftgrenze, gesättigter Wägezelle. |
| B4 | M | Kontrollierter Bremspfad über den Regler; die Trommel speichert bei 60 km/h rund 1,4 kJ. |
| B5 | S | Betriebsstundenzähler bzw. Protokoll der gefahrenen Hochläufe für die Wartung. |

## 5 Nicht-Ziele

- Kein normkonformer Prüfstand. Die Absolutwerte sind durch die Trommelkrümmung
  gegenüber Flachbahnwerten systematisch nach oben verschoben; belastbar sind
  **Vergleiche** unter identischen Bedingungen.
- Keine Messung von Seitenführung, Pannensicherheit, Verschleiß oder Nassgriff.
- Keine automatische Reifenerkennung oder Datenbankanbindung.
- Kein Dauerlaufbetrieb ohne Aufsicht.
