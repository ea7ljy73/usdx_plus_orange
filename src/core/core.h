#ifndef CORE_H
#define CORE_H

#include "../driver/display.h"
#include "../driver/si5351.h"
#include "../dsp/dsp.h"
#include "../hal/hal.h"
#include "../hardware/io_expander.h"
#include "../interface/cat.h"
#include <Arduino.h>

// Forward declarations
void stepsize_showcursor();
void switch_rxtx(uint8_t tx_enable);
int freeMemory();
void initPins();
void encoder_setup();
void timer1_start(uint32_t f);
void timer1_stop();
void timer2_start(uint32_t f);
void timer2_stop();
void adc_start(uint8_t mux_param, bool is_ac, uint32_t f);
void adc_stop();
void fatal(const __FlashStringHelper *msg, int value = 0, char unit = '\0');
void build_lut();
void start_rx();
void show_banner();
void powerDown();
void display_vfo(int32_t f);
void stepsize_change(int8_t val);
void process_encoder_tuning_step(int8_t steps);
uint16_t analogSampleMic();
int16_t smeter(int16_t ref = 0);
uint8_t _digitalRead(uint8_t pin);
#ifdef SWR_METER
void readSWR();
#endif

// Global Variables
extern char __bss_end;
extern int keyer_speed;
extern unsigned long ditTime;
extern int debounce;

#ifdef CAT_EXT
extern volatile uint8_t cat_key;
#endif

#ifdef ONEBUTTON_INV
extern uint8_t inv;
#endif

// Keyer Constants
#define DIT_L 0x01
#define DAH_L 0x02
#define DIT_PROC 0x04
#define PDLSWAP 0x08
#define IAMBICB 0x10
#define IAMBICA 0x00
#define SINGLE 2

enum KSTYPE { IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT };

void update_PaddleLatch();
void loadWPM(int wpm);

extern volatile uint8_t cat_active;
extern uint8_t keyerControl;
extern uint8_t keyerState;
extern uint8_t keyer_mode;
extern uint8_t keyer_swap;
extern uint32_t ktimer;
extern int Key_state;
extern volatile uint32_t rxend_event;
extern volatile uint8_t vox;
extern uint8_t backlight;

extern volatile int8_t encoder_val;
extern volatile int8_t encoder_step;

extern volatile bool change;
extern volatile int32_t freq;
extern int32_t vfo[];
extern uint8_t vfomode[];
enum vfo_t { VFOA = 0, VFOB = 1, SPLIT = 2 };
extern volatile uint8_t vfosel;
extern volatile int16_t rit;

extern uint8_t smode;
extern uint32_t max_absavg256;
extern int16_t dbm;

extern volatile int16_t _centiGain;
extern uint32_t save_event_time;
extern uint8_t vox_tx;
extern uint8_t txdelay;

extern uint8_t semi_qsk;
extern uint32_t semi_qsk_timeout;

extern uint8_t rx_ph_q;

#ifdef QCX
void calibrate_iq();
#endif

#define N_BANDS 11
extern uint32_t band[N_BANDS];
extern uint8_t prev_bandval;
extern uint8_t bandval;

enum step_t {
  STEP_10M,
  STEP_1M,
  STEP_500k,
  STEP_100k,
  STEP_10k,
  STEP_1k,
  STEP_500,
  STEP_100,
  STEP_10,
  STEP_1
};
extern uint32_t stepsizes[10];
extern volatile uint8_t stepsize;
extern uint8_t prev_stepsize[];

extern volatile uint8_t event;
extern volatile uint8_t prev_menumode;
extern volatile int8_t menu;
extern uint8_t eeprom_version;
extern int eeprom_addr;

// Helper Macros
#define pgm_cache_item(addr, sz)                                               \
  byte _item[sz];                                                              \
  memcpy_P(_item, addr, sz); // copy array item from PROGMEM to SRAM
#define get_version_id()                                                       \
  ((VERSION[0] - '1') * 2048 +                                                 \
   ((VERSION[2] - '0') * 10 + (VERSION[3] - '0')) * 32 +                       \
   ((VERSION[4]) ? (VERSION[4] - 'a' + 1) : 0) * 1)

// Menu System
#define N_PARAMS 44
#define N_ALL_PARAMS (N_PARAMS + 5)
#define EEPROM_OFFSET 0x150
#define FT8_EEPROM_ADDR (EEPROM_OFFSET + N_ALL_PARAMS + 2)
#define PREV_MODE_FT8_EEPROM_ADDR (FT8_EEPROM_ADDR + 1)
#define PREV_FILT_FT8_EEPROM_ADDR (PREV_MODE_FT8_EEPROM_ADDR + 1)
#define PREV_AGC_FT8_EEPROM_ADDR (PREV_FILT_FT8_EEPROM_ADDR + 1)
#define PREV_NR_FT8_EEPROM_ADDR (PREV_AGC_FT8_EEPROM_ADDR + 1)

enum params_t {
  _NULL,
  VOLUME,
  MODE,
  FILTER,
  BAND,
  STEP,
  VFOSEL,
  RIT,
  AGC,
  NR,
  ATT,
  ATT2,
  SMETER,
  SWRMETER,
  CWDEC,
  CWTONE,
  CWOFF,
  SEMIQSK,
  KEY_WPM,
  KEY_MODE,
  KEY_PIN,
  KEY_TX,
  VOX,
  VOXGAIN,
  DRIVE,
  TXDELAY,
  MOX,
  FT8MODE,
  CWINTERVAL,
  CWMSG1,
  CWMSG2,
  CWMSG3,
  CWMSG4,
  CWMSG5,
  CWMSG6,
  PWM_MIN,
  PWM_MAX,
  SIFXTAL,
  IQ_ADJ,
  CALIB,
  SR,
  CPULOAD,
  PARAM_A,
  PARAM_B,
  PARAM_C,
  BACKL,
  FREQA,
  FREQB,
  MODEA,
  MODEB,
  VERS,
  ALL = 0xff
};

enum action_t { UPDATE, UPDATE_MENU, NEXT_MENU, LOAD, SAVE, SKIP, NEXT_CH };

void printmenuid(uint8_t menuid);
void printlabel(uint8_t action, uint8_t menuid,
                const __FlashStringHelper *label);
void actionCommon(uint8_t action, uint8_t *ptr, uint8_t size);
int8_t paramAction(uint8_t action, uint8_t id = ALL);

// Template for paramAction (Must remain in header)
template <typename T>
void paramAction(uint8_t action, volatile T &value, uint8_t menuid,
                 const __FlashStringHelper *label, const char *enumArray[],
                 int32_t _min, int32_t _max, bool continuous) {
  switch (action) {
  case UPDATE:
  case UPDATE_MENU:
    if (((int32_t)value + encoder_val) < _min)
      value = (continuous) ? _max : _min;
    else if (((int32_t)value + encoder_val) > _max)
      value = (continuous) ? _min : _max;
    else
      value = (int32_t)value + encoder_val;
    encoder_val = 0;

    lcd.noCursor();
    printlabel(action, menuid, label);
    if (enumArray == NULL) {
      if ((_min < 0) && (value >= 0))
        lcd.print('+');
      lcd.print(value);
    } else {
      lcd.print(enumArray[value]);
    }
    lcd_blanks();
    lcd_blanks();
    break;
  default:
    actionCommon(action, (uint8_t *)&value, sizeof(value));
    break;
  }
}

#ifdef MENU_STR
void paramAction(uint8_t action, char *value, uint8_t menuid,
                 const __FlashStringHelper *label, uint8_t size);
#endif

// Constants Arrays (Declarations)
extern const char *offon_label[2];
extern const char *band_label[N_BANDS];
extern const char *stepsize_label[];
extern const char *att_label[];
extern const char *smode_label[];
extern const char *swr_label[];
extern const char *cw_tone_label[];
extern const char *keyer_mode_label[];
extern const char *agc_label[];
extern const char *filt_label[N_FILT + 1];
extern const char *vfosel_label[];
extern const char *mode_label[5];

#endif // CORE_H
