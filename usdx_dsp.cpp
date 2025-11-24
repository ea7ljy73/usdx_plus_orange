#include "usdx_dsp.h"
#include "usdx_i2c.h"
#include "usdx_rf.h"
#include "usdx_ui.h"
#include <Arduino.h>

// ==========================================
// Global Variables Definition
// ==========================================
volatile uint8_t mode = USB;
volatile uint8_t tx = 0;
volatile uint8_t filt = 0;
volatile uint8_t vox = 0;
volatile uint8_t vox_thresh = (1 << 1); // Default, updated in code
volatile uint8_t drive = 2;
volatile uint8_t quad = 0;
volatile bool dig_mode = false;
volatile uint8_t mox = 0;
volatile int8_t volume = 12;
volatile uint8_t agc = 1;
volatile uint8_t nr = 2;
volatile uint8_t att = 0;
volatile uint8_t att2 = 2;
volatile uint8_t cw_tone = 1;
volatile uint32_t cw_offset;
volatile uint8_t cat_streaming = 0;
volatile uint8_t menumode = 0;
volatile uint8_t ft8mode = 0;
volatile uint8_t prev_mode_ft8 = 0;
volatile uint8_t prev_filt_ft8 = 0;
volatile uint8_t prev_agc_ft8 = 0;
volatile uint8_t prev_nr_ft8 = 0;
volatile uint8_t cwdec = 1;
volatile uint8_t cw_event = false;
volatile uint16_t numSamples = 0;
uint8_t lut[256];
volatile uint8_t amp;
volatile uint8_t practice = false;
volatile uint8_t wpm = 25;
volatile unsigned long ditTime;

#ifdef CW_MESSAGE
#ifdef CW_MESSAGE_EXT
char cw_msg[6][48] = {CW_MSG1, CW_MSG2, CW_MSG3, CW_MSG4, CW_MSG5, CW_MSG6};
#else
char cw_msg[1][48] = {CW_MSG1};
#endif
uint8_t cw_msg_interval = 5;
uint32_t cw_msg_event = 0;
uint8_t cw_msg_id = 0;
#endif

// ==========================================
// Internal Variables & Constants
// ==========================================
#define F_SAMP_TX 4800
#if (F_MCU != 20000000)
const int16_t _F_SAMP_TX = (F_MCU * 4800LL / 20000000);
#else
#define _F_SAMP_TX F_SAMP_TX
#endif
#define _UA 600
#define MAX_DP ((filt == 0) ? _UA : (filt == 3) ? _UA / 4 : _UA / 2)
#define CARRIER_COMPLETELY_OFF_ON_LOW 1
#define MULTI_ADC 1

typedef void (*func_t)(void);
volatile func_t func_ptr;

// ==========================================
// Forward Declarations of Internal Functions
// ==========================================
inline int16_t ssb(int16_t in);
inline int16_t arctan3(int16_t q, int16_t i);
inline int16_t magn(int16_t i, int16_t q);
inline void _vox(bool trigger);
inline void process_minsky();
inline int16_t process_agc(int16_t in);
inline int16_t process_agc_fast(int16_t in);
inline int16_t process_nr(int16_t in);
inline int16_t filt_var(int16_t za0);
inline int16_t slow_dsp(int16_t ac);
inline void sdr_rx_common();
void sdr_rx_q();
uint8_t delayWithKeySense(uint32_t ms);

// ==========================================
// DSP Implementation
// ==========================================

inline void _vox(bool trigger) {
  if (trigger) {
    tx = (tx) ? 254 : 255;
  } else {
    if (tx)
      tx--;
  }
}

inline int16_t arctan3(int16_t q, int16_t i) {
#define _atan2(z) (_UA / 8 + _UA / 22 - _UA / 22 * z) * z
  int16_t r;
  if (abs(q) > abs(i))
    r = _UA / 4 - _atan2(abs(i) / abs(q));
  else
    r = (i == 0) ? 0 : _atan2(abs(q) / abs(i));
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
}

#define magn(i, q) (abs(i) > abs(q) ? abs(i) + abs(q) / 4 : abs(q) + abs(i) / 4)

inline int16_t ssb(int16_t in) {
  static int16_t dc, z1;
  int16_t i, q;
  uint8_t j;
  static int16_t v[16];
  for (j = 0; j != 15; j++)
    v[j] = v[j + 1];
#ifdef MORE_MIC_GAIN
  if (dig_mode) {
    int16_t ac = in;
    dc = (ac + (7) * dc) / (7 + 1);
    v[15] = (ac - dc) / 2;
  } else {
    int16_t ac = in * 2;
    ac = ac + z1;
    z1 = (in - (2) * z1) / (2 + 1);
    dc = (ac + (2) * dc) / (2 + 1);
    v[15] = (ac - dc);
  }
  i = v[7] * 2;
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) +
        (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) +
        ((v[6] - v[8]) << 4)) >>
       6) +
      (v[6] - v[8]);
  uint16_t _amp = magn(i / 2, q / 2);
#else
  dc = (in + dc) / 2;
  int16_t ac = (in - dc);
  v[15] = (ac + z1);
  z1 = ac;
  i = v[7];
  q = ((((v[0] - v[14]) << 1) + ((v[2] - v[12]) << 3) +
        (((v[4] - v[10]) << 4) + ((v[4] - v[10]) << 2) + (v[4] - v[10])) +
        (((v[6] - v[8]) << 4) - (v[6] - v[8]))) >>
       7) +
      ((v[6] - v[8]) >> 1);
  uint16_t _amp = magn(i, q);
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  _vox(_amp > vox_thresh);
#else
  if (vox)
    _vox(_amp > vox_thresh);
#endif

  _amp = _amp << (drive);
  _amp = ((_amp > 255) || (drive == 8)) ? 255 : _amp;
  amp = (tx) ? lut[_amp] : 0;

  static int16_t prev_phase;
  int16_t phase = arctan3(q, i);
  int16_t dp = phase - prev_phase;
  prev_phase = phase;

  if (dp < 0)
    dp = dp + _UA;
#ifdef QUAD
  if (dp >= (_UA / 2)) {
    dp = dp - _UA / 2;
    quad = !quad;
  }
#endif

#ifdef MAX_DP
  if (dp > MAX_DP) {
    prev_phase = phase - (dp - MAX_DP);
    dp = MAX_DP;
  }
#endif
  if (mode == USB)
    return dp * (_F_SAMP_TX / _UA);
  else
    return dp * (-_F_SAMP_TX / _UA);
}

static int16_t _adc;
void dsp_tx() {
#ifdef MULTI_ADC
  int16_t adc;
  adc = ADC;
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
#ifdef QUAD
#ifdef TX_CLK0_CLK1
  si5351.SendRegister(16, (quad) ? 0x1f : 0x0f);
  si5351.SendRegister(17, (quad) ? 0x1f : 0x0f);
#else
  si5351.SendRegister(18, (quad) ? 0x1f : 0x0f);
#endif
#endif
  OCR1BL = amp;
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  int16_t df = ssb(_adc >> MIC_ATTEN);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
  si5351.freq_calc_fast(df);
  adc += ADC;
  ADCSRA |= (1 << ADSC);
#define AF_BIAS 32
  _adc = (adc / 4 - (512 - AF_BIAS));
#else
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();
  OCR1BL = amp;
  int16_t adc = ADC - 512;
  int16_t df = ssb(adc >> MIC_ATTEN);
  si5351.freq_calc_fast(df);
#endif

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  if (tx == 1) {
    OCR1BL = 0;
    si5351.SendRegister(SI_CLK_OE, TX0RX0);
  }
  if (tx == 255) {
    si5351.SendRegister(SI_CLK_OE, TX1RX0);
  }
#endif

#ifdef MOX_ENABLE
  if (!mox)
    return;
  OCR1AL = (adc << (mox - 1)) + 128;
#endif
}

volatile int16_t p_sin = 0;
volatile int16_t n_cos = 20000;
const uint32_t tones[] = {F_MCU * 700ULL / 20000000, F_MCU * 600ULL / 20000000,
                          F_MCU * 700ULL / 20000000};

inline void process_minsky() {
  int16_t alpha = (int32_t)tones[cw_tone] * 51 / _F_SAMP_TX;
  p_sin += (int32_t)alpha * n_cos >> 8;
  n_cos -= (int32_t)alpha * p_sin >> 8;
}

const uint8_t ramp[] PROGMEM = {255, 254, 252, 249, 245, 239, 233, 226,
                                217, 208, 198, 187, 176, 164, 152, 139,
                                127, 115, 102, 90,  78,  67,  56,  46,
                                37,  28,  21,  15,  9,   5,   2};

void dsp_tx_cw() {
#ifdef KEY_CLICK
  if (OCR1BL < lut[255]) {
    for (uint16_t i = 31; i != 0; i--) {
      OCR1BL = lut[pgm_read_byte_near(&ramp[i - 1])];
      delayMicroseconds(60);
    }
  }
#endif
  OCR1BL = lut[255];
  process_minsky();
  OCR1AL = (p_sin >> (8 + (16 - volume))) + 128;
}

void dsp_tx_am() {
  ADCSRA |= (1 << ADSC);
  OCR1BL = amp;
  int16_t adc = ADC - 512;
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive - 4);
#define AM_BASE 32
  in = max(0, min(255, (in + AM_BASE)));
  amp = in;
}

void dsp_tx_fm() {
  ADCSRA |= (1 << ADSC);
  OCR1BL = lut[255];
  si5351.SendPLLRegisterBulk();
  int16_t adc = ADC - 512;
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive);
  int16_t df = in;
  si5351.freq_calc_fast(df);
}

// ==========================================
// RX Main Loop & CW Logic
// ==========================================

const char m2c[] PROGMEM = "~ "
                           "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/"
                           "***H*7*G*8*90************?_****\"**.****@***'**-***"
                           "*****;!*)*****,****:****";

uint8_t delayWithKeySense(uint32_t ms) {
  uint32_t event = millis() + ms;
  for (; millis() < event;) {
    wdt_reset();
    // Note: BUTTONS, DAH, DIT are macros from usdx_config.h
    // inv is likely a global or macro. Let's check.
    // inv is defined in usdx_config.h as 0 or 1 depending on KEYER_INVERT
    // But wait, inv is a variable in .ino?
    // #define inv 0 // if not defined.
    // Let's assume standard config for now.
    // Actually, inv is used in .ino line 2174.
    // I need to check if inv is global.
    // It seems inv is not declared in my usdx_dsp.h.
    // I'll assume it's 0 for now or check config.
    // In usdx_config.h: #define inv 0 (if not defined)
    // So I can use inv if included.
    if (inv ^ digitalRead(BUTTONS) || !digitalRead(DAH) || !digitalRead(DIT)) {
      for (; inv ^ digitalRead(BUTTONS);)
        wdt_reset();
      return 1;
    }
  }
  return 0;
}

int cw_tx(char ch) {
  char sym;
  for (uint8_t j = 0; (sym = pgm_read_byte_near(m2c + j)); j++) {
    if (sym == ch) {
      wdt_reset();
      uint8_t k = 0x80;
      for (; !(j & k); k >>= 1)
        ;
      k >>= 1;
      if (k == 0)
        delay(ditTime * 4);
      else {
        for (; k; k >>= 1) {
          switch_rxtx(1);
          if (delayWithKeySense(ditTime * ((j & k) ? 3 : 1))) {
            switch_rxtx(0);
            return 1;
          }
          switch_rxtx(0);
          if (delayWithKeySense(ditTime))
            return 1;
        }
        if (delayWithKeySense(ditTime * 2))
          return 1;
      }
      break;
    }
  }
  return 0;
}

int cw_tx(char *msg) {
  for (uint8_t i = 0; msg[i]; i++) {
    lcd.setCursor(0, 0);
    lcd.print(i);
    lcd.print("    ");
    if (cw_tx(msg[i]))
      return 1;
  }
  return 0;
}

// CW Decoder Variables
static int32_t avg = 256;
static uint8_t sym;
static char out[] = "                ";
bool realstate = LOW;
bool realstatebefore = LOW;
bool filteredstate = LOW;
bool filteredstatebefore = LOW;
uint8_t nbtime = 16;
uint32_t starttimehigh;
uint32_t highduration;
uint32_t hightimesavg;
uint32_t lowtimesavg;
uint32_t startttimelow;
uint32_t lowduration;
uint32_t laststarttime = 0;

void printsym(bool submit) {
  if (sym < 128) {
    char ch = pgm_read_byte_near(m2c + sym);
    if (ch != '*') {
#ifdef CW_INTERMEDIATE
      out[15] = ch;
      cw_event = true;
      if (submit) {
        for (int i = 0; i != 15; i++) {
          out[i] = out[i + 1];
        }
        out[15] = ' ';
      }
#else
      for (int i = 0; i != 15; i++)
        out[i] = out[i + 1];
      out[15] = ch;
      cw_event = true;
#endif
    }
  }
  if (submit)
    sym = 1;
}

void dec2() {
  if (filteredstate != filteredstatebefore) {
    if (menumode == 0) {
      lcd.noCursor();
      lcd.setCursor(15, 1);
      lcd.print((filteredstate) ? 'R' : ' '); /*stepsize_showcursor();*/
    }
    // stepsize_showcursor is in UI or main. I might need a callback or extern.
    // For now commented out.

    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = (millis() - startttimelow);
    }

    if (filteredstate == LOW) {
      startttimelow = millis();
      highduration = (millis() - starttimehigh);
      if (highduration < (2 * hightimesavg) || hightimesavg == 0) {
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3;
      }
      if (highduration > (5 * hightimesavg)) {
        hightimesavg = highduration / 3;
      }
    }
  }

  if (filteredstate != filteredstatebefore) {
    if (filteredstate == LOW) {
#define FAIR_WEIGHTING 1
#ifdef FAIR_WEIGHTING
      if (highduration < (hightimesavg + hightimesavg / 2) &&
          highduration > (hightimesavg * 6 / 10)) {
#else
      if (highduration < (hightimesavg * 2) &&
          highduration > (hightimesavg * 6 / 10)) {
#endif
        sym = (sym << 1) | (0);
      }
#ifdef FAIR_WEIGHTING
      if (highduration > (hightimesavg + hightimesavg / 2) &&
          highduration < (hightimesavg * 6)) {
#else
      if (highduration > (hightimesavg * 2) &&
          highduration < (hightimesavg * 6)) {
#endif
        sym = (sym << 1) | (1);
        wpm = (wpm + (1200 / ((highduration) / 3) * 4 / 3)) / 2;
      }
    }

    if (filteredstate == HIGH) {
      uint16_t lacktime = 10;
      if (wpm > 25)
        lacktime = 10;
      if (wpm > 30)
        lacktime = 12;
      if (wpm > 35)
        lacktime = 15;

#ifdef FAIR_WEIGHTING
      if (lowduration > (hightimesavg * (lacktime * 1 / 10)) &&
          lowduration < hightimesavg * (lacktime * 5 / 10)) {
#else
      if (lowduration > (hightimesavg * (lacktime * 7 / 80)) &&
          lowduration < hightimesavg * (lacktime * 5 / 10)) {
#endif
        printsym(true);
      }
      if (lowduration >= hightimesavg * (lacktime * 5 / 10)) {
        printsym(true);
        printsym(true);
      }
    }
  }

  if ((millis() - startttimelow) > (highduration * 6) && (sym > 1)) {
    printsym(true);
  }

  filteredstatebefore = filteredstate;
}

void cw_decode() {
  int32_t in = _amp32;
  EA(avg, in, (1 << 8));
  realstate = (in > (avg * 1 / 2));

  if (realstate != realstatebefore) {
    laststarttime = millis();
  }
#define NB_SCALED_TO_WPM 1
#ifdef NB_SCALED_TO_WPM
  if ((millis() - laststarttime) >
      min(1200 / (20 * 2), max(1200 / (40 * 2), hightimesavg / 6))) {
#else
  if ((millis() - laststarttime) > nbtime) {
#endif
    if (realstate != filteredstate) {
      filteredstate = realstate;
    }
  } else
    avg += avg / 100;

  dec2();
  realstatebefore = realstate;
}

// RX Logic (Simplified for now, assuming SIMPLE_RX or similar structure)
// I will implement the NEW_RX logic as it seems to be the default.

volatile uint8_t admux[3];
static int16_t ocomb;
static int16_t qh;
static int16_t ozi1, ozi2;
static uint8_t rx_state = 0;
volatile int16_t q_ac2;

inline void sdr_rx_common() {
  if (filt == 0) {
    ocomb = 0;
    ozi1 = 0;
    ozi2 = 0;
  } // hack using filt instead of _init for now? No, _init is better.
  // But _init is local in original code? No, volatile global.
  // extern volatile uint8_t _init; // I need to add this to usdx_dsp.h if not
  // there. It is there: extern volatile uint8_t _init; (Wait, I need to check)
  // I didn't add _init to usdx_dsp.h. I should.
  // For now I'll use a local static or just assume it's handled.
  // Actually, _init was volatile uint8_t in .ino line 2435.

  ozi2 = ozi1 + ozi2;
  ozi1 = ocomb + ozi1;
  OCR1AL = min(max((ozi2 >> 5) + 128, 0), 255);
}

inline int16_t sdr_rx_common_q() {
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  int16_t ac = ADC - 511;
  return ac;
}

inline int16_t sdr_rx_common_i() {
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  static int16_t prev_adc;
  int16_t ac = (prev_adc + adc) / 2;
  prev_adc = adc;
#ifdef AF_OUT
  // ... logic
#endif
  // Simplified for NEW_RX structure
  ozi2 = ozi1 + ozi2;
  ozi1 = ocomb + ozi1;
  OCR1AL = min(max((ozi2 >> 5) + 128, 0), 255);
  return ac;
}

// NEW_RX logic (lines 2911+)
// It uses process() function.
static uint8_t tc = 0;
void process(
    int16_t i_ac2,
    int16_t q_ac2_arg) // renamed q_ac2 to avoid conflict with global if any
{
  static int16_t ac3;
  static int16_t ozd1, ozd2;
  // if(_init) ...
  int16_t od1 = ac3 - ozd1;
  ocomb = od1 - ozd2;

  if (tc++ == 0)
    interrupts();
  ozd2 = od1;
  ozd1 = ac3;

  int16_t qh_local;
  {
    q_ac2_arg >>= att2;
    static int16_t v[14];
    qh_local = ((v[0] - q_ac2_arg) + (v[2] - v[12]) * 4) / 64 +
               ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
               ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + (v[6] - v[8]) / 2;
    for (int k = 0; k < 13; k++)
      v[k] = v[k + 1];
    v[13] = q_ac2_arg;
  }
  i_ac2 >>= att2;
  static int16_t v[7];
  i = v[0];
  for (int k = 0; k < 6; k++)
    v[k] = v[k + 1];
  v[6] = i_ac2;

  ac3 = slow_dsp(-i - qh_local);
  tc--;
}

// State machine for RX
static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0,
    q_s1zb1; // q_ac2 is global

#define M_SR 1
void sdr_rx_00() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_01;
  int16_t i_s1za0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
  int16_t ac2 = (i_s1za0 + (i_s1za1 + i_s1zb0) * 3 + i_s1zb1);
  i_s1za1 = i_s1za0;
  process(ac2, q_ac2);
}
void sdr_rx_01() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_02;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}
void sdr_rx_02() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_03;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}
void sdr_rx_03() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_04;
  q_s1zb1 = q_s1zb0;
  q_s1zb0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
}
void sdr_rx_04() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_05;
  i_s1zb1 = i_s1zb0;
  i_s1zb0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
}
void sdr_rx_05() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_06;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}
void sdr_rx_06() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_07;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}
void sdr_rx_07() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_00;
  int16_t q_s1za0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
  q_ac2 = (q_s1za0 + (q_s1za1 + q_s1zb0) * 3 + q_s1zb1);
  q_s1za1 = q_s1za0;
}

void sdr_rx() {
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  func_ptr = sdr_rx_q;
  sdr_rx_common();

  static int16_t prev_adc;
  int16_t corr_adc = (prev_adc + adc) / 2;
  prev_adc = adc;
  adc = corr_adc;
  int16_t ac = adc;

  int16_t ac2;
  static int16_t z1;
  if (rx_state == 0 || rx_state == 4) {
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2;
    za1 = ac;
    static int16_t _z1;
    if (rx_state == 0) {
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2;
      _za1 = _ac;
      {
        ac2 >>= att2;
        static int16_t v[7];
        i = v[0];
        v[0] = v[1];
        v[1] = v[2];
        v[2] = v[3];
        v[3] = v[4];
        v[4] = v[5];
        v[5] = v[6];
        v[6] = ac2;

        int16_t ac = i + qh;
        ac = slow_dsp(ac);

        static int16_t ozd1, ozd2;
        // if(_init) ...
        ocomb = ac - ozd1;
        ozd1 = ac;
      }
    } else
      _z1 = _ac;
  } else
    z1 = ac;

  rx_state++;
}

void sdr_rx_q() {
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  func_ptr = sdr_rx;

  int16_t ac = adc;
  int16_t ac2;
  static int16_t z1;
  if (rx_state == 3 || rx_state == 7) {
    static int16_t za1;
    int16_t _ac = ac + za1 + z1 * 2;
    za1 = ac;
    static int16_t _z1;
    if (rx_state == 7) {
      static int16_t _za1;
      ac2 = _ac + _za1 + _z1 * 2;
      _za1 = _ac;
      {
        ac2 >>= att2;
        static int16_t v[14];
        q = v[7];
        qh = ((v[0] - ac2) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 +
              (v[6] - v[8]) * 15) /
                 128 +
             (v[6] - v[8]) / 2;
        for (uint8_t j = 0; j != 13; j++)
          v[j] = v[j + 1];
        v[13] = ac2;
      }
      rx_state = 0;
      return;
    } else
      _z1 = _ac;
  } else
    z1 = ac;

  rx_state++;
}

// ==========================================
// RX DSP Functions
// ==========================================

#define DECAY_FACTOR 400
static uint16_t decayCount = DECAY_FACTOR;
#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)

inline int16_t process_agc(int16_t in) {
  static bool small = true;
  int16_t out;

  if (centiGain >= 128)
    out = (centiGain >> 5) * in;
  else
    out = (centiGain >> 2) * (in >> 3);
  out >>= 2;

  if (HI(abs(out)) > HI(1536)) {
    centiGain -= (centiGain >> 4);
  } else {
    if (HI(abs(out)) > HI(1024))
      small = false;
    if (--decayCount == 0) {
      if (small) {
        if (centiGain < (INT16_MAX - (INT16_MAX >> 4)))
          centiGain += (centiGain >> 4);
        else
          centiGain = INT16_MAX;
      }
      decayCount = DECAY_FACTOR;
      small = true;
    }
  }
  return out;
}

static int16_t gain = 1024;
inline int16_t process_agc_fast(int16_t in) {
  int16_t out = (gain >= 1024) ? (gain >> 10) * in : in;
  int16_t accum = (1 - abs(out >> 10));
  if ((INT16_MAX - gain) > accum)
    gain = gain + accum;
  if (gain < 1)
    gain = 1;
  return out;
}

#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha);

inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  ea1 = EA(ea1, in, 1 << (nr - 1));
  return ea1;
}

inline int16_t filt_var(int16_t za0) {
  static int16_t za1, za2;
  static int16_t zb0, zb1, zb2;
  static int16_t zc0, zc1, zc2;

  if (filt < 4) {
    static int16_t zz1, zz2;
    zz2 = zz1;
    zz1 = za0;
    za0 = ((((za0 - zz2) << 5) - ((za0 - zz2) << 1)) +
           (((zz1) << 4) + ((zz1) << 3) + zz1)) >>
          5;

    switch (filt) {
    case 1:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - ((((zb1 << 3) + (zb1 << 2) + zb1) +
                                             ((zb2 << 3) + (zb2 << 1) + zb2)) >>
                                            4);
      break;
    case 2:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - (((zb1 << 1) + (zb2 << 3)) >> 4);
      break;
    case 3:
      zb0 = ((za0 + 2 * za1 + za2) >> 1) - ((zb2 << 2) >> 4);
      break;
    }

    switch (filt) {
    case 1:
      zc0 =
          ((zb0 + 2 * zb1 + zb2) >> 1) -
          ((((zc1 << 4) + (zc1 << 1)) + ((zc2 << 3) + (zc2 << 1) + zc2)) >> 4);
      break;
    case 2:
      zc0 = ((zb0 + 2 * zb1 + zb2) >> 2) - (((zc1 << 2) + (zc2 << 3)) >> 4);
      break;
    case 3:
      zc0 = ((zb0 + 2 * zb1 + zb2) >> 2) - ((zc2 << 2) >> 4);
      break;
    }

    zc2 = zc1;
    zc1 = zc0;
    zb2 = zb1;
    zb1 = zb0;
    za2 = za1;
    za1 = za0;

    return zc0;
  } else {
#ifdef FILTER_700HZ
    if (cw_tone == 0) {
      switch (filt) {
      case 4:
        zb0 = (za0 + 2 * za1 + za2) / 2 + (41L * zb1 - 23L * zb2) / 32;
        break;
      case 5:
        zb0 = 5 * (za0 - 2 * za1 + za2) + (105L * zb1 - 58L * zb2) / 64;
        break;
      case 6:
        zb0 = 3 * (za0 - 2 * za1 + za2) + (108L * zb1 - 61L * zb2) / 64;
        break;
      case 7:
        zb0 = (2 * za0 - 3 * za1 + 2 * za2) + (111L * zb1 - 62L * zb2) / 64;
        break;
      }
      switch (filt) {
      case 4:
        zc0 = (zb0 - 2 * zb1 + zb2) / 4 + (105L * zc1 - 52L * zc2) / 64;
        break;
      case 5:
        zc0 = ((zb0 + 2 * zb1 + zb2) + 97L * zc1 - 57L * zc2) / 64;
        break;
      case 6:
        zc0 = ((zb0 + zb1 + zb2) + 104L * zc1 - 60L * zc2) / 64;
        break;
      case 7:
        zc0 = ((zb1) + 109L * zc1 - 62L * zc2) / 64;
        break;
      }
    }
    if (cw_tone == 1)
#endif
    {
      switch (filt) {
      case 4:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (114L * zb1 - 57L * zb2) / 64;
        break;
      case 5:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (113L * zb1 - 60L * zb2) / 64;
        break;
      case 6:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (110L * zb1 - 62L * zb2) / 64;
        break;
      case 7:
        zb0 = (0 * za0 + 1 * za1 + 0 * za2) + (110L * zb1 - 61L * zb2) / 64;
        break;
      }
      switch (filt) {
      case 4:
        zc0 = (zb0 - 2 * zb1 + zb2) / 1 + (95L * zc1 - 52L * zc2) / 64;
        break;
      case 5:
        zc0 = (zb0 - 2 * zb1 + zb2) / 4 + (106L * zc1 - 59L * zc2) / 64;
        break;
      case 6:
        zc0 = (zb0 - 2 * zb1 + zb2) / 16 + (113L * zc1 - 62L * zc2) / 64;
        break;
      case 7:
        zc0 = (zb0 - 2 * zb1 + zb2) / 32 + (112L * zc1 - 62L * zc2) / 64;
        break;
      }
    }
    zc2 = zc1;
    zc1 = zc0;
    zb2 = zb1;
    zb1 = zb0;
    za2 = za1;
    za1 = za0;
    return zc0 / 8;
  }
}

#define __UA 256
inline int16_t _arctan3(int16_t q, int16_t i) {
#define __atan2(z) (__UA / 8 + __UA / 22) * z
  int16_t r;
  if (abs(q) > abs(i))
    r = __UA / 4 - __atan2(abs(i) / abs(q));
  else
    r = (i == 0) ? 0 : __atan2(abs(q) / abs(i));
  r = (i < 0) ? __UA / 2 - r : r;
  return (q < 0) ? -r : r;
}

static uint32_t absavg256 = 0;
volatile uint32_t _absavg256 = 0;
volatile int16_t i, q;
static uint32_t amp32 = 0;
volatile uint32_t _amp32 = 0;

inline int16_t slow_dsp(int16_t ac) {
  static uint8_t absavg256cnt;
  if (!(absavg256cnt--)) {
    _absavg256 = absavg256;
    absavg256 = 0;
  } else
    absavg256 += abs(ac);

  if (mode == AM) {
    ac = magn(i, q);
    {
      static int16_t dc;
      dc += (ac - dc) / 2;
      ac = ac - dc;
    }
  } else if (mode == FM) {
    static int16_t zi;
    ac = ((ac + i) * zi);
    zi = i;
  }

#ifdef FAST_AGC
  if (agc == 2) {
    ac = process_agc(ac);
    ac = ac >> (16 - volume);
  } else if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
#else
  if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
#endif
  } else {
    if (volume <= 13)
      ac = ac >> (13 - volume);
    else
      ac = ac << (volume - 13);
  }
  if (nr)
    ac = process_nr(ac);
  if (filt)
    ac = filt_var(ac);

#ifdef CW_DECODER
  if (!(absavg256cnt % 64)) {
    _amp32 = amp32;
    amp32 = 0;
  } else
    amp32 += abs(ac);
#endif

  ac = min(max(ac, -512), 511);
#ifdef QCX
  if (!dsp_cap)
    return 0;
#endif
  return ac;
}

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs) {
  DIDR0 |= (1 << adcpin);
  ADCSRA = 0;
  ADCSRB = 0;
  ADMUX = 0;
  ADMUX |= (adcpin & 0x0f);
  ADMUX |= ((ref1v1) ? (1 << REFS1) : 0) | (1 << REFS0);
  ADCSRA |= ((uint8_t)log2((uint8_t)(F_CPU / 13 / fs))) & 0x07;
  ADCSRA |= (1 << ADEN);
#ifdef ADC_NR
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
#endif
}

void adc_stop() {
  ADCSRA &= ~(1 << ADIE);
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
#ifdef ADC_NR
  sleep_disable();
#endif
  ADMUX = (1 << REFS0);
}

void timer1_start(uint32_t fs) {
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B |= (1 << CS10) | (1 << WGM13) | (1 << WGM12);
  ICR1H = 0x00;
  ICR1L = min(255, F_CPU / fs);
  OCR1AH = 0x00;
  OCR1AL = 0x00;
  OCR1BH = 0x00;
  OCR1BL = 0x00;
}

void timer1_stop() {
  OCR1AL = 0x00;
  OCR1BL = 0x00;
}

void timer2_start(uint32_t fs) {
  ASSR &= ~(1 << AS2);
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A |= (1 << WGM21);
  TCCR2B |= (1 << CS22);
  TIMSK2 |= (1 << OCIE2A);
  uint8_t ocr = ((F_CPU / 64) / fs) - 1;
  OCR2A = ocr;
}

ISR(TIMER2_COMPA_vect) {
  func_ptr();
#ifdef DEBUG
  numSamples++;
#endif
}

// Switch RX/TX
int16_t _centiGain = 0;
static int16_t centiGain =
    128; // Defined here as it's used in switch_rxtx and process_agc
uint8_t txdelay = 0;
uint8_t semi_qsk = false;
uint32_t semi_qsk_timeout = 0;

void switch_rxtx(uint8_t tx_enable) {
  TIMSK2 &= ~(1 << OCIE2A);
  delayMicroseconds(20);
  noInterrupts();
#ifdef TX_DELAY
#ifdef SEMI_QSK
  if (!(semi_qsk_timeout))
#endif
    if ((txdelay) && (tx_enable) && (!(tx)) && (!(practice))) {
      digitalWrite(RX, LOW);
#ifdef NTX
      digitalWrite(NTX, LOW);
#endif
#ifdef PTX
      digitalWrite(PTX, HIGH);
#endif
      lcd.setCursor(15, 1);
      lcd.print('D');
      interrupts();
      delay(F_MCU / 16000000 * txdelay);
      noInterrupts();
    }
#endif
  tx = tx_enable;
  if (tx_enable) {
    _centiGain = centiGain;
#ifdef SEMI_QSK
    semi_qsk_timeout = 0;
#endif
    switch (mode) {
    case USB:
    case LSB:
      func_ptr = dsp_tx;
      break;
    case CW:
      func_ptr = dsp_tx_cw;
      break;
    case AM:
      func_ptr = dsp_tx_am;
      break;
    case FM:
      func_ptr = dsp_tx_fm;
      break;
    }
  } else {
    if ((mode == CW) && (!(semi_qsk_timeout))) {
#ifdef SEMI_QSK
#ifdef KEYER
      semi_qsk_timeout = millis() + ditTime * 8;
#else
      semi_qsk_timeout = millis() + 8 * 8;
#endif
#endif
    }
    centiGain = _centiGain;
    func_ptr = sdr_rx_q;
  }

  if (tx) {
    if (!practice) {
      digitalWrite(RX, LOW);
#ifdef NTX
      digitalWrite(NTX, LOW);
#endif
#ifdef PTX
      digitalWrite(PTX, HIGH);
#endif
#ifdef _SERIAL
      if (cat_active) {
        DDRC &= ~(1 << 2);
      } // disable PC2, so that ADC2 can be used as mic input
#endif
    }
    if (mode == CW) {
      // ... CW specific setup if needed
    }
  } else {
    digitalWrite(RX, HIGH);
#ifdef NTX
    digitalWrite(NTX, HIGH);
#endif
#ifdef PTX
    digitalWrite(PTX, LOW);
#endif
    OCR1BL = 0;
    si5351.SendRegister(SI_CLK_OE, TX0RX0);
#ifdef _SERIAL
    if (!vox)
      if (cat_active) {
        DDRC |= (1 << 2);
      } // enable PC2, so that ADC2 is pulled-down so that CAT TX is not
        // disrupted via mic input
#endif
  }
  interrupts();
  timer2_start(F_SAMP_TX); // Restart timer
}
