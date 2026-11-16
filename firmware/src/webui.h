#pragma once
//
// Bedienfeld über WLAN.
//
// Der Prüfstand spannt einen eigenen Accesspoint auf und liefert die Oberfläche
// selbst aus — kein Internet, kein Router, keine App. Beim Verbinden öffnet sie
// sich von allein (Captive Portal): ein DNS-Server beantwortet jede Anfrage mit
// der eigenen Adresse, und unbekannte Pfade werden auf "/" umgeleitet. Damit
// schlägt die Verbindungsprüfung des Betriebssystems fehl und es zeigt die Seite.
//
// Die Oberfläche ist dieselbe Datei, die auch als Entwurf im Browser läuft:
// firmware/web/index.html, gzip-gebacken nach src/webui_html.h. Antwortet
// /api/state, führt der Prüfstand; sonst simuliert die Seite sich selbst.
//
// TOTMANNSCHALTUNG: Solange ein Lauf fährt, muss der Bedienrechner sich melden.
// Bleibt er länger als LINK_TIMEOUT_MS aus, ist das ein Abschaltkriterium wie
// jedes andere (docs/07-sicherheit.md §4) — state.cpp fragt linkAlive() ab.
//
#include <Arduino.h>

namespace webui {

// Befehlsempfänger. Es ist bewusst derselbe Parser wie für die serielle
// Konsole (docs/10-firmware.md G5), damit es nur EIN Befehlsvokabular gibt.
typedef void (*CommandFn)(const String&);

void begin(CommandFn handler);
void service();

// false, sobald ein Client da war und dann länger als LINK_TIMEOUT_MS schweigt.
// Solange nie ein Client da war, true — der Stand lässt sich auch ohne WLAN
// über die Konsole fahren.
bool linkAlive();

bool clientSeen();
uint32_t msSinceClient();

}  // namespace webui
