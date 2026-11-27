#include "KalmanFilter.h"

// Attribution:
//   Portiert aus OE3KUW/TumbllerVSC:
//   https://github.com/OE3KUW/TumbllerVSC
//   Siehe ATTRIBUTION.md fuer den Umfang der uebernommenen Teile.


void KalmanFilter::Yiorderfilter(float angle_m, float gyro_m,float dt,float K1) {
  angle6 = K1 * angle_m + (1 - K1) * (angle6 + gyro_m * dt);
}

void KalmanFilter::Kalman_Filter(double angle_m, double gyro_m,float dt,float Q_angle,float Q_gyro,float R_angle,float C_0) {
  angle += (gyro_m - q_bias) * dt;
  angle_err = angle_m - angle;
  Pdot[0] = Q_angle - P[0][1] - P[1][0];
  Pdot[1] = - P[1][1];
  Pdot[2] = - P[1][1];
  Pdot[3] = Q_gyro;
  P[0][0] += Pdot[0] * dt;
  P[0][1] += Pdot[1] * dt;
  P[1][0] += Pdot[2] * dt;
  P[1][1] += Pdot[3] * dt;
  PCt_0 = C_0 * P[0][0];
  PCt_1 = C_0 * P[1][0];
  E = R_angle + C_0 * PCt_0;
  K_0 = PCt_0 / E;
  K_1 = PCt_1 / E;
  t_0 = PCt_0;
  t_1 = C_0 * P[0][1];
  P[0][0] -= K_0 * t_0;
  P[0][1] -= K_0 * t_1;
  P[1][0] -= K_1 * t_0;
  P[1][1] -= K_1 * t_1;
  angle += K_0 * angle_err;
  q_bias += K_1 * angle_err;
  angle_dot = gyro_m - q_bias;
}

void KalmanFilter::Angle(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz, float dt, float Q_angle, float Q_gyro, float R_angle, float C_0, float K1) {
  float Angle = atan2(ay , az) * 57.3;
  Gyro_x = (gx - 128.1) / 131;
  Kalman_Filter(Angle, Gyro_x, dt, Q_angle, Q_gyro, R_angle, C_0);

  // ESP32-Fassung: Yiorderfilter() ist im Original deklariert, implementiert
  // und wird nirgends aufgerufen - K1 wird sogar hierher uebergeben und dann
  // ignoriert. Es ist ein Komplementaerfilter, das Gegenstueck zum Kalman.
  // Hier laeuft er mit, damit sich beide zur Laufzeit vergleichen lassen;
  // welcher den Regler speist, entscheidet use_kalman in BalanceCar.h.
  Yiorderfilter(Angle, Gyro_x, dt, K1);
  Gyro_z = -gz / 131;
}
