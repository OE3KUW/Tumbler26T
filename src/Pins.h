#ifndef Pins_h
#define Pins_h

// Pinbelegung der Platine "tumlerV2" (ESP32-WROOM-32E).
//
// Attribution:
//   Namen und Pinzuordnung sind aus OE3KUW/TumbllerVSC portiert:
//   https://github.com/OE3KUW/TumbllerVSC
//   Siehe ATTRIBUTION.md.
//
// Ersetzt die Pins.h des Originals, die auf einen ATmega328 zeigte. Die Namen
// sind absichtlich unveraendert uebernommen, damit der restliche Quelltext
// Zeile fuer Zeile mit der Vorlage vergleichbar bleibt.
//
// Original (Arduino Uno):        hier (ESP32/tumlerV2):
//   AIN1              7            16
//   PWMA_LEFT         5             4
//   BIN1             12            23
//   PWMB_RIGHT        6            18
//   STBY_PIN          8            17
//   ENCODER_LEFT_A    2            19
//   ENCODER_RIGHT_A   4            27
//   RGB_PIN           3            15
//   TRIG_PIN         11             5
//   ECHO_PIN         A3            12
//   KEY_MODE         10             2

#define AIN1                  16
#define PWMA_LEFT              4
#define BIN1                  23
#define PWMB_RIGHT            18
#define STBY_PIN              17

#define ENCODER_LEFT_A_PIN    19
#define ENCODER_RIGHT_A_PIN   27

#define RGB_PIN               15
#define NUMPIXELS              8    // die Platine hat 8 statt 4

#define TRIG_PIN               5
#define ECHO_PIN              12

// GPIO2 ist Strapping-Pin: externer Pulldown, Taster schaltet gegen 3V3.
// Er ist damit aktiv HIGH - das Original fragt seinen Taster aktiv LOW ab.
#define KEY_MODE               2

#define PIN_SDA               21
#define PIN_SCL               22

// Die beiden Motoren sitzen auf dieser Platine spiegelbildlich: dieselbe
// Ansteuerung wuerde die Raeder gegeneinander drehen lassen. Beim Original
// gibt es das nicht, dort passt die Verdrahtung. Ohne diese Umkehr dreht sich
// der Roboter im Kreis, statt zu fahren.
#define MOTOR_RIGHT_INVERT     1

// Nicht bestueckt bzw. nicht vorhanden gegenueber dem Original:
//   RECV_PIN / IR_SEND_PIN   - keine Infrarot-Fernbedienung
//   VOL_MEASURE_PIN          - keine Batteriespannungsmessung
//   LEFT/RIGHT_RECEIVE_PIN   - stattdessen QTR-8A an 36,39,32,33,34,35

#endif
