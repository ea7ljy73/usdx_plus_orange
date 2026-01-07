#include "ssb.h"
#include "../hal/gpio.h"
#include "../drivers/si5351.h"
#include "agc.h"
#include "nr.h"
#include "filters.h"
#include "slow_dsp.h"

#define MIC_ATTEN  0

extern uint8_t admux[3];

extern volatile int16_t p_sin;
extern volatile int16_t n_cos;
extern const uint32_t tones[];
extern inline void process_minsky();

extern int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
extern int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1, q_ac2;

inline void process_minsky()
{
  int16_t alpha = (int32_t)tones[cw_tone] * 51 / _F_SAMP_TX;
  p_sin += (int32_t)alpha * n_cos >> 8;
  n_cos -= (int32_t)alpha * p_sin >> 8;
}

static int16_t _adc;

void dsp_tx()
{
#ifdef MULTI_ADC
  int16_t adc;
  adc = ADC;
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
#ifdef QUAD
#ifdef TX_CLK0_CLK1
  si5351.SendRegister(16, (quad) ? 0x1f : 0x0f);
  si5351.SendRegister(17, (quad) ? 0x1f : 0x0f);
#else
  si5351.SendRegister(18, (quad) ? 0x1f : 0x0f);
#endif
#endif
  OCR1BL = amp;
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  int16_t df = ssb(_adc >> MIC_ATTEN);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  si5351.freq_calc_fast(df);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  #define AF_BIAS   32
  _adc = (adc/4 - (512 - AF_BIAS));
#else
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
  OCR1BL = amp;
  int16_t adc = ADC - 512;
  int16_t df = ssb(adc >> MIC_ATTEN);
  si5351.freq_calc_fast(df);
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  if(tx == 1){ OCR1BL = 0; si5351.SendRegister(SI_CLK_OE, TX0RX0); }
  if(tx == 255){ si5351.SendRegister(SI_CLK_OE, TX1RX0); }
#endif

#ifdef MOX_ENABLE
  if(!mox) return;
  OCR1AL = (adc << (mox-1)) + 128;
#endif
}

void dsp_tx_cw()
{
#ifdef KEY_CLICK
  if(OCR1BL < lut[255]) {
     for(uint16_t i = 31; i != 0; i--) {
        OCR1BL = lut[pgm_read_byte_near(&ramp[i-1])];
        delayMicroseconds(60);
     }
  }
#endif
  OCR1BL = lut[255];

  process_minsky();
  OCR1AL = (p_sin >> (8 + (16 - volume))) + 128;
}

void dsp_tx_am()
{
  ADCSRA |= (1 << ADSC);
  OCR1BL = amp;
  int16_t adc = ADC - 512;
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive-4);
  #define AM_BASE 32
  in=max(0, min(255, (in + AM_BASE)));
  amp=in;
}

void dsp_tx_fm()
{
  ADCSRA |= (1 << ADSC);
  OCR1BL = lut[255];
  si5351.SendPLLRegisterBulk();
  int16_t adc = ADC - 512;
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive);
  int16_t df = in;
  si5351.freq_calc_fast(df);
}

// RX Functions - CIC filter implementation from legacy

void sdr_rx_00()
{
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_01;
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
  int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  i_s1za1 = i_s1za0;
  process(ac2, q_ac2);
}

void sdr_rx_02()
{
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_03;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}

void sdr_rx_04()
{
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_05;
  i_s1zb1 = i_s1zb0;
  i_s1zb0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
}

void sdr_rx_06()
{
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_07;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}

void sdr_rx_01()
{
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_02;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}

void sdr_rx_03()
{
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_04;
  q_s1zb1 = q_s1zb0;
  q_s1zb0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
}

void sdr_rx_05()
{
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_06;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}

void sdr_rx_07()
{
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_00;
  int16_t q_s1za0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
  q_ac2 = (q_s1za0 + (q_s1za1 + q_s1zb0) * 3 + q_s1zb1);
  q_s1za1 = q_s1za0;
}
