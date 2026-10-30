# Prüfablauf (SOP)

Ein vollständiger Durchlauf dauert rund zwei Stunden, davon 30 Minuten Warmlauf.
Abkürzungen rächen sich: die Unterschiede, die gemessen werden sollen, sind kleiner als
die Effekte, die ein ausgelassener Schritt verursacht. Zur Einordnung — die Kennlinie
über der Geschwindigkeit ändert sich zwischen 14 und 36 km/h um ganze 11,6 %.

## 0 Vor dem Einschalten

- [ ] Schutzhaube geschlossen und verriegelt
- [ ] Not-Aus erreichbar und funktionsgeprüft
- [ ] Trommel sichtprüfen: Risse, Nabenanbindung, lose Teile
- [ ] Riemenspannung prüfen
- [ ] Wiegeteil frei beweglich, Endanschläge nicht anliegend, nichts klemmt am Kabel
- [ ] Umgebungstemperatur notieren

## 1 Statische Tara

Trommel steht. `t` auf der seriellen Konsole.

Die Firmware mittelt 200 Samples und setzt den Offset. Erfasst ADC-Offset, Eigengewicht
des Wiegeteils und Drift. **Wiederholen, wenn sich am Aufbau irgendetwas geändert hat.**

## 2 Kalibrierung (nur bei Erstinbetriebnahme oder nach Umbau)

1. Wiegeteil in Messlage, statische Tara wie oben.
2. Bekanntes Gewicht am Kalibrierpunkt anhängen, Rohwert mitteln.
3. Mit mindestens drei Gewichten über den Bereich wiederholen, Linearität prüfen.
4. `LOADCELL_N_PER_COUNT` in `config.h` und `docs/03-parameter.md` eintragen.
5. Hebelarm L nachmessen und ebenfalls in beiden Dateien eintragen.

## 3 Leerlaufkennlinie, Vorlauf

**Laufrad abgehoben.** `b` auf der Konsole.

Die Firmware fährt alle Plateaus von `PLATEAUS_KMH` ab, wartet je `SETTLE_MS` und mittelt
über `AVG_REVS_PER_POINT` ganze Umdrehungen. Ergebnis: P_leer(v) und F_roll,leer(v).

Dauer rund 15 min. Nicht überspringen — ohne diese Kurve sind die Absolutwerte wertlos.

## 4 Prüfling montieren

- [ ] Laufrad einsetzen, Reifen und Druck notieren
- [ ] Reifendruck einstellen und über `p<wert>` eingeben
- [ ] Auflast aufbringen und über `l<wert>` eingeben — **Standard 417 N (42,5 kg)**,
      das ist die Last der veröffentlichten Crr-Datenbank und macht den Messpunkt bei
      28,8 km/h direkt vergleichbar
- [ ] Trommelbremse des Motors vollständig gelöst? Eine schleifende Bremse erzeugt ein
      temperaturabhängiges Schleppmoment und macht die Messreihe wertlos
- [ ] Laufrad absenken, freien Lauf prüfen

## 5 Warmlauf

**30 Minuten** auf mittlerer Geschwindigkeit (28,8 km/h). Der Rollwiderstand sinkt mit
der Reifentemperatur deutlich; ohne definierten Warmlauf sind Vergleiche zwischen Reifen
nicht belastbar. Reifentemperatur, falls erfassbar, protokollieren.

30 Minuten sind kein Sicherheitszuschlag, sondern die Zeit, die der kommerzielle
Referenzprüfstand ansetzt ([Quelle](https://www.bicyclerollingresistance.com/the-test)).
Der misst anschließend **dreimal pro Druckstufe** bei einer Raumtemperatur, die auf
±0,5 °C gehalten wird. Diese Temperaturkonstanz ist in einer Werkstatt nicht zu
erreichen — genau deshalb ist das A/B/A-Protokoll in §9 kein Extra, sondern tragend.

## 6 Messung

`m` auf der Konsole. Gleiche Plateaus wie im Baseline-Lauf — die Auswertung subtrahiert
punktweise, also müssen die Stützstellen übereinstimmen.

Während des Laufs nicht an den Aufbau fassen, nicht anlehnen, Türen und Lüftung nicht
verändern (die Trommelaerodynamik reagiert auf Luftzug).

## 7 Leerlaufkennlinie, Nachlauf

Laufrad abheben, `b`. Zweite Baseline, direkt im Anschluss, im warmen Zustand.

**Abbruchkriterium:** Weicht der Nachlauf an einer Stützstelle um mehr als 0,10 N
(bezogen auf F_roll) vom Vorlauf ab, ist die Messreihe zu verwerfen. Die Ursache ist dann
meist eine Temperaturdrift der Lager oder etwas, das sich während des Laufs verändert hat.

## 8 Auslaufen lassen

`s`, dann Trommel auslaufen lassen. **Nicht von Hand bremsen.** Bei 60 km/h sind rund
1,4 kJ gespeichert, und der freie Auslauf dauert gerechnet **rund 13 Minuten**
([02-messkonzept](02-messkonzept.md) §4.3). Entweder einplanen oder die mechanische
Trommelbremse des Motors benutzen. Schutzhaube erst öffnen, wenn die Trommel steht.

## 9 A/B/A

Für jeden Vergleich gilt: Referenzreifen — Prüfreifen — Referenzreifen, in einer Sitzung,
bei derselben Auflast und demselben Druck. Weicht die zweite Referenzmessung von der
ersten ab, ist der Unterschied dazwischen nicht interpretierbar.

Mindestens drei Wiederholungen pro Reifen, Streuung mit angeben.

**Plausibilitätsanker:** Fahre einmal einen Reifen, für den publizierte Daten existieren,
bei 42,5 kg und 28,8 km/h. Die Absolutwerte werden nicht übereinstimmen — unsere Trommel
ist größer (0,955 m statt 0,77 m, also weniger Krümmungszuschlag) und hat eine glatte
Holzoberfläche statt Riffelblech, beides senkt den gemessenen Crr. Aber die
Größenordnung muss passen, und die Rangfolge mehrerer Reifen muss stimmen. Tut sie das
nicht, stimmt etwas Grundsätzliches nicht.

## 10 Auswertung

```sh
python3 analysis/crr.py data/<datei>.csv
```

Das Skript trennt BASELINE- und MEASURE-Punkte, interpoliert die Leerlaufkennlinie
zeitlich zwischen Vor- und Nachlauf, subtrahiert punktweise und gibt Crr(v) sowie
P_Reifen(v) aus.

## Sonderlauf: Auslaufversuch zur J-Bestimmung

Einmalig, und nach jeder Änderung an der Trommel:

1. Laufrad abgehoben, auf 60 km/h beschleunigen.
2. Antrieb freilaufend trennen, Drehzahlverlauf bis zum Stillstand aufzeichnen.
3. Aus `J · dω/dt = −M_verlust(ω)` folgt M_verlust(ω) **unabhängig** von der Wägezelle.
4. Beide Wege vergleichen. Abweichungen zeigen Fehler in der Momentenmesskette —
   das ist der schärfste verfügbare Test des Gesamtsystems.
