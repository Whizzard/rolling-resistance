#pragma once
// Vorlage fuer firmware/src/secrets.h (git-ignoriert). Kopieren, Kennwort eintragen,
// danach die QR-Codes neu erzeugen (docs/04-hardware.md §13.4):
//   cp firmware/src/secrets.example.h firmware/src/secrets.h
// WPA2 braucht mindestens 8 Zeichen; mit weniger bleibt der Accesspoint OFFEN, und
// jeder in Reichweite kann den Motor fahren (webui.cpp prueft sizeof > 8).
constexpr char WEBUI_PASSWORD[] = "changeme";
