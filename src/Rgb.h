#ifndef Rgb_h
#define Rgb_h

// ===========================================================================
//  Statusanzeige auf dem WS2812-Streifen
//
//  Attribution:
//    Methodennamen und Status-Sequenz uebernommen aus OE3KUW/TumbllerVSC:
//    https://github.com/OE3KUW/TumbllerVSC
//    Siehe ATTRIBUTION.md.
//
//  Die METHODENNAMEN sind die der Vorlage (OE3KUW/TumbllerVSC), damit die
//  Aufrufstellen in main.cpp Zeile fuer Zeile dieselben bleiben. Die
//  Darstellung dahinter ist neu, und zwar aus einem sachlichen Grund:
//
//  Die Klasse der Vorlage ist auf genau VIER LEDs geschrieben, die an den
//  Ecken sitzen - setColorNew() nimmt vier Farbtripel, und
//  flashBlueColorFront() setzt davon Nummer 2 und 3, also "vorne". Diese
//  Platine hat ACHT LEDs in einer Reihe. Ein "vorne" gibt es dort nicht, die
//  vier Ecktripel haetten keine Entsprechung, und die oberen vier Plaetze der
//  Farbfelder waeren uninitialisiert geblieben.
//
//  Die aelteren Projekte auf genau dieser Platine (tumbler_balance,
//  tumbler_mini) behandeln die Reihe deshalb als Balken und zeigen die
//  Neigung als wandernden Punkt - eine Wasserwaage. Das ist hier uebernommen:
//  im Balancierbetrieb sieht man die Neigung, in allen anderen Zustaenden
//  die Farben der Vorlage.
//
//    Zustand                Anzeige
//    STANDBY (balanciert)   Wasserwaage: Punkt wandert, gruen -> gelb -> rot
//    vorwaerts/rueckwaerts  blaues Blinken am jeweiligen Ende der Reihe
//    links/rechts           blaues Blinken auf der jeweiligen Haelfte
//    STOP / umgefallen      alles rot
// ===========================================================================

#include <Adafruit_NeoPixel.h>
#include "Pins.h"

class RGB : public Adafruit_NeoPixel
{
public:
  RGB() : Adafruit_NeoPixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800) {}

  // Acht LEDs auf voller Helligkeit ziehen rund 0,5 A aus derselben
  // Versorgung wie die Motoren. Deshalb gedeckelt.
  int brightness = 50;

  void initialize()
  {
    begin();
    setBrightness(brightness);
    clear();
    show();
  }

  // --- Namen der Vorlage -------------------------------------------------
  void lightOff()             { pattern = P_LEVEL; }   // im Stand: Wasserwaage
  void brightRedColor()       { pattern = P_RED; }
  void flashRedColor()        { pattern = P_RED; }
  void brightBlueColor()      { pattern = P_BLUE_ALL; }
  void flashBlueColorFront()  { pattern = P_FRONT; }
  void flashBlueColorback()   { pattern = P_BACK; }
  void flashBlueColorLeft()   { pattern = P_LEFT; }
  void flashBlueColorRight()  { pattern = P_RIGHT; }

  // Wird wie im Original aus loop() aufgerufen und macht die Animation.
  void blink(unsigned long delay_time)
  {
    if (millis() - last_draw < 25) return;    // 40 Hz reichen fuers Auge
    last_draw = millis();

    const bool on = (millis() / delay_time) % 2 == 0;
    clear();

    switch (pattern)
    {
    case P_RED:      fill_range(0, NUMPIXELS, 255, 0, 0, 1.0f);            break;
    case P_BLUE_ALL: fill_range(0, NUMPIXELS, 0, 0, 255, 1.0f);            break;
    case P_FRONT:    if (on) fill_range(NUMPIXELS/2, NUMPIXELS, 0,0,255, 1.0f); break;
    case P_BACK:     if (on) fill_range(0, NUMPIXELS/2, 0,0,255, 1.0f);    break;
    case P_LEFT:     if (on) fill_range(0, NUMPIXELS/2, 0,0,255, 1.0f);    break;
    case P_RIGHT:    if (on) fill_range(NUMPIXELS/2, NUMPIXELS, 0,0,255, 1.0f); break;
    default:         draw_level();                                        break;
    }
    show();
  }

  // Der Winkel, den die Wasserwaage zeigt - wird aus loop() nachgefuehrt.
  void setAngle(float a) { angle = a; }

private:
  enum Pattern { P_LEVEL, P_RED, P_BLUE_ALL, P_FRONT, P_BACK, P_LEFT, P_RIGHT };
  Pattern pattern = P_LEVEL;
  unsigned long last_draw = 0;
  float angle = 0.0f;

  // Bei dieser Abweichung sitzt der Punkt ganz aussen.
  static constexpr float LEVEL_RANGE = 8.0f;

  void put(int i, uint8_t r, uint8_t g, uint8_t b, float f)
  {
    if (i < 0 || i >= (int)NUMPIXELS) return;
    f = f < 0 ? 0 : (f > 1 ? 1 : f);
    setPixelColor(i, Color((uint8_t)(r*f), (uint8_t)(g*f), (uint8_t)(b*f)));
  }

  void fill_range(int from, int to, uint8_t r, uint8_t g, uint8_t b, float f)
  {
    for (int i = from; i < to; i++) put(i, r, g, b, f);
  }

  // Wasserwaage: der Punkt wandert mit der Neigung, die Farbe zeigt den
  // Betrag. Die Nachbarn glimmen mit - bei nur acht LEDs waere jeder Schritt
  // sonst ein Sprung von zwei Grad und die Anzeige wuerde huepfen.
  void draw_level()
  {
    const float span = angle / LEVEL_RANGE;
    const float clamped = span < -1 ? -1 : (span > 1 ? 1 : span);
    const float posf = (clamped + 1.0f) * 0.5f * (NUMPIXELS - 1);

    const float a = fabsf(angle);
    uint8_t r, g, b;
    if      (a < 1.0f) { r = 0;   g = 255; b = 40; }
    else if (a < 3.0f) { r = 255; g = 180; b = 0;  }
    else               { r = 255; g = 40;  b = 0;  }

    for (int i = 0; i < (int)NUMPIXELS; i++)
    {
      const float d = fabsf(i - posf);
      if (d <= 1.6f) put(i, r, g, b, 1.0f - d / 1.6f);
    }
  }
};

#endif
