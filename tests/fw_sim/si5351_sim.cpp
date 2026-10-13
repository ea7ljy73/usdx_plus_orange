// si5351_sim.cpp - simulacion host del si5351.h REAL de produccion contra la
// clase SI5351 REAL de usdx-legazy (gen_si5351_legacy.py). Los dos escriben
// en un modelo de registros del chip: lo que se compara es el ESTADO del
// SI5351 (lo que de verdad sale por RF), no solo los bytes del bus.
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

static uint32_t seed = 2026;
static uint32_t rnd() { seed = seed * 1103515245u + 12345u; return seed >> 8; }
static bool pll_equal() { return !memcmp(i2c.reg + 26, legacy::i2c.reg + 26, 8); }
static bool chip_equal() { return !memcmp(i2c.reg, legacy::i2c.reg, 256); }
static void both_freq(int32_t f, uint16_t i, uint16_t q) { si5351.freq(f, i, q); legacy::si5351.freq(f, i, q); }
static void both_fast(int32_t df) {
  si5351.freq_calc_fast(df); si5351.SendPLLRegisterBulk();
  legacy::si5351.freq_calc_fast((int16_t)df); legacy::si5351.SendPLLRegisterBulk();
}
// df "de voz": paseo aleatorio con silencios (df repetido) y saltos de fase grandes
static int32_t speech_df(long n) {
  static int32_t d = 0;
  uint32_t r = rnd();
  if((n / 700) % 3 == 2) return d;            // silencio / portadora estable
  if(r % 23 == 0) d = (int32_t)(rnd() % 9600) - 4800; // salto (cruce por cero de envolvente)
  else d += (int32_t)(r % 401) - 200;
  if(d > 4800) d = 4800;
  if(d < -4800) d = -4800;
  return d;
}
static const int32_t BANDS[] = {1840000, 3573000, 5357000, 7074000, 10136000, 14074000, 18100000, 21074000,
                                24915000, 28074000, 29600000, 50313000};

int main() {
  // --- S1: freq() completo (sintonia): mismo chip y misma cache que legacy ---
  {
    long n = 0, bad = 0, badc = 0;
    for(unsigned b = 0; b < 12; b++)
      for(int k = 0; k < 400; k++) {
        int32_t f = BANDS[b] + (int32_t)(rnd() % 200000) - 100000 + ((k % 3 == 2) ? 600 : 0);
        uint16_t i = (k & 1) ? 90 : 0, q = (k & 1) ? 0 : 90; // LSB/CW vs USB
        both_freq(f, i, q);
        n++;
        bad += !chip_equal();
        badc += (si5351._div != legacy::si5351._div) || (si5351._msb128 != legacy::si5351._msb128) ||
                (si5351._msa128min512 != legacy::si5351._msa128min512) || (si5351.iqmsa != legacy::si5351.iqmsa);
      }
    printf("   freq(): %ld sintonias, %ld chips distintos, %ld caches distintas\n", n, bad, badc);
    CHECK(bad == 0, "S1 freq(): registros del SI5351 identicos a legacy en todas las bandas");
    CHECK(badc == 0, "S1 freq(): cache (_div, _msb128, _msa128min512, iqmsa) identica");
  }

  // --- S2: freq_calc_fast sin division de 64 bit == formula de legacy ---
  {
    long n = 0, bad = 0;
    for(unsigned b = 0; b < 12; b++) {
      both_freq(BANDS[b], 0, 90);
      for(int32_t df = -10000; df <= 10000; df += 7) {
        si5351.freq_calc_fast(df);
        legacy::si5351.freq_calc_fast((int16_t)df);
        n++;
        bad += memcmp((const void*)(si5351.pll_regs + 4), (const void*)(legacy::si5351.pll_regs + 4), 4) != 0;
      }
    }
    for(uint32_t fx = 26990000; fx <= 27010000; fx += 2500) { // "Ref freq" calibrado por el usuario
      si5351.fxtal = legacy::si5351.fxtal = fx;
      both_freq(14074000, 0, 90);
      for(int k = 0; k < 4000; k++) {
        int32_t df = (int32_t)(rnd() % 20001) - 10000;
        si5351.freq_calc_fast(df);
        legacy::si5351.freq_calc_fast((int16_t)df);
        n++;
        bad += memcmp((const void*)(si5351.pll_regs + 4), (const void*)(legacy::si5351.pll_regs + 4), 4) != 0;
      }
    }
    si5351.fxtal = legacy::si5351.fxtal = F_XTAL;
    printf("   freq_calc_fast: %ld casos, %ld distintos\n", n, bad);
    CHECK(bad == 0, "S2 freq_calc_fast bit-exacto con legacy (df +-10 kHz, 12 bandas, fxtal calibrado)");
  }

  // --- S3: TX muestra a muestra: bulk diferencial deja el chip igual que el completo ---
  {
    long n = 0, bad = 0;
    for(unsigned b = 0; b < 12; b++) {
      both_freq(BANDS[b], 0, 90);
      for(long k = 0; k < 20000; k++) {
        both_fast(speech_df(k));
        n++;
        bad += !pll_equal();
      }
    }
    printf("   TX: %ld muestras, %ld estados PLL distintos; bytes I2C produccion %u vs legacy %u (%.0f %%)\n", n, bad,
           i2c.bytes, legacy::i2c.bytes, 100.0 * i2c.bytes / legacy::i2c.bytes);
    CHECK(bad == 0, "S3 TX: tras CADA muestra el PLL del chip es identico al de legacy");
    CHECK(i2c.bytes == legacy::i2c.bytes, "S3 mismo trafico I2C que legacy (bulk completo: temporizacion constante");
  }

  // --- S4: operacion real mezclada: sintonia, RIT, CW, OE, cambios de modo ---
  {
    long n = 0, bad = 0;
    both_freq(7074000, 0, 90);
    for(long k = 0; k < 200000; k++) {
      uint32_t r = rnd() % 1000;
      if(r < 3) both_freq(BANDS[rnd() % 12] + (int32_t)(rnd() % 50000), (rnd() & 1) ? 90 : 0, (rnd() & 1) ? 90 : 0);
      else if(r < 6) { si5351.SendRegister(SI_CLK_OE, 0xFB); legacy::si5351.SendRegister(SI_CLK_OE, 0xFB); } // TX1RX0
      else if(r < 9) { si5351.SendRegister(SI_CLK_OE, 0xFC); legacy::si5351.SendRegister(SI_CLK_OE, 0xFC); } // TX0RX1
      else if(r < 12) both_fast(-600);                       // CW: TX en portadora
      else if(r < 15) both_fast((int32_t)(rnd() % 19999) - 9999); // RIT
      else if(r < 18) both_fast(0);                          // vuelta a RX
      else both_fast(speech_df(k));
      n++;
      bad += !chip_equal();
    }
    printf("   mezcla: %ld operaciones, %ld chips distintos\n", n, bad);
    CHECK(bad == 0, "S4 sintonia+RIT+CW+TX/RX mezclados: chip identico a legacy siempre");
  }

  // --- S5: robustez ante una transaccion I2C perdida (glitch en el bus) ---
  // legacy reescribe los 4 registros cada muestra: se cura solo en 1 muestra.
  // El bulk diferencial solo reescribe lo que cambia: mide cuanto tarda.
  {
    static I2CRec ref; // chip de referencia sin fallos (legacy completo)
    long worst_voice = 0, worst_const = 0, stuck_const = 0, leg_worst = 0;
    for(int trial = 0; trial < 300; trial++) {
      bool constant = trial & 1;
      both_freq(BANDS[trial % 12], 0, 90);
      for(long k = 0; k < 50; k++) both_fast(speech_df(k));
      i2c.drop = legacy::i2c.drop = true; // se pierde la siguiente escritura
      int32_t d0 = (int32_t)(rnd() % 6000) - 3000;
      long heal_p = -1, heal_l = -1;
      for(long k = 0; k < 4800; k++) { // 1 s de TX
        int32_t df = constant ? d0 : speech_df(k);
        both_fast(df);
        // estado correcto = lo que legacy::pll_regs pide ahora mismo
        uint8_t want[4];
        memcpy(want, (const void*)(legacy::si5351.pll_regs + 4), 4);
        if(heal_l < 0 && !memcmp(legacy::i2c.reg + 30, want, 4)) heal_l = k;
        if(heal_p < 0 && !memcmp(i2c.reg + 30, want, 4)) heal_p = k;
        if(heal_p >= 0 && heal_l >= 0) break;
      }
      if(heal_l > leg_worst) leg_worst = heal_l;
      if(constant) { if(heal_p < 0) stuck_const++; else if(heal_p > worst_const) worst_const = heal_p; }
      else if(heal_p < 0 || heal_p > worst_voice) worst_voice = (heal_p < 0) ? 4800 : heal_p;
    }
    printf("   fallo I2C: legacy se cura en <=%ld muestras; produccion: voz <=%ld muestras, portadora fija: %ld de 150 NO se curan en 1 s\n",
           leg_worst, worst_voice, stuck_const);
    CHECK(leg_worst <= 1, "S5 legacy se recupera de una escritura perdida en 1 muestra");
    CHECK(stuck_const == 0 && worst_const <= 1 && worst_voice <= 1,
          "S5 produccion se recupera igual que legacy: en 1 muestra");
  }
  return SUMMARY();
}
