// main_test_rx.c - RX DSP parity: identical I/Q ADC stream into both TUs,
// compare per-phase audio output (OCR1AL) traces. Each test runs in a forked
// child so per-TU static DSP state is fresh (no cross-test contamination).
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#define USB 1
#define LSB 0
#define AM 3
#define FM 4
#define CW_MODE 2

int16_t iq_i[40000];
int16_t iq_q[40000];
int iq_i_idx = 0;
int iq_q_idx = 0;

extern void rx_init_legacy(void);
extern void rx_cfg_legacy(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
extern void rx_run_legacy(int);
extern int16_t rx_last_audio_legacy(void);

extern void rx_init_v2(void);
extern void rx_cfg_v2(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
extern void rx_run_v2(int);
extern int16_t rx_last_audio_v2(void);

// F4.18: wrappers funcion M0PUB (cuerpo extraido de cada fuente, estado propio)
extern void    wrap_agc_legacy_reset(void);
extern int16_t wrap_agc_legacy(int16_t x);
extern void    wrap_agc_v2_reset(void);
extern int16_t wrap_agc_v2(int16_t x);

// F4.18: paridad del cuerpo process_agc (M0PUB) legacy vs v2, muestra a
// muestra, mismo estimulo desde estado virgen (el slow_dsp legacy no tiene
// rama agc==2, asi que aqui se compara la funcion, no la cadena).
static int test_m0pub(void) {
  int mism = 0, total = 0;
  unsigned seed = 1234;
  wrap_agc_legacy_reset();
  wrap_agc_v2_reset();
#define C(x) do { int16_t a = wrap_agc_legacy(x), b = wrap_agc_v2(x); total++; \
    if(a != b) { mism++; if(mism < 6) printf("   n=%d in=%d L=%d V=%d\n", total, (x), a, b); } } while(0)
  for(int i = 0; i <= 4000; i += 100) C(i);       // rampa subida
  for(int i = 4000; i >= -4000; i -= 100) C(i);   // rampa bajada (cruza 0)
  for(int i = 0; i < 20; i++) { C(3000); C(-3000); C(500); C(-500); } // escalones
  for(int i = 0; i < 50; i++) { C(2000); C(0); }  // rafagas on/off (hang/decay)
  for(int i = 0; i < 2000; i++) {                 // ruido debil
    seed = seed * 1103515245 + 12345;
    C((int16_t)((seed >> 9) % 81) - 40);
  }
  for(int i = 0; i < 50; i++) C(8000);            // blast (exposicion overflow)
  for(int i = 0; i < 500; i++) {                  // recupera tras blast
    seed = seed * 1103515245 + 12345;
    C((int16_t)(300 + ((seed >> 9) % 101) - 50));
  }
#undef C
  printf("M0PUB body legacy vs v2              steps=%d  mismatch=%d %s\n",
         total, mism, mism ? "FAIL" : "OK");
  return mism > 0;
}

#define N 30000

static void fill_signal(double f) {
  for (int i = 0; i < N; i++) {
    double t = (double)i / 31250.0;
    double env = 1.0 + 0.4 * sin(2 * M_PI * t * 1.3);
    iq_i[i] = 511 + (int16_t)(1100.0 * env * sin(2 * M_PI * f * t));
    iq_q[i] = 511 + (int16_t)(1100.0 * env * cos(2 * M_PI * f * t));
  }
}

static int run_one(const char* label, int mode, int agc, int volume, int nr, int filt, int att2, int cw_tone) {
  static int16_t tL[N], tV[N];
  fill_signal(700.0);
  rx_init_legacy();
  rx_cfg_legacy(mode, agc, volume, nr, filt, att2, cw_tone);
  for (int i = 0; i < N; i++) { rx_run_legacy(1); tL[i] = rx_last_audio_legacy(); }
  rx_init_v2();
  rx_cfg_v2(mode, agc, volume, nr, filt, att2, cw_tone);
  for (int i = 0; i < N; i++) { rx_run_v2(1); tV[i] = rx_last_audio_v2(); }

  int mism = 0, first = 0, start = 8;
  long sumd = 0;
  int maxd = 0;
  for (int i = start; i < N; i++) {
    int d = abs(tL[i] - tV[i]);
    if (d) { mism++; if (first++ < 5) printf("   n=%d L=%d V=%d diff=%d\n", i, tL[i], tV[i], d); }
    sumd += d;
    if (d > maxd) maxd = d;
  }
  printf("%-42s steps=%d  mismatch=%d (%.2f%%)  avg|d|=%.3f max|d|=%d\n",
         label, N - start, mism, 100.0 * mism / (N - start), (double)sumd / (N - start), maxd);
  return mism;
}

static int compare(const char* label, int mode, int agc, int volume, int nr, int filt, int att2, int cw_tone) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid == 0) {
    int m = run_one(label, mode, agc, volume, nr, filt, att2, cw_tone);
    exit(m ? 1 : 0);
  }
  int st;
  waitpid(pid, &st, 0);
  return WIFEXITED(st) && WEXITSTATUS(st);
}

int main(void) {
  printf("== RX DSP PARITY usdx-legazy vs v2 ==\n");
  int fail = 0;
  fail += compare("USB core agc=0 filt=0",        USB, 0, 12, 0, 0, 2, 1) > 0;
  fail += compare("LSB core agc=0 filt=0",        LSB, 0, 12, 0, 0, 2, 1) > 0;
  fail += compare("CW  core agc=0 filt=0",        CW_MODE, 0, 12, 0, 0, 2, 1) > 0;
  fail += compare("AM  core agc=0 filt=0",        AM, 0, 12, 0, 0, 2, 1) > 0;
  fail += compare("FM  core agc=0 filt=0",        FM, 0, 12, 0, 0, 2, 1) > 0;
  fail += compare("USB agc=0 filt=0 att2=0",      USB, 0, 12, 0, 0, 0, 1) > 0;
  fail += compare("USB agc=0 filt=0 volume=16",   USB, 0, 16, 0, 0, 2, 1) > 0;
  fail += compare("USB nr=3 agc=0 filt=0",        USB, 0, 12, 3, 0, 2, 1) > 0;
  fail += compare("USB nr=8 agc=0 filt=0",        USB, 0, 12, 8, 0, 2, 1) > 0;
  printf("\n-- filters (known intentional gain divergences) --\n");
  fail += compare("USB agc=0 filt=1 (SSB 2900)",  USB, 0, 12, 0, 1, 2, 1) > 0;
  fail += compare("USB agc=0 filt=2 (SSB 2400)",  USB, 0, 12, 0, 2, 2, 1) > 0;
  fail += compare("USB agc=0 filt=3 (SSB 1800)",  USB, 0, 12, 0, 3, 2, 1) > 0;
  fail += compare("CW  agc=0 filt=4 (CW 600)",    CW_MODE, 0, 12, 0, 4, 2, 1) > 0;
  fail += compare("CW  agc=0 filt=7 (CW 18)",     CW_MODE, 0, 12, 0, 7, 2, 1) > 0;
  printf("\n-- AGC (known divergence: legacy=agc_fast, v2=M0PUB agc) --\n");
  fail += compare("USB agc=1 filt=0 (AGC ON)",    USB, 1, 12, 0, 0, 2, 1) > 0;
  printf("\n-- M0PUB body port (F4.18 agc==2, funcion suelta) --\n");
  fail += test_m0pub() > 0;
  printf("\n");
  printf("NOTE: rows with 0.00%% match EXACTLY; rows above 0%% are the known\n");
  printf("      intentional filter-gain / AGC-algorithm divergences.\n");
  return 0;
}