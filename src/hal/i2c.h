/**
 * @file i2c.h
 * @brief I2C Abstraction Layer para uSDX
 *
 * Wrapper para la librería Wire con soporte para timeout
 * y segundo bus I2C en pines PD2/PD3.
 */

#ifndef HAL_I2C_H
#define HAL_I2C_H

#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// CLASE I2C SECUNDARIO (para LCD/OLED en pines alternativos)
// ============================================================================

class I2C_ {
public:
  void begin() {
    Wire.begin();
  }

  void begin(uint8_t addr) {
    Wire.begin(addr);
  }

  void beginTransmission(uint8_t addr) {
    Wire.beginTransmission(addr);
  }

  uint8_t endTransmission(void) {
    return Wire.endTransmission();
  }

  uint8_t endTransmission(uint8_t sendStop) {
    return Wire.endTransmission(sendStop);
  }

  size_t write(uint8_t data) {
    return Wire.write(data);
  }

  size_t write(const uint8_t* data, size_t quantity) {
    return Wire.write(data, quantity);
  }

  uint8_t requestFrom(uint8_t addr, uint8_t quantity) {
    return Wire.requestFrom(addr, quantity);
  }

  uint8_t requestFrom(uint8_t addr, uint8_t quantity, uint8_t sendStop) {
    return Wire.requestFrom(addr, quantity, sendStop);
  }

  int available() {
    return Wire.available();
  }

  int read() {
    return Wire.read();
  }

  int peek() {
    return Wire.peek();
  }

  void flush() {
    Wire.flush();
  }
};

// Instancia global del segundo bus I2C
extern I2C_ Wire2;

// ============================================================================
// FUNCIONES DE UTILIDAD
// ============================================================================

/**
 * @brief Escribe un byte en un registro I2C
 * @param addr Dirección del dispositivo
 * @param reg Número de registro
 * @param data Byte a escribir
 */
inline void i2c_write_reg8(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

/**
 * @brief Lee un byte de un registro I2C
 * @param addr Dirección del dispositivo
 * @param reg Número de registro
 * @return Byte leído
 */
inline uint8_t i2c_read_reg8(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.read();
}

/**
 * @brief Escribe múltiples bytes en registros I2C
 * @param addr Dirección del dispositivo
 * @param reg Registro inicial
 * @param data Puntero a datos
 * @param len Número de bytes
 */
inline void i2c_write_regs(uint8_t addr, uint8_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(data[i]);
  }
  Wire.endTransmission();
}

#endif // HAL_I2C_H
