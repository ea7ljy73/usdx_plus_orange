#ifndef FILTERS_H
#define FILTERS_H

#include <Arduino.h>
#include "usdx_config.h"

#define N_FILT 7

inline int16_t filt_var(int16_t za0)
{
  static int16_t za1,za2;
  static int16_t zb0,zb1,zb2;
  static int16_t zc0,zc1,zc2;

  if(filt < 4)
  {
    static int16_t zz1,zz2;
    zz2=zz1;
    zz1=za0;
    za0=((((za0-zz2) << 5) - ((za0-zz2) << 1)) + (((zz1) << 4) + ((zz1) << 3) + zz1)) >> 5;

    switch(filt){
      case 1: zb0=((za0+2*za1+za2)>>1)-((((zb1 << 3) + (zb1 << 2) + zb1) + ((zb2 << 3) + (zb2 << 1) + zb2))>>4); break;
      case 2: zb0=((za0+2*za1+za2)>>1)-(((zb1<<1)+(zb2<<3))>>4); break;
      case 3: zb0=((za0+2*za1+za2)>>1)-((zb2<<2)>>4); break;
    }

    switch(filt){
      case 1: zc0=((zb0+2*zb1+zb2)>>1)-((((zc1 << 4) + (zc1 << 1)) + ((zc2 << 3) + (zc2 << 1) + zc2))>>4); break;
      case 2: zc0=((zb0+2*zb1+zb2)>>2)-(((zc1<<2)+(zc2<<3))>>4); break;
      case 3: zc0=((zb0+2*zb1+zb2)>>2)-((zc2<<2)>>4); break;
    }
  }
  else
  {
    switch(filt){
      case 4: zc0=(zb0-2*zb1+zb2)/1+(95L*zc1-52L*zc2)/64; break;
      case 5: zc0=(zb0-2*zb1+zb2)/4+(106L*zc1-59L*zc2)/64; break;
      case 6: zc0=(zb0-2*zb1+zb2)/16+(113L*zc1-62L*zc2)/64; break;
      case 7: zc0=(zb0-2*zb1+zb2)/32+(112L*zc1-62L*zc2)/64; break;
    }
  }
  zc2=zc1;
  zc1=zc0;

  zb2=zb1;
  zb1=zb0;

  za2=za1;
  za1=za0;

  return zc0 / 8;
}

#endif
