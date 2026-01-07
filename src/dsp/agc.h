#ifndef AGC_H
#define AGC_H

#include <Arduino.h>
#include "usdx_config.h"

// AGC Rápido - Para CW y operación de respuesta rápida
// Optimizado para evitar saturación mientras mantiene buena respuesta
inline int16_t process_agc_fast(int16_t in)
{
  static int32_t gain = 1024;  // Usar 32-bit para evitar overflow
  int32_t out = (gain >= 1024) ? (gain >> 10) * in : (int32_t)in * gain;
  int32_t abs_out = abs(out);
  int32_t accum = 1024 - (abs_out >> 10);

  // Limitar accum para evitar cambios bruscos
  if(accum < 0) accum = 0;
  if(accum > 256) accum = 256;

  // Attack: ganancia aumenta rápidamente
  gain = gain + accum;

  // Decay: ganancia disminuye lentamente si no hay señal fuerte
  if(gain > 1024) gain = gain - (gain >> 8);

  // Limitar ganancia
  if(gain < 64) gain = 64;
  if(gain > 32768) gain = 32768;

  return (int16_t)(out >> 10);
}

// AGC Lento - Para SSB con respuesta más natural
// Evita "breathing" y ruido de fondo excesivo
static int16_t centiGain = 128;
#define DECAY_FACTOR 500  // Ajustado para respuesta más natural
static uint16_t decayCount = DECAY_FACTOR;
#define HI(x)  ((x) >> 8)
#define LO(x)  ((x) & 0xFF)

// Umbrales optimizados para menos ruido de fondo
#define AGC_THRESHOLD_LOW 768    // Para iniciar recovery
#define AGC_THRESHOLD_HIGH 2048  // Para attack rápido
#define AGC_MAX_GAIN 16384
#define AGC_MIN_GAIN 64

inline int16_t process_agc(int16_t in)
{
  static bool small = true;
  int32_t out;

  // Aplicar ganancia con saturación
  if(centiGain >= 128)
    out = (int32_t)(centiGain >> 5) * in;
  else
    out = (int32_t)(centiGain >> 2) * (in >> 3);
  out >>= 2;

  int16_t abs_out = abs((int16_t)out);

  // Attack: ganancia disminuye rápidamente con señales fuertes
  if(abs_out > AGC_THRESHOLD_HIGH) {
    centiGain -= centiGain >> 5;
    if(centiGain < AGC_MIN_GAIN) centiGain = AGC_MIN_GAIN;
    small = false;
  }
  // Hold: mantener ganancia
  else if(abs_out > AGC_THRESHOLD_LOW) {
    small = false;
  }
  // Recovery: ganancia aumenta lentamente
  else {
    if(--decayCount == 0) {
      if(small || abs_out < AGC_THRESHOLD_LOW) {
        // Recovery lento para evitar pumping
        if(centiGain < (AGC_MAX_GAIN - (AGC_MAX_GAIN >> 5)))
          centiGain += centiGain >> 6;  // Recovery más lento
        else
          centiGain = AGC_MAX_GAIN;
      }
      decayCount = DECAY_FACTOR;
      small = true;
    }
  }

  // Limitar salida para evitar saturación del PWM
  if(out > 32767) out = 32767;
  if(out < -32768) out = -32768;

  return (int16_t)out;
}

inline void agc_reset() {
  centiGain = 128;
  decayCount = DECAY_FACTOR;
}

#endif
