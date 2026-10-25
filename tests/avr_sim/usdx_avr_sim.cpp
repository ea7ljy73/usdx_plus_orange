// usdx_avr_sim.cpp - simulador de la radio completa: ATmega328P (simavr)
// ejecutando el .elf REAL del firmware a 20 MHz, con los perifericos del uSDX
// modelados alrededor:
//   * SI5351: decodifica el I2C por bit-bang (PC4/PC5 en colector abierto) y
//     mantiene sus registros; de ellos sale la frecuencia de cada instante.
//   * PCA9555 (banco de reles LPF): mismas lineas I2C, direccion 0x20.
//   * LCD HD44780 (PD0-3, EN=PD4, RS=PC4): se reconstruye la pantalla.
//   * Microfono en ADC2, I/Q en ADC0/ADC1, PTT en PB5 (DIT), botones PC3.
//   * PA: envolvente = PWM de OC1B (OCR1BL/ICR1) mientras CLK2 esta activo.
// Mide la duracion de CADA interrupcion TIMER2_COMPA (RX y TX) con precision
// de ciclo y reconstruye la senal RF transmitida para sacar su espectro.
#include <algorithm>
#include <complex>
#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>
extern "C" {
#include "avr_adc.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "sim_avr.h"
#include "sim_cycle_timers.h"
#include "sim_elf.h"
}

#define F_XTAL_MCU 20000000.0 // cristal real del uSDX+
#define F_XTAL_SI 27000000.0
static avr_t* avr;
static double now_s() { return avr->cycle / F_XTAL_MCU; }

// ------------------------------------------------------------------ opciones
static double opt_boot = 6.0, opt_tx = 2.4, opt_tail = 0.4;
static double opt_a1 = 32, opt_f1 = 700, opt_a2 = 32, opt_f2 = 1100;
static int    opt_log2n = 20; // --win: 2^n muestras de RF analizadas (20 = 0,84 s; 22 = 3,4 s)
static const char *opt_aud = NULL, *opt_eesave = NULL, *opt_eeload = NULL;
static std::vector<std::pair<int, int>> ee_set;
struct TPoke { double t; std::string name; int v; int done; };
static std::vector<TPoke> tpokes;
static int    opt_voice = 0, opt_quiet = 0, opt_study = 0, opt_dump = 0, opt_tone = 0;
static double opt_expect = 0; // --expect-imd3: IMD3 maxima admitida con el RC del esquema
static double opt_rc_us = 100.0; // celula RC del conformador: 10 kohm x 10 nF (esquema usdx.png), dos en cascada

// ------------------------------------------------------------------ SI5351 + PCA9555 por I2C bit-bang
static uint8_t si[256], siptr;
static uint16_t ioexp_out;
static uint8_t  ioexp_ptr;
struct Ev { uint64_t cyc; uint8_t kind, reg, val; }; // kind 0 = registro SI5351, 1 = OCR1BL, 2 = TCCR1A
static std::vector<Ev> evs;
static long i2c_bytes, i2c_xfers, si_writes, lpf_writes;
static struct { int in, nbit, nbyte; uint8_t sh, addr; } bus;
static char lpf_log[256];

static void i2c_byte(uint8_t b) {
  i2c_bytes++;
  if(bus.nbyte == 0) { bus.addr = b; }
  else if(bus.addr == 0xC0) { // SI5351 (0x60, escritura)
    if(bus.nbyte == 1) siptr = b;
    else { si[siptr] = b; evs.push_back({avr->cycle, 0, siptr, b}); siptr++; si_writes++; }
  } else if(bus.addr == 0x40) { // PCA9555 (0x20)
    if(bus.nbyte == 1) ioexp_ptr = b;
    else {
      if(ioexp_ptr == 2) ioexp_out = (ioexp_out & 0xff00) | b;
      if(ioexp_ptr == 3) ioexp_out = (ioexp_out & 0x00ff) | (b << 8);
      ioexp_ptr++; lpf_writes++;
    }
  }
  bus.nbyte++;
}
static void i2c_lines(uint8_t prev, uint8_t cur) { // DDRC: bit4 = SDA baja, bit5 = SCL baja
  int sda0 = !(prev & 0x10), scl0 = !(prev & 0x20), sda1 = !(cur & 0x10), scl1 = !(cur & 0x20);
  if(scl0 && scl1 && sda0 != sda1) { // SDA cambia con SCL alta
    if(!sda1) { bus.in = 1; bus.nbit = 0; bus.nbyte = 0; bus.sh = 0; i2c_xfers++; } // START
    else bus.in = 0;                                                                // STOP
  }
  if(bus.in && !scl0 && scl1) { // flanco de subida de SCL: bit valido
    if(bus.nbit < 8) { bus.sh = (bus.sh << 1) | sda1; bus.nbit++; }
    else { i2c_byte(bus.sh); bus.nbit = 0; bus.sh = 0; } // 9o pulso = ACK
  }
}
static double si_pll(const uint8_t* r) { // fvco de PLLA
  double p1 = ((r[28] & 3) << 16) | (r[29] << 8) | r[30], p2 = ((r[31] & 15) << 16) | (r[32] << 8) | r[33];
  double p3 = ((r[31] >> 4) << 16) | (r[26] << 8) | r[27];
  return F_XTAL_SI * (p1 + 512 + (p3 ? p2 / p3 : 0)) / 128.0;
}
static double si_clk(const uint8_t* r, int ms) { // frecuencia de salida de CLK<ms>
  const uint8_t* m = r + 42 + 8 * ms;
  double p1 = ((m[2] & 3) << 16) | (m[3] << 8) | m[4], p2 = ((m[5] & 15) << 16) | (m[6] << 8) | m[7];
  double p3 = ((m[5] >> 4) << 16) | (m[0] << 8) | m[1];
  double div = (p1 + 512 + (p3 ? p2 / p3 : 0)) / 128.0;
  return (div > 0) ? si_pll(r) / div / (1 << ((m[2] >> 4) & 7)) : 0;
}

static int  adcj_on; static long lcd_nibbles; // medida de puntualidad activa / nibbles escritos al LCD
// ------------------------------------------------------------------ LCD HD44780 (4 bit)
static char lcd[2][17];
static int  lcd_addr, lcd_cg, lcd_hi = -1;
static void lcd_byte(int rs, uint8_t b) {
  if(!rs) {
    if(b == 0x01) { memset(lcd[0], ' ', 16); memset(lcd[1], ' ', 16); lcd_addr = 0; lcd_cg = 0; }
    else if(b & 0x80) { lcd_addr = b & 0x7f; lcd_cg = 0; }
    else if(b & 0x40) lcd_cg = 1;
  } else if(!lcd_cg) {
    int row = (lcd_addr >= 0x40), col = lcd_addr & 0x3f;
    if(col < 16) lcd[row][col] = (b >= 32 && b < 127) ? b : (b < 8 ? "#*_.:|AB"[b] : '?');
    lcd_addr++;
  }
}
static void lcd_en_fall(uint8_t portd, uint8_t ddrc) {
  int rs = !(ddrc & 0x10), nib = portd & 0x0f;
  if(adcj_on) lcd_nibbles++;
  if(lcd_hi < 0) lcd_hi = nib; else { lcd_byte(rs, (lcd_hi << 4) | nib); lcd_hi = -1; }
}
static void lcd_show(const char* tag) {
  if(!opt_quiet) printf("   LCD %-22s |%.16s|\n   %26s |%.16s|\n", tag, lcd[0], "", lcd[1]);
}

// ------------------------------------------------------------------ ADC: microfono e I/Q
static uint32_t vseed = 99;
static double mic_counts(double t) {
  if(opt_voice == 2) { // habla sintetica: silabas de nivel desigual con pausas, pulsos gloticos (120 Hz) y formantes
    static const double lev[8] = {1.0, 0.7, 0.45, 1.0, 0.25, 0.0, 0.6, 0.12};
    static const double fm[4][3] = {{700, 1200, 2500}, {350, 2000, 2700}, {500, 900, 2400}, {450, 1700, 2500}};
    double ts = t * 4.5; long k = (long)ts; double u = ts - k;
    uint32_t hsh = (uint32_t)k * 2654435761u; const double* F = fm[(hsh >> 20) & 3];
    double e = lev[(hsh >> 27) & 7] * sin(M_PI * u), f0 = 120 * (1 + 0.08 * sin(2 * M_PI * 0.9 * t)), s = 0;
    for(int h = 1; h * f0 < 2300; h++) { double f = h * f0, w = 0;
      for(int j = 0; j < 3; j++) { double d = (f - F[j]) / (60 + 40 * j); w += (j ? 0.5 : 1.0) / (1 + d * d); }
      s += (w + 0.03) * cos(2 * M_PI * f * t); }
    vseed = vseed * 1103515245u + 12345u;
    return opt_a1 * e * s * 0.9 + (int)((vseed >> 16) % 3) - 1;
  }
  if(opt_voice) {
    double env = 0.55 + 0.45 * sin(2 * M_PI * 3.1 * t) * sin(2 * M_PI * 0.7 * t);
    vseed = vseed * 1103515245u + 12345u;
    return opt_a1 * env * (sin(2 * M_PI * 310 * t) + 0.6 * sin(2 * M_PI * 870 * t + 1) + 0.4 * sin(2 * M_PI * 1950 * t + 2)) +
           (int)((vseed >> 16) % 5) - 2;
  }
  return opt_a1 * sin(2 * M_PI * opt_f1 * t) + opt_a2 * sin(2 * M_PI * opt_f2 * t);
}
static int mic_on;
static int scenario = 0; // 0 = emision SSB, 1 = recepcion, 2 = CW, 3 = rxt, 4 = CAT
// escenario 4 (cat): tramas CAT por la UART REAL a 38400 baudios (byte a byte, al
// ritmo de la linea: si el firmware tiene RXEN apagado, el byte se pierde como en
// la placa). Cada paso: "CMD;" [ "=" respuesta esperada ] { "@" variable "=" valor };
// en la respuesta '#' = un caracter cualquiera, '*' = el resto.
struct CatStep { std::string cmd, exp; int has_exp; std::vector<std::pair<std::string, int>> vars; };
static std::vector<CatStep> cat_steps;
static std::string          uart_out;
static double               cat_step_s = 0.25; // --cat-step: segundos entre comandos
static void uart_tx_cb(struct avr_irq_t*, uint32_t v, void*) { uart_out += (char)v; }
static bool cat_glob(const char* p, const char* s) {
  for(; *p; p++, s++) {
    if(*p == '*') return true;
    if(!*s || (*p != '#' && *p != *s)) return false;
  }
  return !*s;
}
struct Stim { const char* name; double f, a, f2, a2; int swap; const char* poke; int pv; };
static Stim stims[] = {
  {"tono +1000 Hz (AGC)", 1000, 60, 0, 0, 0, NULL, 0},
  {"tono -1000 Hz (AGC)", 1000, 60, 0, 0, 1, NULL, 0},
  {"sin senal (solo ruido ADC)", 0, 0, 0, 0, 0, NULL, 0},
  {"tono debil, 3 cuentas", 1000, 3, 0, 0, -1, NULL, 0},
  {"tono fuerte, 400 cuentas", 1000, 400, 0, 0, -1, NULL, 0},
  {"sin AGC:  300 Hz", 300, 20, 0, 0, -1, "agc", 0},
  {"sin AGC: 1000 Hz", 1000, 20, 0, 0, -1, NULL, 0},
  {"sin AGC: 2000 Hz", 2000, 20, 0, 0, -1, NULL, 0},
  {"sin AGC: 2800 Hz", 2800, 20, 0, 0, -1, NULL, 0},
  {"sin AGC: 3600 Hz", 3600, 20, 0, 0, -1, NULL, 0},
  {"sin AGC: dos tonos 900+1300", 900, 20, 1300, 20, -1, NULL, 0},
};
static const int NSTIM = sizeof stims / sizeof stims[0];
static int    stim_i = -1, rx_side = 0; // rx_side: signo de Q que da la banda deseada
static int    btn_mv = 0; // tension en la linea de botones (0 = sueltos)
// ---- ADC propio, fiel al ATmega328P (el de simavr calcula el valor al LEER
// y con el canal ya conmutado; el firmware arranca la conversion siguiente y
// lee la anterior). Aqui: se muestrea el canal seleccionado al poner ADSC, el
// resultado aparece en ADCL/ADCH al terminar (13 ciclos de reloj ADC) junto
// con ADIF, ADSC se borra solo, y escribir un 1 en ADIF lo borra (por eso el
// "ADCSRA |= ADSC" del firmware limpia la bandera, como en el chip real).
// escenario 3 (rxt): linea de tiempo libre desde el arranque, para medir dinamica (AGC, NR)
static uint32_t nseed = 7;
struct Seg { double dur, f, a, noise; };
static std::vector<Seg> seq; static double seq_side = -1;
static double gauss() { double s = 0; for(int i = 0; i < 4; i++) { nseed = nseed * 1103515245u + 12345u; s += ((int)((nseed >> 16) % 2001) - 1000) / 1000.0; } return s * 0.866; } // ~N(0,1)
static void sig_iq(double t, double* vi, double* vq) {
  if(scenario == 3) {
    double t0 = 0; const Seg* s = seq.empty() ? NULL : &seq.back();
    for(auto& g : seq) { if(t < t0 + g.dur) { s = &g; break; } t0 += g.dur; }
    double a = s ? s->a : 0, f = s ? s->f : 1000, nz = s ? s->noise : 0;
    *vi = a * cos(2 * M_PI * f * t) + nz * gauss(); *vq = seq_side * a * sin(2 * M_PI * f * t) + nz * gauss();
    return;
  }
  *vi = 60 * sin(2 * M_PI * 1000 * t); *vq = 60 * cos(2 * M_PI * 1000 * t);
  if(scenario == 1 && stim_i >= 0) { // I/Q en cuadratura + ~0,5 LSB de ruido del ADC
    const Stim& s = stims[stim_i];
    double sg = (s.swap < 0) ? rx_side : (s.swap ? -1 : 1);
    nseed = nseed * 1103515245u + 12345u; double n1 = ((int)((nseed >> 16) % 2001) - 1000) / 1000.0;
    nseed = nseed * 1103515245u + 12345u; double n2 = ((int)((nseed >> 16) % 2001) - 1000) / 1000.0;
    *vi = s.a * cos(2 * M_PI * s.f * t) + s.a2 * cos(2 * M_PI * s.f2 * t) + n1;
    *vq = sg * (s.a * sin(2 * M_PI * s.f * t) + s.a2 * sin(2 * M_PI * s.f2 * t)) + n2;
  }
}
static int      adc_busy;
static long     adc_skipped;
// puntualidad del muestreo I/Q: desviacion del intervalo entre arranques respecto a 320 ciclos
static uint64_t adc_last; static long adcj_n, adcj_late8, adcj_late32, adcj_late100; static double adcj_sq; static long adcj_max;
 // ADSC escrito con una conversion aun en curso (el chip lo ignora)
static uint16_t adc_sample;
static long     adc_convs;
static uint16_t adc_counts(int ch, int refs, double t) {
  // Entradas polarizadas a AREF/2 (como en la placa); la senal se define en
  // cuentas con la referencia de 1,1 V y encoge con la de 5 V (atenuador).
  double vref = (refs == 3) ? 1.1 : 5.0, k = 1.1 / vref, v = 0, vi, vq;
  switch(ch) {
  case 0: sig_iq(t, &vi, &vq); v = 512 + vi * k; break;
  case 1: sig_iq(t, &vi, &vq); v = 512 + vq * k; break;
  case 2: v = 512 + (mic_on ? mic_counts(t) : 0) * k; break;
  case 3: v = btn_mv / (vref * 1000.0) * 1024; break; // escalera de botones (tension absoluta)
  case 14: v = 1.1 / vref * 1024; break;               // bandgap
  default: v = 0;
  }
  long r = lrint(v);
  return (uint16_t)(r < 0 ? 0 : r > 1023 ? 1023 : r);
}
static avr_cycle_count_t adc_done(avr_t* a, avr_cycle_count_t, void*) {
  a->data[0x78] = adc_sample & 0xff;
  a->data[0x79] = adc_sample >> 8;
  a->data[0x7a] = (a->data[0x7a] & ~0x40) | 0x10; // ADSC=0, ADIF=1
  adc_busy = 0;
  return 0;
}
static void adcsra_write(avr_t* a, avr_io_addr_t, uint8_t v, void*) {
  uint8_t nv = (v & ~0x10) | ((v & 0x10) ? 0 : (a->data[0x7a] & 0x10)); // escribir 1 en ADIF lo borra
  if(!(v & 0x80)) { adc_busy = 0; avr_cycle_timer_cancel(a, adc_done, NULL); nv &= ~0x40; }
  else if((v & 0x40) && !adc_busy) {
    adc_busy = 1; adc_convs++;
    if(adcj_on && (a->data[0x7c] & 0x0f) < 2) {
      long d = (long)(a->cycle - adc_last) - 320; if(d < 0) d = -d;
      if(adc_last && d < 5000) { adcj_n++; adcj_sq += (double)d * d; if(d > adcj_max) adcj_max = d; adcj_late8 += d > 8; adcj_late32 += d > 32; adcj_late100 += d > 100; }
      adc_last = a->cycle;
    }
    adc_sample = adc_counts(a->data[0x7c] & 0x0f, a->data[0x7c] >> 6, now_s());
    int ps = v & 7; if(ps < 1) ps = 1;
    avr_cycle_timer_register(a, 13 * (1 << ps), adc_done, NULL);
  }
  else if((v & 0x40) && adc_busy) adc_skipped++;
  if(adc_busy) nv |= 0x40;
  a->data[0x7a] = nv;
}
static void adc_install() {
  avr->io[0x7a - 32].w.c = adcsra_write; avr->io[0x7a - 32].w.param = NULL;
  avr->io[0x78 - 32].r.c = NULL; avr->io[0x79 - 32].r.c = NULL; avr->io[0x7a - 32].r.c = NULL; // lecturas directas de RAM
}
static void pin(char port, int bit, int v) { // nivel externo: manda sobre el pull-up interno
  avr_ioport_external_t e = {(unsigned long)port, (unsigned long)(1 << bit), (unsigned long)(v ? (1 << bit) : 0)};
  avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL(port), &e);
  avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(port), bit), v);
}

// ------------------------------------------------------------------ estadistica de la ISR
// La ISR de RX vuelve a habilitar interrupciones para el DSP lento, asi que
// se anida: se lleva una pila de entradas. "Muestras perdidas" = ticks del
// timer que no llegaron a entrar (esperadas por tiempo - entradas reales).
struct IsrStat {
  long n = 0, maxdepth = 0, over = 0; uint64_t sum = 0, mx = 0, mn = ~0ull, c_first = 0, c_last = 0, busy = 0; double period = 0;
  void add(uint64_t entry, uint64_t dur, double per, int depth) {
    period = per;
    if(!n) c_first = entry;
    c_last = entry; n++; sum += dur;
    if(dur > mx) mx = dur;
    if(dur < mn) mn = dur;
    if(depth > maxdepth) maxdepth = depth;
    if(dur > per) over++;
  }
  double expected() const { return n ? (c_last - c_first) / period + 1 : 0; }
  long   lost() const { return (long)lrint(expected() - n); }
};
static void isr_report(const char* name, IsrStat& s) {
  if(!s.n) { printf("   %s: sin interrupciones\n", name); return; }
  double span = (double)(s.c_last - s.c_first);
  printf("   %s: periodo %.0f ciclos = %.1f Hz; %ld entradas en %.3f s (esperadas %.0f) -> muestras perdidas %ld (%.2f %%)\n", name, s.period,
         F_XTAL_MCU / s.period, s.n, span / F_XTAL_MCU, s.expected(), s.lost(), 100.0 * s.lost() / s.expected());
  printf("      duracion de la ISR: min %llu  media %.0f  MAX %llu ciclos (periodo %.0f) -> pico %.0f %% del periodo; anidamiento max %ld\n",
         (unsigned long long)s.mn, (double)s.sum / s.n, (unsigned long long)s.mx, s.period, 100.0 * s.mx / s.period, s.maxdepth);
  printf("      CPU dentro de la ISR: %.1f %%  (queda %.1f %% para el bucle principal); ISR mas largas que un periodo: %.2f %%\n",
         100.0 * s.busy / span, 100 - 100.0 * s.busy / span, 100.0 * s.over / s.n);
}

// ------------------------------------------------------------------ perfilador (simbolos de avr-nm -n)
struct Sym { uint32_t addr; char name[64]; uint64_t cyc; };
static std::vector<Sym> syms, dsyms; // codigo y variables (RAM)
static std::vector<std::pair<std::string, int>> pokes;
static void load_syms(const char* path) {
  FILE* f = fopen(path, "r");
  if(!f) return;
  char line[512], type; unsigned addr; char name[400];
  while(fgets(line, sizeof line, f)) {
    if(sscanf(line, "%x %c %399s", &addr, &type, name) == 3 && strchr("BbDd", type) && addr >= 0x800100) {
      Sym s; s.addr = addr - 0x800000; s.cyc = 0; snprintf(s.name, sizeof s.name, "%.63s", name); dsyms.push_back(s);
    }
    if(sscanf(line, "%x %c %399s", &addr, &type, name) == 3 && (type == 'T' || type == 't' || type == 'W') && addr < 0x8000) {
      Sym s; s.addr = addr; s.cyc = 0; snprintf(s.name, sizeof s.name, "%.63s", name);
      if(!strncmp(name, "__stack", 7) || !strncmp(name, "__heap", 6) || !strncmp(name, "__data", 6) || !strncmp(name, "__bss", 5)) continue;
      if(!syms.empty() && syms.back().addr == addr) continue;
      syms.push_back(s);
    }
  }
  fclose(f);
}
static Sym* sym_at(uint32_t pc) {
  size_t lo = 0, hi = syms.size();
  while(lo + 1 < hi) { size_t m = (lo + hi) / 2; if(syms[m].addr <= pc) lo = m; else hi = m; }
  return syms.empty() ? NULL : &syms[lo];
}

static int poke_var(const char* name, int v) {
  for(auto& d : dsyms) if(!strcmp(d.name, name)) { avr->data[d.addr] = (uint8_t)v; return 1; }
  return 0;
}
static int peek_var(const char* name) {
  for(auto& d : dsyms) if(!strcmp(d.name, name)) return avr->data[d.addr];
  return -1;
}
struct AEv { uint64_t cyc; uint8_t val; };
static std::vector<AEv> aud; // escrituras de OCR1AL (PWM de audio / sidetone)

// ------------------------------------------------------------------ FFT
typedef std::complex<double> cd;
static void fft(std::vector<cd>& a) {
  const size_t n = a.size();
  for(size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for(; j & bit; bit >>= 1) j ^= bit; j ^= bit; if(i < j) std::swap(a[i], a[j]); }
  for(size_t len = 2; len <= n; len <<= 1) {
    double ang = -2 * M_PI / len; cd wl(cos(ang), sin(ang));
    for(size_t i = 0; i < n; i += len) { cd w(1); for(size_t j = 0; j < len / 2; j++) { cd u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
  }
}

// audio de salida: OCR1AL retenido, remuestreado a 62,5 kHz, 2^16 muestras (1,05 s)
struct Aud { double fpk, tone, total, thd, sinad, pp; };
static Aud audio_an(uint64_t c0, double f_expect) {
  const size_t N = 1 << 16; const int STEP = 320; const double fs = F_XTAL_MCU / STEP;
  std::vector<cd> x(N);
  size_t i = 0; uint8_t v = 128;
  while(i < aud.size() && aud[i].cyc < c0) v = aud[i++].val;
  // La rejilla de remuestreo se coloca lo mas lejos posible del instante en que
  // el firmware escribe OCR1AL. Si coincide, el jitter de la ISR hace que unas
  // muestras cojan el valor anterior y otras el nuevo: ruido de medida que no
  // existe en el audio (aparecia segun el desfase de arranque, +3 dB de suelo).
  { long h[STEP] = {0};
    for(size_t j = i; j < aud.size() && aud[j].cyc < c0 + (uint64_t)N * STEP; j++) h[aud[j].cyc % STEP]++;
    long best = -1; int ph = 0;
    for(int p = 0; p < STEP; p++) { long w = 0; for(int d = -40; d <= 40; d++) w += h[(p + d + STEP) % STEP]; if(best < 0 || w < best) { best = w; ph = p; } }
    c0 += (ph + STEP - (int)(c0 % STEP)) % STEP;
    while(i < aud.size() && aud[i].cyc < c0) v = aud[i++].val; }
  double mean = 0, mn = 255, mx = 0; std::vector<double> s(N);
  for(size_t n = 0; n < N; n++) { uint64_t c = c0 + n * STEP; while(i < aud.size() && aud[i].cyc <= c) v = aud[i++].val; s[n] = v; mean += v; if(v < mn) mn = v; if(v > mx) mx = v; }
  mean /= N;
  for(size_t n = 0; n < N; n++) x[n] = (s[n] - mean) * (0.5 - 0.5 * cos(2 * M_PI * n / (double)N));
  fft(x);
  const double dfb = fs / N, norm = 1.0 / (N * N * 0.375 / 2); // potencia -> cuentas^2 (ventana Hann)
  auto band = [&](double f0, double f1) { double p = 0; for(long k = (long)(f0 / dfb); k <= (long)(f1 / dfb) && k < (long)N / 2; k++) if(k > 0) p += std::norm(x[k]); return p * norm; };
  Aud r; r.pp = mx - mn;
  double best = 0; r.fpk = 0;
  for(long k = (long)(100 / dfb); k < (long)(3900 / dfb); k++) if(std::norm(x[k]) > best) { best = std::norm(x[k]); r.fpk = k * dfb; }
  double f0 = f_expect > 0 ? f_expect : r.fpk, W = 20;
  r.tone = band(f0 - W, f0 + W);
  r.total = band(100, 3800);
  r.thd = (band(2 * f0 - W, 2 * f0 + W) + band(3 * f0 - W, 3 * f0 + W)) / (r.tone + 1e-12);
  r.sinad = r.tone / (r.total - r.tone + 1e-12);
  return r;
}
// RF: reproduce los eventos (registros SI5351, OCR1B, TCCR1A) con el PWM real y el
// conformador RC (rc_us = 0: PWM directo). Devuelve tambien la envolvente filtrada.
static void rf_build(uint64_t c0, size_t N, double rc_us, double f_ref, std::vector<cd>& y, std::vector<float>* env) {
  const int STEP = 16; const double fs = F_XTAL_MCU / STEP;
  uint8_t rr[256]; memset(rr, 0, 256);
  uint8_t o = 0, tcc = 0; size_t e = 0;
  for(; e < evs.size() && evs[e].cyc < c0; e++) { if(evs[e].kind == 0) rr[evs[e].reg] = evs[e].val; else if(evs[e].kind == 1) o = evs[e].val; else tcc = evs[e].val; }
  y.assign(N, 0); if(env) env->assign(N, 0);
  double ph = 0, ff = si_clk(rr, 2) - f_ref, v1 = 0, v2 = 0;
  const double k = rc_us > 0 ? (STEP / F_XTAL_MCU) / (rc_us * 1e-6) : 0;
  for(size_t n = 0; n < N; n++) {
    uint64_t c = c0 + n * STEP; int chg = 0;
    for(; e < evs.size() && evs[e].cyc <= c; e++) { if(evs[e].kind == 0) { rr[evs[e].reg] = evs[e].val; chg = 1; } else if(evs[e].kind == 1) o = evs[e].val; else tcc = evs[e].val; }
    if(chg) ff = si_clk(rr, 2) - f_ref;
    int cnt = (int)(c & 255), hi = 0;
    for(int j = 0; j < STEP; j++) hi += ((cnt + j) & 255) < o;
    double vin = ((tcc & 0x20) && !(rr[3] & 4)) ? hi / (double)STEP : 0;
    if(k > 0) { v1 += k * (vin - 2 * v1 + v2); v2 += k * (v1 - v2); } else v2 = vin;
    ph += 2 * M_PI * ff / fs;
    y[n] = std::polar(v2, ph);
    if(env) (*env)[n] = (float)v2;
  }
}

int main(int argc, char** argv) {
  const char* elf = NULL;
  for(int i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "--voice")) opt_voice = 1;
    else if(!strcmp(argv[i], "--speech")) opt_voice = 2;
    else if(!strcmp(argv[i], "--win") && i + 1 < argc) opt_log2n = atoi(argv[++i]);
    else if(!strcmp(argv[i], "--seq") && i + 1 < argc) { // "dur:f:a:ruido;..." (s, Hz, cuentas de pico, cuentas rms) desde el arranque
      for(char* tk = strtok(argv[++i], ";"); tk; tk = strtok(NULL, ";")) { Seg g = {0, 1000, 0, 0}; sscanf(tk, "%lf:%lf:%lf:%lf", &g.dur, &g.f, &g.a, &g.noise); seq.push_back(g); } }
    else if(!strcmp(argv[i], "--side") && i + 1 < argc) seq_side = atof(argv[++i]);
    else if(!strcmp(argv[i], "--aud") && i + 1 < argc) opt_aud = argv[++i];
    else if(!strcmp(argv[i], "--ee-save") && i + 1 < argc) opt_eesave = argv[++i];
    else if(!strcmp(argv[i], "--ee-load") && i + 1 < argc) opt_eeload = argv[++i];
    else if(!strcmp(argv[i], "--ee") && i + 1 < argc) { unsigned a = 0, v = 0; if(sscanf(argv[++i], "%i=%i", (int*)&a, (int*)&v) == 2) ee_set.push_back({(int)a, (int)v}); }
    else if(!strcmp(argv[i], "--poke-at") && i + 2 < argc) { double tt = atof(argv[++i]); char* eq = strchr(argv[++i], '='); if(eq) { *eq = 0; tpokes.push_back({tt, argv[i], atoi(eq + 1), 0}); } }
    else if(!strcmp(argv[i], "--quiet")) opt_quiet = 1;
    else if(!strcmp(argv[i], "--study")) opt_study = 1;
    else if(!strcmp(argv[i], "--dump-state")) opt_dump = 1;
    else if(!strcmp(argv[i], "--scenario") && i + 1 < argc) { i++; scenario = !strcmp(argv[i], "rx") ? 1 : !strcmp(argv[i], "cw") ? 2 : !strcmp(argv[i], "rxt") ? 3 : !strcmp(argv[i], "cat") ? 4 : 0; }
    else if(!strcmp(argv[i], "--cat-step") && i + 1 < argc) cat_step_s = atof(argv[++i]);
    else if(!strcmp(argv[i], "--cat") && i + 1 < argc) { // pasos separados por '|'
      std::string all = argv[++i];
      for(size_t a = 0; a < all.size();) {
        size_t b = all.find('|', a); if(b == std::string::npos) b = all.size();
        std::string t = all.substr(a, b - a); a = b + 1;
        CatStep st; st.has_exp = 0;
        size_t sc = t.find(';'); st.cmd = t.substr(0, sc + 1); t = t.substr(sc + 1);
        size_t at = t.find('@');
        if(!t.empty() && t[0] == '=') { st.exp = t.substr(1, at == std::string::npos ? std::string::npos : at - 1); st.has_exp = 1; }
        while(at != std::string::npos) {
          size_t nx = t.find('@', at + 1), eq = t.find('=', at);
          st.vars.push_back({t.substr(at + 1, eq - at - 1), atoi(t.c_str() + eq + 1)});
          at = nx;
        }
        cat_steps.push_back(st);
      }
    }
    else if(!strcmp(argv[i], "--expect-imd3") && i + 1 < argc) { opt_expect = atof(argv[++i]); opt_study = 1; }
    else if(!strcmp(argv[i], "--rc") && i + 1 < argc) opt_rc_us = atof(argv[++i]);
    else if(!strcmp(argv[i], "--mic") && i + 1 < argc) opt_a1 = opt_a2 = atof(argv[++i]);
    else if(!strcmp(argv[i], "--tone") && i + 2 < argc) { opt_f1 = atof(argv[++i]); opt_a1 = atof(argv[++i]); opt_a2 = 0; opt_tone = 1; } // un solo tono: Hz y cuentas de ADC
    else if(!strcmp(argv[i], "--tx") && i + 1 < argc) opt_tx = atof(argv[++i]);
    else if(!strcmp(argv[i], "--boot") && i + 1 < argc) opt_boot = atof(argv[++i]); // segundos de arranque antes del estimulo
    else if(!strcmp(argv[i], "--syms") && i + 1 < argc) load_syms(argv[++i]);
    else if(!strcmp(argv[i], "--poke") && i + 1 < argc) { // variable=valor (1 byte), aplicado al pulsar PTT
      char* eq = strchr(argv[++i], '=');
      if(eq) { *eq = 0; pokes.push_back({argv[i], atoi(eq + 1)}); }
    }
    else elf = argv[i];
  }
  if(!elf) { fprintf(stderr, "uso: usdx_avr_sim firmware.elf [--mic N] [--voice] [--tx seg]\n"); return 2; }
  elf_firmware_t fw;
  memset(&fw, 0, sizeof fw);
  if(elf_read_firmware(elf, &fw)) { fprintf(stderr, "no puedo leer %s\n", elf); return 2; }
  avr = avr_make_mcu_by_name("atmega328p");
  avr_init(avr);
  avr_load_firmware(avr, &fw);
  avr->frequency = (uint32_t)F_XTAL_MCU;
  avr->vcc = avr->avcc = avr->aref = 5000;
  avr->log = LOG_ERROR;
  memset(lcd, ' ', sizeof lcd);
  adc_install();
  pin('B', 5, 1); pin('B', 4, 1); // DIT/PTT y DAH sueltos (pull-up)
  pin('C', 3, 0);                 // botones sueltos
  pin('D', 6, 1); pin('D', 7, 1); // encoder en reposo
  avr_irq_t* uart_in = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
  if(scenario == 4) {
    uint32_t fl = 0;
    avr_ioctl(avr, AVR_IOCTL_UART_GET_FLAGS('0'), &fl);
    fl &= ~AVR_UART_FLAG_STDIO; // la salida va al comprobador, no a la consola
    avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &fl);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), uart_tx_cb, NULL);
  }

  const double   STIM_S = 2.3; // por estimulo: 1,2 s de asentamiento + 1,05 s de medida
  if(scenario == 1) { opt_tx = NSTIM * STIM_S; opt_tail = 0; }
  if(scenario == 2) { opt_tx = 5.2; opt_tail = 0.2; }
  if(scenario == 3) { opt_boot = 0.001; opt_tx = 0; for(auto& g : seq) opt_tx += g.dur; opt_tail = 0; }
  const double CAT_STEP_S = cat_step_s; // por defecto 250 ms (el cliente garantiza >= 120 ms)
  if(scenario == 4) { opt_boot = 3.0; opt_tx = cat_steps.size() * CAT_STEP_S + 0.01; opt_tail = 0; }
  int      cat_tx_seen = 0; // CAT: estadistica de la ISR de RX solo antes del primer TX
  int      cat_k = -1, cat_fail = 0, cat_lost = 0, rx_on = 1; uint64_t rx_off0 = 0, rx_off_sum = 0, rx_off_max = 0; long rx_off_n = 0; size_t cat_mark = 0, cat_byte = 0; uint64_t cat_next = 0;
  const uint64_t cat_step_cyc = (uint64_t)(CAT_STEP_S * F_XTAL_MCU), cat_byte_cyc = (uint64_t)(10.0 / 38400 * F_XTAL_MCU);
  auto cat_check = [&](int k) {
    const CatStep& st = cat_steps[k];
    std::string r = uart_out.substr(cat_mark);
    int ok = !st.has_exp || cat_glob(st.exp.c_str(), r.c_str());
    std::string bad;
    for(auto& v : st.vars) { int x = peek_var(v.first.c_str()); if(x != (v.second & 0xff)) { ok = 0; bad += " " + v.first + "=" + std::to_string(x); } }
    if(!ok) for(auto& d : dsyms) if(!strcmp(d.name, "CATcmd")) { // lo que el firmware tiene en el buffer
      char cb[41]; for(int j = 0; j < 40; j++) { uint8_t c = avr->data[d.addr + j]; cb[j] = (c >= 32 && c < 127) ? c : '.'; } cb[40] = 0;
      bad += std::string(" CATcmd=[") + cb + "] cat_ptr=" + std::to_string(peek_var("cat_ptr"));
    }
    printf("   CAT %-20s -> %-40s %s%s\n", st.cmd.c_str(), r.empty() ? "(sin respuesta)" : r.c_str(), ok ? "ok" : "FALLO", bad.c_str());
    cat_fail += !ok;
  };
  if(opt_eeload) { // EEPROM de una ejecucion anterior (+ cambios --ee dir=valor): arranque con ajustes de menu reales
    static uint8_t eb[1024]; memset(eb, 0xff, sizeof eb);
    FILE* f = fopen(opt_eeload, "rb"); if(f) { if(fread(eb, 1, sizeof eb, f) != sizeof eb) printf("   (EEPROM incompleta)\n"); fclose(f); } else printf("   (no se pudo leer %s)\n", opt_eeload);
    for(auto& e : ee_set) eb[e.first & 1023] = (uint8_t)e.second;
    avr_eeprom_desc_t d = {eb, 0, sizeof eb}; avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
  }
  const uint64_t c_ptt = (uint64_t)(opt_boot * F_XTAL_MCU), c_rel = c_ptt + (uint64_t)(opt_tx * F_XTAL_MCU),
                 c_end = c_rel + (uint64_t)(opt_tail * F_XTAL_MCU);
  uint8_t  ocr1a = 0;
  char     lcd_stim[32][2][17]; memset(lcd_stim, ' ', sizeof lcd_stim);
  // CW: pulsaciones del boton de modo hasta ver "CW" en el LCD, luego manipulacion
  int      cw_clicks = 0, cw_ready = 0; uint64_t cw_next = c_ptt, cw_key0 = 0;
  auto key_down = [&](uint64_t c) { // 6 puntos de 48 ms (25 wpm) y una raya de 300 ms
    if(!cw_key0 || c < cw_key0) return 0;
    double ms = (c - cw_key0) * 1e3 / F_XTAL_MCU;
    if(ms < 576) return (int)(fmod(ms, 96) < 48);
    return (int)(ms >= 700 && ms < 1000);
  };
  int key_state = 0;
  IsrStat st_rx, st_tx;
  uint8_t  ddrc = avr->data[0x27], portd = avr->data[0x2b], ocr1b = avr->data[0x8a], tccr1a = avr->data[0x80];
  int      stage = 0, depth = 0, dumped = 0;
  struct { uint64_t c0; uint16_t sp; } stk[32];
  uint64_t t_tx_on = 0, t_tx_off = 0, busy_from = 0;
  const uint64_t c_rx0 = c_ptt - (uint64_t)(1.0 * F_XTAL_MCU); // ventana RX estable: ultimo segundo antes de la PTT
  uint64_t i2c_t0 = 0, i2c_mx = 0, i2c_sum = 0; long i2c_n = 0; int was_in = 0;
  double   f_dial = 0;
  uint8_t  si_rx[256];
  while(avr->cycle < c_end) {
    if(avr->pc == 0x1c && depth < 31) {
      if(!depth) busy_from = avr->cycle;
      stk[depth].c0 = avr->cycle; stk[depth].sp = avr->data[0x5d] | (avr->data[0x5e] << 8); depth++;
    }
    Sym*     psym = (depth && t_tx_on && !t_tx_off && !syms.empty()) ? sym_at(avr->pc) : NULL;
    uint64_t pc0 = avr->cycle;
    int state = avr_run(avr);
    if(psym) psym->cyc += avr->cycle - pc0;
    if(state == cpu_Done || state == cpu_Crashed) { printf("   CPU parada (estado %d) en t=%.3f s\n", state, now_s()); break; }
    if(depth && (uint16_t)(avr->data[0x5d] | (avr->data[0x5e] << 8)) == (uint16_t)(stk[depth - 1].sp + 2)) {
      depth--;
      double per = 64.0 * (avr->data[0xb3] + 1); // OCR2A, prescaler 64
      int    txi = scenario == 0 && t_tx_on && !t_tx_off && avr->cycle < c_rel, rxi = (stage == 0 && stk[depth].c0 >= c_rx0) || (scenario == 1 && stage == 9) || (scenario == 4 && stage == 9 && !cat_tx_seen);
      IsrStat* st = txi ? &st_tx : rxi ? &st_rx : NULL;
      if(st) { st->add(stk[depth].c0, avr->cycle - stk[depth].c0, per, depth + 1); if(!depth) st->busy += avr->cycle - busy_from; }
    }
    uint8_t d = avr->data[0x27];
    if(d != ddrc) {
      i2c_lines(ddrc, d); ddrc = d;
      if(bus.in && !(d & 0x20)) { if(!i2c_t0) i2c_t0 = avr->cycle; } // SCL baja: empieza a transmitir
      if(bus.in && !was_in) i2c_t0 = 0;
      if(!bus.in && was_in && bus.nbyte >= 2 && i2c_t0 && t_tx_on && !t_tx_off) { uint64_t t = avr->cycle - i2c_t0; i2c_n++; i2c_sum += t; if(t > i2c_mx) i2c_mx = t; }
      was_in = bus.in;
    }
    uint8_t p = avr->data[0x2b];
    if(p != portd) { if((portd & 0x10) && !(p & 0x10)) lcd_en_fall(portd, ddrc); portd = p; }
    uint8_t o = avr->data[0x8a];
    if(o != ocr1b) { ocr1b = o; evs.push_back({avr->cycle, 1, 0, o}); }
    uint8_t oa = avr->data[0x88];
    if(oa != ocr1a) { ocr1a = oa; aud.push_back({avr->cycle, oa}); }
    if(opt_dump && avr->cycle >= c_ptt && !dumped && depth == 0) { dumped = 1; // --dump-state: registros y variables para comparar con legacy
      static const struct { const char* n; int a; } regs[] = {{"PINB",0x23},{"DDRB",0x24},{"PORTB",0x25},{"DDRC",0x27},{"PORTC",0x28},{"DDRD",0x2a},{"PORTD",0x2b},
        {"TIFR0",0x35},{"TCCR0A",0x44},{"TCCR0B",0x45},{"OCR0A",0x47},{"OCR0B",0x48},{"SMCR",0x53},{"MCUCR",0x55},{"WDTCSR",0x60},{"PRR",0x64},{"PCICR",0x68},{"EICRA",0x69},{"PCMSK0",0x6b},{"PCMSK1",0x6c},{"PCMSK2",0x6d},
        {"TIMSK0",0x6e},{"TIMSK1",0x6f},{"TIMSK2",0x70},{"ADCSRA",0x7a},{"ADCSRB",0x7b},{"ADMUX",0x7c},{"DIDR0",0x7e},{"DIDR1",0x7f},{"TCCR1A",0x80},{"TCCR1B",0x81},{"TCCR1C",0x82},{"ICR1L",0x86},{"ICR1H",0x87},{"OCR1AH",0x89},{"OCR1BL",0x8a},
        {"TCCR2A",0xb0},{"TCCR2B",0xb1},{"OCR2A",0xb3},{"OCR2B",0xb4},{"ASSR",0xb6},{"UCSR0A",0xc0},{"UCSR0B",0xc1},{"UCSR0C",0xc2},{"UBRR0L",0xc4},{"UBRR0H",0xc5},{"ACSR",0x50}};
      for(auto& r : regs) printf("REG %-7s %02x\n", r.n, avr->data[r.a] & (strcmp(r.n,"ADCSRA") ? 0xff : 0xaf));
      static const char* vars[] = {"mode","volume","agc","nr","att","att2","filt","smode","rx_ph_q","cwdec","semi_qsk","vox","vox_thresh","drive","keyer_mode","keyer_speed","keyer_swap","practice","rit","bandval","stepsize","vfosel","cw_tone","backlight","pwm_min","pwm_max","tx","mox","_init","rx_state","tc","inv","cat_active","txdelay","menumode","quad"};
      for(auto v : vars) { int x = peek_var(v); if(x >= 0) printf("VAR %-11s %d\n", v, x); else printf("VAR %-11s (no existe)\n", v); }
      for(auto n : {"cw_offset","centiGain","gain","si5351"}) for(auto& d : dsyms) if(!strcmp(d.name, n)) printf("VAR16 %-9s %d\n", n, avr->data[d.addr] | (avr->data[d.addr + 1] << 8));
      for(int i = 0; i < 3; i++) for(auto& d : dsyms) if(!strcmp(d.name, "admux")) printf("VAR admux[%d]   %02x\n", i, avr->data[d.addr + i]);
      printf("SI5351 regs 3,16-18,24,26-33,42-49,50-57,58-65,165-167,177,183,187:");
      for(int r : {3,16,17,18,24,26,27,28,29,30,31,32,33,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,165,166,167,183,187}) printf(" %d=%02x", r, si[r]); printf("\n");
      printf("RELES expansor = %04x\n", ioexp_out);
    }
    if(!tpokes.empty()) for(auto& tp : tpokes) if(!tp.done && now_s() >= tp.t) { tp.done = 1; if(!poke_var(tp.name.c_str(), tp.v)) printf("   (variable %s no encontrada)\n", tp.name.c_str()); }
    if(scenario == 1 && avr->cycle >= c_ptt) { // recepcion: secuencia de estimulos
      int k = (int)((avr->cycle - c_ptt) / (uint64_t)(STIM_S * F_XTAL_MCU));
      if(k != stim_i && k < NSTIM) {
        if(stim_i >= 0) { memcpy(lcd_stim[stim_i][0], lcd[0], 16); memcpy(lcd_stim[stim_i][1], lcd[1], 16); }
        if(k == 2) { // tras los dos primeros: cual de las dos bandas es la deseada
          Aud a0 = audio_an(c_ptt + (uint64_t)(1.2 * F_XTAL_MCU), 1000), a1 = audio_an(c_ptt + (uint64_t)((STIM_S + 1.2) * F_XTAL_MCU), 1000);
          rx_side = (a0.tone >= a1.tone) ? 1 : -1;
        }
        stim_i = k;
        adcj_on = (k >= 5 && k <= 9);
        if(stims[k].poke && !poke_var(stims[k].poke, stims[k].pv)) printf("   (variable %s no encontrada)\n", stims[k].poke);
      }
      stage = 9;
    }
    if(scenario == 4) { // ventanas con el receptor UART apagado (LCD en los pines compartidos)
      int on = avr->data[0xc1] & 0x10;
      if(!on && rx_on) rx_off0 = avr->cycle;
      if(on && !rx_on && rx_off0 && avr->cycle >= c_ptt) { uint64_t w = avr->cycle - rx_off0; rx_off_n++; rx_off_sum += w; if(w > rx_off_max) rx_off_max = w; }
      rx_on = on;
    }
    // depuracion: CAT_WATCH=variable traza sus cambios (y la funcion que los hizo)
    if(scenario == 4 && getenv("CAT_WATCH")) { static int wa = -2, lv = -1; static uint16_t lpc;
      if(wa == -2) { wa = -1; for(auto& d : dsyms) if(!strcmp(d.name, getenv("CAT_WATCH"))) wa = d.addr; }
      if(wa >= 0 && avr->data[wa] != lv) { Sym* sy = syms.empty() ? NULL : sym_at(lpc); printf("      t=%.5f %s=%d (pc anterior %s)\n", now_s(), getenv("CAT_WATCH"), avr->data[wa], sy ? sy->name : "?"); lv = avr->data[wa]; }
      lpc = avr->pc; }
    if(scenario == 4 && avr->cycle >= c_ptt) { // CAT por la UART
      stage = 9;
      if(avr->data[0xb3] > 16) cat_tx_seen = 1; // OCR2A a frecuencia de TX
      int k = (int)((avr->cycle - c_ptt) / cat_step_cyc);
      if(k != cat_k) {
        if(cat_k >= 0 && cat_k < (int)cat_steps.size()) cat_check(cat_k);
        cat_k = k; cat_mark = uart_out.size(); cat_byte = 0; cat_next = avr->cycle;
      }
      if(cat_k < (int)cat_steps.size() && cat_byte < cat_steps[cat_k].cmd.size() && avr->cycle >= cat_next) {
        if(!(avr->data[0xc1] & 0x10)) { cat_lost++; printf("   (byte '%c' de %s perdido: RXEN apagado, t=%.4f s)\n", cat_steps[cat_k].cmd[cat_byte], cat_steps[cat_k].cmd.c_str(), now_s()); }
        avr_raise_irq(uart_in, (uint8_t)cat_steps[cat_k].cmd[cat_byte++]); cat_next += cat_byte_cyc;
      }
    }
    if(scenario == 2 && avr->cycle >= c_ptt) { // CW
      stage = 9;
      if(!cw_ready && avr->cycle >= cw_next) {
        if(btn_mv) { btn_mv = 0; pin('C', 3, 0); cw_next = avr->cycle + (uint64_t)(0.6 * F_XTAL_MCU);
                     }
        else if(!strncmp(lcd[1] + 11, "CW", 2)) { cw_ready = 1; cw_key0 = avr->cycle + (uint64_t)(0.5 * F_XTAL_MCU); lcd_show("modo CW seleccionado"); }
        else if(cw_clicks < 6) { btn_mv = 4500; pin('C', 3, 1); cw_clicks++; cw_next = avr->cycle + (uint64_t)(0.12 * F_XTAL_MCU); } // boton derecho (modo)
        else { cw_ready = -1; }
      }
      int kd = key_down(avr->cycle);
      if(kd != key_state) { key_state = kd; pin('B', 5, !kd); }
      if(!t_tx_on && cw_key0 && (si[3] & 4) == 0) t_tx_on = avr->cycle;
    }
    uint8_t t1 = avr->data[0x80];
    if(t1 != tccr1a) { tccr1a = t1; evs.push_back({avr->cycle, 2, 0, t1}); }
    if(scenario == 0 && stage == 0 && avr->cycle >= c_ptt) {
      lcd_show("en recepcion");
      f_dial = si_clk(si, 0);
      memcpy(si_rx, si, 256);
      if(!opt_quiet) printf("   t=%.2f s  RX: CLK0 = %.1f Hz, reles LPF = 0x%04x, I2C: %ld transacciones\n", now_s(), f_dial, ioexp_out, i2c_xfers);
      for(auto& p : pokes) {
        int done = 0;
        for(auto& d : dsyms) if(p.first == d.name) { avr->data[d.addr] = (uint8_t)p.second; done = 1; }
        printf("   %s = %d %s\n", p.first.c_str(), p.second, done ? "(fijado en RAM)" : "(VARIABLE NO ENCONTRADA)");
      }
      mic_on = 1; pin('B', 5, 0); stage = 1; // PTT pulsado
    }
    if(stage == 1 && !t_tx_on && (si[3] & 4) == 0) t_tx_on = avr->cycle;
    if(stage == 1 && avr->cycle >= c_rel - (uint64_t)(0.05 * F_XTAL_MCU) && lcd_hi != -2) { lcd_show("en transmision"); lcd_hi = -1; stage = 2; }
    if(stage == 2 && avr->cycle >= c_rel) { pin('B', 5, 1); stage = 3; }
    if(stage == 3 && !t_tx_off && (si[3] & 4)) t_tx_off = avr->cycle;
  }
  if(opt_eesave) { static uint8_t eb[1024]; avr_eeprom_desc_t d = {eb, 0, sizeof eb}; avr_ioctl(avr, AVR_IOCTL_EEPROM_GET, &d);
    FILE* f = fopen(opt_eesave, "wb"); if(f) { fwrite(eb, 1, sizeof eb, f); fclose(f); } }
  if(scenario == 4) {
    if(cat_k >= 0 && cat_k < (int)cat_steps.size()) cat_check(cat_k);
    printf("   CAT: %d/%d pasos ok, %d bytes perdidos con RXEN apagado\n", (int)cat_steps.size() - cat_fail, (int)cat_steps.size(), cat_lost);
    if(st_rx.n) {
      isr_report("ISR de RX con trafico CAT (hasta el primer TX)", st_rx);
      // mismo criterio que el escenario RX (el anidamiento 2 es de diseno: sdr_rx_00 hace el DSP de 8 muestras)
      if(labs(st_rx.lost()) > st_rx.expected() * 0.001) { printf("   la ISR de RX pierde muestras con CAT: FALLO\n"); cat_fail++; }
    }
    printf("   UART RX apagada por la LCD: %ld veces, %.1f ms en total (%.2f %% del tiempo), maximo %.0f us\n", rx_off_n,
           rx_off_sum * 1e3 / F_XTAL_MCU, 100.0 * rx_off_sum / (double)(c_end - c_ptt), rx_off_max * 1e6 / F_XTAL_MCU);
    return cat_fail ? 1 : 0;
  }
  if(scenario == 3) { // audio completo a 7812,5 Hz (media de 8 muestras de 62,5 kHz), float32, para analizar fuera
    long h[320] = {0}; for(auto& e : aud) h[e.cyc % 320]++;
    long best = -1; int ph = 0;
    for(int p = 0; p < 320; p++) { long w = 0; for(int d = -40; d <= 40; d++) w += h[(p + d + 320) % 320]; if(best < 0 || w < best) { best = w; ph = p; } }
    FILE* f = opt_aud ? fopen(opt_aud, "wb") : NULL; size_t i = 0; uint8_t v = 128; double acc = 0; int na = 0; long nout = 0;
    for(uint64_t c = ph; c < c_end; c += 320) { while(i < aud.size() && aud[i].cyc <= c) v = aud[i++].val; acc += v; if(++na == 8) { float o = (float)(acc / 8); if(f) fwrite(&o, 4, 1, f); acc = 0; na = 0; nout++; } }
    if(f) fclose(f);
    printf("AUDIO %ld muestras a 7812.5 Hz en %s | LCD |%.16s|%.16s|\n", nout, opt_aud ? opt_aud : "(sin --aud)", lcd[0], lcd[1]);
    return 0;
  }
  // ================================================================ escenario RX
  if(scenario == 1) {
    memcpy(lcd_stim[NSTIM - 1][0], lcd[0], 16); memcpy(lcd_stim[NSTIM - 1][1], lcd[1], 16);
    printf("\n== Interrupcion de muestreo en RX ==\n");
    isr_report("RX", st_rx);
    printf("   ADC: %ld conversiones, %ld arranques ignorados por conversion en curso (%.2f %%)\n", adc_convs, adc_skipped, 100.0 * adc_skipped / (adc_convs + adc_skipped + 1));
    { double secs = 5 * STIM_S;
      printf("   puntualidad del muestreo I/Q (estimulos sin AGC, %.1f s): desviacion rms %.2f ciclos, max %ld; muestras desplazadas >8 ciclos: %.0f/s, >32: %.0f/s, >100: %.0f/s\n",
             secs, sqrt(adcj_sq / (adcj_n + 1)), adcj_max, adcj_late8 / secs, adcj_late32 / secs, adcj_late100 / secs);
      printf("   escrituras al LCD en ese tiempo: %.0f nibbles/s\n", lcd_nibbles / secs); }
    printf("\n== Recepcion: senal conocida en I/Q (ADC0/ADC1) -> audio en OC1A ==\n");
    printf("   banda deseada: Q %s respecto a I\n", rx_side > 0 ? "retrasada" : "adelantada");
    printf("   %-28s  pico Hz   tono(cuentas rms)  resto   SINAD    THD   pico-pico   medidor LCD\n", "estimulo");
    Aud r[32];
    for(int k = 0; k < NSTIM; k++) {
      uint64_t c0 = c_ptt + (uint64_t)((k * STIM_S + 1.2) * F_XTAL_MCU);
      r[k] = audio_an(c0, stims[k].f);
      Aud t2 = stims[k].f2 ? audio_an(c0, stims[k].f2) : r[k];
      printf("   %-28s %7.1f   %8.2f        %7.2f  %5.1f dB %6.1f dB  %4.0f      |%.7s|\n", stims[k].name, r[k].fpk, sqrt(r[k].tone),
             sqrt(fmax(0, r[k].total - r[k].tone - (stims[k].f2 ? t2.tone : 0))), 10 * log10(r[k].sinad + 1e-12), 10 * log10(r[k].thd + 1e-12), r[k].pp, lcd_stim[k][0] + 9);
    }
    double rej = 10 * log10((rx_side > 0 ? r[1].tone / r[0].tone : r[0].tone / r[1].tone) + 1e-12);
    double agc = 20 * log10(sqrt(r[4].tone / (r[3].tone + 1e-12)));
    printf("\n   rechazo de la banda lateral opuesta: %.1f dB\n", rej);
    printf("   AGC: entrada +%.1f dB (3 -> 400 cuentas) => salida %+.1f dB\n", 20 * log10(400.0 / 3), agc);
    printf("   respuesta sin AGC re 1000 Hz:  300 Hz %+.1f dB   2000 Hz %+.1f dB   2800 Hz %+.1f dB   3600 Hz %+.1f dB\n", 10 * log10(r[5].tone / r[6].tone),
           10 * log10(r[7].tone / r[6].tone), 10 * log10(r[8].tone / r[6].tone), 10 * log10(r[9].tone / r[6].tone));
    { // IMD de recepcion: productos 2f1-f2 (500 Hz) y 2f2-f1 (1700 Hz) con dos tonos
      uint64_t c0 = c_ptt + (uint64_t)((10 * STIM_S + 1.2) * F_XTAL_MCU);
      Aud i1 = audio_an(c0, 500), i2 = audio_an(c0, 1700), w1 = audio_an(c0, 900);
      printf("   IMD3 de recepcion (dos tonos 900+1300 Hz): %.1f dBc\n", 10 * log10(fmax(i1.tone, i2.tone) / (w1.tone + 1e-12)));
    }
    int ok = st_rx.n && labs(st_rx.lost()) <= st_rx.expected() * 0.001 && fabs(r[0].fpk - 1000) < 5 + 1000 * (rx_side < 0) && rej < -30 &&
             fabs(r[6].fpk - 1000) < 5 && fabs(r[7].fpk - 2000) < 5;
    ok = st_rx.n && labs(st_rx.lost()) <= st_rx.expected() * 0.001 && rej < -40 && fabs(r[6].fpk - 1000) < 5 && fabs(r[7].fpk - 2000) < 5;
    { // medidor S: de 3 a 400 cuentas son +42,5 dB; la lectura del LCD debe seguirlo y ser negativa (dBm reales)
      int d3 = atoi(lcd_stim[3][0] + 9), d400 = atoi(lcd_stim[4][0] + 9);
      int okm = abs((d400 - d3) - 42) <= 4 && d400 < -30;
      printf("   medidor S: %d dBm (3 cuentas) -> %d dBm (400 cuentas): %+d dB para +42,5 dB de entrada  [%s]\n", d3, d400, d400 - d3, okm ? "OK" : "INCORRECTO");
      ok = ok && okm;
    }
    printf("\nRESULTADO RX: %s\n", ok ? "OK" : "FALLO");
    return ok ? 0 : 1;
  }
  // ================================================================ escenario CW
  if(scenario == 2) {
    lcd_show("tras manipular");
    printf("\n== CW: boton de modo real + manipulacion con la llave (6 puntos de 48 ms y una raya) ==\n");
    if(cw_ready != 1) { printf("   no se llego al modo CW con %d pulsaciones del boton\n", cw_clicks); return 1; }
    printf("   modo CW tras %d pulsacion(es) del boton derecho\n", cw_clicks);
    if(!t_tx_on) { printf("   la llave no produjo RF\n"); return 1; }
    double f_nom = 0;
    for(int i = 1; i < 12; i++) if(lcd[1][i] >= '0' && lcd[1][i] <= '9') f_nom = f_nom * 10 + (lcd[1][i] - '0');
    f_nom *= 10;
    const size_t N = (size_t)1 << 20; const double fs = F_XTAL_MCU / 16;
    uint64_t c0 = cw_key0 - (uint64_t)(0.02 * F_XTAL_MCU);
    int ok = 1;
    for(int pass = 0; pass < 2; pass++) {
      double rc = pass ? opt_rc_us : 0;
      std::vector<cd> y; std::vector<float> env;
      rf_build(c0, N, rc, f_nom, y, &env);
      float top = 0; for(float v : env) if(v > top) top = v;
      // flancos del primer punto: tiempos 10-90 %
      size_t a10 = 0, a90 = 0, d90 = 0, d10 = 0;
      for(size_t n = 0; n < N; n++) { if(!a10 && env[n] > 0.1f * top) a10 = n; if(!a90 && env[n] > 0.9f * top) { a90 = n; break; } }
      for(size_t n = a90 + (size_t)(0.02 * fs); n < N; n++) { if(!d90 && env[n] < 0.9f * top) d90 = n; if(d90 && env[n] < 0.1f * top) { d10 = n; break; } }
      long nel = 0; for(size_t n = 1; n < N; n++) nel += (env[n - 1] <= 0.5f * top && env[n] > 0.5f * top);
      for(size_t n = 0; n < N; n++) y[n] *= 0.5 - 0.5 * cos(2 * M_PI * n / (double)N);
      fft(y);
      std::vector<double> p(N); double tot = 0, best = 0, fpk = 0;
      for(size_t i = 0; i < N; i++) p[i] = std::norm(y[i]);
      const double dfb = fs / N;
      auto band = [&](double f0, double f1) { double s = 0; for(long k = (long)floor(f0 / dfb); k <= (long)ceil(f1 / dfb); k++) s += p[(size_t)(((k % (long)N) + N) % N)]; return s; };
      for(long k = (long)(-3000 / dfb); k <= (long)(3000 / dfb); k++) { double v = p[(size_t)(((k % (long)N) + N) % N)]; if(v > best) { best = v; fpk = k * dfb; } }
      tot = band(-20000, 20000);
      auto out = [&](double w) { return 10 * log10((tot - band(fpk - w, fpk + w)) / tot + 1e-12); };
      printf("   %s\n", pass ? "con el conformador RC del esquema (2 x 100 us):" : "PWM directo (sin filtro RC):");
      if(pass) printf("      elementos emitidos: %ld; subida 10-90 %%: %.2f ms; bajada 90-10 %%: %.2f ms\n", nel, (a90 - a10) * 1e3 / fs, d10 > d90 ? (d10 - d90) * 1e3 / fs : -1.0);
      printf("      portadora a dial %+.1f Hz; potencia fuera de +-250 Hz: %.1f dB, +-500 Hz: %.1f dB, +-1 kHz: %.1f dB, +-2 kHz: %.1f dB\n", fpk, out(250), out(500), out(1000), out(2000));
      if(pass && (nel != 7 || fabs(fpk) > 20 || (a90 - a10) * 1e3 / fs < 0.5)) ok = 0;
    }
    // sidetone: tono en OC1A mientras la llave esta abajo (raya de 300 ms)
    { const size_t NA = 1 << 14; std::vector<cd> x(NA); size_t i = 0; uint8_t v = 128; uint64_t ca = cw_key0 + (uint64_t)(0.72 * F_XTAL_MCU);
      while(i < aud.size() && aud[i].cyc < ca) v = aud[i++].val;
      double mean = 0; std::vector<double> s(NA);
      for(size_t n = 0; n < NA; n++) { uint64_t c = ca + n * 320; while(i < aud.size() && aud[i].cyc <= c) v = aud[i++].val; s[n] = v; mean += v; }
      mean /= NA; for(size_t n = 0; n < NA; n++) x[n] = (s[n] - mean) * (0.5 - 0.5 * cos(2 * M_PI * n / (double)NA));
      fft(x); double best = 0, fpk = 0; for(size_t k2 = 5; k2 < NA / 2; k2++) if(std::norm(x[k2]) > best) { best = std::norm(x[k2]); fpk = k2 * 62500.0 / NA; }
      printf("   sidetone durante la raya: %.0f Hz\n", fpk);
    }
    printf("\nRESULTADO CW: %s\n", ok ? "OK" : "FALLO");
    return ok ? 0 : 1;
  }
  lcd_show("de vuelta en recepcion");

  printf("\n== Interrupcion de muestreo TIMER2_COMPA (firmware real, ciclo a ciclo) ==\n");
  isr_report("RX", st_rx);
  isr_report("TX", st_tx);
  if(i2c_n) printf("   I2C en TX: %ld transacciones, duracion media %.0f ciclos (%.1f us), max %llu ciclos\n", i2c_n, (double)i2c_sum / i2c_n,
                   i2c_sum / i2c_n / 20.0, (unsigned long long)i2c_mx);
  if(!syms.empty() && st_tx.n) {
    std::vector<Sym> v = syms;
    std::sort(v.begin(), v.end(), [](const Sym& a, const Sym& b) { return a.cyc > b.cyc; });
    printf("   perfil de la ISR de TX (ciclos por muestra):\n");
    for(size_t i = 0; i < v.size() && i < 12 && v[i].cyc; i++) printf("      %7.0f  %s\n", (double)v[i].cyc / st_tx.n, v[i].name);
  }
  // desborda = se pierden muestras, o la ISR de TX (no anidada) no deja al menos un 5 % de CPU al bucle principal
  int ok_rx = st_rx.n && labs(st_rx.lost()) <= st_rx.expected() * 0.001 && st_rx.busy < 0.95 * (st_rx.c_last - st_rx.c_first);
  int ok_tx = st_tx.n && labs(st_tx.lost()) <= st_tx.expected() * 0.001 && st_tx.busy < 0.95 * (st_tx.c_last - st_tx.c_first) && st_tx.mx < 2 * st_tx.period;

  printf("\n== Conmutacion ==\n");
  if(!t_tx_on) { printf("   El firmware NO llego a transmitir (CLK2 nunca habilitado)\n"); return 1; }
  printf("   PTT -> RF en %.2f ms; soltar PTT -> RF fuera en %.2f ms\n", (t_tx_on - c_ptt) * 1e3 / F_XTAL_MCU,
         t_tx_off ? (t_tx_off - c_rel) * 1e3 / F_XTAL_MCU : -1.0);
  printf("   I2C total: %ld transacciones, %ld bytes; SI5351 %ld escrituras de registro; reles %ld\n", i2c_xfers, i2c_bytes, si_writes, lpf_writes);

  // ---- reconstruccion de la RF: reproducir los eventos sobre una rejilla de 16 ciclos (1,25 MHz)
  const int    LOG2N = opt_log2n, STEP = 16;
  const size_t N = (size_t)1 << LOG2N;
  const double fs = F_XTAL_MCU / STEP;
  uint64_t     c0 = t_tx_on + (uint64_t)(1.2 * F_XTAL_MCU); // ALC y filtros asentados
  if(c0 + N * STEP > c_rel) c0 = (c_rel > N * STEP + t_tx_on) ? c_rel - N * STEP - 1000 : t_tx_on;
  uint8_t r[256];
  memcpy(r, si_rx, 256);
  uint8_t oc = 0, tc = 0;
  size_t  ei = 0;
  // estado en c0: aplicar todo lo anterior
  auto apply = [&](const Ev& e) { if(e.kind == 0) r[e.reg] = e.val; else if(e.kind == 1) oc = e.val; else tc = e.val; };
  memset(r, 0, 256);
  for(; ei < evs.size() && evs[ei].cyc < c0; ei++) apply(evs[ei]);
  const double icr = avr->data[0x86] ? avr->data[0x86] : 255;
  std::vector<cd> x(N);
  double phase = 0, f = si_clk(r, 2) - f_dial, pk = 0, pw = 0;
  long   clip = 0, nenv = 0;
  uint8_t pwm_max = 0;
  for(size_t k = 0; k < evs.size(); k++) if(evs[k].kind == 1 && evs[k].val > pwm_max && evs[k].cyc > t_tx_on && evs[k].cyc < c_rel) pwm_max = evs[k].val;
  for(size_t n = 0; n < N; n++) {
    uint64_t c = c0 + n * STEP;
    int chg = 0;
    for(; ei < evs.size() && evs[ei].cyc <= c; ei++) { apply(evs[ei]); chg |= evs[ei].kind == 0; }
    if(chg) f = si_clk(r, 2) - f_dial;
    double a = ((tc & 0x20) && !(r[3] & 4)) ? oc / icr : 0;
    phase += 2 * M_PI * f / fs;
    double w = 0.5 - 0.5 * cos(2 * M_PI * n / (double)N);
    x[n] = std::polar(a * w, phase);
    if(a > pk) pk = a;
    pw += a * a; nenv++; clip += (oc >= pwm_max);
  }
  fft(x);
  std::vector<double> psd(N);
  for(size_t k = 0; k < N; k++) psd[k] = std::norm(x[k]);
  const double dfb = fs / N;
  auto band = [&](double f0, double f1) { double s = 0; for(long k = (long)floor(f0 / dfb); k <= (long)ceil(f1 / dfb); k++) s += psd[(size_t)(((k % (long)N) + N) % N)]; return s; };
  auto peak = [&](double f0, double f1, double excl) { double best = 0, bf = 0; for(long k = (long)(f0 / dfb); k <= (long)(f1 / dfb); k++) { double fk = k * dfb; if(excl && fabs(fk - excl) < 60) continue; double v = psd[(size_t)(((k % (long)N) + N) % N)]; if(v > best) { best = v; bf = fk; } } return bf; };
  printf("\n== Emision SSB reconstruida (%.2f s de RF a %.2f MHz de muestreo) ==\n", N / fs, fs / 1e6);
  double f_nom = 0; // dial nominal: los digitos que muestra el LCD (resolucion 10 Hz)
  for(int i = 1; i < 12; i++) if(lcd[1][i] >= '0' && lcd[1][i] <= '9') f_nom = f_nom * 10 + (lcd[1][i] - '0');
  f_nom *= 10;
  if(fabs(f_nom - f_dial) > 50) f_nom = round(f_dial / 100) * 100;
  printf("   dial nominal %.0f Hz; OL de RX programado a %+.1f Hz del dial (cuantizacion del SI5351 en freq())\n", f_nom, f_dial - f_nom);
  const double tone_ref = f_nom - f_dial; // los tonos se miden respecto al dial nominal
  printf("   envolvente: pico %.0f %% del PWM maximo (OCR1B max %u, ICR1 %.0f), potencia media %.1f dB re pico, %.2f %% del tiempo en el tope\n",
         100.0 * pk * icr / (pwm_max ? pwm_max : 1), pwm_max, icr, 10 * log10(pw / nenv / (pk * pk + 1e-12)), 100.0 * clip / nenv);
  int ok_spec = 1;
  { // medida compacta para barridos (mejoras de TX): niveles en dB re portadora a PWM maximo (pwm_max del menu = 0 dB)
    double fs0 = N * N * 0.25; // tono a plena escala con ventana Hann
    double up = band(100, 3900), lo = band(-3900, -100), pt = 0;
    if(opt_tone) { double pf = peak(-4000, 4000, 0); pt = band(pf - 25, pf + 25); }
    printf("MEDIDA pico %.1f %%  media %.2f dB re pico  potencia en banda %.2f dB  tono %.2f dB  resto en banda %.1f dBc  banda opuesta %.1f dBc  tope %.2f %%  adyacente %.1f dBc\n",
           100.0 * pk, 10 * log10(pw / nenv / (pk * pk + 1e-12)), 10 * log10(fmax(up, lo) / fs0 + 1e-12), 10 * log10(pt / fs0 + 1e-12),
           opt_tone ? 10 * log10(fmax(fmax(up, lo) - pt, 1e-30) / (pt + 1e-30)) : 0.0, 10 * log10((fmin(up, lo) + 1e-30) / fmax(up, lo)), 100.0 * clip / nenv, 10 * log10((band(3300, 6300) + band(-6300, -3300) + 1e-30) / fmax(up, lo)));
  }
  if(opt_tone) { printf("\nRESULTADO: ISR RX %s | ISR TX %s\n", ok_rx ? "OK" : "DESBORDA", ok_tx ? "OK" : "DESBORDA"); }
  else if(!opt_voice) {
    double p1 = peak(-4000, 4000, 0), p2 = peak(-4000, 4000, p1);
    double q1 = p1 - tone_ref, q2 = p2 - tone_ref; // respecto al dial nominal
    int lsb_side = (p1 + p2) < 0;
    if(fabs(p1) > fabs(p2)) std::swap(p1, p2);
    double W = 25, want = band(p1 - W, p1 + W) + band(p2 - W, p2 + W), d = p2 - p1, sg = lsb_side ? -1 : 1;
    double imd3 = 10 * log10(fmax(band(p1 - d - W, p1 - d + W), band(p2 + d - W, p2 + d + W)) / (want / 2));
    double imd5 = 10 * log10(fmax(band(p1 - 2 * d - W, p1 - 2 * d + W), band(p2 + 2 * d - W, p2 + 2 * d + W)) / (want / 2));
    double opp = 10 * log10(((lsb_side ? band(100, 3000) : band(-3000, -100)) + 1e-30) / want), adj = 10 * log10((band(3300, 6300) + band(-6300, -3300) + 1e-30) / want);
    double far = 10 * log10((band(6300, 50000) + band(-50000, -6300) + 1e-30) / want);
    printf("   micro: dos tonos de %.0f y %.0f Hz -> en antena (%s) a dial %+.1f y %+.1f Hz (error %+.1f %% y %+.1f %%)\n", opt_f1, opt_f2,
           lsb_side ? "LSB" : "USB", fabs(q1) < fabs(q2) ? q1 : q2, fabs(q1) < fabs(q2) ? q2 : q1, 100 * (sg * p1 / opt_f1 - 1 - sg * tone_ref / opt_f1),
           100 * (sg * p2 / opt_f2 - 1 - sg * tone_ref / opt_f2));
    printf("   IMD3 %.1f dBc   IMD5 %.1f dBc   banda lateral opuesta %.1f dBc   canal adyacente %.1f dBc   espurios lejanos %.1f dBc\n", imd3, imd5, opp, adj, far);
    ok_spec = fabs(sg * (p1 - tone_ref) / opt_f1 - 1) < 0.02 && fabs(sg * (p2 - tone_ref) / opt_f2 - 1) < 0.02;
    printf("\nRESULTADO: ISR RX %s | ISR TX %s | frecuencias de audio %s\n", ok_rx ? "OK" : "DESBORDA", ok_tx ? "OK" : "DESBORDA", ok_spec ? "OK" : "INCORRECTAS");
  } else {
    double up = band(100, 3000), lo = band(-3000, -100), inb = fmax(up, lo); // la banda con mas potencia es la deseada (USB o LSB)
    double opp = 10 * log10((fmin(up, lo) + 1e-30) / inb), adj = 10 * log10((band(3300, 6300) + band(-6300, -3300) + 1e-30) / inb);
    printf("   voz: banda opuesta %.1f dBc, canal adyacente %.1f dBc, lejanos %.1f dBc (re potencia en 0,1-3 kHz)\n", opp, adj,
           10 * log10((band(6300, 50000) + band(-50000, -6300) + 1e-30) / inb));
    printf("\nRESULTADO: ISR RX %s | ISR TX %s\n", ok_rx ? "OK" : "DESBORDA", ok_tx ? "OK" : "DESBORDA");
  }
  // ---- estudio con el conformador de envolvente del esquema (usdx.png):
  // KEY_OUT (PWM 78 kHz) -> 10k -> 10n -> 10k -> 10n -> puertas de los BS170.
  // Se modela el PWM real (no su media) y las dos celulas RC en cascada; la
  // amplitud de RF se toma proporcional a la tension filtrada (la curva
  // polarizacion->potencia del PA no se conoce: es el limite del modelo).
  // 'avance' adelanta la amplitud respecto a la frecuencia para ver cuanto
  // compensa el retardo de grupo del filtro (3*RC a baja frecuencia).
  if(opt_study && !opt_voice) {
    std::vector<Ev> evs0 = evs; // eventos originales del firmware
    // preenfasis de envolvente: a' = a + c1*(a - a1) + c2*(a - 2*a1 + a2), recortada a 0..tope
    auto preemph = [&](double c1, double c2) {
      evs = evs0;
      double a1 = 0, a2 = 0;
      for(auto& e : evs) if(e.kind == 1 && e.cyc > t_tx_on && e.cyc < c_rel) {
        double a = e.val, v = a + c1 * (a - a1) + c2 * (a - 2 * a1 + a2);
        a2 = a1; a1 = a;
        e.val = (uint8_t)(v < 0 ? 0 : v > pwm_max ? pwm_max : lrint(v));
      }
    };
    auto study = [&](double adv_us, double rc_us, double* o3, double* o5, double* oadj, double* opwm) {
      const uint64_t adv = (uint64_t)(adv_us * 20.0 + 0.5);
      uint8_t rr[256]; memset(rr, 0, 256);
      uint8_t o = 0, tcc = 0;
      size_t ef = 0, ea = 0;
      for(; ef < evs.size() && evs[ef].cyc < c0; ef++) if(evs[ef].kind == 0) rr[evs[ef].reg] = evs[ef].val;
      for(; ea < evs.size() && evs[ea].cyc < c0 + adv; ea++) { if(evs[ea].kind == 1) o = evs[ea].val; else if(evs[ea].kind == 2) tcc = evs[ea].val; }
      std::vector<cd> y(N);
      double ph = 0, ff = si_clk(rr, 2) - f_dial, v1 = 0, v2 = 0;
      const double k = (STEP / F_XTAL_MCU) / (rc_us * 1e-6);
      for(int pre = 0; pre < 4000; pre++) { double vin = o / 256.0; if(rc_us > 0) { v1 += k * (vin - 2 * v1 + v2); v2 += k * (v1 - v2); } else v2 = vin; }
      for(size_t n = 0; n < N; n++) {
        uint64_t c = c0 + n * STEP;
        int chg = 0;
        for(; ef < evs.size() && evs[ef].cyc <= c; ef++) if(evs[ef].kind == 0) { rr[evs[ef].reg] = evs[ef].val; chg = 1; }
        for(; ea < evs.size() && evs[ea].cyc <= c + adv; ea++) { if(evs[ea].kind == 1) o = evs[ea].val; else if(evs[ea].kind == 2) tcc = evs[ea].val; }
        if(chg) ff = si_clk(rr, 2) - f_dial;
        // PWM rapido de Timer1 (TOP=255): alto mientras el contador < OCR1B
        int cnt = (int)(c & 255), hi = 0;
        for(int j = 0; j < STEP; j++) hi += ((cnt + j) & 255) < o;
        double vin = ((tcc & 0x20) && !(rr[3] & 4)) ? hi / (double)STEP : 0;
        if(rc_us > 0) { v1 += k * (vin - 2 * v1 + v2); v2 += k * (v1 - v2); } else v2 = vin;
        ph += 2 * M_PI * ff / fs;
        double w = 0.5 - 0.5 * cos(2 * M_PI * n / (double)N);
        y[n] = std::polar(v2 * w, ph);
      }
      fft(y);
      for(size_t i = 0; i < N; i++) psd[i] = std::norm(y[i]);
      double p1 = peak(-4000, 4000, 0), p2 = peak(-4000, 4000, p1);
      if(fabs(p1) > fabs(p2)) std::swap(p1, p2);
      double W = 25, want = band(p1 - W, p1 + W) + band(p2 - W, p2 + W), d = p2 - p1;
      *o3 = 10 * log10(fmax(band(p1 - d - W, p1 - d + W), band(p2 + d - W, p2 + d + W)) / (want / 2));
      *o5 = 10 * log10(fmax(band(p1 - 2 * d - W, p1 - 2 * d + W), band(p2 + 2 * d - W, p2 + 2 * d + W)) / (want / 2));
      *oadj = 10 * log10((band(3300, 6300) + band(-6300, -3300) + 1e-30) / want);
      *opwm = 10 * log10((band(78125 - 3000, 78125 + 3000) + band(-78125 - 3000, -78125 + 3000) + 1e-30) / want);
    };
    printf("\n== Estudio: conformador de envolvente del esquema (2 x RC de %.0f us) ==\n", opt_rc_us);
    printf("   avance de amplitud    IMD3     IMD5   adyacente  PWM +-78 kHz\n");
    double a3, a5, aadj, apwm;
    study(0, 0, &a3, &a5, &aadj, &apwm);
    printf("   sin filtro RC       %6.1f   %6.1f   %6.1f   %6.1f dBc   (PWM directo, referencia)\n", a3, a5, aadj, apwm);
    double best = 0, best3 = 99;
    for(double adv : {0.0, 100.0, 200.0, 250.0, 300.0, 350.0, 400.0, 500.0}) {
      study(adv, opt_rc_us, &a3, &a5, &aadj, &apwm);
      printf("   %5.0f us            %6.1f   %6.1f   %6.1f   %6.1f dBc%s\n", adv, a3, a5, aadj, apwm, adv == 0 ? "   <- firmware actual (= legacy)" : "");
      if(a3 < best3) { best3 = a3; best = adv; }
      if(adv == 0 && opt_expect < 0) {
        printf("   exigido IMD3 <= %.1f dBc con el RC del esquema: %s\n", opt_expect, a3 <= opt_expect ? "OK" : "NO SE CUMPLE");
        if(a3 > opt_expect) ok_spec = 0;
      }
    }
    printf("   mejor alineamiento: amplitud adelantada %.0f us (%.2f muestras de TX) -> IMD3 %.1f dBc\n", best, best / 208.0, best3);
    printf("   con preenfasis digital de la envolvente  a' = a + c1*(a-a1) + c2*(a-2*a1+a2):\n");
    printf("     c1    c2   avance    IMD3     IMD5   adyacente\n");
    const double T = 208.0, tau = opt_rc_us;
    struct { double c1, c2; } pe[] = {{1.0, 0}, {1.5, 0}, {3 * tau / T, 0}, {3 * tau / T, tau * tau / (T * T)}, {2.0, 0.25}};
    for(auto& p : pe)
      for(double adv : {0.0, 50.0, 100.0, 200.0}) {
        preemph(p.c1, p.c2);
        study(adv, opt_rc_us, &a3, &a5, &aadj, &apwm);
        printf("   %5.2f %5.2f  %4.0f us  %6.1f   %6.1f   %6.1f\n", p.c1, p.c2, adv, a3, a5, aadj);
      }
    evs = evs0;
  }
  return (ok_rx && ok_tx && ok_spec) ? 0 : 1;
}
