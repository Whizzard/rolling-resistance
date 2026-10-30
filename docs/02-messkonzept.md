# Messkonzept

## 1 Geometrie

| Größe | Symbol | Wert |
|---|---|---|
| Trommelumfang | U | 3,000 m |
| Trommeldurchmesser | d = U/π | 0,9549 m |
| Trommelradius | r = U/2π | 0,4775 m |

Drehzahl aus Geschwindigkeit:

```
n [1/min] = 5,556 · v [km/h]          ω [rad/s] = 2π · v[m/s] / U
```

| v [km/h] | n [1/min] | ω [rad/s] | Pulsperiode bei 1 Marke |
|---|---|---|---|
| 10 | 55,6 | 5,82 | 1,080 s |
| 30 | 166,7 | 17,45 | 0,360 s |
| 60 | 333,3 | 34,91 | 0,180 s |
| 100 | 555,6 | 58,18 | 0,108 s |

## 2 Kernformel

Der Motor stützt sich über den Hebelarm L mit der Kraft F gegen die Wägezelle ab.
Antriebsmoment an der Trommelwelle: `M = F · L`.
Tangentialkraft an der Trommeloberfläche:

```
F_roll = M / r = F · L / r = F · L · 2π / U
```

**Die Geschwindigkeit kommt darin nicht vor.** Nach Abzug des Leerlaufanteils:

```
Crr = (F_roll − F_roll,leer(v)) / F_N
P   = M · ω = F · L · ω
```

Damit ist die Crr-Genauigkeit vollständig von der Drehzahlauflösung entkoppelt. Die
Geschwindigkeit wird nur gebraucht, um die Leistung anzuzeigen und um den richtigen
Punkt der Leerlaufkennlinie zu treffen.

### 2.1 Was die Pendelwiege misst — und was nicht

Das ist der wichtigste Satz des ganzen Konzepts, weil er bestimmt, welche Verluste
überhaupt im Messsignal auftauchen.

Nimm als System alles, was auf der Wiege sitzt: Gehäuse, Stator, Rotor, und beim
Direktantrieb auch die Trommel. Im stationären Zustand ist der Drehimpuls konstant, also
ist die Summe der **äußeren** Momente null. Äußere Momentenangriffe gibt es nur zwei —
den Hebelarm zur Wägezelle und das, was die Umwelt an der Trommel angreifen lässt:

```
T_Arm = T_außen
```

Alles Innere hebt sich paarweise auf. Rastmoment, Eisenverluste, Lagerreibung zwischen
Rotor und Stator, Getriebereibung: jedes dieser Momente hat seine Reaktion innerhalb des
Systems und fällt heraus. Ein lastfrei drehender Motor übt kein Moment auf seine Wiege
aus, obwohl er Strom zieht und warm wird.

| Bauteil | innerhalb der Wiege? | wird gemessen? |
|---|---|---|
| Rastmoment, Eisenverluste des Motors | ja | **nein** |
| Lagerreibung Rotor ↔ Stator | ja | **nein** |
| Getriebereibung (falls Getriebe mit auf der Wiege) | ja | **nein** |
| Trommellager, wenn die Trommel auf dem Motorrotor sitzt | ja | **nein** |
| Trommellager, wenn die Trommel eigene Lager hat | nein | ja |
| Riemenverluste (Riemen liegt außerhalb) | nein | ja |
| Luftwiderstand der Trommel | nein | ja → Leerlaufkennlinie |
| Reifen am Latsch | nein | ja → **das Signal** |
| Reibung im Pendellager selbst | — | ja, als Hysteresefehler |

Daraus folgen drei Dinge:

1. **Der Direktantrieb mit der Trommel auf dem Motorrotor ist die sauberste Anordnung.**
   Dann bleibt als Leerlauf nur die Aerodynamik übrig; die temperaturdriftende
   Lagerreibung, die Vor- und Nachlauf auseinanderlaufen lässt, entfällt vollständig.
2. **Ein Riemen verschlechtert die Messung**, weil er außerhalb liegt: gemessen wird dann
   das Motorabtriebsmoment, und das ist um 1/(i·η) größer als das an der Trommel.
   η ≈ 0,97 geht als systematischer Skalenfehler von 2–3 % ein.
3. **Das Pendellager ist die verbleibende Fehlerquelle.** Seine Reibung ist das einzige
   äußere Moment, das wir weder wollen noch wegrechnen können. Deshalb Flexgelenk oder
   sehr gutes Wälzlager und nichts Reibendes im Momentenpfad.

Der kommerzielle Referenzprüfstand misst stattdessen die elektrische Leistungsaufnahme
und rechnet mit einer Wirkungsgradkennlinie des Motors zurück
([Beschreibung](https://www.bicyclerollingresistance.com/the-test)). Genau diese Kennlinie
ist dort die größte Fehlerquelle — die Pendelwiege braucht sie gar nicht.

### Wie groß ist das Signal wirklich?

Gemessene Crr-Werte aus der Literatur, einheitlich bei 42,5 kg Last und 28,8 km/h auf
einer 77-cm-Trommel mit Riffelblech-Oberfläche erhoben
([Quelle](https://www.bicyclerollingresistance.com/the-test)):

| Reifen | Leistung | Crr | Moment an unserer Trommel bei 42,5 kg |
|---|---|---|---|
| Vittoria Corsa Speed (bester Rennreifen) | ~11 W | ~0,0025 | 0,50 Nm |
| typischer guter Rennreifen | 13–17 W | 0,0035–0,0040 | 0,70–0,80 Nm |
| Schwalbe Marathon Plus (Pannenschutz) | 25,5 W | 0,00764 | 1,52 Nm |
| Schwalbe Hans Dampf TrailStar (langsamster MTB) | 42,2 W | ≈ 0,0127 | 2,53 Nm |
| **Auslegungsobergrenze** | | **0,015** | **2,99 Nm** |

Der Crr des MTB-Reifens ist selbst gerechnet, weil die Quelle für MTB nur Watt ausweist:
`42,2 W / 8,0 m/s / (42,5 · 9,81 N) = 0,0127`. Zwischen bestem und schlechtestem Reifen
liegt also etwa **Faktor 5**.

Umrechnung auf die Wägezelle, Beispiel Crr = 0,005 bei F_N = 417 N (42,5 kg):

```
F_roll = 0,005 · 417 N              = 2,08 N
M      = 2,08 N · 0,4775 m          = 0,996 Nm
F      = 0,996 Nm / 0,07 m          = 14,2 N  ≈ 1,45 kg
```

Rund 15 % vom Endwert der 10-kg-Zelle. Der Hebelarm von 0,07 m ist so gewählt, dass der
ungünstigste Fall samt Anfahren (4,81 Nm → 69 N) bei etwa 70 % des Messbereichs landet.

## 3 Die sechs Fallstricke

### 3.1 Eine Rampe erzeugt ein Scheinmoment

Die Trommel hat grob J ≈ 2–3 kg·m² (Vollscheibe, ~20 kg, r = 0,48 m; **zu messen**).
Mit der eingestellten Rampe von 0,5 km/h pro Sekunde:

```
dω/dt = 2π · 0,5/3,6 / 3,0 = 0,29 rad/s²
M_beschl = J · dω/dt ≈ 2,3 · 0,29 = 0,67 Nm
```

Das ist **zwei Drittel des Reifensignals** eines guten Rennreifens (0,74 Nm bei 42,5 kg).
Selbst eine zehnmal langsamere Rampe läge noch bei 7 %. Eine durchfahrene Rampe
verfälscht die Kennlinie also in jedem Fall, und zwar um ein Vielfaches des Effekts, den
sie sichtbar machen soll — die Geschwindigkeitsabhängigkeit beträgt über den ganzen
Bereich nur rund 12 %.

→ Gemessen wird auf **Plateaus mit Haltezeit**. Die Rampe dient nur dem Erreichen des
nächsten Plateaus. J wird einmal aus dem Auslaufversuch bestimmt, damit eine
rampenbasierte Messung später optional rechnerisch korrigierbar ist.

### 3.2 Ein Nullpunkt genügt nicht

Zwei verschiedene Dinge heißen umgangssprachlich „Nullabgleich":

**(a) Statischer Zellen-Nullpunkt**, im Stillstand. Erfasst ADC-Offset, das Eigengewicht
des Wiegeteils am Hebelarm und die Drift seit dem letzten Abgleich.

**(b) Leerlaufkennlinie P_leer(v)**, bei drehender Trommel und **abgehobenem Laufrad**.
Enthält genau das, was **außerhalb** der Pendelwiege liegt (siehe §2.1). Beim
Direktantrieb mit der Trommel auf dem Motorrotor ist das allein die Trommelaerodynamik,
rund 0,29 Nm bei 60 km/h. Sie wächst in der Leistung etwa mit v³ und dominiert nach oben.

Nicht enthalten sind Rastmoment, Eisenverluste und Motorlagerreibung: das sind
Rotor-Stator-Paare und damit wiegeintern. Ein lastfrei drehender Motor übt kein Moment
auf seine Wiege aus.

Nur (b) macht die Absolutwerte brauchbar. Weil sich die Lagerreibung mit der Temperatur
ändert, wird die Leerlaufkennlinie **vor und nach** jeder Messreihe gefahren; die
Auswertung interpoliert linear über die Zeit zwischen beiden.

### 3.3 Was sonst noch mitgemessen wird

Im Signal stecken neben der Reifenhysterese:

- Nabenlager des Prüflaufrads
- Aerodynamik des Laufrads (Speichen!) — bei hoher Geschwindigkeit nicht klein
- Scrub im Latsch durch die Trommelkrümmung

Keines davon lässt sich mit dem abgehobenen Laufrad erfassen, denn das Laufrad steht
dabei still. → Für A/B-Vergleiche **dasselbe Laufrad, dieselbe Auflast, dieselbe
Temperatur**. Wer Absolutwerte will, braucht einen Referenzreifen mit bekanntem Crr.

### 3.4 Auflösung gegen Anspruch

Das Verhältnis ist günstiger als zunächst angenommen: der Leerlauf beträgt mit 0,29 Nm
nur rund **30 % des Reifensignals**, nicht ein Vielfaches davon.

Die eigentliche Messlatte setzt etwas anderes. Crr steigt mit der Geschwindigkeit, aber
zwischen 14,4 und 36 km/h nur um **11,6 %**
([Messung](https://www.bicyclerollingresistance.com/specials/crr-speed-test)):

| v [km/h] | 14,4 | 21,6 | 28,8 | 36,0 |
|---|---|---|---|---|
| Crr (Mittel aus 6 Reifen) | 0,00346 | 0,00362 | 0,00378 | 0,00386 |

Die Kennlinie, die dieser Prüfstand aufnehmen soll, ist also ein flacher Effekt. Um ihn
über mehrere Stützstellen aufzulösen, braucht es **1–2 % je Messpunkt**: bei einem guten
Rennreifen sind 1 % gerade 0,0087 Nm, am Hebelarm von 0,07 m also 0,12 N an der Zelle.
Zum Vergleich: zwei gute Reifen unterscheiden sich um ΔCrr ≈ 0,0005, bei 42,5 kg also
0,21 N Tangentialkraft bzw. 1,4 N an der Zelle.

Die Elektronik schafft beides. Die Grenzen liegen woanders:

| Fehlerquelle | Gegenmaßnahme |
|---|---|
| Haftreibung/Hysterese im Pendellager | Flexgelenk oder gutes Wälzlager, nichts Reibendes |
| Steifigkeit der Motorkabel über das Drehlager | weiche Schlaufe; beim Nabenmotor treten die Kabel axial auf der Pendelachse aus |
| Thermische Drift von Zelle und HX711 | Platine vom Motor entkoppeln, statische Tara wiederholen |
| Drift der Leerlaufkennlinie | Baseline vor und nach der Reihe, interpolieren |
| Reifentemperatur | 30 min Warmlauf, Temperatur protokollieren |

→ Jede Aussage braucht Wiederholungen und ein A/B/A-Protokoll, keine Einzelmessung.

### 3.5 Mittelung muss umdrehungssynchron sein

Trommel-Rundlauffehler, die Trommelnaht und die Unrundheit des Reifens modulieren die
Kraft periodisch mit der Trommeldrehung. Ein Mittelwert über eine beliebige Zeitspanne
enthält davon einen Rest, der mit der Phasenlage schwankt.

→ Ein Messpunkt ist immer der Mittelwert über eine **ganze Zahl von Umdrehungen**,
getriggert auf die Indexmarke. Zusätzlich lassen sich die Samples nach Markennummer in
Winkel-Bins sortieren; das macht Naht und Unrundheit sichtbar, statt sie nur wegzumitteln.

### 3.6 Trommelkrümmung

Auf einer Trommel von 0,955 m Durchmesser wird der Reifen stärker verformt als auf der
Ebene; gemessene Crr liegen deshalb systematisch über Flachbahnwerten. Für A/B-Vergleiche
ist das belanglos, für Absolutwerte nicht.

**Offen:** ein belastbarer Korrekturansatz. Wird nicht geraten — siehe
[06-offene-fragen](06-offene-fragen.md).

## 4 Momenten- und Leistungsbudget

Alle Werte bei 60 km/h (ω = 34,9 rad/s), Auflast 42,5 kg, stationär auf dem Plateau.
Das Anfahren steht separat, weil es vor der Messung abgeschlossen ist.

| Anteil | Herkunft | Moment | Leistung |
|---|---|---|---|
| Trommelaerodynamik (Mantel + Seitenscheiben) | gerechnet, §4.1 | 0,29 Nm | 10 W |
| Prüflaufrad (Speichen + Nabenlager) | geschätzt ~10 W | 0,25 Nm | 9 W |
| Reifen, guter Rennreifen (Crr 0,0037) | Literatur | 0,74 Nm | 26 W |
| Reifen, Marathon Plus (Crr 0,0076) | Literatur | 1,52 Nm | 53 W |
| Reifen, langsamster MTB (Crr 0,0127) | Literatur | 2,53 Nm | 88 W |
| Reifen, Auslegungsgrenze (Crr 0,015) | Festlegung | 2,99 Nm | 104 W |
| **Summe Auslegungsfall** | | **3,53 Nm** | **123 W** |
| Anfahren, Rampe 0,5 km/h/s | J·dω/dt, J ≈ 2,3 | +0,67 Nm | +23 W |
| **Spitze beim Hochlauf** | | **4,20 Nm** | **146 W** |

Elektrisch kommen bei etwa 80 % Motor- und 95 % Reglerwirkungsgrad rund **160 W**
heraus, in der Spitze 190 W. Der Strom liegt bei kt = 0,568 Nm/A im Auslegungsfall bei
6,2 A gegen einen Nennstrom von etwa 9,7 A.

### 4.1 Woher die Trommelaerodynamik kommt

Bei 16,67 m/s Umfangsgeschwindigkeit, Trommel Ø 0,955 m, Breite 0,1 m:

```
Mantelfläche:    Re = v·D/ν = 1,1·10⁶ ,  Cf ≈ 0,004 ,  A = π·D·b = 0,30 m²
                 τ = ½·ρ·Cf·v² = 0,67 Pa  →  F = 0,20 N  →  M = 0,096 Nm

Seitenscheiben:  Re_ω = ω·R²/ν = 5,3·10⁵ ,  Cm = 0,146·Re^−0,2 = 0,0105
                 M = Cm·½·ρ·ω²·R⁵ = 0,19 Nm

Summe ≈ 0,29 Nm  ≈ 10 W
```

Die Rechnung gilt für eine **glatte, geschlossene** Trommel in ruhender Luft. Eine offene
Speichenkonstruktion, eine raue Oberfläche, Schrauben oder eine dicht anliegende
Schutzhaube können das Vielfache ergeben. Der erste Baseline-Lauf ersetzt diese Rechnung.

### 4.2 Fehlerbudget

Für ΔCrr = 0,0005 bei F_N = 417 N, also 0,21 N Tangentialkraft:

| Beitrag | Ziel | Anmerkung |
|---|---|---|
| Rauschen der Zelle nach Mittelung | < 0,05 N | über ≥ 10 Umdrehungen |
| Hysterese des Pendellagers | < 0,05 N | konstruktiv, nicht wegrechenbar |
| Drift der Leerlaufkennlinie zwischen Vor- und Nachlauf | < 0,10 N | sonst Reihe verwerfen |
| Unsicherheit von L und r | < 0,5 % | Kalibrierung mit bekanntem Gewicht |
| Unsicherheit der Auflast | < 1 % | geht linear in Crr ein |

Die Summe liegt knapp unter dem Zielwert. Das ist beabsichtigt eng: Einzelmessungen
reichen nicht, Wiederholungen sind Teil des Verfahrens.

### 4.3 Auslaufzeit

Mit dem gerechneten Leerlauf folgt aus `J·dω/dt = −(0,05 + 0,29·(ω/34,9)²)`:

```
t = (J/√(ab))·arctan(ω₀·√(b/a))  mit a = 0,05, b = 2,38·10⁻⁴
  = (2,3/0,00345)·arctan(2,41) ≈ 780 s
```

**Rund 13 Minuten freier Auslauf** von 60 km/h bis Stillstand. Betrieblich lästig und
sicherheitsrelevant (siehe [07-sicherheit](07-sicherheit.md)), messtechnisch dagegen
günstig: der Auslaufversuch nach §5 liefert eine lange, gut aufgelöste Kurve.

## 5 Auslaufversuch als unabhängige Kontrolle

Trommel auf Zielgeschwindigkeit bringen, Antrieb freilaufend trennen, Drehzahlverlauf
aufzeichnen. Daraus:

```
J · dω/dt = −M_verlust(ω)
```

- Liefert **J** (zusammen mit einer bekannten Zusatzmasse oder aus der Geometrie).
- Liefert M_verlust(ω) **unabhängig** von der Wägezelle. Beide Wege müssen übereinstimmen —
  das ist der schärfste verfügbare Test für die gesamte Momentenmesskette.

## 6 Berechnungskette in der Firmware

```
HX711-Rohwert
  → (raw − offset_static) · N_PER_COUNT            = F [N]
  → F · TORQUE_ARM_M                                = M [Nm]
  → M / DRUM_RADIUS_M                               = F_roll [N]
  → M · ω                                           = P [W]
  → (F_roll − F_roll_leer(v)) / F_N                 = Crr
```

`F_roll_leer(v)` stammt aus dem BASELINE-Lauf und wird in der Auswertung, nicht in der
Firmware, interpoliert. Die Firmware loggt beide Läufe roh und rechnet nur eine
Live-Vorschau.
