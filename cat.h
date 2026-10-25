// cat.h - uSDX Plus Orange (producción)
// Kenwood TS-480 CAT interface. Base: subconjunto legacy (v1 Section 12) +
// comandos TS-480 estándar sobre los parámetros del menú (tabla CAT_MAP, sin
// código por parámetro) y EX genérico (EXnnn0000[v]; nnn = nº de menú en LCD).
// Provides cat_serial_event() called from serialEvent in the .ino.
// Escrituras: silenciosas (TS-480); rechazo o fuera de rango -> "?;".
// Lo que toca RF (SI5351/LPF/ADC) nunca se aplica en TX: FA/MD/RIT se difieren
// a cat_apply(); los parámetros con on_change se rechazan con "?;" en TX.
// Las respuestas se montan sobre el propio CATcmd (ya trae las letras del
// comando) y salen con un único Serial.print.

#pragma once

#include "menu.h" // menu object + MENU[] + display_vfo/vfo_save_current/set_lpf decls
#include "si5351.h"
#include "usdx_settings.h"
#include <Arduino.h>
#include <stdint.h>

#define CATCMD_SIZE 40
char CATcmd[CATCMD_SIZE];

volatile uint8_t cat_active = 0;
volatile uint8_t cat_ptr    = 0;
static uint8_t   cat_len    = 0; // longitud de la trama en curso (con ';')

extern volatile int32_t freq;
extern volatile uint8_t mode;
extern volatile uint8_t prev_mode;
extern volatile uint8_t changedModeCAT;
extern volatile int32_t rit;
extern volatile uint8_t vfosel;
extern uint8_t          vfomode[2];
extern volatile uint32_t semi_qsk_timeout;
extern volatile uint8_t  smode;
extern volatile uint32_t rxend_event;
extern int32_t vfo[2];
extern int16_t dbm; // S-meter (main .ino)
extern volatile uint32_t save_event_time; // deferred VFO persist (main .ino)
extern void             switch_rxtx(uint8_t tx_enable);
extern void             vfo_apply(void);
extern volatile uint8_t tx;
extern volatile uint8_t vox_tx;

// FA/MD/RIT recibidos: se aplican en loop() SOLO con TX off (legacy 'change',
// 5676: "prevent simultaneous I2C bus access" con el ISR de TX).
volatile uint8_t cat_change = 0;

// --- response formatter (no sprintf, saves flash) --------------------------
// escribe v en d con 'digits' cifras (0 = las justas); devuelve el final
static char* cat_num(char* d, uint32_t v, uint8_t digits) {
  char    t[11];
  uint8_t n = 0;
  do {
    t[n++] = '0' + (v % 10);
    v /= 10;
  } while(v || n < digits);
  while(n)
    *d++ = t[--n];
  return d;
}
static void cat_end(char* d) { // cierra con ';' y envía CATcmd entero
  *d++ = ';';
  *d   = 0;
  Serial.print(CATcmd);
}
static void cat_reply(uint32_t v, uint8_t digits) { cat_end(cat_num(CATcmd + 2, v, digits)); }
static void cat_err() { Serial.print(F("?;")); }
// TX real: la ISR muestrea a F_SAMP_TX (OCR2A grande) desde switch_rxtx(1)
// hasta switch_rxtx(0). 'tx' no basta: con CARRIER_COMPLETELY_OFF_ON_LOW es el
// latch de portadora y ssb() lo baja a 0 en cada pausa de la voz (TX por CAT),
// y entonces se tocaria el SI5351 con la ISR de TX activa (tests/avr_sim).
#define CAT_OCR2A_RX_MAX 16 // RX 62,5 kHz -> 4; TX 4,8 kHz -> 64 (static_assert en el .ino)
static inline uint8_t cat_busy() { return tx || vox_tx || OCR2A > CAT_OCR2A_RX_MAX; }
// parámetro numérico en CATcmd+i (nunca grabar 0 por basura: "RTC;", "GTx;")
static int32_t cat_arg(uint8_t i, uint8_t& ok) {
  char c = CATcmd[i];
  ok     = (c == '-') || (uint8_t)(c - '0') <= 9;
  return atol(CATcmd + i);
}

// --- acceso genérico a MENU[] (por nº de menú, el que muestra la LCD) -------
static uint8_t cat_find(uint8_t id, MenuParam& p) {
  for(uint8_t i = 0; i < (uint8_t)MENU_COUNT; i++) {
    memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
    if(p.label == id)
      return p.value && p.type != P_TEXT; // CQ Message (texto) fuera de CAT
  }
  return 0;
}
static int32_t cat_val(const MenuParam& p) {
  if(p.value == (void*)&rit)
    return rit != 0; // RIT on/off (el offset va por IF/RU/RD)
  if(p.type == P_T8S)
    return *(int8_t*)p.value;
  if(p.type == P_T32)
    return *(int32_t*)p.value;
  return *(uint8_t*)p.value; // P_T8 / P_ENUM (no hay P_T16 en MENU[])
}
// como confirmar la edición en el menú: rango, valor, EEPROM y on_change.
// Mismo valor: no hace nada (sin escrituras de EEPROM ni resets de PLL).
static uint8_t cat_set(const MenuParam& p, int32_t v) {
  if(v < p.min || v > p.max)
    return 0;
  if(v == cat_val(p))
    return 1;
  if(p.on_change && cat_busy())
    return 0;
  uint8_t sz = 1;
  if(p.type == P_T32) {
    *(int32_t*)p.value = v;
    sz                 = 4;
  } else
    *(uint8_t*)p.value = (uint8_t)v;
  if(p.eslot)
    menu_eeprom_save(p.eslot, p.value, sz);
  if(p.on_change) {
    p.on_change();
    cat_change = 1; // LPF/RIT/pantalla al día (cat_apply)
  }
  return 1;
}

// --- comandos TS-480 que son un parámetro del menú con escala --------------
enum { K_DIR, K_VOL, K_X100, K_CAP1, K_CAP2, K_PCT, K_MIC };
struct CatMap {
  char    c0, c1;
  uint8_t id;   // nº de menú
  uint8_t rlen; // longitud de la lectura sin ';' (AG0; = 3)
  uint8_t dig;  // dígitos de la respuesta
  uint8_t kind;
};
static const CatMap CAT_MAP[] PROGMEM = {
    {'A', 'G', 0, 3, 4, K_VOL},   // AG0nnn: volumen 000-255 <-> 0..16
    {'G', 'T', 7, 2, 3, K_DIR},   // GTnnn: AGC 000 OFF / 001 Fast / 002 Slow
    {'R', 'A', 9, 2, 4, K_X100},  // RAnn(00): atenuador 00..07
    {'N', 'R', 8, 2, 1, K_CAP2},  // NR0/1/2: NR off / 1 / >=2 (puerta)
    {'R', 'L', 8, 2, 2, K_DIR},   // RLnn: nivel NR 00..08
    {'P', 'C', 20, 2, 3, K_PCT},  // PCnnn: potencia 000-100 <-> TX Drive 0..8
    {'M', 'G', 28, 2, 3, K_MIC},  // MGnnn: micro 000-100 <-> Mic Atten 4..0
    {'K', 'S', 14, 2, 3, K_DIR},  // KSnnn: keyer wpm
    {'V', 'X', 18, 2, 1, K_DIR},  // VX0/1: VOX
    {'P', 'R', 30, 2, 1, K_CAP1}, // PR0/1: TX Comp off / on (nivel por EX030)
    {'R', 'T', 6, 2, 1, K_CAP1},  // RT0/1: RIT off / on (on = +1 Hz, como el menú)
    {'F', 'R', 5, 2, 1, K_DIR},   // FR0/1: VFO A / B (on_vfosel)
};

static uint8_t cat_map(char c0, char c1) {
  for(uint8_t i = 0; i < sizeof(CAT_MAP) / sizeof(CatMap); i++) {
    CatMap m;
    memcpy_P(&m, &CAT_MAP[i], sizeof(m));
    if(m.c0 != c0 || m.c1 != c1)
      continue;
    MenuParam p;
    if(!cat_find(m.id, p))
      return 0;
    int16_t cur = cat_val(p); // 16 bit: todos los mapeados caben
    uint8_t cap = m.kind - K_CAP1 + 1;
    if(cat_len <= m.rlen + 1) { // lectura: "AG0;" o "AG;" (nunca graba 0)
      switch(m.kind) {
      case K_VOL:
        cur = (cur < 0) ? 0 : (cur > 15) ? 255 : cur << 4;
        break;
      case K_X100:
        cur *= 100;
        break;
      case K_CAP1:
      case K_CAP2:
        if(cur > cap)
          cur = cap;
        break;
      case K_PCT:
        cur = (cur * 25) >> 1;
        break;
      case K_MIC:
        cur = 100 - cur * 25;
        break;
      }
      cat_reply(cur, m.dig);
      return 1;
    }
    uint8_t ok;
    int16_t n = cat_arg(m.rlen, ok); // escritura
    switch(m.kind) {
    case K_VOL:
      n = (n + 8) >> 4;
      break;
    case K_CAP1:
    case K_CAP2:
      if(n >= cap && cur >= cap)
        n = cur; // PR1/NR2/RT1 conservan el nivel elegido
      break;
    case K_PCT:
      n = (n * 2 + 12) / 25;
      break;
    case K_MIC:
      n = (112 - n) / 25;
      break;
    }
    if(!ok || !cat_set(p, n))
      cat_err();
    return 1;
  }
  return 0;
}

static void Command_SETFreqA() { // legacy 4512-4520: no range check
  if(CATcmd[2] != ';') { // 'FAxxxx...;'
    freq = (uint32_t)atol(CATcmd + 2);
    cat_change = 1; // legacy 4519: change=true (aplicado en loop, fuera de TX)
  }
}
// legacy change block (5676-5712): VFO, LPF, SI5351 y display, nunca en TX
static void cat_apply() {
  if(!cat_change || cat_busy())
    return;
  cat_change = 0;
  vfo[vfosel % 2] = freq; // VFO follows (legacy change block)
  save_event_time = millis() + 1000; // persist when idle (legacy 5684)
  uint8_t f = freq / 1000000UL;
  set_lpf(f); // switch LPF band (legacy 5701)
  bandval_align();
  vfo_apply(); // mode-dependent IQ phase + CW offset (legacy 5704)
#ifdef RIT_ENABLE
  if(rit) { // legacy 5712
    si5351.freq_calc_fast(rit);
    si5351.SendPLLRegisterBulk();
  }
#endif
  if(menu.state == MENU_MAIN)
    display_vfo(); // refresh + cursor (legacy change block)
}

// IF: trama TS-480 completa de 38 caracteres (legacy 4547) con el offset y
// el estado del RIT, TX/RX y el VFO activo
static void Command_IF() {
  int32_t r = rit;
  char*   d = cat_num(CATcmd + 2, freq, 11);
  d         = cat_num(d, 0, 5);  // P2 paso
  *d++      = (r < 0) ? '-' : '+'; // P3 offset RIT
  d         = cat_num(d, (r < 0) ? -r : r, 4);
  *d++      = r ? '1' : '0';     // P4 RIT on
  d         = cat_num(d, 0, 4);  // P5 XIT, P6-P7 canal de memoria
  *d++      = '0' + cat_busy();  // P8 TX/RX
  *d++      = '1' + mode;        // P9 modo
  *d++      = '0' + (vfosel & 1); // P10 VFO
  cat_end(cat_num(d, 0, 6));     // P11-P15
}
static void Command_SetMD() { // legacy 4589-4596 (legacy: no range check)
  int8_t m = CATcmd[2] - '1';
  if(m < 0)
    m = 0; // clamp: MD0/MD6+ would corrupt mode (wild PROGMEM read on display)
  if(m > 4)
    m = 4; // LSB/USB/CW/FM/AM only
  mode = (uint8_t)m;
  vfomode[vfosel % 2] = mode; // legacy 4593
  si5351.iqmsa = 0;           // enforce PLL reset (legacy 4595)
  cat_change = 1;             // legacy 4594: change=true (aplicado fuera de TX)
}
static void __attribute__((noinline)) cat_rit(int32_t r) { // RIT por CAT: limitado como el dial, aplicado en loop
  rit        = (r > 9999) ? 9999 : (r < -9999) ? -9999 : r;
  cat_change = 1;
}
// EXnnn0000; -> EXnnn0000v;  EXnnn0000v; graba (silencioso)
static void Command_EX() {
  MenuParam p;
  uint8_t   id = (CATcmd[2] - '0') * 100 + (CATcmd[3] - '0') * 10 + (CATcmd[4] - '0');
  if(cat_len < 10 || !cat_find(id, p))
    return cat_err();
  if(CATcmd[9] == ';') {
    int32_t v = cat_val(p);
    char*   d = CATcmd + 9;
    if(v < 0) {
      *d++ = '-';
      v    = -v;
    }
    cat_end(cat_num(d, v, 0));
  } else {
    uint8_t ok;
    int32_t v = cat_arg(9, ok);
    if(!ok || !cat_set(p, v))
      cat_err();
  }
}

#define CC(a, b) (((uint16_t)(a) << 8) | (uint8_t)(b))
static void analyseCATcmd() {
  char    c1 = CATcmd[1], c2 = CATcmd[2];
  uint8_t rd = (c2 == ';'); // lectura sin parámetros
  switch(CC(CATcmd[0], c1)) {
  case CC('F', 'A'):
    rd ? cat_reply(freq, 11) : Command_SETFreqA();
    break;
  case CC('F', 'B'): { // VFO no activo: no conmuta ni toca RF
    int32_t& v = vfo[(vfosel & 1) ^ 1];
    if(rd)
      cat_reply(v, 11);
    else {
      v               = atol(CATcmd + 2);
      save_event_time = millis() + 1000; // persist when idle
    }
  } break;
  case CC('F', 'T'): // sin split: TX siempre en el VFO de RX (FR, en CAT_MAP)
    if(rd)
      cat_reply(vfosel & 1, 1);
    else if((uint8_t)(c2 - '0') != (vfosel & 1))
      cat_err();
    break;
  case CC('I', 'F'):
    if(rd)
      Command_IF();
    break;
  case CC('M', 'D'):
    if(rd)
      cat_reply(mode + 1, 1);
    else if(CATcmd[3] == ';')
      Command_SetMD();
    break;
  case CC('R', 'X'):
    if(rd) {
      switch_rxtx(0);
      semi_qsk_timeout = 0; // hack for multiple RX cmds (legacy 4607)
      Serial.print(F("RX0;"));
    }
    break;
  case CC('T', 'X'):
    if(rd || c2 == '0' || c2 == '1' || c2 == '2')
      switch_rxtx(1);
    break;
  case CC('R', 'U'):
  case CC('R', 'D'): { // RU; / RUnnnnn; (Hz, 10 Hz sin parámetro)
    int32_t n = rd ? 10 : atol(CATcmd + 2);
    cat_rit(rit + ((c1 == 'U') ? n : -n));
  } break;
  case CC('R', 'C'): // legacy 4347 (antes 'RTC': inalcanzable)
    if(rd) {
      cat_rit(0); // y quita el offset del SI5351
      Serial.print(F("RC;"));
    }
    break;
  case CC('S', 'M'): // SM0nnnn: 0..30, S9 = 15 (-73 dBm), 4 dB por cuenta
    if(rd || c2 == '0') {
      int16_t s = (dbm + 133) >> 2;
      cat_reply((s < 0) ? 0 : (s > 30) ? 30 : s, 5);
    }
    break;
  case CC('E', 'X'):
    Command_EX();
    break;
  case CC('I', 'D'):
    if(rd)
      Serial.print(F("ID020;"));
    break;
  case CC('P', 'S'):
    if(rd)
      Serial.print(F("PS1;")); // PS1; no-op (legacy parity)
    break;
  case CC('A', 'I'): // AI and AI0 both -> AI0 (legacy parity)
    Serial.print(F("AI0;"));
    break;
  case CC('X', 'T'):
    if(c2 == '1')
      Serial.print(F("XT1;"));
    break;
  case CC('F', 'L'):
    if(c2 == '0')
      Serial.print(F("FL0;"));
    break;
  case CC('R', 'S'):
    if(rd)
      Serial.print(F("RS0;"));
    break;
  default:
    if(!cat_map(CATcmd[0], c1))
      cat_err(); // legacy 4424-4431
  }
}

// called from serialEvent() in the .ino
static void cat_serial_event() {
  if(Serial.available()) {
    rxend_event = millis() + 10; // block display to prevent CAT interleave (legacy 4438)
    char data         = Serial.read();
    CATcmd[cat_ptr++] = data;
    if(data == ';') {
      CATcmd[cat_ptr] = '\0';
      cat_len         = cat_ptr;
      cat_ptr         = 0;
      if(!cat_active) {
        cat_active = 1;
        smode      = 0; // disable smeter to reduce display activity (legacy 4445)
      }
      analyseCATcmd();
      delay(10); // legacy 4455
    } else if(cat_ptr > (CATCMD_SIZE - 1)) {
      Serial.print(F("E;"));
      cat_ptr = 0;
    }
  }
}
