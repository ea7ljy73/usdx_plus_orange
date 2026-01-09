#ifndef SI5351_H
#define SI5351_H

#include "../hardware/soft_i2c.h"
#include "../usdx_settings.h"
#include <Arduino.h>
#include <stdint.h>

// Si5351 Driver Module
// Controls the Si5351 Frequency Synthesizer

// External dependency on global i2c instance
extern I2C i2c;

/**
 * @brief Si5351 Frequency Synthesizer Driver.
 * Manages PLLs and Multisynths to generate RF and CLK signals.
 */
class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div; // note: uint8_t asserts fout > 3.5MHz with R_DIV=1
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  volatile uint8_t pll_regs[8];

#define BB0(x) ((uint8_t)(x)) // Bash byte x of int32_t
#define BB1(x) ((uint8_t)((x) >> 8))
#define BB2(x) ((uint8_t)((x) >> 16))

#define FAST __attribute__((optimize("Ofast")))

  volatile uint32_t fxtal = F_XTAL;

#define NEW_TX 1

  /**
   * @brief Fast Frequency Calculation.
   * Updates PLL registers for small frequency deviations (modulation).
   * @param df Frequency deviation in Hz.
   */
  void FAST freq_calc_fast(int16_t df);

  void SendPLLRegisterBulk();

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

#define SI_CLK_OE 3
};

extern SI5351 si5351;

#endif // SI5351_H
