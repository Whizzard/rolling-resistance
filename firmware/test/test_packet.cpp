// Prüfung des VESC-Paketrahmens — läuft auf dem RECHNER, nicht auf dem Board.
//
//     c++ -std=c++17 -Wall -Wextra -I../src firmware/test/test_packet.cpp \
//         firmware/src/vesc/packet.cpp -o /tmp/test_packet && /tmp/test_packet
//
// Zweck: Rahmen und Prüfsumme stimmen, BEVOR etwas angeschlossen wird. Ein falscher
// CRC am angeschlossenen Regler sieht aus wie ein Verkabelungsfehler und kostet
// Stunden.
//
// Der Ankerwert ist 0x31C3 für "123456789" — das ist der Katalogprüfwert von
// CRC-16/XMODEM und damit unabhängig von diesem Projekt. Stimmt er, ist die
// Prüfsumme richtig; stimmt er nicht, nützt der Rest nichts.
#include "vesc/packet.h"

#include <cstdio>
#include <cstring>

static int g_fails = 0;

static void check(bool ok, const char* was) {
  std::printf("%-58s %s\n", was, ok ? "ok" : "FEHLGESCHLAGEN");
  if (!ok) g_fails++;
}

int main() {
  using namespace vesc;

  // ---------------------------------------------------------------- Prüfsumme
  const char* chk = "123456789";
  check(crc16(reinterpret_cast<const uint8_t*>(chk), 9) == 0x31C3,
        "CRC-16/XMODEM(\"123456789\") == 0x31C3");
  check(crc16(nullptr, 0) == 0x0000, "CRC über 0 Bytes ist 0");

  // ----------------------------------------------------------- Rahmen, kurz
  // COMM_FW_VERSION ist ein einzelnes Nullbyte. CRC darüber ist 0x0000.
  const uint8_t fw[] = {0x00};
  uint8_t out[32];
  size_t n = frame(fw, 1, out, sizeof(out));
  const uint8_t want[] = {0x02, 0x01, 0x00, 0x00, 0x00, 0x03};
  check(n == 6 && std::memcmp(out, want, 6) == 0,
        "COMM_FW_VERSION rahmt zu 02 01 00 00 00 03");

  // Zu kleiner Ausgabepuffer darf nicht schreiben, sondern 0 melden.
  check(frame(fw, 1, out, 5) == 0, "zu kleiner Puffer meldet 0");
  check(frame(fw, 0, out, sizeof(out)) == 0, "Nutzlast 0 wird abgewiesen");

  // ----------------------------------------------------------- Rahmen, lang
  static uint8_t big[300];
  for (size_t i = 0; i < sizeof(big); i++) big[i] = static_cast<uint8_t>(i);
  static uint8_t outBig[320];
  n = frame(big, sizeof(big), outBig, sizeof(outBig));
  check(n == sizeof(big) + 6 && outBig[0] == 0x03 && outBig[1] == 0x01 &&
            outBig[2] == 0x2C,
        "300 Byte nehmen die lange Form 03 01 2C");

  // ------------------------------------------------------------- Rundlauf
  Reader r;
  bool done = false;
  for (size_t i = 0; i < 6; i++) done = r.feed(want[i]);
  check(done && r.payloadLen() == 1 && r.payload()[0] == 0x00,
        "Rundlauf kurz: Nutzlast kommt unverändert an");

  r.reset();
  done = false;
  for (size_t i = 0; i < n; i++) done = r.feed(outBig[i]);
  check(done && r.payloadLen() == sizeof(big) &&
            std::memcmp(r.payload(), big, sizeof(big)) == 0,
        "Rundlauf lang: 300 Byte kommen unverändert an");

  // ------------------------------------------------- Müll vor dem Paket
  r.reset();
  done = false;
  const uint8_t muell[] = {0xFF, 0xAA, 0x00};
  for (uint8_t b : muell) r.feed(b);
  for (size_t i = 0; i < 6; i++) done = r.feed(want[i]);
  check(done && r.payloadLen() == 1, "Müll davor verschluckt das Paket nicht");
  check(r.dropped() == 3, "die drei Müllbytes sind gezählt");

  // ------------------------------------------- angefangener Fehlrahmen
  // Der heikle Fall: ein gestörtes Längenbyte lässt den Empfänger auf eine Nutzlast
  // warten, die es nicht gibt, und er verbraucht dabei das FOLGENDE echte Paket.
  // Erwartet wird nicht, dass nichts verloren geht — das kann keine Umsetzung —,
  // sondern dass er sich wieder fängt: ein Störbyte darf höchstens ein Paket kosten.
  r.reset();
  r.feed(0x02); r.feed(0x05); r.feed(0x11);   // 0x02 0x05 verspricht 5 Nutzlastbytes
  int decoded = 0;
  for (int rep = 0; rep < 2; rep++)
    for (size_t i = 0; i < 6; i++)
      if (r.feed(want[i])) decoded++;
  check(decoded == 2, "nach Fehlrahmen werden beide folgenden Pakete gelesen");

  // ----------------------------------------------------- falsche Prüfsumme
  r.reset();
  uint8_t kaputt[6];
  std::memcpy(kaputt, want, 6);
  kaputt[3] ^= 0x01;
  done = false;
  for (size_t i = 0; i < 6; i++) done = r.feed(kaputt[i]);
  check(!done, "falsche Prüfsumme liefert kein Paket");

  // ------------------------------------------- lange Form mit kurzer Länge
  // Der Regler weist das ab; wir auch, sonst haben beide Seiten verschiedene
  // Vorstellungen davon, was ein gültiger Rahmen ist.
  r.reset();
  done = false;
  r.feed(0x03); r.feed(0x00); r.feed(0x01); r.feed(0x00);
  r.feed(0x00); r.feed(0x00); done = r.feed(0x03);
  check(!done, "lange Form mit Länge < 256 wird abgewiesen");

  std::printf("\n%s\n", g_fails ? "ES GIBT FEHLER" : "alle Prüfungen bestanden");
  return g_fails ? 1 : 0;
}
