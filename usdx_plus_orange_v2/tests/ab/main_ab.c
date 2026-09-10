// main_ab.c - Fase 0: métricas A/B (no paridad) para comparar mejoras.
//  TX: dos tonos por ssb() -> IMD3 dBc, RMS/peak df, % clips.
//  RX: tono USB 800 Hz -> respuesta; tono imagen -800 -> rechazo; silencio ->
//      piso de ruido. Todo con la cadena v2 (tx_v2.c + rx_v2.c de parity).
// Compilar vía run_ab.sh. Frecuencias coherentes con N (sin leakage).
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- TU TX polar (ab_tx.c generado por gen_ab.py: tx=1, LUT lineal) ---
extern void    ab_tx_init(void);
extern void    ab_ssb(int16_t in);
extern int16_t ab_df_out;
extern uint8_t ab_amp_out;
extern volatile uint8_t drive;

// --- TU RX (parity_rx/rx_v2.c) ---
int16_t iq_i[40000];
int16_t iq_q[40000];
int     iq_i_idx = 0;
int     iq_q_idx = 0;
extern void    rx_init_v2(void);
extern void    rx_cfg_v2(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
extern void    rx_run_v2(int);
extern int16_t rx_last_audio_v2(void);

// Goertzel en una frecuencia (Hz), fs dada, N muestras desde gen()
static double goertzel(double f, double fs, double (*gen)(int), int n0, int n1) {
  double w = 2.0 * M_PI * f / fs, c = cos(w), s = sin(w);
  double u0 = 0, u1 = 0;
  for(int n = n0; n < n1; n++) {
    double x = gen(n);
    double u = x + 2 * c * u0 - u1;
    u1 = u0;
    u0 = u;
  }
  double re = u0 - c * u1, im = s * u1;
  return sqrt(re * re + im * im) / (n1 - n0);
}

// ---------------- TX: IMD dos tonos (SSB polar reconstruida) ----------------
// df = dp*(Fs/UA) -> dp = df/8 (pasos de 1/600 de vuelta); envolvente = amp.
// s[n] = amp * e^{j*phase} es la SSB en banda base (portadora en 0).
// Métrica real de calidad TX: portadoras USB (+700/+1100), imagen (-700/-1100),
// IMD3 (+300 = 2f1-f2, +1500 = 2f2-f1), fuga portadora (0). Todo dBc.
#define TXN 4800 // 1 s @ 4800 SPS; todo Hz entero es coherente
static double tx_re[TXN], tx_im[TXN];

static void tx_run(double a1, double a2) {
  double phase = 0;
  for(int n = 0; n < TXN; n++) {
    double m = a1 * sin(2 * M_PI * 700.0 * n / 4800.0) +
               a2 * sin(2 * M_PI * 1100.0 * n / 4800.0);
    ab_ssb((int16_t)m);
    double dp = (double)ab_df_out / 8.0; // pasos _UA
    phase += dp * (2.0 * M_PI / 600.0);
    double a = (double)ab_amp_out / 255.0;
    tx_re[n] = a * cos(phase);
    tx_im[n] = a * sin(phase);
  }
}
// Goertzel complejo en f (signo incluido: f<0 = banda imagen).
// X = u[N-1] - e^{-jw} u[N-2] con u compleja (recurrencia separada re/im).
static double cgoertzel(double f, int n0, int n1) {
  double w = 2.0 * M_PI * f / 4800.0, c = cos(w), s = sin(w);
  double u0r = 0, u1r = 0, u0i = 0, u1i = 0;
  for(int n = n0; n < n1; n++) {
    double ur = tx_re[n] + 2 * c * u0r - u1r;
    u1r = u0r;
    u0r = ur;
    double ui = tx_im[n] + 2 * c * u0i - u1i;
    u1i = u0i;
    u0i = ui;
  }
  double re = u0r - c * u1r - s * u1i;
  double im = u0i - c * u1i + s * u1r;
  return sqrt(re * re + im * im) / (n1 - n0);
}

static void tx_imd(const char* tag, double amp) {
  tx_run(amp, amp);
  int    skip = 480;
  double c1   = cgoertzel(700.0, skip, TXN);
  double c2   = cgoertzel(1100.0, skip, TXN);
  double img  = cgoertzel(-700.0, skip, TXN) > cgoertzel(-1100.0, skip, TXN)
                    ? cgoertzel(-700.0, skip, TXN)
                    : cgoertzel(-1100.0, skip, TXN);
  double imd = cgoertzel(300.0, skip, TXN) > cgoertzel(1500.0, skip, TXN)
                   ? cgoertzel(300.0, skip, TXN)
                   : cgoertzel(1500.0, skip, TXN);
  double car  = cgoertzel(0.0, skip, TXN);
  double carr = (c1 + c2) / 2.0;
  printf("TX %-14s mic=%5.0f usb=%6.3f img=%6.1fdBc imd3=%6.1fdBc car=%6.1fdBc\n",
         tag, amp, carr, 20 * log10(img / (carr + 1e-12)),
         20 * log10(imd / (carr + 1e-12)), 20 * log10(car / (carr + 1e-12)));
}

// ---------------- RX: respuesta / imagen / ruido ----------------
#define RXN 30000
static int16_t rx_audio[RXN];

static void rx_fill(double f, double am, double nz) {
  unsigned s = 12345;
  for(int i = 0; i < RXN; i++) {
    double t   = (double)i / 31250.0; // misma base que parity_rx
    double env = 1.0 + 0.4 * sin(2 * M_PI * t * 1.3);
    double nzv = 0;
    if(nz > 0) { // LCG determinista
      s        = s * 1103515245 + 12345;
      nzv      = nz * ((int)(s >> 16) % 2000 - 1000) / 1000.0;
    }
    iq_i[i] = 511 + (int16_t)(am * env * sin(2 * M_PI * f * t) + nzv);
    iq_q[i] = 511 + (int16_t)(am * env * cos(2 * M_PI * f * t) + nzv);
  }
}
static void rx_run(int mode, int agc, int vol, int nr, int filt, int att2) {
  rx_init_v2();
  rx_cfg_v2(mode, agc, vol, nr, filt, att2, 1);
  iq_i_idx = iq_q_idx = 0;
  for(int i = 0; i < RXN; i++) {
    rx_run_v2(1);
    rx_audio[i] = rx_last_audio_v2();
  }
}
// Cada medida RX corre en un fork (estado DSP virgen): el arrastre de estado
// entre estímulos distintos falsea el piso/transitorios (ver Fase 0).
static void isolate(void (*fn)(void)) {
  fflush(stdout);
  pid_t p = fork();
  if(p == 0) {
    fn();
    fflush(stdout);
    _exit(0);
  }
  int st;
  waitpid(p, &st, 0);
}

static double rx_rms(int n0) { // RMS AC (sin DC del PWM centrado en 128)
  double mean = 0;
  for(int i = n0; i < RXN; i++)
    mean += rx_audio[i];
  mean /= (RXN - n0);
  double s = 0;
  for(int i = n0; i < RXN; i++) {
    double d = rx_audio[i] - mean;
    s += d * d;
  }
  return sqrt(s / (RXN - n0));
}

static double rx_gen_aud(int n) { return (double)rx_audio[n]; }
// Goertzel real sobre el audio demodulado (fs = 1 muestra/rx_run).
static double rx_goertzel(double f, double fs, int n0, int n1) {
  double w = 2.0 * M_PI * f / fs, c = cos(w), s = sin(w);
  double u0 = 0, u1 = 0;
  for(int n = n0; n < n1; n++) {
    double u = rx_gen_aud(n) + 2 * c * u0 - u1;
    u1 = u0;
    u0 = u;
  }
  double re = u0 - c * u1, im = s * u1;
  return sqrt(re * re + im * im) / (n1 - n0);
}
// Medida RX aislada: tono +800 con envolvente (como parity), estado virgen.
// Barrrido espectral 200..3200 para hallar el pico (sin asumir tasa exacta),
// RMS, y armónicos 2º/3º del pico = limpieza de demodulación.
static int   m_filt;
static void  m_rx_tone(void) {
  rx_fill(800.0, 1100.0, 0.0);
  rx_run(1, 0, 12, 0, m_filt, 2);
  int    skip = 8000;
  double rms  = rx_rms(skip);
  double best = 0, bf = 0;
  for(double f = 200; f <= 3200; f += 100) {
    double m = rx_goertzel(f, 31250.0, skip, RXN);
    if(m > best) {
      best = m;
      bf   = f;
    }
  }
  double h2 = rx_goertzel(2 * bf, 31250.0, skip, RXN);
  double h3 = rx_goertzel(3 * bf, 31250.0, skip, RXN);
  printf("RX filt=%d pico=%5.0fHz rms=%7.1f hd2=%6.1fdBc hd3=%6.1fdBc\n", m_filt,
         bf, rms, 20 * log10(h2 / (best + 1e-9)), 20 * log10(h3 / (best + 1e-9)));
}
static void m_rx_floor(void) {
  rx_fill(0.0, 0.0, 0.0);
  rx_run(1, 0, 12, 0, 0, 2);
  printf("RX piso=%6.1f\n", rx_rms(8000));
}

int main(void) {
  ab_tx_init();
  printf("== AB TX (dos tonos 700+1100, SSB reconstruida) ==\n");
  for(int d = 2; d <= 6; d += 2) {
    char tag[16];
    drive = (uint8_t)d;
    snprintf(tag, sizeof(tag), "drive=%d", d);
    tx_imd(tag, 150.0);
  }
  tx_imd("bajo", 60.0);
  tx_imd("alto", 300.0);

  printf("== AB RX (USB, agc=0, vol=12, att2=2, nr=0, estado virgen) ==\n");
  isolate(m_rx_floor);
  for(int f = 0; f <= 3; f++) {
    m_filt = f;
    isolate(m_rx_tone);
  }
  return 0;
}
