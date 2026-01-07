#include "bands.h"
#include "state.h"

void band_next()
{
  band++;
  if(band >= N_BANDS) band = 0;
  bandswitch(band);
}

void band_prev()
{
  if(band == 0) band = N_BANDS - 1;
  else band--;
  bandswitch(band);
}

uint8_t band_get_index(uint32_t freq)
{
  uint8_t i = 0;
  for(; i < N_BANDS; i++){
    if(freq < band_freqs[i+1] * 1000UL) break;
  }
  return i;
}

uint32_t band_get_center(uint8_t band_index)
{
  return band_freqs[band_index] * 1000UL;
}
