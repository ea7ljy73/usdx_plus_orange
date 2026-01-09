#include "wire.h"
#include "../usdx_settings.h"

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

I2C_ Wire;

void I2C_::start() {
  _I2C_INIT();
  _I2C_START();
}

void I2C_::stop() {
  _I2C_STOP();
  _I2C_SUSPEND();
}

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

void I2C_::begin() {}

void I2C_::beginTransmission(uint8_t addr) {
  start();
  SendByte(addr << 1);
}

bool I2C_::write(uint8_t byte) {
  SendByte(byte);
  return 1;
}

uint8_t I2C_::endTransmission() {
  stop();
  return 0;
}
