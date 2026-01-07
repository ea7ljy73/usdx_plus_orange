#ifndef NR_H
#define NR_H

#include <Arduino.h>
#include "usdx_config.h"

// Macros para filtrado exponencial optimizado
#define EA(y, x, one_over_alpha)  (y) = (y) + ((x) - (y)) / (one_over_alpha)
#define MLEA(y, x, L, M)  (y)  = (y) + ((((x) - (y)) >> (L)) - (((x) - (y)) >> (M)))

// NR Básico - Filtro EMA adaptativo
// Nivel 1: respuesta rápida (alpha=32)
// Nivel 2: respuesta media (alpha=64)
// Nivel 3: respuesta lenta (alpha=128)
inline int16_t process_nr(int16_t in)
{
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr + 4));  // nr=0->16, nr=1->32, nr=2->64
  return ea1;
}

// NR Spectral Subtraction - Reduce ruido de fondo
// Mantiene un promedio del espectro de ruido y lo sustrae
inline int16_t process_nr_spectral(int16_t in)
{
  static int16_t noise_floor = 0;
  static int16_t signal_avg = 0;
  static uint16_t count = 0;

  // Actualizar promedio de ruido con partes silenciosas
  if(abs(in) < 50) {
    noise_floor = EA(noise_floor, in * in, 128);
  }

  // Promedio de señal
  signal_avg = EA(signal_avg, in * in, 32);

  // SNR estimado
  int16_t snr = signal_avg - noise_floor;

  // Si SNR es bajo, aplicar más atenuación
  if(snr < noise_floor * 2) {
    // Señal probablemente es ruido, atenuar
    return in >> 1;
  }

  return in;
}

// NR Adaptativo - Combina filtrado con detección de señal
inline int16_t process_nr_adaptive(int16_t in)
{
  static int16_t avg_abs = 0;
  static int16_t threshold = 100;

  // Promedio del valor absoluto
  avg_abs = EA(avg_abs, abs(in), 64);

  // Actualizar threshold basado en el nivel de ruido de fondo
  if(avg_abs < threshold) {
    threshold = EA(threshold, avg_abs, 256);
  }

  // Si la señal está por encima del threshold + margen, pasar completa
  // Si está cerca del threshold, aplicar filtrado suave
  if(abs(in) > threshold * 3) {
    return in;  // Señal fuerte, pasar sin filtrar
  }

  // Señal débil, aplicar NR
  static int16_t filtered;
  filtered = EA(filtered, in, 1 << (nr + 4));
  return filtered;
}

#endif
