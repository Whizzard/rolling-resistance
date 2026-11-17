//
// Reglervariante — Hauptprogramm.
//
// Fährt NUR den Motorregler. Keine Wägezelle, keine Hall-Geber, kein LCD, keine
// Zustandsmaschine, kein Messbetrieb. Drei Drähte zum Regler, und er liefert
// Drehzahl, Spannung, Strom, Temperatur und Fehlercode von selbst.
//
// Zweck: den Antriebsstrang in Betrieb nehmen und kennenlernen, bevor ein einziger
// Messwert ansteht. docs/10-firmware.md §2 (T1…T8) und §5.5 (D23…D28).
//
// ZWEI TOTMANNSCHALTUNGEN, unabhaengig voneinander:
//   1. Hier: bleibt der Bedienrechner laenger als LINK_TIMEOUT_MS aus, wird der
//      Antrieb freigegeben (Trommel laeuft aus).
//   2. Im Regler: bleibt laenger als timeout_msec kein Sollwert, stellt er selbst
//      ab. Greift auch dann, wenn DIESE Firmware stehenbleibt.
//
#include <Arduino.h>
#include "config.h"
#include "webui.h"
#include "webui_vesc_html.h"
#include "statusled.h"
#include "vesc/vesc.h"

namespace {

bool g_linkWasAlive = true;

void printHelp() {
  Serial.println("# Befehle: r<erpm>=Sollwert  s=Stop (laeuft aus)  ?=Hilfe");
  Serial.print("# Grenze: +/-"); Serial.print(VESC_ERPM_MAX);
  Serial.println(" ERPM (vorlaeufig, Polzahl noch nicht gemessen)");
}

// Dasselbe Befehlsvokabular fuer Konsole und Weboberflaeche (docs/10-firmware.md G5).
void handleCommand(const String& cmd) {
  if (cmd.length() == 0) return;

  switch (cmd[0]) {
    case 'r': {
      const long v = cmd.substring(1).toInt();
      vesc::setErpm(static_cast<int32_t>(v));
      Serial.print("# Sollwert = "); Serial.print(vesc::targetErpm());
      Serial.println(" ERPM");
      break;
    }
    case 's':
      vesc::release();
      Serial.println("# Stop — Antrieb stromlos, Trommel laeuft aus");
      break;
    case '?':
      printHelp();
      break;
    default:
      Serial.println("# Unbekannter Befehl — '?' fuer Hilfe");
      break;
  }
}

// Antwort auf /api/state. Was der Regler nicht gemeldet hat, ist null — die
// Oberflaeche zeigt dafuer einen Gedankenstrich. Keine erfundenen Zahlen.
void buildStateJson(char* buf, size_t len) {
  const vesc::Values& v = vesc::values();
  const bool ok = vesc::connected();

  snprintf(buf, len,
    "{\"link\":%s,\"erpm\":%ld,\"set\":%ld,\"drive\":%s,"
    "\"vin\":%s,\"cur\":%s,\"tfet\":%s,"
    "\"fault\":%u,\"faultTxt\":\"%s\",\"status\":%u,"
    "\"age\":%lu,\"fw\":\"%s\",\"erpmMax\":%ld}",
    ok ? "true" : "false",
    static_cast<long>(ok ? v.erpm : 0),
    static_cast<long>(vesc::targetErpm()),
    vesc::driving() ? "true" : "false",
    ok ? String(v.vIn, 1).c_str() : "null",
    ok ? String(v.currentMotor, 2).c_str() : "null",
    ok ? String(v.tempFet, 1).c_str() : "null",
    static_cast<unsigned>(v.fault), vesc::faultText(v.fault),
    static_cast<unsigned>(v.status),
    static_cast<unsigned long>(v.ageMs),
    vesc::fwKnown() ? String(String(vesc::fwMajor()) + "." + String(vesc::fwMinor())).c_str() : "",
    static_cast<long>(VESC_ERPM_MAX));
}

void pollSerial() {
  static char line[32];
  static uint8_t n = 0;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[n] = '\0';
      if (n) handleCommand(String(line));
      n = 0;
    } else if (n < sizeof(line) - 1) {
      line[n++] = c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("# Reifenpruefstand — REGLERVARIANTE (misst nichts, faehrt nur)");

  statusled::begin();
  webui::begin(handleCommand, buildStateJson,
               WEBUI_VESC_HTML_GZ, WEBUI_VESC_HTML_GZ_LEN);
  vesc::begin();

  printHelp();
}

void loop() {
  statusled::service();
  webui::service();
  vesc::service();
  pollSerial();

  // Totmannschaltung: reisst die Bedienverbindung ab, wird der Antrieb freigegeben.
  // Nur die FLANKE auswerten, sonst wuerde jeder Durchlauf release() senden.
  const bool alive = webui::linkAlive();
  if (g_linkWasAlive && !alive && vesc::driving()) {
    vesc::release();
    Serial.println("# Bedienverbindung weg — Antrieb freigegeben");
  }
  g_linkWasAlive = alive;
}
