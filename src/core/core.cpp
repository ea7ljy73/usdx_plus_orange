#include "core.h"
#include "../driver/display.h"
#include "../driver/si5351.h"
#include "../dsp/dsp.h"
#include "../hal/hal.h"
#include "../hardware/io_expander.h"
#include "../hardware/wire.h"
#include "../interface/cat.h"
#include "../usdx_settings.h"
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

extern char __bss_end;
// SI5351 si5351; // Moved to src/driver/si5351.cpp
uint8_t inv = 1;

int freeMemory() {
  char *sp = reinterpret_cast<char *>(SP);
  return sp - &__bss_end;
}

#ifdef CAT_EXT
volatile uint8_t cat_key = 0;
uint8_t _digitalRead(uint8_t pin) {
  serialEvent(); // allows CAT update
  if (cat_key) {
    return (pin == BUTTONS) ? ((cat_key & 0x07) > 0)
           : (pin == DIT)   ? ~cat_key & 0x10
           : (pin == DAH)   ? ~cat_key & 0x20
                            : 0;
  }
  return digitalRead(pin);
}
#else
uint8_t _digitalRead(uint8_t pin) { return digitalRead(pin); }
#endif

// Keyer Globals
int keyer_speed = 25;
// ditTime is extern in dsp.h
uint8_t keyerControl;
uint8_t keyerState;
uint8_t keyer_mode = 2; //->  SINGLE
uint8_t keyer_swap = 0; //->  DI/DAH

uint32_t ktimer;
int Key_state;
extern int debounce; // extern in header, defined here? No, 'int debounce;' was
                     // in line 43.

int debounce;

void update_PaddleLatch() {
  if (_digitalRead(DIT) == LOW) {
    keyerControl |= keyer_swap ? DAH_L : DIT_L;
  }
  if (_digitalRead(DAH) == LOW) {
    keyerControl |= keyer_swap ? DIT_L : DAH_L;
  }
}

void loadWPM(int wpm) {
#if (F_MCU != 20000000)
  ditTime = (1200ULL * F_MCU / 16000000) / wpm;
#else
  ditTime = (1200 * 5 / 4) / wpm;
#endif
}

static uint8_t practice = false;

volatile uint8_t cat_active = 0;
volatile uint32_t rxend_event = 0;
volatile uint8_t vox = 0;

uint8_t backlight = 8;

volatile int8_t encoder_val = 0;
volatile int8_t encoder_step = 0;
static uint8_t last_state;

ISR(PCINT2_vect) {
  switch (last_state = (last_state << 4) | (_digitalRead(ROT_B) << 1) |
                       _digitalRead(ROT_A)) {
#ifdef ENCODER_ENHANCED_RESOLUTION
  case 0x31:
  case 0x10:
  case 0x02:
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
  case 0x20:
  case 0x01:
  case 0x13:
    encoder_val--;
    break;
#else
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
    encoder_val--;
    break;
#endif
  }
}

void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23);
  PCICR |= (1 << PCIE2);
  last_state = (_digitalRead(ROT_B) << 1) | _digitalRead(ROT_A);
  interrupts();
}

uint8_t log2(uint16_t x) {
  uint8_t y = 0;
  for (; x >>= 1;)
    y++;
  return y;
}

ISR(TIMER2_COMPA_vect) {
  func_ptr();
#ifdef DEBUG
  numSamples++;
#endif
}

uint16_t analogSampleMic() {
  uint16_t adc;
  noInterrupts();
  ADCSRA = (1 << ADEN) |
           (((uint8_t)log2((uint8_t)(F_CPU / 13 / (192307 / 1)))) & 0x07);

  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(RX, LOW);

  uint8_t oldmux = ADMUX;
  for (; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = admux[2];
  ADCSRA |= (1 << ADSC);
  for (; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = oldmux;

  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(RX, HIGH);

  adc = ADC;
  interrupts();
  return adc;
}

volatile bool change = true;
volatile int32_t freq = 14000000;
int32_t vfo[] = {7074000, 14074000};
uint8_t vfomode[] = {USB, USB};
volatile uint8_t vfosel = VFOA;
volatile int16_t rit = 0;

uint8_t smode = 2;
uint32_t max_absavg256 = 0;
int16_t dbm;

const uint16_t log10_lut[] = {0, 301, 477, 602, 699, 778, 845, 903, 954};

int32_t log10_fix(uint32_t n) {
  if (n == 0)
    return -32768;
  int32_t l = 0;
  uint32_t n_copy = n;
  while (n_copy >= 10) {
    n_copy /= 10;
    l++;
  }
  return l * 1000 + log10_lut[n_copy - 1];
}

static int16_t smeter_cnt = 0;

int16_t smeter(int16_t ref) {
  max_absavg256 = max(_absavg256, max_absavg256);

  if ((smode) && ((++smeter_cnt % 2048) == 0)) {
    int32_t log_val = log10_fix(max_absavg256);
    if (log_val > -32768) {
      int32_t dbm_scaled;
      if (dsp_cap == SDR) {
        dbm_scaled = (20 * log_val) / 1000 + 6 * att2 - 185;
      } else {
        dbm_scaled = (20 * log_val) / 1000 + 6 * att2 - 176;
      }
      dbm = dbm_scaled - ref;
    } else {
      dbm = -127 - ref;
    }

    lcd.noCursor();
    if (smode == 1) {
      lcd.setCursor(9, 0);
      lcd.print((int16_t)dbm);
      lcd.print(F("dBm "));
    }
    if (smode == 2) {
      uint8_t s =
          (dbm < -63) ? ((dbm - -127) / 6) : (((uint8_t)(dbm - -73)) / 10) * 10;
      lcd.setCursor(14, 0);
      if (s < 10)
        lcd.print('S');
      lcd.print(s);
    }
    if (smode == 3) {
      int8_t s =
          (dbm < -63) ? ((dbm - -127) / 6) : (((uint8_t)(dbm - -73)) / 10) * 10;
      char tmp[5];
      for (uint8_t i = 0; i != 4; i++) {
        tmp[i] = max(2, min(5, s + 1));
        s = s - 3;
      }
      tmp[4] = 0;
      lcd.setCursor(12, 0);
      lcd.print(tmp);
    }
#ifdef CW_DECODER
    if (smode == 4) {
      lcd.setCursor(14, 0);
      if (mode == CW)
        lcd.print(wpm);
      lcd.print("  ");
    }
#endif
#ifdef VSS_METER
    if (smode == 5) {
#define R_VSS 1000
      uint8_t vss10 = (uint32_t)analogSafeRead(BUTTONS, true) * (R_VSS + 10) *
                      11 / (10 * 1024);
      lcd.setCursor(10, 0);
      lcd.print(vss10 / 10);
      lcd.print('.');
      lcd.print(vss10 % 10);
      lcd.print("V ");
    }
#endif
#ifdef CLOCK
    if (smode == 6) {
      uint32_t _s = (millis() * 16000000ULL / F_MCU) / 1000;
      uint8_t h = (_s / 3600) % 24;
      uint8_t m = (_s / 60) % 60;
      uint8_t s = (_s) % 60;
      lcd.setCursor(8, 0);
      lcd.print(h / 10);
      lcd.print(h % 10);
      lcd.print(':');
      lcd.print(m / 10);
      lcd.print(m % 10);
      lcd.print(':');
      lcd.print(s / 10);
      lcd.print(s % 10);
      lcd.print("  ");
    }
#endif
    stepsize_showcursor();
    max_absavg256 /= 2;
  }
  return dbm;
}

void start_rx() {
  _init = 1;
  rx_state = 0;
  func_ptr = sdr_rx_00;
  adc_start(2, true, F_ADC_CONV * 4);
  admux[2] = ADMUX;
  if (dsp_cap == SDR) {
#ifdef SWAP_RX_IQ
    adc_start(1, !(att == 1), F_ADC_CONV);
    admux[0] = ADMUX;
    adc_start(0, !(att == 1), F_ADC_CONV);
    admux[1] = ADMUX;
#else
    adc_start(0, !(att == 1), F_ADC_CONV);
    admux[0] = ADMUX;
    adc_start(1, !(att == 1), F_ADC_CONV);
    admux[1] = ADMUX;
#endif
  } else {
    adc_start(0, false, F_ADC_CONV);
    admux[0] = ADMUX;
    admux[1] = ADMUX;
  }
  timer1_start(F_SAMP_PWM);
  timer2_start(F_SAMP_RX);
  TCCR1A &= ~(1 << COM1B1);
  digitalWrite(KEY_OUT, LOW);
}

volatile int16_t _centiGain = 0;
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
      if (semi_qsk)
        func_ptr = dummy;
      else
        func_ptr = sdr_rx_00;
    } else {
      centiGain = _centiGain;
#ifdef SEMI_QSK
      semi_qsk_timeout = 0;
#endif
      func_ptr = sdr_rx_00;
    }
  }
  if ((!dsp_cap) && (!tx_enable) && vox)
    func_ptr = dummy;

  interrupts();
  if (tx_enable)
    ADMUX = admux[2];
  else
    _init = 1;
  rx_state = 0;
#ifdef CW_DECODER
  if ((cwdec) && (mode == CW)) {
    filteredstate = tx_enable;
    dec2();
  }
#endif

  if (tx_enable) {
    if (practice) {
      digitalWrite(RX, LOW);
      lcd.setCursor(15, 1);
      lcd.print('P');
      si5351.SendRegister(SI_CLK_OE, TX0RX0);
    } else {
      digitalWrite(RX, LOW);
#ifdef NTX
      digitalWrite(NTX, LOW);
#endif
#ifdef PTX
      digitalWrite(PTX, HIGH);
#endif
      lcd.setCursor(15, 1);
      lcd.print('T');
      if (mode == CW) {
        si5351.freq_calc_fast(-cw_offset);
        si5351.SendPLLRegisterBulk();
      }
#ifdef RIT_ENABLE
      else if (rit) {
        si5351.freq_calc_fast(0);
        si5351.SendPLLRegisterBulk();
      }
#endif
      si5351.SendRegister(SI_CLK_OE, TX1RX0);
      OCR1AL = 0x80;
      if ((!mox) && (mode != CW))
        TCCR1A &= ~(1 << COM1A1);
      TCCR1A |= (1 << COM1B1);
#ifdef _SERIAL
      if (cat_active) {
        DDRC &= ~(1 << 2);
      }
#endif
    }
  } else {
#ifdef KEY_CLICK
    if (OCR1BL != 0) {
      for (uint16_t i = 0; i != 31; i++) {
        OCR1BL = lut[pgm_read_byte_near(&ramp[i])];
        delayMicroseconds(60);
      }
    }
#endif
    TCCR1A |= (1 << COM1A1);
    TCCR1A &= ~(1 << COM1B1);
    digitalWrite(KEY_OUT, LOW);
    OCR1BL = 0;
#ifdef QUAD
#ifdef TX_CLK0_CLK1
    si5351.SendRegister(16, 0x0f);
    si5351.SendRegister(17, 0x0f);
#else
    si5351.SendRegister(18, 0x0f);
#endif
#endif
    si5351.SendRegister(SI_CLK_OE, TX0RX1);
#ifdef SEMI_QSK
    if ((!semi_qsk_timeout) || (!semi_qsk))
#endif
    {
      digitalWrite(RX, !(att == 2));
#ifdef NTX
      digitalWrite(NTX, HIGH);
#endif
#ifdef PTX
      digitalWrite(PTX, LOW);
#endif
    }
#ifdef RIT_ENABLE
    si5351.freq_calc_fast(rit);
    si5351.SendPLLRegisterBulk();
#else
    si5351.freq_calc_fast(0);
    si5351.SendPLLRegisterBulk();
#endif
#ifdef SWR_METER
    if (swrmeter > 0) {
      show_banner();
      lcd.print("                ");
    }
#endif
    lcd.setCursor(15, 1);
    lcd.print((vox) ? 'V' : 'R');
#ifdef _SERIAL
    if (!vox)
      if (cat_active) {
        DDRC |= (1 << 2);
      }
#endif
  }
  OCR2A = ((F_CPU / 64) / ((tx_enable) ? F_SAMP_TX : F_SAMP_RX)) - 1;
  TIMSK2 |= (1 << OCIE2A);
}

uint8_t rx_ph_q = 90;

#ifdef QCX
#ifdef CAL_IQ
int16_t cal_iq_dummy = 0;
void calibrate_iq() {
  smode = 1;
  lcd.setCursor(0, 0);
  lcd_blanks();
  lcd_blanks();
  digitalWrite(SIG_OUT, true);
  si5351.freq(freq, 0, 90);
  si5351.SendRegister(SI_CLK_OE, TX1RX1);
  float dbc;
  si5351.freqb(freq + 700);
  delay(100);
  dbc = smeter();
  si5351.freqb(freq - 700);
  delay(100);
  lcd.setCursor(0, 1);
  lcd.print("I-Q bal. 700Hz");
  lcd_blanks();
  for (; !_digitalRead(BUTTONS);) {
    wdt_reset();
    smeter(dbc);
  }
  for (; _digitalRead(BUTTONS);)
    wdt_reset();
  si5351.freqb(freq + 600);
  delay(100);
  dbc = smeter();
  si5351.freqb(freq - 600);
  delay(100);
  lcd.setCursor(0, 1);
  lcd.print("Phase Lo 600Hz");
  lcd_blanks();
  for (; !_digitalRead(BUTTONS);) {
    wdt_reset();
    smeter(dbc);
  }
  for (; _digitalRead(BUTTONS);)
    wdt_reset();
  si5351.freqb(freq + 800);
  delay(100);
  dbc = smeter();
  si5351.freqb(freq - 800);
  delay(100);
  lcd.setCursor(0, 1);
  lcd.print("Phase Hi 800Hz");
  lcd_blanks();
  for (; !_digitalRead(BUTTONS);) {
    wdt_reset();
    smeter(dbc);
  }
  for (; _digitalRead(BUTTONS);)
    wdt_reset();

  lcd.setCursor(9, 0);
  lcd_blanks();
  digitalWrite(SIG_OUT, false);
  si5351.SendRegister(SI_CLK_OE, TX0RX1);
  change = true;
}
#endif
#endif

uint8_t prev_bandval = 3;
uint8_t bandval = 3;

#ifdef CW_FREQS_QRP
uint32_t band[N_BANDS] = {1810000,  3560000,  5351500,  7030000,
                          10106000, 14060000, 18096000, 21060000,
                          24906000, 28060000, 50096000};
#else
#ifdef CW_FREQS_FISTS
uint32_t band[N_BANDS] = {1818000,  3558000,  5351500,  7028000,
                          10118000, 14058000, 18085000, 21058000,
                          24908000, 28058000, 50058000};
#else
uint32_t band[N_BANDS] = {1840000,  3573000,  5357000,  7074000,
                          10136000, 14074000, 18100000, 21074000,
                          24915000, 28074000, 50313000};
#endif
#endif

uint32_t stepsizes[10] = {10000000, 1000000, 500000, 100000, 10000,
                          1000,     500,     100,    10,     1};
volatile uint8_t stepsize = STEP_1k;
uint8_t prev_stepsize[] = {STEP_1k, STEP_500};

void process_encoder_tuning_step(int8_t steps) {
  int32_t stepval = stepsizes[stepsize];
  if (rit) {
    rit += steps * stepval;
    rit = max(-9999, min(9999, rit));
  } else {
    freq += steps * stepval;
    freq = max(1, min(999999999, freq));
  }
  change = true;
}

void stepsize_showcursor() {
  lcd.setCursor(stepsize + 1, 1);
  lcd.cursor();
}

void stepsize_change(int8_t val) {
  stepsize += val;
  if (stepsize < STEP_1M)
    stepsize = STEP_10;
  if (stepsize > STEP_10)
    stepsize = STEP_1M;
  if (stepsize == STEP_10k || stepsize == STEP_500k)
    stepsize += val;
  stepsize_showcursor();
}

void powerDown() {
  lcd.setCursor(0, 1);
  lcd.print(F("Power-off 73 :-)"));
  lcd_blanks();

  MCUSR = ~(1 << WDRF);
  wdt_disable();

  timer2_stop();
  timer1_stop();
  adc_stop();

  si5351.powerDown();
  delay(1500);

  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;
  TIMSK0 = 0;
  TIMSK1 = 0;
  TIMSK2 = 0;
  WDTCSR = 0;
  *digitalPinToPCMSK(BUTTONS) |= (1 << digitalPinToPCMSKbit(BUTTONS));
  *digitalPinToPCICR(BUTTONS) |= (1 << digitalPinToPCICRbit(BUTTONS));

  PRR = 0xff;
  lcd.noDisplay();
  PORTD &= ~0x08;

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  interrupts();
  sleep_bod_disable();
  sleep_cpu();
  sleep_disable();

  do {
    wdt_enable(WDTO_15MS);
    for (;;)
      ;
  } while (0);
}

void show_banner() {
  lcd.setCursor(0, 0);
#ifdef QCX
  lcd.print(F("QCX"));
  const char *cap_label[] = {"SSB", "DSP", "SDR"};
  if (ssb_cap || dsp_cap) {
    lcd.print('-');
    lcd.print(cap_label[dsp_cap]);
  }
#else
  lcd.print(F(ARID));
#endif
  lcd.print('\x01');
  lcd_blanks();
  lcd_blanks();
}

const char *vfosel_label[] = {"A", "B"};
const char *mode_label[5] = {"LSB", "USB", "CW ", "FM ", "AM "};

void display_vfo(int32_t f) {
  lcd.setCursor(0, 1);
  lcd.print((rit)                                       ? ' '
            : ((vfosel % 2) | ((vfosel == SPLIT) & tx)) ? '\x07'
                                                        : '\x06');

  int32_t scale = 10e6;
  if (rit) {
    f = rit;
    scale = 1e3;
    lcd.print(F("RIT "));
    lcd.print(rit < 0 ? '-' : '+');
  } else {
    if (f / scale == 0) {
      lcd.print(' ');
      scale /= 10;
    }
  }
  for (; scale != 1; f %= scale, scale /= 10) {
    lcd.print(abs(f / scale));
    if (scale == (int32_t)1e3 || scale == (int32_t)1e6)
      lcd.print(',');
  }

  lcd.print(' ');
  lcd.print(mode_label[mode]);
  lcd.print(' ');
  lcd.setCursor(15, 1);
  lcd.print((vox) ? 'V' : 'R');
}

volatile uint8_t event;
volatile uint8_t prev_menumode = 0;
volatile int8_t menu = 0;

uint8_t eeprom_version;
int eeprom_addr;

void printmenuid(uint8_t menuid) {
  static const char seperator[] = {'.', ' '};
  uint8_t ids[] = {(uint8_t)(menuid >> 4), (uint8_t)(menuid & 0xF)};
  for (int i = 0; i < 2; i++) {
    uint8_t id = ids[i];
    if (id >= 10) {
      id -= 10;
      lcd.print('1');
    }
    lcd.print(char('0' + id));
    lcd.print(seperator[i]);
  }
}

void printlabel(uint8_t action, uint8_t menuid,
                const __FlashStringHelper *label) {
  if (action == UPDATE_MENU) {
    lcd.setCursor(0, 0);
    printmenuid(menuid);
    lcd.print(label);
    lcd_blanks();
    lcd_blanks();
    lcd.setCursor(0, 1);
    if (menumode >= 2)
      lcd.print('>');
  } else {
    lcd.setCursor(0, 1);
    lcd.print(label);
    lcd.print(F(": "));
  }
}

void actionCommon(uint8_t action, uint8_t *ptr, uint8_t size) {
  switch (action) {
  case LOAD:
    eeprom_read_block((void *)ptr, (const void *)eeprom_addr, size);
    break;
  case SAVE:
    eeprom_write_block((const void *)ptr, (void *)eeprom_addr, size);
    break;
  case SKIP:
    break;
  }
  eeprom_addr += size;
}

#ifdef MENU_STR
static uint8_t pos = 0;
void paramAction(uint8_t action, char *value, uint8_t menuid,
                 const __FlashStringHelper *label, uint8_t size) {
  const uint8_t _min = ' ';
  const uint8_t _max = 'Z';
  switch (action) {
  case NEXT_CH:
    if (pos < size)
      pos++;
    action = UPDATE_MENU;
  case UPDATE:
  case UPDATE_MENU:
    if (menumode != 3)
      pos = 0;
    if (menumode == 2)
      menumode = 3;
    if (((value[pos] + encoder_val) < _min) ||
        ((value[pos] + encoder_val) == 0))
      value[pos] = _min;
    else if ((value[pos] + encoder_val) > _max)
      value[pos] = _max;
    else
      value[pos] = value[pos] + encoder_val;
    encoder_val = 0;

    printlabel(action, menuid, label);
    for (int i = 0; i != 13; i++) {
      char ch = value[(pos / 8) * 8 + i];
      if (ch)
        lcd.print(ch);
      else
        break;
    }
    lcd.print('\x01');
    lcd_blanks();
    lcd.setCursor((pos % 8) + (menumode >= 2), 1);
    lcd.cursor();
    break;
  case SAVE:
    for (uint8_t i = size; i > 0; i--) {
      if ((value[i - 1] == ' ') || (value[i - 1] == 0))
        value[i - 1] = 0;
      else
        break;
    }
  default:
    actionCommon(action, (uint8_t *)value, size);
    break;
  }
}
#endif

uint32_t save_event_time = 0;
uint8_t vox_tx = 0;
static uint8_t vox_sample = 0;
static uint16_t vox_adc = 0;

static uint8_t pwm_min = 0;
#ifdef QCX
static uint8_t pwm_max = 255;
#else
static uint8_t pwm_max = 128;
#endif

const char *offon_label[2] = {"OFF", "ON"};
#if (F_MCU > 16000000)
const char *filt_label[N_FILT + 1] = {"Full", "3000", "2400", "1800",
                                      "500",  "200",  "100",  "50"};
#else
const char *filt_label[N_FILT + 1] = {"Full", "2400", "2000", "1500",
                                      "500",  "200",  "100",  "50"};
#endif
const char *band_label[N_BANDS] = {"160m", "80m", "60m", "40m", "30m", "20m",
                                   "17m",  "15m", "12m", "10m", "6m"};
const char *stepsize_label[] = {"10M", "1M",   "0.5M", "100k", "10k",
                                "1k",  "0.5k", "100",  "10",   "1"};
const char *att_label[] = {"0dB",   "-13dB", "-20dB", "-33dB",
                           "-40dB", "-53dB", "-60dB", "-73dB"};
#ifdef CLOCK
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm", "Vss", "time"};
#else
#ifdef VSS_METER
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm", "Vss"};
#else
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm"};
#endif
#endif
#ifdef SWR_METER
const char *swr_label[] = {"OFF", "FWD-SWR", "FWD-REF", "VFWD-VREF"};
#endif
const char *cw_tone_label[] = {"700", "600"};
#ifdef KEYER
const char *keyer_mode_label[] = {"Iambic A", "Iambic B", "Straight"};
#endif
const char *agc_label[] = {"OFF", "Fast", "Slow"};

void initPins() {
  digitalWrite(SIG_OUT, LOW);
  digitalWrite(RX, HIGH);
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(SIDETONE, LOW);

  pinMode(SIDETONE, OUTPUT);
  pinMode(SIG_OUT, OUTPUT);
  pinMode(RX, OUTPUT);
  pinMode(KEY_OUT, OUTPUT);
#ifdef ONEBUTTON
  pinMode(BUTTONS, INPUT_PULLUP);
#else
  pinMode(BUTTONS, INPUT);
#endif
  pinMode(DIT, INPUT_PULLUP);
  pinMode(DAH, INPUT);

  digitalWrite(AUDIO1, LOW);
  digitalWrite(AUDIO2, LOW);
  pinMode(AUDIO1, INPUT);
  pinMode(AUDIO2, INPUT);

#ifdef NTX
  digitalWrite(NTX, HIGH);
  pinMode(NTX, OUTPUT);
#endif
#ifdef PTX
  digitalWrite(PTX, LOW);
  pinMode(PTX, OUTPUT);
#endif
#ifdef SWR_METER
  pinMode(PIN_FWD, INPUT);
  pinMode(PIN_REF, INPUT);
#endif
#ifdef OLED
  pinMode(PD4, OUTPUT);
  pinMode(PD5, OUTPUT);
#endif
}

void fatal(const __FlashStringHelper *msg, int value, char unit) {
  lcd.setCursor(0, 1);
  lcd.print('!');
  lcd.print('!');
  lcd.print(msg);
  if (unit != '\0') {
    lcd.print('=');
    lcd.print(value);
    lcd.print(unit);
  }
  lcd_blanks();
  delay(1500);
  wdt_reset();
}

void build_lut() {
  for (uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (pwm_max - pwm_min)) / 255 + pwm_min;
}

#ifdef SWR_METER
void readSWR() {
  int32_t v_FWD_raw = 0;
  int32_t v_REF_raw = 0;
  for (int i = 0; i <= 7; i++) {
    v_FWD_raw += analogRead(PIN_FWD);
    v_REF_raw += analogRead(PIN_REF);
    delay(5);
  }

  int32_t v_FWD_scaled = ((int64_t)v_FWD_raw * 57500) / 8184;
  int32_t v_REF_scaled = ((int64_t)v_REF_raw * 57500) / 8184;

  int32_t p_FWD_scaled = ((int64_t)v_FWD_scaled * v_FWD_scaled) / 1000000;
  int32_t p_REV_scaled = ((int64_t)v_REF_scaled * v_REF_scaled) / 1000000;

  int32_t VSWR_scaled;
  if (v_FWD_scaled <= v_REF_scaled) {
    VSWR_scaled = 9999;
  } else {
    VSWR_scaled = ((int64_t)(v_FWD_scaled + v_REF_scaled) * 100) /
                  (v_FWD_scaled - v_REF_scaled);
  }

  if (VSWR_scaled > 9999)
    VSWR_scaled = 9999;
  if (VSWR_scaled < 100)
    VSWR_scaled = 100;

  float p_FWD_float = (float)p_FWD_scaled / 100.0;
  float VSWR_float = (float)VSWR_scaled / 100.0;

  if (swrmeter == 1) { // FWD-SWR
    lcd.setCursor(0, 0);
    lcd.print(" ");
    lcd.print(p_FWD_float, 2);
    lcd.print("W SWR:");
    lcd.print(VSWR_float, 2);
  } else if (swrmeter == 2) { // FWD-REF
    lcd.setCursor(0, 0);
    lcd.print(" F:");
    lcd.print(p_FWD_float, 2);
    lcd.print("W R:");
    lcd.print((float)p_REV_scaled / 100.0, 2);
    lcd.print("W");
  } else if (swrmeter == 3) { // VFWD-VREF
    lcd.setCursor(0, 0);
    lcd.print(" F:");
    lcd.print((float)v_FWD_scaled / 10000.0, 2);
    lcd.print("V R:");
    lcd.print((float)v_REF_scaled / 10000.0, 2);
    lcd.print("V");
  }

  FWD = p_FWD_float;
  SWR = VSWR_float;
}
#endif
#ifndef _N
#define _N(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef NULL
#define NULL 0
#endif

// Re-implemented paramAction dispatcher
int8_t paramAction(uint8_t action, uint8_t id) {
  if ((action == SAVE) || (action == LOAD)) {
    eeprom_addr = EEPROM_OFFSET;
    for (uint8_t _id = 1; _id < id; _id++)
      paramAction(SKIP, _id);
  }
  if (id == ALL)
    for (id = 1; id != N_ALL_PARAMS + 1; id++)
      paramAction(action, id); // for all parameters

  switch (id) { // Visible parameters
  case VOLUME:
    paramAction(action, volume, 0x11, F("Volume"), NULL, -1, 16, false);
    break;
  case MODE:
    paramAction(action, mode, 0x12, F("Mode"), mode_label, 0,
                _N(mode_label) - 1, false);
    break;
  case FILTER:
    paramAction(action, filt, 0x13, F("Filter BW"), filt_label, 0,
                _N(filt_label) - 1, false);
    break;
  case BAND:
    paramAction(action, bandval, 0x14, F("Band"), band_label, 0,
                _N(band_label) - 1, false);
    break;
  case STEP:
    paramAction(action, stepsize, 0x15, F("Tune Rate"), stepsize_label, 0,
                _N(stepsize_label) - 1, false);
    break;
  case VFOSEL:
    paramAction(action, vfosel, 0x16, F("VFO Mode"), vfosel_label, 0,
                _N(vfosel_label) - 1, false);
    break;
#ifdef RIT_ENABLE
  case RIT:
    paramAction(action, rit, 0x17, F("RIT"), offon_label, 0, 1, false);
    break;
#endif
#ifdef FAST_AGC
  case AGC:
    paramAction(action, agc, 0x18, F("AGC"), agc_label, 0, _N(agc_label) - 1,
                false);
    break;
#else
  case AGC:
    paramAction(action, agc, 0x18, F("AGC"), offon_label, 0, 1, false);
    break;
#endif
  case NR:
    paramAction(action, nr, 0x19, F("NR"), NULL, 0, 8, false);
    break;
  case ATT:
    paramAction(action, att, 0x1A, F("ATT"), att_label, 0, 7, false);
    break;
  case ATT2:
    paramAction(action, att2, 0x1B, F("ATT2"), NULL, 0, 16, false);
    break;
  case SMETER:
    paramAction(action, smode, 0x1C, F("S-meter"), smode_label, 0,
                _N(smode_label) - 1, false);
    break;
#ifdef SWR_METER
  case SWRMETER:
    paramAction(action, swrmeter, 0x1D, F("SWR Meter"), swr_label, 0,
                _N(swr_label) - 1, false);
    break;
#endif
#ifdef CW_DECODER
  case CWDEC:
    paramAction(action, cwdec, 0x21, F("CW Decoder"), offon_label, 0, 1, false);
    break;
#endif
#ifdef FILTER_700HZ
  case CWTONE:
    if (dsp_cap)
      paramAction(action, cw_tone, 0x22, F("CW Tone"), cw_tone_label, 0, 1,
                  false);
    break;
#endif
#ifdef QCX
  case CWOFF:
    paramAction(action, cw_offset, 0x23, F("CW Offset"), NULL, 300, 2000,
                false);
    break;
#endif
#ifdef SEMI_QSK
  case SEMIQSK:
    paramAction(action, semi_qsk, 0x24, F("Semi QSK"), offon_label, 0, 1,
                false);
    break;
#endif
#if defined(KEYER) || defined(CW_MESSAGE)
  case KEY_WPM:
    paramAction(action, keyer_speed, 0x25, F("Keyer Speed"), NULL, 1, 60,
                false);
    break;
#endif
#ifdef KEYER
  case KEY_MODE:
    paramAction(action, keyer_mode, 0x26, F("Keyer Mode"), keyer_mode_label, 0,
                2, false);
    break;
  case KEY_PIN:
    paramAction(action, keyer_swap, 0x27, F("Keyer Swap"), offon_label, 0, 1,
                false);
    break;
#endif
  case KEY_TX:
    paramAction(action, practice, 0x28, F("Practice"), offon_label, 0, 1,
                false);
    break;
#ifdef VOX_ENABLE
  case VOX:
    paramAction(action, vox, 0x31, F("VOX"), offon_label, 0, 1, false);
    break;
  case VOXGAIN:
    paramAction(action, vox_thresh, 0x32, F("Noise Gate"), NULL, 0, 255, false);
    break;
#endif
    // case DRIVE:   paramAction(action, drive,   0x33, F("TX Drive"), NULL, 0,
    // 8, false); break; // drive might be undefined
#ifdef TX_DELAY
  case TXDELAY:
    paramAction(action, txdelay, 0x34, F("TX Delay"), NULL, 0, 255, false);
    break;
#endif
#ifdef MOX_ENABLE
  case MOX:
    paramAction(action, mox, 0x35, F("MOX"), NULL, 0, 2, false);
    break;
#endif
    /* FT8 block removed to avoid complexity if vars missing - re-enable if
     * needed */
  case PWM_MIN:
    paramAction(action, pwm_min, 0x81, F("PA Bias min"), NULL, 0, pwm_max - 1,
                false);
    break;
  case PWM_MAX:
    paramAction(action, pwm_max, 0x82, F("PA Bias max"), NULL, pwm_min, 255,
                false);
    break;
  case SIFXTAL:
    paramAction(action, si5351.fxtal, 0x83, F("Ref freq"), NULL, 14000000,
                28000000, false);
    break;
  case IQ_ADJ:
    paramAction(action, rx_ph_q, 0x84, F("IQ Phase"), NULL, 0, 180, false);
    break;
#ifdef CAL_IQ
  case CALIB:
    if (dsp_cap != SDR)
      paramAction(action, cal_iq_dummy, 0x85, F("IQ Test/Cal."), NULL, 0, 0,
                  false);
    break;
#endif
#ifdef DEBUG
  case SR:
    paramAction(action, sr, 0x91, F("Sample rate"), NULL, INT32_MIN, INT32_MAX,
                false);
    break;
  case CPULOAD:
    paramAction(action, cpu_load, 0x92, F("CPU load %"), NULL, INT32_MIN,
                INT32_MAX, false);
    break;
  case PARAM_A:
    paramAction(action, param_a, 0x93, F("Param A"), NULL, 0, UINT16_MAX,
                false);
    break;
  case PARAM_B:
    paramAction(action, param_b, 0x94, F("Param B"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
  case PARAM_C:
    paramAction(action, param_c, 0x95, F("Param C"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
#endif
  case BACKL:
    paramAction(action, backlight, 0xA1, F("Backlight"), offon_label, 0, 1,
                false);
    break;
  // Invisible parameters
  case FREQA:
    paramAction(action, vfo[VFOA], 0, NULL, NULL, 0, 0, false);
    break;
  case FREQB:
    paramAction(action, vfo[VFOB], 0, NULL, NULL, 0, 0, false);
    break;
  case MODEA:
    paramAction(action, vfomode[VFOA], 0, NULL, NULL, 0, 0, false);
    break;
  case MODEB:
    paramAction(action, vfomode[VFOB], 0, NULL, NULL, 0, 0, false);
    break;
  case VERS:
    paramAction(action, eeprom_version, 0, NULL, NULL, 0, 0, false);
    break;

  // Non-parameters
  case _NULL:
    menumode = 0;
    show_banner();
    change = true;
    break;
  default:
    if ((action == NEXT_MENU) && (id != N_PARAMS))
      id = paramAction(
          action,
          max(1 /*0*/, min(N_PARAMS, id + ((encoder_val > 0) ? 1 : -1))));
    break;
  }
  return id;
}
