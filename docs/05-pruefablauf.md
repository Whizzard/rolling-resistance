# Prüfablauf (SOP)

Nackte Liste. Kein Wort zur Begründung — die steht in den **Anmerkungen** am Ende, je
Schritt über die Marke `(n)` verknüpft, und ausführlich in
[09-messtheorie](09-messtheorie.md). Zahlenwerte stehen hier nur, wo sie zum Handeln
gebraucht werden; verbindlich sind `firmware/src/config.h` und
[03-parameter](03-parameter.md).

| Block | wann |
|---|---|
| **A1** Geometrie | nach jedem Eingriff an Hebelarm oder Trommeloberfläche |
| **A2** Wägezelle | nach jedem Eingriff an Zelle, Wiege oder deren Kabel |
| **A3** Antrieb | je Motor-/Reglerkonfiguration einmal |
| **A4** Auflast einrichten | einmal, dann nur bei Umbau der Schwinge |
| **A5** Trägheit und Verlustmodelle | Trommel: nach jeder Änderung. Laufrad: je Laufrad |
| **B** Messung | jede Messreihe |

---

## A Kalibrieren

Firmware: Variante `esp32s3-vesc` (Regler + Wägezelle + Einrichtungsseite). Befehle
gelten gleich für die serielle Konsole und die Weboberfläche.

### A0 Anschließen

1. Testvariante flashen: `cd firmware && pio run -e esp32s3-vesc -t upload -t monitor`.
2. WLAN `Pruefstand` verbinden — QR `hardware/qr-wlan.png`, Passwort in `config.h`.
3. Oberfläche öffnen: `http://tire.local/` oder `http://4.3.2.1/` — QR
   `hardware/qr-ui.png`. Reiter **Einrichtung**.
4. `i` eingeben. Regler, Wägezelle und Bedienverbindung müssen **alle drei** da sein. (1)

### A1 Geometrie

1. Maßband um die Trommel, drei Messungen mitteln → `d<m>`. (2)
2. Zellenarm ausmessen, ±0,1 mm → `a<m>`. Sollmaß 0,080 m.
3. Kalibrierarm ausmessen, ±0,1 mm → `b<m>`. Sollmaß 0,160 m.
4. In der Oberfläche das Verhältnis prüfen: Soll **2,0-fach**.

### A2 Wägezelle und Momentenkette

1. Trommel steht. Wiegeteil in Messlage, Kabel wie im Betrieb verlegt, Arm frei. (3)
2. Nullen: `t`. 200 Samples.
3. Kalibriergewicht **selbst wiegen**, an die Kalibrierbohrung hängen. **Höchstens
   2,4 kg** — am 2:1-Arm ist das der Endwert der 5-kg-Zelle. (4)
4. `k<kg>`. Die Konsole druckt Kraft, Counts und Faktor.
5. Mit **mindestens drei** Massen über den Bereich wiederholen, z. B. 0,5 / 1,0 / 2,0 kg
   = 20 / 40 / 80 % vom Endwert. Je Masse den Rohwert („Roh") notieren. (5)
6. Geradenfit von Hand: `M = k · (raw − offset)`. **Linearitätsabweichung < 0,2 %**
   verlangen.
7. Hysterese: auf- und absteigend belasten. Differenz bei gleicher Last ins Protokoll,
   **nicht** in den Fit.
8. Gegenprobe: dieselbe Masse am Zellenarm → halbe Kraft. Prüft die Armmaße aus A1.
9. Kriechen: Last 5 min anstehen lassen, Abweichung protokollieren.
10. `LOADCELL_N_PER_COUNT` von Hand in `firmware/src/config.h` **und**
    [03-parameter](03-parameter.md) §2 eintragen. Die Konsole druckt die Zeile. (6)

### A3 Antrieb einmessen

1. Trommel frei, nichts liegt an.
2. Drehrichtung: `l`, dann Trommel **von Hand in Messrichtung** drehen, ≥ 2 km/h.
3. Losbrechmoment: `x`. Trommel nicht anfassen. Tastverhältnis steigt mit 0,01/s,
   Abbruch bei 0,15.
4. Gemessenes Losbrechmoment gegen `DRUM_LOSS_CONST_NM` halten. (7)

### A4 Auflast einrichten

1. Schwinge **ohne Laufrad und ohne Laufgewicht** mit dem Gegengewicht auswuchten.
2. Restkraft an der Radposition mit der Federwaage prüfen: **< ±2 N**.
3. Laufrad wiegen → `m_Rad`.
4. Abstand `a` waagerecht ausmessen, ±0,5 mm.
5. Maßband auf den Holm kleben, Null **auf der Pivotachse**. Index am Laufgewicht
   ankörnen. (8)

### A5 Trägheit und Verlustmodelle

1. **Trommelauslauf:** Rad abgehoben, auf 60 km/h, `c`, Drehzahlverlauf bis Stillstand
   aufzeichnen → `M_verlust(ω) = −J · dω/dt`.
2. Ergebnis gegen die Leerlaufkennlinie aus B5 halten. Beide Wege müssen
   übereinstimmen. (9)
3. `J` aus demselben Versuch mit bekannter Zusatzmasse.
4. **Bifilarpendel** je Laufrad → `J_Rad`: zwei gleich lange Fäden an gegenüberliegenden
   Felgenpunkten, 20 Schwingungen, Ausschlag < 10°, Reifen montiert und aufgepumpt.
5. **Laufrad-Auslauf** je Laufrad → `A₁`, `B₁`: Trommel mit aufliegendem Rad auf 60 km/h,
   Rad **im Lauf** abheben, Raddrehzahl aufzeichnen, Modell `A₁ + B₁·ω²` über den
   **ganzen** Auslauf fitten. (10)
6. Referenzlauf mit starrer Rolle: offen, [06-offene-fragen](06-offene-fragen.md) A11.

---

## B Messung

Firmware: Hauptvariante `esp32s3` (Zustandsmaschine, Plateaus, Protokoll) — siehe **Stand
heute** am Ende. Rund zwei Stunden, davon 30 min Warmlauf. (11)

> **Seit 2026-11-23 führt die Firmware durch B3 bis B9:** *Messung* im Bedienfeld öffnet
> den geführten Messlauf (`run.h`, [11-messablauf-review](11-messablauf-review.md) §3).
> Er prüft vor dem Start Regler, Zelle, Rauschen, Kalibrierstand und Modelle, nullt,
> fragt das Kontrollgewicht ab, fährt bei Bedarf den Leerlauf-Vorlauf, dann allein
> Warmlauf, Plateaus mit drei Punkten, Ankerpunkt und Anhalten, optional den Nachlauf,
> und legt Protokoll und Daten unter `/runs/` ab. Die Schritte unten bleiben die
> Referenz dafür, was der Mensch dabei tut. **Noch nicht am Stand gefahren.**

| # | Schritt | Dauer |
|---|---|---|
| B0 | Vor dem Einschalten | 5 min |
| B1 | Laufrad einsetzen, Geometrie stellen | 10 min |
| B2 | Reifen vorbereiten | 5 min |
| B3 | Statische Tara `t` | 1 min |
| B4 | Baseline Vorlauf `b`, Rad abgehoben | 15 min |
| B5 | Warmlauf, Rad abgesenkt | 30 min |
| B6 | Messlauf `m` | 15 min |
| B7 | Baseline Nachlauf `b`, Rad abgehoben | 15 min |
| B8 | Auslaufen lassen | 5–30 min |
| B9 | Auswertung | 2 min |

### B0 Vor dem Einschalten

- [ ] Not-Aus erreichbar und funktionsgeprüft
- [ ] Trommel sichtgeprüft: Risse, Stoßstelle des Mantelblechs, lose Schrauben,
      Magnetschrauben fest
- [ ] Wiegeteil frei beweglich, Endanschläge nicht anliegend, nichts klemmt am Kabel
- [ ] Kupplung spielfrei
- [ ] Prüflaufrad gesichert, Blockierkeil entfernt
- [ ] Nichts Loses im Prüfraum, keine losen Kleidungsstücke, Schutzbrille
- [ ] Zeit für den Auslauf eingeplant (12)
- [ ] Umgebungstemperatur notiert

### B1 Laufrad einsetzen und Geometrie stellen

- [ ] Steckachse Ø12 mit passenden Buchsen, Rad **formschlüssig** gesichert
- [ ] Laufrad **mittig zur Trommel**
- [ ] Ausleger so klemmen, dass der Gabelholm **waagerecht** steht (Wasserwaage) (13)
- [ ] Radachse senkrecht über der Trommelachse, x = 0 ± 2 mm
- [ ] Rad von Hand drehen: freier Lauf, kein Schlag, Reifen läuft nicht über die
      Trommelkante
- [ ] Auflast wiegen: Rad abheben, Waage auf die Trommel, **Schieber um die Bauhöhe der
      Waage anheben**, Rad absenken, Laufgewicht auf den Zielwert justieren, Position am
      Maßband ablesen, Waage heraus, Schieber zurück (14)

### B2 Reifen vorbereiten

- [ ] Reifen wiegen → `J_Rad = J_ref + (m_Reifen − m_ref) · r_Reifen²`
- [ ] Druck einstellen, `p<bar>` eingeben
- [ ] Reifenumfang belastet messen
- [ ] Auflast eingeben, `l<N>` — Standard `STANDARD_LOAD_N` = **416,9 N** (42,5 kg) (15)

### B3 Statische Tara

1. Trommel steht.
2. `t`. `TARE_SAMPLES` = 200 Samples, ≈ 2,5 s.
3. Nach **jedem** mechanischen Eingriff wiederholen, auch nach Umstellen der
   Auslegerhöhe oder Verlegen eines Kabels.

### B4 Baseline Vorlauf

1. Laufrad mit dem Exzenterhebel **abheben**.
2. *Leerlauf* auf der Messseite (Konsole `m0`). Die Firmware fährt alle
   Messgeschwindigkeiten aus den Einstellungen der Reihe nach ab (Vorgabe
   `PLATEAUS_KMH`: 15, 28,8, 40, 50, 60 km/h), wartet je `SETTLE_MS` und mittelt
   über `AVG_REVS_PER_POINT` **ganze** Umdrehungen.
3. Ergebnis `M_leer(ω)` → Fit `A₀ + B₀·ω²`. Nicht überspringen. (16)

### B5 Warmlauf

1. Rad absenken.
2. **30 min bei 28,8 km/h** (`WARMUP_MS`). (17)

### B6 Messlauf

1. *Messung* auf der Messseite (Konsole `m1`), vorher die Auflast eintragen.
   **Dieselbe Liste** wie in B4 — es gibt nur eine.
2. Während des Laufs:
   - [ ] nicht an den Aufbau fassen, nicht anlehnen
   - [ ] Türen und Lüftung nicht verändern
3. Druckreihe nur bei 28,8 km/h, je Stufe **5 min Angleichzeit**, dann messen.
4. Lastreihe nur bei 28,8 km/h und 4 bar, drei Lasten.
   Stützstellen beider Reihen: [09-messtheorie](09-messtheorie.md) §7.1.

### B7 Baseline Nachlauf

1. Laufrad abheben.
2. `b`, direkt im Anschluss, im warmen Zustand.

### B8 Auslaufen und abrüsten

1. `s`, dann **frei auslaufen lassen. Nicht von Hand bremsen.**
2. Erst weiterarbeiten, wenn die Trommel **sichtbar steht**.
3. Vor Arbeiten am Aufbau: Blockierkeil zwischen Rahmen und Speiche.

### B9 Auswertung und Annahme

1. `python3 analysis/crr.py data/<datei>.csv`
2. **Verwerfen**, wenn der Nachlauf an einer Stützstelle um mehr als **0,02 N**
   (auf `F_roll` bezogen) vom Vorlauf abweicht. `crr.py` bricht dann selbst ab. (18)
3. Je Vergleich **A/B/A**: Referenzreifen — Prüfreifen — Referenzreifen, in **einer**
   Sitzung, gleiche Last, gleicher Druck.
4. Mindestens drei Wiederholungen je Reifen, Streuung mit angeben.
5. Einmal einen Reifen mit publiziertem Wert fahren, 42,5 kg und 28,8 km/h. (19)

### B10 Was ein Wechsel nachzieht

| gewechselt | nachzuziehen |
|---|---|
| Geschwindigkeit, Druck, Last | nichts; bei Druck und Last 5 min nachwärmen |
| **Reifen** | `J_Rad` korrigieren (B2), **neue Baseline**, **volle 30 min** Warmlauf |
| **Laufrad** | zusätzlich A5.4 (Bifilar) und A5.5 (Laufrad-Auslauf) |

---

## Stand heute: was Teil B noch blockiert

- `LOADCELL_N_PER_COUNT` ist TBD, also gibt die Firmware keine Crr-Werte aus, sondern nur
  Rohwerte — [03-parameter](03-parameter.md) §2. **A2 ist der erste Schritt überhaupt.**
- Die Hall-Geber an der Trommel sind nicht verdrahtet. Die Testvariante rechnet `v` aus
  der Reglerdrehzahl (ERPM / 15).
- Die Hauptvariante mit Zustandsmaschine, Plateaus und Protokoll wird mit `-DDRIVE_STUB`
  gebaut, fährt also keinen Motor. Teil B ist bis heute nicht gefahren.
- `r<m>`, `w<A₁> <B₁>` und `n<A₀> <B₀>` aus [10-firmware](10-firmware.md) §8 sind
  Anforderungen, **keine Befehle**: in der Hauptvariante schaltet `n` die Anzeigeseite
  weiter. A₀/B₀ und A₁/B₁ bleiben bis dahin in der Auswertung.
- Es gibt keine Bremse — [07-sicherheit](07-sicherheit.md) §3.

## Anmerkungen

1. Fehlt die Zelle, meldet `i` den HX711 mit Pinnummern; fehlt der Regler, ist es die
   UART. Beides vor dem ersten Gewicht klären.
2. 1 mm Fehler am Umfang sind 0,03 %. Trommelmaß ist offen —
   [06-offene-fragen](06-offene-fragen.md) A14: `config.h` führt 2,837 m (Ø 903),
   [03-parameter](03-parameter.md) 2,931 m (Ø 933). Deshalb steht hier kein Sollwert:
   **messen**.
3. Kalibriert wird nicht die Zelle allein, sondern die ganze Kette bis zum Moment — das
   erspart die getrennte Unsicherheit von Zelle und Hebelarm.
   [09-messtheorie](09-messtheorie.md) §5.1.
4. Die Zelle sieht am Arm L = 80 mm die doppelte Kraft des Gewichts am Kalibrierarm
   L_cal = 160 mm. Rechnung und Stützstellen: [09-messtheorie](09-messtheorie.md) §5.1.
5. `k<kg>` setzt den Faktor aus **einer** Masse. Linearität über mehrere Massen prüft die
   Firmware nicht ([10-firmware](10-firmware.md) W5, Priorität S) — Rohwerte notieren und
   von Hand fitten.
6. Der Faktor wird angezeigt, **nicht** automatisch nach `config.h` übernommen
   ([10-firmware](10-firmware.md) W4). In NVS steht er trotzdem, `r` setzt zurück.
7. Das Losbrechmoment ist Haftreibung und Rastmoment, `DRUM_LOSS_CONST_NM` die
   Laufreibung. Liegt Losbruch weit darüber, klemmt etwas.
8. Damit ist die Hebelwaage reproduzierbar einstellbar, auch ohne Wägung. Die abgelesene
   Position kommt in die CSV; weicht sie ab, hat sich an Laufrad, Reifen oder Mechanik
   etwas geändert.
9. Der Auslauf liefert `M_verlust` **ohne** die Wägezelle. Die Übereinstimmung mit der
   Leerlaufkennlinie ist der schärfste verfügbare Test des Gesamtsystems —
   [09-messtheorie](09-messtheorie.md) §9.1.
10. Ohne diesen Lauf steckt der Laufradverlust ungetrennt im Ergebnis: 0,25 Nm sind ein
    scheinbares Crr von 0,00129, die Hälfte des besten Rennreifens.
    [09-messtheorie](09-messtheorie.md) §9.3.
11. Abkürzungen rächen sich: die zu messenden Unterschiede sind kleiner als die Effekte
    eines ausgelassenen Schritts. Zwei gute Rennreifen trennen 0,2 N Umfangskraft.
12. Keine Bremse. Bei 60 km/h sind rund 1,3 kJ gespeichert, der Auslauf dauert je
    Schleppmoment 3 bis 80 min — [09-messtheorie](09-messtheorie.md) §8.9.
13. Waagerechter Holm heißt: Drehpunkt auf Radachshöhe, die Umfangskraft hat keinen
    Hebel, der Korrekturterm entfällt. [08-staender](08-staender.md) §5.2.
14. Ohne das Anheben kippt die Schwinge um die Bauhöhe der Waage: bei 40 mm und 400 mm
    Schwingenlänge rund 0,5 %. [09-messtheorie](09-messtheorie.md) §5.2.
15. 416,9 N (42,5 kg) ist die Last der publizierten Crr-Datenbank, damit die Werte
    vergleichbar sind.
16. Statische Tara und Leerlaufkennlinie erfassen Verschiedenes und ersetzen sich nicht:
    die Tara den ADC-Offset und das Eigengewicht, die Kennlinie die Aerodynamik.
    [09-messtheorie](09-messtheorie.md) §5.3.
17. 30 min setzt der kommerzielle Referenzprüfstand an. Dessen Temperaturkonstanz von
    ±0,5 °C ist in der Werkstatt nicht zu erreichen — deshalb trägt das A/B/A-Protokoll.
18. 0,02 N sind 1,3 % vom Rennreifensignal. Übliche Ursachen für ein Auseinanderlaufen:
    Temperaturdrift der Lager, schleifende Bremse, verändertes Kabel, Luftzug.
    [09-messtheorie](09-messtheorie.md) §8.8.
19. Die Absolutwerte werden nicht exakt übereinstimmen
    ([09-messtheorie](09-messtheorie.md) §10), aber die Größenordnung muss passen und die
    Rangfolge mehrerer Reifen muss stimmen.
