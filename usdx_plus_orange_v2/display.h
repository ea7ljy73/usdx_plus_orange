// display.h - uSDX Plus Orange v2
// HD44780 LCD (4-bit direct IO) + rotary encoder.
// Minimal essential driver for WHITE_BUTTONS (LCD 16x2/16x4 direct IO).
// (v1's full Print subclass / serial-coexistence removed by design.)

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Direct-IO LCD (standard uSDX wiring: PD0..PD3 data, PD4 EN, PC4 RS)
// ---------------------------------------------------------------------------
class LCD {
public:
#define _dn 0 // PD0..PD3 = D4..D7
#define _en 4 // PD4 = EN
#define _rs 4 // PC4 = RS (shared with SI5351 SDA, pull-up)
#define RS_PULLUP 1
#ifdef RS_PULLUP
#  define LCD_RS_HI()                                                                                                  \
    do {                                                                                                               \
      DDRC &= ~(1 << _rs);                                                                                             \
      asm("nop");                                                                                                      \
    } while(0)
#  define LCD_RS_LO()                                                                                                  \
    do {                                                                                                               \
      DDRC |= 1 << _rs;                                                                                                \
    } while(0)
#else
#  define LCD_RS_LO()                                                                                                  \
    do {                                                                                                               \
      PORTC &= ~(1 << _rs);                                                                                            \
    } while(0)
#  define LCD_RS_HI()                                                                                                  \
    do {                                                                                                               \
      PORTC |= (1 << _rs);                                                                                             \
    } while(0)
#endif
#define LCD_EN_LO()                                                                                                    \
  do {                                                                                                                 \
    PORTD &= ~(1 << _en);                                                                                              \
  } while(0)
#define LCD_EN_HI()                                                                                                    \
  do {                                                                                                                 \
    PORTD |= (1 << _en);                                                                                               \
  } while(0)
#define LCD_PREP_NIBBLE(b) ((PORTD & ~(0xf << _dn)) | ((b) << _dn) | (1 << _en))

  uint8_t _cols, _rows;
  void    begin(uint8_t cols = 16, uint8_t rows = 4) {
    _cols = cols;
    _rows = rows;
    DDRD |= 0xf << _dn | 1 << _en;
    DDRC |= 1 << _rs;
    LCD_RS_LO();
    LCD_EN_LO();
    _delay_ms(50);
    cmd(0x33);
    _delay_us(4500);
    cmd(0x33);
    _delay_us(4500);
    cmd(0x33);
    _delay_us(150);
    cmd(0x32);
    cmd(0x28); // 2-line 5x8, 4-bit
    cmd(0x0c); // display on, cursor off
    cmd(0x01); // clear
    _delay_ms(3);
    cmd(0x06); // entry left, no shift
  }
  void clear() {
    cmd(0x01);
    _delay_ms(3);
  }
  void setCursor(uint8_t c, uint8_t r) {
    uint8_t row = (r >= _rows) ? _rows - 1 : r;
    cmd(0x80 | (row * 0x40 + c));
  }
  void noCursor() { cmd(0x0c); }
  void cursor() { cmd(0x0f); }

  void print(const char* s) {
    while(*s)
      write((uint8_t)*s++);
  }
  void print(char c) { write((uint8_t)c); }
  void print(int n) {
    char b[7];
    itoa(n, b, 10);
    print(b);
  }
  void write(uint8_t b) {
    nib(b >> 4, true);
    nib(b, true);
  }
  void createChar(uint8_t n, const uint8_t* data) {
    // (CGRAM load kept minimal for the uSDX gauge fonts)
    ;
  }

private:
  void cmd(uint8_t b) {
    nib(b >> 4, false);
    nib(b, false);
  }
  void nib(uint8_t b, bool isData) {
    if(isData)
      LCD_RS_HI();
    else
      LCD_RS_LO();
    PORTD = LCD_PREP_NIBBLE(b); // data + EN high
    _delay_us(1);               // EN pulse > 450ns
    LCD_EN_LO();
    _delay_us(50); // commands need > 37us
  }
};

#undef LCD_RS_HI
#undef LCD_RS_LO
#undef LCD_EN_HI
#undef LCD_EN_LO
#undef LCD_PREP_NIBBLE

extern LCD lcd; // instance defined in main .ino

// ---------------------------------------------------------------------------
// Rotary encoder (PCINT2 on ROT_A/ROT_B)
// ---------------------------------------------------------------------------
volatile uint8_t last_state;
volatile int16_t encoder_val;
volatile uint8_t encoder_pressed;

ISR(PCINT2_vect) { // Interrupt on rotary encoder turn
  switch(last_state = (last_state << 4) | (digitalRead(ROT_B) << 1) | digitalRead(ROT_A)) {
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
    encoder_val--;
    break;
  }
}

void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23);
  PCICR |= (1 << PCIE2);
  last_state = (digitalRead(ROT_B) << 1) | digitalRead(ROT_A);
  interrupts();
}