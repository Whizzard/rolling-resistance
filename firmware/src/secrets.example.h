#pragma once
// Template for firmware/src/secrets.h (git-ignored). Copy, enter the password,
// then regenerate the QR codes (docs/04-hardware.md §13.4):
//   cp firmware/src/secrets.example.h firmware/src/secrets.h
// WPA2 needs at least 8 characters; with fewer the access point stays OPEN, and
// anyone in range can drive the motor (webui.cpp checks sizeof > 8).
constexpr char WEBUI_PASSWORD[] = "changeme";
