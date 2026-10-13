// tx.h - uSDX Plus Orange (producción)
// Transmit DSP: polar SSB modulator, CW/AM/FM TX ISRs, mic processing.
// Strict parity with usdx-legazy.ino (verified working hardware).
// High-end improvements (CESSB, voice comp, mic EQ, low-cut, pre-emphasis)
// are DEFERRED: reintroduce one by one, verified by parity tests.

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

#define AM_BASE 32       // AM carrier bias
#define AF_BIAS 32       // mic positive bias offset

volatile uint8_t mic_atten = 0; // F3.6: mic attenuation 0..4 in 6dB steps (menu)

volatile bool     comp_enable = false; // F3.9b: voice compressor 2:1 (menu)
#define COMP_TH 128                    // threshold on |ac| (post 6dB gain scale)
static int16_t comp_env = 0;           // envelope follower state

volatile uint8_t tx_lowcut = 0; // F3.9c: HPF micro 0=off,1=100Hz,2=200Hz,3=400Hz
volatile uint8_t tx_envpre = 0; // F3.10: preenfasis de envolvente 0=off(legacy), 1..3 = c1 0,5/1/1,5
static int16_t  lc_lp     = 0;  // HPF state

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

volatile uint8_t tone_vol = 12; // (reserved)
volatile uint8_t cw_tone  = 1;
const uint32_t   tones[]  = {F_MCU * 700ULL / 20000000, F_MCU * 600ULL / 20000000, F_MCU * 700ULL / 20000000};

volatile uint32_t cw_offset = 0; // CW TX/RX offset from dial (legacy 2174, set in setup)

volatile bool dig_mode = false; // F3.7: flat TX path for digital modes (menu)
volatile int8_t mox = 0; // legacy parity (never set without MOX_ENABLE)

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
// ssb() - polar SSB modulator. Base EXACTA de usdx-legazy:2031 (paridad TX
// verificada con todo off) + mejoras conmutadas Fase 3, cada una medida en
// A/B: DIGI flat path (F3.7), compresor voz (F3.9b), low-cut HPF (F3.9c),
// ALC (F3.8). Con todo off, bit-exacto con legacy.
// ---------------------------------------------------------------------------
inline int16_t ssb(int16_t in) {
// x/3 y x/9 con signo (truncadas, como el operador / de C) por multiplicacion:
// identicas a la division para todo int16 (verificado exhaustivamente en
// tests/fw_sim/txamp_sim) y ~170 ciclos mas rapidas cada una en el AVR.
#define TX_DIV3(x) (((x) < 0) ? -(int16_t)(((uint32_t)(uint16_t)(-(x)) * 43691U) >> 17) : (int16_t)(((uint32_t)(uint16_t)(x) * 43691U) >> 17))
#define TX_DIV9(x) (((x) < 0) ? -(int16_t)(((uint32_t)(uint16_t)(-(x)) * 58255U) >> 19) : (int16_t)(((uint32_t)(uint16_t)(x) * 58255U) >> 19))
  static int16_t dc, z1;
  int16_t        i, q;
  uint8_t        j;
  static int16_t v[16];
  for(j = 0; j != 15; j++)
    v[j] = v[j + 1];
  if(tx_lowcut && !dig_mode) { // F3.9c HPF: quita retumbe sub-audio (ahorra IMD)
    int16_t hp = in - lc_lp;   // fc ~= 4800/(2*pi*2^(4-v)): 100/200/400Hz
    lc_lp += hp >> (4 - tx_lowcut);
    in = hp;
  }
#ifdef MORE_MIC_GAIN
  int16_t ac;
  if(dig_mode) { // F3.7 flat path for digital modes (legacy DIG_MODE formula)
    ac    = in;
    dc    = (ac + (7) * dc) / (7 + 1); // slow DC average (HPF ~100Hz)
    v[15] = (ac - dc) / 2;             // -6dB (no LPF emphasis, no clipper)
  } else {
  ac = in * 2;                    // 6dB gain
  ac         = ac + z1;                   // lpf
  {
    int16_t t = in - (8) * z1;
    z1        = TX_DIV9(t); // lpf: (in - 8*z1) / 9 (legacy 2045)
  }

  // F3.9b voice compressor 2:1 above COMP_TH (soft knee via instant attack +
  // slow release envelope). Ganancia en Q6 con UNA division de 16 bit: la de
  // 32 bit original (~550 ciclos) desbordaba la ISR de TX (medido en
  // tests/avr_sim: 4300 ciclos de media para un periodo de 4160).
  if(comp_enable && !dig_mode) {
    int16_t aa = (ac >= 0) ? ac : -ac;
    if(aa > comp_env)
      comp_env = aa; // attack: instant
    else
      comp_env -= (comp_env - aa) >> 8; // release: ~53ms
    if(comp_env > COMP_TH) {
      uint8_t g = (uint16_t)((uint16_t)(COMP_TH + ((comp_env - COMP_TH) >> 1)) << 6) / (uint16_t)comp_env; // 32..64
      ac        = (int16_t)(((int32_t)ac * g) >> 6);
    }
  }

  // smooth clipping limiter
  if(ac > 250) {
    ac = 250 + (ac - 250) / 2;
  } else if(ac < -250) {
    ac = -250 - (-250 - ac) / 2;
  }

  {
    int16_t t = ac + (2) * dc;
    dc        = TX_DIV3(t); // (ac + 2*dc) / 3 (legacy 2054)
  }
  v[15] = (ac - dc);
  }
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

  // F3.8 ALC de pico (sustituye al de pasos de 6 dB, que recortaba ANTES de
  // atenuar: misma distorsion con 18 dB menos de potencia). Ganancia fina
  // alc_g/65536 aplicada ANTES del recorte: si un pico supera 255 baja un
  // 6 % (0,56 dB) por muestra: ataque de pocos ms; si no, sube un 0,8 % cada
  // 64 muestras (~5 dB/s). El pico de envolvente queda en 255 (PEP completa)
  // sin recorte sostenido. Con alc_g=0xFFFF (reposo) es bit-exacto con legacy
  // (2086-2088). Excluye drive=8 (amplitud constante) y dig_mode (tonos).
  static uint16_t alc_g   = 0xFFFF;
  static uint8_t  alc_rel = 0;
  uint16_t        a       = _amp << (drive);
  if(drive == 8) {
    a = 255; // legacy 2087: drive=8 = salida maxima constante
  } else if(dig_mode) {
    if(a > 255)
      a = 255;
  } else {
    if(alc_g != 0xFFFF)
      a = ((uint32_t)a * alc_g) >> 16;
    if(a > 255) {
      a = 255;
      alc_g -= (alc_g >> 4) + 1; // nunca llega a 0: minimo ~ -70 dB
    } else if(alc_g != 0xFFFF && !(++alc_rel & 63)) {
      uint16_t g = alc_g + (alc_g >> 7) + 1;
      alc_g      = (g < alc_g) ? 0xFFFF : g; // satura en ganancia unidad
    }
  }
  amp = (tx) ? lut[a] : 0;

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
// F3.10 Preenfasis de envolvente. KEY_OUT llega a las puertas del PA por dos
// celulas RC (10k/10n en el esquema: ~300 us de retardo de grupo, casi muestra
// y media) que retrasan y suavizan la amplitud respecto a la fase. Se compensa
// con a' = a + c1*(a - a_ant), c1 = tx_envpre/2, recortado al rango del PWM.
// Simulado (tests/avr_sim --study, dos tonos, RC del esquema): IMD3 -19,8 ->
// -31,9 dBc con c1 = 1. 0 = apagado: salida identica a legacy.
// ---------------------------------------------------------------------------
static uint8_t env_prev;
inline uint8_t tx_env_out() {
  uint8_t a = amp;
  if(tx_envpre) {
    int16_t v = a + ((((int16_t)a - env_prev) * tx_envpre) >> 1);
    env_prev  = a;
    a         = (v < 0) ? 0 : (v > lut[255]) ? lut[255] : v;
  }
  return a;
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
  OCR1BL = tx_env_out();        // submit amplitude to PWM (F3.10: preenfasis opcional)
  adc += ADC;
  ADCSRA |= (1 << ADSC); // causes RFI on QCX-SSB (use direct biasing!)
  int16_t df = ssb(_adc >> mic_atten);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  si5351.freq_calc_fast(df);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  _adc = (adc / 4 - (512 - AF_BIAS)); // keep positive bias offset
#else                                 // SSB with single ADC conversion:
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
  OCR1BL      = tx_env_out();
  int16_t adc = ADC - 512;
  int16_t df  = ssb(adc >> mic_atten);
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
  int16_t in  = (adc >> mic_atten);
  in          = in << (drive - 4);
  in          = max(0, min(255, (in + AM_BASE)));
  amp         = in;
}

void dsp_tx_fm() {
  ADCSRA |= (1 << ADSC);
  OCR1BL = lut[255];
  si5351.SendPLLRegisterBulk();
  int16_t adc = ADC - 512;
  int16_t in  = (adc >> mic_atten);
  in          = in << (drive);
  int16_t df  = in;
  si5351.freq_calc_fast(df);
}
