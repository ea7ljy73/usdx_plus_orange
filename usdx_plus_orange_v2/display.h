// display.h - uSDX Plus Orange v2
// HD44780 LCD driver + rotary encoder.
// LCD: faithful port of usdx-legazy.ino active branch (NO RS_HIGH_ON_IDLE) -
// the one verified working on this hardware. Backlight is driven in post().

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "usdx_settings.h"

// defined in main .ino (menu "Light") - used by LCD::post
extern volatile uint8_t backlight;

// Serial port coexists with LCD data lines on PD0/PD1: disable UART while
// writing the LCD, re-enable after (usdx-legazy:50 _SERIAL). WITHOUT this the
// LCD data nibbles on PD0/PD1 collide with RX/TX -> garbage on screen.
#define _SERIAL 1

// ---------------------------------------------------------------------------
// Direct-IO LCD (PD0..PD3 data, PD4 EN, PC4 RS shared with SI5351 SDA)
// ---------------------------------------------------------------------------
class LCD {
public:
#define _dn 0 // PD0..PD3 = D4..D7
#define _en 4 // PD4 = EN
#define _rs 4 // PC4 = RS (shared with SI5351 SDA, pull-up)
#define RS_PULLUP 1
#define BACKLIGHT_PIN 0x08 // PD3 (uSDX+ backlight control)

#ifdef RS_PULLUP
#  define LCD_RS_HI()                                                                                                  \
    DDRC &= ~(1 << _rs);                                                                                               \
    asm("nop");                         // RS high (pull-up)
#  define LCD_RS_LO() DDRC |= 1 << _rs; // RS low (pull-down)
#else
#  define LCD_RS_LO() PORTC &= ~(1 << _rs); // RS low
#  define LCD_RS_HI() PORTC |= (1 << _rs);  // RS high
#endif
#define LCD_EN_LO() PORTD &= ~(1 << _en);                                  // EN low
#define LCD_EN_HI() PORTD |= (1 << _en);                                   // EN high
#define LCD_PREP_NIBBLE(b) (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en // data + enable high

  uint8_t _cols;
  void    begin(uint8_t x = 0, uint8_t y = 0) {
    _cols = x;
    DDRD |= 0xf << _dn | 1 << _en; // Make data, EN outputs
    DDRC |= 1 << _rs;              // RS output
    DDRD |= BACKLIGHT_PIN;         // backlight pin as output (PD3 = D7 + backlight)
    PORTD |= BACKLIGHT_PIN;        // backlight ON initial
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
    // Disable UART so PD0/PD1 (also LCD D4/D5) don't collide while writing LCD
    // (usdx-legazy _SERIAL). noInterrupts to keep backlight/output stable.
    UCSR0B &= ~((1 << RXEN0) | (1 << TXEN0)); // mask serial on shared pins
    noInterrupts();                           // do not allow LCD transfer to be interrupted
  }
  void post() {
    if(backlight)
      PORTD |= BACKLIGHT_PIN;
    else
      PORTD &= ~BACKLIGHT_PIN;             // backlight control
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0); // re-enable serial
    interrupts();
  }
  void nib(uint8_t b) { // Send four bit nibble (RS low = command)
    pre();
    PORTD = LCD_PREP_NIBBLE(b); // data + enable high
    delayMicroseconds(4);       // enable pulse > 450ns
    LCD_EN_LO();
    post();
    delayMicroseconds(60); // execution time
  }
  void cmd(uint8_t b) {
    nib(b >> 4);
    nib(b & 0xf); // write command: send nibbles while RS low
  }
  void write(uint8_t b) { // Write data: send nibbles while RS high
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4); // high nibble + enable high
    PORTD        = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;
    LCD_RS_LO();
    LCD_RS_HI();
    LCD_EN_LO();
    LCD_RS_LO();
    post();
    delayMicroseconds(60); // execution time
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
  void noDisplay() { cmd(0x08); } // display off (powerDown, legacy)
  void display() { cmd(0x0c); }   // display on
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
// Fonts (CGRAM 1..8) - exact copies of usdx-legazy:3427-3472
//  1=logo, 2..5=s-meter bars, 6=VFO-A arrow, 7=VFO-B arrow, 8=TBD
// ---------------------------------------------------------------------------
#define N_FONTS 8
const uint8_t display_fonts[N_FONTS][8] PROGMEM = {
    {0b01000, 0b00100, 0b01010, 0b00101, 0b01010, 0b00100, 0b01000, 0b00000}, // 1 logo
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}, // 2 s-meter 0 bars
    {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000}, // 3 s-meter 1 bars
    {0b10000, 0b10000, 0b10100, 0b10100, 0b10100, 0b10100, 0b10100, 0b10100}, // 4 s-meter 2 bars
    {0b10000, 0b10000, 0b10101, 0b10101, 0b10101, 0b10101, 0b10101, 0b10101}, // 5 s-meter 3 bars
    {0b01100, 0b10010, 0b11110, 0b10010, 0b10010, 0b00000, 0b00000, 0b00000}, // 6 VFO-A arrow
    {0b11100, 0b10010, 0b11100, 0b10010, 0b11100, 0b00000, 0b00000, 0b00000}, // 7 VFO-B arrow
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}, // 8 TBD
};

void display_init_fonts() {
  uint8_t item[8];
  for(uint8_t i = 0; i != N_FONTS; i++) {
    memcpy_P(item, display_fonts[i], 8);
    lcd.createChar(0x01 + i, item);
  }
}

// show_banner (legacy 3922-3932): uSDX + logo CGRAM
void show_banner() {
  lcd.setCursor(0, 0);
  lcd.print("uSDX");
  lcd.print('\x01'); // logo CGRAM
  lcd.print("        ");
  lcd.print("        ");
}

// ---------------------------------------------------------------------------
// Button(s) read by ADC (usdx-legazy parity): BUTTONS=A3/ADC3 feeds a resistor
// divider for Left/Right/Encoder push. Reads happen ONLY when a key press is
// detected on the digital line (see menu.h), never every loop iteration, so the
// RX ISR keeps the ADC most of the time.
//   thresholds (5V ref):  <4.2V => BL (left), <4.8V => BR (right), else BE
// ---------------------------------------------------------------------------
uint16_t analogSafeRead(uint8_t adcpin) { // legacy active variant (usdx-legazy:3495-3510)
  noInterrupts();
  for(; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  uint8_t adcsra = ADCSRA;
  uint8_t admux  = ADMUX;
  ADCSRA &= ~(1 << ADIE);                              // disable interrupts when measurement complete
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 prescaler for 9.6kHz
  ADMUX  = (1 << REFS0);                               // restore reference voltage AREF (5V)
  delay(1);                                            // settle
  int val = analogRead(adcpin);
  ADCSRA = adcsra;
  ADMUX  = admux;
  interrupts();
  return val;
}

// BUTTONS pin 17 = PC3/A3 => ADC channel 3 (NOT (17&15)=1!)
#define BUTTONS_ADC ((BUTTONS == 17) ? 3 : (BUTTONS & 0x0f))

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