#ifndef mode_h
#define mode_h

// Attribution:
//   Operating modes and their numbering are ported from OE3KUW/TumbllerVSC:
//   https://github.com/OE3KUW/TumbllerVSC
//   Siehe ATTRIBUTION.md.
//
// Unveraendert aus OE3KUW/TumbllerVSC uebernommen. IRREMOTE und BLUETOOTH
// bleiben in der Aufzaehlung stehen, obwohl beide hier nicht bedient werden -
// so bleibt die Nummerierung und damit jeder Vergleich mit dem Original
// gueltig. Der Infrarotempfaenger fehlt tatsaechlich; Bluetooth koennte der
// ESP32 dagegen sehr wohl, es ist nur nicht portiert.
enum FUNCTION_MODE
{
  IDLE,
  IRREMOTE,
  OBSTACLE,
  FOLLOW,
  BLUETOOTH,
  FOLLOW2,
} function_mode = IDLE;

enum MOTION_MODE
{
  STANDBY,
  FORWARD,
  BACKWARD,
  TURNLEFT,
  TURNRIGHT,
  STOP,
  START,
} motion_mode = START;

#endif
