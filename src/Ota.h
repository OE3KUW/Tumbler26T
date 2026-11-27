#ifndef Ota_h
#define Ota_h

// ===========================================================================
//  WLAN und Aktualisierung ueber Funk
//
//  Gehoert NICHT zum Original - das laeuft auf einem ATmega328, der weder das
//  eine noch das andere kann. Hier ist es dazugekommen, damit der Roboter
//  erreichbar bleibt: ohne OTA waere jede weitere Aenderung und auch der Weg
//  zurueck zur anderen Firmware nur noch mit dem USB-Kabel moeglich.
//
//  An der Regelung aendert das nichts. WLAN und Webserver laufen auf Kern 0,
//  die Regelung in ihrer eigenen Task auf Kern 1.
// ===========================================================================

#include <esp_bt.h>

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "secrets.h"

#define OTA_HOSTNAME "tumbler-orig"

void otaBegin()
{
  // BLE wird hier nicht gebraucht - nur Bluetooth Classic. Den Speicher des
  // BLE-Teils gleich freigeben, das sind rund 50 KB Arbeitsspeicher.
  esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print(F("WLAN verbinde"));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000)
  {
    delay(250);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print(F("\nWLAN verbunden, IP="));
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println(F("\nkein WLAN - laeuft trotzdem, nur ohne Funkupdate"));
    return;
  }

  MDNS.begin(OTA_HOSTNAME);

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    // Endstufen aus, bevor der Flash beschrieben wird - sonst faehrt er
    // waehrend des Updates unkontrolliert weiter.
    motion_mode = STOP;
    digitalWrite(STBY_PIN, LOW);

    // Die Regelung schlafen legen. Sie laeuft mit Prioritaet 5 alle 5 ms,
    // das Update im Hauptprogramm mit Prioritaet 1 - bei einem 1,7-MB-Abbild
    // kam es dadurch nicht mehr durch und brach reproduzierbar bei 11 % ab.
    // Die Motoren sind ohnehin aus, und gleich wird neu gestartet.
    if (balance_task) vTaskSuspend(balance_task);

    Serial.println(F("OTA-Update startet"));
  });

  // Bricht das Update ab, bliebe die Regelung sonst fuer immer schlafen -
  // der Roboter waere dann zwar erreichbar, wuerde aber nie wieder regeln.
  // Ein Neustart ist hier der ehrlichste Ausweg: das alte Abbild ist noch
  // vollstaendig, es startet einfach wieder.
  ArduinoOTA.onError([](ota_error_t) {
    Serial.println(F("OTA fehlgeschlagen - Neustart"));
    delay(50);
    ESP.restart();
  });

  ArduinoOTA.begin();
}

inline void otaLoop()
{
  ArduinoOTA.handle();
}

#endif
