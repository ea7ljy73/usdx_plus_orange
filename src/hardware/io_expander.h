#ifndef IO_EXPANDER_H
#define IO_EXPANDER_H

#include "soft_i2c.h"
#include <Arduino.h>

// IO Expander & LPF Switching Module
// Supports PCA9536, IOExpander16 (PCA9539/TCA9555), MCP23008

extern I2C i2c; // Dependency on global SoftI2C instance
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
class PCA9536 {
public:
#define PCA9536_ADDR                                                           \
  0x41 // PCA9536   https://www.ti.com/lit/ds/symlink/pca9536.pdf
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(PCA9536_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    SendRegister(0x03, 0x00);
  } // configuration cmd: IO0-IO7 as output
  inline void write(uint8_t data) {
    init();
    SendRegister(0x01, data);
  } // output port cmd: write bits D7-D0 to IO7-IO0
};
PCA9536 ioext;

void set_latch(
    uint8_t io) { // reset all latches and set latch k to corresponding GPIO,
                  // all relays share a common (ground) GPIO
#define LATCH_TIME 15 // set/reset time latch relay
  for (int i = 0; i != 8; i++) {
    ioext.write((~(1 << i)) | 0x01);
    delay(LATCH_TIME);
  }
  ioext.write(0x00); // reset all latches
  ioext.write((1 << io) | 0x00);
  delay(LATCH_TIME);
  ioext.write(0x00); // set latch wired to io port
}

static uint8_t prev_lpf_io = 0xff;
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io =
      (f > 8)   ? 1
      : (f > 4) ? 2
                : /*(f <= 4)*/ 3; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io != lpf_io) {
    prev_lpf_io = lpf_io;
    set_latch(lpf_io);
  }; // set relay
}
#endif // LPF_SWITCHING_DL2MAN_USDX_REV1

#if defined(LPF_SWITCHING_DL2MAN_USDX_REV3) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA)
class IOExpander16 {
public:
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
#define IOEXP16_ADDR                                                           \
  0x74 // PCA9539 with A1..A0 set to 0
       // https://www.nxp.com/docs/en/data-sheet/PCA9539_PCA9539R.pdf
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2
#define IOEXP16_ADDR                                                           \
  0x24 // TCA/PCA9555 with A2=1 A1..A0=0
       // https://www.ti.com/lit/ds/symlink/tca9555.pdf
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
#define IOEXP16_ADDR                                                           \
  0x20 // TCA/PCA9555 with A2=0 A1..A0=0
       // https://www.ti.com/lit/ds/symlink/tca9555.pdf
#endif
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(IOEXP16_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    write(0U);
  } // IO0, IO1 as input, IO0 to 0, IO0 as output, IO1 to 0, IO1 as output
  inline void write(uint16_t data) {
    SendRegister(0x07, 0xff);
    SendRegister(0x06, 0xff); /*Common last!*/
    SendRegister(0x02, data);
    SendRegister(0x06, 0x00); /*Common first!*/
    SendRegister(0x03, data >> 8);
    SendRegister(0x07, 0x00);
  } // output port cmd: write bits D15-D0 to IO1.7-0.0;
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

void set_latch(
    uint8_t io, uint8_t common_io,
    bool latch = true) { // reset all latches and set latch k to corresponding
                         // GPIO, all relays share a common (ground) GPIO
#define LATCH_TIME 30    // set/reset time latch relay
  if (latch) {
    ioext.write((1U << io) | 0x0000);
    delay(LATCH_TIME);
    ioext.write(0x0000); // set latch wired to io port
  } else {
    if (io == 0xff) {
      ioext.init();
      for (int io = 0; io != 16; io++)
        set_latch(io, common_io, latch);
    } // reset all latches
    else {
      ioext.write((~(1U << io)) | (1U << common_io));
      delay(LATCH_TIME);
      ioext.write(0x0000);
    } // reset latch wired to io port
  }
}

static uint8_t prev_lpf_io = 0xff; // inits and resets all latches
inline void set_lpf(uint8_t f) {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  uint8_t lpf_io =
      (f > 26)   ? IO1_3
      : (f > 20) ? IO1_4
      : (f > 17) ? IO1_2
      : (f > 12) ? IO1_5
      : (f > 8)  ? IO1_1
      : (f > 5)  ? IO1_6
      : (f > 4)
          ? IO1_0
          : /*(f <= 4)*/ IO1_7; // cut-off freq in MHz to IO port of LPF relay
#ifndef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_0, false);
    set_latch(lpf_io, IO0_0);
    prev_lpf_io = lpf_io;
  }; // set relay (latched)
#else
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }; // set relay (non-latched)
#endif // LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
#else  // LPF_SWITCHING_DL2MAN_USDX_REV2 LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
  uint8_t lpf_io =
      (f > 12)  ? IO0_3
      : (f > 8) ? IO0_5
      : (f > 5) ? IO0_7
      : (f > 4)
          ? IO1_1
          : /*(f <= 4)*/ IO1_3; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_1, false);
    set_latch(lpf_io, IO0_1);
    prev_lpf_io = lpf_io;
  }; // set relay

#endif
}
#endif // LPF_SWITCHING_DL2MAN_USDX_REV3 LPF_SWITCHING_DL2MAN_USDX_REV2
       // REV2_BETA

#ifdef LPF_SWITCHING_WB2CBA_USDX_OCTOBAND
class MCP23008 {
public:
#define MCP23008_ADDR                                                          \
  0x20 // MCP23008 with A1..A0 set to 0
       // https://ww1.microchip.com/downloads/en/DeviceDoc/21919e.pdf
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(MCP23008_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    SendRegister(0x09, 0x00);
    SendRegister(0x00, 0x00);
  } // GP0-7 to 0, GP0-7 as output
  inline void write(uint16_t data) {
    SendRegister(0x09, data);
  } // output port cmd: write bits D7-D0 to GP7-GP0
};
MCP23008 ioext;

static uint8_t prev_lpf_io = 0xff; // inits and resets all latches
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io =
      (f > 26)   ? 7
      : (f > 20) ? 6
      : (f > 17) ? 5
      : (f > 12) ? 4
      : (f > 8)  ? 3
      : (f > 6)  ? 2
      : (f > 4)  ? 1
                : /*(f <= 4)*/ 0; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io == 0xff) {
    ioext.init();
  }
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }; // set relay (non-latched)
}
#endif // LPF_SWITCHING_WB2CBA_USDX_OCTOBAND

#if defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {
  pinMode(PD5, OUTPUT);
  digitalWrite(PD5, (f >= LPF_SWITCHING_PE1DDA_USDXDUO));
}
#endif // LPF_SWITCHING_PE1DDA_USDXDUO

#if !defined(LPF_SWITCHING_DL2MAN_USDX_REV1) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA) &&                           \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV3) &&                                \
    !defined(LPF_SWITCHING_WB2CBA_USDX_OCTOBAND) &&                            \
    !defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {} // dummy
#endif

#endif // IO_EXPANDER_H
