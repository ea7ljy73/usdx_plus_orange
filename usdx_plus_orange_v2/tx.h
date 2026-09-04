// tx.h - uSDX Plus Orange v2
// Transmit DSP: polar SSB modulator, CW/AM/FM TX ISRs, mic processing.
// Extracted from v1 Section 07/08, behavior identical (incl. bugfixes already
// applied: legacy LPF 1/9 + smooth clipper + CESSB + comp/EQ/lowcut/pre).

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#include "si5351.h"
#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Constants / macros
// ---------------------------------------------------------------------------
#define F_SAMP_TX 4800 // TX sample rate (design; fits OCR2A timing @20MHz)

#if(F_MCU != 20000000)
const int16_t _F_SAMP_TX = (F_MCU * 4800LL / 20000000);
#else
#  define _F_SAMP_TX F_SAMP_TX
#endif

#define _UA 600 // unit angle (2pi in 0..600 integer)

#define MAX_DP ((filt == 0) ? _UA : (filt == 3) ? _UA / 4 : _UA / 2)

#define CARRIER_COMPLETELY_OFF_ON_LOW 1 // disable oscillator on low amplitudes
#define MULTI_ADC 1                     // multiple ADC conversions (+12dB mic gain)
#define MORE_MIC_GAIN 1                 // extra mic gain for SSB quality

#define CESSB_THRESH 200 // CESSB envelope clipper threshold
#define AM_BASE 32       // AM carrier bias
#define AF_BIAS 32       // mic positive bias offset

#define MIC_ATTEN 0 // 0*6dB attenuation

// ---------------------------------------------------------------------------
// External references (defined in other modules)
// ---------------------------------------------------------------------------
extern volatile uint8_t mode; // 0=LSB, 1=USB, ... (owned by vfo/mode module)
extern volatile int8_t  volume;

#define USB 1 // mode constant (matches v1)
#define LSB 0

// ---------------------------------------------------------------------------
// TX globals
// ---------------------------------------------------------------------------
uint8_t          lut[256];
volatile uint8_t amp;
volatile uint8_t vox_thresh = 4; // legacy noise-gate default
volatile uint8_t drive      = 4; // legacy factory default (setup forces 4)

volatile uint8_t tx   = 0; // TX latch: 0=off, reaches 255 when triggered
volatile uint8_t filt = 0; // filter select (shared with RX; owned here for MAX_DP)
volatile uint8_t vox  = 0; // vox master enable (default OFF, like legacy usdx-legazy:228)
volatile uint8_t quad = 0; // QUAD frequency divider state (legacy parity)

volatile int8_t p_sin = 0;   // Minsky sin state (int8_t, legacy parity)
volatile int8_t n_cos = 448 / 4; // Minsky cos state (legacy parity)

volatile uint8_t tone_vol = 12;
volatile uint8_t cw_tone  = 1;
const uint32_t   tones[]  = {F_MCU * 700ULL / 20000000, F_MCU * 600ULL / 20000000, F_MCU * 700ULL / 20000000};

volatile uint32_t cw_offset = 0; // CW TX/RX offset from dial (legacy 2174, set in setup)

volatile bool dig_mode = false;
volatile int8_t mox = 0; // legacy parity (never set without MOX_ENABLE)

// Voice processing (menu-configurable; all off by default)
volatile uint8_t  comp_enable    = 0;
volatile uint16_t comp_threshold = 128;
volatile int16_t  comp_envelope  = 0;
volatile int8_t   eq_low         = 0;
volatile int8_t   eq_high        = 0;
static int16_t    eq_low_iir     = 0;
static int16_t    eq_high_iir    = 0;
volatile uint8_t  tx_lowcut      = 0;
volatile uint8_t  pre_emph       = 0;
static int16_t    pre_z1         = 0;

// ---------------------------------------------------------------------------
// _vox - VOX/TX latching (tx counts up/down; 255 when freshly triggered)
// ---------------------------------------------------------------------------
inline void _vox(bool trigger) {
  if(trigger) {
    tx = (tx) ? 254 : 255;
  } else {
    if(tx)
      tx--;
  }
}
#define _vox_p(a) _vox((a) > vox_thresh)

// ---------------------------------------------------------------------------
// arctan3 (0.8 deg accuracy), magn approximation
// ---------------------------------------------------------------------------
inline int16_t arctan3(int16_t q, int16_t i) {
#define _atan2(z) (_UA / 8 + _UA / 22 - _UA / 22 * z) * z
  int16_t r;
  if(abs(q) > abs(i))
    r = _UA / 4 - _atan2(abs(i) / abs(q));
  else
    r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
}

#define magn(i, q) (abs(i) > abs(q) ? abs(i) + (abs(q) >> 2) : abs(q) + (abs(i) >> 2))

// ---------------------------------------------------------------------------
// ssb() - polar SSB modulator
// EXACT COPY of usdx-legazy:2031 (strict parity; improvements are deferred
// and will be reintroduced later as optional, verified by parity tests).
// ---------------------------------------------------------------------------
inline int16_t ssb(int16_t in) {
  static int16_t dc, z1;
  int16_t        i, q;
  uint8_t        j;
  static int16_t v[16];
  for(j = 0; j != 15; j++)
    v[j] = v[j + 1];
#ifdef MORE_MIC_GAIN
  int16_t ac = in * 2;                    // 6dB gain
  ac         = ac + z1;                   // lpf
  z1         = (in - (8) * z1) / (8 + 1); // lpf

  // smooth clipping limiter
  if(ac > 250) {
    ac = 250 + (ac - 250) / 2;
  } else if(ac < -250) {
    ac = -250 - (-250 - ac) / 2;
  }

  dc    = (ac + (2) * dc) / (2 + 1);
  v[15] = (ac - dc);
#else
  dc         = (in + dc) / 2; // average
  int16_t ac = (in - dc);     // DC decoupling
  v[15]      = (ac + z1);
  z1         = ac;
#endif
  i = v[7] * 2;
  q = ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 16) / 64 +
      (v[6] - v[8]); // Hilbert

  uint16_t _amp = magn(i / 2, q / 2);

  _vox(_amp > vox_thresh);

  _amp = _amp << (drive);
  _amp = ((_amp > 255) || (drive == 8)) ? 255 : _amp;
  amp  = (tx) ? lut[_amp] : 0;

  static int16_t prev_phase;
  int16_t        phase = arctan3(q, i);

  int16_t dp = phase - prev_phase;
  prev_phase = phase;

  if(dp < 0)
    dp = dp + _UA;
#ifdef QUAD
  if(dp >= (_UA / 2)) {
    dp   = dp - _UA / 2;
    quad = !quad;
  }
#endif
#ifdef MAX_DP
  if(dp > MAX_DP) {
    prev_phase = phase - (dp - MAX_DP);
    dp         = MAX_DP;
  }
#endif
  if(mode == USB)
    return dp * (_F_SAMP_TX / _UA);
  else
    return dp * (-_F_SAMP_TX / _UA);
}

// ---------------------------------------------------------------------------
// dsp_tx() - SSB TX ISR body
// ---------------------------------------------------------------------------
static int16_t _adc;
inline void    dsp_tx() { // jitter dependent things first
#ifdef MULTI_ADC          // SSB with multiple ADC conversions:
  int16_t adc;            // current ADC sample 10-bits, note: first ADCL then ADCH
  adc = ADC;
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk(); // submit frequency regs (88us I2C)
  OCR1BL = amp;                 // submit amplitude to PWM
  adc += ADC;
  ADCSRA |= (1 << ADSC); // causes RFI on QCX-SSB (use direct biasing!)
  int16_t df = ssb(_adc >> MIC_ATTEN);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  si5351.freq_calc_fast(df);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  _adc = (adc / 4 - (512 - AF_BIAS)); // keep positive bias offset
#else                                 // SSB with single ADC conversion:
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
  OCR1BL      = amp;
  int16_t adc = ADC - 512;
  int16_t df  = ssb(adc >> MIC_ATTEN);
  si5351.freq_calc_fast(df);
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  if(tx == 1) {
    OCR1BL = 0;
    si5351.SendRegister(SI_CLK_OE, TX0RX0);
  }
  if(tx == 255) {
    si5351.SendRegister(SI_CLK_OE, TX1RX0);
  }
#endif

#ifdef MOX_ENABLE
  if(!mox)
    return;
  OCR1AL = (adc << (mox - 1)) + 128; // TX audio monitoring
#endif
}

// ---------------------------------------------------------------------------
// CW / AM / FM TX
// ---------------------------------------------------------------------------
inline void process_minsky() { // legacy 2175-2182 (600 Hz sidetone)
  int8_t alpha127 = tones[cw_tone] * 798 / _F_SAMP_TX;
  p_sin += alpha127 * n_cos / 127;
  n_cos -= alpha127 * p_sin / 127;
}

const uint8_t ramp[] PROGMEM = {255, 254, 252, 249, 245, 239, 233, 226, 217, 208, 198, 187, 176, 164, 152, 139,
                                127, 115, 102, 90,  78,  67,  56,  46,  37,  28,  21,  15,  9,   5,   2};

void dsp_tx_cw() {
#ifdef KEY_CLICK
  if(OCR1BL < lut[255]) {
    for(uint16_t i = 31; i != 0; i--) { // soft rising slope against key-clicks
      OCR1BL = lut[pgm_read_byte_near(&ramp[i - 1])];
      delayMicroseconds(60);
    }
  }
#endif
  OCR1BL = lut[255];
  process_minsky();
  OCR1AL = (p_sin >> (16 - volume)) + 128; // legacy 2204
}

void dsp_tx_am() {
  ADCSRA |= (1 << ADSC);
  OCR1BL      = amp;
  int16_t adc = ADC - 512;
  int16_t in  = (adc >> MIC_ATTEN);
  in          = in << (drive - 4);
  in          = max(0, min(255, (in + AM_BASE)));
  amp         = in;
}

void dsp_tx_fm() {
  ADCSRA |= (1 << ADSC);
  OCR1BL = lut[255];
  si5351.SendPLLRegisterBulk();
  int16_t adc = ADC - 512;
  int16_t in  = (adc >> MIC_ATTEN);
  in          = in << (drive);
  int16_t df  = in;
  si5351.freq_calc_fast(df);
}
