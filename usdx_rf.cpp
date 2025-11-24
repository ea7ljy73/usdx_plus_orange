#include "usdx_rf.h"

// Global instance
SI5351 si5351;
I2C i2c; // Used by SI5351 and Filters

// ==========================================
// Si5351 Implementation
// ==========================================

#define BB0(x) ((uint8_t)(x)) // Bash byte x of int32_t
#define BB1(x) ((uint8_t)((x) >> 8))
#define BB2(x) ((uint8_t)((x) >> 16))
#define FAST __attribute__((optimize("Ofast")))

#define NEW_TX 1
#ifdef NEW_TX
inline void FAST
SI5351::freq_calc_fast(int16_t df) // note: relies on cached variables: _msb128,
                                   // _msa128min512, _div, _fout, fxtal
{
#define _MSC 0x10000
  uint32_t msb128 =
      _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;

  uint16_t msp1 =
      _msa128min512 + msb128 / _MSC; // = 128 * _msa + msb128 / _MSC - 512;
  uint16_t msp2 =
      msb128; // = msb128 % _MSC;  assuming MSC is covering exact uint16_t so
              // the mod operation can dissapear (and the upper BB2 byte) // =
              // msb128 - msb128/_MSC * _MSC;

  // pll_regs[0] = BB1(msc);  // 3 regs are constant
  // pll_regs[1] = BB0(msc);
  // pll_regs[2] = BB2(msp1);
  // pll_regs[3] = BB1(msp1);
  pll_regs[4] = BB0(msp1);
  pll_regs[5] =
      ((_MSC & 0xF0000) >>
       (16 - 4)) /*|BB2(msp2)*/; // top nibble MUST be same as top nibble of
                                 // _MSC !  assuming that BB2(msp2) is always 0
                                 // -> so reg is constant
  pll_regs[6] = BB1(msp2);
  pll_regs[7] = BB0(msp2);
}

inline void SI5351::SendPLLRegisterBulk() {
  i2c.start();
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(26 + 0 * 8 + 4); // Write to PLLA
  // i2c.SendByte(26+1*8 + 4);  // Write to PLLB
  i2c.SendByte(pll_regs[4]);
  i2c.SendByte(pll_regs[5]);
  i2c.SendByte(pll_regs[6]);
  i2c.SendByte(pll_regs[7]);
  i2c.stop();
}
#else                // !NEW_TX
inline void FAST
SI5351::freq_calc_fast(int16_t df) // note: relies on cached variables: _msb128,
                                   // _msa128min512, _div, _fout, fxtal
{
#define _MSC 0x80000 // 0x80000: 98% CPU load   0xFFFFF: 114% CPU load
  uint32_t msb128 =
      _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;

  uint32_t msp1 =
      _msa128min512 + msb128 / _MSC; // = 128 * _msa + msb128 / _MSC - 512;
  uint32_t msp2 = msb128 % _MSC;     // = msb128 - msb128/_MSC * _MSC;

  // pll_regs[0] = BB1(msc);  // 3 regs are constant
  // pll_regs[1] = BB0(msc);
  // pll_regs[2] = BB2(msp1);
  pll_regs[3] = BB1(msp1);
  pll_regs[4] = BB0(msp1);
  pll_regs[5] = ((_MSC & 0xF0000) >> (16 - 4)) |
                BB2(msp2); // top nibble MUST be same as top nibble of _MSC !
  pll_regs[6] = BB1(msp2);
  pll_regs[7] = BB0(msp2);
}

inline void SI5351::SendPLLRegisterBulk() {
  i2c.start();
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(26 + 0 * 8 + 3); // Write to PLLA
  // i2c.SendByte(26+1*8 + 3);  // Write to PLLB
  i2c.SendByte(pll_regs[3]);
  i2c.SendByte(pll_regs[4]);
  i2c.SendByte(pll_regs[5]);
  i2c.SendByte(pll_regs[6]);
  i2c.SendByte(pll_regs[7]);
  i2c.stop();
}
#endif               // !NEW_TX

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
  msa = div_nom / div_denom; // integer part: msa must be in range 15..90 for
                             // PLL, 8+1/1048575..900 for MS
  if (msa == 4)
    _int = 1; // To satisfy the MSx_INT=1 requirement of AN619, section 4.1.3
              // which basically says that for MS divider a value of 4 and
              // integer mode must be used
  msb = (_int) ? 0
               : (((uint64_t)(div_nom % div_denom) * _MSC) /
                  div_denom); // fractional part
  msc = (_int) ? 1 : _MSC;

  msp1 = 128 * msa + 128 * msb / msc - 512;
  msp2 = 128 * msb - 128 * msb / msc * msc;
  msp3 = msc;
  uint8_t ms_reg2 = BB2(msp1) | (rdiv << 4) | ((msa == 4) * 0x0C);
  uint8_t ms_regs[8] = {BB1(msp3), BB0(msp3),
                        ms_reg2,   BB1(msp1),
                        BB0(msp1), BB2(((msp3 & 0x0F0000) << 4) | msp2),
                        BB1(msp2), BB0(msp2)};

  SendRegister(n * 8 + 42, ms_regs, 8); // Write to MSx
  if (n < 0) {
    SendRegister(n + 16 + 8,
                 0x80 |
                     (0x40 * _int)); // MSNx PLLn: 0x40=FBA_INT; 0x80=CLKn_PDN
  } else {
    // SendRegister(n+16, ((pll)*0x20)|0x0C|0|(0x40*_int));  // MSx CLKn:
    // 0x0C=PLLA,0x2C=PLLB local msynth; 0=2mA; 0x40=MSx_INT; 0x80=CLKx_PDN
    SendRegister(
        n + 16,
        ((pll) * 0x20) | 0x0C | 3 |
            (0x40 * _int)); // MSx CLKn: 0x0C=PLLA,0x2C=PLLB local msynth;
                            // 3=8mA; 0x40=MSx_INT; 0x80=CLKx_PDN
    SendRegister(n + 165, (!_int) * phase * msa /
                              90); // when using: make sure to configure MS in
                                   // fractional-mode, perform reset afterwards
  }
}

void SI5351::phase(int8_t n, uint32_t div_nom, uint32_t div_denom,
                   uint16_t phase) {
  SendRegister(n + 165, phase * (div_nom / div_denom) / 90);
}

void SI5351::reset() {
  SendRegister(177, 0xA0);
} // 0x20 reset PLLA; 0x80 reset PLLB

void SI5351::oe(uint8_t mask) {
  SendRegister(3, ~mask);
} // output-enable mask: CLK2=4; CLK1=2; CLK0=1

void SI5351::freq(
    int32_t fout, uint16_t i,
    uint16_t q) {   // Set a CLK0,1,2 to fout Hz with phase i, q (on PLLA)
  uint8_t rdiv = 0; // CLK pin sees fout/(2^rdiv)
  if (fout > 300000000) {
    i /= 3;
    q /= 3;
    fout /= 3;
  } // for higher freqs, use 3rd harmonic
  if (fout < 500000) {
    rdiv = 7;
    fout *= 128;
  } // Divide by 128 for fout 4..500kHz
  uint16_t d;
  if (fout < 30000000)
    d = (16 * fxtal) / fout;
  else
    d = (32 * fxtal) / fout; // Integer part  .. maybe 44?
  if (fout < 3500000)
    d = (7 * fxtal) / fout; // PLL at 189MHz to cover 160m (freq>1.48MHz) when
                            // using 27MHz crystal
  if (fout > 140000000)
    d = 4; // for f=140..300MHz; AN619; 4.1.3, this implies integer mode
  if (d % 2)
    d++; // even numbers preferred for divider (AN619 p.4 and p.6)
  if ((d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal))
    d += 2; // Test if multiplier remains same for freq deviation +/- 5kHz, if
            // not use different divider to make same
  uint32_t fvcoa = d * fout; // Variable PLLA VCO frequency at integer multiple
                             // of fout at around 27MHz*16 = 432MHz

  ms(MSNA, fvcoa, fxtal); // PLLA in fractional mode
  // ms(MSNB, fvcoa, fxtal);
  ms(MS0, fvcoa, fout, PLLA, 0, i,
     rdiv); // Multisynth stage with integer divider but in frac mode due to
            // phase setting
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
  oe(0b00000011); // output enable CLK0, CLK1

  _fout = fout; // cache
  _div = d;
  _msa128min512 = fvcoa / fxtal * 128 - 512;
  _msb128 = ((uint64_t)(fvcoa % fxtal) * _MSC * 128) / fxtal;
}

void SI5351::freqb(uint32_t fout) { // Set a CLK2 to fout Hz (on PLLB)
  uint16_t d = (16 * fxtal) / fout;
  if (d % 2)
    d++; // even numbers preferred for divider (AN619 p.4 and p.6)
  uint32_t fvcoa = d * fout; // Variable PLLA VCO frequency at integer multiple
                             // of fout at around 27MHz*16 = 432MHz

  ms(MSNB, fvcoa, fxtal);
  ms(MS2, fvcoa, fout, PLLB, 0, 0, 0);
}

uint8_t SI5351::RecvRegister(uint8_t reg) {
  i2c.start(); // Data write to set the register address
  i2c.SendByte(SI5351_ADDR << 1);
  i2c.SendByte(reg);
  i2c.stop();
  i2c.start(); // Data read to retrieve the data from the set address
  i2c.SendByte((SI5351_ADDR << 1) | 1);
  uint8_t data = i2c.RecvByte(true);
  i2c.stop();
  return data;
}

void SI5351::powerDown() {
  SendRegister(3, 0b11111111);  // Disable all CLK outputs
  SendRegister(24, 0b00010000); // Disable state: CLK2 HIGH state, CLK0 & CLK1
                                // LOW state when disabled
  SendRegister(25, 0b00000000); // Disable state: LOW state when disabled
  for (int addr = 16; addr != 24; addr++)
    SendRegister(addr, 0b10000000); // Conserve power when output is disabled
  SendRegister(187, 0);             // Disable fanout (power-safe)
  // To initialise things as they should:
  SendRegister(149, 0);          // Disable spread spectrum enable
  SendRegister(183, 0b11010010); // Internal CL = 10 pF (default)
}

// ==========================================
// Filter Switching Implementation
// ==========================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
PCA9536 ioext;
inline void PCA9536::SendRegister(uint8_t reg, uint8_t val) {
  i2c.begin();
  i2c.beginTransmission(PCA9536_ADDR);
  i2c.write(reg);
  i2c.write(val);
  i2c.endTransmission();
}
inline void PCA9536::init() {
  SendRegister(0x03, 0x00);
} // configuration cmd: IO0-IO7 as output
inline void PCA9536::write(uint8_t data) {
  init();
  SendRegister(0x01, data);
} // output port cmd: write bits D7-D0 to IO7-IO0

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
void set_lpf(uint8_t f) {
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
IOExpander16 ioext;
inline void IOExpander16::SendRegister(uint8_t reg, uint8_t val) {
  i2c.begin();
  i2c.beginTransmission(IOEXP16_ADDR);
  i2c.write(reg);
  i2c.write(val);
  i2c.endTransmission();
}
inline void IOExpander16::init() {
  write(0U);
} // IO0, IO1 as input, IO0 to 0, IO0 as output, IO1 to 0, IO1 as output
inline void IOExpander16::write(uint16_t data) {
  SendRegister(0x07, 0xff);
  SendRegister(0x06, 0xff); /*Common last!*/
  SendRegister(0x02, data);
  SendRegister(0x06, 0x00); /*Common first!*/
  SendRegister(0x03, data >> 8);
  SendRegister(0x07, 0x00);
} // output port cmd: write bits D15-D0 to IO1.7-0.0;

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
void set_lpf(uint8_t f) {
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
MCP23008 ioext;
inline void MCP23008::SendRegister(uint8_t reg, uint8_t val) {
  i2c.begin();
  i2c.beginTransmission(MCP23008_ADDR);
  i2c.write(reg);
  i2c.write(val);
  i2c.endTransmission();
}
inline void MCP23008::init() {
  SendRegister(0x09, 0x00);
  SendRegister(0x00, 0x00);
} // GP0-7 to 0, GP0-7 as output
inline void MCP23008::write(uint16_t data) {
  SendRegister(0x09, data);
} // output port cmd: write bits D7-D0 to GP7-GP0

static uint8_t prev_lpf_io = 0xff; // inits and resets all latches
void set_lpf(uint8_t f) {
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
void set_lpf(uint8_t f) {
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
void set_lpf(uint8_t f) {} // dummy
#endif
