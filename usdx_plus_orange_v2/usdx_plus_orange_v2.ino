// usdx_plus_orange_v2.ino - uSDX Plus Orange v2 (modular)
// Paso 6: menu declarativo integrado + UI + VOX.

#include "cat.h"
#include "cw.h"
#include "display.h"
#include "hw.h"
#include "i2c.h"
#include "lpf.h"
#include "menu.h"
#include "perf.h" // Fase 0: medidor carga CPU (coste 0 sin PERF_METER)
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
volatile int32_t  last_full_freq   = 0; // base del último programado SI5351 completo (fast-tune)
volatile int8_t   last_tune_dir    = 1; // último sentido del dial (+1/-1, GW8RDI banda direccional)

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
static const char menu_label_28[] PROGMEM = "Mic Atten";
static const char menu_label_29[] PROGMEM = "DIGI Mode";
static const char menu_label_30[] PROGMEM = "TX Comp";
static const char menu_label_31[] PROGMEM = "TX LoCut";
static const char menu_label_32[] PROGMEM = "AGC Start";
static const char menu_label_33[] PROGMEM = "AGC Rec";
const char* const MENU_LABELS[34] PROGMEM = {menu_label_0, menu_label_1, menu_label_2, menu_label_3, menu_label_4, menu_label_5, menu_label_6, menu_label_7, menu_label_8, menu_label_9, menu_label_10, menu_label_11, menu_label_12, menu_label_13, menu_label_14, menu_label_15, menu_label_16, menu_label_17, menu_label_18, menu_label_19, menu_label_20, menu_label_21, menu_label_22, menu_label_23, menu_label_24, menu_label_25, menu_label_26, menu_label_27, menu_label_28, menu_label_29, menu_label_30, menu_label_31, menu_label_32, menu_label_33};

void menu_print_label(uint8_t id) {
  lcd.print((const __FlashStringHelper*)pgm_read_ptr(&MENU_LABELS[id]));
}

// --- EEPROM helpers (stable slots; stride x8 salvo texto 48B) ---
volatile uint16_t eeprom_offs = 0x150;
#define EEPROM_MAGIC_OFF 0x140 // version signature (outside menu/vfo regions)
#define EEPROM_TEXT_OFF 0x290 // CQ Message 48B (0x290..0x2BF; evita eslots 27..31)
#define EEPROM_XTRA_OFF 0x2C0 // eslots >= 32: 1B c/u (0x2C0+eslot-32; sin solape)
#define F_VER_ID 3             // bump when EEPROM layout/semantics change
void menu_eeprom_load(uint8_t eslot, void* ptr, uint8_t size) {
  if(eslot == 26) { // CQ Message: región propia 48B (no cabe en stride x8)
    eeprom_read_block(ptr, (const void*)EEPROM_TEXT_OFF, size);
    return;
  }
  if(eslot >= 32) { // banco extra 1B (stride x8 agotado en 0..31)
    eeprom_read_block(ptr, (const void*)(uint16_t)(EEPROM_XTRA_OFF + eslot - 32), size);
    return;
  }
  eeprom_read_block(ptr, (const void*)(uint16_t)(eeprom_offs + eslot * 8), size);
}
void menu_eeprom_save(uint8_t eslot, const void* ptr, uint8_t size) {
  if(eslot == 26) { // CQ Message: región propia 48B (evita pisar eslots 27..31)
    eeprom_update_block(ptr, (void*)EEPROM_TEXT_OFF, size);
    return;
  }
  if(eslot >= 32) { // banco extra 1B
    eeprom_update_block(ptr, (void*)(uint16_t)(EEPROM_XTRA_OFF + eslot - 32), size);
    return;
  }
  eeprom_update_block(ptr, (void*)(uint16_t)(eeprom_offs + eslot * 8), size);
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
        uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : (p.type == P_TEXT) ? 48 : 1;
        menu_eeprom_save(p.eslot, p.value, sz);
      }
    }
    eeprom_write_byte((uint8_t*)EEPROM_MAGIC_OFF, F_VER_ID);
  }
}

// --- Callbacks (post-handling effects) ---
extern volatile uint32_t save_event_time; // deferred VFO persist (defined below)
uint8_t prev_stepsize[2] = {5, 6}; // {STEP_1k SSB, STEP_500 CW} legacy 3843
uint8_t prev_filt[2]     = {0, 4}; // {Full SSB, filter4 CW} legacy 2637
void    on_mode() { // legacy 5572-5580: MENU edit of Mode -> hard reset + vfomode save
  vfomode[vfosel % 2] = mode;
  vfo_save_current(); // persist band slot + VFO state (legacy 5574 MODEA/B)
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
void on_band() { // legacy BAND edit (5581 + change handler 5682): recall band memory or default
  if(bandval >= 1 && bandval <= BANDCOUNT)
    vfo_recall_band(bandval); // freq/mode from band memory (or default band freq)
  else
    freq = (int32_t)pgm_read_dword(&band[bandval]); // 160m/6m: band default (legacy)
  set_lpf(freq / 1000000UL);
  vfo_apply();
}
// VFO A/B memory (legacy 3550-3553; vfosel already declared above)
int32_t vfo[2]     = {7074000, 14074000}; // VFOA=40m, VFOB=20m (legacy defaults)
uint8_t vfomode[2] = {LSB, USB}; // VFOA=LSB (40m), VFOB=USB (v1 G8RDI: was USB,USB)
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
  bandval_align(); // VFO may be on another band (legacy change block)
  save_event_time = millis() + 1000; // persist swapped VFO when idle (legacy change block)
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
    // Mic Atten (F3.6: runtime MIC_ATTEN, 6dB/step)
    {28, (void*)&mic_atten, P_T8, 0, 4, NULL, 22, NULL},
    // DIGI Mode (F3.7: flat TX path for digital modes)
    {29, (void*)&dig_mode, P_ENUM, 0, 1, offon_label, 23, NULL},
    // TX Comp (F3.9b: voice compressor 2:1)
    {30, (void*)&comp_enable, P_ENUM, 0, 1, offon_label, 24, NULL},
    // TX LoCut (F3.9c: HPF micro; eslot 32 = banco extra)
    {31, (void*)&tx_lowcut, P_T8, 0, 3, NULL, 32, NULL},
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
    // AGC Start (F4.16: precarga x8 al arrancar; eslot 33 = banco extra 0x2C1)
    {32, (void*)&agc_start, P_ENUM, 0, 1, offon_label, 33, NULL},
    // AGC Rec (F4.17: divisor de recovery 1..8, 1=legacy; eslot 34 = banco extra 0x2C2)
    {33, (void*)&agc_rec, P_T8, 1, 8, NULL, 34, NULL},
};

const int8_t MENU_COUNT = 34; // number of entries above

// --- VFO / sintonia ---
uint32_t max_absavg256 = 0; // smeter peak (legacy 3560)
int16_t  smeter_cnt    = 0;
int16_t  dbm           = 0;

// 20*log10 LUTs (Fase 1: S-meter sin float; maxerr 0.61dB vs exacto, verificado
// en host; legacy truncaba float con hasta ~1dB de error + UB con att2=16)
static const uint8_t  SM_FRAC[128] PROGMEM = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,27,28,29,30,31,32,33,34,34,35,36,37,38,39,39,40,41,42,43,43,44,45,46,47,47,48,49,50,50,51,52,53,53,54,55,56,56,57,58,59,59,60,61,61,62,63,63,64,65,65,66,67,67,68,69,69,70,71,71,72,73,73,74,75,75,76,77,77,78,78,79,80,80,81,81,82,83,83,84,84,85,86,86,87,87,88,89,89,90,90,91,91,92,92,93,94,94,95,95,96};
static const uint16_t SM_EXP[32] PROGMEM = {0,96,193,289,385,482,578,674,771,867,963,1060,1156,1252,1349,1445,1541,1638,1734,1830,1927,2023,2119,2216,2312,2408,2505,2601,2697,2794,2890,2986};
static const uint16_t SM_ATT[17] PROGMEM = {0,96,193,289,385,482,578,674,771,867,963,1060,1156,1252,1349,1445,1541};
#define SM_C 2954 // round(184.636*16): 20*log10(K)-(30-10*log10(50)), K = SDR const

// 20*log10(M) en 1/16 dB (M>=1). Equivale a la fórmula float de legacy 3571:
//   rms = M*2^att2 / (256*1024*4*8*500*1.414/(0.707*1.1))
//   dbm = 10*log10(rms*rms/50) + 30 - ref
static int16_t sm_log20_16(uint32_t M) {
  uint8_t e = 31 - __builtin_clz(M); // floor(log2), __builtin_clz(0) indef -> M>=1
  uint8_t j = (e >= 7) ? ((M >> (e - 7)) & 0xFF) - 128 : ((M << (7 - e)) & 0xFF) - 128;
  return (int16_t)(pgm_read_word(&SM_EXP[e]) + pgm_read_byte(&SM_FRAC[j]));
}

// S-meter as legacy (usdx-legazy:3565-3614); draws dBm (smode 1) or S (smode 2).
// Legacy cadence: peak tracked always, recompute+draw+decay every ~1-2s
// (%2048 loops). Ours: same structure with time-based ticks (10x100ms).
static int16_t smeter(int16_t ref = 0) {
  max_absavg256 = max(_absavg256, max_absavg256); // peak
  if(smode && (++smeter_cnt % 5) == 0) { // display ~2Hz: fluido sin saturar
    { // recompute dBm: integer log10 (~100 ciclos), sin el float de legacy
      uint32_t M = max_absavg256;
      if(M == 0)
        M = 1;
      uint8_t a = (att2 > 16) ? 16 : att2;
      int16_t t = sm_log20_16(M) + (int16_t)pgm_read_word(&SM_ATT[a]) - SM_C;
      dbm       = ((t >= 0) ? ((t + 8) >> 4) : -((8 - t) >> 4)) - ref;
    }
    { // draw every call using cached dbm (NO noCursor here: legacy re-enables
      // it via stepsize_showcursor in the same call; blinking on/off at tick
      // rate would be visible. Cursor is managed by display_vfo()/tick tail.
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

volatile uint32_t save_event_time = 0; // deferred VFO persist (legacy 5684/5717: save 1s after last tune, never while tuning)
// legacy stepsize_change: dial range 1..8 (never 1Hz idx 9, never 10M idx 0)
void stepsize_change(int8_t val) {
  stepsize += val;
  if(stepsize < 1)
    stepsize = 8; // STEP_10
  if(stepsize > 8)
    stepsize = 1; // STEP_1M
  if(stepsize == 2 || stepsize == 4) // STEP_500k / STEP_10k (comma columns)
    stepsize += val;
  lcd.setCursor(stepsize + 1, 1); // cursor only (legacy stepsize_showcursor, no full redraw)
  lcd.cursor();
}
inline void do_tune() {
  if(tx || vox_tx)
    return; // no tuning while transmitting (legacy parity)
  noInterrupts();
  int32_t d   = encoder_val; // atomic read+clear: no perder pasos del ISR
  encoder_val = 0;
  interrupts();
  if(d) {
    menu.note_dial_turn(); // cancel pending dial double-click (stepping intent)
    last_tune_dir = (d > 0) ? 1 : -1; // F2.5: sentido para cambio de banda
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
      vfo[vfosel % 2] = freq; // VFO follows tune (legacy 5682)
      save_event_time = millis() + 1000; // schedule persist when idle (legacy 5684: no EEPROM wear while tuning)
      vfo_apply();
      uint8_t f = freq / 1000000UL;
      set_lpf(f); // switch LPF band (legacy 5701)
      bandval_align(); // align bandval with freq (legacy 5702)
    }
#ifdef RIT_ENABLE
    if(rit) { // apply RIT offset in real time (legacy 5712)
      si5351.freq_calc_fast(rit);
      si5351.SendPLLRegisterBulk();
    }
#endif
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

// Light periodic refresh (legacy per-loop smeter parity, 5197): ONLY the
// meter/decoder digits on line 0 — never a full rewrite while tuning, so dial
// steps are never stalled by the display (each nibble masks the encoder ISR).
void display_tick() {
  if(mode == CW && cwdec && cw_event && !tx) {
    lcd.setCursor(8, 0);
    lcd.print(cw_line + 8);
  } else if(!(semi_qsk_timeout) && (!vox_tx)) {
    smeter();
  }
  if(menu.state == MENU_MAIN) { // smeter() apaga el cursor: reponerlo (display_vfo parity)
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
  last_full_freq = f; // base for fast-tune deltas (do_tune)
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
  menu.begin();
  drive = 4; // Init settings (legacy 5072); EEPROM restore overrides if valid
  cw_offset = tones[cw_tone]; // CW TX/RX offset (legacy 5079)
  // NOTE: use digitalRead(BUTTONS) here (like legacy 5084) - the ADC is not yet
  // enabled at this point in setup, so analog ADC read would block forever on ADIF.
  // Legacy parity (usdx-legazy:5083-5092): reset to factory defaults when the
  // rotary-key is pressed at power-on or the version signature mismatches.
  // The check runs BEFORE any EEPROM load so RAM still holds compiled defaults.
  bool do_reset = (inv ^ digitalRead(BUTTONS)) ||
                  (eeprom_read_byte((const uint8_t*)EEPROM_MAGIC_OFF) != F_VER_ID);
  if(do_reset) {
    lcd.setCursor(0, 1);
    lcd.print("Reset settings..");
    for(uint8_t i = 0; i != MENU_COUNT; i++) { // persist compiled defaults
      MenuParam p;
      memcpy_P(&p, (PGM_P)&MENU[i], sizeof(MenuParam));
      if(p.eslot && p.value) {
        uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : (p.type == P_TEXT) ? 48 : 1;
        menu_eeprom_save(p.eslot, p.value, sz);
      }
    }
    for(uint8_t b = 0; b < BANDCOUNT; b++) { freq_last[b] = 0; mode_last[b] = 0xFF; } // band defaults (0xFF mode = unset)
    // vfo[]/vfomode[] still hold compiled defaults (no load happened)
    vfo_eeprom_save();
    eeprom_write_byte((uint8_t*)EEPROM_MAGIC_OFF, F_VER_ID);
    delay(500);
  } else {
    vfo_eeprom_load(); // restore band + VFO A/B memories
    menu_load_all(); // restore saved menu params (volume, mode, agc, drive, ...)
  }
  on_pwm(); // build lut with LOADED pwm_min/max (legacy: build_lut after LOAD, 5105)
  freq = vfo[vfosel % 2]; // restore last VFO state (legacy boot parity, 5101)
  mode = vfomode[vfosel % 2];
  bandval_align(); // align bandval with freq (legacy change block)
  // Legacy order parity (usdx-legazy:5110): start RX FIRST, program RF after.
  // set_lpf() warms ~17 latches (~1s) and vfo_apply() bit-bangs the SI5351;
  // doing it before start_rx kept the receiver silent that whole time, while
  // legacy already runs the RX ISR (dark SI5351 = noise, same as here).
  if(agc_start)
    agc_precharge(); // F4.16: gain x8 desde la primera muestra (OFF = x1 legacy)
  start_rx(); // arm RX DSP (ADC I/Q/mic + timers + func_ptr) as legacy
  set_lpf(freq / 1000000UL); // warm LPF relays (first call inits 16 latches, ~550ms+; RX already running like legacy)
  vfo_apply();              // hw freq with loaded rx_ph_q / cw_offset
  save_event_time = 0;      // no pending VFO persist at boot
  encoder_setup();
  perf_init(); // sonda PD5 solo con PERF_METER
  vox = 0; // disable VOX at boot (legacy parity + seguridad: nunca TX al arrancar)
  // nr NO se fuerza: persiste el último valor guardado (divergencia intencional de legacy)
  loadWPM(keyer_speed);       // CW timing
  keyer_set_mode(keyer_mode); // initialize keyerControl (IAMBICA/B, SINGLE)

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
  perf_loop_tick();
  perf_report();

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

  // --- Deferred VFO persist (legacy 5717): save freq when 1s passed since
  // last tune, never while tuning (no EEPROM wear, no tune stalls) ---
  if(save_event_time && (int32_t)(millis() - save_event_time) >= 0) {
    vfo_save_current();
    save_event_time = 0;
  }

  static uint32_t last_display = 0; // throttled periodic refresh (single-shot)
  if(menu.state == MENU_MAIN && !tx && !vox_tx && (int32_t)(millis() - last_display) >= 100) {
    last_display = millis();
    display_tick(); // light meter-only refresh (legacy: skip while TX to avoid I2C conflict)
  }

  // --- VOX based RX/TX (SSB only, legacy 5144) ---
  if(vox && (mode == LSB || mode == USB)) {
    if(!vox_tx) {
      static uint8_t  vox_sample;
      static uint16_t vox_adc;
      if(vox_sample++ == 16) {
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >> mic_atten);
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
