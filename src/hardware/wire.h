#ifndef USDX_HARDWARE_WIRE_H
#define USDX_HARDWARE_WIRE_H

#include <Arduino.h>

class I2C_ { // Secundairy I2C class used by I2C LCD/OLED, uses alternate pins:
             // PD2 (SDA) and PD3 (SCL)
public:
  void start();
  void stop();
  void SendByte(uint8_t data);
  void SendRegister(uint8_t addr, uint8_t *data, uint8_t n);

  // Helpers for compatibility with standard Wire/SoftWire interfaces if needed
  void begin();
  void beginTransmission(uint8_t addr);
  bool write(uint8_t byte);
  uint8_t endTransmission();
};

extern I2C_ Wire;

#endif // USDX_HARDWARE_WIRE_H
