// txamp_sim.cpp - envolvente de TX: ssb() REAL de produccion contra la de
// usdx-legazy (gen_txamp.py), comparando df Y amplitud con tx=1, y medida
// del ALC de pico (F3.8). Regla: mientras el ALC esta en reposo la salida es
// bit-exacta con legacy; cuando el audio satura, el ALC debe quitar el
// recorte SIN perder potencia de pico (PEP).
#include <initializer_list>
#include <math.h>
#include "sim_common.h"
#include <stdint.h>

extern "C" {
void    legacy_ssb(int16_t in);
void    new_ssb(int16_t in);
void    legacy_cfg(uint8_t m, uint8_t f, uint8_t d, uint8_t dig);
void    new_cfg(uint8_t m, uint8_t f, uint8_t d, uint8_t dig);
extern int16_t legacy_df, new_df;
extern uint8_t legacy_amp, new_amp;
}
#define FS 4800.0
static uint32_t seed = 4242;
static uint32_t rnd() { seed = seed * 1103515245u + 12345u; return seed >> 8; }
static void cfg(uint8_t m, uint8_t f, uint8_t d, uint8_t dig = 0) { legacy_cfg(m, f, d, 0); new_cfg(m, f, d, dig); }
static void both(int16_t in) { legacy_ssb(in); new_ssb(in); }
static void silence(double s) { for(long n = 0; n < (long)(s * FS); n++) both(0); } // el ALC vuelve a reposo
// "voz": tres formantes con envolvente silabica + ruido
static int16_t voice(long n, double a) {
  double t = n / FS, env = 0.55 + 0.45 * sin(2 * M_PI * 3.1 * t) * sin(2 * M_PI * 0.7 * t);
  return (int16_t)(a * env * (sin(2 * M_PI * 310 * t) + 0.6 * sin(2 * M_PI * 870 * t + 1) + 0.4 * sin(2 * M_PI * 1950 * t + 2)) +
                   ((a > 8) ? (int)(rnd() % 5) - 2 : 0));
}
static int16_t two(long n, double a) { return (int16_t)(a * sin(2 * M_PI * 700 * n / FS) + a * sin(2 * M_PI * 1100 * n / FS)); }

struct Sp { double imd3, pwr, pep, clipfrac; };
// analiza N=4800 muestras (1 s, tonos coherentes) de la SSB reconstruida amp*e^{j*fase}
static Sp analyse(bool legacy, double a, long n0) {
  static double re[4800], im[4800];
  static double ph_l = 0, ph_n = 0;
  double& ph = legacy ? ph_l : ph_n;
  Sp r = {0, 0, 0, 0};
  for(long n = 0; n < 4800; n++) {
    int16_t in = two(n0 + n, a);
    if(legacy) legacy_ssb(in); else new_ssb(in);
    int16_t df = legacy ? legacy_df : new_df;
    uint8_t am = legacy ? legacy_amp : new_amp;
    ph += 2 * M_PI * df / FS;
    re[n] = am / 255.0 * cos(ph);
    im[n] = am / 255.0 * sin(ph);
    r.pwr += (am / 255.0) * (am / 255.0);
    if(am > r.pep) r.pep = am;
    r.clipfrac += (am == 255);
  }
  auto bin = [&](double f) {
    double sr = 0, si = 0;
    for(long n = 0; n < 4800; n++) {
      double w = -2 * M_PI * f * n / FS;
      sr += re[n] * cos(w) - im[n] * sin(w);
      si += re[n] * sin(w) + im[n] * cos(w);
    }
    return sr * sr + si * si;
  };
  double want = (bin(700) + bin(1100)) / 2, i3 = fmax(bin(300), bin(1500));
  r.imd3 = 10 * log10(i3 / want);
  r.pwr = 10 * log10(r.pwr / 4800);
  r.clipfrac /= 4800;
  return r;
}

#include "txamp_div.h"
int main() {
  // --- A0: las divisiones por multiplicacion de ssb() son la division de C para TODO int16 ---
  {
    long bad3 = 0, bad9 = 0;
    for(long v = -32768; v <= 32767; v++) {
      int16_t x = (int16_t)v;
      if(v == -32768) continue; // -x no representable; ssb() nunca lo alcanza (|t| < 16000)
      bad3 += TX_DIV3(x) != x / 3;
      bad9 += TX_DIV9(x) != x / 9;
    }
    CHECK(bad3 == 0 && bad9 == 0, "A0 TX_DIV3/TX_DIV9 identicas a x/3 y x/9 en todo el rango int16");
  }
  // --- A1: sin saturar, df y amplitud identicos a legacy (todas las configuraciones) ---
  {
    long n = 0, bad_df = 0, bad_amp = 0, sat = 0;
    for(int drv = 0; drv <= 5; drv++) // con drive 6-7 casi cualquier audio satura (lo cubre A3)
      for(int f = 0; f <= 3; f++)
        for(int m = 0; m <= 1; m++) {
          cfg(m, f, drv);
          double a = 24.0 / (1 << drv); // pico de envolvente < 255 con margen
          for(long k = 0; k < 6000; k++) {
            both((k < 3000) ? voice(k, a) : two(k, a * 0.45));
            n++;
            bad_df += legacy_df != new_df;
            bad_amp += legacy_amp != new_amp;
            sat += legacy_amp == 255;
          }
        }
    printf("   sin saturar: %ld muestras, df distintos %ld, amplitud distinta %ld (muestras en 255: %ld)\n", n, bad_df, bad_amp, sat);
    CHECK(sat == 0, "A1 el estimulo no satura (test valido)");
    CHECK(bad_df == 0, "A1 fase (df) identica a legacy con MAX_DP (filt 0..3), LSB/USB y drive 0..5");
    CHECK(bad_amp == 0, "A1 envolvente identica a legacy mientras no hay saturacion");
  }
  // --- A2: drive=8 (amplitud constante) identico a legacy a cualquier nivel ---
  {
    long bad = 0;
    cfg(1, 0, 8);
    for(long k = 0; k < 20000; k++) {
      both((k % 4000 < 500) ? 0 : voice(k, 40 + (k % 300)));
      bad += (legacy_df != new_df) || (legacy_amp != new_amp);
    }
    CHECK(bad == 0, "A2 drive=8: df y amplitud identicos a legacy (incluido el silencio)");
  }
  // --- A3: la fase NUNCA depende del ALC: df identico tambien saturando ---
  {
    long bad = 0;
    cfg(1, 0, 4);
    silence(1);
    for(long k = 0; k < 30000; k++) {
      both(voice(k, 150));
      bad += legacy_df != new_df;
    }
    CHECK(bad == 0, "A3 saturando, la fase (df) sigue identica a legacy");
  }

  // --- B: ALC con dos tonos sobreexcitados (drive x nivel) ---
  printf("   drive  mic | legacy: IMD3  pot   | produccion: IMD3  pot    PEP  %%muestras en 255\n");
  double worst_imd = -99, worst_pwr = 0, worst_clip = 0, min_gain_imd = 99;
  int    pep_ok = 1;
  for(int drv : {2, 4, 6})
    for(double a : {32.0, 64.0}) {
      if(drv == 2 && a < 60) continue; // ahi no satura
      cfg(1, 0, drv);
      silence(25);
      for(long k = 0; k < 9600; k++) new_ssb(two(k, a)); // 2 s: ALC asentado
      for(long k = 0; k < 9600; k++) legacy_ssb(two(k, a));
      Sp l = analyse(true, a, 9600), p = analyse(false, a, 9600);
      printf("   %5d %4.0f |        %6.1f %5.1f |            %6.1f %5.1f  %4.0f  %5.2f\n", drv, a, l.imd3, l.pwr, p.imd3, p.pwr, p.pep, 100 * p.clipfrac);
      if(p.imd3 > worst_imd) worst_imd = p.imd3;
      if(p.pwr < worst_pwr) worst_pwr = p.pwr;
      if(p.clipfrac > worst_clip) worst_clip = p.clipfrac;
      if(l.imd3 - p.imd3 < min_gain_imd) min_gain_imd = l.imd3 - p.imd3;
      if(p.pep < 250) pep_ok = 0;
    }
  CHECK(worst_imd <= -24.0, "B1 saturando: IMD3 <= -24 dBc (legacy y el ALC anterior: -8..-10 dBc)");
  CHECK(min_gain_imd >= 12.0, "B1 mejora de IMD3 frente a legacy >= 12 dB en todos los casos");
  CHECK(pep_ok, "B2 la potencia de pico (PEP) se mantiene: envolvente llega a 250..255");
  CHECK(worst_pwr >= -4.3, "B2 potencia media >= -4,3 dB (dos tonos limpios a PEP completa = -3,0 dB; el ALC viejo daba -18)");
  CHECK(worst_clip <= 0.02, "B3 en regimen menos del 2 % de muestras tocan el tope (sin recorte sostenido)");

  // --- C: dinamica ---
  {
    cfg(1, 0, 4);
    silence(25);
    long clipped = 0, last_clip = -1;
    for(long k = 0; k < 4800; k++) { // entra un sobre-nivel de +24 dB de golpe
      new_ssb(two(k, 128));
      if(new_amp == 255) { clipped++; last_clip = k; }
    }
    long burst_end = 0; // ultimo instante con recorte denso (>25 % en ventana de 20 ms)
    {
      cfg(1, 0, 4); silence(25);
      static uint8_t c[4800];
      for(long k = 0; k < 4800; k++) { new_ssb(two(k, 128)); c[k] = new_amp == 255; }
      for(long k = 0; k + 96 <= 4800; k++) { int s = 0; for(int j = 0; j < 96; j++) s += c[k + j]; if(s > 24) burst_end = k + 96; }
    }
    printf("   ataque (+24 dB de golpe): recorte denso durante %.1f ms; %ld muestras en 255 en el primer segundo\n", burst_end / 4.8, clipped);
    (void)last_clip;
    CHECK(burst_end <= 144, "C1 ataque: el recorte desaparece en <= 30 ms");
    // vuelta: tras el sobre-nivel, a nivel normal la ganancia vuelve a la unidad (salida = legacy)
    long t_eq = -1, run = 0;
    for(long k = 0; k < 48000; k++) {
      int16_t in = two(k, 3);
      both(in);
      if(legacy_amp == new_amp) { if(++run == 2400 && t_eq < 0) t_eq = k - 2399; } else run = 0;
    }
    printf("   recuperacion: ganancia unidad (salida = legacy) a los %.1f s\n", t_eq / FS);
    CHECK(t_eq >= 0 && t_eq <= 8 * 4800, "C2 recuperacion: vuelve a ser bit-exacto con legacy en <= 8 s");
    // voz muy fuerte: nunca se queda sin salida ni pegado al tope
    cfg(1, 0, 4);
    silence(25);
    long hi = 0, n = 0, c255 = 0;
    double p = 0;
    for(long k = 0; k < 48000; k++) {
      new_ssb(voice(k, 120));
      if(k >= 9600) { n++; p += new_amp * (double)new_amp; hi += new_amp > 200; c255 += new_amp == 255; }
    }
    printf("   voz fuerte: potencia media %.1f dB re PEP, %.1f %% de muestras > 200, %.2f %% en 255\n", 10 * log10(p / n / 65025.0),
           100.0 * hi / n, 100.0 * c255 / n);
    CHECK(hi > n / 50 && c255 < n / 25, "C3 voz fuerte: usa toda la envolvente sin quedarse pegada al tope");
  }
  return SUMMARY();
}
