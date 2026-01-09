#include "io_expander.h"
#include "../usdx_settings.h"
#include "soft_i2c.h"

extern I2C i2c;

// =========================================================================================
// Implementation Details
// =========================================================================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
class PCA9536 {
public:
#define PCA9536_ADDR 0x41
  void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(PCA9536_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  void init() {
    SendRegister(0x03, 0x00);
  } // configuration cmd: IO0-IO7 as output
  void write(uint8_t data) {
    init();
    SendRegister(0x01, data);
  } // output port cmd: write bits D7-D0 to IO7-IO0
};

static PCA9536 ioext;

static void set_latch(uint8_t io) {
#define LATCH_TIME 15
  for (int i = 0; i != 8; i++) {
    ioext.write((~(1 << i)) | 0x01);
    delay(LATCH_TIME);
  }
  ioext.write(0x00);
  ioext.write((1 << io) | 0x00);
  delay(LATCH_TIME);
  ioext.write(0x00);
}

static uint8_t prev_lpf_io = 0xff;
void set_lpf(uint8_t f) {
  uint8_t lpf_io = (f > 8) ? 1 : (f > 4) ? 2 : 3;
  if (prev_lpf_io != lpf_io) {
    prev_lpf_io = lpf_io;
    set_latch(lpf_io);
  };
}
#endif // REV1

#if defined(LPF_SWITCHING_DL2MAN_USDX_REV3) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA)

class IOExpander16 {
public:
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
#define IOEXP16_ADDR 0x74
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2
#define IOEXP16_ADDR 0x24
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
#define IOEXP16_ADDR 0x20
#endif
  void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(IOEXP16_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  void init() { write(0U); }
  void write(uint16_t data) {
    SendRegister(0x07, 0xff);
    SendRegister(0x06, 0xff); /*Common last!*/
    SendRegister(0x02, data);
    SendRegister(0x06, 0x00); /*Common first!*/
    SendRegister(0x03, data >> 8);
    SendRegister(0x07, 0x00);
  }
};

static IOExpander16 ioext;

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

static void set_latch(uint8_t io, uint8_t common_io, bool latch = true) {
#define LATCH_TIME 30
  if (latch) {
    ioext.write((1U << io) | 0x0000);
    delay(LATCH_TIME);
    ioext.write(0x0000);
  } else {
    if (io == 0xff) {
      ioext.init();
      for (int io = 0; io != 16; io++)
        set_latch(io, common_io, latch);
    } else {
      ioext.write((~(1U << io)) | (1U << common_io));
      delay(LATCH_TIME);
      ioext.write(0x0000);
    }
  }
}

static uint8_t prev_lpf_io = 0xff;
void set_lpf(uint8_t f) {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  uint8_t lpf_io = (f > 26)   ? IO1_3
                   : (f > 20) ? IO1_4
                   : (f > 17) ? IO1_2
                   : (f > 12) ? IO1_5
                   : (f > 8)  ? IO1_1
                   : (f > 5)  ? IO1_6
                   : (f > 4)  ? IO1_0
                              : IO1_7;
#ifndef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_0, false);
    set_latch(lpf_io, IO0_0);
    prev_lpf_io = lpf_io;
  };
#else
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  };
#endif
#else // REV2 / REV2_BETA
  uint8_t lpf_io = (f > 12)  ? IO0_3
                   : (f > 8) ? IO0_5
                   : (f > 5) ? IO0_7
                   : (f > 4) ? IO1_1
                             : IO1_3;
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_1, false);
    set_latch(lpf_io, IO0_1);
    prev_lpf_io = lpf_io;
  };
#endif
}
#endif

#ifdef LPF_SWITCHING_WB2CBA_USDX_OCTOBAND
class MCP23008 {
public:
#define MCP23008_ADDR 0x20
  void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(MCP23008_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  void init() {
    SendRegister(0x09, 0x00);
    SendRegister(0x00, 0x00);
  }
  void write(uint16_t data) { SendRegister(0x09, data); }
};
static MCP23008 ioext;

static uint8_t prev_lpf_io = 0xff;
void set_lpf(uint8_t f) {
  uint8_t lpf_io = (f > 26)   ? 7
                   : (f > 20) ? 6
                   : (f > 17) ? 5
                   : (f > 12) ? 4
                   : (f > 8)  ? 3
                   : (f > 6)  ? 2
                   : (f > 4)  ? 1
                              : 0;
  if (prev_lpf_io == 0xff) {
    ioext.init();
  }
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  };
}
#endif

#if defined(LPF_SWITCHING_PE1DDA_USDXDUO)
void set_lpf(uint8_t f) {
  pinMode(PD5, OUTPUT);
  digitalWrite(PD5, (f >= LPF_SWITCHING_PE1DDA_USDXDUO));
}
#endif

#if !defined(LPF_SWITCHING_DL2MAN_USDX_REV1) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA) &&                           \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV3) &&                                \
    !defined(LPF_SWITCHING_WB2CBA_USDX_OCTOBAND) &&                            \
    !defined(LPF_SWITCHING_PE1DDA_USDXDUO)
void set_lpf(uint8_t f) {}
#endif
