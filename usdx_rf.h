#ifndef USDX_RF_H
#define USDX_RF_H

#include "usdx_config.h"
#include "usdx_i2c.h"

// ==========================================
// Si5351 Class
// ==========================================
class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div; // note: uint8_t asserts fout > 3.5MHz with R_DIV=1
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  // volatile uint32_t _mod;
  volatile uint8_t pll_regs[8];

  volatile uint32_t fxtal = F_XTAL;

  inline void freq_calc_fast(int16_t df);
  inline void SendPLLRegisterBulk();
  void SendRegister(uint8_t reg, uint8_t *data, uint8_t n);
  void SendRegister(uint8_t reg, uint8_t val);

  int16_t iqmsa; // to detect a need for a PLL reset

  enum ms_t {
    PLLA = 0,
    PLLB = 1,
    MSNA = -2,
    MSNB = -1,
    MS0 = 0,
    MS1 = 1,
    MS2 = 2,
    MS3 = 3,
    MS4 = 4,
    MS5 = 5
  };

  void ms(int8_t n, uint32_t div_nom, uint32_t div_denom, uint8_t pll = PLLA,
          uint8_t _int = 0, uint16_t phase = 0, uint8_t rdiv = 0);
  void phase(int8_t n, uint32_t div_nom, uint32_t div_denom, uint16_t phase);
  void reset();
  void oe(uint8_t mask);
  void freq(int32_t fout, uint16_t i, uint16_t q);
  void freqb(uint32_t fout);
  uint8_t RecvRegister(uint8_t reg);
  void powerDown();
};

extern SI5351 si5351;

// ==========================================
// Filter Switching Classes
// ==========================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
class PCA9536 {
public:
#define PCA9536_ADDR 0x41
  inline void SendRegister(uint8_t reg, uint8_t val);
  inline void init();
  inline void write(uint8_t data);
};
extern PCA9536 ioext;
#endif

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
  inline void SendRegister(uint8_t reg, uint8_t val);
  inline void init();
  inline void write(uint16_t data);
};
extern IOExpander16 ioext;
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
#endif

#ifdef LPF_SWITCHING_WB2CBA_USDX_OCTOBAND
class MCP23008 {
public:
#define MCP23008_ADDR 0x20
  inline void SendRegister(uint8_t reg, uint8_t val);
  inline void init();
  inline void write(uint16_t data);
};
extern MCP23008 ioext;
#endif

// Helper functions
void set_lpf(uint8_t f);

#endif // USDX_RF_H
