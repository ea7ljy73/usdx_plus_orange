// rx.h - uSDX Plus Orange v2
// Receive DSP: exact parity with usdx-legazy.ino (the firmware verified working
// on this hardware). Architecture: 1-arg slow_dsp(int16_t ac) with Hilbert in
// process(), global i/q/qh/ocomb. AGC keeps the hang-time + noise-floor fix.

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "tx.h" // magn, mode (mode_t), filt, volume
#include "usdx_filter.h"
#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// RX sample rate / decimation
// ---------------------------------------------------------------------------
#define F_SAMP_PWM (78125 / 1)
#define F_SAMP_RX 62500
#define F_ADC_CONV                                                                                                     \
  (192307 / 2) // RX I/Q ADC rate (v1: slower than 192307 ->
               // avoids audio clicks)
#define R 4    // CIC decimation 62500/2 -> 7812.5 SPS

// Audio output stage ON (usdx-legazy:2826). WITHOUT this the OCR1AL audio
// write is compiled out entirely -> no RX audio.
#define AF_OUT 1
#define OUTLET 1

#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)

#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha);

// ---------------------------------------------------------------------------
// Globals (shared with UI / CAT / legacy parity)
// ---------------------------------------------------------------------------
volatile uint8_t nr        = 2; // noise reduction level
volatile uint8_t nb_enable = 0; // noise blanker on/off
volatile uint8_t att       = 0; // analog attenuator
volatile uint8_t att2      = 2; // digital attenuator (CIC stage)
volatile uint8_t rf_atten  = 0;

extern volatile uint8_t agc; // agc select (1/2; from main)

// legacy-parity globals for the demod path:
volatile uint8_t  rx_state   = 0;
volatile uint8_t  _init      = 1; // first-sample accumulators reset
static uint32_t   absavg256  = 0;
volatile uint32_t _absavg256 = 0;
volatile int16_t  i, q;  // demodulated I/Q (global, used by slow_dsp)
volatile int16_t  ocomb; // audio out comb (shared)
volatile int16_t  qh;    // Hilbert Q (global)

// ---------------------------------------------------------------------------
// AGC (M0PUB + hang-time + adaptive noise floor)
// ---------------------------------------------------------------------------
static int16_t   centiGain  = 128;
volatile uint8_t agc_decay  = 8;
static uint16_t  decayCount = 800;

inline int16_t process_agc(int16_t in) {
  static bool     small    = true;
  static uint16_t nfloor   = 64; // adaptive noise floor (scaled = centiGain/128)
  static uint16_t hang_cnt = 0;  // samples since signal last present
  int16_t         out;

  if(centiGain >= 128)
    out = (int16_t)(((int32_t)(centiGain >> 5) * in) >> 2);
  else
    out = (int16_t)(((int32_t)(centiGain >> 2) * (in >> 3)) >> 2);

  uint16_t abs_out = abs(out);

  // adaptive noise floor: tracks quiet periods (resolution /128)
  if(abs_out < nfloor)
    nfloor -= (nfloor - abs_out) >> 4; // slow leak down
  else
    nfloor += (abs_out - nfloor) >> 11; // very slow rise
  if(nfloor < 16)
    nfloor = 16;

  if(HI(abs_out) > HI(1536)) {
    centiGain -= (centiGain >> 4); // fast attack
    hang_cnt = 0;
  } else {
    if(HI(abs_out) > HI(256))
      hang_cnt = 0; // signal present: reset hang
    else if(hang_cnt < 2048)
      hang_cnt++;
    if(HI(abs_out) > HI(1024))
      small = false;
    if(--decayCount == 0) {
      decayCount = ((mode == CW_MODE) ? 2 : (uint16_t)agc_decay) * 100;
      if(small && (hang_cnt >= 600) && ((HI(abs_out) << 8) > ((uint32_t)nfloor * 3))) {
        if(centiGain < (INT16_MAX - (INT16_MAX >> 4)))
          centiGain += (centiGain >> 4);
        else
          centiGain = INT16_MAX;
      }
      small = true;
      if(hang_cnt >= 2048)
        nfloor = (nfloor * 3) >> 2; // re-learn floor after long silence
    }
  }
  return out;
}

// Fast AGC alternative (agc=1). int16 overflow fixed with int32 (v1 bug).
static int16_t gain = 1024;
inline int16_t process_agc_fast(int16_t in) {
  int16_t out     = (gain >= 1024) ? (int16_t)(((int32_t)(gain >> 10) * in)) : in;
  int16_t abs_out = abs(out);
  int16_t hi      = abs_out >> 10;
  if(hi > 1) { // strong signal: reduce gain
    gain -= (abs_out >> 3);
    if(gain < 1024)
      gain = 1024;
  } else { // weak signal: increase gain
    int16_t accum = 1 - hi;
    if(accum > 0 && (INT16_MAX - gain) > accum)
      gain += accum;
  }
  if(gain < 1)
    gain = 1;
  if(gain > 32767)
    gain = 32767;
  return out;
}

// ---------------------------------------------------------------------------
// Noise reduction
// ---------------------------------------------------------------------------
inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr - 1));
  return ea1;
}

// ---------------------------------------------------------------------------
// slow_dsp(int16_t ac) - demod + AGC + NR + filter (usdx-legazy parity)
// Requires global i, q set by process() for AM/FM (Hilbert done in process()).
// ---------------------------------------------------------------------------
inline int16_t slow_dsp(int16_t ac) {
  static uint8_t absavg256cnt;
  if(!(absavg256cnt--)) {
    _absavg256 = absavg256;
    absavg256  = 0;
  } else
    absavg256 += abs(ac);

  if(mode == AM) {
    ac                    = magn(i, q);
    static int32_t dc_avg = 0;
    dc_avg                = (dc_avg * 63 + ac) / 64;
    ac                    = ac - dc_avg;
  } else if(mode == FM) {
    static int16_t prev_i       = 0;
    static int16_t prev_q       = 0;
    int32_t        product      = (int32_t)i * prev_q - (int32_t)q * prev_i;
    int32_t        magnitude_sq = (int32_t)i * i + (int32_t)q * q;
    if(magnitude_sq > 1000) {
      ac = (product << 4) / (magnitude_sq >> 3);
    } else {
      ac = 0;
    }
    prev_i                = i;
    prev_q                = q;
    static int16_t fm_lpf = 0;
    fm_lpf                = (fm_lpf * 3 + ac) / 4; // alpha = 1/4 (~3-4kHz)
    ac                    = fm_lpf;
  } else {
    ; // USB, LSB, CW
  }

  if(agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
  } else if(agc == 2) {
    ac = process_agc(ac);
    ac = ac >> (16 - volume);
  } else {
    if(volume <= 13)
      ac = ac >> (13 - volume);
    else
      ac = ac << (volume - 13);
  }

  if(nr)
    ac = process_nr(ac);

  if(filt)
    ac = filt_var(ac);

  ac = min(max(ac, -512), 511);
  return ac;
}

// ---------------------------------------------------------------------------
// process() - CIC output stage + Hilbert + slow_dsp (usdx-legazy parity)
// ---------------------------------------------------------------------------
static uint8_t tc = 0;

void process(int16_t i_ac2, int16_t q_ac2) {
  static int16_t ac3;
#ifdef AF_OUT
  static int16_t ozd1, ozd2; // Output stage
  if(_init) {
    ac3   = 0;
    ozd1  = 0;
    ozd2  = 0;
    _init = 0;
  } // first-sample reset
  int16_t od1 = ac3 - ozd1; // Comb section
  ocomb       = od1 - ozd2;
#endif
#ifdef OUTLET
  if(tc++ == 0) // prevent recursion
#endif
    interrupts(); // allow subsequent interrupts for further rx sampling while processing
#ifdef AF_OUT
  ozd2 = od1;
  ozd1 = ac3;
#endif
  {
    q_ac2 >>= att2;       // digital gain control
    static int16_t v[14]; // Process Q (down-sampled) samples
    qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 + ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
         ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + (v[6] - v[8]) / 2; // Hilbert
    v[0]  = v[1];
    v[1]  = v[2];
    v[2]  = v[3];
    v[3]  = v[4];
    v[4]  = v[5];
    v[5]  = v[6];
    v[6]  = v[7];
    v[7]  = v[8];
    v[8]  = v[9];
    v[9]  = v[10];
    v[10] = v[11];
    v[11] = v[12];
    v[12] = v[13];
    v[13] = q_ac2;
  }
  i_ac2 >>= att2; // digital gain control
  i = i_ac2;
  q = q_ac2;
  static int16_t v[7]; // Delay I to match Hilbert on Q
  v[0] = v[1];
  v[1] = v[2];
  v[2] = v[3];
  v[3] = v[4];
  v[4] = v[5];
  v[5] = v[6];
  v[6] = i_ac2;
  ac3  = slow_dsp(-q - qh); // inverting I and Q dampens PWM-out/ADC feedback loop
#ifdef OUTLET
  tc--;
#endif
}

// ---------------------------------------------------------------------------
// CIC decimator (sdr_rx_00..07) - direct I/Q ADC sampling (usdx-legazy parity)
// ---------------------------------------------------------------------------
volatile uint8_t admux[3]; // ADC channel selectors (I/Q/mic); set in setup

static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1, q_ac2;

#define M_SR 1 // CIC N=3

// func_ptr owns the next phase; definition in hw.h
typedef void (*func_t)(void);
extern volatile func_t func_ptr;

// forward declarations (defined below)
inline int16_t sdr_rx_common_q();
inline int16_t sdr_rx_common_i();
void           sdr_rx_01();
void           sdr_rx_02();
void           sdr_rx_03();
void           sdr_rx_04();
void           sdr_rx_05();
void           sdr_rx_06();
void           sdr_rx_07();

void sdr_rx_00() {
  int16_t ac      = sdr_rx_common_i();
  func_ptr        = sdr_rx_01;
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1         = ac;
  int16_t ac2     = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  i_s1za1         = i_s1za0;
  process(ac2, q_ac2); // note: uses q_ac2 computed in sdr_rx_07 (global)
}
void sdr_rx_02() {
  int16_t ac = sdr_rx_common_i();
  func_ptr   = sdr_rx_03;
  i_s0zb1    = i_s0zb0;
  i_s0zb0    = ac;
}
void sdr_rx_04() {
  int16_t ac = sdr_rx_common_i();
  func_ptr   = sdr_rx_05;
  i_s1zb1    = i_s1zb0;
  i_s1zb0    = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1    = ac;
}
void sdr_rx_06() {
  int16_t ac = sdr_rx_common_i();
  func_ptr   = sdr_rx_07;
  i_s0zb1    = i_s0zb0;
  i_s0zb0    = ac;
}
void sdr_rx_01() {
  int16_t ac = sdr_rx_common_q();
  func_ptr   = sdr_rx_02;
  q_s0zb1    = q_s0zb0;
  q_s0zb0    = ac;
}
void sdr_rx_03() {
  int16_t ac = sdr_rx_common_q();
  func_ptr   = sdr_rx_04;
  q_s1zb1    = q_s1zb0;
  q_s1zb0    = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1    = ac;
}
void sdr_rx_05() {
  int16_t ac = sdr_rx_common_q();
  func_ptr   = sdr_rx_06;
  q_s0zb1    = q_s0zb0;
  q_s0zb0    = ac;
}
void sdr_rx_07() {
  int16_t ac      = sdr_rx_common_q();
  func_ptr        = sdr_rx_00;
  int16_t q_s1za0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1         = ac;
  q_ac2           = (q_s1za0 + (q_s1za1 + q_s1zb0) * 3 + q_s1zb1);
  q_s1za1         = q_s1za0;
}

// ---------------------------------------------------------------------------
// Audio output PWM (AF_OUT): comb in process(), integrator here (parity)
// ---------------------------------------------------------------------------
static int16_t ozi1, ozi2;

inline int16_t sdr_rx_common_q() {
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  return ADC - 511;
}
inline int16_t sdr_rx_common_i() {
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t        adc = ADC - 511;
  static int16_t prev_adc;
  int16_t        ac = (prev_adc + adc) / 2;
  prev_adc          = adc;
#ifdef AF_OUT
  if(_init) {
    ocomb = 0;
    ozi1  = 0;
    ozi2  = 0;
  } // first-sample hack
  ozi2   = ozi1 + ozi2; // Integrator section
  ozi1   = ocomb + ozi1;
  OCR1AL = min(max((ozi2 >> 5) + 128, 0), 255); // PWM audio out
#endif
  return ac;
}