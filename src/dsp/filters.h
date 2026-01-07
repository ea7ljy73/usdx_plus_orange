#ifndef FILTERS_H
#define FILTERS_H

#include <Arduino.h>
#include "usdx_config.h"

#define N_FILT 7

// Filtro Notch para eliminar interferencia de red (50/60Hz)
// Implementación de filtro IIR biquad
inline int16_t notch_filter(int16_t input) {
  static int16_t x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  // Coeficientes para notch a 60Hz con Fs=48kHz
  // b0 = 1, b1 = -1.902, b2 = 1, a1 = -1.893, a2 = 0.903
  // Optimizado para operaciones enteras
  const int16_t b1 = -1218;  // -1.902 * 640
  const int16_t b2 = 640;    // 1.0 * 640
  const int16_t a1 = -1212;  // -1.893 * 640
  const int16_t a2 = 578;    // 0.903 * 640

  int32_t y = (int32_t)input * 640 + (int32_t)x1 * b1 + (int32_t)x2 * b2
              - (int32_t)y1 * a1 - (int32_t)y2 * a2;

  x2 = x1;
  x1 = input;
  y2 = y1;
  y1 = y >> 10;  // Desnormalizar

  return (int16_t)(y >> 10);
}

// CW Peak Filter - Filtro paso-banda estrecho para CW
inline int16_t cw_filter(int16_t input) {
  static int16_t z1 = 0, z2 = 0, z3 = 0, z4 = 0, z5 = 0;
  // Filtro resonante centrado en ~600-800Hz para CW
  // Optimizado para respuesta rápida
  int32_t result = input + z1 * 2 + z2 - z4 * 2 - z5;
  z5 = z4;
  z4 = z3;
  z3 = z2;
  z2 = z1;
  z1 = input;
  return (int16_t)(result >> 3);
}

// Filtro de audio mejorado con respuesta optim variableizada
inline int16_t filt_var(int16_t za0)
{
  static int16_t za1,za2;
  static int16_t zb0,zb1,zb2;
  static int16_t zc0,zc1,zc2;

  // Filtros de paso bajo para voz (ancho de banda reducible)
  if(filt < 4)
  {
    static int16_t zz1,zz2;
    zz2=zz1;
    zz1=za0;
    // Filtro pre-énfasis para compensar respuesta del micrófono
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
    // Filtros de paso de banda más estrechos para SSB
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
