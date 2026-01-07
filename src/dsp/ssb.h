#ifndef SSB_H
#define SSB_H

#include <Arduino.h>
#include "usdx_config.h"

#define F_SAMP_TX 4800
#if(F_MCU != 20000000)
const int16_t _F_SAMP_TX = (F_MCU * 4800LL / 20000000);
#else
#define _F_SAMP_TX  F_SAMP_TX
#endif

#define _UA  600
#define MAX_DP  ((filt == 0) ? _UA : (filt == 3) ? _UA/4 : _UA/2)
#define CARRIER_COMPLETELY_OFF_ON_LOW  1
#define MULTI_ADC  1
#define M_SR  1  // CIC decimation factor

#define magn(i, q) (abs(i) > abs(q) ? abs(i) + abs(q) / 4 : abs(q) + abs(i) / 4)

inline int16_t arctan3(int16_t q, int16_t i)
{
#define _atan2(z)  (_UA/8 + _UA/22 - _UA/22 * z) * z
  int16_t r;
  if(abs(q) > abs(i))
    r = _UA / 4 - _atan2(abs(i) / abs(q));
  else
    r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
#undef _atan2
}

inline int16_t ssb(int16_t in)
{
  static int16_t dc, z1;
  int16_t i, q;
  uint8_t j;
  static int16_t v[16];
  for(j = 0; j != 15; j++) v[j] = v[j + 1];

#ifdef MORE_MIC_GAIN
  if(dig_mode){
    int16_t ac = in;
    dc = (ac + (7) * dc) / (7 + 1);
    v[15] = (ac - dc) / 2;
  } else {
    int16_t ac = in * 2;
    ac = ac + z1;
    z1 = (in - (2) * z1) / (2 + 1);
    dc = (ac + (2) * dc) / (2 + 1);
    v[15] = (ac - dc);
  }
  i = v[7] * 2;
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) + (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) + ((v[6] - v[8]) << 4)) >> 6) + (v[6] - v[8]);
  uint16_t _amp = magn(i / 2, q / 2);
#else
  dc = (in + dc) / 2;
  int16_t ac = (in - dc);
  v[15] = (ac + z1);
  z1 = ac;

  i = v[7];
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) + (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) + (((v[6] - v[8]) << 4) - (v[6] - v[8]))) >> 7) + ((v[6] - v[8]) >> 1);
  uint16_t _amp = magn(i, q);
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  vox = (_amp > vox_thresh);
#else
  if(vox) vox = (_amp > vox_thresh);
#endif

  _amp = _amp << (drive);
  _amp = ((_amp > 255) || (drive == 8)) ? 255 : _amp;
  amp = (tx) ? lut[_amp] : 0;

  static int16_t prev_phase;
  int16_t phase = arctan3(q, i);
  int16_t dp = phase - prev_phase;
  prev_phase = phase;

  if(dp < 0) dp = dp + _UA;
#ifdef QUAD
  if(dp >= (_UA/2)){ dp = dp - _UA/2; quad = !quad; }
#endif

#ifdef MAX_DP
  if(dp > MAX_DP){
    prev_phase = phase - (dp - MAX_DP);
    dp = MAX_DP;
  }
#endif
  if(mode == USB)
    return dp * ( _F_SAMP_TX / _UA);
  else
    return dp * (-_F_SAMP_TX / _UA);
}

void dsp_tx();
void dsp_tx_cw();
void dsp_tx_am();
void dsp_tx_fm();

void sdr_rx_00();
void sdr_rx_01();
void sdr_rx_02();
void sdr_rx_03();
void sdr_rx_04();
void sdr_rx_05();
void sdr_rx_06();
void sdr_rx_07();

#endif
