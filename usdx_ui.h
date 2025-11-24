#ifndef USDX_UI_H
#define USDX_UI_H

#include "usdx_config.h"
#include "usdx_i2c.h"

// ==========================================
// Encoder Logic
// ==========================================
extern volatile int8_t encoder_val;
extern volatile int8_t encoder_step;
void encoder_setup();

// ==========================================
// Display Classes
// ==========================================

class LCD : public Print { // inspired by: http://www.technoblogy.com/show?2BET
public:
  uint8_t _cols;
  void begin(uint8_t x = 0, uint8_t y = 0);
  void nib(uint8_t b, bool isData); // For I2C
  void nib(uint8_t b);              // For parallel
  void cmd(uint8_t b);
  size_t write(uint8_t b);
  void setCursor(uint8_t x, uint8_t y);
  void cursor();
  void noCursor();
  void noDisplay();
  void createChar(uint8_t l, uint8_t glyph[]);

  // Helper for parallel mode
  void pre();
  void post();
};

class OLEDDevice : public Print {
public:
  uint8_t oledX = 0, oledY = 0;
  uint8_t renderingFrame = 0xB0;
  bool wrap = false;
  bool curs = false;

  void cmd(uint8_t b);
  void begin(uint8_t cols, uint8_t rows, uint8_t charsize = 0);
  void noCursor();
  void cursor();
  void noDisplay();
  void createChar(uint8_t l, uint8_t glyph[]);
  void _setCursor(uint8_t x, uint8_t y);
  void drawCursor(bool en);
  void setCursor(uint8_t x, uint8_t y);
  void newLine();
  size_t write(byte c);
  void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
              const uint8_t bitmap[]);
};

template <class parent>
class Display
    : public parent { // This class spoofs display contents and cursor state
public:
#ifdef CAT_EXT
  uint8_t x, y;
  bool curs;
  char text[2 * 16 + 1];
  Display() : parent() { clear(); };
  size_t write(uint8_t b) {
    if ((x < 16) && (y < 2)) {
      text[y * 16 + x] = ((b < 9) ? "> :*#AB"[b - 1] : b);
      x++;
    }
    return parent::write(b);
  }
  void setCursor(uint8_t _x, uint8_t _y) {
    x = _x;
    y = _y;
    parent::setCursor(_x, _y);
  }
  void cursor() {
    curs = true;
    parent::cursor();
  }
  void noCursor() {
    curs = false;
    parent::noCursor();
  }
  void clear() {
    for (uint8_t i = 0; i != 2 * 16; i++)
      text[i] = ' ';
    text[2 * 16] = '\0';
    x = 0;
    y = 0;
  }
#endif // CAT_EXT
};

#ifdef BLIND
class Blind : public Print { // This class is a dummy LCD replacement
public:
  size_t write(uint8_t b) {}
  void setCursor(uint8_t _x, uint8_t _y) {}
  void cursor() {}
  void noCursor() {}
  void begin(uint8_t x = 0, uint8_t y = 0) {}
  void noDisplay() {}
  void createChar(uint8_t l, uint8_t glyph[]) {}
};
extern Display<Blind> lcd;
#else
#ifdef OLED
extern Display<OLEDDevice> lcd;
#else
extern Display<LCD> lcd;
#endif
#endif

#endif // USDX_UI_H
