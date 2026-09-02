// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 6: menu declarativo integrado + UI + VOX.

#include "cat.h"
#include "cw.h"
#include "display.h"
#include "hw.h"
#include "i2c.h"
#include "menu.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"
#include "vfo.h"
#include <avr/eeprom.h>

SI5351 si5351;
LCD    lcd;
Menu   menu;

int32_t vfo_cache_freq          = 0;
void (*vfo_apply_freq)(int32_t) = NULL;

// --- Operador / control ------------------------------------------------
volatile uint8_t mode   = USB;
volatile int8_t  volume = 12;
volatile uint8_t agc    = 2;

volatile uint8_t txdelay  = 1; // TX relay/delay (ms)
volatile uint8_t practice = 0; // TX disabled
volatile uint8_t vox_tx   = 0; // VOX currently transmitting

// --- CW ---
volatile uint8_t keyer_speed = 25; // wpm
volatile uint8_t keyer_mode  = 0;  // 0=IambicA, 1=IambicB, 2=Straight

// --- CAT ---
volatile uint8_t prev_mode      = 0;
volatile uint8_t changedModeCAT = 0;

// --- Params (menú) ---
volatile uint8_t bandval   = 3; // band index (0-based; 40m default)
volatile uint8_t smode     = 1; // S-meter mode
volatile uint8_t backlight = 1;
volatile uint8_t rx_ph_q   = 90; // IQ phase
volatile uint8_t semi_qsk  = 0;  // semi-qsk
volatile uint8_t cwdec     = 1;  // CW decoder
volatile uint8_t vfosel    = 0;  // VFO A/B
volatile int32_t rit       = 0;  // RIT offset (Hz, set by CAT)
volatile int32_t rit_off   = 0;  // (reserved)
volatile uint8_t rit_on    = 0;  // RIT on/off (menu)

// PA bias
volatile uint8_t pwm_min = 115;
volatile uint8_t pwm_max = 220;

// --- VFO ---
volatile int32_t freq             = 7100000;
volatile uint8_t stepsize         = 3; // 0=10Hz 1=100 2=1k 3=10k
const int32_t    step_mult[]      = {10, 100, 1000, 10000};
volatile uint8_t semi_qsk_timeout = 0;

// --- Labels (enum arrays) ---
const char* const offon_label[2] PROGMEM     = {"OFF", "ON"};
const char* const mode_label[5] PROGMEM      = {"LSB", "USB", "CW ", "FM ", "AM "};
const char* const filt_label[8] PROGMEM      = {"Full", "3000", "2400", "1800", "500", "200", "100", "50"};
const char* const band_label[7] PROGMEM      = {"x", "80m", "60m", "40m", "30m", "20m", "17m"};
const char* const stepsize_label[10] PROGMEM = {"10M", "1M", ".5M", "100k", "10k", "1k", ".5k", "100", "10", "1"};
const char* const vfosel_label[2] PROGMEM    = {"A", "B"};
const char* const agc_label[3] PROGMEM       = {"OFF", "Fast", "Slow"};
const char* const att_label[8] PROGMEM       = {"0dB", "-13dB", "-20dB", "-33dB", "-40dB", "-53dB", "-60dB", "-73dB"};
const char* const smode_label[7] PROGMEM     = {"OFF", "dBm", "S", "Sbar", "wpm", "Vss", "time"};
const char* const lowcut_label[4] PROGMEM    = {"Off", "100", "200", "400"};

// --- EEPROM helpers (stable slots) ---
volatile uint16_t eeprom_offs = 0x150;
void              menu_eeprom_load(uint8_t eslot, void* ptr, uint8_t size) {
  eeprom_read_block(ptr, (const void*)(uint16_t)(eeprom_offs + eslot * 8), size);
}
void menu_eeprom_save(uint8_t eslot, const void* ptr, uint8_t size) {
  eeprom_write_block(ptr, (void*)(uint16_t)(eeprom_offs + eslot * 8), size);
}

// --- Callbacks (post-handling effects) ---
void on_mode() {
  if(mode != CW)
    stepsize = 3;
  else
    stepsize = 1;
  if(mode == CW) {
    filt = 4;
    nr   = 0;
  } else
    filt = 0;
  si5351.iqmsa = 0; // enforce PLL reset
}
void on_band() {
  vfo_save_current(); // store prev band freq/mode
  vfo_recall_band(bandval);
}
void on_vfosel() {
  // placeholder: single VFO in v2 minimal
}
void on_pwm() {
  // rebuild LUT with new bias limits
  for(uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (int16_t)(pwm_max - pwm_min)) / 255 + pwm_min;
}
void on_tx_quality() {} // no-op (kept for table symmetry)

// --- Table of menu entries (declarative; same order as legacy visible) ---
// Flash labels, indexed via MENU_LABELS[]. (PROGMEM: strings stay in flash)
const char* const MENU_LABELS[] PROGMEM = { // keep order == label ids below
    "Vol",       "Mode",     "FilterBW",    "Band",     "Tune Rate", "VFO Mode",  "RIT",        "AGC",
    "NR",        "ATT",      "ATT2",        "S-Meter",  "AGC Dcy",   "Noise Blk", "CW Decoder", "Semi QSK",
    "Practice",  "VOX",      "Noise Gate",  "TX Drive", "TX Comp",   "TX Emph",   "TX Delay",   "EQ Bass",
    "EQ Treble", "TX LoCut", "PA bias min", "PA max",   "Ref frq",   "IQ phase",  "Light"};

void menu_print_label(uint8_t id) {
  const char* s = (const char*)pgm_read_ptr(&MENU_LABELS[id]);
  while(char c = pgm_read_byte(s++))
    lcd.print(c);
}

const MenuParam MENU[] PROGMEM = {
    {0, (void*)&volume, P_T8, -1, 16, NULL, 1, NULL},
    {1, (void*)&mode, P_ENUM, 0, 4, mode_label, 2, on_mode},
    {2, (void*)&filt, P_ENUM, 0, 7, filt_label, 3, NULL},
    {3, (void*)&bandval, P_T8, 1, 6, band_label, 4, NULL},
    {4, (void*)&stepsize, P_ENUM, 0, 9, stepsize_label, 5, NULL},
    {5, (void*)&vfosel, P_ENUM, 0, 1, vfosel_label, 6, on_vfosel},
    {6, (void*)&rit_on, P_ENUM, 0, 1, offon_label, 7, NULL},
    {7, (void*)&agc, P_ENUM, 0, 2, agc_label, 8, NULL},
    {8, (void*)&nr, P_T8, 0, 8, NULL, 9, NULL},
    {9, (void*)&att, P_ENUM, 0, 7, att_label, 10, NULL},
    {10, (void*)&att2, P_T8, 0, 16, NULL, 11, NULL},
    {11, (void*)&smode, P_ENUM, 0, 6, smode_label, 12, NULL},
    {12, (void*)&agc_decay, P_T8, 1, 16, NULL, 13, NULL},
    {13, (void*)&nb_enable, P_ENUM, 0, 1, offon_label, 14, NULL},
    {14, (void*)&cwdec, P_ENUM, 0, 1, offon_label, 15, NULL},
    {15, (void*)&semi_qsk, P_ENUM, 0, 1, offon_label, 16, NULL},
    {16, (void*)&practice, P_ENUM, 0, 1, offon_label, 17, NULL},
    {17, (void*)&vox, P_ENUM, 0, 1, offon_label, 18, NULL},
    {18, (void*)&vox_thresh, P_T8, 0, 255, NULL, 19, NULL},
    {19, (void*)&drive, P_T8, 0, 8, NULL, 20, NULL},
    {20, (void*)&comp_enable, P_ENUM, 0, 1, offon_label, 21, NULL},
    {21, (void*)&pre_emph, P_T8, 0, 3, NULL, 22, NULL},
    {22, (void*)&txdelay, P_T8, 0, 255, NULL, 23, NULL},
    {23, (void*)&eq_low, P_T8, -7, 7, NULL, 24, NULL},
    {24, (void*)&eq_high, P_T8, -7, 7, NULL, 25, NULL},
    {25, (void*)&tx_lowcut, P_ENUM, 0, 3, lowcut_label, 26, NULL},
    {26, (void*)&pwm_min, P_T8, 0, 254, NULL, 27, on_pwm},
    {27, (void*)&pwm_max, P_T8, 1, 255, NULL, 28, on_pwm},
    {28, (void*)&si5351.fxtal, P_T32, 14000000, 28000000, NULL, 29, NULL},
    {29, (void*)&rx_ph_q, P_T8, 0, 180, NULL, 30, NULL},
    {30, (void*)&backlight, P_ENUM, 0, 1, offon_label, 31, NULL},
};

const int8_t MENU_COUNT = 31; // number of entries above

// --- VFO / sintonia ---
volatile uint32_t last_band_save = 0;
inline void       do_tune() {
  int32_t d = encoder_val;
  if(d) {
    encoder_val = 0;
    freq += d * step_mult[stepsize];
    if(freq < 100000)
      freq = 100000;
    if(freq > 60000000)
      freq = 60000000;
    vfo_apply();
    // persist on tune (throttled: ~every 2s max)
    if(millis() - last_band_save > 2000) {
      vfo_save_current();
      last_band_save = millis();
    }
  }
}

void display_vfo() {
  lcd.setCursor(0, 0);
  const char* ml = (mode == LSB) ? "LSB" : (mode == USB) ? "USB" : (mode == CW) ? "CW " : (mode == FM) ? "FM " : "AM ";
  lcd.print(ml);
  lcd.print(" ");
  char b[11];
  ltoa(freq, b, 10);
  lcd.print(b);
  lcd.print("               ");
  lcd.setCursor(0, 1);
  lcd.print(tx ? "TX" : "RX");
  lcd.print(" ");
  // CW decoder line takes priority during RX CW
  if(mode == CW && cw_line[15] != ' ') {
    lcd.print(cw_line);
  } else {
    // S-meter: 6-segment bar from _absavg256 (SDR signal strength)
    int16_t db = (int16_t)(_absavg256 >> 10);
    db         = (db < 0) ? 0 : (db > 6) ? 6 : db;
    lcd.print("S");
    for(int8_t s = 0; s < 6; s++)
      lcd.print((s < db) ? '=' : '-');
  }
  lcd.print("               ");
}

void vfo_hw_apply(int32_t f) { si5351.freq(f, 0, 0); }

void setup() {
  vfo_apply_freq = vfo_hw_apply;
  digitalWrite(KEY_OUT, LOW);
  si5351.powerDown();

  MCUSR = 0;
  wdt_enable(WDTO_4S);
  ADMUX  = (1 << REFS0);
  PCICR  = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  initPins();
  Serial.begin(16000000ULL * 115200 / F_MCU); // CAT115K (corrected for 20MHz)
  delay(100);
  lcd.begin(16, 2);
  lcd.print("uSDX v2");
  delay(300);

  timer1_start(78125);
  vfo_eeprom_load();        // restore band memories
  vfo_recall_band(bandval); // apply current band freq/mode (or default)
  vfo_apply();              // hw freq from loaded/recalled value
  last_band_save = millis();

  admux[0] = 0;
  admux[1] = 1;
  admux[2] = 2;
  adc_start(admux[0], true, F_SAMP_RX);
  adc_stop();
  adc_start(admux[2], true, 192307);

  on_pwm(); // build lut from pwm_min/pwm_max
  encoder_setup();
  menu.begin();
  loadWPM(keyer_speed); // CW timing

  func_ptr = sdr_rx_00;
  rx_state = 0;
  timer2_start(F_SAMP_RX);
  display_vfo();
}

void loop() {
  wdt_reset();

  if(menu.state == MENU_MAIN) {
    do_tune(); // encoder = tuning
  }
  menu.process(); // nav/edits when in menu; re-enters on button

  // --- CW keyer & decoder ---
  if(mode == CW) {
    if(keyer_mode != 2) // not straight key
      keyer_process();
    if(cwdec && !tx) {
      cw_set_keyed(tx); // decoder sees key off (RX)
      cw_decode();
    }
  }

  if(!(millis() % 500) && menu.state == MENU_MAIN)
    display_vfo(); // periodic refresh

  // --- VOX based RX/TX ---
  if(vox && (mode == LSB || mode == USB)) {
    if(!vox_tx) {
      static uint8_t  vox_sample;
      static uint16_t vox_adc;
      if(vox_sample++ == 16) {
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >> MIC_ATTEN);
        vox_sample = 0;
        vox_adc    = 0;
      } else {
        vox_adc += analogSampleMic();
      }
      if(tx) {
        vox_tx = 1;
        switch_rxtx(255);
        display_vfo();
      }
    } else if(!tx) {
      switch_rxtx(0);
      vox_tx = 0;
      display_vfo();
    }
  }
  delay(1);
}

// Arduino serial event (CAT)
void serialEvent() { cat_serial_event(); }