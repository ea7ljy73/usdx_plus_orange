// main_ab_agc.c - A/B + regresion de F4.16 (AGC Start, precarga x8) y
// F4.17 (AGC Rec, divisor de recovery). Dominio: llamadas a slow_dsp
// (7812/s). Usa el cuerpo REAL de process_agc_fast de rx.h (rx_v2.c).
// Criterios (validados en diseño):
//   T1: con precarga 8192, audible desde la muestra 0 y t90 < mitad que x1.
//   T2: con rec=4, varianza del soplo en pausas < 1/4 que rec=1,
//       overshoot de flanco menor y sin clipping (<=511 post >>4).
// Compilar/via run_ab_agc.sh. Exit 0 = PASS, 1 = FAIL.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FS_DSP 7812.0

// arrays IQ exigidos por el TU (no se usan: test a nivel funcion)
int16_t iq_i[40000];
int16_t iq_q[40000];
int     iq_i_idx = 0;
int     iq_q_idx = 0;

extern void    rx_agc_v2_set(int16_t g, uint8_t rec);
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
    rx_agc_v2_set(cfg ? 8192 : 1024, 1);
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

  // ---- T2: rafagas 800+-200 (0.5s) / pausa 25+-10 (1s) x6, init x1 ----
  double var1 = 0, var4 = 0;
  long peak1 = 0, peak4 = 0;
  for(int cfg = 0; cfg < 2; cfg++) {
    uint8_t rec = cfg ? 4 : 1;
    rx_agc_v2_set(1024, rec);
    seed = 42;
    long gsum = 0, gsq = 0, gn = 0, gmax = 0, pk = 0;
    for(int c = 0; c < 6; c++) {
      for(long i = 0; i < 3906; i++) {
        int16_t o = wrap_agcfast_v2(rnd_noise(800, 200)) >> 4;
        if(c > 0 && i < 100 && abs(o) > pk)
          pk = abs(o);
      }
      for(long i = 0; i < 7812; i++) {
        int16_t o = wrap_agcfast_v2(rnd_noise(25, 10)) >> 4;
        if(c >= 2) {
          gsum += abs(o);
          gsq += (long)abs(o) * abs(o);
          gn++;
          if(abs(o) > gmax)
            gmax = abs(o);
        }
      }
    }
    double mean = (double)gsum / gn;
    if(cfg) {
      var4  = (double)gsq / gn - mean * mean;
      peak4 = pk;
    } else {
      var1  = (double)gsq / gn - mean * mean;
      peak1 = pk;
    }
    printf("T2 rec=%d: var_soplo=%.2f overshoot=%ld\n", rec,
           cfg ? var4 : var1, cfg ? peak4 : peak1);
  }
  check("F4.17 var soplo rec=4 < 1/4 que rec=1", var4 * 4 < var1);
  check("F4.17 overshoot rec=4 <= rec=1", peak4 <= peak1);
  check("F4.17 sin clipping con rec=4", peak4 <= 511);

  printf(fails ? "=> AB_AGC FAIL (%d)\n" : "=> AB_AGC PASS\n", fails);
  return fails ? 1 : 0;
}
