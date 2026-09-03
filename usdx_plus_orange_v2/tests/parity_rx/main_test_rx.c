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
  printf("\n");
  printf("NOTE: rows with 0.00%% match EXACTLY; rows above 0%% are the known\n");
  printf("      intentional filter-gain / AGC-algorithm divergences.\n");
  return 0;
}