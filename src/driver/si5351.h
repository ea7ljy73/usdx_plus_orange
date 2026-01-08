#ifndef SI5351_H
#define SI5351_H

#include "../hardware/soft_i2c.h"
#include <Arduino.h>

// Si5351 Driver Module
// Controls the Si5351 Frequency Synthesizer

// External dependency on global i2c instance
extern I2C i2c;
class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div; // note: uint8_t asserts fout > 3.5MHz with R_DIV=1
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  // volatile uint32_t _mod;
  volatile uint8_t pll_regs[8];

#define BB0(x) ((uint8_t)(x)) // Bash byte x of int32_t
#define BB1(x) ((uint8_t)((x) >> 8))
#define BB2(x) ((uint8_t)((x) >> 16))

#define FAST __attribute__((optimize("Ofast")))

  volatile uint32_t fxtal = F_XTAL;

#define NEW_TX 1
#ifdef NEW_TX
  inline void FAST
  freq_calc_fast(int16_t df) // note: relies on cached variables: _msb128,
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
                                   // _MSC !  assuming that BB2(msp2) is always
                                   // 0 -> so reg is constant
    pll_regs[6] = BB1(msp2);
    pll_regs[7] = BB0(msp2);
  }

  inline void SendPLLRegisterBulk() {
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
  freq_calc_fast(int16_t df) // note: relies on cached variables: _msb128,
                             // _msa128min512, _div, _fout, fxtal
  {
#define _MSC 0x80000 // 0x80000: 98% CPU load   0xFFFFF: 114% CPU load
    uint32_t msb128 =
        _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;
    // uint32_t msb128 = ((int64_t)(_div * (int32_t)df + _mod) * _MSC * 128) /
    // fxtal; // @pre: 14<=_div<=144, |df|<=5000, _mod<=1800e3 (for fout<30M),
    // _MSC=524288

    // #define _MSC  (F_XTAL/128)   // MSC exact multiple of F_XTAL (and
    // maximized to fit in max. span 1048575) uint32_t msb128 = (_div *
    // (int32_t)df + _mod);

    // #define _MSC  0xFFFFF  // Old algorithm 114% CPU load, shortcut for a
    // fixed fxtal=27e6 register uint32_t xmsb = (_div * (_fout + (int32_t)df))
    // % fxtal;  // xmsb = msb * fxtal/(128 * _MSC); uint32_t msb128 = xmsb *
    // 5*(32/32) - (xmsb/32);  // msb128 = xmsb * 159/32, where 159/32 = 128 *
    // 0xFFFFF / fxtal; fxtal=27e6

    // #define _MSC  (F_XTAL/128)  // 114% CPU load  perfect alignment
    // uint32_t msb128 = (_div * (_fout + (int32_t)df)) % fxtal;

    uint32_t msp1 =
        _msa128min512 + msb128 / _MSC; // = 128 * _msa + msb128 / _MSC - 512;
    uint32_t msp2 = msb128 % _MSC;     // = msb128 - msb128/_MSC * _MSC;
    // uint32_t msp1 = _msa128min512;  // = 128 * _msa + msb128 / _MSC - 512;
    // assuming msb128 < _MSC, so that msp1 is constant uint32_t msp2 = msb128;
    // // = msb128 - msb128/_MSC * _MSC, assuming msb128 < _MSC

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

  inline void SendPLLRegisterBulk() {
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

  void SendRegister(uint8_t reg, uint8_t *data, uint8_t n) {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    while (n--)
      i2c.SendByte(*data++);
    i2c.stop();
  }
  void SendRegister(uint8_t reg, uint8_t val) { SendRegister(reg, &val, 1); }
  int16_t iqmsa; // to detect a need for a PLL reset
                 ///*
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
          uint8_t _int = 0, uint16_t phase = 0, uint8_t rdiv = 0) {
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
    // lcd.setCursor(0, 0); lcd.print(n); lcd.print(":"); lcd.print(msa);
    // lcd.print(" "); lcd.print(msb); lcd.print(" "); lcd.print(msc);
    // lcd.print(F("    ")); delay(500);
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
      SendRegister(n + 165,
                   (!_int) * phase * msa /
                       90); // when using: make sure to configure MS in
                            // fractional-mode, perform reset afterwards
    }
  }

  void phase(int8_t n, uint32_t div_nom, uint32_t div_denom, uint16_t phase) {
    SendRegister(n + 165, phase * (div_nom / div_denom) / 90);
  } // when using: make sure to configure MS in fractional-mode!, perform reset
    // afterwards

  void reset() { SendRegister(177, 0xA0); } // 0x20 reset PLLA; 0x80 reset PLLB

  void oe(uint8_t mask) {
    SendRegister(3, ~mask);
  } // output-enable mask: CLK2=4; CLK1=2; CLK0=1

  void freq(int32_t fout, uint16_t i,
            uint16_t q) { // Set a CLK0,1,2 to fout Hz with phase i, q (on PLLA)
    uint8_t rdiv = 0;     // CLK pin sees fout/(2^rdiv)
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
    uint32_t fvcoa = d * fout; // Variable PLLA VCO frequency at integer
                               // multiple of fout at around 27MHz*16 = 432MHz
    // si5351 spectral purity considerations:
    // https://groups.io/g/QRPLabs/message/42662

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

#ifdef x
    ms(MSNA, fvcoa, fxtal);
    ms(MSNB, fvcoa, fxtal);
#define F_DEV 4
    ms(MS0, fvcoa, (fout + F_DEV), PLLA, 0, 0, rdiv);
    ms(MS1, fvcoa, (fout + F_DEV), PLLA, 0, 0, rdiv);
    ms(MS2, fvcoa, fout, PLLA, 0, 0, rdiv);
    reset();
    ms(MS0, fvcoa, fout, PLLA, 0, 0, rdiv);
    delayMicroseconds(
        F_MCU / 16000000 * 1000000UL /
        F_DEV); // Td = 1/(4 * Fdev) phase-shift
                // https://tj-lab.org/2020/08/27/si5351%e5%8d%98%e4%bd%93%e3%81%a73mhz%e4%bb%a5%e4%b8%8b%e3%81%ae%e7%9b%b4%e4%ba%a4%e4%bf%a1%e5%8f%b7%e3%82%92%e5%87%ba%e5%8a%9b%e3%81%99%e3%82%8b/
    ms(MS1, fvcoa, fout, PLLA, 0, 0, rdiv);
    oe(0b00000011); // output enable CLK0, CLK1
#endif
    _fout = fout; // cache
    _div = d;
    _msa128min512 = fvcoa / fxtal * 128 - 512;
    _msb128 = ((uint64_t)(fvcoa % fxtal) * _MSC * 128) / fxtal;
    //_mod = fvcoa % fxtal;
  }

  void freqb(uint32_t fout) { // Set a CLK2 to fout Hz (on PLLB)
    uint16_t d = (16 * fxtal) / fout;
    if (d % 2)
      d++; // even numbers preferred for divider (AN619 p.4 and p.6)
    uint32_t fvcoa = d * fout; // Variable PLLA VCO frequency at integer
                               // multiple of fout at around 27MHz*16 = 432MHz

    ms(MSNB, fvcoa, fxtal);
    ms(MS2, fvcoa, fout, PLLB, 0, 0, 0);
  }

  //*/
  /*
    void freq(uint32_t fout, uint16_t i, uint16_t q){  // Set a CLK0,1 to fout
    Hz with phase i, q uint16_t msa; uint32_t msb, msc, msp1, msp2, msp3;
        uint8_t rdiv = 0;             // CLK pin sees fout/(2^rdiv)
        if(fout > 300000000){ i/=3; q/=3; fout/=3; }  // for higher freqs, use
    3rd harmonic if(fout < 500000){ rdiv = 7; fout *= 128; } // Divide by 128
    for fout 4..500kHz

        uint16_t d = (16 * fxtal) / fout;  // Integer part
        //if(fout > 7000000) d = (33 * fxtal) / fout;
        if(fout < 3500000) d = (7 * fxtal) / fout;  // PLL at 189MHz to cover
    160m (freq>1.48MHz) when using 27MHz crystal

        if( (d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal) ) d++; //
    Test if multiplier remains same for freq deviation +/- 5kHz, if not use
    different divider to make same if(d % 2) d++;  // even numbers preferred for
    divider (AN619 p.4 and p.6) bool divby4 = 0; if(fout > 140000000){ d = 4;
    divby4 = 1; } // for f=140..300MHz; AN619; 4.1.3 uint32_t fvcoa = d * fout;
    // Variable PLLA VCO frequency at integer multiple of fout at around
    27MHz*16 = 432MHz msa = fvcoa / fxtal;     // Integer part of vco/fxtal. msa
    must be in range 15..90 msb = ((uint64_t)(fvcoa % fxtal)*_MSC) / fxtal; //
    fractional part msc = _MSC;

        msp1 = 128*msa + 128*msb/msc - 512;
        msp2 = 128*msb - 128*msb/msc * msc;
        msp3 = msc;
        uint8_t pll_regs[8] = { BB1(msp3), BB0(msp3), BB2(msp1), BB1(msp1),
    BB0(msp1), BB2(((msp3 & 0x0F0000)<<4) | msp2), BB1(msp2), BB0(msp2) };
        SendRegister(26+0*8, pll_regs, 8); // Write to PLLA
        SendRegister(26+1*8, pll_regs, 8); // Write to PLLB
        SendRegister(16+6, 0x80); // PLLA in fractional mode; 0x40=FBA_INT;
    0x80=CLK6_PDN SendRegister(16+7, 0x80); // PLLB in fractional mode;
    0x40=FBB_INT; 0x80=CLK7_PDN

        msa = fvcoa / fout;     // Integer part of vco/fout. msa must be in
    range 6..127 (support for integer and initial phase offset)
        //lcd.setCursor(0, 0); lcd.print(fvcoa/fxtal); lcd.print(" ");
    lcd.print(msb); lcd.print(" "); lcd.print(msa); lcd.print(F("     ")); msp1
    = (divby4) ? 0 : (128*msa - 512);     // msp1 and msp2=0, msp3=1, integer
    division msp2 = 0; msp3 = 1; uint8_t ms_regs[8] = { BB1(msp3), BB0(msp3),
    BB2(msp1) | (rdiv<<4) | (divby4*0x0C), BB1(msp1), BB0(msp1), BB2(((msp3 &
    0x0F0000)<<4) | msp2), BB1(msp2), BB0(msp2) }; SendRegister(42+0*8, ms_regs,
    8); // Write to MS0 SendRegister(42+1*8, ms_regs, 8); // Write to MS1
        SendRegister(42+2*8, ms_regs, 8); // Write to MS2
        SendRegister(16+0, 0x0C|3|(0x40*divby4));  // CLK0: 0x0C=PLLA local
    msynth; 3=8mA; 0x40=MS0_INT; 0x80=CLK0_PDN SendRegister(16+1,
    0x0C|3|(0x40*divby4));  // CLK1: 0x0C=PLLA local msynth; 3=8mA;
    0x40=MS1_INT; 0x80=CLK1_PDN SendRegister(16+2, 0x2C|3|(0x40*divby4));  //
    CLK2: 0x2C=PLLB local msynth; 3=8mA; 0x40=MS2_INT; 0x80=CLK2_PDN
        SendRegister(165, i * msa / 90);  // CLK0: I-phase (on change -> Reset
    PLL) SendRegister(166, q * msa / 90);  // CLK1: Q-phase (on change -> Reset
    PLL) if(iqmsa != ((i-q)*msa/90)){ iqmsa = (i-q)*msa/90; SendRegister(177,
    0xA0); } // 0x20 reset PLLA; 0x80 reset PLLB SendRegister(3, 0b11111100); //
    Enable/disable clock

        _fout = fout;  // cache
        _div = d;
        _msa128min512 = fvcoa / fxtal * 128 - 512;
        _msb128=((uint64_t)(fvcoa % fxtal)*_MSC*128) / fxtal;
    }
  */
  uint8_t RecvRegister(uint8_t reg) {
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
  void powerDown() {
    SendRegister(3, 0b11111111); // Disable all CLK outputs
    SendRegister(
        24,
        0b00010000); // Disable state: CLK2 HIGH state, CLK0 & CLK1 LOW state
                     // when disabled; CLK2 needs to be in HIGH state to make
                     // sure that cap to gate is already charged, preventing
                     // "exponential pulse is caused by CLK2, which had been at
                     // 0v whilst it was disabled, suddenly generating a 5vpp
                     // waveform, which is “added to” the 0v filtered PWM output
                     // and causing the output fets to be driven with the full
                     // 5v pp.", see:
                     // https://forum.dl2man.de/viewtopic.php?t=146&p=1307#p1307
    SendRegister(25, 0b00000000); // Disable state: LOW state when disabled
    for (int addr = 16; addr != 24; addr++)
      SendRegister(addr, 0b10000000); // Conserve power when output is disabled
    SendRegister(187, 0);             // Disable fanout (power-safe)
    // To initialise things as they should:
    SendRegister(149, 0);          // Disable spread spectrum enable
    SendRegister(183, 0b11010010); // Internal CL = 10 pF (default)
  }
#define SI_CLK_OE 3
};
static SI5351 si5351;

#endif // SI5351_H
