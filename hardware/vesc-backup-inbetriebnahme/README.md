# VESC-Konfiguration nach der Inbetriebnahme (2026-11-17)

Zustand **nach** dem Vermessen des Antriebsstrangs. Der Urzustand liegt daneben in
`hardware/vesc-backup/` — das war eine fremde **Skateboard-Konfiguration**, von der
kein Wert zu diesem Aufbau gehoerte.

Unterschied zum Urzustand: 59 Bytes in neun Bereichen, und jeder davon ist eine
bewusste Aenderung. Keine unbeabsichtigte dabei.

| Bereich | Feld | Urzustand | jetzt |
|---|---|---|---|
| @9…26 | Stromgrenzen | ±63,8 A Motor, 99/−60 A Eingang, 130 A absolut | 20/−20 A Motor, 4,5/−1 A Eingang, 40 A absolut |
| @54…62 | Spannungsgrenzen | `l_max_vin` 57 V, Abschaltung 10,2/9,0 V | 43,5 V, 20/18 V |
| @154…161, @208…227 | R, L, Flussverkettung | fremd | gemessen: 238,7 mΩ, 403,8 µH, 17,287 mWb |
| @199 | `foc_sensor_mode` | Sensorlos | **Hall** |
| @263…268 | `foc_hall_table` | nie erkannt (255) | 68, 0, 34, 134, 102, 167 |
| @434…442 | Mechanik | 14 Pole, Übersetzung 2,77, Rad 83 mm | **30 Pole**, 1,0, 0,9030 m |

Dazu in der `appconf`: `timeout_msec` 1000 → **300 ms**, `timeout_brake_current`
bleibt **0** (Begruendung in `docs/04-hardware.md` §10.5 — ein eingerasteter
Bremszustand macht Auslaufversuche unmoeglich).

## Achtung beim Zurueckspielen

Die **Spannungsgrenzen haengen an der Energiequelle.** Hier stehen 20/18 V, weil
zuletzt das Netzteil dran war, das unter Last auf 33,8 V einbricht. Fuer das
10S-Akkupaket gehoeren **32/30 V** hin. Siehe `docs/07-sicherheit.md` §3.2c.

## Wie zurueckspielen

Die Rohbytes aus `vesc-mcconf-fw5.02.bin` bzw. `vesc-appconf-fw5.02.bin` als Nutzlast
senden, mit der Kennung an Position 0 ersetzt: 13 fuer `COMM_SET_MCCONF`, 16 fuer
`COMM_SET_APPCONF`. Gilt **nur fuer Firmware 5.02** — die Signatur wechselt zwischen
Versionen, nach einem Firmware-Wechsel ist die Datei nicht mehr direkt brauchbar.
