// eng_lab.h - uSDX Lab (firmware de laboratorio, NO produccion)
// Monitor ADC + audio tras flag de menu (0=OFF, 1=ADC, 2=AUDIO).
// Vive solo en usdx-lab/: aqui hay sitio (sin CAT) para crest/divisiones.
// Acumuladores enteros; sqrt y promedios SOLO al refrescar pantalla (2 Hz).

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "display.h" // lcd
#include "menu.h"    // menu.state (MENU_MAIN)

// ---------------------------------------------------------------------------
// Selector (parametro de menu, eslot 35): 0=OFF, 1=ADC, 2=AUDIO
// ---------------------------------------------------------------------------
volatile uint8_t eng_view = 0;

// ---------------------------------------------------------------------------
// Raiz cuadrada entera 32->16 (Newton bit a bit)
// ---------------------------------------------------------------------------
inline uint16_t eng_isqrt(uint32_t x) {
  uint32_t r = 0, b = 1UL << 30;
  while(b) {
    uint32_t t = r + b;
    r >>= 1;
    if(x >= t) {
      x -= t;
      r += b;
    }
    b >>= 2;
  }
  return (uint16_t)r;
}

// ---------------------------------------------------------------------------
// Acumuladores por bloque (los llena la ISR, los vuelca loop).
// N potencia de 2: los promedios son shifts.
// ---------------------------------------------------------------------------
#define ENG_ADC_SH 12 // bloque 4096 (~65 ms a 62.5 kHz)
#define ENG_AU_SH 11  // bloque 2048 (~262 ms a 7812 Hz)

volatile uint32_t eng_adc_sum = 0, eng_adc_sq = 0; // sq sobre (raw>>1)
volatile uint16_t eng_adc_min = 1023, eng_adc_max = 0;
volatile uint16_t eng_adc_clip = 0, eng_adc_n = 0;

volatile int32_t  eng_au_sum = 0;
volatile uint32_t eng_au_sq  = 0;
volatile uint16_t eng_au_peak = 0, eng_au_clip = 0, eng_au_n = 0;

// Tap ADC crudo (desde sdr_rx_common_i, raw 0..1023)
__attribute__((noinline)) void eng_feed_adc(uint16_t raw) {
  if(!eng_view)
    return;
  if(eng_adc_n < (1U << ENG_ADC_SH)) {
    eng_adc_sum += raw;
    uint16_t h = raw >> 1;
    eng_adc_sq += (uint32_t)h * h;
    if(raw < eng_adc_min)
      eng_adc_min = raw;
    if(raw > eng_adc_max)
      eng_adc_max = raw;
    if(raw == 0 || raw >= 1023)
      eng_adc_clip++;
    eng_adc_n++;
  }
}

// Tap audio final (desde slow_dsp, tras clamp +-511)
__attribute__((noinline)) void eng_feed_audio(int16_t ac) {
  if(!eng_view)
    return;
  if(eng_au_n < (1U << ENG_AU_SH)) {
    uint16_t a = (ac < 0) ? (uint16_t)-ac : (uint16_t)ac;
    eng_au_sum += ac;
    eng_au_sq += (uint32_t)a * a;
    if(a > eng_au_peak)
      eng_au_peak = a;
    if(a >= 511)
      eng_au_clip++;
    eng_au_n++;
  }
}

// Impresion entera via ltoa (ya enlazado por el menu)
inline void eng_print(long v) {
  char b[8];
  ltoa(v, b, 10);
  lcd.print(b);
}

inline void eng_blank() { lcd.print("                "); }

inline void eng_render_adc() {
  noInterrupts();
  uint32_t sum = eng_adc_sum, sq = eng_adc_sq;
  uint16_t mn = eng_adc_min, mx = eng_adc_max, cl = eng_adc_clip, n = eng_adc_n;
  eng_adc_sum  = 0;
  eng_adc_sq   = 0;
  eng_adc_min  = 1023;
  eng_adc_max  = 0;
  eng_adc_clip = 0;
  eng_adc_n    = 0;
  interrupts();
  if(!n)
    return;
  uint16_t mean  = (uint16_t)(sum >> ENG_ADC_SH);
  uint16_t rms   = eng_isqrt(sq >> ENG_ADC_SH) * 2; // (raw>>1)^2 -> x2
  uint16_t range = mx - mn;
  lcd.setCursor(0, 0);
  eng_blank();
  lcd.setCursor(0, 0);
  lcd.print("ADC ");
  eng_print(mean);
  lcd.print(' ');
  eng_print(mn);
  lcd.print(' ');
  eng_print(mx);
  lcd.setCursor(0, 1);
  eng_blank();
  lcd.setCursor(0, 1);
  lcd.print("RMS ");
  eng_print(rms);
  lcd.print(" CL");
  eng_print(cl);
  lcd.print(" R");
  eng_print(range);
}

inline void eng_render_audio() {
  noInterrupts();
  int32_t  sum = eng_au_sum;
  uint32_t sq  = eng_au_sq;
  uint16_t pk = eng_au_peak, cl = eng_au_clip, n = eng_au_n;
  eng_au_sum  = 0;
  eng_au_sq   = 0;
  eng_au_peak = 0;
  eng_au_clip = 0;
  eng_au_n    = 0;
  interrupts();
  if(!n)
    return;
  int16_t  dc    = (int16_t)(sum >> ENG_AU_SH); // shift aritmetico int32
  uint16_t rms   = eng_isqrt(sq >> ENG_AU_SH);
  uint16_t crest = rms ? (uint16_t)((uint16_t)(pk * 10) / rms) : 0; // x10 (16 = 1.6)
  lcd.setCursor(0, 0);
  eng_blank();
  lcd.setCursor(0, 0);
  lcd.print("AU R");
  eng_print(rms);
  lcd.print(" P");
  eng_print(pk);
  lcd.setCursor(0, 1);
  eng_blank();
  lcd.setCursor(0, 1);
  lcd.print("DC");
  lcd.print(dc >= 0 ? '+' : '-');
  eng_print(dc >= 0 ? dc : (int16_t)-dc);
  lcd.print(" CR");
  eng_print(crest);
  lcd.print(" CL");
  eng_print(cl);
}

inline void eng_tick() { // llamado desde display_tick con eng_view!=0, MENU_MAIN, RX
  static uint32_t last = 0;
  uint32_t        now  = millis();
  if((int32_t)(now - last) < 500)
    return;
  last = now;
  if(eng_view == 1)
    eng_render_adc();
  else if(eng_view == 2)
    eng_render_audio();
}
