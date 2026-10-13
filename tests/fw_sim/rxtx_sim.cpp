// rxtx_sim.cpp - simulacion host de switch_rxtx() REAL de produccion contra
// la de usdx-legazy (gen_rxtx.py). Las dos corren sobre el mismo estado de
// partida con registros AVR, SI5351, reles, LCD y decoder simulados; se
// compara la SECUENCIA completa de acciones sobre el hardware y el estado
// final. Es el camino critico de RF: orden PA/receptor/oscilador al conmutar.
#include "sim_common.h"
#include <stdint.h>
#include <stdlib.h>

// mini-Arduino propio (este test define su propio lcd y sus registros AVR)
static uint32_t sim_millis = 0;
static inline uint32_t millis(void) { return sim_millis; }
static inline void     noInterrupts(void) {}
static inline void     interrupts(void) {}
#define pgm_read_byte_near(p) (*(const uint8_t*)(p))
#define LOW 0
#define HIGH 1

// ---- configuracion activa (igual que produccion y que legacy) ----
#define TX_ENABLE 1
#define KEY_CLICK 1
#define _SERIAL 1
#define RIT_ENABLE 1
#define SEMI_QSK 1
#define KEYER 1
#define CW_DECODER 1
#define RX 8
#define KEY_OUT 10
#define USB 1
#define LSB 0
#define CW 2
#define FM 3
#define AM 4
#define SDR 2
#define F_CPU 20000000
#define F_SAMP_TX 4800
#define F_SAMP_RX 62500
#define SI_CLK_OE 3
#define TX1RX0 0b11111011
#define TX1RX1 0b11111000
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111
#define OCIE2A 1
#define COM1A1 7
#define COM1B1 5

// ---- todo el estado que switch_rxtx lee o escribe ----
typedef void (*func_t)(void);
void dsp_tx() {}
void dsp_tx_cw() {}
void dsp_tx_am() {}
void dsp_tx_fm() {}
void sdr_rx_00() {}
void dummy() {}
struct State {
  uint8_t  TIMSK2, TCCR1A, OCR1AL, OCR1BL, OCR2A, ADMUX, DDRC;
  uint8_t  tx, mode, practice, semi_qsk, cwdec, vox, att, cat_active, txdelay, sim_init, rx_state, filteredstate;
  int8_t   mox;
  int16_t  centiGain, _centiGain;
  int32_t  rit;
  uint32_t semi_qsk_timeout, cw_offset, ditTime;
  func_t   func_ptr;
  uint8_t  pin_rx, pin_key;
} S;
// el firmware ve registros y variables con su nombre real (referencias al estado)
static uint8_t &TIMSK2 = S.TIMSK2, &TCCR1A = S.TCCR1A, &OCR1AL = S.OCR1AL, &OCR2A = S.OCR2A, &ADMUX = S.ADMUX, &DDRC = S.DDRC;
static uint8_t &tx = S.tx, &mode = S.mode, &practice = S.practice, &semi_qsk = S.semi_qsk, &cwdec = S.cwdec, &vox = S.vox;
static uint8_t &att = S.att, &cat_active = S.cat_active, &txdelay = S.txdelay, &rx_state = S.rx_state, &filteredstate = S.filteredstate;
static uint8_t &init_ref = S.sim_init;
#define _init init_ref // _init choca con la libc del host
static int8_t   &mox = S.mox;
static int16_t  &centiGain = S.centiGain, &_centiGain = S._centiGain;
static int32_t  &rit = S.rit;
static uint32_t &semi_qsk_timeout = S.semi_qsk_timeout, &cw_offset = S.cw_offset, &ditTime = S.ditTime;
static func_t   &func_ptr = S.func_ptr;
static const uint8_t dsp_cap = SDR;
static uint8_t       admux[3] = {0x40, 0x41, 0xC2};
static uint8_t       lut[256];
static const uint8_t ramp[] = {255, 254, 252, 249, 245, 239, 233, 226, 217, 208, 198, 187, 176, 164, 152, 139,
                               127, 115, 102, 90,  78,  67,  56,  46,  37,  28,  21,  15,  9,   5,   2};

// ---- grabadora de acciones sobre el hardware, en orden ----
static char  hwlog[8192];
static void  L(const char* fmt, long a = 0, long b = 0) {
  char b2[64];
  snprintf(b2, sizeof b2, fmt, a, b);
  strncat(hwlog, b2, sizeof(hwlog) - strlen(hwlog) - 1);
}
// OCR1BL = PWM de la envolvente del PA: se graba cada escritura (rampa anti-click)
struct Ocr1b {
  Ocr1b& operator=(uint8_t v) { S.OCR1BL = v; L("PA%ld;", v); return *this; }
  operator uint8_t() const { return S.OCR1BL; }
} OCR1BL;
static void digitalWrite(uint8_t pin, uint8_t v) {
  if(pin == RX) S.pin_rx = v; else S.pin_key = v;
  L((pin == RX) ? "RXpin%ld;" : "KEYpin%ld;", v);
}
static void delayMicroseconds(unsigned us) { L("d%ldus;", us); }
struct { void setCursor(int x, int y) { L("cur%ld,%ld;", x, y); } void print(char c) { L("lcd'%c';", c); } } lcd;
struct Si {
  void freq_calc_fast(int32_t df) { L("FAST(%ld);", df); }
  void SendPLLRegisterBulk() { L("BULK;"); }
  void SendRegister(uint8_t r, uint8_t v) { L("REG%ld=%02lx;", r, v); }
} si5351;
static void dec2() { L("dec2(%ld);", S.filteredstate); }

#include "rxtx_under_test.h"

static uint32_t seed = 77;
static uint32_t rnd() { seed = seed * 1103515245u + 12345u; return seed >> 8; }
static const char* MN[] = {"LSB", "USB", "CW", "FM", "AM"};

int main() {
  for(int i = 0; i < 256; i++) lut[i] = (uint8_t)(i * 128 / 255);
  long n = 0, bad = 0;
  char first[256] = "";
  // barrido exhaustivo de la configuracion + estado previo aleatorio
  for(int rep = 0; rep < 40; rep++)
    for(int m = 0; m < 5; m++)
      for(int txe = 0; txe < 3; txe++) // 0=RX, 1=TX, 255=TX por VOX
        for(int cfg = 0; cfg < 128; cfg++) {
          State s0;
          memset(&s0, 0, sizeof s0);
          s0.mode = m; s0.practice = cfg & 1; s0.semi_qsk = (cfg >> 1) & 1; s0.cwdec = (cfg >> 2) & 1;
          s0.vox = (cfg >> 3) & 1; s0.cat_active = (cfg >> 4) & 1; s0.tx = ((cfg >> 5) & 1) ? 1 : 0;
          s0.rit = ((cfg >> 6) & 1) ? (int32_t)(rnd() % 19999) - 9999 : 0;
          s0.att = rnd() % 4; s0.mox = 0; s0.txdelay = 0;
          s0.semi_qsk_timeout = (rnd() & 1) ? 0 : 5000 + rnd() % 1000;
          s0.centiGain = rnd() % 4000; s0._centiGain = rnd() % 4000;
          s0.cw_offset = (rnd() & 1) ? 600 : 700; s0.ditTime = 60;
          s0.TIMSK2 = 2; s0.TCCR1A = rnd() & 0xff; s0.OCR1BL = (rnd() & 1) ? 0 : rnd() & 0x7f; s0.OCR1AL = rnd() & 0xff;
          s0.ADMUX = 0x40; s0.DDRC = rnd() & 0xff; s0.func_ptr = sdr_rx_00; s0.pin_rx = 1;
          sim_millis = 100000 + rnd() % 100000;
          uint8_t te = (txe == 2) ? 255 : txe;

          S = s0; hwlog[0] = 0;
          prod::switch_rxtx(te);
          State sp = S; static char lp[8192]; strcpy(lp, hwlog);
          S = s0; hwlog[0] = 0;
          legacy::switch_rxtx(te);
          n++;
          if(strcmp(lp, hwlog) || memcmp(&sp, &S, sizeof S)) {
            if(!bad) snprintf(first, sizeof first, "modo %s tx_enable=%d cfg=%d\n     prod  : %.90s\n     legacy: %.90s", MN[m], te, cfg, lp, hwlog);
            bad++;
          }
        }
  printf("   switch_rxtx: %ld conmutaciones, %ld distintas de legacy\n", n, bad);
  if(bad) printf("   primera diferencia: %s\n", first);
  CHECK(bad == 0, "X switch_rxtx identica a legacy: secuencia de hardware y estado final");

  // --- invariantes de seguridad RF sobre la funcion de produccion ---
  auto run = [](uint8_t m, uint8_t te, void (*prep)(State&)) {
    memset(&S, 0, sizeof S);
    S.mode = m; S.func_ptr = sdr_rx_00; S.pin_rx = 1; S.ditTime = 60; S.cw_offset = 600; S.TIMSK2 = 2;
    if(prep) prep(S);
    hwlog[0] = 0;
    prod::switch_rxtx(te);
  };
  #define LOGHAS(s) (strstr(hwlog, s) != NULL)
  #define BEFORE(a, b) (strstr(hwlog, a) && strstr(hwlog, b) && strstr(hwlog, a) < strstr(hwlog, b))
  run(USB, 1, 0);
  CHECK(BEFORE("RXpin0;", "REG3=fb;"), "X TX: el receptor se desconecta ANTES de habilitar el oscilador de TX");
  CHECK(S.func_ptr == dsp_tx && S.OCR2A == (F_CPU / 64) / F_SAMP_TX - 1, "X TX SSB: ISR dsp_tx a 4800 Hz");
  CHECK((S.TCCR1A & (1 << COM1B1)) && !(S.TCCR1A & (1 << COM1A1)), "X TX SSB: PWM de PA activo y sidetone apagado");
  CHECK(S.ADMUX == admux[2], "X TX: ADC en el microfono");
  run(CW, 1, [](State& s) { s.rit = 300; });
  CHECK(LOGHAS("FAST(-600);BULK;") && BEFORE("BULK;", "REG3=fb;"), "X TX CW: portadora en frecuencia (sin offset) antes de habilitar salida");
  run(USB, 1, [](State& s) { s.rit = 300; });
  CHECK(LOGHAS("FAST(0);BULK;"), "X TX con RIT: se transmite en la frecuencia del dial, no en la de RIT");
  run(USB, 1, [](State& s) { s.practice = 1; });
  CHECK(LOGHAS("REG3=ff;") && !LOGHAS("REG3=fb;") && !(S.TCCR1A & (1 << COM1B1)), "X Practice: ni oscilador de TX ni PWM de PA");
  run(USB, 0, [](State& s) { s.tx = 1; s.OCR1BL = 100; s.rit = -250; });
  {
    int ramp_n = 0;
    for(const char* p = hwlog; (p = strstr(p, "PA")); p++) ramp_n++;
    CHECK(ramp_n == 32 && LOGHAS("d60us;"), "X RX: rampa de bajada de 31 pasos x 60 us y PA a 0 (sin click)");
  }
  CHECK(BEFORE("PA0;", "REG3=fc;") && BEFORE("KEYpin0;", "REG3=fc;"), "X RX: PA a cero ANTES de conmutar el oscilador a RX");
  CHECK(S.OCR1BL == 0 && S.pin_key == 0 && !(S.TCCR1A & (1 << COM1B1)), "X RX: KEY_OUT sin PWM y a nivel bajo");
  CHECK(LOGHAS("FAST(-250);BULK;") && S.func_ptr == sdr_rx_00 && S.sim_init == 1, "X RX: frecuencia de RX con RIT, DSP rearmado");
  CHECK(S.OCR2A == (F_CPU / 64) / F_SAMP_RX - 1 && (S.TIMSK2 & (1 << OCIE2A)), "X RX: ISR de vuelta a 62,5 kHz y habilitada");
  run(USB, 0, [](State& s) { s.att = 2; });
  CHECK(S.pin_rx == 0, "X RX con ATT -20 dB: la linea RX queda baja");
  run(CW, 0, [](State& s) { s.semi_qsk = 1; s.tx = 1; s.pin_rx = 0; }); // venimos de TX: linea RX baja
  CHECK(S.func_ptr == dummy && S.semi_qsk_timeout == sim_millis + 60 * 8 && S.pin_rx == 0,
        "X Semi-QSK: RX muda (ISR parada, linea RX baja) 8 dits tras soltar la llave");
  return SUMMARY();
}
