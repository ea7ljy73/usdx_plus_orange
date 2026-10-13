// Stub i2c.h: bus I2C grabador + modelo de registros del SI5351 (puntero
// de registro con autoincremento, como el chip real).
#pragma once
#include <stdint.h>
#include <string.h>
struct I2CRec {
  uint8_t  reg[256];   // estado del chip
  uint8_t  stream[64]; // bytes de la ultima transaccion
  uint8_t  n, ptr;
  uint32_t bytes, transactions; // contadores acumulados
  bool     drop;       // inyeccion de fallo: la proxima transaccion se pierde
  bool     dropping;
  I2CRec() { memset(this, 0, sizeof *this); }
  void start() { n = 0; dropping = drop; drop = false; transactions++; }
  void SendByte(uint8_t d) {
    if(n < 64) stream[n] = d;
    if(n == 1) ptr = d;
    else if(n > 1 && !dropping) reg[ptr++] = d;
    n++;
    bytes++;
  }
  void    stop() {}
  uint8_t RecvByte(uint8_t) { return 0; }
};
extern I2CRec i2c;
