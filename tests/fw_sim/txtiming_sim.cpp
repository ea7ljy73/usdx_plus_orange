// txtiming_sim.cpp - simulacion de la senal RF de TX SSB (EER polar) con la
// temporizacion real del bus I2C. Cadena: dos tonos de micro -> ssb() REAL de
// tx.h (tests/ab/ab_tx.c) -> freq_calc_fast + SendPLLRegisterBulk REALES ->
// modelo del SI5351 donde cada byte de registro surte efecto al escribirse ->
// portadora compleja a 1,23 MHz -> FFT.
// Compara tres temporizaciones: IDEAL (todo al inicio de la muestra), LEGACY
// (bulk completo: retardo constante) y PRODUCCION (bulk diferencial F1.3: el
// instante en que cambian frecuencia y amplitud depende del dato).
#include <complex>
#include <math.h>
#include <vector>
// (C++ antes que el stub de Arduino: sus macros min/max rompen la STL)
#include "sim_common.h"
#include <Arduino.h>

uint32_t   sim_millis = 0;
uint8_t    sim_pin[32];
void       sim_on_ms(void) {}
SerialStub Serial;
LcdStub    lcd;

#include "i2c.h"
I2CRec i2c;
#include "si5351_under_test.h"
SI5351 si5351;
#include "si5351_legacy.h"

extern "C" {
void    ab_tx_init(void);
void    ab_ssb(int16_t in);
void    ab_dig_mode(uint8_t v);
extern int16_t          ab_df_out;
extern uint8_t          ab_amp_out;
extern volatile uint8_t drive;
}

#define FS 4800
#define OS 256                 // 1 tick = 0,814 us
#define TICK_BYTE 16           // 13 us por byte I2C (9 bits a 731 kbit/s)
#define TICK_EDGE 2            // start / stop
#define LOG2N 20
#define NFFT (1 << LOG2N)
enum { IDEAL, LEGACY, PROD };
typedef std::complex<double> cd;

static void fft(std::vector<cd>& a) {
  const size_t n = a.size();
  for(size_t i = 1, j = 0; i < n; i++) {
    size_t bit = n >> 1;
    for(; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if(i < j) std::swap(a[i], a[j]);
  }
  for(size_t len = 2; len <= n; len <<= 1) {
    double ang = -2 * M_PI / len;
    cd wl(cos(ang), sin(ang));
    for(size_t i = 0; i < n; i += len) {
      cd w(1);
      for(size_t j = 0; j < len / 2; j++) {
        cd u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}
static std::vector<double> psd;
static double band(double f0, double f1) { // potencia entre f0 y f1 Hz (con signo)
  const double df = (double)FS * OS / NFFT;
  double p = 0;
  for(long k = (long)floor(f0 / df); k <= (long)ceil(f1 / df); k++)
    p += psd[(size_t)((k % NFFT + NFFT) % NFFT)];
  return p;
}
static double dB(double p, double ref) { return 10 * log10((p + 1e-30) / ref); }

struct Res { double imd3, imd5, img, adj, far, jitter_us, bytes; };

// offset de frecuencia de salida (Hz) segun el estado de los registros del chip
static double f_of(const uint8_t* r, uint8_t div, double f0) {
  uint32_t p1 = ((uint32_t)(r[28] & 3) << 16) | ((uint32_t)r[29] << 8) | r[30];
  uint32_t p2 = ((uint32_t)(r[31] & 15) << 16) | ((uint32_t)r[32] << 8) | r[33];
  return (double)F_XTAL * (((double)p1 + 512.0) + (double)p2 / 65536.0) / 128.0 / div - f0;
}

static Res run(int timing, int32_t carrier, double a1, double f1, double a2, double f2, uint8_t drv, uint8_t digi) {
  I2CRec&  bus = (timing == PROD) ? i2c : legacy::i2c;
  si5351.freq(carrier, 0, 90);
  legacy::si5351.freq(carrier, 0, 90);
  uint8_t chip[256];
  memcpy(chip, bus.reg, 256);
  const uint8_t div = si5351._div;
  const double  f0  = f_of(chip, div, 0);
  ab_tx_init();
  ab_dig_mode(digi);
  drive = drv;
  for(int n = 0; n < 2400; n++) // asentar el estado de ssb()
    ab_ssb((int16_t)(a1 * sin(2 * M_PI * f1 * n / FS) + a2 * sin(2 * M_PI * f2 * n / FS)));

  std::vector<cd> x(NFFT);
  double  phase = 0, amp = 0, fcur = 0, sumd = 0, sumd2 = 0;
  uint8_t amp_next = 0;
  uint32_t b0 = bus.bytes;
  const long NS = NFFT / OS;
  for(long n = 0; n < NS; n++) {
    // --- lo que hace dsp_tx(): 1) envia los registros calculados la muestra anterior
    if(timing == PROD) si5351.SendPLLRegisterBulk(); else legacy::si5351.SendPLLRegisterBulk();
    int ndata = (bus.n > 2) ? bus.n - 2 : 0;
    int first = bus.stream[1];
    int t_byte[8], t_amp;
    if(timing == IDEAL) { for(int k = 0; k < 8; k++) t_byte[k] = 0; t_amp = 0; }
    else {
      for(int k = 0; k < ndata; k++) t_byte[k] = TICK_EDGE + (3 + k) * TICK_BYTE; // tras addr, reg y el propio byte
      t_amp = ndata ? TICK_EDGE + (2 + ndata) * TICK_BYTE + TICK_EDGE : 0;       // 2) OCR1BL = amp justo despues
    }
    sumd += t_amp; sumd2 += (double)t_amp * t_amp;
    int kb = 0;
    for(int t = 0; t < OS; t++) {
      while(kb < ndata && t >= t_byte[kb]) { chip[first + kb] = bus.stream[2 + kb]; kb++; fcur = f_of(chip, div, f0); }
      if(t == t_amp) amp = amp_next / 255.0;
      phase += 2 * M_PI * fcur / ((double)FS * OS);
      double w = 0.5 - 0.5 * cos(2 * M_PI * (n * OS + t) / (double)NFFT); // Hann
      x[n * OS + t] = std::polar(amp * w, phase);
    }
    // --- 3) ssb() de la nueva muestra de micro y freq_calc_fast para la siguiente
    long m = n + 2400;
    ab_ssb((int16_t)(a1 * sin(2 * M_PI * f1 * m / FS) + a2 * sin(2 * M_PI * f2 * m / FS)));
    amp_next = ab_amp_out;
    si5351.freq_calc_fast(ab_df_out);
    legacy::si5351.freq_calc_fast(ab_df_out);
  }
  fft(x);
  psd.assign(NFFT, 0);
  for(size_t k = 0; k < NFFT; k++) psd[k] = std::norm(x[k]);
  const double W = 25; // semiancho de integracion por tono
  double want = band(f1 - W, f1 + W) + ((a2 > 0) ? band(f2 - W, f2 + W) : 0);
  Res r;
  double d = f2 - f1;
  r.imd3 = (a2 > 0) ? dB(fmax(band(2 * f1 - f2 - W, 2 * f1 - f2 + W), band(2 * f2 - f1 - W, 2 * f2 - f1 + W)), want / 2) : -999;
  r.imd5 = (a2 > 0) ? dB(fmax(band(f1 - 2 * d - W, f1 - 2 * d + W), band(f2 + 2 * d - W, f2 + 2 * d + W)), want / 2) : -999;
  r.img  = dB(band(-3000, -100), want);                       // banda lateral no deseada
  r.adj  = dB(band(3300, 6300) + band(-6300, -3300), want);   // canal adyacente
  r.far  = dB(band(6300, 50000) + band(-50000, -6300), want); // espurios lejanos
  double mean = sumd / NS;
  r.jitter_us = sqrt(fmax(0, sumd2 / NS - mean * mean)) * 1e6 / ((double)FS * OS);
  r.bytes = (double)(bus.bytes - b0) / NS;
  return r;
}

int main() {
  static const char* const TN[] = {"ideal", "legacy", "produccion"};
  struct Case { const char* name; int32_t f; double a1, f1, a2, f2; uint8_t drv, digi; } cs[] = {
    {"dos tonos 700+1100, 14 MHz", 14200000, 40, 700, 40, 1100, 4, 0},
    {"dos tonos 700+1100, 7 MHz", 7100000, 40, 700, 40, 1100, 4, 0},
    {"dos tonos 700+1100, 28 MHz", 28400000, 40, 700, 40, 1100, 4, 0},
    {"dos tonos fuerte, 14 MHz", 14200000, 100, 700, 100, 1100, 4, 0},
    {"tono 1000 Hz, 14 MHz", 14200000, 80, 1000, 0, 0, 4, 0},
    {"digital 1500 Hz (FT8), 14 MHz", 14074000, 80, 1500, 0, 0, 4, 1},
  };
  double worst = -99;
  for(auto& c : cs) {
    printf("-- %s --\n", c.name);
    Res r[3];
    for(int t = 0; t < 3; t++) {
      r[t] = run(t, c.f, c.a1, c.f1, c.a2, c.f2, c.drv, c.digi);
      printf("   %-10s IMD3 %6.1f  IMD5 %6.1f  banda opuesta %6.1f  adyacente %6.1f  lejanos %6.1f dBc | jitter %4.1f us, %.1f bytes/muestra\n",
             TN[t], r[t].imd3, r[t].imd5, r[t].img, r[t].adj, r[t].far, r[t].jitter_us, r[t].bytes);
    }
    double d[] = {r[PROD].imd3 - r[LEGACY].imd3, r[PROD].imd5 - r[LEGACY].imd5, r[PROD].img - r[LEGACY].img,
                  r[PROD].adj - r[LEGACY].adj, r[PROD].far - r[LEGACY].far};
    for(double v : d) if(v > worst && v < 500) worst = v;
    printf("   produccion - legacy: IMD3 %+.1f  IMD5 %+.1f  opuesta %+.1f  adyacente %+.1f  lejanos %+.1f dB\n", d[0], d[1], d[2], d[3], d[4]);
  }
  printf("\npeor degradacion de produccion frente a legacy: %+.2f dB\n", worst);
  CHECK(worst <= 1.0, "T la temporizacion I2C de produccion no ensucia el espectro de TX mas que legacy (<= 1 dB)");
  return SUMMARY();
}
