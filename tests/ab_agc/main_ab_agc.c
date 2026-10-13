// main_ab_agc.c - A/B + regresion de F4.16 (AGC Start, precarga x8) y
// F4.17 (AGC Hang, retencion del AGC Slow). Dominio: llamadas a slow_dsp
// (7812/s). Usa el cuerpo REAL de process_agc_fast de rx.h (rx_v2.c).
// Criterios (validados en diseño):
//   T1: con precarga 8192, audible desde la muestra 0 y t90 < mitad que x1.
//   T2: AGC Slow con Hang 5: sin bombeo en pausas de 0,5 s (<1 dB; legacy >3)
//       y la senal debil posterior recuperada a los 2 s.
// Compilar/via run_ab_agc.sh. Exit 0 = PASS, 1 = FAIL.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FS_DSP 7812.0

// arrays IQ exigidos por el TU (no se usan: test a nivel funcion)
int16_t iq_i[40000];
int16_t iq_q[40000];
int     iq_i_idx = 0;
int     iq_q_idx = 0;

extern void    rx_agc_v2_set(int16_t g);
extern void    rx_agcslow_v2_set(uint8_t hang);
extern int16_t rx_centigain_v2(void);
extern int16_t wrap_agc_v2(int16_t x);
extern int16_t rx_gain_v2(void);
extern int16_t wrap_agcfast_v2(int16_t x);

static unsigned seed;
static int16_t rnd_noise(int16_t center, int16_t span) {
  seed = seed * 1103515245 + 12345;
  return (int16_t)(center + ((seed >> 8) % (2 * span + 1)) - span);
}

static int fails = 0;
static void check(const char* name, int cond) {
  printf("  [%s] %s\n", cond ? "PASS" : "FAIL", name);
  if(!cond)
    fails++;
}

int main(void) {
  printf("== AB AGC v2 (F4.16/F4.17) ==\n");

  // ---- T1: rampa debil in=5+-5 (banda floja real), rec=1, init 1024 vs 8192 ----
  long t_aud_1024 = -1, t90_1024 = -1, t_aud_8192 = -1, t90_8192 = -1;
  for(int cfg = 0; cfg < 2; cfg++) {
    rx_agc_v2_set(cfg ? 8192 : 1024);
    seed           = 99;
    long ta = -1, t90 = -1, n = 0;
    while(n < (long)(FS_DSP * 40)) {
      int16_t o = wrap_agcfast_v2(rnd_noise(5, 5)) >> 4; // volume 12
      if(ta < 0 && (o >= 3 || o <= -3))
        ta = n;
      if(t90 < 0 && rx_gain_v2() >= 29000)
        t90 = n;
      if(ta >= 0 && t90 >= 0)
        break;
      n++;
    }
    if(cfg) {
      t_aud_8192 = ta;
      t90_8192   = t90;
    } else {
      t_aud_1024 = ta;
      t90_1024   = t90;
    }
  }
  printf("T1 rampa debil: x1 t_aud=%.2fs t90=%.2fs | x8 t_aud=%.2fs t90=%.2fs\n",
         t_aud_1024 / FS_DSP, t90_1024 / FS_DSP, t_aud_8192 / FS_DSP, t90_8192 / FS_DSP);
  check("F4.16 audible al instante con x8", t_aud_8192 == 0);
  check("F4.16 sin x8 tarda (el problema existe)", t_aud_1024 / FS_DSP > 0.25);
  check("F4.16 t90 x8 < x1 (converge antes)", t90_8192 > 0 && t90_1024 > 0 && t90_8192 < t90_1024);

  // ---- T2 (F4.17 AGC Hang, AGC Slow/M0PUB): voz 0,4 s / pausa 0,5 s con ruido x6,
  // luego senal 20 dB mas debil. Hang 1 = legacy; Hang 5 = 0,4 s de retencion.
  double pump[2], rec2[2];
  for(int cfg = 0; cfg < 2; cfg++) {
    uint8_t hang = cfg ? 5 : 1;
    rx_agcslow_v2_set(hang);
    seed = 42;
    for(long i = 0; i < 5 * 7812; i++) wrap_agc_v2((int16_t)(2000 * sin(2 * M_PI * 1000 * i / FS_DSP)));
    double g0 = 0, g1 = 0;
    for(int c = 0; c < 6; c++) {
      for(long i = 0; i < 3125; i++) wrap_agc_v2((int16_t)(2000 * sin(2 * M_PI * 1000 * i / FS_DSP)));
      for(long i = 0; i < 3906; i++) {
        wrap_agc_v2(rnd_noise(0, 100));
        if(c >= 2 && i == 400) g0 += rx_centigain_v2();
        if(c >= 2 && i == 3900) g1 += rx_centigain_v2();
      }
    }
    pump[cfg] = 20 * log10(g1 / g0);
    double l2 = 0, l5 = 0;
    for(long i = 0; i < 6 * 7812; i++) {
      int16_t o = wrap_agc_v2((int16_t)(200 * sin(2 * M_PI * 1000 * i / FS_DSP)));
      if(i >= 2 * 7812 && i < 2 * 7812 + 2000) l2 += abs(o);
      if(i >= 5 * 7812 && i < 5 * 7812 + 2000) l5 += abs(o);
    }
    rec2[cfg] = l2 / l5;
    printf("T2 hang=%d: bombeo en pausa %.1f dB, senal debil a 2 s = %.0f %% del nivel final\n", hang, pump[cfg], 100 * rec2[cfg]);
  }
  check("F4.17 el bombeo existe sin retencion (>3 dB)", pump[0] > 3);
  check("F4.17 Hang 5 lo elimina (<1 dB)", pump[1] < 1);
  check("F4.17 Hang 5 recupera la senal debil en 2 s (>80 %)", rec2[1] > 0.8);

  printf(fails ? "=> AB_AGC FAIL (%d)\n" : "=> AB_AGC PASS\n", fails);
  return fails ? 1 : 0;
}
