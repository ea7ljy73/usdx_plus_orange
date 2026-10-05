// main_test_tx.c - Parity TX: feed same mic signal, compare ssb() df output
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void legacy_ssb(int16_t in);
extern void v2_ssb(int16_t in);

int main(void) {
  const int N = 2000;
  int mismatch = 0, total = 0, first = 0;
  double rms_leg = 0, rms_v2 = 0, max_leg = 0, max_v2 = 0;
  double sum_absdiff = 0, max_absdiff = 0;

  /* need the RETURN of ssb; expose via a small wrapper hack: re-call returns
     are lost because ssb() returns int16. Instead, compare the SIDE EFFECT on
     OCR1BL by feeding through... simpler: run each TU's ssb in its own loop and
     compare the sequence via a global sink.
     We'll capture df by making ssb return into a volatile in each TU. */
  /* fallback: drive many samples and verify both produce non-zero, same shape
     by fingerprint (sum of |df|). */
  long sumL = 0, sumV = 0, nzL = 0, nzV = 0, sumL2 = 0, sumV2 = 0;
  for (int n = 0; n < N; n++) {
    double env = 1.0 + 0.6 * sin(2 * M_PI * n * 0.7 / N);
    double m = env * (170.0 * sin(2 * M_PI * n * 12.0 / N) +
                      90.0 * sin(2 * M_PI * n * 29.0 / N));
    int16_t mic = (int16_t)m;
    legacy_ssb(mic);
    v2_ssb(mic);
    /* capture df via extern from each TU */
    extern int16_t legacy_df_out, v2_df_out;
    int l = legacy_df_out, v = v2_df_out;
    total++;
    if (l != v) {
      mismatch++;
      if (first++ < 5)
        printf("n=%d L=%d V=%d diff=%d\n", n, l, v, l - v);
    }
    sumL += abs(l);
    sumV += abs(v);
    sumL2 += (long)l * l;
    sumV2 += (long)v * v;
    if (l)
      nzL++;
    if (v)
      nzV++;
    double ad = l < v ? (double)(v - l) : (double)(l - v);
    sum_absdiff += ad;
    if (ad > max_absdiff)
      max_absdiff = ad;
    if (abs(l) > max_leg)
      max_leg = abs(l);
    if (abs(v) > max_v2)
      max_v2 = abs(v);
  }
  rms_leg = sqrt((double)sumL2 / total);
  rms_v2 = sqrt((double)sumV2 / total);
  printf("\n== TX PARITY (ssb df) usdx-legazy vs v2 ==\n");
  printf("samples: %d\n", total);
  printf("mismatches: %d (%.1f%%)\n", mismatch, 100.0 * mismatch / total);
  printf("sum|df| legacy=%ld v2=%ld\n", sumL, sumV);
  printf("rms legacy=%.1f v2=%.1f\n", rms_leg, rms_v2);
  printf("peak legacy=%.1f v2=%.1f\n", max_leg, max_v2);
  printf("avg|diff|=%.3f max|diff|=%.1f\n", sum_absdiff / total, max_absdiff);
  if (mismatch == 0)
    printf("=> TX PARIDAD EXACTA\n");
  else if (max_absdiff <= 2)
    printf("=> TX PARIDAD FUNCIONAL (+/-1..2 LSB)\n");
  else
    printf("=> TX DIFERENCIAS\n");
  return mismatch ? 1 : 0;
}