// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 6: menu declarativo integrado + UI + VOX.

#include "cat.h"
#include "cw.h"
#include "display.h"
#include "hw.h"
#include "i2c.h"
#include "lpf.h"
#include "menu.h"
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"
#include "vfo.h"
#include <avr/eeprom.h>
#include <math.h>

SI5351 si5351;
LCD    lcd;
Menu   menu;

int32_t vfo_cache_freq          = 0;
void (*vfo_apply_freq)(int32_t) = NULL;

// --- Operador / control ------------------------------------------------
volatile uint8_t mode     = USB;
volatile int8_t  volume   = 12;
volatile uint8_t agc      = 1; // legacy default 1 (offon; w/o FAST_AGC)
volatile uint8_t txdelay  = 0; // legacy default 0 (ms)
volatile uint8_t practice = 0; // TX disabled
volatile uint8_t vox_tx   = 0; // VOX currently transmitting

// --- CW ---
volatile uint8_t keyer_speed = 25; // wpm
volatile uint8_t keyer_mode  = 2;  // 2=SINGLE (v1 default), 0=IambicA, 1=IambicB

// --- CAT ---
volatile uint8_t prev_mode      = 0;
volatile uint8_t changedModeCAT = 0;
volatile uint32_t rxend_event   = 0; // CAT: block LCD until this time (legacy 4438)

// --- Params (menú) ---
volatile uint8_t bandval   = 3;  // band index (0-based; 40m default)
volatile uint8_t smode     = 1;  // S-meter mode
volatile uint8_t backlight = 8;  // legacy default (was 1)
volatile uint8_t rx_ph_q   = 90; // IQ phase
volatile uint8_t semi_qsk  = 0;  // semi-qsk
volatile uint8_t cwdec     = 1;  // CW decoder
volatile uint8_t vfosel    = 0;  // VFO A/B
volatile int32_t rit       = 0;  // RIT offset (Hz, set by CAT)
volatile int32_t rit_off   = 0;  // (reserved)
volatile uint8_t rit_on    = 0;  // RIT on/off (menu)

// PA bias
volatile uint8_t pwm_min = 0;   // legacy default (0 for biasing BS170 directly)
volatile uint8_t pwm_max = 128; // legacy default non-QCX (128 for biasing BS170 directly)

// --- VFO ---
volatile int32_t  freq             = 14000000; // legacy initializer (3549)
volatile uint8_t  stepsize         = 5; // STEP_1k (legacy default; indices = step_t)
const int32_t     step_mult[]      = {10, 100, 1000, 10000};
volatile uint32_t semi_qsk_timeout = 0;

// --- Labels (enum arrays) - strings in FLASH (PROGMEM), pointer array
// in PROGMEM. Read with pgm_read_ptr -> print via __FlashStringHelper*
// LCD overload. Saves ~500B RAM vs RAM string literals.
static const char offon_label_0[] PROGMEM = "OFF";
static const char offon_label_1[] PROGMEM = "ON";
const char* const offon_label[2] PROGMEM = {offon_label_0, offon_label_1};

static const char mode_label_0[] PROGMEM = "LSB";
static const char mode_label_1[] PROGMEM = "USB";
static const char mode_label_2[] PROGMEM = "CW ";
static const char mode_label_3[] PROGMEM = "FM ";
static const char mode_label_4[] PROGMEM = "AM ";
const char* const mode_label[5] PROGMEM = {mode_label_0, mode_label_1, mode_label_2, mode_label_3, mode_label_4};

static const char filt_label_0[] PROGMEM = "Full";
static const char filt_label_1[] PROGMEM = "3000";
static const char filt_label_2[] PROGMEM = "2400";
static const char filt_label_3[] PROGMEM = "1800";
static const char filt_label_4[] PROGMEM = "500";
static const char filt_label_5[] PROGMEM = "200";
static const char filt_label_6[] PROGMEM = "100";
static const char filt_label_7[] PROGMEM = "50";
const char* const filt_label[8] PROGMEM = {filt_label_0, filt_label_1, filt_label_2, filt_label_3, filt_label_4, filt_label_5, filt_label_6, filt_label_7};

static const char keyer_mode_label_0[] PROGMEM = "Iambic A";
static const char keyer_mode_label_1[] PROGMEM = "Iambic B";
static const char keyer_mode_label_2[] PROGMEM = "Straight";
const char* const keyer_mode_label[3] PROGMEM = {keyer_mode_label_0, keyer_mode_label_1, keyer_mode_label_2};

static const char band_label_0[] PROGMEM = "160m";
static const char band_label_1[] PROGMEM = "80m";
static const char band_label_2[] PROGMEM = "60m";
static const char band_label_3[] PROGMEM = "40m";
static const char band_label_4[] PROGMEM = "30m";
static const char band_label_5[] PROGMEM = "20m";
static const char band_label_6[] PROGMEM = "17m";
static const char band_label_7[] PROGMEM = "15m";
static const char band_label_8[] PROGMEM = "12m";
static const char band_label_9[] PROGMEM = "10m";
static const char band_label_10[] PROGMEM = "6m";
const char* const band_label[11] PROGMEM = {band_label_0, band_label_1, band_label_2, band_label_3, band_label_4, band_label_5, band_label_6, band_label_7, band_label_8, band_label_9, band_label_10};

static const char stepsize_label_0[] PROGMEM = "10M";
static const char stepsize_label_1[] PROGMEM = "1M";
static const char stepsize_label_2[] PROGMEM = "0.5M";
static const char stepsize_label_3[] PROGMEM = "100k";
static const char stepsize_label_4[] PROGMEM = "10k";
static const char stepsize_label_5[] PROGMEM = "1k";
static const char stepsize_label_6[] PROGMEM = "0.5k";
static const char stepsize_label_7[] PROGMEM = "100";
static const char stepsize_label_8[] PROGMEM = "10";
static const char stepsize_label_9[] PROGMEM = "1";
const char* const stepsize_label[10] PROGMEM = {stepsize_label_0, stepsize_label_1, stepsize_label_2, stepsize_label_3, stepsize_label_4, stepsize_label_5, stepsize_label_6, stepsize_label_7, stepsize_label_8, stepsize_label_9};

static const char vfosel_label_0[] PROGMEM = "A";
static const char vfosel_label_1[] PROGMEM = "B";
const char* const vfosel_label[2] PROGMEM = {vfosel_label_0, vfosel_label_1};

static const char att_label_0[] PROGMEM = "0dB";
static const char att_label_1[] PROGMEM = "-13dB";
static const char att_label_2[] PROGMEM = "-20dB";
static const char att_label_3[] PROGMEM = "-33dB";
static const char att_label_4[] PROGMEM = "-40dB";
static const char att_label_5[] PROGMEM = "-53dB";
static const char att_label_6[] PROGMEM = "-60dB";
static const char att_label_7[] PROGMEM = "-73dB";
const char* const att_label[8] PROGMEM = {att_label_0, att_label_1, att_label_2, att_label_3, att_label_4, att_label_5, att_label_6, att_label_7};

static const char smode_label_0[] PROGMEM = "OFF";
static const char smode_label_1[] PROGMEM = "dBm";
static const char smode_label_2[] PROGMEM = "S";
static const char smode_label_3[] PROGMEM = "S-bar";
static const char smode_label_4[] PROGMEM = "wpm";
const char* const smode_label[5] PROGMEM = {smode_label_0, smode_label_1, smode_label_2, smode_label_3, smode_label_4};

static const char menu_label_0[] PROGMEM = "Volume";
static const char menu_label_1[] PROGMEM = "Mode";
static const char menu_label_2[] PROGMEM = "Filter BW";
static const char menu_label_3[] PROGMEM = "Band";
static const char menu_label_4[] PROGMEM = "Tune Rate";
static const char menu_label_5[] PROGMEM = "VFO Mode";
static const char menu_label_6[] PROGMEM = "RIT";
static const char menu_label_7[] PROGMEM = "AGC";
static const char menu_label_8[] PROGMEM = "NR";
static const char menu_label_9[] PROGMEM = "ATT";
static const char menu_label_10[] PROGMEM = "ATT2";
static const char menu_label_11[] PROGMEM = "S-meter";
static const char menu_label_12[] PROGMEM = "CW Decoder";
static const char menu_label_13[] PROGMEM = "Semi QSK";
static const char menu_label_14[] PROGMEM = "Keyer Speed";
static const char menu_label_15[] PROGMEM = "Keyer Mode";
static const char menu_label_16[] PROGMEM = "Keyer Swap";
static const char menu_label_17[] PROGMEM = "Practice";
static const char menu_label_18[] PROGMEM = "VOX";
static const char menu_label_19[] PROGMEM = "Noise Gate";
static const char menu_label_20[] PROGMEM = "TX Drive";
static const char menu_label_21[] PROGMEM = "CQ Interval";
static const char menu_label_22[] PROGMEM = "CQ Message";
static const char menu_label_23[] PROGMEM = "PA Bias min";
static const char menu_label_24[] PROGMEM = "PA Bias max";
static const char menu_label_25[] PROGMEM = "Ref freq";
static const char menu_label_26[] PROGMEM = "IQ Phase";
static const char menu_label_27[] PROGMEM = "Backlight";
static const char menu_label_28[] PROGMEM = "CESSB";
static const char menu_label_29[] PROGMEM = "TX Comp";
static const char menu_label_30[] PROGMEM = "TX Emph";
static const char menu_label_31[] PROGMEM = "EQ Bass";
static const char menu_label_32[] PROGMEM = "EQ Treble";
static const char menu_label_33[] PROGMEM = "TX LoCut";
const char* const MENU_LABELS[34] PROGMEM = {menu_label_0, menu_label_1, menu_label_2, menu_label_3, menu_label_4, menu_label_5, menu_label_6, menu_label_7, menu_label_8, menu_label_9, menu_label_10, menu_label_11, menu_label_12, menu_label_13, menu_label_14, menu_label_15, menu_label_16, menu_label_17, menu_label_18, menu_label_19, menu_label_20, menu_label_21, menu_label_22, menu_label_23, menu_label_24, menu_label_25, menu_label_26, menu_label_27, menu_label_28, menu_label_29, menu_label_30, menu_label_31, menu_label_32, menu_label_33};

void menu_print_label(uint8_t id) {
  lcd.print((const __FlashStringHelper*)pgm_read_ptr(&MENU_LABELS[id]));
}

// --- EEPROM helpers (stable slots) ---
volatile uint16_t eeprom_offs = 0x150;
#define EEPROM_MAGIC_OFF 0x140 // version signature (outside menu/vfo regions)
#define F_VER_ID 4             // bump when EEPROM layout/semantics change
void menu_eeprom_load(uint8_t eslot, void* ptr, uint8_t size) {
  eeprom_read_block(ptr, (const void*)(uint16_t)(eeprom_offs + eslot * 8), size);
}
void menu_eeprom_save(uint8_t eslot, const void* ptr, uint8_t size) {
  eeprom_write_block(ptr, (void*)(uint16_t)(eeprom_offs + eslot * 8), size);
}

// save the menu entry whose eslot matches (used by button post-handling, legacy
// paramAction(SAVE, id))
void save_menu_eslot(uint8_t eslot) {
  for(uint8_t i = 0; i < MENU_COUNT; i++) {
    MenuParam p;
    memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
    if(p.eslot == eslot && p.value) {
      uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : (p.type == P_TEXT) ? 48 : 1;
      menu_eeprom_save(eslot, p.value, sz);
      return;
    }
  }
}

// restore all menu parameters from EEPROM (call once at setup).
// Only loads if the persisted version signature matches this firmware; on a
// mismatched/empty EEPROM it leaves defaults AND writes the signature so that
// the next boot actually restores values (v1-style first-boot guard).
void menu_load_all() {
  uint8_t sig   = eeprom_read_byte((const uint8_t*)EEPROM_MAGIC_OFF);
  bool    valid = (sig == F_VER_ID);
  if(valid) {
    for(int8_t i = 0; i < MENU_COUNT; i++) {
      MenuParam p;
      memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
      if(p.eslot && p.value) {
        uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : (p.type == P_TEXT) ? 48 : 1;
        // Read raw; only apply when slot was actually written (not all 0xFF)
        uint8_t raw[48];
        menu_eeprom_load(p.eslot, raw, sz);
        bool never = true;
        for(uint8_t k = 0; k < sz; k++)
          if(raw[k] != 0xFF)
            never = false;
        if(!never) {
          // copy in and clamp to declared range (guards rewritten/garbage slots)
          memcpy(p.value, raw, sz);
          if(p.type == P_T8S) { // signed byte (volume)
            int8_t v = *(int8_t*)p.value;
            if(v < p.min)
              v = p.min;
            if(v > p.max)
              v = p.max;
            *(int8_t*)p.value = v;
          } else if(p.type == P_T8) { // unsigned byte
            uint8_t v = *(uint8_t*)p.value;
            if(v < p.min)
              v = p.min;
            if(v > p.max)
              v = p.max;
            *(uint8_t*)p.value = v;
          } else if(p.type == P_ENUM) {
            uint8_t v = *(uint8_t*)p.value;
            if(v < p.min)
              v = p.min;
            if(v > p.max)
              v = p.max;
            *(uint8_t*)p.value = v;
          }
        }
      }
    }
  } else {
    // virgin/mismatched EEPROM: persist current defaults (so the next boot
    // loads sane values), then claim the region.
    for(int8_t i = 0; i < MENU_COUNT; i++) {
      MenuParam p;
      memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
      if(p.eslot && p.value) {
        uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : 1;
        menu_eeprom_save(p.eslot, p.value, sz);
      }
    }
    eeprom_write_byte((uint8_t*)EEPROM_MAGIC_OFF, F_VER_ID);
  }
}

// --- Callbacks (post-handling effects) ---
uint8_t prev_stepsize[2] = {5, 6}; // {STEP_1k SSB, STEP_500 CW} legacy 3843
uint8_t prev_filt[2]     = {0, 4}; // {Full SSB, filter4 CW} legacy 2637
void    on_mode() { // legacy 5572-5580: MENU edit of Mode -> hard reset + vfomode save
  vfomode[vfosel % 2] = mode;
  vfo_eeprom_save(); // save vfomode (legacy 5574 MODEA/B)
  if(mode != CW)
    stepsize = STEP_1k;
  else
    stepsize = STEP_500;
  if(mode == CW) {
    filt = 4;
    nr   = 0;
  } else
    filt = 0;
  si5351.iqmsa = 0; // enforce PLL reset (legacy 5576)
  vfo_apply();
}
void on_band() { // legacy BAND edit (5581 + change handler 5682): freq = band[bandval]
  freq = (int32_t)pgm_read_dword(&band[bandval]);
  set_lpf(freq / 1000000UL);
  vfo_apply();
}
// VFO A/B memory (legacy 3550-3553; vfosel already declared above)
int32_t vfo[2]     = {7074000, 14074000}; // VFOA=40m, VFOB=20m (legacy defaults)
uint8_t vfomode[2] = {USB, USB};
void    on_vfosel() { // legacy 5585-5592
  uint8_t other   = !vfosel;
  vfo[vfosel]     = freq; // keep slots current (v2)
  vfomode[vfosel] = mode;
  vfosel          = other;
  freq            = vfo[vfosel];
  mode            = vfomode[vfosel];
  if(mode != CW)
    stepsize = STEP_1k;
  else
    stepsize = STEP_500;
  if(mode == CW) {
    filt = 4;
    nr   = 0;
  } else
    filt = 0;
  vfo_apply();
}
void on_pwm() { // legacy 5620-5622: build_lut on PWM_MIN/PWM_MAX edit
  if(pwm_max < pwm_min)
    pwm_max = pwm_min;
  for(uint16_t i = 0; i != 256; i++)
    lut[i] = (i * (int16_t)(pwm_max - pwm_min)) / 255 + pwm_min;
}
void on_rit() { // legacy 5594-5597
  stepsize = (rit) ? STEP_10 : STEP_500;
  vfo_apply();
}
void on_att() { // legacy 5600-5615: apply analog attenuator immediately
  noInterrupts();
  adc_start(0, !(att & 0x01), F_ADC_CONV);
  admux[0] = ADMUX;
  adc_start(1, !(att & 0x01), F_ADC_CONV);
  admux[1] = ADMUX;
  interrupts();
  digitalWrite(RX, !(att & 0x02)); // att bit1: -20dB via RX line
  pinMode(AUDIO1, (att & 0x04) ? OUTPUT : INPUT); // att bit2: -40dB terminate ADC
  pinMode(AUDIO2, (att & 0x04) ? OUTPUT : INPUT);
}
void on_sifxtal() { vfo_apply(); } // legacy 5616 change=true (re-apply freq with new fxtal)
void on_iq() { vfo_apply(); }      // legacy 5627 change=true (re-apply freq with new phase)
void on_tx_quality() {} // no-op (kept for table symmetry)
// legacy 5636-5643: KEY_WPM -> loadWPM, KEY_MODE -> keyerControl
void on_keyer_speed() { loadWPM(keyer_speed); }
void on_keyer_mode() { keyer_set_mode(keyer_mode); }


const MenuParam MENU[] PROGMEM = {
    {0, (void*)&volume, P_T8S, -1, 16, NULL, 1, NULL},
    {1, (void*)&mode, P_ENUM, 0, 4, mode_label, 2, on_mode},
    {2, (void*)&filt, P_ENUM, 0, 7, filt_label, 3, NULL},
    {3, (void*)&bandval, P_ENUM, 0, 10, band_label, 4, on_band}, // legacy BAND 0..10
    {4, (void*)&stepsize, P_ENUM, 0, 9, stepsize_label, 5, NULL},
    {5, (void*)&vfosel, P_ENUM, 0, 1, vfosel_label, 6, on_vfosel},
    {6, (void*)&rit, P_ENUM, 0, 1, offon_label, 7, on_rit}, // legacy RIT toggle (rit!=0 = active)
    {7, (void*)&agc, P_ENUM, 0, 1, offon_label, 8, NULL}, // legacy w/o FAST_AGC: offon 0..1
    {8, (void*)&nr, P_T8, 0, 8, NULL, 9, NULL},
    {9, (void*)&att, P_ENUM, 0, 7, att_label, 10, on_att},
    {10, (void*)&att2, P_T8, 0, 16, NULL, 11, NULL},
    {11, (void*)&smode, P_ENUM, 0, 4, smode_label, 12, NULL}, // legacy 0..4 (no CLOCK/VSS)
    // CW Decoder (CW_DECODER legacy 0x21)
    {12, (void*)&cwdec, P_ENUM, 0, 1, offon_label, 15, NULL},
    // Semi QSK (SEMIQSK legacy 0x24)
    {13, (void*)&semi_qsk, P_ENUM, 0, 1, offon_label, 16, NULL},
    // Keyer Speed / Mode / Swap (KEY_WPM/KEY_MODE/KEY_PIN legacy 0x25/0x26/0x27)
    {14, (void*)&keyer_speed, P_T8, 1, 60, NULL, 13, on_keyer_speed},
    {15, (void*)&keyer_mode, P_ENUM, 0, 2, keyer_mode_label, 14, on_keyer_mode},
    {16, (void*)&keyer_swap, P_ENUM, 0, 1, offon_label, 21, NULL}, // eslot unique (was dup 15)
    // Practice (KEY_TX legacy 0x28)
    {17, (void*)&practice, P_ENUM, 0, 1, offon_label, 17, NULL},
    // VOX / Noise Gate (VOX/VOXGAIN legacy 0x31/0x32)
    {18, (void*)&vox, P_ENUM, 0, 1, offon_label, 18, NULL},
    {19, (void*)&vox_thresh, P_T8, 0, 255, NULL, 19, NULL},
    // TX Drive (DRIVE legacy 0x33)
    {20, (void*)&drive, P_T8, 0, 8, NULL, 20, NULL},
    // CQ Interval / CQ Message (CWINTERVAL/CWMSG1 legacy 0x41/0x42)
    {21, (void*)&cw_msg_interval, P_T8, 0, 60, NULL, 25, NULL},
    {22, (void*)cw_msg[0], P_TEXT, 0, 0, NULL, 26, NULL},
    // PA Bias min/max (PWM_MIN/PWM_MAX legacy 0x81/0x82)
    {23, (void*)&pwm_min, P_T8, 0, 254, NULL, 27, on_pwm},
    {24, (void*)&pwm_max, P_T8, 1, 255, NULL, 28, on_pwm},
    // Ref freq / IQ phase (SIFXTAL/IQ_ADJ legacy 0x83/0x84)
    {25, (void*)&si5351.fxtal, P_T32, 14000000, 28000000, NULL, 29, on_sifxtal}, // eslot<=31 -> 0x248 (no VFO clash)
    {26, (void*)&rx_ph_q, P_T8, 0, 180, NULL, 30, on_iq},
    // Backlight (BACKL legacy 0xA1)
    {27, (void*)&backlight, P_ENUM, 0, 1, offon_label, 31, NULL},
    // TX quality (v2 improvements; default OFF -> legacy parity)
    {28, (void*)&cessb_enable, P_ENUM, 0, 1, offon_label, 22, NULL},
    {29, (void*)&comp_enable, P_ENUM, 0, 1, offon_label, 23, NULL},
    {30, (void*)&pre_emph, P_T8, 0, 3, NULL, 24, NULL},
    {31, (void*)&eq_low, P_T8S, -7, 7, NULL, 0, NULL},
    {32, (void*)&eq_high, P_T8S, -7, 7, NULL, 0, NULL},
    {33, (void*)&tx_lowcut, P_T8, 0, 3, NULL, 0, NULL},
};

const int8_t MENU_COUNT = 34; // number of entries above

// --- VFO / sintonia ---
uint32_t max_absavg256 = 0; // smeter peak (legacy 3560)
int16_t  smeter_cnt    = 0;
int16_t  dbm           = 0;

// S-meter as legacy (usdx-legazy:3565-3614); draws dBm (smode 1) or S (smode 2)
static int16_t smeter(int16_t ref = 0) {
  max_absavg256 = max(_absavg256, max_absavg256); // peak
  if(smode) {
    if((++smeter_cnt & 3) == 0) { // recompute dBm (log10) every 4th (2s), like
      // legacy %2048 per-loop, so the slow float log10 doesn't block the RX
      // ISR on every 500ms display refresh.
      float rms = (float)max_absavg256 * (float)(1 << att2);
      rms /= (256.0 * 1024.0 * (float)4 * 8.0 * 500.0 * 1.414 / (0.707 * 1.1)); // SDR const (legacy 3571)
      dbm = 10 * log10((rms * rms) / 50) + 30 - ref;
    }
    { // draw every call using cached dbm
      lcd.noCursor();
      if(smode == 1) { // dBm meter
        lcd.setCursor(9, 0);
        lcd.print((int16_t)dbm);
        lcd.print("dBm ");
      } else if(smode == 2) { // S-meter
        uint8_t s = (dbm < -63) ? ((dbm - -127) / 6) : (((uint8_t)(dbm - -73)) / 10) * 10;
        lcd.setCursor(14, 0);
        if(s < 10)
          lcd.print('S');
        lcd.print((int)s);
      } else if(smode == 3) { // S-bar (legacy 3584-3587, CGRAM fonts 2..5)
        int8_t s = (dbm < -63) ? ((dbm - -127) / 6) : (((uint8_t)(dbm - -73)) / 10) * 10;
        char   tmp[5];
        for(uint8_t i = 0; i != 4; i++) {
          tmp[i] = max(2, min(5, s + 1));
          s      = s - 3;
        }
        tmp[4] = 0;
        lcd.setCursor(12, 0);
        for(uint8_t i = 0; i != 4; i++)
          lcd.write(tmp[i]);  // glyphs 2..5 (bars)
      } else if(smode == 4) { // wpm (legacy 3590-3592, CW_DECODER)
        lcd.setCursor(14, 0);
        if(mode == CW)
          lcd.print((int)wpm);
        lcd.print("  ");
      }
      max_absavg256 /= 2; // peak hold/decay (legacy 3612)
    }
  }
  return dbm;
}

volatile uint32_t last_band_save = 0;
// legacy stepsize_change (3865-3870): indices = step_t, skip .5M/10k
void stepsize_change(int8_t val) {
  stepsize += val;
  if(stepsize < 1)
    stepsize = 9; // STEP_1..STEP_10M
  if(stepsize > 9)
    stepsize = 1;
  if(stepsize == 2 || stepsize == 4) // STEP_500k / STEP_10k
    stepsize += val;
  display_vfo();
}
inline void do_tune() {
  if(tx || vox_tx)
    return; // no tuning while transmitting (legacy parity)
  int32_t d = encoder_val;
  if(d) {
    encoder_val = 0;
    // note: stepsizes[10] (PROGMEM) matches the menu 0..9 range
    int32_t stepval = (stepsize < 10) ? (int32_t)pgm_read_dword(&stepsizes[stepsize]) : 1000;
    if(rit) { // RIT active: encoder tweaks the RIT offset (legacy 3849-3854)
      rit += d * stepval;
      rit = max(-9999, min(9999, rit));
    } else {
      freq += d * stepval;
      if(freq < 1) // legacy 3854 clamp
        freq = 1;
      if(freq > 999999999)
        freq = 999999999;
      vfo_apply();
      uint8_t f = freq / 1000000UL;
      set_lpf(f); // switch LPF band (legacy 5701)
      bandval = (f > 32) ? 10 : (f > 26) ? 9 : (f > 22) ? 8 : (f > 20) ? 7 : (f > 16) ? 6 : (f > 12) ? 5 : (f > 8) ? 4 : (f > 6) ? 3 : (f > 4) ? 2 : (f > 2) ? 1 : 0; // align bandval (legacy 5702)
    }
#ifdef RIT_ENABLE
    if(rit) { // apply RIT offset in real time (legacy 5712)
      si5351.freq_calc_fast(rit);
      si5351.SendPLLRegisterBulk();
    }
#endif
    // persist on tune (throttled: ~every 2s max)
    if(millis() - last_band_save > 2000) {
      vfo_save_current();
      last_band_save = millis();
    }
    display_vfo_line1(); // instant frequency update on screen (light, no smeter)
  }
}

void display_vfo_line1() { // light: only line 1 (freq/mode) - instant on tune
  lcd.setCursor(0, 1);
  lcd.print((rit) ? ' ' : ((vfosel % 2) ? '\x07' : '\x06')); // RIT/VFO A-B arrow (legacy 3939)
  int32_t f     = freq;
  int32_t scale = 10e6; // 10,000,000 (legacy)
  if(rit) {
    f     = rit;
    scale = 1e3; // RIT frequency (legacy 3942-3945)
    lcd.print("RIT ");
    lcd.print(rit < 0 ? '-' : '+');
  } else {
    if(f / scale == 0) { // initial space instead of zero (legacy 3947)
      lcd.print(' ');
      scale /= 10;
    }
  }
  for(; scale != 1; f %= scale, scale /= 10) {
    lcd.print((int)abs(f / scale));
    if(scale == 1000 || scale == 1000000)
      lcd.print(','); // thousands separator (legacy 3951)
  }
  lcd.print(' ');
  lcd.print((const __FlashStringHelper*)pgm_read_ptr(&mode_label[mode])); // legacy 3954
  lcd.print(' ');
  lcd.setCursor(15, 1);
  lcd.print((vox) ? 'V' : 'R'); // like legacy 3955
}

void display_vfo() {
  display_vfo_line1();
  // Line 0: banner uSDX+logo (cols 0-4) + S-meter (col 9+, legacy 3577-3587)
  lcd.setCursor(0, 0);
  lcd.print("uSDX");
  lcd.print('\x01'); // logo CGRAM (legacy 3926)
  lcd.print("    "); // gap before the meter
  smeter(); // draws according to smode (1=dBm, 2=S, 3=Sbar, 4=wpm)
  // CW decoder on line 0 (legacy 5187): right-aligned RX CW
  if(mode == CW && cwdec && cw_event && !tx) {
    lcd.setCursor(8, 0);
    lcd.print(cw_line + 8);
  }
  // stepsize cursor on the frequency line (like legacy stepsize_showcursor)
  if(menu.state == MENU_MAIN) {
    lcd.setCursor(stepsize + 1, 1);
    lcd.cursor();
  }
}

void vfo_hw_apply(int32_t f) { // legacy 5704-5710: mode-dependent IQ phase + CW offset
  if(mode == CW)
    si5351.freq(f + cw_offset, rx_ph_q, 0); // RX in CW-R (=LSB), correct for CW-tone offset
  else if(mode == LSB)
    si5351.freq(f, rx_ph_q, 0); // RX in LSB
  else
    si5351.freq(f, 0, rx_ph_q); // RX in USB, ...
}

void setup() {
  vfo_apply_freq = vfo_hw_apply;
  digitalWrite(KEY_OUT, LOW);
  // Backlight sanity test FIRST: if the MCU runs at all, PD3 goes HIGH before
  // anything else. Tells us software vs hardware immediately.
  DDRD |= 0x08;  // PD3 (backlight) output
  PORTD |= 0x08; // backlight ON

  si5351.powerDown();

  MCUSR = 0;
  wdt_enable(WDTO_4S);
  ADMUX  = (1 << REFS0);
  PCICR  = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  initPins();

  // LCD init BEFORE serial: PC0/PC1 (LCD D4/D5) share the UART pins, and the
  // legacy only re-enables serial carefully after the display is up.
  delay(100);
  wdt_reset();
  lcd.begin(16, 4);     // Init LCD (mismo que legacy)
  display_init_fonts(); // load CGRAM fonts (logo, VFO, S-bar)
  show_banner();        // uSDX + logo
  lcd.setCursor(0, 1);
  lcd.print(FIRMWARE_VERSION); // show version at boot
  lcd.print("                ");
  delay(300);
  wdt_reset();

  Serial.begin(16000000ULL * 38400 / F_MCU); // CAT 38400 (legacy 5116, no CAT_STREAMING)

  timer1_start(78125);
  vfo_eeprom_load();        // restore band memories
  menu.begin();
  drive = 4; // Init settings (legacy 5072); EEPROM restore overrides if valid
  cw_offset = tones[cw_tone]; // CW TX/RX offset (legacy 5079)
  menu_load_all(); // restore saved menu params (volume, mode, agc, drive, ...)
  on_pwm(); // build lut with LOADED pwm_min/max (legacy: build_lut after LOAD, 5105)
  vfo_recall_band(bandval); // apply current band freq/mode (or default)
  vfo_apply();              // hw freq with loaded rx_ph_q / cw_offset
  last_band_save = millis();
  encoder_setup();
  // Legacy parity (usdx-legazy:5084,5098): force factory-default reset when the
  // rotary-key is pressed at power-on, and always disable VOX on boot.
  // NOTE: use digitalRead(BUTTONS) here (like legacy 5084) - the ADC is not yet
  // enabled at this point in setup, so analog ADC read would block forever on ADIF.
  if(inv ^ digitalRead(BUTTONS)) { // left button pressed at power-on -> reset settings (legacy 5084)
    lcd.setCursor(0, 1);
    lcd.print("Reset settings..");
    for(uint8_t i = 0; i != MENU_COUNT; i++) { // re-persist defaults over EEPROM
      MenuParam p;
      memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
      if(p.eslot && p.value) {
        uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : 1;
        menu_eeprom_save(p.eslot, p.value, sz);
      }
    }
    eeprom_write_byte((uint8_t*)EEPROM_MAGIC_OFF, F_VER_ID);
    delay(500);
  }
  vox = 0;                    // disable VOX at boot (legacy parity)
  nr  = 0;                    // disable NR (legacy parity)
  loadWPM(keyer_speed);       // CW timing
  keyer_set_mode(keyer_mode); // initialize keyerControl (IAMBICA/B, SINGLE)

  start_rx(); // arm RX DSP (ADC I/Q/mic + timers + func_ptr) as legacy
  display_vfo();

#if KEYER
  // wait until DIH/DAH/PTT released to prevent TX on startup (v1 parity)
  {
    unsigned long key_wait_until = millis() + 2000;
    while(digitalRead(DIT) == LOW || ((mode == CW && keyer_mode != 2) && digitalRead(DAH) == LOW)) {
      wdt_reset();
      if(millis() > key_wait_until)
        break; // don't hang forever if a paddle is held
    }
  }
#endif
}

void loop() {
  wdt_reset();

  if(menu.state == MENU_MAIN) {
    // skip tuning while a button is held: dial hold+turn = VOLUME (legacy 5472)
    if(!(inv ^ digitalRead(BUTTONS)))
      do_tune();
  }
  menu.process(); // nav/edits when in menu; re-enters on button

  // --- CW keyer (legacy 5201-5285): iambic in CW, else SINGLE (DIT/DAH = PTT) ---
  if(mode == CW && keyer_mode != 2) {
    keyer_process(); // iambic A/B
  } else {
    uint8_t pin = ((mode == CW) && (keyer_swap)) ? DAH : DIT; // legacy 5269
    if(!vox_tx) {
      if(!digitalRead(pin)) { // PTT/DIT keys transmitter
        cw_msg_event = 0;
        switch_rxtx(1);
        do { // hold loop (legacy 5276-5283)
          wdt_reset();
          delay(10);
          if(inv ^ digitalRead(BUTTONS))
            break; // break if button pressed (prevent lock-up)
        } while(!digitalRead(pin)); // until released
        switch_rxtx(0);
      }
    }
  }
  if(mode == CW && cwdec && !tx && !semi_qsk_timeout)
    cw_decode(); // CW decoder only active during RX (legacy 5176)

#ifdef CW_MESSAGE
  if((mode == CW) && (cw_msg_event) && (millis() > cw_msg_event)) { // time to send a CW message (legacy 5724)
    if((cw_tx(cw_msg[cw_msg_id]) == 0) && ((cw_msg[cw_msg_id][0] == 'C') && (cw_msg[cw_msg_id][1] == 'Q')) && cw_msg_interval)
      cw_msg_event = millis() + 1000 * cw_msg_interval;
    else
      cw_msg_event = 0;
  }
#endif //CW_MESSAGE

  // --- Semi-QSK: delayed RX return after CW keying (legacy 5292) ---
  if((semi_qsk_timeout) && (millis() > semi_qsk_timeout)) {
    switch_rxtx(0);
  }

  if(!(millis() % 500) && menu.state == MENU_MAIN && !tx && !vox_tx)
    display_vfo(); // periodic refresh (legacy: skip while TX to avoid I2C conflict)

  // --- VOX based RX/TX (SSB only, legacy 5144) ---
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
      }
    } else if(!tx) {
      switch_rxtx(0);
      vox_tx = 0;
      delay(32); // legacy 5166
    }
  }
}

// Arduino serial event (CAT)
void serialEvent() { cat_serial_event(); }
