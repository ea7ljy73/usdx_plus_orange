#include "usdx_i2c.h"

// ==========================================
// I2C_ Implementation
// ==========================================

// I2C_ Macros
#if (F_MCU > 20900000)
#ifdef OLED_SH1106
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 9; i++)                                             \
    asm("nop");
#else
#ifdef OLED_SSD1306
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 6; i++)                                             \
    asm("nop");
#else // other (I2C_LCD)
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 7; i++)                                             \
    asm("nop");
#endif
#endif
#else // slow F_MCU
#ifdef OLED_SH1106
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 8; i++)                                             \
    asm("nop");
#else
#ifdef OLED_SSD1306
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 4; i++)                                             \
    asm("nop"); // 4=731kb/s
#else           // other (I2C_LCD)
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 5; i++)                                             \
    asm("nop");
#endif
#endif
#endif // F_MCU

#define _I2C_SDA (1 << 2) // PD2
#define _I2C_SCL (1 << 3) // PD3

#ifdef _I2C_DIRECT_IO
#define _I2C_INIT()                                                            \
  _I2C_SDA_HI();                                                               \
  _I2C_SCL_HI();                                                               \
  DDRD |= (_I2C_SDA | _I2C_SCL); // direct I/O (no need for pull-ups)
#define _I2C_SDA_HI() PORTD |= _I2C_SDA;
#define _I2C_SDA_LO() PORTD &= ~_I2C_SDA;
#define _I2C_SCL_HI()                                                          \
  PORTD |= _I2C_SCL;                                                           \
  _DELAY();
#define _I2C_SCL_LO()                                                          \
  PORTD &= ~_I2C_SCL;                                                          \
  _DELAY();
#else // !_I2C_DIRECT_IO
#define _I2C_INIT()                                                            \
  PORTD &= ~_I2C_SDA;                                                          \
  PORTD &= ~_I2C_SCL;                                                          \
  _I2C_SDA_HI();                                                               \
  _I2C_SCL_HI(); // open-drain
#define _I2C_SDA_HI() DDRD &= ~_I2C_SDA;
#define _I2C_SDA_LO() DDRD |= _I2C_SDA;
#define _I2C_SCL_HI()                                                          \
  DDRD &= ~_I2C_SCL;                                                           \
  _DELAY();
#define _I2C_SCL_LO()                                                          \
  DDRD |= _I2C_SCL;                                                            \
  _DELAY();
#endif // !_I2C_DIRECT_IO

#define _I2C_START()                                                           \
  _I2C_SDA_LO();                                                               \
  _DELAY();                                                                    \
  _I2C_SCL_LO(); // _I2C_SDA_HI();
#define _I2C_STOP()                                                            \
  _I2C_SDA_LO();                                                               \
  _I2C_SCL_HI();                                                               \
  _I2C_SDA_HI();
#define _I2C_SUSPEND() //_I2C_SDA_LO(); // SDA_LO to allow re-use as output port

#define _SendBit(data, bit)                                                    \
  if (data & 1 << bit) {                                                       \
    _I2C_SDA_HI();                                                             \
  } else {                                                                     \
    _I2C_SDA_LO();                                                             \
  }                                                                            \
  _I2C_SCL_HI();                                                               \
  _I2C_SCL_LO();

void I2C_::start() {
  _I2C_INIT();
  _I2C_START();
};
void I2C_::stop() {
  _I2C_STOP();
  _I2C_SUSPEND();
};
void I2C_::SendByte(uint8_t data) {
  _SendBit(data, 7);
  _SendBit(data, 6);
  _SendBit(data, 5);
  _SendBit(data, 4);
  _SendBit(data, 3);
  _SendBit(data, 2);
  _SendBit(data, 1);
  _SendBit(data, 0);
  _I2C_SDA_HI(); // recv ACK
  _DELAY();      //
  _I2C_SCL_HI();
  _I2C_SCL_LO();
}

void I2C_::SendRegister(uint8_t addr, uint8_t *data, uint8_t n) {
  start();
  SendByte(addr << 1);
  while (n--)
    SendByte(*data++);
  stop();
}

void I2C_::beginTransmission(uint8_t addr) {
  start();
  SendByte(addr << 1);
};
bool I2C_::write(uint8_t byte) {
  SendByte(byte);
  return 1;
};
uint8_t I2C_::endTransmission() {
  stop();
  return 0;
};

I2C_ Wire;

// ==========================================
// I2C Implementation
// ==========================================

#if (F_MCU > 20900000)
#define I2C_DELAY 6
#else
#define I2C_DELAY 4
#endif
#define I2C_DDR DDRC // Pins for the I2C bit banging
#define I2C_PIN PINC
#define I2C_PORT PORTC
#define I2C_SDA (1 << 4) // PC4
#define I2C_SCL (1 << 5) // PC5
#define DELAY(n)                                                               \
  for (uint8_t i = 0; i != n; i++)                                             \
    asm("nop");
#define I2C_SDA_GET() I2C_PIN &I2C_SDA
#define I2C_SCL_GET() I2C_PIN &I2C_SCL
#define I2C_SDA_HI() I2C_DDR &= ~I2C_SDA;
#define I2C_SDA_LO() I2C_DDR |= I2C_SDA;
#define I2C_SCL_HI()                                                           \
  I2C_DDR &= ~I2C_SCL;                                                         \
  DELAY(I2C_DELAY);
#define I2C_SCL_LO()                                                           \
  I2C_DDR |= I2C_SCL;                                                          \
  DELAY(I2C_DELAY);

I2C::I2C() {
  I2C_PORT &= ~(I2C_SDA | I2C_SCL);
  I2C_SCL_HI();
  I2C_SDA_HI();
#ifndef RS_HIGH_ON_IDLE
  suspend();
#endif
}

I2C::~I2C() {
  I2C_PORT &= ~(I2C_SDA | I2C_SCL);
  I2C_DDR &= ~(I2C_SDA | I2C_SCL);
}

void I2C::start() {
#ifdef RS_HIGH_ON_IDLE
  I2C_SDA_LO();
#else
  resume(); // prepare for I2C
#endif
  I2C_SCL_LO();
  I2C_SDA_HI();
}

void I2C::stop() {
  I2C_SDA_LO();
  I2C_SCL_HI();
  I2C_SDA_HI();
  I2C_DDR &= ~(I2C_SDA | I2C_SCL);
#ifndef RS_HIGH_ON_IDLE
  suspend();
#endif
}

#define SendBit(data, mask)                                                    \
  if (data & mask) {                                                           \
    I2C_SDA_HI();                                                              \
  } else {                                                                     \
    I2C_SDA_LO();                                                              \
  }                                                                            \
  I2C_SCL_HI();                                                                \
  I2C_SCL_LO();

void I2C::SendByte(uint8_t data) {
  SendBit(data, 1 << 7);
  SendBit(data, 1 << 6);
  SendBit(data, 1 << 5);
  SendBit(data, 1 << 4);
  SendBit(data, 1 << 3);
  SendBit(data, 1 << 2);
  SendBit(data, 1 << 1);
  SendBit(data, 1 << 0);
  I2C_SDA_HI(); // recv ACK
  DELAY(I2C_DELAY);
  I2C_SCL_HI();
  I2C_SCL_LO();
}

uint8_t I2C::RecvBit(uint8_t mask) {
  I2C_SCL_HI();
  uint16_t i = 60000;
  for (; !(I2C_SCL_GET()) && i; i--)
    ;
  uint8_t data = I2C_SDA_GET();
  I2C_SCL_LO();
  return (data) ? mask : 0;
}

uint8_t I2C::RecvByte(uint8_t last) {
  uint8_t data = 0;
  data |= RecvBit(1 << 7);
  data |= RecvBit(1 << 6);
  data |= RecvBit(1 << 5);
  data |= RecvBit(1 << 4);
  data |= RecvBit(1 << 3);
  data |= RecvBit(1 << 2);
  data |= RecvBit(1 << 1);
  data |= RecvBit(1 << 0);
  if (last) {
    I2C_SDA_HI(); // NACK
  } else {
    I2C_SDA_LO(); // ACK
  }
  DELAY(I2C_DELAY);
  I2C_SCL_HI();
  I2C_SDA_HI(); // restore SDA for read
  I2C_SCL_LO();
  return data;
}

void I2C::resume() {
#ifdef LCD_RS_PORTIO
  I2C_PORT &= ~I2C_SDA; // pin sharing SDA/LCD_RS mitigation
#endif
}

void I2C::suspend() {
  I2C_SDA_LO(); // pin sharing SDA/LCD_RS: pull-down LCD_RS; QCXLiquidCrystal
                // require this for any operation
}

void I2C::beginTransmission(uint8_t addr) {
  start();
  SendByte(addr << 1);
};
bool I2C::write(uint8_t byte) {
  SendByte(byte);
  return 1;
};
uint8_t I2C::endTransmission() {
  stop();
  return 0;
};
