# Sicherheit

Privates Gerät, offener Aufbau **ohne Schutzhaube**, nur vom Erbauer und nur unter
Aufsicht betrieben. Keine Gefährdungsbeurteilung nach Arbeitsschutzrecht.

## 1 Energie der Trommel

J = 2,07 kg·m² (aus der Geometrie, durch Auslaufversuch zu bestätigen), U = 2,931 m.

> **Diese beiden Zahlen gelten der Ø-933-Trommel.** `config.h` rechnet dagegen mit
> U = 2,837 m und J = 1,94 kg·m² (Ø 903), und die Messungen vom 2026-11-17 weiter unten
> benutzen 1,94. Welche Trommel gilt, ist **nicht entschieden** —
> [06-offene-fragen](06-offene-fragen.md) A14. Die Energiewerte in der Tabelle sind damit
> um 7 % zu hoch, also auf der sicheren Seite; die Momente aus den Auslaufversuchen
> wären mit 2,07 entsprechend größer. Vor der ersten Messreihe ausmessen.
>
> Der Auslaufversuch vom 2026-11-17 bestätigt J **nicht**: er liefert das Verlustmoment
> *unter Annahme* von J. Für J selbst braucht es einen Auslauf mit bekannter
> Zusatzmasse.

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
| Bremspfad | §3. Relais verbaut und gemessen (3,25 Nm, 368 1/min → Stillstand in 26 s). Freier Auslauf dauert dagegen von 100 km/h **160 s**. |
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

> **Stand 2026-11-23, so ist es gebaut:** ein **manueller** Not-Aus über drei Relais,
> der den Motor vom Regler trennt und zugleich kurzschließt. Regler, ESP32 und
> Bedienung bleiben am Netz. Eine Rückmeldung gibt es noch nicht; die Firmware
> erkennt den Fall am **Motorstrom** (§4): Stellwert da, Strom weg, Drehung weg.

**Gebremst wird über Kurzschluss der drei Phasen** — Schaltung in
[04-hardware](04-hardware.md) §9. Gemessen: **25 s** aus über 400 1/min. Zwei Pfade,
beide in dieselbe Richtung: Bus weg → gebremst, GPIO low → gebremst, Reset → gebremst.

**Eine Feststellbremse gibt es nicht.** Eine stehende Trommel ist nicht gesichert:

- Nach jedem Lauf warten, bis die Trommel **sichtbar** steht.
- Vor Arbeiten am Aufbau Blockierkeil zwischen Rahmen und Speiche.

Die Relais sind seit dem 2026-11-17 verbaut und gemessen. Der freie Auslauf bleibt als
zweiter Weg: von 100 km/h **160 s**, von 43 km/h etwa 100 s (`DRUM_LOSS_*` in
[03-parameter](03-parameter.md) §1).

### 3.0 Alles in §3.1 bis §3.2c wurde auf Firmware 5.02 gemessen

> **Der Regler läuft seit dem 2026-11-17 auf Firmware 7.01**, und dort ist
> `foc_short_ls_on_zero_duty` = **1** gesetzt — der echte Brückenkurzschluss, den 5.02
> nicht kannte. Dazu drosselt `l_battery_regen_cut_start/end` = 42,5/44 V die
> Rückspeisung ab, statt den Antrieb fallen zu lassen.
>
> **Die Aussage „mit Netzteil gibt es keine Reglerbremse" gilt damit nur noch für
> 5.02.** Auf 7.01 am 2026-11-23 nachgemessen: **20 A Bremsstrom bremst mit Netzteil
> bis etwa 54 km/h sauber** und endet im echten Kurzschluss (Busstrom ≈ 0). Darüber
> fällt der Regler beim **Einstieg** mit Überspannung — warum, steht in §3.2d aus dem
> Quelltext. Die Firmware bremst deshalb erst unter `BRAKE_MAX_KMH`, darüber lässt sie
> auslaufen.
>
> Was unverändert gilt, weil es Physik und nicht Firmware ist: ein Netzteil nimmt
> keinen Strom auf, und die Zwischenkreiskondensatoren puffern nur Millisekunden.

### 3.1 Der Regler kann nicht bremsen — gemessen auf Firmware 5.02

**Stand 2026-11-17: auf Firmware 5.02 gibt es über den Regler keinen Bremsweg.**
Dreimal belegt, mit montierter Trommel und 40-V-Netzteil:

| Verfahren | Ergebnis |
|---|---|
| Tastverhältnis 0 aus 43 km/h | Bus 40 → **55,5 V**, Fehlercode 1 (Überspannung), Abwurf |
| Tastverhältnis 0 aus 30,7 km/h, Rückspeisegrenze 1 A | Bus 40 → **55,8 V**, Fehlercode 1, Abwurf |
| Drehzahl geregelt herunterfahren, aus 41 km/h | Bus 40 → **53,3 V** |

In allen drei Fällen verlangsamte sich die Trommel vorher kaum — bei 30,7 km/h von 180
auf 177 1/min in 0,6 s, dann war Schluss.

**Warum das vorher falsch eingeschätzt wurde.** Die Annahme war: `SET_DUTY(0)` schaltet
alle drei Halbbrücken auf gleiches Tastverhältnis, also Brückenkurzschluss, Energie
bleibt im Kupfer. **Das gilt für den BLDC-Betrieb, nicht für FOC.** Im feldorientierten
Betrieb ist Tastverhältnis 0 ein *geregelter Arbeitspunkt*: der Regler zieht dafür
Bremsstrom, und der geht in den Bus. Den echten Dauerkurzschluss der unteren Schalter
macht `foc_short_ls_on_zero_duty` — und das gibt es erst ab **Firmware 6.05**.

Ein erster Versuch **ohne** Trommel zeigte keinen Spannungsanstieg und führte in die
Irre: ohne die 1,94 kg·m² ist einfach zu wenig Energie im Spiel. Das war kein Beweis,
sondern ein zu kleiner Versuch.

**Die Rückspeisegrenze hilft nicht.** `l_in_current_min` von 3 auf 1 A herunterzuziehen
änderte nichts: es flossen nur 0,35 A zurück, und der Bus stand trotzdem in 0,6 s an
der Grenze. Die Zwischenkreiskondensatoren des Reglers puffern nichts, und ein Netzteil
nimmt keinen Strom auf.

### 3.2 Was daraus folgt

**Es gibt genau zwei Wege, die Trommel zu verlangsamen:**

1. **Freier Auslauf.** Gemessen am 2026-11-17: 97 → 29 km/h in 140 s, von 43 km/h bis
   Stillstand etwa 100 s. Verlustmoment `DRUM_LOSS_*` in [03-parameter](03-parameter.md) §1.
2. **Kurzschluss der drei Phasen über die Relais** ([04-hardware](04-hardware.md) §9).
   Gemessenes Bremsmoment 3,25 Nm, also dreimal so viel wie der Regler mit 3 A
   überhaupt aufbringen könnte (0,389 Nm/A · 3 A = 1,17 Nm).

**Die Relais sind damit nicht Zusatzausstattung, sondern die einzige Bremse.** Bis sie
geprüft verbaut sind, gilt: kein Lauf über die Drehzahl hinaus, deren freier Auslauf
abgewartet werden kann.

Zwei Wege wären offen, falls eine Reglerbremse gewünscht ist: **Firmware ≥ 6.05**
(bringt `foc_short_ls_on_zero_duty` und `l_battery_regen_cut_start/end`) oder ein
**Akku statt des Netzteils**, der die Energie aufnimmt. Beides nicht entschieden.

### 3.2a Mit Akku bremst der Regler — gemessen 2026-11-17

Am selben Tag wurde das 40-V-Netzteil (maximal 5 A) gegen ein **Akkupaket**
getauscht, 40,55 V im Leerlauf, vermutlich 10S Li-Ion. Damit kehrt sich das Bild aus
§3.1 um: der Akku **nimmt die Energie auf**, und `SET_CURRENT_BRAKE` arbeitet.

| Bremsstrom | aus | Dauer | Busanstieg | Fehler |
|---|---|---|---|---|
| 3 A | 25,5 km/h | 30 s | **+0,4 V** | keiner |
| 20 A | 51,2 km/h | 22,7 s | **+0,3 V** | keiner |

Mit Netzteil stieg der Bus bei vergleichbarer Drehzahl um **+15 V** bis zum Abwurf.
Der Unterschied ist nicht der Regler, sondern was am Bus hängt.

**Der Regler bremst mit 20 A so stark wie die Relais:** 6,2 Nm bei 255 1/min gegen
5,7 Nm beim Kurzschluss über die Relais, und über den ganzen Bereich praktisch
dieselbe Kurve. **Damit verschiebt sich der Zweck der Relais**: sie sind nicht mehr
die *stärkere* Bremse, sondern die **unabhängige** — der Pfad, der auch bei Ausfall
von Firmware, Regler oder Rechner wirkt. Das bleibt ihr Wert.

> **Unter 5 km/h sind Momentenwerte aus der Reglerdrehzahl unbrauchbar.** Bei 0,2 s
> Abstand reicht die Hall-Auflösung dort nicht mehr; der Mitschrieb zeigte 7,9 Nm bei
> 18 1/min, was Rechenrauschen ist.

### 3.2b Bremsstromkurve — und warum ein großer Bremsstrom netzteilfreundlich ist

Am 2026-11-17 als Reihe aufgenommen: je Stufe aus 50 km/h mit `SET_CURRENT_BRAKE`
gebremst, Zeit bis 5 km/h und Energiebilanz gemessen. Verfügbar waren **911 J**.

| Bremsstrom | 50 → 5 km/h | M @ 250 1/min | M @ 150 1/min | Busanstieg | Energie |
|---|---|---|---|---|---|
| 3 A | 31,4 s | 2,04 Nm | 2,56 Nm | +0,40 V | **+487 J** (53 %) |
| 6 A | 19,6 s | 3,27 Nm | 2,54 Nm | +0,70 V | +409 J (45 %) |
| 10 A | 14,9 s | 4,90 Nm | 4,03 Nm | +0,50 V | +223 J (24 %) |
| 15 A | 13,9 s | 7,39 Nm | 6,97 Nm | +0,60 V | **+36 J** (4 %) |
| 20 A | 13,1 s | — | 6,53 Nm | +0,40 V | **−140 J** |

**Bremsleistung und Rückgewinnung schließen sich aus.** Bei 20 A ist das Vorzeichen
negativ: der Regler hat Energie aus dem Akku *geholt*, um so stark zu bremsen.

Der Grund steckt in der Arbeitsweise von `SET_CURRENT_BRAKE` (Stand 2026-11-23 aus dem
Quelltext korrigiert, §3.2d): der Regler **endet** im Kurzschluss, wenn der Sollwert
über dem Kurzschlussstrom liegt — er **beginnt** dort nicht, sondern durchquert beim
Einstieg die Rückspeisezone.

- Sollwert **unter** dem Kurzschlussstrom → er muss ihn *herunterregeln*, und das geht
  nur über den Bus → **Rückspeisung**.
- Sollwert **über** dem Kurzschlussstrom → er muss Strom *zusätzlich treiben*, und das
  **kostet** Busleistung → kein Rückfluss.

> **Hieraus wurde zunächst gefolgert, mit Netzteil dürfe ab 15 A gebremst werden.
> Diese Folgerung ist am 2026-11-17 durch Messung WIDERLEGT.** Mit Netzteil und einem
> Sollwert von 20 A aus 40 km/h: **Fehlercode 1 nach 0,61 s.** Der Eingangsstrom zeigte
> gerade −0,11 A, die Busanzeige 40,2 V — aber die Abfrage lief mit 5 Hz, und ein
> Rückspeisestoß ist eine Sache von Millisekunden. Zwischen zwei Messpunkten stand der
> Bus über `l_max_vin`.
>
> **Der Denkfehler:** die Energiebilanz ist eine **Summe über den ganzen Vorgang**. Bei
> hoher Drehzahl liegt der Sollwert *unter* dem Kurzschlussstrom, dort speist der Regler
> **zurück**; erst bei kleiner Drehzahl entnimmt er, und netto bleibt Entnahme übrig.
> Der Akku schluckt den Anfangsstoß, das Netzteil nicht. Eine Summe sagt nichts über
> den gefährlichsten Augenblick eines Vorgangs.

### 3.2c Stand nach vier Versuchen

| Verfahren | Netzteil | Akku |
|---|---|---|
| `SET_DUTY(0)` | **Überspannung** | — |
| Drehzahl geregelt herunterfahren | **Überspannung** | — |
| `SET_CURRENT_BRAKE` 20 A | **Überspannung** | funktioniert |
| `SET_CURRENT_BRAKE` 3 … 20 A | — | funktioniert, 13–31 s aus 50 km/h |

**Mit Netzteil gibt es keine Reglerbremse.** Es bleiben freier Auslauf oder die
Kurzschlussrelais. **Mit Akku** bremst der Regler mit 15–20 A so stark wie die Relais,
aber steuerbar und stufenlos.

**Festlegung des Erbauers (2026-11-17): es wird immer der schnellstmögliche Stopp
gefahren**, nie ein langsamer. Rückgewinnung ist damit kein Auswahlkriterium — sie
fällt an oder nicht. Es gibt deshalb nur **einen** Bremsstrom,
`VESC_BRAKE_CURRENT_STOP` = 20 A.

Daraus folgt für die Energiequelle: **der Akku ist nicht Zubehör, sondern Voraussetzung
für einen schnellen, steuerbaren Stopp.** Am Netzteil bliebe dafür nur der Relaispfad,
und der ist ein Alles-oder-nichts-Schalter ohne Zwischenstufen.

> `l_max_vin` = 43,5 V hat beim Versuch genau das getan, wofür es gesetzt wurde: früh
> abgeworfen statt an der Hardwaregrenze von 57 V. Die Grenze bleibt.
>
> **Die Unterspannungsabschaltung muss der Quelle folgen.** Für das 10S-Paket gehören
> 32/30 V hin; das Netzteil sackt unter Last aber auf **33,8 V** ab und würde sich damit
> selbst abdrosseln. Beim Wechsel der Quelle sind `l_battery_cut_start/end`
> mitzuführen — für das Netzteil 20/18 V. [TBD: in eine Checkliste aufnehmen]

> Die Energiezahlen stehen auf schwächerem Grund als die Momente. Der Eingangsstrom
> ist im Bremsbetrieb eine **gerechnete** Größe des Reglers, keine Messung; beim
> 20-A-Lauf stand er durchgehend auf exakt 0,00 A. Für eine belastbare Bilanz braucht
> es eine Stromzange oder den Zähler des Akkus. [TBD]

**Schutzgrenzen für den Akkubetrieb**, gesetzt am 2026-11-17:

| | auf 5.02 | **jetzt (7.01)** | Grund |
|---|---|---|---|
| `l_battery_regen_cut_start` / `_end` | *gibt es nicht* | **42,5 / 44 V** | **Die eigentliche Schutzgrenze.** Drosselt die Rückspeisung ab, statt abzuwerfen. 42,5 V liegt über der Ladeschlussspannung eines 10S-Pakets (42,0 V) und über der Leerlaufspannung des Netzteils. |
| `l_max_vin` | 43,5 V | **48 V** | harter Fehlerfall, bewusst **über** der Abdrosselung — erst drosseln, dann abwerfen. Auf 5.02 war 43,5 V nötig, weil es die Abdrosselung nicht gab und das BMS sonst der einzige Schutz gewesen wäre. Ein BMS, das unter Rückspeisung **auftrennt**, lässt den Regler schlagartig in den Leerlauf speisen — der Fall, der Endstufen zerstört. |
| `l_battery_cut_start` / `_end` | 32 / 30 V | **32 / 30 V** | 3,20 / 3,00 V je Zelle bei 10S. Im Werkszustand standen dort 10 / 8 V, also wirkungslos. |
| `l_in_current_min` | −10 A | **−10 A** | Rückspeisestrom. Höher nur, wenn die Ladestromgrenze des Pakets bekannt ist. [TBD] |

> **Noch nicht geprüft:** Zellzahl, Chemie und zulässiger **Ladestrom** des Pakets sind
> nicht bestätigt, die 10S sind aus 40,55 V erschlossen. Ein **volles** Paket nimmt
> ebenfalls kaum etwas auf — dann kommt die Überspannung zurück, nur mit dem Paket als
> Opfer statt dem Netzteil. Vor längeren Bremsreihen klären.


### 3.2d Was der Regler beim Bremsen wirklich tut — Quelltext FW 7.01, gelesen 2026-11-23

Gelesen in `mcpwm_foc.c` und `mc_interface.c` (vedderb/bldc, Stand 7.01), nach dem
Bremsversuch aus 60,9 km/h mit Netzteil, der viermal Fehler 1 brachte.

| Befehl / Zustand | Was der Regler tut |
|---|---|
| Motor freigegeben (`SET_CURRENT 0`) | Brücke aus, Ströme 0. Der Regler misst aber weiter die Phasenspannungen und führt daraus `vq` und ein **Tastverhältnis** mit — bei 60 km/h steht dort 0,42, obwohl nichts gestellt wird (Fehlerspeicher: „Duty 0.419" bei freigegebenem Motor). |
| `SET_CURRENT_BRAKE 20 A` aus Fahrt | Der Kurzschlusszweig (Modulation auf 0 geklemmt, mit `foc_short_ls_on_zero_duty` alle unteren Schalter an, kein Busstrom) wird nur betreten, wenn das mitgeführte Tastverhältnis schon unter 0,001 liegt oder Drehzahl bzw. `vq` das Vorzeichen wechseln. Aus Fahrt ist beides falsch → direkte Stromregelung auf −20 A. Der Stromregler startet bei `vq` = Gegen-EMK und fährt herunter; dabei durchquert der Strom **zwangsläufig** die Zone 0 … −I_kurz, in der Leistung in den Bus fließt. Dieser Stoß dauert Millisekunden und ist durch keinen Parameter abschaltbar. |
| … danach, Sollwert > I_kurz | Modulation auf 0 geklemmt, echter Kurzschluss, Energie bleibt im Kupfer. Gemessen: ab 54 km/h **14–16 A** Motorstrom bei 0,05 A Busstrom — das ist der Kurzschlussstrom selbst, nicht der Sollwert. |
| … danach, Sollwert < I_kurz | Geregelte Rückspeisung in den Bus, dauerhaft. Kleine Bremsströme sind darum am Netzteil die gefährlichen. |
| `SET_DUTY 0` | PI-Rampe (`foc_duty_dowmramp_kp/ki` = 20/400) vom aktuellen Tastverhältnis auf 0 mit **bis zu 20 A Rückspeisung** während der Rampe. Nicht sicherer als der Bremsstrom. Die Firmware schickt diesen Befehl nicht. |
| `l_max_erpm_fbrake`, `l_max_erpm_fbrake_cc` | Kommen in `mcpwm_foc.c` und `mc_interface.c` nicht vor, nur im BLDC-Code `mcpwm.c`. **In FOC ohne Wirkung.** |
| Überspannung (Fehler 1) | Integrator über (U − `l_max_vin`) je PWM-Takt, Auslösung ab 2,4 V·Takte — also nach **1 bis 3 Takten** bei 25 kHz. Der Spannungswert im Fehlerspeicher wird vom Fehler-Thread erst danach gelesen und zeigt nicht die Spitze (46,1 V, später 42,7 V bei einem Fehler, der 48 V voraussetzt). |
| `l_battery_regen_cut_start/end` 42,5/44 V | Wird mit 1 kHz aus einer 45-Hz-gefilterten Spannung gerechnet. Gegen den Einstiegsstoß **wirkungslos**, gegen langsame Rückspeisung (Tastverhältnisrampe) wirksam. |
| Nach dem Fehler | PWM aus, Motor frei; **500 ms** lang werden alle Stellbefehle verworfen (`m_fault_stop_time_ms`), dann ist das Fehlerbit weg. |

**Messpunkte 2026-11-23, Netzteil am Bus, Rad aufgelegt:** Bremseinstieg bei 59,2 /
57,7 / 56,1 km/h → sofort Fehler 1; bei 54,0 km/h sauber, danach Kurzschluss bis zum
Stand. Die Energie des Stoßes wächst etwa mit dem Drehzahlquadrat; zwischen 54 und 56
km/h liegt sie offenbar gerade an der Grenze der Pufferung. Wie hoch die Spitze geht,
ist **nicht gemessen** (bräuchte ein Oszilloskop am Bus). [TBD]

**Folgen für die Firmware:** `BRAKE_MAX_KMH` = 45 km/h — darüber wird freigegeben und
ausgelaufen, darunter einmal 20 A. Kein Nachsetzen innerhalb `BRAKE_FAULT_HOLD_MS`
nach einem Fehler. Auslauf 60 → 45 km/h mit Rad dauert etwa 30 s (0,47 km/h/s aus dem
Log).

**Offene Wege, falls aus 60 km/h gebremst werden soll:** `l_max_vin` von 48 V auf die
Firmware-Vorgabe 57 V (Hardware der 4.x-Klasse: 60-V-Schalter; ob das Netzteil und
alles andere am Bus eine solche Spitze vertragen, ist zu prüfen) oder ein Puffer am
Bus (Akku, Bremswiderstand mit Komparator). Nichts davon ist beschlossen.

### 3.3 Der Not-Aus nimmt den Antrieb, nicht die Drehung

Das Abschalten des Netzteils entfernt das **treibende Moment**. Die Trommel dreht danach
weiter und läuft frei aus — Minuten, nicht Sekunden. Als Not-Aus gegen „Motor treibt
weiter" ist es wirksam, gegen „Trommel dreht noch" nicht.

> **Fallstrick beim Wiederanfahren, am 2026-11-17 ausgelöst.** Ein Sollwert, der
> *unter* der momentanen Drehzahl liegt, ist ein **Bremsbefehl**. Wird nach einem Stopp
> mit Stellwert 0 wieder angefahren, während die Trommel noch dreht, speist der Regler
> zurück und wirft bei Überspannung ab. Jedes Werkzeug, das anfährt, muss den Stellwert
> aus der **gemessenen** Drehzahl vorbesetzen (`MOTOR_NO_LOAD_RPM_PER_V`). Dasselbe gilt
> für einen Richtungswechsel: erst Stillstand abwarten.

## 4 Automatische Abschaltkriterien (Firmware)

| Kriterium | Grenze | Reaktion |
|---|---|---|
| Keine Drehzahlpulse trotz Sollwert > 0 | `STALL_TIMEOUT_MS` = 2000 ms | FAULT, Antrieb aus |
| n_Motor gegen n_Trommel unplausibel | `SLIP_TOLERANCE` = 5 % | FAULT, Antrieb aus |
| Kraft an der Wägezelle | `MAX_FORCE_N` = 45 N | FAULT, Antrieb aus |
| Wägezelle gesättigt oder HX711 ohne Antwort | — | FAULT, Antrieb aus |
| Trommeldrehzahl über Grenze | `MAX_DRUM_RPM` = 341 1/min | FAULT, Antrieb aus |
| Motortemperatur | **TBD**, siehe A5 | FAULT, Antrieb aus |
| **Bedienverbindung abgerissen** | `LINK_TIMEOUT_MS` = 10 s (bis 2026-11-23 2 s; iOS hält den Einzelclient-Webserver bis 5 s mit leeren Vorab-Verbindungen, die Abfragen der Seite warten derweil) | FAULT, Antrieb aus |
| **Kein Motor am Regler** (Not-Aus ausgelöst oder Motor abgeklemmt): Stellwert > `ESTOP_DUTY_MIN` = 0,05 (bis 2026-11-23 abends `STALL_DUTY_MIN` = 0,20, damit unter 30 km/h blind), Motorstrom < `ESTOP_IMOT_MAX_A` = 0,3 A **und** (Drehzahl < `ESTOP_SPEED_RATIO` = 0,6 × Leerlaufdrehzahl zum Stellwert **oder** Trommel wird schneller als `ESTOP_DECEL_KMH_S` = 0,5 km/h/s langsamer, ohne dass der Sollwert sinkt). Strom allein gab Fehlalarm im Leerlauf (0,2 A bei 28 km/h); Stillstand allein kam 25 s zu spät, weil die Hall-Geber die auslaufende Trommel melden (beides 2026-11-23). Ein angeschlossener Motor kann nicht stromlos hinter seiner Spannung zurückbleiben | `ESTOP_DETECT_MS` = 1000 ms | Antrieb freigegeben, Meldung in Oberfläche und Konsole; im Lauf FAULT. Seit 2026-11-23 |
| **Not-Aus-Hilfskontakt** an `PIN_ESTOP_FB` | nur wenn `ESTOP_FB_WIRED` | FAULT; **noch nicht verdrahtet (TBD)** |

FAULT hält, solange die Trommel dreht, und löst sich zwei Sekunden nach dem
Stillstand von selbst (seit 2026-11-23; vorher rastend mit Quittierung, was am Stand
nur störte). Der Antrieb bleibt bis dahin aus, der Grund steht rot im Bedienfeld.
`q` auf der Konsole quittiert weiterhin, ebenfalls nur im Stillstand.

> **Der eigene Trommelaufnehmer ist Pflicht, nicht Redundanz.** Am 2026-11-17 gemessen:
> `foc_sl_erpm` = 4000 bedeutet, dass der Regler oberhalb von **4000 ERPM = 267 1/min
> = 45,4 km/h** auch im Hall-Modus mit dem **Flussbeobachter** rechnet statt mit den
> Gebern. Der Beobachter braucht Strom und Spannung am Motor — trennen die Not-Aus-Relais
> den Regler ab, ist er blind, und seine Drehzahlmeldung driftet.
>
> Beobachtet: beim Not-Aus aus 550 1/min (93,7 km/h) meldete der Regler anschließend
> 708 1/min, also 157 1/min **zu viel**. Erst unter 267 1/min übernahmen die Hall-Geber
> wieder und die Werte wurden gültig. Genau im Bereich, in dem die Bremse zählt, ist
> der Regler also keine Drehzahlquelle.
>
> **Verschärfung, am 2026-11-17 bestätigt:** beim Auslösen der Relaisbremse aus
> 101 km/h riss gleichzeitig die **USB-Verbindung** zum Regler ab — der letzte Messpunkt
> lag im selben Augenblick, und abgeschaltet wurde erst danach.
>
> **Der Not-Aus trennt nur Regler↔Motor, nicht Netzteil↔Regler** (Auskunft des
> Erbauers, 2026-11-17). Der Regler hatte also Strom und ist trotzdem vom USB-Bus
> verschwunden. Er hat sich demnach **zurückgesetzt oder ist abgestürzt**.
>
> *Hier stand zuerst, er habe die Versorgung verloren. Das war falsch und ist
> zurückgenommen.* Richtig bleibt die Teilbeobachtung, die es nahelegte: **dieser
> Regler bekommt über USB keinen Strom** — mit abgeklemmtem Akku verschwindet er
> vollständig vom Bus, die Logik hängt am Leistungseingang. (Der originale VESC 4.12
> versorgt seine Logik aus den 5 V des USB, dieser Flipsky nicht. Nicht
> verallgemeinern.)
>
> **Verdacht auf die Ursache**, und er steht als Warnung schon in
> [04-hardware](04-hardware.md) §10.4: *„Erst den Treiber sperren, dann die Relais
> abfallen lassen."* Beim Versuch wurde die Phasenverbindung aufgetrennt, **während der
> Regler mit Tastverhältnis 0,8 trieb** und Strom durch die Wicklungen floss. Einen
> stromführenden induktiven Kreis zu öffnen erzeugt eine Spannungsspitze — an den
> Phasenklemmen oder über die Relaisspulen zurück in die Versorgung. Eines von beidem
> hat den Rechner im Regler umgeworfen. **Das belastet den Regler bei jedem
> Auslösen.** [TBD]
>
> **So ist es zu prüfen:** nach dem nächsten Not-Aus im Reglerterminal `uptime` und
> `faults` abfragen. Eine kleine Laufzeit beweist den Neustart, die Fehlerliste nennt
> den Grund. (Am 2026-11-17 nicht mehr möglich — das Firmware-Update hat die Liste
> gelöscht.)
>
> **Unabhängig von der Ursache gilt:** nach dem Not-Aus liefert der Regler keine
> brauchbare Information. Der eigene Trommelaufnehmer am ESP32 ist damit **nicht die
> zweite Quelle, sondern die einzige**, und der ESP32 braucht seine Versorgung
> unabhängig vom geschalteten Leistungspfad — hat er über USB-C
> ([04-hardware](04-hardware.md) §5).
>
> Zwei Wege: die **8 Marken an der Trommel** am ESP32 auswerten (unabhängig von allem,
> was im Leistungspfad passiert), und/oder `foc_sl_erpm` so weit heraufsetzen, dass die
> Hall-Geber über den ganzen Bereich in Betrieb bleiben. Das Zweite ändert die
> Kommutierung bei Drehzahl und muss erprobt werden. [TBD]

**Zur Bedienverbindung.** Solange ein Lauf fährt, muss sich der Bedienrechner melden —
reißt die Verbindung ab, bricht der Lauf ab und die Trommel läuft aus. Drei Festlegungen
dazu:

- Maßgeblich ist ein **Herzschlag**, nicht die TCP-Verbindung: ein weggelegtes oder
  eingeschlafenes Telefon merkt TCP erst nach Minuten. Jede Abfrage von `/api/state`
  zählt als Lebenszeichen, die Oberfläche fragt 5× je Sekunde.
- Das Kriterium ist **erst scharf, nachdem ein Client da war** (`webui::clientSeen()`).
  Ohne Bedienfeld lässt sich der Stand weiterhin allein über die serielle Konsole
  fahren — sonst wäre jeder Konsolenlauf sofort ein Fehlerfall.
- Es hält wie jedes andere Kriterium, bis die Trommel steht. Ein Lauf, der nach
  einem Verbindungsabriss von selbst weiterfährt, sobald das WLAN wiederkommt, wäre
  die schlechtere Eigenschaft.

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

## 9 Kein Befehl darf sich von selbst wiederholen

Am 2026-11-18 setzte sich der Antrieb beim **Seitenaufbau** von selbst auf
20 km/h. Kein Fehlgriff, kein Fehler im Regler — eine Eigenschaft von HTTP:

Die Oberfläche schickte Befehle als **GET**. Ein GET gilt als wiederholbar, und
iOS schickt ihn nach einem Verbindungsabriss von selbst noch einmal. Der
`WebServer` des Arduino-Kerns kann laut eigenem Quelltextkommentar nur **einen**
Client gleichzeitig; beim Seitenaufbau reißt deshalb etwas ab. Der Browser
wiederholte den letzten Befehl — Sekunden später, als `v20`.

**Zwei Sperren, nicht eine:**

1. Befehle gehen nur noch als **POST**. Ein wiederholter GET landet in
   `onNotFound` und zeigt die Oberfläche, statt zu fahren.
2. Jeder Befehl trägt den **Zeitstempel** mit, den die Oberfläche zuletzt aus
   `/api/state` gesehen hat. Was älter ist als `CMD_MAX_AGE_MS` = 2000 ms, wird
   mit **409** abgewiesen und auf der Konsole als `VERWORFEN` vermerkt.

> **STOP und Auslaufen sind von der Altersprüfung ausgenommen.** Ein
> verspätetes STOP ist harmlos; ein verworfenes wäre eine Sperre gegen das
> Anhalten, und das ist genau die falsche Richtung.

Die allgemeine Lehre: **eine Bedienhandlung an einer Maschine ist nicht
idempotent.** Sie darf nicht über ein Verfahren laufen, das Wiederholung als
unschädlich annimmt.

## 10 Betriebserfahrung 2026-11-23: was Abbrüche und Überspannung verhindert

Erster Tag mit Last auf dem Rad. Alles unten ist gemessen, nicht vermutet, und in
Firmware und Parameterliste eingearbeitet. Reihenfolge nach Häufigkeit des Ärgers.

| Symptom | Ursache | Abhilfe |
|---|---|---|
| Lauf bricht nach Sekunden ab, „Bedienverbindung weg" | Der Arduino-Webserver bedient **einen** Client; iOS hält ihn mit leeren Vorab-Verbindungen bis 5 s. Erst blockierte das die Hauptschleife (2,8 s), dann die Abfragen der Seite. | Webserver in eigener Task; Totmann 10 s statt 2 s; Totmann gilt nur für Läufe, die aus dem Browser gestartet wurden. |
| Regler meldet Überspannung beim Herunterfahren | Tastverhältnis senken ist in FOC **Rückspeisung** in den Bus. Das Netzteil nimmt nichts auf: 43,8 V, Fehler 1 bei 40 → 15 km/h. | Soll unter Ist heißt **Bremsen**, nicht stellen. |
| Überspannung beim Bremsen aus 60 km/h, dann tack-tack-tack | Der **Einstieg** in den Bremsstrom durchquert in FOC immer die Rückspeisezone (§3.2d); der Stoß wächst mit dem Drehzahlquadrat und reichte ab 56 km/h für Fehler 1. Danach setzte die Firmware alle 3 s bei fast derselben Drehzahl nach — jeder Wiederanlauf fiel erneut. Bei 54 km/h griff die Bremse und hielt den echten Kurzschluss (14–16 A, Busstrom ≈ 0). | Oberhalb `BRAKE_MAX_KMH` = 45 km/h freigeben und auslaufen lassen, darunter einmal 20 A. Gilt für STOP, für FAULT und für jeden Abstieg. Tastverhältnis 0 ist **kein** Ausweg (PI-Rampe mit Rückspeisung); `l_max_erpm_fbrake` wirkt nur im BLDC-Code. |
| „Drehzahlgrenze" auf dem obersten Plateau | Das Plateau liegt auf `V_MAX_KMH`, 1 km/h Überschwingen reichte. | `OVERSPEED_MARGIN_KMH` = 3 km/h Luft; ein abgebrochener Leerlauflauf speichert sein Modell trotzdem, wenn er drei Punkte hat. |
| „Trommel dreht nicht" beim Start unter Last | Losbrechen unter 494 N erst bei Stellwert 0,139 / 8,3 A; Stillstandsgrenze 0,05 stammte aus Läufen ohne Last. | Anfahrfenster 0,16 solange die Trommel steht, Klemme mit Lastzuschlag 0,08, Stillstandsgrenze 0,20. |
| Motor bestromt, dreht nicht, kein Fehler | Hall-Tabelle passte nach dem Umstecken nicht mehr. | Nach jedem Umstecken Hall-Tabelle neu erkennen **und** Drehrichtung prüfen ([04-hardware](04-hardware.md) §11.2). |
| Not-Aus nicht erkannt, Soll bleibt stehen | Strom allein: Fehlalarm im Leerlauf (0,2 A). Stillstand allein: Geber melden die auslaufende Trommel 25 s lang. | Kein Strom **und** Drehzahl weit unter der Leerlaufdrehzahl zum Stellwert, **oder** Trommel wird ohne Strom langsamer bei konstantem Soll. Erkennung in 1,2 s. Hilfskontakt an `PIN_ESTOP_FB` wäre sofort und eindeutig — noch nicht verdrahtet. |
| Reglerfehler nie zu sehen | Der Regler löscht den Code nach 500 ms. | Letzter Fehler bleibt 30 s im Bedienfeld, jeder neue auf der Konsole mit Spannung, Strom, Drehzahl; `:faults` liefert die Historie. |
| Seite friert ein, Meldung erst nach Refresh | Ein JS-Fehler in `paint()` wurde vom Fehlerfang geschluckt. | Fehler werden rot angezeigt; die Seite lädt sich neu, wenn die Firmware eine andere Buildkennung meldet. |
| Überschwingen beim Hochlaufen | Reiner Integrator, Rampe lief mit voller Steigung ins Ziel, Trommel hinkte 3 km/h nach. | PI-D, Anti-Windup, D nur über die Rampensteigung hinaus, Rampe höchstens 1,5 km/h vor der Trommel, abflachend vor dem Ziel. 0,8–1,0 km/h, danach ±0,2. |
| Hauptschleife stand 266 ms ohne Mitleser | USB-CDC blockiert, wenn der Rechner nicht liest. | `Serial.setTxTimeoutMs(20)`. Offen: bei Telemetriefenstern über eine Minute setzt die USB-Ausgabe zeitweise aus, Schleife und Antrieb laufen weiter. |

**Physik, die dahintersteht:**

- Ein angeschlossener Motor zieht unter Stellwert immer Strom; stromlos hinter der
  Spannung zurückbleiben kann nur ein abgeklemmter. Darauf beruht die Not-Aus-Erkennung.
- Bremsen mit Strom ≥ Kurzschlussstrom hält die Energie im Kupfer; kleinere Bremsströme
  und jedes Absenken des Tastverhältnisses schieben sie in den Bus.
- Beim Beschleunigen steckt J·dω/dt im gemessenen Moment, beim Einregeln pendelt es:
  Verlustleistung und Crr nur bei konstanter Fahrt, gemittelt (`STEADY_*`, `LIVE_WINDOW_MS`).
- Hall-Tabelle und Drehrichtung sind Eigenschaften der Verdrahtung, nicht des Motors.
