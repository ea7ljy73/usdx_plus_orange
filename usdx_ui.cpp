#include "usdx_ui.h"
#include "usdx_config.h"
#include "usdx_utils.h"
#include <Arduino.h>

// ==========================================
// Encoder Implementation
// ==========================================
volatile int8_t encoder_val = 0;
volatile int8_t encoder_step = 0;
static uint8_t last_state;

ISR(PCINT2_vect) { // Interrupt on rotary encoder turn
  switch (last_state = (last_state << 4) | (_digitalRead(ROT_B) << 1) |
                       _digitalRead(ROT_A)) { // transition
// #define ENCODER_ENHANCED_RESOLUTION  1
#ifdef ENCODER_ENHANCED_RESOLUTION
  case 0x31:
  case 0x10:
  case 0x02:
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
  case 0x20:
  case 0x01:
  case 0x13:
    encoder_val--;
    break;
#else
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
    encoder_val--;
    break;
#endif
  }
}

void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |= (1 << PCINT22) |
            (1 << PCINT23); // interrupt-enable for ROT_A, ROT_B pin changes
  PCICR |= (1 << PCIE2);
  last_state = (_digitalRead(ROT_B) << 1) | _digitalRead(ROT_A);
  interrupts();
}

// ==========================================
// LCD Implementation
// ==========================================

// LCD Macros
#define _dn 0       // PD0 to PD3 connect to D4 to D7 on the display
#define _en 4       // PD4 - MUST have pull-up resistor
#define _rs 4       // PC4 - MUST have pull-up resistor
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

#ifdef LCD_I2C
#define PCF_ADDR                                                               \
  0x27 // LCD I2C address where PCF8574 addess selection A0, A1, A2 are all open
#define PCF_RS 0x01
#define PCF_RW 0x02 // the 0xF0 bits are used for 4-bit data to the display.
#define PCF_EN 0x04
#define PCF_BACKLIGHT 0x08
#endif

void LCD::begin(uint8_t x, uint8_t y) {
#ifdef LCD_I2C
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  delayMicroseconds(50000);
#else        //! LCD_I2C
  DDRD |= 0xf << _dn | 1 << _en; // Make data, EN outputs
  DDRC |= 1 << _rs;
  delayMicroseconds(50000); // *
  LCD_RS_LO();
  LCD_EN_LO();
#endif       //! LCD_I2C
  cmd(0x33); // Ensures display is in 8-bit mode
  delayMicroseconds(4500);
  cmd(0x33);
  delayMicroseconds(4500);
  cmd(0x33);
  delayMicroseconds(150); // * Ensures display is in 8-bit mode
  cmd(0x32);              // Puts display in 4-bit mode
  cmd(0x28);              // * Function set: 2-line, 5x8
  cmd(0x0c);              // Display on
  cmd(0x01);              // Clear display
  delay(3);               // Allow to execute Clear on display
  cmd(0x06);              // * Entrymode: left, shift-dec
}

#ifdef LCD_I2C
void LCD::nib(uint8_t b, bool isData) {
  b = (b << 4) | ((backlight) ? PCF_BACKLIGHT : 0) | ((isData) ? PCF_RS : 0);
  Wire.write(b | PCF_EN); // write command EN HI
  delayMicroseconds(4);   // enable pulse must be >450ns
  Wire.write(b);          // write command EN LO
  delayMicroseconds(60);  // commands need > 37us to settle
  Wire.write(b);          // must write for some unknown reason
}
void LCD::cmd(uint8_t b) {
  Wire.beginTransmission(PCF_ADDR);
  nib(b >> 4, false);
  nib(b, false);
  Wire.endTransmission();
}
size_t LCD::write(uint8_t b) {
  Wire.beginTransmission(PCF_ADDR);
  nib((b >> 4), true);
  nib((b), true);
  Wire.endTransmission();
  return 1;
}
#else //! LCD_I2C
void LCD::pre() {
#ifdef _SERIAL
  if (!vox)
    if (cat_active) {
      Serial.flush();
      for (; millis() < rxend_event;)
        wdt_reset();
      PORTC |= 1 << 2;
      DDRC |= 1 << 2;
    }
  UCSR0B &= ~((1 << RXEN0) |
              (1 << TXEN0)); // Complete serial TX and RX; mask PD1 LCD
                             // data-exchange by pulling-up TXD via PC2 HIGH;
                             // enable PD0/PD1, disable serial port
#endif
  noInterrupts(); // do not allow LCD tranfer to be interrupted, to prevent
                  // backlight to lighten-up
}
void LCD::post() {
  if (backlight)
    PORTD |= 0x08;
  else
    PORTD &= ~0x08; // Backlight control
#ifdef _SERIAL
  UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
  if (!vox)
    if (cat_active) {
      PORTC &= ~(1 << 2);
    } // Enable serial port, disable PD0, PD1; PC2 LOW to prevent CAT TX
      // disruption via MIC input
#endif
  interrupts();
}

#ifdef RS_HIGH_ON_IDLE
void LCD::cmd(uint8_t b) {
  pre();
  uint8_t nibh =
      LCD_PREP_NIBBLE(b >> 4); // Prepare high nibble data and enable high
  PORTD = nibh;                // Send high nibble data and enable high
  uint8_t nibl =
      LCD_PREP_NIBBLE(b & 0xf); // Prepare low nibble data and enable high
  LCD_RS_LO();
  LCD_EN_LO();
  PORTD = nibl; // Send low nibble data and enable high
  asm("nop");
  asm("nop"); // Keep RS low, but complete enable cycle (should be 500ns)
  LCD_EN_LO();
  LCD_RS_HI();
  post();
  delayMicroseconds(60); // Execution time  (37+4)*1.25 us
}
size_t LCD::write(uint8_t b) { // Write data:    send nibbles while RS high
  pre();
  uint8_t nibh =
      LCD_PREP_NIBBLE(b >> 4); // Prepare high nibble data and enable high
  PORTD = nibh;                // Send high nibble data and enable high
  uint8_t nibl =
      LCD_PREP_NIBBLE(b & 0xf); // Prepare low nibble data and enable high
  LCD_RS_HI();
  LCD_EN_LO();
  PORTD = nibl; // Send low nibble data and enable high
  asm("nop");
  asm("nop"); // Keep RS high, but complete enable cycle (should be 500ns)
  LCD_EN_LO();
  post();
  delayMicroseconds(60); // Execution time  (37+4)*1.25 us
  return 1;
}
#else  //! RS_HIGH_ON_IDLE
void LCD::nib(uint8_t b) { // Send four bit nibble to display
  pre();
  PORTD = LCD_PREP_NIBBLE(b); // Send data and enable high
  delayMicroseconds(4);
  LCD_EN_LO();
  post();
  delayMicroseconds(60); // Execution time
}
void LCD::cmd(uint8_t b) {
  nib(b >> 4);
  nib(b & 0xf);
} // Write command: send nibbles while RS low
size_t LCD::write(uint8_t b) { // Write data:    send nibbles while RS high
  pre();
  uint8_t nibh =
      LCD_PREP_NIBBLE(b >> 4); // Prepare high nibble data and enable high
  uint8_t nibl =
      LCD_PREP_NIBBLE(b & 0xf); // Prepare low nibble data and enable high
  PORTD = nibh;                 // Send high nibble data and enable high
  LCD_RS_HI();
  LCD_EN_LO();
  PORTD = nibl; // Send low nibble data and enable high
  LCD_RS_LO();
  LCD_RS_HI();
  LCD_EN_LO();
  LCD_RS_LO();
  post();
  delayMicroseconds(60); // Execution time  (37+4)*1.25 us
  return 1;
}
#endif // RS_HIGH_ON_IDLE
#endif //! LCD_I2C

#ifdef CONDENSED
void LCD::setCursor(uint8_t x, uint8_t y) {
  cmd(0x80 | (x + (uint8_t[]){0x00, 0x40, 0x00 + 20, 0x40 + 20}[y]));
} // ONLY for LCD2004 display
#else
void LCD::setCursor(uint8_t x, uint8_t y) { cmd(0x80 | (x + y * 0x40)); }
#endif
void LCD::cursor() { cmd(0x0e); }
void LCD::noCursor() { cmd(0x0c); }
void LCD::noDisplay() { cmd(0x08); }
void LCD::createChar(uint8_t l, uint8_t glyph[]) {
  cmd(0x40 | ((l & 0x7) << 3));
  for (int i = 0; i != 8; i++)
    write(glyph[i]);
}

// ==========================================
// OLED Implementation
// ==========================================

// C64 real font
const uint8_t font[] PROGMEM = {
    0x00,      0x00,      0x00,      0x00,      0x00,      0x00,      0x00,
    0x00, // ' '
    0x00,      0x00,      0x00,      0x4f,      0x4f,      0x00,      0x00,
    0x00, // !
    0x00,      0x07,      0x07,      0x00,      0x00,      0x07,      0x07,
    0x00, // "
    0x14,      0x7f,      0x7f,      0x14,      0x14,      0x7f,      0x7f,
    0x14, // #
    0x00,      0x24,      0x2e,      0x6b,      0x6b,      0x3a,      0x12,
    0x00, // $
    0x00,      0x63,      0x33,      0x18,      0x0c,      0x66,      0x63,
    0x00, // %
    0x00,      0x32,      0x7f,      0x4d,      0x4d,      0x77,      0x72,
    0x50, // &
    0x00,      0x00,      0x00,      0x04,      0x06,      0x03,      0x01,
    0x00, // '
    0x00,      0x00,      0x1c,      0x3e,      0x63,      0x41,      0x00,
    0x00, // (
    0x00,      0x00,      0x41,      0x63,      0x3e,      0x1c,      0x00,
    0x00, // )
    0x08,      0x2a,      0x3e,      0x1c,      0x1c,      0x3e,      0x2a,
    0x08, // *
    0x00,      0x08,      0x08,      0x3e,      0x3e,      0x08,      0x08,
    0x00, // +
    0x00,      0x00,      0x80,      0xe0,      0x60,      0x00,      0x00,
    0x00, // ,
    0x00,      0x08,      0x08,      0x08,      0x08,      0x08,      0x08,
    0x00, // -
    0x00,      0x00,      0x00,      0x60,      0x60,      0x00,      0x00,
    0x00, // .
    0x00,      0x40,      0x60,      0x30,      0x18,      0x0c,      0x06,
    0x02, // /
    0x00,      0x3e,      0x7f,      0x49,      0x45,      0x7f,      0x3e,
    0x00, // 0
    0x00,      0x40,      0x44,      0x7f,      0x7f,      0x40,      0x40,
    0x00, // 1
    0x00,      0x62,      0x73,      0x51,      0x49,      0x4f,      0x46,
    0x00, // 2
    0x00,      0x22,      0x63,      0x49,      0x49,      0x7f,      0x36,
    0x00, // 3
    0x00,      0x18,      0x18,      0x14,      0x16,      0x7f,      0x7f,
    0x10, // 4
    0x00,      0x27,      0x67,      0x45,      0x45,      0x7d,      0x39,
    0x00, // 5
    0x00,      0x3e,      0x7f,      0x49,      0x49,      0x7b,      0x32,
    0x00, // 6
    0x00,      0x03,      0x03,      0x79,      0x7d,      0x07,      0x03,
    0x00, // 7
    0x00,      0x36,      0x7f,      0x49,      0x49,      0x7f,      0x36,
    0x00, // 8
    0x00,      0x26,      0x6f,      0x49,      0x49,      0x7f,      0x3e,
    0x00, // 9
    0x00,      0x00,      0x00,      0x24,      0x24,      0x00,      0x00,
    0x00, // :
    0x00,      0x00,      0x80,      0xe4,      0x64,      0x00,      0x00,
    0x00, // ;
    0x00,      0x08,      0x1c,      0x36,      0x63,      0x41,      0x41,
    0x00, // <
    0x00,      0x14,      0x14,      0x14,      0x14,      0x14,      0x14,
    0x00, // =
    0x00,      0x41,      0x41,      0x63,      0x36,      0x1c,      0x08,
    0x00, // >
    0x00,      0x02,      0x03,      0x51,      0x59,      0x0f,      0x06,
    0x00, // ?
    0x00,      0x3e,      0x7f,      0x41,      0x4d,      0x4f,      0x2e,
    0x00, // @
    0x00,      0x7c,      0x7e,      0x0b,      0x0b,      0x7e,      0x7c,
    0x00, // A
    0x00,      0x7f,      0x7f,      0x49,      0x49,      0x7f,      0x36,
    0x00, // B
    0x00,      0x3e,      0x7f,      0x41,      0x41,      0x63,      0x22,
    0x00, // C
    0x00,      0x7f,      0x7f,      0x41,      0x63,      0x3e,      0x1c,
    0x00, // D
    0x00,      0x7f,      0x7f,      0x49,      0x49,      0x41,      0x41,
    0x00, // E
    0x00,      0x7f,      0x7f,      0x09,      0x09,      0x01,      0x01,
    0x00, // F
    0x00,      0x3e,      0x7f,      0x41,      0x49,      0x7b,      0x3a,
    0x00, // G
    0x00,      0x7f,      0x7f,      0x08,      0x08,      0x7f,      0x7f,
    0x00, // H
    0x00,      0x00,      0x41,      0x7f,      0x7f,      0x41,      0x00,
    0x00, // I
    0x00,      0x20,      0x60,      0x41,      0x7f,      0x3f,      0x01,
    0x00, // J
    0x00,      0x7f,      0x7f,      0x1c,      0x36,      0x63,      0x41,
    0x00, // K
    0x00,      0x7f,      0x7f,      0x40,      0x40,      0x40,      0x40,
    0x00, // L
    0x00,      0x7f,      0x7f,      0x06,      0x0c,      0x06,      0x7f,
    0x7f, // M
    0x00,      0x7f,      0x7f,      0x0e,      0x1c,      0x7f,      0x7f,
    0x00, // N
    0x00,      0x3e,      0x7f,      0x41,      0x41,      0x7f,      0x3e,
    0x00, // O
    0x00,      0x7f,      0x7f,      0x09,      0x09,      0x0f,      0x06,
    0x00, // P
    0x00,      0x1e,      0x3f,      0x21,      0x61,      0x7f,      0x5e,
    0x00, // Q
    0x00,      0x7f,      0x7f,      0x19,      0x39,      0x6f,      0x46,
    0x00, // R
    0x00,      0x26,      0x6f,      0x49,      0x49,      0x7b,      0x32,
    0x00, // S
    0x00,      0x01,      0x01,      0x7f,      0x7f,      0x01,      0x01,
    0x00, // T
    0x00,      0x3f,      0x7f,      0x40,      0x40,      0x7f,      0x3f,
    0x00, // U
    0x00,      0x1f,      0x3f,      0x60,      0x60,      0x3f,      0x1f,
    0x00, // V
    0x00,      0x7f,      0x7f,      0x30,      0x18,      0x30,      0x7f,
    0x7f, // W
    0x00,      0x63,      0x77,      0x1c,      0x1c,      0x77,      0x63,
    0x00, // X
    0x00,      0x07,      0x0f,      0x78,      0x78,      0x0f,      0x07,
    0x00, // Y
    0x00,      0x61,      0x71,      0x59,      0x4d,      0x47,      0x43,
    0x00, // Z
    0x00,      0x00,      0x7f,      0x7f,      0x41,      0x41,      0x00,
    0x00, // [
    0x00,      0x02,      0x06,      0x0c,      0x18,      0x30,      0x60,
    0x40,      0x00,      0x00,      0x41,      0x41,      0x7f,      0x7f,
    0x00,      0x00, // ]
    0x00,      0x08,      0x0c,      0xfe,      0xfe,      0x0c,      0x08,
    0x00, // ^
    0x80,      0x80,      0x80,      0x80,      0x80,      0x80,      0x80,
    0x80, // _
    0x00,      0x01,      0x03,      0x06,      0x04,      0x00,      0x00,
    0x00, // '
    0x00,      0x20,      0x74,      0x54,      0x54,      0x7c,      0x78,
    0x00, // a
    0x00,      0x7e,      0x7e,      0x48,      0x48,      0x78,      0x30,
    0x00, // b
    0x00,      0x38,      0x7c,      0x44,      0x44,      0x44,      0x00,
    0x00, // c
    0x00,      0x30,      0x78,      0x48,      0x48,      0x7e,      0x7e,
    0x00, // d
    0x00,      0x38,      0x7c,      0x54,      0x54,      0x5c,      0x18,
    0x00, // e
    0x00,      0x00,      0x08,      0x7c,      0x7e,      0x0a,      0x0a,
    0x00, // f
    0x00,      0x98,      0xbc,      0xa4,      0xa4,      0xfc,      0x7c,
    0x00, // g
    0x00,      0x7e,      0x7e,      0x08,      0x08,      0x78,      0x70,
    0x00, // h
    0x00,      0x00,      0x48,      0x7a,      0x7a,      0x40,      0x00,
    0x00, // i
    0x00,      0x00,      0x80,      0x80,      0x80,      0xfa,      0x7a,
    0x00, // j
    0x00,      0x7e,      0x7e,      0x10,      0x38,      0x68,      0x40,
    0x00, // k
    0x00,      0x00,      0x42,      0x7e,      0x7e,      0x40,      0x00,
    0x00, // l
    0x00,      0x7c,      0x7c,      0x18,      0x38,      0x1c,      0x7c,
    0x78, // m
    0x00,      0x7c,      0x7c,      0x04,      0x04,      0x7c,      0x78,
    0x00, // n
    0x00,      0x38,      0x7c,      0x44,      0x44,      0x7c,      0x38,
    0x00, // o
    0x00,      0xfc,      0xfc,      0x24,      0x24,      0x3c,      0x18,
    0x00, // p
    0x00,      0x18,      0x3c,      0x24,      0x24,      0xfc,      0xfc,
    0x00, // q
    0x00,      0x7c,      0x7c,      0x04,      0x04,      0x0c,      0x08,
    0x00, // r
    0x00,      0x48,      0x5c,      0x54,      0x54,      0x74,      0x24,
    0x00, // s
    0x00,      0x04,      0x04,      0x3e,      0x7e,      0x44,      0x44,
    0x00, // t
    0x00,      0x3c,      0x7c,      0x40,      0x40,      0x7c,      0x7c,
    0x00, // u
    0x00,      0x1c,      0x3c,      0x60,      0x60,      0x3c,      0x1c,
    0x00, // v
    0x00,      0x1c,      0x7c,      0x70,      0x38,      0x70,      0x7c,
    0x1c, // w
    0x00,      0x44,      0x6c,      0x38,      0x38,      0x6c,      0x44,
    0x00, // x
    0x00,      0x9c,      0xbc,      0xa0,      0xe0,      0x7c,      0x3c,
    0x00, // y
    0x00,      0x44,      0x64,      0x74,      0x5c,      0x4c,      0x44,
    0x00, // z
    0x00,      0x08,      0x3e,      0x77,      0x41,      0x41,      0x00,
    0x00, // {
    0x00,      0x00,      0x00,      0xff,      0xff,      0x00,      0x00,
    0x00, // |
    0x00,      0x00,      0x41,      0x41,      0x77,      0x3e,      0x08,
    0x00, // }
    0x00,      0x04,      0x02,      0x02,      0x04,      0x04,      0x02,
    0x00, // ~

    0b0000000, // 126+1; logo
    0b1010101, 0b0101010, 0b0101010, 0b0010100, 0b0010100, 0b0001000, 0b0001000,
    0b00000, // 126+2; s-meter, 0 bars
    0b00000,   0b00000,   0b00000,   0b00000,   0b00000,   0b00000,   0b00000,
    0b00000, // 126+3; s-meter, 1 bars
    0b00000,   0b00000,   0b11111,   0b00000,   0b00000,   0b00000,   0b00000,
    0b00000, // 126+4; s-meter, 2 bars
    0b00000,   0b00000,   0b11111,   0b00000,   0b11111,   0b00000,   0b00000,
    0b00000, // 126+5; s-meter, 3 bars
    0b00000,   0b00000,   0b11111,   0b00000,   0b11111,   0b00000,   0b11111,
    0b00000, // 126+6; vfo-a
    0b11100,   0b11110,   0b00101,   0b00101,   0b11110,   0b11100,   0b00000,
    0b00000, // 126+7; vfo-b
    0b11111,   0b11111,   0b10101,   0b10101,   0b01010,   0b01010,   0b00000};

#define FONT_W 8
#define FONT_H 2
#define FONT_STRETCHV 1
#define FONT_STRETCHH 0

static const uint8_t oled_init_sequence[] PROGMEM = {
    // Initialization Sequence
    // 0xAE,       // Display OFF (sleep mode)
    0xD5, 0x80, // 0x01--set display clock divide ratio/oscillator frequency
#ifdef CONDENSED
    0xA8, 0x3F, // Set multiplex ratio(1 to 64)   128x64
#else
    0xA8, 0x1F, // Set multiplex ratio(1 to 64)   128x32
#endif
    0xD3, 0x00,     // Set display offset. 00 = no offset
#ifndef OLED_SH1106 // for SSD1306 only:
    0x40,           // Set display start line address
    0x8D, 0x14,     // Set charge pump, internal VCC
    0x20, 0x02, // Set Memory Addressing; 0=Horizontal Mode; 1=Vertical Mode;
                // 2=Page Mode
    0xA4,       // Output RAM to Display  (display all on resume)
#endif          //! OLED_SH1106
    0xA1,       // Set Segment Re-map. A0=column 0 mapped to SEG0; A1=column 127
                // mapped to SEG0. Flip Horizontally
    0xC8,       // Set COM Output Scan Direction.  Flip Veritically.
#ifdef CONDENSED
    0xDA, 0x12, // Set com pins hardware configuration  128x64
#else
    0xDA, 0x02, // Set com pins hardware configuration  128x32
#endif
    0x81, 0x80, // Set contrast control register
    0xDB, 0x40, // Set vcomh 0x20 = 0.77xVcc
    0xD9, 0xF1, // 0xF1=brighter //0x22 Set pre-charge period
    0xB0 | 0x0, // Set page address, 0-7
#ifdef OLED_SH1106
    0xAD, 0x8B, // SH1106 Set pump mode: pump ON
    0x30 | 0x2, // SH1106 Pump voltage 8.0V
#endif          // OLED_SH1106
#ifdef INVERSE
    0xA7, // Set display mode: Inverse
#else
    0xA6, // Set display mode: Normal
#endif
    // 0x00,       // Set low nibble of column address
    // 0x10,       // Set high nibble of column address
    0xAF, // Display ON
};

#define OLED_ADDR 0x3C // Slave address
#define OLED_PAGES 4
#define OLED_COMMAND 0x00
#define OLED_DATA 0x40

void OLEDDevice::cmd(uint8_t b) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(OLED_COMMAND);
  Wire.write(b);
  Wire.endTransmission();
}
void OLEDDevice::begin(uint8_t cols, uint8_t rows, uint8_t charsize) {
  Wire.begin();
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(OLED_COMMAND);
  for (uint8_t i = 0; i < sizeof(oled_init_sequence); i++) {
    Wire.write(pgm_read_byte(&oled_init_sequence[i]));
  }
  Wire.endTransmission();
  delayMicroseconds(100);
#ifdef CONDENSED
  for (uint8_t y = 0; y != rows; y++)
    for (uint8_t x = 0; x != cols; x++) {
      setCursor(x, y);
      write(' ');
    } // clear
#endif
}
void OLEDDevice::noCursor() { curs = false; }
void OLEDDevice::cursor() { curs = true; }
void OLEDDevice::noDisplay() { cmd(0xAE); }
void OLEDDevice::createChar(uint8_t l, uint8_t glyph[]) {}

void OLEDDevice::_setCursor(uint8_t x, uint8_t y) {
  oledX = x;
  oledY = y;
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(OLED_COMMAND);
  Wire.write(renderingFrame | (oledY & 0x07));
  uint8_t _oledX = oledX;
#ifdef OLED_SH1106
  _oledX += 2; // SH1106 is a 132x64 controller.  Use middle 128 columns.
#endif
  Wire.write(0x10 | ((_oledX & 0xf0) >> 4));
  Wire.write(_oledX & 0x0f);
  Wire.endTransmission();
}
void OLEDDevice::drawCursor(bool en) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(OLED_DATA);
  Wire.write((en) ? 0xf0 : 0x00); // horizontal line
  Wire.endTransmission();
}
void OLEDDevice::setCursor(uint8_t x, uint8_t y) {
  if (curs) {
    drawCursor(false);
  }
  _setCursor(x * FONT_W, y * FONT_H);
  if (curs) {
    drawCursor(true);
    _setCursor(oledX, oledY);
  }
}

void OLEDDevice::newLine() {
  oledY += FONT_H;
  if (oledY > OLED_PAGES - FONT_H) {
    oledY = OLED_PAGES - FONT_H;
  }
  _setCursor(0, oledY);
}

size_t OLEDDevice::write(byte c) {
  if ((c == '\n') || (oledX > ((uint8_t)128 - FONT_W))) {
    if (wrap)
      newLine();
    return 1;
  }
  c = ((c < 9) ? (c + '~') : c) - ' ';

  uint16_t offset = ((uint16_t)c) * FONT_W / (FONT_STRETCHH + 1) * FONT_H;
  uint8_t line = FONT_H;
  do {
    if (FONT_STRETCHV)
      offset = ((uint16_t)c) * FONT_W / (FONT_STRETCHH + 1) * FONT_H /
               (2 * FONT_STRETCHV);
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_DATA);
    for (uint8_t i = 0; i < (FONT_W / (FONT_STRETCHH + 1)); i++) {
      uint8_t b = pgm_read_byte(&(font[offset++]));
      if (FONT_STRETCHV) {
        uint8_t b2 = 0;
        if (line > 1)
          for (int i = 0; i != 4; i++)
            b2 |= /* ! */ (b & (1 << i)) ? (1 << (i * 2)) | (1 << ((i * 2) + 1))
                                         : 0x00;
        else
          for (int i = 0; i != 4; i++)
            b2 |= /* ! */ (b & (1 << (i + 4)))
                      ? (1 << (i * 2)) | (1 << ((i * 2) + 1))
                      : 0x00;
        Wire.write(b2);
        if (FONT_STRETCHH)
          Wire.write(b2);
      } else {
        Wire.write(b);
        if (FONT_STRETCHH)
          Wire.write(b);
      }
    }
    Wire.endTransmission();
    if (FONT_H == 1) {
      oledX += FONT_W;
    } else {
      if (line > 1) {
        _setCursor(oledX, oledY + 1);
      } else {
        _setCursor(oledX + FONT_W, oledY - (FONT_H - 1));
      }
    }
  } while (--line);
  return 1;
}

void OLEDDevice::bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                        const uint8_t bitmap[]) {
  uint16_t j = 0;
  for (uint8_t y = y0; y < y1; y++) {
    _setCursor(x0, y);
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_DATA);
    for (uint8_t x = x0; x < x1; x++) {
      Wire.write(pgm_read_byte(&bitmap[j++]));
    }
    Wire.endTransmission();
  }
  setCursor(0, 0);
}

// ==========================================
// Global Display Object
// ==========================================
#ifdef BLIND
Display<Blind> lcd;
#else
#ifdef OLED
Display<OLEDDevice> lcd;
#else
Display<LCD> lcd;
#endif
#endif
