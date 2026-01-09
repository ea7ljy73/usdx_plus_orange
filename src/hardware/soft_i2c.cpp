#include "soft_i2c.h"
#include "../usdx_settings.h"

// Implementation of Soft I2C (Bit-banging)

#if (F_MCU > 20900000)
#define I2C_DELAY 6
#else
#define I2C_DELAY                                                              \
  4 // Determines I2C Speed (2=939kb/s (too fast!!); 3=822kb/s; 4=731kb/s;
    // 5=658kb/s; 6=598kb/s). Increase this value when you get I2C tx errors
    // (E05); decrease this value when you get a CPU overload (E01). An
    // increment eats ~3.5% CPU load; minimum value is 3 on my QCX, resulting
    // in 84.5% CPU load
#endif

// Macros for pin manipulation (Moved from header)
#define I2C_DDR DDRC // Pins for the I2C bit banging
#define I2C_PIN PINC
#define I2C_PORT PORTC
// Note: I2C_SDA_PIN and I2C_SCL_PIN are defined in usdx_settings.h (renamed
// from SDA/SCL) But wait, the original code used hardcoded (1<<4) and (1<<5).
// Let's use the definitions from the original header which were:
// #define I2C_SDA (1 << 4) // PC4
// #define I2C_SCL (1 << 5) // PC5
// Checks usage in io_expander or settings. usdx_settings.h has definitions too.
// Ideally usage should be consistent.
// src/usdx_settings.h: #define I2C_SDA_PIN 18     // PC4    (pin 27)
// But 18 is the arduino pin number. Here we need the bitmask for PORTC.
// PC4 is bit 4.
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

I2C i2c; // Global instance

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
  I2C_SDA_LO(); // ensure SDA is LO so STOP-condition can be initiated by
                // pulling SCL HI (in case of ACK it SDA was already LO, but
                // for a delayed ACK or NACK it is not!)
  I2C_SCL_HI();
  I2C_SDA_HI();
  I2C_DDR &= ~(I2C_SDA | I2C_SCL); // prepare for a start: pull-up both SDA, SCL
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
    ; // wait util slave release SCL to HIGH (meaning data valid), or timeout
      // at 3ms
  // if(!i){ lcd.setCursor(0, 1); lcd.print(F("E07 I2C timeout")); }
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
  I2C_SDA_LO(); // pin sharing SDA/LCD_RS: pin sharing SDA/LCD_RS: pull-down
                // LCD_RS; QCXLiquidCrystal require this for any operation
}

void I2C::begin() {}

void I2C::beginTransmission(uint8_t addr) {
  start();
  SendByte(addr << 1);
}

bool I2C::write(uint8_t byte) {
  SendByte(byte);
  return 1;
}

uint8_t I2C::endTransmission() {
  stop();
  return 0;
}
