#pragma once
//
// Paketrahmen des VESC-Protokolls.
//
//   1…255 Byte Nutzlast:    0x02  len[1]          Nutzlast  crc_hi crc_lo  0x03
//   256…65535 Byte:         0x03  len_hi len_lo   Nutzlast  crc_hi crc_lo  0x03
//
// CRC16-CCITT/XMODEM: Polynom 0x1021, Startwert 0x0000, MSB zuerst, kein End-XOR,
// gebildet AUSSCHLIESSLICH über die Nutzlast (nicht über Start- und Längenbytes),
// übertragen big-endian. Alle Mehrbytezahlen im Protokoll sind big-endian.
//
// Das erste Nutzlastbyte ist immer die Befehlskennung; Antworten spiegeln sie.
//
// Der Regler lehnt die lange Form ab, wenn die Nutzlast in die kurze passt
// ("A shorter packet should use less length bytes") — frame() wählt deshalb selbst,
// und Reader weist die lange Form mit kurzer Länge ebenfalls ab. Beide Seiten müssen
// dieselbe Vorstellung davon haben, was ein gültiger Rahmen ist.
//
// BEWUSST OHNE Arduino.h: so lässt sich dieser Teil auf dem Rechner übersetzen und
// gegen bekannte Bytefolgen prüfen, bevor irgendetwas angeschlossen wird.
// Siehe firmware/test/test_packet.cpp.
//
#include <cstddef>
#include <cstdint>

namespace vesc {

// Größte Nutzlast, die der Regler annimmt (PACKET_MAX_PL_LEN der Firmware).
constexpr size_t PAYLOAD_MAX = 512;

// Größter vollständiger Rahmen: lange Form (3) + Nutzlast + Prüfsumme (2) + Ende (1).
constexpr size_t FRAME_MAX = PAYLOAD_MAX + 6;

// Prüfsumme über die Nutzlast. Prüfwert für "123456789" ist 0x31C3 — das ist der
// Katalogwert von CRC-16/XMODEM und damit eine vom Projekt unabhängige Gegenprobe.
uint16_t crc16(const uint8_t* data, size_t len);

// Rahmt payload nach out. Gibt die Gesamtlänge zurück oder 0, wenn out zu klein ist
// oder die Nutzlast 0 bzw. größer als PAYLOAD_MAX wäre.
size_t frame(const uint8_t* payload, size_t len, uint8_t* out, size_t outCap);

// Empfänger. Bytes einzeln einfüttern, nie blockierend — der ESP32 bedient nebenher
// den Webserver.
//
// Gepuffert und bei Bedarf NEU AUSGEWERTET, nicht als reine Zustandsmaschine: Eine
// Zustandsmaschine, die auf ein gestörtes Längenbyte hereinfällt, verbraucht das
// folgende echte Paket als vermeintliche Nutzlast und kann es nicht zurückholen.
// Hier wird bei einem Fehler das vorderste Byte verworfen und ab der nächsten
// Position erneut ausgewertet — damit kostet ein Störbyte höchstens ein Paket.
class Reader {
 public:
  // true, sobald ein vollständiges, prüfsummengeprüftes Paket vorliegt.
  // payload() bleibt bis zum nächsten feed() gültig.
  bool feed(uint8_t b);

  const uint8_t* payload() const { return m_rx + m_plOff; }
  size_t payloadLen() const { return m_plLen; }

  void reset();

  // Verworfene Bytes seit dem Start — Maß für die Leitungsqualität, fürs Protokoll.
  uint32_t dropped() const { return m_dropped; }

 private:
  enum class Try : uint8_t { INCOMPLETE, BAD, GOOD };

  Try tryDecode(size_t& frameLen) const;
  void dropFront(size_t k);

  uint8_t  m_rx[FRAME_MAX] = {0};
  size_t   m_n = 0;          // Bytes im Puffer
  size_t   m_plOff = 0;      // Beginn der Nutzlast im Puffer
  size_t   m_plLen = 0;      // Länge der Nutzlast
  size_t   m_pending = 0;    // nach Erfolg zu verwerfende Rahmenlänge
  uint32_t m_dropped = 0;
};

}  // namespace vesc
