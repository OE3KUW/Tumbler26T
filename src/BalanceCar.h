#ifndef BalanceCar_h
#define BalanceCar_h

// ===========================================================================
//  Regelung, uebernommen aus OE3KUW/TumbllerVSC (Yahboom Tumbller)
//
//  Attribution:
//    https://github.com/OE3KUW/TumbllerVSC
//    Siehe ATTRIBUTION.md fuer den Umfang der uebernommenen Teile.
//
//  Die Rechnung ist Zeile fuer Zeile die des Originals - gleiche Struktur,
//  gleiche Verstaerkungen, gleiche Grenzen. Geaendert wurde nur, was auf dem
//  ESP32 anders sein MUSS; jede dieser Stellen ist unten einzeln vermerkt.
//
//  Abweichungen vom Original und ihr Grund:
//
//   1. MsTimer2 -> FreeRTOS-Task alle 5 ms.
//      Das Original haengt balanceCar() in einen Timer-Interrupt. Auf dem
//      ESP32 geht das nicht: die Funktion liest den MPU6050 ueber I2C, und
//      der Wire-Treiber benutzt selbst Interrupts und Mutexe - aus einer ISR
//      heraus fuehrt das zum Absturz. Eine eigene Task mit vTaskDelayUntil
//      haelt denselben 5-ms-Takt und gibt dieselbe Garantie, dass die
//      Regelung nicht vom Hauptprogramm aufgehalten wird.
//
//   2. analogWrite -> LEDC.
//      Der ESP32 hat kein analogWrite in diesem Core. LEDC ist das
//      Gegenstueck. Die Frequenz liegt hier bei 20 kHz statt der rund 1 kHz
//      des AVR - oberhalb des Hoerbereichs, sonst pfeifen die Motoren.
//
//   3. EnableInterrupt -> attachInterrupt.
//      Die Bibliothek des Originals ist reiner AVR-Code (Pin-Change-Register).
//      Der ESP32 kann ohnehin jeden Pin unterbrechen.
//
//   4. Motor rechts umgekehrt (MOTOR_RIGHT_INVERT).
//      Auf dieser Platine sitzen die Motoren spiegelbildlich. Ohne die
//      Umkehr drehen die Raeder gegeneinander und der Roboter dreht sich im
//      Kreis, statt zu fahren. Beim Original passt die Verdrahtung.
//
//  Nicht geaendert, obwohl es sich anboete - das waere dann nicht mehr das
//  Original: die Encoder werden weiter nur einspurig gezaehlt und ihre
//  Richtung aus dem Vorzeichen des PWM geraten, und der Nullpunkt des Gyros
//  steckt fest im Kalman-Filter (Gyro_x = (gx - 128.1) / 131).
// ===========================================================================

#include "KalmanFilter.h"
#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include "Pins.h"

MPU6050 mpu;
KalmanFilter kalmanfilter;

// --- LEDC statt analogWrite (Abweichung 2) ---
#define PWM_CH_LEFT   0
#define PWM_CH_RIGHT  1
#define PWM_FREQ      20000
#define PWM_BITS      8

static inline void motorWrite(int channel, int value)
{
  ledcWrite(channel, value);
}

// Setting PID parameters
double kp_balance = 55, kd_balance = 0.75;
double kp_speed = 10, ki_speed = 0.26;
double kp_turn = 2.5, kd_turn = 0.5;

// Setting MPU6050 calibration parameters
double angle_zero = 0;            // x axle angle calibration
double angular_velocity_zero = 0; // x axle angular velocity calibration

// Beide Werte stehen im Original im Regelgesetz, werden dort aber nirgends
// gesetzt - sie bleiben auf null. Damit regelt er auf die Senkrechte des
// SENSORS, nicht auf die Lage, in der sein Schwerpunkt wirklich ueber der
// Achse steht. Die beiden unterscheiden sich um den Einbauwinkel des GY-521
// und um die Gewichtsverteilung; die Folge ist ein dauerhaftes Wegdriften.
//
// ZERO fuellt sie: es mittelt eine halbe Sekunde lang und uebernimmt das
// Ergebnis. Bewusst ohne Warteschleife - die Regelung darf dabei nicht
// stehenbleiben, also zaehlt der Regeltakt selbst mit.
volatile uint16_t zero_todo = 0;   // wieviele Messungen noch fehlen
volatile int8_t   zero_result = 0; // 1 = uebernommen, -1 = verworfen, 0 = laeuft
double zero_acc_angle = 0;
double zero_acc_rate  = 0;
volatile double zero_last_angle = 0, zero_last_rate = 0;
constexpr uint16_t ZERO_SAMPLES = 100;   // 100 * 5 ms = 0,5 s

volatile unsigned long encoder_count_right_a = 0;
volatile unsigned long encoder_count_left_a = 0;
int16_t ax, ay, az, gx, gy, gz;
float dt = 0.005, Q_angle = 0.001, Q_gyro = 0.005, R_angle = 0.5, C_0 = 1, K1 = 0.05;

int encoder_left_pulse_num_speed = 0;
int encoder_right_pulse_num_speed = 0;
double speed_control_output = 0;
double rotation_control_output = 0;

// Aufintegrierte Blickrichtung, Grad. Die Vorlage kennt sie nicht: sie
// daempft mit kd_turn nur die DrehRATE. Das bremst das Verdrehen, holt ihn
// aber nie zurueck, weil nirgends steht, wo er einmal hingeschaut hat.
// Bezeichnend: kp_turn ist im Original deklariert und wird nirgends benutzt -
// der Winkelanteil war offenbar vorgesehen und fehlt. Genau den ergaenzt das
// hier, unter demselben Namen.
double yaw_angle = 0;
double speed_filter = 0;
int speed_control_period_count = 0;
double car_speed_integeral = 0;
double speed_filter_old = 0;
int setting_car_speed = 0;
int setting_turn_speed = 0;
double pwm_left = 0;
double pwm_right = 0;
float kalmanfilter_angle;

// Welcher Filter den Regler speist. Beide rechnen immer mit, umschaltbar auf
// der Weboberflaeche - in der abgestimmten Fassung war der Komplementaerfilter
// messbar ruhiger (0,566 gegen 0,633 Grad), das laesst sich hier nachpruefen.
bool use_kalman = true;
// Das Original nimmt +-22 Grad. Diese Annahme haengt daran, dass ein
// umgefallenes Fahrzeug rund 90 Grad meldet und damit sicher ausserhalb liegt.
// Auf diesem Aufbau stimmt das NICHT: liegend misst er 17,7 bis 18,9 Grad,
// also innerhalb des Fensters. Die Vorlage haelt ihn dann fuer aufrecht und
// regelt mit 55 * 17,8 = 979, gekappt auf volle 255 - er faehrt los, obwohl
// er liegt. Deshalb hier enger; BALANCE_WINDOW 22 stellt das Original her.
#define BALANCE_WINDOW 14
// ACHTUNG, Portierungsfalle: das Original schreibt hier char. Auf dem AVR ist
// char vorzeichenBEHAFTET, auf dem Xtensa des ESP32 vorzeichenLOS. Aus -14
// wurde dadurch 242, und die Pruefung "242 <= Winkel" war immer falsch: jeder
// Startversuch landete im STOP-Zweig, STANDBY wurde nie erreicht. Der Roboter
// hat deshalb nur im Uebergangszustand START geregelt und nach zwei Sekunden
// abgeschaltet - bei kerzengeradem Winkel, was zunaechst unerklaerlich aussah.
int8_t balance_angle_min = -BALANCE_WINDOW;
int8_t balance_angle_max = BALANCE_WINDOW;

volatile uint8_t stop_reason = 0;   // 1 = Sturzpruefung, 2 = Startpruefung
volatile float   stop_angle  = 0;   // Winkel im Moment der Abschaltung

void carStop()
{
  digitalWrite(AIN1, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(STBY_PIN, HIGH);
  motorWrite(PWM_CH_LEFT, 0);
  motorWrite(PWM_CH_RIGHT, 0);
}

void carForward(unsigned char speed)
{
  digitalWrite(AIN1, 0);
  digitalWrite(BIN1, MOTOR_RIGHT_INVERT ? 1 : 0);
  motorWrite(PWM_CH_LEFT, speed);
  motorWrite(PWM_CH_RIGHT, speed);
}

void carBack(unsigned char speed)
{
  digitalWrite(AIN1, 1);
  digitalWrite(BIN1, MOTOR_RIGHT_INVERT ? 0 : 1);
  motorWrite(PWM_CH_LEFT, speed);
  motorWrite(PWM_CH_RIGHT, speed);
}

void balanceCar()
{
  encoder_left_pulse_num_speed += pwm_left < 0 ? -encoder_count_left_a : encoder_count_left_a;
  encoder_right_pulse_num_speed += pwm_right < 0 ? -encoder_count_right_a : encoder_count_right_a;
  encoder_count_left_a = 0;
  encoder_count_right_a = 0;

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  kalmanfilter.Angle(ax, ay, az, gx, gy, gz, dt, Q_angle, Q_gyro, R_angle, C_0, K1);

  // Richtung mitfuehren. Fuer die Hochachse gibt es keinen zweiten Sensor,
  // der den Nullpunkt nachzieht - die reine Integration driftet also. Ein
  // Restfehler von 0,1 Grad/s waeren 6 Grad je Minute, und der Regler wuerde
  // diese eingebildete Drehung real wegdrehen. Deshalb laeuft der Wert mit
  // 30 s Zeitkonstante gegen null zurueck: echte Verdrehungen faengt der
  // Regler in Sekundenbruchteilen ab und merkt vom Leck nichts, nur das
  // Aufsummieren ueber Minuten wird gebremst.
  // Laeuft gerade ein ZERO, hier mitzaehlen.
  if (zero_todo > 0)
  {
    zero_acc_angle += kalmanfilter.angle;
    zero_acc_rate  += kalmanfilter.Gyro_x;
    if (--zero_todo == 0)
    {
      const double a = zero_acc_angle / ZERO_SAMPLES;
      const double g = zero_acc_rate  / ZERO_SAMPLES;
      zero_last_angle = a;
      zero_last_rate  = g;

      // Auch am ENDE pruefen, nicht nur beim Druecken. Ein Balancierer faellt
      // um, sobald die Motoren aus sind - die Mittelung erwischt ihn dann
      // mitten im Kippen. Gemessen wurden so 13,2 Grad und 22 Grad/s, obwohl
      // beim Start des Vorgangs noch alles im Rahmen war.
      if (fabs(a) > 10.0 || fabs(g) > 20.0)
      {
        zero_result = -1;          // verwerfen, alte Werte bleiben stehen
      }
      else
      {
        angle_zero = a;
        angular_velocity_zero = g;
        zero_result = 1;
      }
    }
  }

  yaw_angle += kalmanfilter.Gyro_z * dt;
  yaw_angle -= yaw_angle * (dt / 30.0);

  // Wird gewollt gedreht, ist die aktuelle Richtung die neue Sollrichtung -
  // sonst zoege er nach dem Loslassen wieder auf den alten Kurs zurueck.
  if (setting_turn_speed != 0) yaw_angle = 0;
  kalmanfilter_angle = use_kalman ? kalmanfilter.angle : kalmanfilter.angle6;
  double balance_control_output = kp_balance * (kalmanfilter_angle - angle_zero) +
                                  kd_balance * (kalmanfilter.Gyro_x - angular_velocity_zero);

  speed_control_period_count++;
  if (speed_control_period_count >= 8)
  {
    speed_control_period_count = 0;
    double car_speed = (encoder_left_pulse_num_speed + encoder_right_pulse_num_speed) * 0.5;
    encoder_left_pulse_num_speed = 0;
    encoder_right_pulse_num_speed = 0;
    speed_filter = speed_filter_old * 0.7 + car_speed * 0.3;
    speed_filter_old = speed_filter;
    car_speed_integeral += speed_filter;
    car_speed_integeral += -setting_car_speed;
    car_speed_integeral = constrain(car_speed_integeral, -3000, 3000);
    speed_control_output = -kp_speed * speed_filter - ki_speed * car_speed_integeral;
    // kd_turn daempft die Drehrate (wie im Original), kp_turn holt die
    // Richtung zurueck (neu). Das Vorzeichen ist dasselbe wie beim
    // Daempfungsanteil - der arbeitet nachweislich richtig herum, also muss
    // es nicht geraten werden.
    rotation_control_output = setting_turn_speed
                            + kd_turn * kalmanfilter.Gyro_z
                            + kp_turn * yaw_angle;
  }

  pwm_left = balance_control_output - speed_control_output - rotation_control_output;
  pwm_right = balance_control_output - speed_control_output + rotation_control_output;

  pwm_left = constrain(pwm_left, -255, 255);
  pwm_right = constrain(pwm_right, -255, 255);

  if (motion_mode != START && motion_mode != STOP &&
      (kalmanfilter_angle < balance_angle_min || balance_angle_max < kalmanfilter_angle))
  {
    stop_reason = 1;
    stop_angle  = kalmanfilter_angle;
    motion_mode = STOP;
    carStop();
  }

  if (motion_mode == STOP)
  {
    car_speed_integeral = 0;
    setting_car_speed = 0;
    pwm_left = 0;
    pwm_right = 0;
    carStop();
  }
  else
  {
    // Richtungspin und Betrag - beim Original identisch, nur mit LEDC statt
    // analogWrite und mit der Umkehr fuer den spiegelbildlichen Motor.
    if (pwm_left < 0)
    {
      digitalWrite(AIN1, 1);
      motorWrite(PWM_CH_LEFT, (int)-pwm_left);
    }
    else
    {
      digitalWrite(AIN1, 0);
      motorWrite(PWM_CH_LEFT, (int)pwm_left);
    }

    const bool right_back = MOTOR_RIGHT_INVERT ? (pwm_right >= 0) : (pwm_right < 0);
    digitalWrite(BIN1, right_back ? 1 : 0);
    motorWrite(PWM_CH_RIGHT, (int)(pwm_right < 0 ? -pwm_right : pwm_right));
  }
}

void IRAM_ATTR encoderCountRightA()
{
  encoder_count_right_a++;
}

void IRAM_ATTR encoderCountLeftA()
{
  encoder_count_left_a++;
}

// Abweichung 1: eigene Task statt Timer-Interrupt.
volatile uint32_t balance_us = 0;   // wie lange ein Regelschritt wirklich braucht

TaskHandle_t balance_task = nullptr;

void balanceTask(void *arg)
{
  const TickType_t period = pdMS_TO_TICKS(5);
  TickType_t last_wake = xTaskGetTickCount();
  for (;;)
  {
    const uint32_t t0 = micros();
    balanceCar();
    balance_us = micros() - t0;
    vTaskDelayUntil(&last_wake, period);
  }
}

void carInitialize()
{
  pinMode(AIN1, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(STBY_PIN, OUTPUT);

  ledcSetup(PWM_CH_LEFT, PWM_FREQ, PWM_BITS);
  ledcSetup(PWM_CH_RIGHT, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PWMA_LEFT, PWM_CH_LEFT);
  ledcAttachPin(PWMB_RIGHT, PWM_CH_RIGHT);

  carStop();
  Serial.println(F("  [init] Motoren/LEDC bereit"));

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  // Ohne Zeitgrenze bleibt der ESP32-Wire-Treiber haengen, wenn der Bus
  // klemmt - das Original hat das Problem nicht, dessen AVR-Wire gibt auf.
  Wire.setTimeOut(50);

  Serial.print(F("  [init] I2C-Scan:"));
  for (uint8_t a = 1; a < 127; a++)
  {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.print(F(" 0x")); Serial.print(a, HEX); }
  }
  Serial.println();
  Serial.println(F("  [init] I2C bereit, MPU6050 wird angesprochen ..."));
  mpu.initialize();
  Serial.print(F("  [init] MPU6050 antwortet: "));
  Serial.println(mpu.testConnection() ? F("ja") : F("NEIN"));

  pinMode(ENCODER_LEFT_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_RIGHT_A_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A_PIN), encoderCountLeftA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_A_PIN), encoderCountRightA, CHANGE);

  // Auf Kern 1, damit WLAN und Hauptschleife auf Kern 0 nichts verzoegern.
  Serial.println(F("  [init] Encoder-Interrupts gesetzt, Regeltask startet"));
  xTaskCreatePinnedToCore(balanceTask, "balance", 4096, NULL, 5, &balance_task, 1);
}

#endif
