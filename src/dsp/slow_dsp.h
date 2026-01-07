#ifndef SLOW_DSP_H
#define SLOW_DSP_H

#include <Arduino.h>
#include "usdx_config.h"
#include "ssb.h"
#include "agc.h"
#include "nr.h"
#include "filters.h"

extern volatile int16_t i, q;
extern volatile int16_t ocomb;
extern volatile uint32_t _absavg256;
extern volatile uint8_t _init;

inline int16_t slow_dsp(int16_t ac)
{
  static uint8_t absavg256cnt;
  if(!(absavg256cnt--)){ _absavg256 = _absavg256; _absavg256 = 0; } else _absavg256 += abs(ac);

  if(mode == AM) {
    ac = magn(i, q);
    static int16_t dc;
    dc += (ac - dc) / 2;
    ac = ac - dc;
  } else if(mode == FM){
    static int16_t zi;
    ac = ((ac + i) * zi);
    zi = i;
  }

#ifdef FAST_AGC
  if(agc == 2) {
    ac = process_agc(ac);
    ac = ac >> (16-volume);
  } else if(agc == 1){
    ac = process_agc_fast(ac);
    ac = ac >> (16-volume);
  }
#else
  if(agc == 1){
    ac = process_agc_fast(ac);
    ac = ac >> (16-volume);
  }
#endif
  else {
    if(volume <= 13)
      ac = ac >> (13-volume);
    else
      ac = ac << (volume-13);
  }

  if(nr) ac = process_nr(ac);

  if(filt) ac = filt_var(ac);

  ac = min(max(ac, -512), 511);

#ifndef QCX
  if(!dsp_cap) return 0;
#endif

  return ac;
}

void process(int16_t i_ac2, int16_t q_ac2);
void sdr_rx();
void sdr_rx_common();

#endif
