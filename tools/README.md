# Werkzeuge für den Rechner

Nicht Teil der Firmware. Laufen am Mac oder Laptop und sprechen über USB mit dem
Motorregler — dasselbe Paketprotokoll wie über UART, deshalb ist der Rahmen hier
derselbe wie in `firmware/src/vesc/packet.cpp`.

Brauchen **pyserial**. PlatformIOs Umgebung hat es bereits:

```sh
~/.platformio/penv/bin/python tools/vesc_console.py /dev/cu.usbmodem3041
```

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
