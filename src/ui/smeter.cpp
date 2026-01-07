#include "smeter.h"
#include "../hal/adc.h"
#include "../dsp/slow_dsp.h"

void smeter_init()
{
  max_absavg256 = 0;
  dbm = -73;
}

void smeter_update()
{
  uint32_t avg = _absavg256;
  if(avg > max_absavg256) max_absavg256 = avg;

  if(max_absavg256 > 0){
    int16_t p = (int16_t)(10 * log10(avg) + 10 * log10(1000000UL / 4096UL) - 10 * log10(max_absavg256) - 73);
    dbm = (p > -73) ? p : -73;
  }
}

uint8_t smeter_read()
{
  if(dbm <= -73) return 0;
  if(dbm < -63) return 1 + (dbm + 73);
  if(dbm < -53) return 9 + (dbm + 63) / 2;
  return 9 + (dbm + 43) / 10;
}

int16_t smeter_get_dbm()
{
  return dbm;
}
