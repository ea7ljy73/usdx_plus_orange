// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 6: menu declarativo integrado + UI + VOX.

#include "display.h"
#include "hw.h"
#include "i2c.h"
#include "menu.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"
#include <avr/eeprom.h>

SI5351 si5351;
LCD    lcd;
Menu   menu;

// --- Operador / control ------------------------------------------------
volatile uint8_t mode   = USB;
volatile int8_t  volume = 12;
volatile uint8_t agc    = 2;

volatile uint8_t txdelay  = 1; // TX relay/delay (ms)
volatile uint8_t practice = 0; // TX disabled
volatile uint8_t vox_tx   = 0; // VOX currently transmitting

// --- Params (menú) ---
volatile int8_t  bandval   = 1; // band index
volatile uint8_t smode     = 1; // S-meter mode
volatile uint8_t backlight = 1;
volatile uint8_t rx_ph_q   = 90; // IQ phase
volatile uint8_t semi_qsk  = 0;  // semi-qsk
volatile uint8_t cwdec     = 1;  // CW decoder
volatile uint8_t vfosel    = 0;  // VFO A/B
volatile uint8_t rit       = 0;  // RIT on/off
volatile int32_t rit_off   = 0;  // RIT offset (Hz)

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
  static const int32_t bandfreq[] = {0, 3600000, 5250000, 7100000, 10120000, 14100000, 18100000};
  freq                            = bandfreq[bandval] ? bandfreq[bandval] : freq;
  si5351.freq(freq, 0, 0);
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
const MenuParam MENU[] = {
    {"Vol", (void*)&volume, P_T8, -1, 16, NULL, 1, NULL},
    {"Mode", (void*)&mode, P_ENUM, 0, 4, mode_label, 2, on_mode},
    {"FilterBW", (void*)&filt, P_ENUM, 0, 7, filt_label, 3, NULL},
    {"Band", (void*)&bandval, P_T8, 1, 6, band_label, 4, NULL},
    {"Tune Rate", (void*)&stepsize, P_ENUM, 0, 9, stepsize_label, 5, NULL},
    {"VFO Mode", (void*)&vfosel, P_ENUM, 0, 1, vfosel_label, 6, on_vfosel},
    {"RIT", (void*)&rit, P_ENUM, 0, 1, offon_label, 7, NULL},
    {"AGC", (void*)&agc, P_ENUM, 0, 2, agc_label, 8, NULL},
    {"NR", (void*)&nr, P_T8, 0, 8, NULL, 9, NULL},
    {"ATT", (void*)&att, P_ENUM, 0, 7, att_label, 10, NULL},
    {"ATT2", (void*)&att2, P_T8, 0, 16, NULL, 11, NULL},
    {"S-Meter", (void*)&smode, P_ENUM, 0, 6, smode_label, 12, NULL},
    {"AGC Dcy", (void*)&agc_decay, P_T8, 1, 16, NULL, 13, NULL},
    {"Noise Blk", (void*)&nb_enable, P_ENUM, 0, 1, offon_label, 14, NULL},
    {"CW Decoder", (void*)&cwdec, P_ENUM, 0, 1, offon_label, 15, NULL},
    {"Semi QSK", (void*)&semi_qsk, P_ENUM, 0, 1, offon_label, 16, NULL},
    {"Practice", (void*)&practice, P_ENUM, 0, 1, offon_label, 17, NULL},
    {"VOX", (void*)&vox, P_ENUM, 0, 1, offon_label, 18, NULL},
    {"Noise Gate", (void*)&vox_thresh, P_T8, 0, 255, NULL, 19, NULL},
    {"TX Drive", (void*)&drive, P_T8, 0, 8, NULL, 20, NULL},
    {"TX Comp", (void*)&comp_enable, P_ENUM, 0, 1, offon_label, 21, NULL},
    {"TX Emph", (void*)&pre_emph, P_T8, 0, 3, NULL, 22, NULL},
    {"TX Delay", (void*)&txdelay, P_T8, 0, 255, NULL, 23, NULL},
    {"EQ Bass", (void*)&eq_low, P_T8, -7, 7, NULL, 24, NULL},
    {"EQ Treble", (void*)&eq_high, P_T8, -7, 7, NULL, 25, NULL},
    {"TX LoCut", (void*)&tx_lowcut, P_ENUM, 0, 3, lowcut_label, 26, NULL},
    {"PA bias min", (void*)&pwm_min, P_T8, 0, 254, NULL, 27, on_pwm},
    {"PA max", (void*)&pwm_max, P_T8, 1, 255, NULL, 28, on_pwm},
    {"Ref frq", (void*)&si5351.fxtal, P_T32, 14000000, 28000000, NULL, 29, NULL},
    {"IQ phase", (void*)&rx_ph_q, P_T8, 0, 180, NULL, 30, NULL},
    {"Light", (void*)&backlight, P_ENUM, 0, 1, offon_label, 31, NULL},
};

const int8_t MENU_COUNT = 31; // number of entries above

// --- VFO / sintonia ---
inline void do_tune() {
  int32_t d = encoder_val;
  if(d) {
    encoder_val = 0;
    freq += d * step_mult[stepsize];
    if(freq < 100000)
      freq = 100000;
    if(freq > 60000000)
      freq = 60000000;
    si5351.freq(freq, 0, 0);
  }
}

void display_vfo() {
  lcd.setCursor(0, 0);
  lcd.print(mode == 1 ? "USB" : (mode == 0 ? "LSB" : (mode == 2 ? "CW " : "X  ")));
  lcd.print(" ");
  char b[11];
  ltoa(freq, b, 10);
  lcd.print(b);
  lcd.print("               ");
  lcd.setCursor(0, 1);
  lcd.print(tx ? "TX" : "RX");
  lcd.print(" ");
  lcd.print(att);
  lcd.print("              ");
}

void setup() {
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
  delay(100);
  lcd.begin(16, 2);
  lcd.print("uSDX v2");
  delay(300);

  timer1_start(78125);
  si5351.freq(freq, 0, 0);

  admux[0] = 0;
  admux[1] = 1;
  admux[2] = 2;
  adc_start(admux[0], true, F_SAMP_RX);
  adc_stop();
  adc_start(admux[2], true, 192307);

  on_pwm(); // build lut from pwm_min/pwm_max
  encoder_setup();
  menu.begin();

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