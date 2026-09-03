// lpf.h - uSDX Plus Orange v2
// Low-pass filter band switching (WHITE_BUTTONS LPF_SWITCHING_DL2MAN_USDX_REV3).
// Faithful port of usdx-legazy:1883-1917 (PCA/TCA9555 GPIO expander, 8-band
// latching relays). Uses the existing I2C bus (i2c) on PC4/PC5.

#pragma once

#include "i2c.h"
#include "usdx_settings.h"
#include <stdint.h>

#define LATCH_TIME 30 // set/reset time latching relay (ms)

class IOExpander16 {
public:
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
#  define IOEXP16_ADDR 0x20 // TCA/PCA9555 with A2=0 A1..A0=0
#endif
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(IOEXP16_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() { write(0U); }
  inline void write(uint16_t data) {
    SendRegister(0x07, 0xff);      // IO0 as input
    SendRegister(0x06, 0xff);      // IO1 as input
    SendRegister(0x02, data);      // IO0 output
    SendRegister(0x06, 0x00);      // IO1 output (common first)
    SendRegister(0x03, data >> 8); // IO1 output
    SendRegister(0x07, 0x00);
  }
};
IOExpander16 ioext;

enum gpioext_t {
  IO0_0,
  IO0_1,
  IO0_2,
  IO0_3,
  IO0_4,
  IO0_5,
  IO0_6,
  IO0_7,
  IO1_0,
  IO1_1,
  IO1_2,
  IO1_3,
  IO1_4,
  IO1_5,
  IO1_6,
  IO1_7
};

void set_latch(uint8_t io, uint8_t common_io, bool latch = true) {
  if(latch) {
    ioext.write((1U << io) | 0x0000);
    delay(LATCH_TIME);
    ioext.write(0x0000);
  } else {
    if(io == 0xff) {
      ioext.init();
      for(uint8_t i = 0; i != 16; i++)
        set_latch(i, common_io, latch);
    } else {
      ioext.write((~(1U << io)) | (1U << common_io));
      delay(LATCH_TIME);
      ioext.write(0x0000);
    }
  }
}

static uint8_t prev_lpf_io = 0xff; // inits & resets all latches

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
inline void set_lpf(uint8_t f) {
  // cut-off freq (MHz) -> IO port of LPF relay (legacy 1914)
  uint8_t lpf_io = (f > 26)   ? IO1_3
                   : (f > 20) ? IO1_4
                   : (f > 17) ? IO1_2
                   : (f > 12) ? IO1_5
                   : (f > 8)  ? IO1_1
                   : (f > 5)  ? IO1_6
                   : (f > 4)  ? IO1_0
                              : IO1_7;
  if(prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_0, false);
    set_latch(lpf_io, IO0_0);
    prev_lpf_io = lpf_io;
  }
}
#else
inline void set_lpf(uint8_t f) { (void)f; } // dummy
#endif