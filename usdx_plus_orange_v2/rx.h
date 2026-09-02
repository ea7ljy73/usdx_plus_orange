// rx.h - uSDX Plus Orange v2
// Receive DSP: CIC decimation, Hilbert, demod (SSB/AM/FM), AGC, NR, NB.
// Extracted from v1 Section 10, behavior identical. AGC already includes the
// hang-time + noise-floor improvement.

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

#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)

#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha);

// ---------------------------------------------------------------------------
// Globals (shared with UI / CAT)
// ---------------------------------------------------------------------------
volatile uint8_t nr        = 2; // noise reduction level
volatile uint8_t nb_enable = 0; // noise blanker on/off
volatile uint8_t att       = 0; // analog attenuator
volatile uint8_t att2      = 2; // digital attenuator (CIC stage)
volatile uint8_t rf_atten  = 0;

extern volatile uint8_t agc; // agc select (1/2; from main)

uint32_t          absavg256  = 0;
volatile uint32_t _absavg256 = 0;
volatile int16_t  i, q;  // demodulated I/Q (smeter)
int16_t           v[14]; // Q delay line (Hilbert)
int16_t           vi[7]; // I delay line

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

// Fast AGC alternative (agc=1). Note: potential int16 overflow fixed with int32.
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
// slow_dsp() - post-decimation demod + AGC + NR + filter
// ---------------------------------------------------------------------------
int16_t qh; // global q-phase (shared with CIC output stage)

inline int16_t slow_dsp(int16_t i_ac2, int16_t q_ac2) {
  int16_t ac, acm;

  q_ac2 >>= att2; // digital gain control

  // Hilbert transform on Q (SSB/CW only)
  if(mode != AM_MODE && mode != FM_MODE) {
    qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 + ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
         ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + (v[6] - v[8]) / 2;
    // shuffle Q sample v[13]->v[0]
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

  i_ac2 >>= att2;
  i = i_ac2;
  q = q_ac2;

  int16_t id = vi[0];
  vi[0]      = vi[1];
  vi[1]      = vi[2];
  vi[2]      = vi[3];
  vi[3]      = vi[4];
  vi[4]      = vi[5];
  vi[5]      = vi[6];
  vi[6]      = i_ac2;

  if(mode == AM_MODE) {
    acm                   = -i - q; // S-meter
    ac                    = magn(i, q);
    static int32_t dc_avg = 0;
    dc_avg += (ac - dc_avg) >> 6; // ~19Hz high-pass to remove AM carrier
    ac = ac - dc_avg;
  } else if(mode == FM_MODE) {
    acm                   = -i - q; // S-meter
    static int16_t prev_i = 0, prev_q = 0;
    int32_t        product = (int32_t)i * prev_q - (int32_t)q * prev_i;
    int32_t        mag_sq  = (int32_t)i * i + (int32_t)q * q;
    if(mag_sq > 1000) {
      ac = (product << 4) / (mag_sq >> 3);
    } else {
      ac = 0;
    }
    prev_i = i;
    prev_q = q;
  } else { // USB, LSB, CW
    acm = -id - qh;
    ac  = acm;
  }

  static uint8_t absavg256cnt;
  if(!(absavg256cnt--)) {
    _absavg256 = absavg256;
    absavg256  = 0;
  } else
    absavg256 += abs(acm);

    // Noise blanker
#define NB_RATE_SHIFT 5
#define NB_THRESH_MULT 5
#define NB_MIN_LEVEL 10
#define NB_HOLD_SAMPLES 4
  if(nb_enable) {
    static int16_t  nb_prev  = 0;
    static uint16_t nb_level = 0;
    static uint8_t  nb_hold  = 0;
    int16_t         abs_ac   = ac < 0 ? -ac : ac;
    nb_level += ((int16_t)(abs_ac - (int16_t)nb_level) >> NB_RATE_SHIFT);
    if(nb_hold > 0) {
      nb_hold--;
      ac = nb_prev;
    } else if(abs_ac > (int16_t)nb_level * NB_THRESH_MULT && nb_level > NB_MIN_LEVEL) {
      nb_hold = NB_HOLD_SAMPLES;
      ac      = nb_prev;
    }
    nb_prev = ac;
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
// Audio output via PWM (upsampling CIC comb stage)
// ---------------------------------------------------------------------------
static int16_t ac3, ocomb, ozd1, ozd2;
static uint8_t tc;

inline void process_rx_dac(int16_t in) {
  ac3   = in;
  ozd1  = ac3 - ozd1; // comb section
  ocomb = ozd1 - ozd2;
  ozd2  = ac3;
}

// ---------------------------------------------------------------------------
// CIC decimator state machine (sdr_rx_00..07) - direct I/Q ADC sampling
// ---------------------------------------------------------------------------
volatile uint8_t admux[3];
volatile uint8_t rx_state = 0;

static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1;
static int16_t q_ac2;

#define M_SR 1 // CIC N=3

// sdr_rx_common_i/q read ADCs and feed the CIC.
// NOTE: full ISR chain (ADC config, output stage PWM) is wired in loop/setup.
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
  int16_t        ac = (prev_adc + adc) >> 1;
  prev_adc          = adc;
  OCR1AL            = min(max((ocomb >> 5) + 128, 0), 255); // PWM audio out
  return ac;
}

// The 8-phase CIC state machine (as v1 NEW_RX). func_ptr drives next phase.
void sdr_rx_00();
void sdr_rx_02();
void sdr_rx_04();
void sdr_rx_06();
void sdr_rx_01();
void sdr_rx_03();
void sdr_rx_05();
void sdr_rx_07();

// func_ptr is owned by hw.h; types must match exactly
typedef void (*func_t)(void);
extern volatile func_t func_ptr;

void sdr_rx_00() {
  int16_t ac      = sdr_rx_common_i();
  func_ptr        = sdr_rx_01;
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1         = ac;
  int16_t ac2     = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  i_s1za1         = i_s1za0;
  process_rx_dac(slow_dsp(ac2, q_ac2));
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
