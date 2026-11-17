#include "webui.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

#include "config.h"

namespace webui {
namespace {

WebServer     g_server(WEBUI_PORT);
DNSServer     g_dns;
CommandFn     g_cmd     = nullptr;
StateJsonFn   g_state   = nullptr;
const uint8_t* g_html   = nullptr;
size_t        g_htmlLen = 0;

const IPAddress AP_IP(WEBUI_AP_IP[0], WEBUI_AP_IP[1], WEBUI_AP_IP[2], WEBUI_AP_IP[3]);

bool     g_seen     = false;   // war überhaupt schon ein Client da?
uint32_t g_lastSeen = 0;

void touch() { g_seen = true; g_lastSeen = millis(); }

// --------------------------------------------------------------- Protokoll
// Zweck ist das Nachvollziehen am USB-Anschluss: welche Adresse fragt das Telefon
// ab (daran erkennt man, ob die Verbindungspruefung greift), wer verbindet sich,
// und laeuft der zyklische Abruf der Oberflaeche. /api/state wird NICHT einzeln
// protokolliert — 5 Zeilen je Sekunde waeren unlesbar; stattdessen eine Zaehlung.
uint32_t g_polls = 0, g_cmds = 0, g_other = 0, g_lastSummary = 0;

void logReq(const char* what) {
  Serial.print("# HTTP ");
  Serial.print(g_server.method() == HTTP_GET ? "GET " : "??? ");
  Serial.print(g_server.uri());
  Serial.print("  <- ");
  Serial.print(g_server.client().remoteIP());
  Serial.print("  ");
  Serial.print(what);
  const String ua = g_server.header("User-Agent");
  if (ua.length()) { Serial.print("  UA="); Serial.print(ua.substring(0, 70)); }
  Serial.println();
}

void summary() {
  const uint32_t now = millis();
  if (now - g_lastSummary < 5000) return;
  g_lastSummary = now;
  if (!g_polls && !g_cmds && !g_other) return;
  Serial.print("# HTTP 5 s: ");
  Serial.print(g_polls);  Serial.print(" x /api/state, ");
  Serial.print(g_cmds);   Serial.print(" x /api/cmd, ");
  Serial.print(g_other);  Serial.print(" x sonstiges, Clients ");
  Serial.print(WiFi.softAPgetStationNum());
  Serial.print(", Link ");
  Serial.println(linkAlive() ? "steht" : "WEG");
  g_polls = g_cmds = g_other = 0;
}

void onWiFiEvent(arduino_event_id_t ev, arduino_event_info_t info) {
  if (ev == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.print("# WLAN: Client verbunden, jetzt ");
    Serial.println(WiFi.softAPgetStationNum());
  } else if (ev == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    Serial.print("# WLAN: Client getrennt, jetzt ");
    Serial.println(WiFi.softAPgetStationNum());
  } else if (ev == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED) {
    Serial.println("# WLAN: Adresse per DHCP vergeben");
  }
}

// Winzige Seite fuer das Portalfenster. Sie versucht, an Safari zu uebergeben, und
// holt die richtige Oberflaeche nach, falls das nicht greift. Kein Knopf: beides
// laeuft von allein.
void sendIndex() {
  logReq("-> Oberflaeche");
  g_server.sendHeader("Content-Encoding", "gzip");
  g_server.sendHeader("Cache-Control", "no-store");
  // charset MUSS mit: ohne ihn liest iOS die Seite als Latin-1 und aus "Prüflaufrad"
  // wird "PrÃ¼flaufrad". Das <meta charset> im Dokument kommt zu spaet, wenn der
  // Kopf etwas anderes behauptet.
  g_server.send_P(200, "text/html; charset=utf-8",
                  reinterpret_cast<const char*>(g_html), g_htmlLen);
}

void handleState() {
  touch();
  g_polls++;

  // Den Inhalt kennt die Variante, nicht der Webserver.
  char buf[512];
  buf[0] = '\0';
  if (g_state) g_state(buf, sizeof(buf));
  if (buf[0] == '\0') snprintf(buf, sizeof(buf), "{}");

  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "application/json; charset=utf-8", buf);
}

void handleCmd() {
  touch();
  g_cmds++;
  const String c = g_server.arg("c");
  Serial.print("# CMD '"); Serial.print(c); Serial.println("'");
  if (c.length() && g_cmd) g_cmd(c);
  g_server.sendHeader("Cache-Control", "no-store");
  g_server.send(200, "text/plain", "ok");
}

// Jede unbekannte Adresse zeigt die Oberfläche. Der DNS beantwortet ohnehin alles mit
// der eigenen Adresse — was immer jemand eintippt, er will hierher.
void handleNotFound() {
  g_other++;
  sendIndex();
}

// Verbindungsprüfung. In der Portalphase umbiegen (das Telefon öffnet die Seite
// von allein), danach beantworten (das Netz gilt als in Ordnung und bleibt stehen).
// Die Verbindungspruefung bekommt genau das, was sie erwartet. Dann gilt das Netz als
// in Ordnung, es springt kein Fenster auf, und — der eigentliche Punkt — das Telefon
// behaelt die Verbindung, statt sie nach kurzer Zeit wegzuwerfen.
// Die Laenge muss byte-genau stimmen, sonst prueft iOS endlos weiter.
void probe(const char* body, int code, const char* type) {
  g_other++;
  logReq("Pruefung -> ok");
  g_server.send(code, type, body);
}
// Genau diese Zeichenfolgen erwarten die Prüfer; eine falsche Länge laesst iOS
// endlos weiterprueefen (tzapu/WiFiManager #868).
void handleProbeApple() { probe(
  "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>\n", 200, "text/html"); }
void handleProbe204()   { probe("", 204, "text/plain"); }
void handleProbeMsft()  { probe("Microsoft Connect Test", 200, "text/plain"); }

}  // namespace

void begin(CommandFn handler, StateJsonFn state,
           const uint8_t* htmlGz, size_t htmlGzLen) {
  g_cmd     = handler;
  g_state   = state;
  g_html    = htmlGz;
  g_htmlLen = htmlGzLen;

  WiFi.persistent(false);
  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  const char* pw = (sizeof(WEBUI_PASSWORD) > 8) ? WEBUI_PASSWORD : nullptr;  // <8 → offen
  const bool ok = WiFi.softAP(WEBUI_SSID, pw, WEBUI_CHANNEL, 0, WEBUI_MAX_CLIENTS);
  Serial.print("# softAP(\""); Serial.print(WEBUI_SSID);
  Serial.print("\", Kanal "); Serial.print(WEBUI_CHANNEL);
  Serial.print(pw ? ", WPA2" : ", OFFEN");
  Serial.print(") = "); Serial.print(ok ? "ok" : "FEHLGESCHLAGEN");
  Serial.print("  IP "); Serial.print(WiFi.softAPIP());
  Serial.print("  MAC "); Serial.println(WiFi.softAPmacAddress());

  g_dns.setErrorReplyCode(DNSReplyCode::NoError);
  g_dns.start(53, "*", AP_IP);            // jede Anfrage auf uns selbst

  if (MDNS.begin(WEBUI_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEBUI_PORT);
    Serial.print("# mDNS: http://"); Serial.print(WEBUI_HOSTNAME); Serial.println(".local/");
  }

  // "/" entscheidet sich zur Laufzeit: im Portalfenster der Uebergabeversuch,
  // sonst gleich die Oberflaeche. "/ui" ist immer die Oberflaeche.
  g_server.on("/", sendIndex);
  g_server.on("/ui", sendIndex);
  g_server.on("/api/state", handleState);
  g_server.on("/api/cmd", handleCmd);

  g_server.on("/hotspot-detect.html", handleProbeApple);     // iOS, macOS
  g_server.on("/library/test/success.html", handleProbeApple);
  g_server.on("/generate_204", handleProbe204);              // Android
  g_server.on("/gen_204", handleProbe204);
  g_server.on("/connecttest.txt", handleProbeMsft);          // Windows
  g_server.on("/ncsi.txt", handleProbeMsft);

  const char* collect[] = {"User-Agent"};
  g_server.collectHeaders(collect, 1);
  g_server.onNotFound(handleNotFound);
  g_server.begin();
  Serial.print("# Bedienfeld: http://"); Serial.print(AP_IP);
  Serial.println("/  — kein Portal, Netz gilt sofort als in Ordnung");
}

void service() {
  g_dns.processNextRequest();
  g_server.handleClient();
  summary();
}

bool linkAlive() {
  if (!g_seen) return true;             // ohne Bedienrechner gilt die Konsole
  return (millis() - g_lastSeen) < LINK_TIMEOUT_MS;
}

bool clientSeen() { return g_seen; }
uint32_t msSinceClient() { return g_seen ? millis() - g_lastSeen : 0; }

}  // namespace webui
