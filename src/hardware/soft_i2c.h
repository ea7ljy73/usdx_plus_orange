#ifndef USDX_HARDWARE_SOFT_I2C_H
#define USDX_HARDWARE_SOFT_I2C_H

#include <Arduino.h>

class I2C {
public:
  I2C();
  ~I2C();
  void start();
  void stop();
  void SendByte(uint8_t data);
  uint8_t RecvBit(uint8_t mask);
  uint8_t RecvByte(uint8_t last);
  void resume();
  void suspend();

  // Arduino Wire-like API helpers
  void begin();
  void beginTransmission(uint8_t addr);
  bool write(uint8_t byte);
  uint8_t endTransmission();
};

extern I2C i2c;

#endif // USDX_HARDWARE_SOFT_I2C_H
