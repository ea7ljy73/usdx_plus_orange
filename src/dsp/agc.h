#ifndef AGC_H
#define AGC_H

#include <Arduino.h>
#include "usdx_config.h"

inline int16_t process_agc_fast(int16_t in)
{
  static int16_t gain = 1024;
  int16_t out = (gain >= 1024) ? (gain >> 10) * in : in;
  int16_t accum = (1 - abs(out >> 10));
  if((INT16_MAX - gain) > accum) gain = gain + accum;
  if(gain < 1) gain = 1;
  return out;
}

static int16_t centiGain = 128;
#define DECAY_FACTOR 400
static uint16_t decayCount = DECAY_FACTOR;
#define HI(x)  ((x) >> 8)
#define LO(x)  ((x) & 0xFF)

inline int16_t process_agc(int16_t in)
{
  static bool small = true;
  int16_t out;

  if(centiGain >= 128)
    out = (centiGain >> 5) * in;
  else
    out = (centiGain >> 2) * (in >> 3);
  out >>= 2;

  if(HI(abs(out)) > HI(1536)){
    centiGain -= (centiGain >> 4);
  } else {
    if(HI(abs(out)) > HI(1024))
      small = false;
    if(--decayCount == 0){
      if(small){
        if(centiGain < (INT16_MAX-(INT16_MAX >> 4)))
          centiGain += (centiGain >> 4);
        else
          centiGain = INT16_MAX;
      }
      decayCount = DECAY_FACTOR;
      small = true;
    }
  }
  return out;
}

void agc_reset();

#endif
