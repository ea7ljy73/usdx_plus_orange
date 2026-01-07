/**
 * @file usdx_config.h
 * @brief Configuración Unificada para uSDX Plus Orange
 *
 * Este archivo contiene toda la configuración del firmware:
 * - Definiciones de pines
 * - Enums unificados
 * - Macros de utilidad
 * - Flags de features
 * - Constantes de hardware
 *
 * Basado en el código legacy de QCX-SSB / uSDX
 */

#ifndef USDX_CONFIG_H
#define USDX_CONFIG_H

// ============================================================================
// VERSION
// ============================================================================

#define VERSION "1.03x"

// ============================================================================
// INCLUDES BASE
// ============================================================================

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <EEPROM.h>

// ============================================================================
// FRECUENCIAS DE CPU
// ============================================================================

#ifndef F_MCU
#define F_MCU 20000000  // 20MHz ATMEGA328P crystal
#endif

// Ajuste para frecuencia real del cristal (varía ligeramente de 20MHz)
#define F_XTAL 20000000
#define F_CPU F_XTAL

// ============================================================================
// PINES DE HARDWARE (ATmega328P)
// ============================================================================

// LCD Parallel 16x2
#define LCD_D4   0   // PD0    (pin 2)
#define LCD_D5   1   // PD1    (pin 3)
#define LCD_D6   2   // PD2    (pin 4)
#define LCD_D7   3   // PD3    (pin 5)
#define LCD_EN   4   // PD4    (pin 6)
#define LCD_RS  18   // PC4    (pin 27)

// Encoder Rotativo
#define ROT_A    6   // PD6    (pin 12)
#define ROT_B    7   // PD7    (pin 13)

// Control TX/RX
#define RX       8   // PB0    (pin 14)
#define SIDETONE 9   // PB1    (pin 15)
#define KEY_OUT 10   // PB2    (pin 16)

// Keyer CW
#define DAH     12   // PB4    (pin 18)
#define DIT     13   // PB5    (pin 19)

// Audio
#define AUDIO1  14   // PC0/A0 (pin 23)
#define AUDIO2  15   // PC1/A1 (pin 24)

// Medición
#define DVM     16   // PC2/A2 (pin 25)
#define BUTTONS 17   // PC3/A3 (pin 26)

// I2C (comparte pines con LCD en algunas configs)
#define SDA     18   // PC4    (pin 27)
#define SCL     19   // PC5    (pin 28)

// Frecuencia counter
#define FREQCNT 5    // PD5    (pin 11)

// Salida de señal
#define SIG_OUT 11   // PB3    (pin 17)

// Pines opcionales para TX
// #define NTX    11   // PB3    (pin 17)
// #define PTX    11   // PB3    (pin 17)

// SWR Meter
#ifdef SWR_METER
#define PIN_FWD  A6
#define PIN_REF  A7
#endif

// ============================================================================
// CONFIGURACIÓN DE SWAP DE ROTARY
// ============================================================================

#ifdef SWAP_ROTARY
#undef ROT_A
#undef ROT_B
#define ROT_A   7    // PD7    (pin 13)
#define ROT_B   6    // PD6    (pin 12)
#endif

// ============================================================================
// CONFIGURACIÓN DE DISPLAY
// ============================================================================

#if (defined(OLED_SSD1306) || defined(OLED_SH1106))
#define OLED 1
#endif

#if (defined(CAT) || defined(TESTBENCH)) && !(OLED)
#define _SERIAL 1    // Coexistencia serial + LCD en mismos pines
#endif

// ============================================================================
// LPF SWITCHING (DL2MAN USDX REV3)
// ============================================================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1
#endif

// ============================================================================
// CONFIGURACIÓN DE CLK PARA TX/RX
// ============================================================================

#ifdef TX_CLK0_CLK1
#ifdef F_CLK2
#define TX1RX0  0b11111000
#define TX1RX1  0b11111000
#define TX0RX1  0b11111000
#define TX0RX0  0b11111011
#else  //!F_CLK2
#define TX1RX0  0b11111100
#define TX1RX1  0b11111100
#define TX0RX1  0b11111100
#define TX0RX0  0b11111111
#endif //F_CLK2
#else  //!TX_CLK0_CLK1
#define TX1RX0  0b11111011
#define TX1RX1  0b11111000
#define TX0RX1  0b11111100
#define TX0RX0  0b11111111
#endif //TX_CLK0_CLK1

#if defined(F_CLK2) && !defined(TX_CLK0_CLK1)
#error "TX_CLK0_CLK1 must be enabled in order to use F_CLK2."
#endif

// ============================================================================
// FEATURES DE TX (desactivados si no hay TX_ENABLE)
// ============================================================================

#ifndef TX_ENABLE
#undef KEYER
#undef TX_DELAY
#undef SEMI_QSK
#undef RIT_ENABLE
#undef VOX_ENABLE
#undef MOX_ENABLE
#endif //!TX_ENABLE

// ============================================================================
// MODOS DE OPERACIÓN
// ============================================================================

enum mode_t {
  LSB = 0,
  USB = 1,
  CW = 2,
  FM = 3,
  AM = 4
};

// Capacidades DSP
enum dsp_cap_t {
  ANALOG = 0,
  DSP = 1,
  SDR = 2
};

// Modo VFO
enum vfo_t {
  VFOA = 0,
  VFOB = 1,
  SPLIT = 2
};

// Acciones de menú (UNIFICADO)
enum action_t {
  UPDATE = 0,
  UPDATE_MENU = 1,
  NEXT_MENU = 2,
  LOAD = 3,
  SAVE = 4,
  SKIP = 5,
  NEXT_CH = 6
};

// Parámetros (UNIFICADO)
enum params_t {
  _NULL = 0,
  VOLUME = 1,
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
  CWSPEED,
  CWMODE,
  CWSWAP,
  PRACTICE,
  VOX,
  VOXLEVEL,
  DRIVE,
  TXDELAY,
  MOX,
  SIFXTAL,
  IQ_ADJ,
  CAESSION,
  BACKL,
  FREQA,
  FREQB,
  MODEA,
  MODEB,
  VERS,
  ALL = 0xff
};

#define N_PARAMS 44
#define N_ALL_PARAMS (N_PARAMS + 5)

// Estados del keyer CW
enum KSTYPE {
  IDLE,
  CHK_DIT,
  CHK_DAH,
  KEYED_PREP,
  KEYED,
  INTER_ELEMENT
};

// Pasos de tuning
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

// ============================================================================
// BANDAS DE FRECUENCIA
// ============================================================================

#define N_BANDS 11

// Band frequencies array (kHz) - indexed by band number
#ifdef CW_FREQS_QRP
const uint32_t band_freqs[N_BANDS] = {
  1810, 3560, 5351, 7030, 10106,
  14060, 18096, 21060, 24906, 28060, 50096
};
#elif defined(CW_FREQS_FISTS)
const uint32_t band_freqs[N_BANDS] = {
  1818, 3558, 5351, 7028, 10118,
  14058, 18085, 21058, 24908, 28058, 50058
};
#else  // Default FT8 frequencies
const uint32_t band_freqs[N_BANDS] = {
  1840, 3573, 5357, 7074, 10136,
  14074, 18100, 21074, 24915, 28074, 50313
};
#endif

// Tamaños de paso
#define N_STEPSIZES 10
extern const uint32_t stepsizes[N_STEPSIZES];

// ============================================================================
// KEYER CONTROL BITS
// ============================================================================

#define DIT_L     0x01  // Dit latch
#define DAH_L     0x02  // Dah latch
#define DIT_PROC  0x04  // Dit is being processed
#define PDLSWAP   0x08  // 0 for normal, 1 for swap
#define IAMBICB   0x10  // 0 for Iambic A, 1 for Iambic B
#define IAMBICA   0x00  // 0 for Iambic A, 1 for Iambic B
#define SINGLE    2     // Keyer Mode 0 1 -> Iambic2  2 -> SINGLE

// ============================================================================
// MACROS DE UTILIDAD
// ============================================================================

// Número de elementos en array
#define _N(x) (sizeof(x) / sizeof(x[0]))

// Exponential moving average
#define EMA(acc, val, alpha) (((acc) * ((alpha) - 1) + (val)) / (alpha))

// Cache de item desde PROGMEM
#define pgm_cache_item(addr, sz) \
  byte _item[sz]; \
  memcpy_P(_item, addr, sz)

// Versión desde string
#define get_version_id() \
  ((VERSION[0] - '1') * 2048 + \
   ((VERSION[2] - '0') * 10 + (VERSION[3] - '0')) * 32 + \
   ((VERSION[4]) ? (VERSION[4] - 'a' + 1) : 0) * 1)

// ============================================================================
// EEPROM
// ============================================================================

#define EEPROM_OFFSET 0x150
#define FT8_EEPROM_ADDR (EEPROM_OFFSET + N_ALL_PARAMS + 2)
#define PREV_MODE_FT8_EEPROM_ADDR (FT8_EEPROM_ADDR + 1)

// ============================================================================
// CONSTANTES DE CONFIGURACIÓN
// ============================================================================

// Volumen default
#define DEFAULT_VOLUME 12

// Keyer speed default
#define DEFAULT_KEYER_SPEED 25

// WPM default
#define DEFAULT_WPM 25

// Drive default
#define DEFAULT_DRIVE 2

// AGC default
#ifdef FAST_AGC
#define DEFAULT_AGC 2
#else
#define DEFAULT_AGC 1
#endif

// NR default
#define DEFAULT_NR 2

// ============================================================================
// ETIQUETAS DE MENÚ (definidas en otro archivo)
// ============================================================================

extern const char* const vfosel_label[];
extern const char* const mode_label[];
extern const char* const offon_label[];
extern const char* const filt_label[];
extern const char* const band_label[];
extern const char* const stepsize_label[];
extern const char* const att_label[];
extern const char* const smode_label[];
extern const char* const swr_label[];
extern const char* const cw_tone_label[];
extern const char* const keyer_mode_label[];
extern const char* const agc_label[];
extern const char* const cap_label[];

// ============================================================================
// DECLARACIONES DE VARIABLES GLOBALES
// ============================================================================

// Estado del radio
extern volatile uint8_t mode;
extern volatile uint8_t tx;
extern volatile uint8_t filt;
extern volatile int32_t freq;
extern volatile uint8_t vfosel;

extern int32_t vfo[2];
extern uint8_t vfomode[2];

extern volatile uint8_t agc;
extern volatile uint8_t nr;
extern volatile uint8_t att;
extern volatile uint8_t att2;

extern volatile uint8_t drive;
extern volatile uint8_t vox;
extern volatile uint8_t vox_level;
extern volatile uint8_t vox_thresh;

extern uint8_t band;
extern uint8_t bandval;

extern volatile uint8_t ssb_cap;
extern volatile uint8_t dsp_cap;

// Encoder
extern volatile int8_t encoder_val;
extern volatile int8_t encoder_step;
extern uint8_t last_state;

// Menú
extern volatile uint8_t menumode;
extern volatile uint8_t prev_menumode;
extern volatile int8_t menu;
extern volatile uint8_t ft8mode;
extern volatile uint8_t prev_mode_ft8;
extern volatile uint8_t prev_filt_ft8;
extern volatile uint8_t prev_agc_ft8;
extern volatile uint8_t prev_nr_ft8;

// CW
extern volatile uint8_t cwdec;
extern volatile uint8_t cw_tone;
extern volatile uint32_t cw_offset;
extern volatile int16_t rit;
extern uint8_t keyer_speed;
extern uint8_t keyer_mode;
extern uint8_t keyer_swap;
extern volatile uint8_t practice;
extern volatile uint32_t _amp32;
extern volatile uint8_t cw_event;

// Morse code table
extern const char m2c[] PROGMEM;

// Variables de audio
extern uint8_t lut[256];
extern volatile uint8_t amp;
extern volatile int8_t volume;
extern volatile int8_t mox;

// Variables de estado
extern volatile uint8_t _init;
extern volatile bool change;
extern volatile uint16_t numSamples;

// S-meter
extern uint8_t smode;
extern uint32_t max_absavg256;
extern int16_t dbm;

// SWR Meter
#ifdef SWR_METER
extern float FWD;
extern float SWR;
extern float ref_V;
extern uint32_t stimer;
#endif

// Variables CAT
extern volatile uint8_t cat_active;
extern volatile uint8_t cat_key;

// ============================================================================
// EXTERN DE CLASE SI5351
// ============================================================================

extern class SI5351 si5351;

// ============================================================================
// EXTERN DE DISPLAY
// ============================================================================

#ifdef OLED
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
extern class SSD1306AsciiWire display;
#else
#include <LiquidCrystal.h>
extern class LiquidCrystal lcd;
#endif

#endif // USDX_CONFIG_H
