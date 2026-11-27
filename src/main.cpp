// ===========================================================================
//  Tumbller auf dem ESP32
//
//  Portierung von OE3KUW/TumbllerVSC (Arduino Uno, ATmega328) auf die Platine
//  "tumlerV2" mit ESP32-WROOM-32E. Die Regelung ist unveraendert uebernommen,
//  siehe BalanceCar.h - dort steht auch, was auf dem ESP32 anders sein musste
//  und warum.
//
//  Weggelassen, weil die Hardware es nicht hat:
//    Infrarot-Fernbedienung, Batteriespannungsmessung.
//  Bluetooth waere moeglich - der ESP32-WROOM-32E kann Classic und BLE -,
//  ist aber nicht portiert: dieselben Tastenzeichen kommen ueber WLAN.
//  Ihre Rolle uebernimmt die serielle Konsole: dieselben Tastenzeichen, die
//  das Original von der Fernbedienung bekommt, koennen hier getippt werden.
//
//    s  Stillstand      f  vorwaerts     b  rueckwaerts
//    l  links           i  rechts        5  starten
//
//  Der Taster an GPIO2 tut dasselbe wie 's' bzw. '5'.
//
//  Attribution:
//    Portiert aus OE3KUW/TumbllerVSC:
//    https://github.com/OE3KUW/TumbllerVSC
//    Siehe ATTRIBUTION.md fuer den Umfang der uebernommenen Teile.
// ===========================================================================

#include <Arduino.h>
#include "Pins.h"
#include "mode.h"
#include "BalanceCar.h"
#include "Ota.h"       // nicht im Original, siehe dort
#include "Console.h"   // ebenso wenig - Bedienung ueber WLAN statt Kabel
#include "WebUi.h"     // und auch nicht - Bedienseite im Browser (bindet Bt.h ein)
#include "Rgb.h"       // Statusanzeige, Methodennamen wie in der Vorlage

RGB rgb;

unsigned long start_prev_time = 0;
char key_value = '\0';

// Die Vorlage kennt STOP nur als kurzen Zwischenzustand: eine Sekunde spaeter
// schaltet setMotionState() von selbst nach STANDBY zurueck, sobald er
// aufrecht steht. Das ist gewollt - so faengt er sich nach einem Sturz von
// allein wieder. Ein bewusstes Anhalten gibt es dort aber gar nicht, und
// damit auch keinen Stillstand, in dem sich der Nullpunkt messen liesse.
//
// halted unterscheidet beides: von Hand angehalten bleibt angehalten, ein
// Sturz nicht.
bool halted = false;
uint32_t button_spikes = 0;   // wie oft GPIO2 nur kurz zuckte
char key_flag = '\0';

// Das Original liest hier eine Infrarot-Fernbedienung. Auf dieser Platine
// gibt es keinen Empfaenger, also kommen die Zeichen ueber die Konsole.
void getKeyValue()
{
  if (Serial.available())
  {
    char c = Serial.read();
    if (c != '\n' && c != '\r')
      key_value = c;
  }

  // Dieselben Zeichen nehmen wir auch ueber WLAN und Bluetooth entgegen.
  const char net = consolePoll();
  if (net) key_value = net;
  const char air = btPoll();
  if (air) key_value = air;

  // Taster: auf dieser Platine aktiv HIGH (externer Pulldown an GPIO2, weil
  // es ein Strapping-Pin ist). Das Original fragt seinen Taster aktiv LOW ab.
  //
  // Eine reine Flankenauswertung reicht hier nicht. GPIO2 haengt nur an einem
  // schwachen externen Pulldown und liegt neben den Motorleitungen, die mit
  // 20 kHz schalten. Eingestreute Spitzen wurden dadurch als Tastendruck
  // gewertet und haben den Regler mitten im Balancieren neu gestartet - im
  // Protokoll sah das aus wie STOP -> START ohne erkennbaren Anlass.
  // Deshalb muss der Pin durchgehend HIGH sein, nicht nur einmal.
  static bool     last_state = false;
  static uint32_t high_since = 0;
  static uint32_t spikes = 0;
  const bool now_high = digitalRead(KEY_MODE) == HIGH;

  if (now_high)
  {
    if (high_since == 0) high_since = millis();
    if (!last_state && millis() - high_since >= 30)
    {
      last_state = true;
      // Wechselschalter: steht er, geht er los - laeuft er, haelt er an.
      //
      // Das Original schickt hier 's', also STANDBY, und das heisst "auf der
      // Stelle balancieren" - der Taster konnte ihn damit gar nicht anhalten.
      key_value = (motion_mode == STOP) ? '5' : 'x';
    }
  }
  else
  {
    if (high_since != 0 && !last_state) spikes++;   // war HIGH, aber zu kurz
    high_since = 0;
    last_state = false;
  }
  button_spikes = spikes;
}

void setMotionState()
{
  switch (motion_mode)
  {
  case FORWARD:
    setting_car_speed = 40;
    setting_turn_speed = 0;
    break;
  case BACKWARD:
    setting_car_speed = -40;
    setting_turn_speed = 0;
    break;
  case TURNLEFT:
    setting_car_speed = 0;
    setting_turn_speed = 50;
    break;
  case TURNRIGHT:
    setting_car_speed = 0;
    setting_turn_speed = -50;
    break;
  case STANDBY:
    setting_car_speed = 0;
    setting_turn_speed = 0;
    break;
  case STOP:
    if (!halted && millis() - start_prev_time > 1000)
    {
      function_mode = IDLE;
      if (balance_angle_min <= kalmanfilter_angle && kalmanfilter_angle <= balance_angle_max)
      {
        motion_mode = STANDBY;
        rgb.lightOff();
      }
    }
    break;
  case START:
    if (millis() - start_prev_time > 2000)
    {
      if (balance_angle_min <= kalmanfilter_angle && kalmanfilter_angle <= balance_angle_max)
      {
        car_speed_integeral = 0;
        setting_car_speed = 0;
        motion_mode = STANDBY;
        rgb.lightOff();
      }
      else
      {
        stop_reason = 2;
        stop_angle  = kalmanfilter_angle;
        motion_mode = STOP;
        carStop();
        rgb.brightRedColor();
      }
    }
    break;
  default:
    break;
  }
}

void keyEventHandle()
{
  if (key_value == '\0')
    return;

  key_flag = key_value;
  switch (key_value)
  {
  case 's': motion_mode = STANDBY;   rgb.lightOff();  halted = false;  break;
  case 'f': motion_mode = FORWARD;   rgb.flashBlueColorFront();  break;
  case 'b': motion_mode = BACKWARD;  rgb.flashBlueColorback();   break;
  case 'l': motion_mode = TURNLEFT;  rgb.flashBlueColorLeft();   break;
  case 'i': motion_mode = TURNRIGHT; rgb.flashBlueColorRight();  break;
  case '5':
    motion_mode = START;
    start_prev_time = millis();
    halted = false;
    break;
  case 'z':
    // Aktuelle Lage als Senkrechte uebernehmen. Aendert den Betriebszustand
    // nicht - man kann also im Stand nullen oder im Lauf nachtrimmen.
    //
    // Aber nur nahe der Senkrechten: sonst uebernimmt er die Lage, in der er
    // gerade LIEGT, als Nullpunkt und regelt kuenftig auf 18 Grad Schraeglage.
    // Genau das ist beim ersten Test passiert.
    // Zwei Bedingungen, beide am eigenen Fehlversuch gelernt:
    //
    //  - Nahe der Senkrechten. Sonst uebernimmt er die Lage, in der er gerade
    //    LIEGT, als Nullpunkt und regelt kuenftig auf 18 Grad Schraeglage.
    //
    //  - Und im Stillstand. Waehrend er balanciert, bewegt er sich staendig;
    //    die Mittelung erwischt dann irgendeinen Punkt der Pendelbewegung.
    //    Beim Versuch kamen so -6,7 Grad und -69 Grad/s heraus - der
    //    Drehraten-Nullpunkt haette den D-Anteil dauerhaft um 69 Grad/s
    //    verschoben und den Regler unbrauchbar gemacht.
    if (motion_mode != STOP)
    {
      consolePrint("ZERO nur im Stillstand - erst STOP druecken");
    btPrint("ZERO nur im Stillstand - erst STOP druecken");
      webUiNotify("ZERO nur im Stillstand - erst STOP druecken");
      Serial.println(F("ZERO nur im Stillstand - erst STOP druecken"));
      break;
    }
    // 40 Grad/s statt 15: von Hand gehalten wackelt er mehr, und mit der
    // strengeren Grenze wurde praktisch jeder Versuch abgelehnt. Ob es ruhig
    // genug WAR, entscheidet ohnehin erst die Mittelung am Ende.
    if (fabs(kalmanfilter.angle) > 10.0 || fabs(kalmanfilter.Gyro_x) > 40.0)
    {
      String w = String("ZERO abgelehnt: ") + String(kalmanfilter.angle, 1) +
                 " Grad, " + String(kalmanfilter.Gyro_x, 0) +
                 " Grad/s - aufrecht und ruhig halten";
      Serial.println(w);
      consolePrint(w);
      btPrint(w);
      webUiNotify(w);
      break;
    }
    zero_acc_angle = 0;
    zero_acc_rate  = 0;
    zero_todo = ZERO_SAMPLES;
    Serial.println(F("ZERO: mittle eine halbe Sekunde, bitte ruhig halten"));
    consolePrint("ZERO: mittle eine halbe Sekunde, bitte ruhig halten");
    btPrint("ZERO: mittle eine halbe Sekunde, bitte ruhig halten");
    webUiNotify("ZERO: mittle eine halbe Sekunde, bitte ruhig halten");
    break;
  case 'x':
    // Nicht im Original: dort gibt es kein Halte-Kommando, auch der Taster
    // schaltet in STANDBY nur wieder auf STANDBY. Ohne das laesst sich ein
    // laufender Versuch nur durch Umkippen oder Ausschalten beenden.
    motion_mode = STOP;
    start_prev_time = millis();
    halted = true;          // bleibt aus, bis START oder STANDBY kommt
    carStop();
    rgb.brightRedColor();
    consolePrint("angehalten - START oder STANDBY setzt ihn wieder in Gang");
    btPrint("angehalten - START oder STANDBY setzt ihn wieder in Gang");
    webUiNotify("angehalten - START oder STANDBY setzt ihn wieder in Gang");
    break;
  default:
    break;
  }
  if (key_flag == key_value)
    key_value = '\0';
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println(F("Tumbller (Portierung von TumbllerVSC) auf ESP32"));
  Serial.println(F("s Stillstand  f vor  b zurueck  l links  i rechts  5 starten"));

  rgb.initialize();

  pinMode(KEY_MODE, INPUT);   // KEIN Pullup - externer Pulldown, aktiv HIGH

  // WICHTIG, und beim ersten Anlauf falsch herum gewesen:
  //
  // mode.h setzt motion_mode beim Start auf START, und in diesem Zustand
  // schaltet balanceCar() bewusst die Sturzabschaltung ab und stellt die
  // Motoren. Beim Original macht das nichts, weil setup() nur Millisekunden
  // dauert und loop() sofort danach STOP setzt.
  //
  // otaBegin() sucht aber bis zu acht Sekunden nach dem WLAN. Stand es vor
  // carInitialize() nicht auf STOP, lief der Regler diese acht Sekunden lang
  // mit voller Stellgroesse und ohne Sturzabschaltung - bei einem liegenden
  // Roboter heisst das: Vollgas quer durchs Zimmer.
  //
  // Deshalb zuerst STOP setzen und das WLAN erledigen, und erst danach die
  // Regelung anwerfen.
  motion_mode = STOP;
  otaBegin();

  start_prev_time = millis();
  carInitialize();
  consoleBegin();
  webUiBegin();
  btBegin();
  rgb.brightRedColor();   // aus, aber sichtbar bereit
}

void loop()
{
  otaLoop();
  webUiLoop();
  getKeyValue();
  keyEventHandle();
  setMotionState();

  // Zustandswechsel sofort melden. Ein Abtasten alle 100 ms verpasst genau
  // die kurzen Uebergaenge, auf die es hier ankommt.
  rgb.setAngle(kalmanfilter_angle - angle_zero);
  rgb.blink(100);

  // Bluetooth umschalten, falls die Oberflaeche es angefordert hat.
  if (bt_request >= 0)
  {
    const int wish = bt_request;
    bt_request = -1;
    if (wish)
    {
      const bool ok = btOn();
      String m = ok ? String("Bluetooth an als \"") + BT_NAME +
                      "\" - Achtung: OTA wird dadurch unzuverlaessig"
                    : String("Bluetooth liess sich nicht starten");
      Serial.println(m); consolePrint(m); webUiNotify(m);
    }
    else
    {
      btOff();
      String m = "Bluetooth aus - OTA wieder zuverlaessig";
      Serial.println(m); consolePrint(m); webUiNotify(m);
    }
  }

  // Ergebnis der Nullung melden, sobald sie durch ist.
  static bool zero_was_running = false;
  if (zero_was_running && zero_todo == 0)
  {
    zero_was_running = false;
    String z;
    if (zero_result == 1)
      z = String("ZERO fertig: Winkel ") + String(angle_zero, 2) +
          " Grad, Drehrate " + String(angular_velocity_zero, 3) + " Grad/s";
    else
      z = String("ZERO verworfen: er war nicht ruhig genug (") +
          String((double)zero_last_angle, 1) + " Grad, " +
          String((double)zero_last_rate, 0) + " Grad/s) - alter Wert bleibt";
    Serial.println(z);
    consolePrint(z);
    btPrint(z);
    webUiNotify(z);
  }
  if (zero_todo > 0) { zero_was_running = true; zero_result = 0; }

  static MOTION_MODE last_mode = STOP;
  if (motion_mode != last_mode)
  {
    const char *mn2[] = {"STANDBY","FORWARD","BACKWARD","TURNLEFT","TURNRIGHT","STOP","START"};
    Serial.print(F(">> "));
    Serial.print(mn2[last_mode]);
    Serial.print(F(" -> "));
    Serial.print(mn2[motion_mode]);
    Serial.print(F("  bei "));
    Serial.print(kalmanfilter_angle);
    Serial.print(F(" Grad"));
    if (motion_mode == STOP)
    {
      Serial.print(F("   Grund: "));
      Serial.print(stop_reason == 1 ? F("Sturzpruefung") :
                   stop_reason == 2 ? F("Startpruefung") : F("keiner gesetzt"));
      Serial.print(F(" bei "));
      Serial.print(stop_angle);
      Serial.print(F(" Grad"));
    }
    Serial.println();

    String w = String(">> ") + mn2[last_mode] + " -> " + mn2[motion_mode] +
               "  bei " + String(kalmanfilter_angle, 2) + " Grad";
    if (motion_mode == STOP)
      w += String("   Grund: ") + (stop_reason == 1 ? "Sturzpruefung" :
                                   stop_reason == 2 ? "Startpruefung" : "keiner") +
           " bei " + String(stop_angle, 2);
    consolePrint(w);
    btPrint(w);
    stop_reason = 0;
    last_mode = motion_mode;
  }

  static unsigned long print_time;
  if (millis() - print_time > 100)
  {
    print_time = millis();
    const char *mn[] = {"STANDBY","FORWARD","BACKWARD","TURNLEFT","TURNRIGHT","STOP","START"};
    Serial.print(F("stoer:")); Serial.print(button_spikes); Serial.print('\t');
    Serial.print(mn[motion_mode]);
    Serial.print('\t');
    Serial.print(balance_us);
    Serial.print(F("us\t"));
    Serial.print(kalmanfilter.angle);
    Serial.print('\t');
    Serial.print(pwm_left);
    Serial.print('\t');
    Serial.println(pwm_right);

    btPrint(String(mn[motion_mode]) + "  " + String(kalmanfilter.angle, 2) +
            " Grad   pwm " + String(pwm_left, 0) + "/" + String(pwm_right, 0));
    consolePrint(String(mn[motion_mode]) + "\t" + String(balance_us) + "us\t" +
                 String(kalmanfilter.angle, 2) + "\t" + String(pwm_left, 2) +
                 "\t" + String(pwm_right, 2));
    webUiSend();
  }

  // Das Original faengt zwei Sekunden nach dem Einschalten von selbst an zu
  // balancieren. Hier ist das ab Werk aus: der Ruhewinkel dieses Roboters
  // liegt innerhalb des Balancierfensters, er wuerde also nach jedem Neustart
  // im Liegen mit voller Stellgroesse losfahren. Mit AUTO_START 1 verhaelt er
  // sich wie die Vorlage.
#define AUTO_START 0
  static unsigned long start_time = millis();
  if (millis() - start_time < 10)
  {
    function_mode = IDLE;
    motion_mode = STOP;
    carStop();
  }
#if AUTO_START
  if (millis() - start_time > 2000 && key_flag == '\0')
  {
    key_value = '5';
    key_flag = '5';
  }
#endif

  delay(5);
}
