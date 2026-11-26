#!/usr/bin/env python3
"""VESC packet framing for the tools in this folder: CRC-16/XMODEM, frame,
receiver. Until 2026-11-25 this existed three times (vesc_log, vesc_console, vesc_backup).

Frame: 0x02 <len8> | 0x03 <len16> , payload, CRC16 big-endian, 0x03.
On an error the receiver resynchronises from the next byte -- the same property
as firmware/src/vesc/packet.cpp, a corrupt byte costs at most one packet.
Self-test:  python3 tools/vescproto.py
"""
import struct

PAYLOAD_MAX = 512


def crc16(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(payload: bytes) -> bytes:
    n = len(payload)
    head = bytes([2, n]) if n <= 255 else bytes([3, n >> 8, n & 0xFF])
    return head + payload + struct.pack(">H", crc16(payload)) + b"\x03"


def unframe(buf: bytearray):
    """Yields complete payloads from buf and removes them from it (generator)."""
    while True:
        if not buf:
            return
        if buf[0] == 2:
            hdr = 2
            if len(buf) < 2:
                return
            ln = buf[1]
        elif buf[0] == 3:
            hdr = 3
            if len(buf) < 3:
                return
            ln = (buf[1] << 8) | buf[2]
        else:
            del buf[0]
            continue
        if ln == 0 or ln > PAYLOAD_MAX:
            del buf[0]
            continue
        total = hdr + ln + 3
        if len(buf) < total:
            return
        body = bytes(buf[hdr:hdr + ln])
        if buf[total - 1] == 3 and crc16(body) == struct.unpack(">H", buf[hdr + ln:hdr + ln + 2])[0]:
            del buf[:total]
            yield body
        else:
            del buf[0]


def unframe_first(raw: bytes):
    """The first valid payload in raw, otherwise None (for request-reply tools)."""
    for body in unframe(bytearray(raw)):
        return body
    return None


class Reader:
    """Buffers incoming bytes; feed() yields the complete payloads."""

    def __init__(self):
        self.buf = bytearray()

    def feed(self, data: bytes):
        self.buf += data
        yield from unframe(self.buf)


if __name__ == "__main__":
    assert crc16(b"123456789") == 0x31C3, "CRC-16/XMODEM check value"
    assert frame(b"\x00") == bytes([2, 1, 0, 0, 0, 3])
    big = bytes(range(256)) + bytes(range(44))
    f = frame(big)
    assert f[:3] == bytes([3, 1, 44]) and list(unframe(bytearray(b"\xff\xaa" + f + f))) == [big, big]
    r = Reader()
    assert list(r.feed(frame(b"\x05\x06")[:3])) == [] and list(r.feed(frame(b"\x05\x06")[3:])) == [b"\x05\x06"]
    assert unframe_first(b"\x00" + frame(b"\x07")) == b"\x07" and unframe_first(b"\x02\x01") is None
    print("vescproto: ok")
