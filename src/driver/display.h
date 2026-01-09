#ifndef DISPLAY_H
#define DISPLAY_H

#include "../hardware/wire.h"
#include <Arduino.h>
#include <avr/wdt.h>

// LCD pin definitions used by class LCD
#define _dn 0 // PD0 to PD3 connect to D4 to D7 on the display
#define _en 4 // PD4 - MUST have pull-up resistor
#define _rs 4 // PC4 - MUST have pull-up resistor

#define RS_PULLUP 1 // Use pullup on RS line

#ifdef RS_PULLUP
#define LCD_RS_HI()                                                            \
  DDRC &= ~(1 << _rs);                                                         \
  asm("nop");                         // RS high (pull-up)
#define LCD_RS_LO() DDRC |= 1 << _rs; // RS low (pull-down)
#else
#define LCD_RS_LO() PORTC &= ~(1 << _rs); // RS low
#define LCD_RS_HI() PORTC |= (1 << _rs);  // RS high
#endif                                    // RS_PULLUP
#define LCD_EN_LO() PORTD &= ~(1 << _en); // EN low
#define LCD_EN_HI() PORTD |= (1 << _en);  // EN high
#define LCD_PREP_NIBBLE(b)                                                     \
  (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en // Send data and enable high

/**
 * @brief LCD Driver Class.
 * Supports standard 4-bit parallel and I2C LCDs (via compile-time flags).
 */
class LCD : public Print { // inspired by: http://www.technoblogy.com/show?2BET
public:
  uint8_t _cols;
  void begin(uint8_t x = 0, uint8_t y = 0);
#ifdef LCD_I2C
  void nib(uint8_t b, bool isData);
  void cmd(uint8_t b);
  size_t write(uint8_t b);
#else
  void pre();
  void post();
#ifdef RS_HIGH_ON_IDLE
  void cmd(uint8_t b);
  size_t write(uint8_t b);
#else
  void nib(uint8_t b);
  void cmd(uint8_t b);
  size_t write(uint8_t b);
#endif
#endif
  void setCursor(uint8_t x, uint8_t y);
  void cursor();
  void noCursor();
  void noDisplay();
  void createChar(uint8_t l, uint8_t glyph[]);
};

#define FONT_W 8
#define FONT_H 2
#define FONT_STRETCHV 1
#define FONT_STRETCHH 0

class OLEDDevice
    : public Print { // https://www.buydisplay.com/download/manual/ER-OLED0.91-3_Series_Datasheet.pdf
public:
#define OLED_ADDR 0x3C // Slave address
#define OLED_PAGES 4
#define OLED_COMMAND 0x00
#define OLED_DATA 0x40
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
// #define BLIND 1             // uSDX in head-less operation

class Blind : public Print { // This class is a dummy LCD replacement
public:
  size_t write(uint8_t b);
  void setCursor(uint8_t _x, uint8_t _y);
  void cursor();
  void noCursor();
  void begin(uint8_t x = 0, uint8_t y = 0);
  void noDisplay();
  void createChar(uint8_t l, uint8_t glyph[]);
};

#ifdef BLIND
extern Display<Blind> lcd;
#else
#ifdef OLED
extern Display<OLEDDevice> lcd;
#else
extern Display<LCD>
    lcd; // highly-optimized LCD driver, OK for QCX supplied displays
#endif
#endif

#endif // DISPLAY_H
