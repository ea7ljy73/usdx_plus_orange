#ifndef NR_H
#define NR_H

#include <Arduino.h>
#include "usdx_config.h"

#define EA(y, x, one_over_alpha)  (y) = (y) + ((x) - (y)) / (one_over_alpha)
#define MLEA(y, x, L, M)  (y)  = (y) + ((((x) - (y)) >> (L)) - (((x) - (y)) >> (M)))

inline int16_t process_nr_old(int16_t ac)
{
  ac = ac >> (6-abs(ac));
  ac = ac << 3;
  return ac;
}

inline int16_t process_nr_old2(int16_t ac)
{
  static int16_t ea1;
  ea1 = EA(ea1, ac, 64);
  return ea1;
}

inline int16_t process_nr(int16_t in)
{
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr-1) );
  return ea1;
}

#endif
