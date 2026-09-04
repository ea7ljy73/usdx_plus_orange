// i2c.h - uSDX Plus Orange v2
// Bit-banged I2C master, consolidated from the v1 monolithic .ino.
// Both buses share this single header (single source of truth).
//
// The SI5351 I2C runs inside the TX ISR (every sample @4800Hz, budget ~208us).
// Implementation keeps the exact proven v1 register-level bit-bang for timing.

#pragma once

#include <avr/io.h>
#include <stdint.h>

#ifndef I2C_DELAY_NOP
#  define I2C_DELAY_NOP 4 // ~731kb/s @20MHz (as v1)
#  define _I2C_DELAY()                                                                                                 \
    for(uint8_t i = 0; i != I2C_DELAY_NOP; i++)                                                                        \
      asm("nop");
#endif

// ---------------------------------------------------------------------------
// Display bus (I2C LCD/OLED) on PD2 (SDA) / PD3 (SCL)  [v1 I2C_]
// ---------------------------------------------------------------------------
class I2C_ {
public:
#define _I2C_SDA (1 << 2) // PD2
#define _I2C_SCL (1 << 3) // PD3
#define _I2C_INIT()                                                                                                    \
  PORTD &= ~_I2C_SDA;                                                                                                  \
  PORTD &= ~_I2C_SCL;                                                                                                  \
  _I2C_SDA_HI();                                                                                                       \
  _I2C_SCL_HI(); // open-drain
#define _I2C_SDA_HI() DDRD &= ~_I2C_SDA;
#define _I2C_SDA_LO() DDRD |= _I2C_SDA;
#define _I2C_SCL_HI()                                                                                                  \
  DDRD &= ~_I2C_SCL;                                                                                                   \
  _I2C_DELAY();
#define _I2C_SCL_LO()                                                                                                  \
  DDRD |= _I2C_SCL;                                                                                                    \
  _I2C_DELAY();
#define _I2C_START()                                                                                                   \
  _I2C_SDA_LO();                                                                                                       \
  _I2C_DELAY();                                                                                                        \
  _I2C_SCL_LO(); // _I2C_SDA_HI();
#define _I2C_STOP()                                                                                                    \
  _I2C_SDA_LO();                                                                                                       \
  _I2C_SCL_HI();                                                                                                       \
  _I2C_SDA_HI();
#define _SendBit(data, bit)                                                                                            \
  if(data & 1 << bit) {                                                                                                \
    _I2C_SDA_HI();                                                                                                     \
  } else {                                                                                                             \
    _I2C_SDA_LO();                                                                                                     \
  }                                                                                                                    \
  _I2C_SCL_HI();                                                                                                       \
  _I2C_SCL_LO();
  inline void start() {
    _I2C_INIT();
    _I2C_START();
  };
  inline void stop() { _I2C_STOP(); };
  inline void SendByte(uint8_t data) {
    _SendBit(data, 7);
    _SendBit(data, 6);
    _SendBit(data, 5);
    _SendBit(data, 4);
    _SendBit(data, 3);
    _SendBit(data, 2);
    _SendBit(data, 1);
    _SendBit(data, 0);
    _I2C_SDA_HI(); // recv ACK
    _I2C_DELAY();  //
    _I2C_SCL_HI();
    _I2C_SCL_LO();
  }
  void SendRegister(uint8_t addr, uint8_t* data, uint8_t n) {
    start();
    SendByte(addr << 1);
    while(n--)
      SendByte(*data++);
    stop();
  }
  void begin() {};
  void beginTransmission(uint8_t addr) {
    start();
    SendByte(addr << 1);
  };
  bool write(uint8_t byte) {
    SendByte(byte);
    return 1;
  };
  uint8_t endTransmission() {
    stop();
    return 0;
  };
};

// ---------------------------------------------------------------------------
// Primary bus (SI5351, I/O expanders) on PC4 (SDA) / PC5 (SCL)  [v1 I2C]
// ---------------------------------------------------------------------------
class I2C {
public:
#define I2C_DDR DDRC
#define I2C_PIN PINC
#define I2C_PORT PORTC
#define I2C_SDA (1 << 4) // PC4
#define I2C_SCL (1 << 5) // PC5
#define I2C_DELAY 4      // 731kb/s @20MHz (usdx-legazy)
#define DELAY(n)                                                                                                       \
  for(uint8_t i = 0; i != n; i++)                                                                                      \
  asm("nop")
#define I2C_SDA_GET() I2C_PIN& I2C_SDA
#define I2C_SCL_GET() I2C_PIN& I2C_SCL
#define I2C_SDA_HI() I2C_DDR &= ~I2C_SDA;
#define I2C_SDA_LO() I2C_DDR |= I2C_SDA;
#define I2C_SCL_HI()                                                                                                   \
  I2C_DDR &= ~I2C_SCL;                                                                                                 \
  DELAY(I2C_DELAY);
#define I2C_SCL_LO()                                                                                                   \
  I2C_DDR |= I2C_SCL;                                                                                                  \
  DELAY(I2C_DELAY);

  // Pin sharing SDA(PC4)/LCD_RS mitigation (usdx-legazy 1353-1360)
  inline void resume() {
#ifdef LCD_RS_PORTIO
    I2C_PORT &= ~I2C_SDA; // pin sharing SDA/LCD_RS mitigation
#endif
  }
  inline void suspend() { I2C_SDA_LO(); } // pull-down LCD_RS; required by LCD

  I2C() {
    I2C_PORT &= ~(I2C_SDA | I2C_SCL);
    I2C_SCL_HI();
    I2C_SDA_HI();
    suspend();
  }
  ~I2C() {
    I2C_PORT &= ~(I2C_SDA | I2C_SCL);
    I2C_DDR &= ~(I2C_SDA | I2C_SCL);
  }
  inline void start() {
    resume(); // prepare for I2C
    I2C_SCL_LO();
    I2C_SDA_HI();
  }
  inline void stop() {
    I2C_SDA_LO(); // ensure SDA LO so STOP can be initiated
    I2C_SCL_HI();
    I2C_SDA_HI();
    I2C_DDR &= ~(I2C_SDA | I2C_SCL); // prepare for a start: pull-up both
    suspend();
  }
#define SendBit(data, mask)                                                                                            \
  if(data & mask) {                                                                                                    \
    I2C_SDA_HI();                                                                                                      \
  } else {                                                                                                             \
    I2C_SDA_LO();                                                                                                      \
  }                                                                                                                    \
  I2C_SCL_HI();                                                                                                        \
  I2C_SCL_LO();
  inline void SendByte(uint8_t data) {
    SendBit(data, 1 << 7);
    SendBit(data, 1 << 6);
    SendBit(data, 1 << 5);
    SendBit(data, 1 << 4);
    SendBit(data, 1 << 3);
    SendBit(data, 1 << 2);
    SendBit(data, 1 << 1);
    SendBit(data, 1 << 0);
    I2C_SDA_HI(); // recv ACK
    _I2C_DELAY();
    I2C_SCL_HI();
    I2C_SCL_LO();
  }
  inline uint8_t RecvBit(uint8_t mask) {
    I2C_SCL_HI();
    uint16_t i = 60000;
    for(; !(I2C_SCL_GET()) && i; i--)
      ; // wait until slave releases SCL (or timeout ~3ms)
    uint8_t data = I2C_SDA_GET();
    I2C_SCL_LO();
    return (data) ? mask : 0;
  }
  inline uint8_t RecvByte(uint8_t last) {
    uint8_t data = 0;
    data |= RecvBit(1 << 7);
    data |= RecvBit(1 << 6);
    data |= RecvBit(1 << 5);
    data |= RecvBit(1 << 4);
    data |= RecvBit(1 << 3);
    data |= RecvBit(1 << 2);
    data |= RecvBit(1 << 1);
    data |= RecvBit(1 << 0);
    if(last) {
      I2C_SDA_HI(); // NACK
    } else {
      I2C_SDA_LO(); // ACK
    }
    _I2C_DELAY();
    I2C_SCL_HI();
    I2C_SDA_HI(); // restore SDA for read
    I2C_SCL_LO();
    return data;
  }
  void begin() {};
  void beginTransmission(uint8_t addr) {
    start();
    SendByte(addr << 1);
  };
  bool write(uint8_t byte) {
    SendByte(byte);
    return 1;
  };
  uint8_t endTransmission() {
    stop();
    return 0;
  };
};

// Instances
I2C_ Wire; // display bus
I2C  i2c;  // main bus (SI5351 / expanders)
