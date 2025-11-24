#ifndef USDX_I2C_H
#define USDX_I2C_H

#include "usdx_config.h"

// ==========================================
// I2C_ Class (Secondary I2C for Display)
// ==========================================
class I2C_ {
public:
  void start();
  void stop();
  void SendByte(uint8_t data);
  void SendRegister(uint8_t addr, uint8_t *data, uint8_t n);
  void begin() {};
  void beginTransmission(uint8_t addr);
  bool write(uint8_t byte);
  uint8_t endTransmission();
};

extern I2C_ Wire;

// ==========================================
// I2C Class (Primary I2C for RF/Si5351)
// ==========================================
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
  void begin() {};
  void beginTransmission(uint8_t addr);
  bool write(uint8_t byte);
  uint8_t endTransmission();
};

#endif // USDX_I2C_H
