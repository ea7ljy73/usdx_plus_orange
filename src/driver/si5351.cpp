#include "si5351.h"

SI5351 si5351;

#ifdef NEW_TX
void FAST SI5351::freq_calc_fast(int16_t df) {
#define _MSC 0x10000
  uint32_t msb128 =
      _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;

  uint16_t msp1 = _msa128min512 + msb128 / _MSC;
  uint16_t msp2 = msb128;
  pll_regs[4] = BB0(msp1);
  pll_regs[5] = ((_MSC & 0xF0000) >> (16 - 4));
  pll_regs[6] = BB1(msp2);
  pll_regs[7] = BB0(msp2);
}

void SI5351::SendPLLRegisterBulk() {
  i2c.start();
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(26 + 0 * 8 + 4); // Write to PLLA
  i2c.SendByte(pll_regs[4]);
  i2c.SendByte(pll_regs[5]);
  i2c.SendByte(pll_regs[6]);
  i2c.SendByte(pll_regs[7]);
  i2c.stop();
}
#else
void FAST SI5351::freq_calc_fast(int16_t df) {
#define _MSC 0x80000
  uint32_t msb128 =
      _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;
  uint32_t msp1 = _msa128min512 + msb128 / _MSC;
  uint32_t msp2 = msb128 % _MSC;
  pll_regs[3] = BB1(msp1);
  pll_regs[4] = BB0(msp1);
  pll_regs[5] = ((_MSC & 0xF0000) >> (16 - 4)) | BB2(msp2);
  pll_regs[6] = BB1(msp2);
  pll_regs[7] = BB0(msp2);
}

void SI5351::SendPLLRegisterBulk() {
  i2c.start();
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(26 + 0 * 8 + 3); // Write to PLLA
  i2c.SendByte(pll_regs[3]);
  i2c.SendByte(pll_regs[4]);
  i2c.SendByte(pll_regs[5]);
  i2c.SendByte(pll_regs[6]);
  i2c.SendByte(pll_regs[7]);
  i2c.stop();
}
#endif

void SI5351::SendRegister(uint8_t reg, uint8_t *data, uint8_t n) {
  i2c.start();
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(reg);
  while (n--)
    i2c.SendByte(*data++);
  i2c.stop();
}

void SI5351::SendRegister(uint8_t reg, uint8_t val) {
  SendRegister(reg, &val, 1);
}

void SI5351::ms(int8_t n, uint32_t div_nom, uint32_t div_denom, uint8_t pll,
                uint8_t _int, uint16_t phase, uint8_t rdiv) {
  uint16_t msa;
  uint32_t msb, msc, msp1, msp2, msp3;
  msa = div_nom / div_denom;
  if (msa == 4)
    _int = 1;
  msb = (_int) ? 0 : (((uint64_t)(div_nom % div_denom) * _MSC) / div_denom);
  msc = (_int) ? 1 : _MSC;
  msp1 = 128 * msa + 128 * msb / msc - 512;
  msp2 = 128 * msb - 128 * msb / msc * msc;
  msp3 = msc;
  uint8_t ms_reg2 = BB2(msp1) | (rdiv << 4) | ((msa == 4) * 0x0C);
  uint8_t ms_regs[8] = {BB1(msp3), BB0(msp3),
                        ms_reg2,   BB1(msp1),
                        BB0(msp1), BB2(((msp3 & 0x0F0000) << 4) | msp2),
                        BB1(msp2), BB0(msp2)};

  SendRegister(n * 8 + 42, ms_regs, 8);
  if (n < 0) {
    SendRegister(n + 16 + 8, 0x80 | (0x40 * _int));
  } else {
    SendRegister(n + 16, ((pll) * 0x20) | 0x0C | 3 | (0x40 * _int));
    SendRegister(n + 165, (!_int) * phase * msa / 90);
  }
}

void SI5351::phase(int8_t n, uint32_t div_nom, uint32_t div_denom,
                   uint16_t phase) {
  SendRegister(n + 165, phase * (div_nom / div_denom) / 90);
}

void SI5351::reset() { SendRegister(177, 0xA0); }

void SI5351::oe(uint8_t mask) { SendRegister(3, ~mask); }

void SI5351::freq(int32_t fout, uint16_t i, uint16_t q) {
  uint8_t rdiv = 0;
  if (fout > 300000000) {
    i /= 3;
    q /= 3;
    fout /= 3;
  }
  if (fout < 500000) {
    rdiv = 7;
    fout *= 128;
  }
  uint16_t d;
  if (fout < 30000000)
    d = (16 * fxtal) / fout;
  else
    d = (32 * fxtal) / fout;
  if (fout < 3500000)
    d = (7 * fxtal) / fout;
  if (fout > 140000000)
    d = 4;
  if (d % 2)
    d++;
  if ((d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal))
    d += 2;
  uint32_t fvcoa = d * fout;
  ms(MSNA, fvcoa, fxtal);
  ms(MS0, fvcoa, fout, PLLA, 0, i, rdiv);
  ms(MS1, fvcoa, fout, PLLA, 0, q, rdiv);
#ifdef F_CLK2
  freqb(F_CLK2);
#else
  ms(MS2, fvcoa, fout, PLLA, 0, 0, rdiv);
#endif
  if (iqmsa != (((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90)) {
    iqmsa = ((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90;
    reset();
  }
  oe(0b00000011);

  _fout = fout;
  _div = d;
  _msa128min512 = fvcoa / fxtal * 128 - 512;
  _msb128 = ((uint64_t)(fvcoa % fxtal) * _MSC * 128) / fxtal;
}

void SI5351::freqb(uint32_t fout) {
  uint16_t d = (16 * fxtal) / fout;
  if (d % 2)
    d++;
  uint32_t fvcoa = d * fout;

  ms(MSNB, fvcoa, fxtal);
  ms(MS2, fvcoa, fout, PLLB, 0, 0, 0);
}

uint8_t SI5351::RecvRegister(uint8_t reg) {
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

void SI5351::powerDown() {
  SendRegister(3, 0b11111111);
  SendRegister(24, 0b00010000);
  SendRegister(25, 0b00000000);
  for (int addr = 16; addr != 24; addr++)
    SendRegister(addr, 0b10000000);
  SendRegister(187, 0);
  SendRegister(149, 0);
  SendRegister(183, 0b11010010);
}
