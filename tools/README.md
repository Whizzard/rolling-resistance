# Werkzeuge für den Rechner

Nicht Teil der Firmware. Laufen am Mac oder Laptop und sprechen über USB mit dem
Motorregler — dasselbe Paketprotokoll wie über UART, deshalb ist der Rahmen hier
derselbe wie in `firmware/src/vesc/packet.cpp`.

Brauchen **pyserial**. PlatformIOs Umgebung hat es bereits:

```sh
~/.platformio/penv/bin/python tools/vesc_console.py /dev/cu.usbmodem3041
```

## `konsole.py` — Konsole des Prüfstands über USB

Spricht mit dem **ESP32**, nicht mit dem Regler: dasselbe Befehlsvokabular wie
Oberfläche und serielle Konsole, mit Zeitstempeln mitgeschrieben. Fürs Fehlersuchen
und Steuern vom Rechner aus, ohne Browser.

```sh
~/.platformio/penv/bin/python tools/konsole.py i                  # Zustand
~/.platformio/penv/bin/python tools/konsole.py j8 v15 sleep:6 c   # Telemetrie, anfahren, freigeben
~/.platformio/penv/bin/python tools/konsole.py ":faults"          # Terminal des Reglers, durchgereicht
~/.platformio/penv/bin/python tools/konsole.py --watch 30         # nur mitlesen
```

Argumente sind Befehle der Reihe nach, `sleep:<s>` wartet dazwischen, `--log` schreibt
den Mitschnitt zusätzlich in eine Datei. `j<s>` ist die Live-Telemetrie der Firmware
(eine Zeile alle 200 ms), `:<text>` reicht einen Textbefehl an das Terminal des
Reglers durch, `><hex>` ein Rohpaket. Befehlstabelle in
[04-hardware](../docs/04-hardware.md) §14.1.

Am 2026-11-23 damit gefunden: der Motor bestromte, drehte aber nicht — ERPM blieb 0,
während der Strom bis 6,4 A stieg. `:hall_analyze 10` und `:foc_openloop 10 300`
zeigten Sensoren und Leistungsteil in Ordnung, der Fehler lag in der Hall-Tabelle.

## `vesc_console.py` — Regler von Hand fahren

Für den ersten Probelauf, bevor die Firmware dazwischensteht.

| Taste | |
|---|---|
| ↑ / ↓ | Sollwert ± 100 ERPM |
| → / ← | ± 1000 ERPM |
| Leertaste | **STOP** — Antrieb stromlos, Trommel läuft aus |
| `0` | Sollwert auf null |
| `q` | Beenden, stoppt vorher |

Angezeigt werden Drehzahl, Sollwert, Busspannung, Motor- und Eingangsstrom,
Aussteuerung, FET-Temperatur und der Fehlercode im Klartext.

**Was es sicher macht:**

- Der Sollwert wird **nur gesendet, solange das Skript lebt**. Stürzt es ab oder wird
  das Fenster geschlossen, kommt nichts mehr, und der Regler stellt nach seinem
  eigenen `timeout_msec` ab — die zweite, unabhängige Ebene.
- **Totmann**: 30 s ohne Tastendruck, und der Sollwert geht von selbst auf null.
  Der Rest der Zeit läuft als Rückwärtszähler mit.
- Beim Beenden — auch bei Strg-C oder einem Fehler — wird freigegeben.
- Gekappt auf **± 2400 ERPM**, derselbe Wert wie `VESC_ERPM_MAX` in `config.h`.
- Starten verweigert, solange der Regler einen Fehler meldet.

**Was es nicht ist:** ein Not-Aus. Der trennt in Hardware.

> Steht `timeout_brake_current` des Reglers auf 0 — die Werksvorgabe —, heißt
> „abstellen" **auslaufen**, nicht bremsen. Die Trommel braucht dann 3 bis 6 Minuten.

`--poles <n>` rechnet zusätzlich auf mechanische 1/min um. Ohne Angabe wird **nicht**
umgerechnet: eine geratene Polzahl ergäbe eine erfundene Drehzahl.

## `vesc_backup.py` — Konfiguration sichern

Liest `mcconf` und `appconf` roh aus und legt sie samt Klartext in
`hardware/vesc-backup/` ab. Nur lesend. Die Sicherung vom 2026-11-17 liegt dort.
