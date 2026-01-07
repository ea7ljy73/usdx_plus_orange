/**
 * uSDX Plus Orange - Firmware Modular
 *
 * Implementación desde cero basada en el código legacy de QCX-SSB / uSDX.
 *
 * Autor: EA7LJY
 * Basado en trabajo de: Guido PE1NNZ (QCX-SSB)
 *
 * Licencia: MIT (ver LICENSE)
 */

// ============================================================================
// INCLUDES
// ============================================================================

#include "usdx_config.h"

// HAL - Hardware Abstraction Layer
#include "src/hal/gpio.h"
#include "src/hal/i2c.h"
#include "src/hal/adc.h"

// Drivers
#include "src/drivers/si5351.h"
#include "src/drivers/lpf_switch.h"

// DSP
#include "src/dsp/ssb.h"
#include "src/dsp/agc.h"
#include "src/dsp/nr.h"
#include "src/dsp/filters.h"
#include "src/dsp/slow_dsp.h"

// State
#include "src/state/state.h"
#include "src/state/bands.h"
#include "src/state/vfo.h"
#include "src/state/rx_tx.h"

// UI
#include "src/ui/display.h"
#include "src/ui/menu.h"
#include "src/ui/encoder.h"
#include "src/ui/smeter.h"
#include "src/ui/ft8_menu.h"

// CAT
#include "src/cat/cat_interface.h"

// CW
#include "src/cw/keyer.h"
#include "src/cw/decoder.h"

// ============================================================================
// CONFIGURACIÓN DE PANTALLA
// ============================================================================

#ifdef OLED
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#else
#include <LiquidCrystal.h>
#endif

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================

// Variables de estado del radio
volatile uint8_t mode = USB;
volatile uint8_t tx = 0;
volatile uint8_t filt = 0;
volatile int32_t freq = 14074000;
volatile uint8_t vfosel = VFOA;

int32_t vfo[2] = {7074000, 14074000};
uint8_t vfomode[2] = {USB, USB};

volatile uint8_t agc = DEFAULT_AGC;
volatile uint8_t nr = DEFAULT_NR;
volatile uint8_t att = 0;
volatile uint8_t att2 = 2;

volatile uint8_t drive = DEFAULT_DRIVE;
volatile uint8_t vox = 0;
volatile uint8_t vox_level = 0;
volatile uint8_t vox_thresh = (1 << 2);

uint8_t band = 0;
uint8_t bandval = 3;

volatile uint8_t ssb_cap = 1;
volatile uint8_t dsp_cap = SDR;

// Variables de encoder
volatile int8_t encoder_val = 0;
volatile int8_t encoder_step = 0;
uint8_t last_state = 0;

// Variables de menú
volatile uint8_t menumode = 0;
volatile uint8_t prev_menumode = 0;
volatile int8_t menu = 0;
volatile uint8_t ft8mode = 0;
volatile uint8_t prev_mode_ft8 = 0;
volatile uint8_t prev_filt_ft8 = 0;
volatile uint8_t prev_agc_ft8 = 0;
volatile uint8_t prev_nr_ft8 = 0;

// Variables CW
volatile uint8_t cwdec = 1;
volatile uint8_t cw_tone = 1;
volatile uint32_t cw_offset = 700;
volatile int16_t rit = 0;
uint8_t keyer_speed = DEFAULT_KEYER_SPEED;
uint8_t keyer_mode = SINGLE;
uint8_t keyer_swap = 0;
volatile uint8_t practice = 0;

// Variables de audio
uint8_t lut[256];
volatile uint8_t amp;
volatile int8_t volume = DEFAULT_VOLUME;
volatile int8_t mox = 0;

// Variables de estado
volatile uint8_t _init = 0;
volatile bool change = true;
volatile uint16_t numSamples = 0;

// S-meter
uint8_t smode = 2;
uint32_t max_absavg256 = 0;
int16_t dbm = 0;

// Variables CAT
volatile uint8_t cat_active = 0;
volatile uint8_t cat_key = 0;

// Stepsizes
const uint32_t stepsizes[N_STEPSIZES] = {
  10000000,  // STEP_10M
  1000000,   // STEP_1M
  500000,    // STEP_500k
  100000,    // STEP_100k
  10000,     // STEP_10k
  1000,      // STEP_1k
  500,       // STEP_500
  100,       // STEP_100
  10,        // STEP_10
  1          // STEP_1
};

volatile uint8_t stepsize = STEP_1k;
uint8_t prev_stepsize[] = { STEP_1k, STEP_500 };

// ========================================================================
// ETIQUETAS DE MENÚ
// ========================================================================

const char* const vfosel_label[] = { "A", "B" };
const char* const mode_label[] = { "LSB", "USB", "CW ", "FM ", "AM " };
const char* const offon_label[] = { "OFF", "ON" };

#if (F_MCU > 16000000)
const char* const filt_label[N_FILT + 1] = { "Full", "3000", "2400", "1800",
                                              "500",  "200",  "100",  "50" };
#else
const char* const filt_label[N_FILT + 1] = { "Full", "2400", "2000", "1500",
                                              "500",  "200",  "100",  "50" };
#endif

const char* const band_label[N_BANDS] = { "160m", "80m", "60m", "40m", "30m", "20m",
                                           "17m",  "15m", "12m", "10m", "6m" };

const char* const stepsize_label[] = { "10M", "1M",   "0.5M", "100k", "10k",
                                        "1k",  "0.5k", "100",  "10",   "1" };

const char* const att_label[] = { "0dB",   "-13dB", "-20dB", "-33dB",
                                   "-40dB", "-53dB", "-60dB", "-73dB" };

#ifdef CLOCK
const char* const smode_label[] = { "OFF", "dBm", "S", "S-bar", "wpm", "Vss", "time" };
#elif defined(VSS_METER)
const char* const smode_label[] = { "OFF", "dBm", "S", "S-bar", "wpm", "Vss" };
#else
const char* const smode_label[] = { "OFF", "dBm", "S", "S-bar", "wpm" };
#endif

#ifdef SWR_METER
const char* const swr_label[] = { "OFF", "FWD-SWR", "FWD-REF", "VFWD-VREF" };
#endif

const char* const cw_tone_label[] = { "700", "600" };

#ifdef KEYER
const char* const keyer_mode_label[] = { "Iambic A", "Iambic B", "Straight" };
#endif

const char* const agc_label[] = { "OFF", "Fast", "Slow" };
const char* const cap_label[] = { "SSB", "DSP", "SDR" };

// ========================================================================
// INSTANCIAS DE CLASES
// ========================================================================

#ifdef OLED
SSD1306AsciiWire display;
#else
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif

SI5351 si5351;

// ========================================================================
// CONFIGURACIÓN DE BANDAS
// ========================================================================

void change_band(uint8_t new_band) {
  if (new_band >= N_BANDS) return;
  band = new_band;
  bandval = band;
}

// ========================================================================
// SETUP
// ========================================================================

void setup() {
#ifdef OLED
  Wire.begin();
  display.begin(&Adafruit128x64, 0x3C);
  display.setFont(System5x7);
#else
  lcd.begin(16, 2);
#endif

  pinMode(LCD_EN, OUTPUT);
  pinMode(LCD_RS, OUTPUT);
  pinMode(LCD_D4, OUTPUT);
  pinMode(LCD_D5, OUTPUT);
  pinMode(LCD_D6, OUTPUT);
  pinMode(LCD_D7, OUTPUT);

  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  pinMode(BUTTONS, INPUT_PULLUP);

  pinMode(RX, OUTPUT);
  digitalWrite(RX, HIGH);

  si5351.init(SI5351_CRYSTAL_LOAD_10PF, F_XTAL, 0);
  si5351.freq(freq, 0, 90);
}

// ========================================================================
// LOOP PRINCIPAL
// ========================================================================

void loop() {
}

