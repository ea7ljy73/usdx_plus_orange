// display.h - uSDX Plus Orange v2
// HD44780 LCD (direct IO, RS_HIGH_ON_IDLE like v1) + rotary encoder.
// LCD writes disable interrupts (pre/post) so the enable pulse is not
// corrupted by the sample-rate ISR - this was the v1 requirement that the
// earlier minimal driver missed (blank screen symptom).

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Direct-IO LCD (PD0..PD3 data, PD4 EN, PC4 RS shared with SI5351 SDA)
// ---------------------------------------------------------------------------
class LCD {
public:
#define _dn 0 // PD0..PD3 = D4..D7
#define _en 4 // PD4 = EN
#define _rs 4 // PC4 = RS (shared with SI5351 SDA, pull-up)
#define RS_HIGH_ON_IDLE 1

#define LCD_RS_LO() DDRC |= 1 << _rs; // RS low (drive)      [commands]
#define LCD_RS_HI()                                                                                                    \
  DDRC &= ~(1 << _rs);                                                                                                 \
  asm("nop");                                                              // RS high (release -> pull-up) [data]
#define LCD_EN_LO() PORTD &= ~(1 << _en);                                  // EN low
#define LCD_EN_HI() PORTD |= (1 << _en);                                   // EN high
#define LCD_PREP_NIBBLE(b) (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en // data + enable high

  uint8_t _cols;
  void    begin(uint8_t x = 16, uint8_t y = 2) {
    _cols = x;
    DDRD |= 0xf << _dn | 1 << _en; // data, EN outputs
    DDRC |= 1 << _rs;              // RS output
    delayMicroseconds(50000);      // power-up wait
    LCD_RS_LO();
    LCD_EN_LO();
    cmd(0x33); // 8-bit mode
    delayMicroseconds(4500);
    cmd(0x33);
    delayMicroseconds(4500);
    cmd(0x33);
    delayMicroseconds(150);
    cmd(0x32); // 4-bit mode
    cmd(0x28); // 2-line 5x8
    cmd(0x0c); // display on, cursor off
    cmd(0x01); // clear
    delay(3);
    cmd(0x06); // entry left, no shift
  }
  void pre() {
    // v1: never let the sample-rate ISR corrupt the LCD cycle
    noInterrupts();
  }
  void post() {
    interrupts();
    delayMicroseconds(60); // command exec time
  }
  void cmd(uint8_t b) {
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4); // high nibble + EN high
    PORTD        = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_LO();
    LCD_EN_LO();
    PORTD = nibl;
    asm("nop");
    asm("nop"); // enable pulse >= 500ns
    LCD_EN_LO();
    LCD_RS_HI();
    post();
  }
  void write(uint8_t b) {
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4);
    PORTD        = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;
    asm("nop");
    asm("nop");
    LCD_EN_LO();
    post();
  }
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
  void setCursor(uint8_t x, uint8_t y) { cmd(0x80 | (x + y * 0x40)); }
  void cursor() { cmd(0x0e); }
  void noCursor() { cmd(0x0c); }
  void clear() {
    cmd(0x01);
    delay(3);
  }
  void createChar(uint8_t l, uint8_t glyph[]) {
    cmd(0x40 | ((l & 0x7) << 3));
    for(int i = 0; i != 8; i++)
      write(glyph[i]);
  }
};

#undef LCD_RS_LO
#undef LCD_RS_HI
#undef LCD_EN_LO
#undef LCD_EN_HI
#undef LCD_PREP_NIBBLE

extern LCD lcd; // instance defined in main .ino

// ---------------------------------------------------------------------------
// Rotary encoder (PCINT2 on ROT_A/ROT_B)
// ---------------------------------------------------------------------------
volatile uint8_t last_state;
volatile int16_t encoder_val;
volatile uint8_t encoder_pressed;

ISR(PCINT2_vect) { // Interrupt on rotary encoder turn (direct PIND read)
  uint8_t p = PIND;
  switch(last_state = (last_state << 4) | ((p & (1 << ROT_B)) ? 2 : 0) | ((p & (1 << ROT_A)) ? 1 : 0)) {
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