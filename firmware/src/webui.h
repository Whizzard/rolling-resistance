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
// EIGENE TASK, seit 2026-11-23: Der Webserver der Arduino-Bibliothek BLOCKIERT, wenn
// ein Browser eine Verbindung oeffnet, aber keine Anfrage schickt — iOS tut das
// vorsorglich, und die Bibliothek wartet dann fest 5 s (HTTP_MAX_DATA_WAIT). In der
// Hauptschleife hiess das: 2,8 s Stillstand, der Totmann des Reglers (300 ms)
// schlug zu, und die Regelung gab frei, weil ueber 1 s keine Messwerte kamen. Zwei
// Starts brachen so ab. Deshalb laufen DNS, Webserver und JSON-Aufbau jetzt in
// einer eigenen Task auf Kern 0, neben dem WLAN. Befehle aus dem Browser gehen
// ueber eine Warteschlange an die Hauptschleife und werden dort ausgefuehrt —
// die Regelung bleibt eingaedig, das Netz kann sie nicht mehr anhalten.
//
// Der JSON-Aufbau liest dabei Werte, die die Hauptschleife schreibt. Das sind
// einzelne 32-Bit-Groessen, die auf dem ESP32 atomar sind; ein inkonsistentes
// Paar (etwa Zaehler und Zeitstempel eines Gebers) ist moeglich und kostet
// hoechstens eine Anzeige fuer 200 ms. Gestellt wird aus dieser Task NICHTS.
//
#include <Arduino.h>

namespace webui {

// Befehlsempfänger. Es ist bewusst derselbe Parser wie für die serielle
// Konsole (docs/10-firmware.md G5), damit es nur EIN Befehlsvokabular gibt.
typedef void (*CommandFn)(const String&);

// Baut die Antwort auf /api/state. Die jeweilige Firmwarevariante liefert ihn mit;
// dadurch weiß der Webserver nichts über Messkette oder Antrieb und ist für alle
// Varianten derselbe. Der Puffer ist vorgegeben, die Funktion schreibt JSON hinein.
typedef void (*StateJsonFn)(char* buf, size_t len);

// htmlGz zeigt auf die gzip-gebackene Oberfläche im PROGMEM. Auch sie kommt von
// außen, damit jede Variante ihre eigene Seite ausliefern kann.
void begin(CommandFn handler, StateJsonFn state,
           const uint8_t* htmlGz, size_t htmlGzLen);
// Aus der Hauptschleife rufen: fuehrt die Befehle aus, die der Webserver
// eingereiht hat. Netzwerk wird hier nicht mehr bedient.
void service();

// false, sobald ein Client da war und dann länger als LINK_TIMEOUT_MS schweigt.
// Solange nie ein Client da war, true — der Stand lässt sich auch ohne WLAN
// über die Konsole fahren.
bool linkAlive();
// Wer faehrt? Die Totmannschaltung gilt nur, wenn der Antrieb aus dem Browser
// gestartet wurde. Wer von der Konsole startet, ist selbst der Bediener -- ein
// schlafendes Handy darf ihm den Lauf nicht abwuergen (2026-11-23, mehrfach).
bool inWebCommand();           // true, waehrend ein Browser-Befehl ausgefuehrt wird
void setWebOperator(bool web); // beim Start merken
bool webOperator();

bool clientSeen();
uint32_t msSinceClient();

}  // namespace webui
