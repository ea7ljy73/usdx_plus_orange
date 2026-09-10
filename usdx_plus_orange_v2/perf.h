// perf.h - uSDX Plus Orange v2 (Fase 0: instrumentación)
// Medidor de carga CPU + loops/s. TODO tras #ifdef PERF_METER: con el flag
// apagado el coste es cero (funciones vacías inline).
//
// Uso:
//   arduino-cli compile --fqbn arduino:avr:uno --build-property compiler.cpp.extra_flags=-DPERF_METER .
// Sonda: PD5 (pin 11, legacy FREQCNT) en alto durante la ISR de TIMER2.
//   duty % en osciloscopio = carga RX/TX. Reporte loops/s por Serial cada 2 s
//   (desconectar CAT mientras se mide).

#pragma once

#include <Arduino.h>
#include <stdint.h>

#ifdef PERF_METER

#define PERF_PIN 5 // PD5 (pin 11). Libre en config REV3 (LPF por I2C).

inline void perf_init() {
  DDRD |= (1 << PERF_PIN);
  PORTD &= ~(1 << PERF_PIN);
}
inline void perf_isr_enter() { PORTD |= (1 << PERF_PIN); }
inline void perf_isr_exit() { PORTD &= ~(1 << PERF_PIN); }

volatile uint32_t perf_loops = 0;
inline void       perf_loop_tick() { perf_loops++; }
inline void       perf_report() {
  static uint32_t last = 0, last_loops = 0;
  uint32_t        now = millis();
  if((int32_t)(now - last) >= 2000) {
    uint32_t lps = (perf_loops - last_loops) * 1000 / (now - last);
    Serial.print("perf loops/s=");
    Serial.println(lps);
    last       = now;
    last_loops = perf_loops;
  }
}

#else // !PERF_METER: coste cero

inline void perf_init() {}
inline void perf_isr_enter() {}
inline void perf_isr_exit() {}
inline void perf_loop_tick() {}
inline void perf_report() {}

#endif
