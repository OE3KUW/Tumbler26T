#ifndef Bt_h
#define Bt_h

// ===========================================================================
//  Bedienung über Bluetooth
//
//  Das Original spricht mit einem HC-05-Modul über die serielle
//  Schnittstelle und liest dort dieselben Tastenzeichen. Auf dem ESP32
//  braucht es dafür kein Modul: Bluetooth Classic steckt im Chip.
//  BluetoothSerial ist der direkte Ersatz - die Zeichen landen in derselben
//  key_value wie die von Konsole, Weboberfläche und Taster.
//
//  Classic (SPP) und nicht BLE, weil damit jede beliebige
//  "Serial Bluetooth Terminal"-App unter Android funktioniert, ohne dass man
//  etwas Eigenes schreiben muss. iPhones können SPP nicht - dort bräuchte es
//  BLE und eine passende App.
//
//  WICHTIG: WLAN und Bluetooth teilen sich eine Antenne. Am eigenen Aufbau
//  gemessen, mit dauerhaft laufendem Bluetooth:
//
//      Ping   5 ms  ->  41 bis 84 ms
//      OTA    lief zuverlaessig  ->  bricht reproduzierbar bei 8 % ab
//
//  Die Aktualisierung ueber Funk war damit unbrauchbar - und ohne Kabel kam
//  man an den Roboter nicht mehr heran, um es zurueckzunehmen. Deshalb wird
//  Bluetooth NICHT mehr beim Start eingeschaltet, sondern nur auf Wunsch
//  ("BT=1"), und laesst sich jederzeit wieder abschalten ("BT=0").
//
//  Die Regelung merkt von alledem nichts, sie laeuft auf Kern 1 in ihrer
//  eigenen Task.
// ===========================================================================

#define BT_ENABLED 1
#define BT_NAME    "Tumbler26"

#if BT_ENABLED

#include <BluetoothSerial.h>

BluetoothSerial bt;
int bt_state = 0;   // 0 = nicht gestartet, 1 = bereit, 2 = verbunden

// Wird beim Start NICHT mehr aufgerufen - siehe oben.
inline bool btOn()
{
  if (bt_state != 0) return true;
  if (bt.begin(BT_NAME))
  {
    bt_state = 1;
    Serial.print(F("Bluetooth an als \""));
    Serial.print(F(BT_NAME));
    Serial.println(F("\""));
    return true;
  }
  Serial.println(F("Bluetooth liess sich nicht starten"));
  return false;
}

inline void btOff()
{
  if (bt_state == 0) return;
  bt.end();
  bt_state = 0;
  Serial.println(F("Bluetooth aus - OTA ist wieder zuverlaessig"));
}

inline void btBegin() { /* bewusst leer: Bluetooth startet erst auf Wunsch */ }

// Liefert ein empfangenes Zeichen oder 0. Zeilenenden werden verworfen, damit
// es sich genau wie die serielle Konsole verhält.
inline char btPoll()
{
  if (bt.available())
  {
    char c = bt.read();
    if (c != '\n' && c != '\r') return c;
  }
  return '\0';
}

inline void btPrint(const String &line)
{
  if (bt.hasClient()) bt.println(line);
}

inline bool btConnected() { return bt.hasClient(); }
inline int  btState() { return bt_state == 0 ? 0 : (bt.hasClient() ? 2 : 1); }

#else

inline void btBegin() {}
inline bool btOn()  { return false; }
inline void btOff() {}
inline char btPoll() { return '\0'; }
inline void btPrint(const String &) {}
inline bool btConnected() { return false; }
inline int  btState() { return -1; }

#endif
#endif
