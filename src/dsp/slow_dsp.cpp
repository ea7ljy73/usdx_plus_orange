#include "slow_dsp.h"
#include "agc.h"
#include "nr.h"
#include "filters.h"

void process(int16_t i_ac2, int16_t q_ac2)
{
  static int16_t ac3;
  static int16_t ozd1, ozd2;
  if(_init){ ac3 = 0; ozd1 = 0; ozd2 = 0; _init = 0; }
  int16_t od1 = ac3 - ozd1;
  ocomb = od1 - ozd2;
  interrupts();
  ozd2 = od1;
  ozd1 = ac3;

  qh = 0;
  {
    q_ac2 >>= att2;
    static int16_t v[14];
    qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 + ((v[4] - v[10]) + (v[6] - v[8])) / 8 + ((v[4] - v[10]) * 5 - (v[6] - v[8]) ) / 128 + (v[6] - v[8]) / 2;
    v[0] = v[1]; v[1] = v[2]; v[2] = v[3]; v[3] = v[4]; v[4] = v[5]; v[5] = v[6]; v[6] = v[7]; v[7] = v[8]; v[8] = v[9]; v[9] = v[10]; v[10] = v[11]; v[11] = v[12]; v[12] = v[13]; v[13] = q_ac2;
  }
  i_ac2 >>= att2;
  static int16_t v_i[7];
  int16_t delay_i = v_i[0]; v_i[0] = v_i[1]; v_i[1] = v_i[2]; v_i[2] = v_i[3]; v_i[3] = v_i[4]; v_i[4] = v_i[5]; v_i[5] = v_i[6]; v_i[6] = i_ac2;
  i = delay_i;
  q = q_ac2;
  ac3 = slow_dsp(-i - qh);
}

inline int16_t sdr_rx_common_i()
{
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  static int16_t prev_adc;
  int16_t ac = (prev_adc + adc) / 2;
  prev_adc = adc;
  return ac;
}

inline int16_t sdr_rx_common_q()
{
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  int16_t ac = ADC - 511;
  return ac;
}

void sdr_rx()
{
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_q;
}

void sdr_rx_q()
{
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx;
}
