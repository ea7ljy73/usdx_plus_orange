// si5351.h - uSDX Plus Orange v2
// SI5351A clock generator driver, extracted from the v1 monolithic .ino.
// Functional behavior is identical (essentially byte-for-byte; NEW_TX branch).

#pragma once

#include "i2c.h"
#include "usdx_settings.h"
#include <stdint.h>

class SI5351 {
public:
  volatile int32_t  _fout;
  volatile uint8_t  _div; // note: uint8_t asserts fout > 3.5MHz with R_DIV=1
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  volatile uint8_t  pll_regs[8];

#define BB0(x) ((uint8_t)(x))
#define BB1(x) ((uint8_t)((x) >> 8))
#define BB2(x) ((uint8_t)((x) >> 16))

#define FAST __attribute__((optimize("Ofast")))

  volatile uint32_t fxtal = F_XTAL;

  inline void FAST freq_calc_fast(int32_t df) {
    // note: relies on cached variables: _msb128, _msa128min512, _div, fxtal
#define _MSC 0x10000
    uint32_t msb128 = _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;

    uint16_t msp1 = _msa128min512 + msb128 / _MSC;
    uint16_t msp2 = msb128; // = msb128 % _MSC (since _MSC = 2^16)

    pll_regs[4] = BB0(msp1);
    pll_regs[5] = ((_MSC & 0xF0000) >> (16 - 4)); // top nibble == _MSC top nibble
    pll_regs[6] = BB1(msp2);
    pll_regs[7] = BB0(msp2);
  }

  inline void SendPLLRegisterBulk() {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(26 + 0 * 8 + 4); // Write to PLLA
    i2c.SendByte(pll_regs[4]);
    i2c.SendByte(pll_regs[5]);
    i2c.SendByte(pll_regs[6]);
    i2c.SendByte(pll_regs[7]);
    i2c.stop();
  }

  void SendRegister(uint8_t reg, uint8_t* data, uint8_t n) {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    while(n--)
      i2c.SendByte(*data++);
    i2c.stop();
  }
  void    SendRegister(uint8_t reg, uint8_t val) { SendRegister(reg, &val, 1); }
  int16_t iqmsa; // to detect a need for a PLL reset

  enum ms_t { PLLA = 0, PLLB = 1, MSNA = -2, MSNB = -1, MS0 = 0, MS1 = 1, MS2 = 2 };

  void ms(int8_t n, uint32_t div_nom, uint32_t div_denom, uint8_t pll = PLLA, uint8_t _int = 0, uint16_t phase = 0,
          uint8_t rdiv = 0) {
    uint16_t msa;
    uint32_t msb, msc, msp1, msp2, msp3;
    msa = div_nom / div_denom;
    if(msa == 4)
      _int = 1; // MSx_INT requirement, AN619 4.1.3
    msb                = (_int) ? 0 : (((uint64_t)(div_nom % div_denom) * _MSC) / div_denom);
    msc                = (_int) ? 1 : _MSC;
    msp1               = 128 * msa + 128 * msb / msc - 512;
    msp2               = 128 * msb - 128 * msb / msc * msc;
    msp3               = msc;
    uint8_t ms_reg2    = BB2(msp1) | (rdiv << 4) | ((msa == 4) * 0x0C);
    uint8_t ms_regs[8] = {BB1(msp3), BB0(msp3), ms_reg2, BB1(msp1), BB0(msp1), BB2(((msp3 & 0x0F0000) << 4) | msp2),
                          BB1(msp2), BB0(msp2)};

    SendRegister(n * 8 + 42, ms_regs, 8); // Write to MSx
    if(n < 0) {
      SendRegister(n + 16 + 8, 0x80 | (0x40 * _int)); // MSNx PLLn
    } else {
      SendRegister(n + 16, ((pll) * 0x20) | 0x0C | 3 | (0x40 * _int)); // MSx CLKn
      SendRegister(n + 165, (!_int) * phase * msa / 90);
    }
  }

  void phase(int8_t n, uint32_t div_nom, uint32_t div_denom, uint16_t phase) {
    SendRegister(n + 165, phase * (div_nom / div_denom) / 90);
  }

  void reset() { SendRegister(177, 0xA0); } // 0x20 reset PLLA; 0x80 reset PLLB
  void oe(uint8_t mask) { SendRegister(3, ~mask); }

  void freq(int32_t fout, uint16_t i, uint16_t q) {
    uint8_t rdiv = 0;
    if(fout > 300000000) {
      i /= 3;
      q /= 3;
      fout /= 3;
    }
    if(fout < 500000) {
      rdiv = 7;
      fout *= 128;
    }
    uint16_t d;
    if(fout < 30000000)
      d = (16 * fxtal) / fout;
    else
      d = (32 * fxtal) / fout;
    if(fout < 3500000)
      d = (7 * fxtal) / fout;
    if(fout > 140000000)
      d = 4;
    if(d % 2)
      d++;
    if((d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal))
      d += 2; // keep multiplier constant over +/-5kHz
    uint32_t fvcoa = d * fout;

    ms(MSNA, fvcoa, fxtal); // PLLA in fractional mode
    ms(MS0, fvcoa, fout, PLLA, 0, i, rdiv);
    ms(MS1, fvcoa, fout, PLLA, 0, q, rdiv);
    ms(MS2, fvcoa, fout, PLLA, 0, 0, rdiv);
    if(iqmsa != (((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90)) {
      iqmsa = ((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90;
      reset();
    }
    oe(0b00000011); // enable CLK0, CLK1

    _fout         = fout;
    _div          = d;
    _msa128min512 = fvcoa / fxtal * 128 - 512;
    _msb128       = ((uint64_t)(fvcoa % fxtal) * _MSC * 128) / fxtal;
  }

  uint8_t RecvRegister(uint8_t reg) {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    i2c.stop();
    i2c.start();
    i2c.SendByte((SI5351_ADDR << 1) | 1);
    uint8_t data = i2c.RecvByte(true);
    i2c.stop();
    return data;
  }
  void powerDown() {
    SendRegister(3, 0b11111111); // Disable all CLK outputs
    SendRegister(24, 0);
    SendRegister(25, 0);
    for(int addr = 16; addr != 24; addr++)
      SendRegister(addr, 0b10000000);
    SendRegister(187, 0);
    SendRegister(149, 0);
    SendRegister(183, 0b11010010);
  }
#define SI_CLK_OE 3
};

extern SI5351 si5351;