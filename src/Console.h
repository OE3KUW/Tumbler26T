#ifndef Console_h
#define Console_h

// ===========================================================================
//  Bedienung und Telemetrie ueber WLAN
//
//  Gehoert NICHT zum Original. Der Grund ist praktisch: ein Balancierroboter
//  faehrt herum, und ein USB-Kabel loest sich dabei staendig oder verhindert
//  den Versuch ganz. Genau das hat hier mehrfach eine Messung abgebrochen.
//
//  Es ist bewusst nur ein roher TCP-Anschluss auf Port 23, kein Webserver:
//  Alles, was ueber die serielle Konsole geht, geht auch hier - dieselben
//  Tastenzeichen hinein, dieselben Zeilen zurueck.
//
//      nc tumbler-orig.local 23
//
//  An der Regelung aendert das nichts, der Anschluss wird aus loop() bedient.
// ===========================================================================

#include <WiFi.h>

WiFiServer consoleServer(23);
WiFiClient consoleClient;

inline void consoleBegin()
{
  consoleServer.begin();
  consoleServer.setNoDelay(true);
}

// Liefert ein empfangenes Zeichen oder 0. Zeilenenden werden verworfen, damit
// sich der Anschluss genau wie die serielle Konsole verhaelt.
inline char consolePoll()
{
  if (!consoleClient || !consoleClient.connected())
  {
    WiFiClient next = consoleServer.available();
    if (next) consoleClient = next;
  }

  if (consoleClient && consoleClient.available())
  {
    char c = consoleClient.read();
    if (c != '\n' && c != '\r') return c;
  }
  return '\0';
}

// Dieselbe Zeile, die auf der seriellen Konsole erscheint, auch ueber WLAN.
inline void consolePrint(const String &line)
{
  if (consoleClient && consoleClient.connected())
    consoleClient.println(line);
}

#endif
