/*
 *  uSDX Plus Orange - Clean Refactored Version
 *  ============================================
 *  Firmware para transceiver SDR basado en ATMEGA328P
 *  Original: https://github.com/threeme3/usdx
 *  Modificaciones: EA7LJY - Julian (2026-01-12)
 *
 *  Licencia: MIT - see LICENSE file
 *
 *  Este archivo es una refactorización limpia del código original,
 *  reorganizando funciones y variables para mejor mantenibilidad,
 *  conservando toda la funcionalidad condicional (#ifdef) para
 *  diferentes configuraciones de hardware.
 */

// Configuración - editar en usdx_settings.h
#include "usdx_settings.h"
#include <Arduino.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <math.h>

// Version del firmware
#define VERSION "1.18"

// ============================================================================
// SECCIÓN 1: DEFINICIONES DE PINES DE HARDWARE
// ============================================================================

#define LCD_D4 0   // PD0    (pin 2)
#define LCD_D5 1   // PD1    (pin 3)
#define LCD_D6 2   // PD2    (pin 4)
#define LCD_D7 3   // PD3    (pin 5)
#define LCD_EN 4   // PD4    (pin 6)
#define FREQCNT 5  // PD5    (pin 11)
#define ROT_A 6    // PD6    (pin 12)
#define ROT_B 7    // PD7    (pin 13)
#define RX 8       // PB0    (pin 14)
#define SIDETONE 9 // PB1    (pin 15)
#define KEY_OUT 10 // PB2    (pin 16)
#define SIG_OUT 11 // PB3    (pin 17)
#define DAH 12     // PB4    (pin 18)
#define DIT 13     // PB5    (pin 19)
#define AUDIO1 14  // PC0/A0 (pin 23)
#define AUDIO2 15  // PC1/A1 (pin 24)
#define DVM 16     // PC2/A2 (pin 25)
#define BUTTONS 17 // PC3/A3 (pin 26)
#define LCD_RS 18  // PC4    (pin 27)
#define SDA 18     // PC4    (pin 27)
#define SCL 19     // PC5    (pin 28)

#ifndef BLUETOOTH_PIN
#define BLUETOOTH_PIN BUTTONS
#endif

// Conmutación de pines del encoder rotatorio
#ifdef SWAP_ROTARY
#undef ROT_A
#undef ROT_B
#define ROT_A 7 // PD7    (pin 13)
#define ROT_B 6 // PD6    (pin 12)
#endif

// Detección automática de display OLED
#if (defined(OLED_SSD1306) || defined(OLED_SH1106))
#define OLED 1
#endif

// Compatibilidad Serial + LCD (usan mismos pines)
#if (defined(CAT) || defined(TESTBENCH)) && !(OLED)
#define _SERIAL 1
#endif

// LPF switching legacy support
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1
#endif

// Configuración TX/RX para el sintetizador SI5351
#ifdef TX_CLK0_CLK1
#ifdef F_CLK2
#define TX1RX0 0b11111000
#define TX1RX1 0b11111000
#define TX0RX1 0b11111000
#define TX0RX0 0b11111011
#else
#define TX1RX0 0b11111100
#define TX1RX1 0b11111100
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111
#endif
#else
#define TX1RX0 0b11111011
#define TX1RX1 0b11111000
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111
#endif

#if defined(F_CLK2) && !defined(TX_CLK0_CLK1)
#error "TX_CLK0_CLK1 must be enabled in order to use F_CLK2."
#endif

// COMMENTED OUT - This was incorrectly disabling VOX_ENABLE even when TX_ENABLE was set
//#ifndef TX_ENABLE
//#undef TX_DELAY
//#undef SEMI_QSK
//#undef RIT_ENABLE
//#undef VOX_ENABLE
//#undef MOX_ENABLE
//#endif

#ifdef SWR_METER
float FWD;
float SWR;
float ref_V = 5 * 1.15;
static uint32_t stimer;
#define PIN_FWD A6
#define PIN_REF A7
static uint8_t tx_pwr_display = 0;
static uint8_t tx_swr_display = 10;
#endif

// ============================================================================
// SECCIÓN 2: VERIFICACIÓN DE ARQUITECTURA
// ============================================================================

#if !(defined(ARDUINO_ARCH_AVR))
#error                                                                         \
    "Unsupported architecture, select Arduino IDE > Tools > Board > Arduino AVR Boards > Arduino Uno."
#endif

// Override F_CPU to actual hardware frequency (20MHz)
#undef F_CPU
#define F_CPU 20007000

#ifndef F_MCU
#define F_MCU 20000000
#endif

// ============================================================================
// SECCIÓN 3: VARIABLES GLOBALES - ESTADO DEL SISTEMA
// ============================================================================

// Definiciones de tipos y enumeraciones (primero para evitar errores de
// compilación)
enum dsp_cap_t { ANALOG, DSP, SDR };
enum mode_t { LSB, USB, CW, FM, AM };
enum vfo_t { VFOA = 0, VFOB = 1, SPLIT = 2 };
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

// Estados de operacion
volatile uint8_t mode = USB;
volatile uint16_t numSamples = 0;
volatile uint8_t tx = 0;
volatile uint8_t filt = 0;

// Frecuencia y VFO
volatile bool change = true;
volatile int32_t freq = 14000000;
static int32_t vfo[] = {7074000, 14074000};
static uint8_t vfomode[] = {USB, USB};
volatile uint8_t vfosel = VFOA;
volatile int16_t rit = 0;

// Variables de procesamiento DSP
volatile int8_t mox = 0;
volatile int8_t volume = 12;  // Legacy default: better sensitivity for weak signals
#ifdef FAST_AGC
volatile uint8_t agc = 2;
#else
volatile uint8_t agc = 1;
#endif
volatile uint8_t nr = 0;
volatile uint8_t att = 0;
volatile uint8_t att2 = 2;
volatile uint8_t drive = 2;
volatile uint8_t amp = 0;
volatile uint8_t quad = 0;

// Variables de muestreo
volatile int16_t i, q;
volatile uint32_t acc = 0;
volatile uint32_t cw_offset = 0;
volatile uint8_t cw_tone = 1;

// Encoder
volatile int8_t encoder_val = 0;
volatile int8_t encoder_last_delta = 0;
volatile int8_t encoder_step = 0;
static uint8_t last_state;

// Threshold VOX
volatile uint8_t vox_thresh;

// VOX and Menu
volatile uint8_t vox = 0;
volatile uint32_t rxend_event = 0;
volatile uint8_t cat_active = 0;
volatile uint8_t menumode = 0;
volatile uint8_t prev_menumode = 0;
volatile int8_t menu = 0;
volatile uint8_t event = 0;

// Capability
#ifdef QCX
uint8_t dsp_cap = 0;
uint8_t ssb_cap = 0;
#else
const uint8_t ssb_cap = 1;
const uint8_t dsp_cap = SDR;
#endif

// Filtros
uint8_t prev_filt[] = {0, 4};

#include "usdx_filter.h"

// Puntero a funcion de procesamiento actual
typedef void (*func_t)(void);
volatile func_t func_ptr;

// CAT streaming
volatile uint8_t cat_streaming = 0;
volatile uint8_t _cat_streaming = 0;

// CW decoder
#ifdef CW_DECODER
volatile uint8_t cwdec = 1;
static int32_t avg = 256;
static uint8_t sym;
static uint32_t amp32 = 0;
volatile uint32_t _amp32 = 0;
static char out[] = "                ";
volatile uint8_t cw_event = false;
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
uint8_t wpm = 25;
#endif

#ifdef SWR_METER
volatile uint8_t swrmeter = 1;
volatile uint8_t calpwr = PWR_CALIBRATION_CONSTANT;
#ifdef INA219_POWER_METER
volatile uint16_t calshunt = CURRENT_SHUNT_CALIBRATION_CONSTANT;
#endif
#endif

uint8_t lut[256];

// TX State
static uint8_t vox_tx = 0;
volatile uint8_t _init = 0;

// Keyer State
enum KSTYPE {IDLE, CHK_DIT, CHK_DAH, KEYED_PREP, KEYED, INTER_ELEMENT }; // State machine states
volatile uint8_t keyerState = IDLE;

// Abs average
static uint32_t absavg256 = 0;
volatile uint32_t _absavg256 = 0;

// CentiGain backup
int16_t _centiGain = 0;

// TX Delay and QSK
uint8_t txdelay = 0;
uint8_t semi_qsk = false;
uint32_t semi_qsk_timeout = 0;
static uint8_t practice = false;

// Encoder phase
uint8_t rx_ph_q = 90;

// EEPROM
uint16_t eeprom_version;
#define EEPROM_OFFSET 0x150
int eeprom_addr;

inline uint16_t get_version_id() {
  return (VERSION[0] - '1') * 2048 +
         ((VERSION[2] - '0') * 10 + (VERSION[3] - '0')) * 32 +
         ((VERSION[4]) ? (VERSION[4] - 'a' + 1) : 0);
}

//=========================================================================
// SECCIÓN 4: INCLUDES DEL SISTEMA
//=========================================================================

#include <avr/sleep.h>
#include <avr/wdt.h>

//=========================================================================
// SECCIÓN 5: CAT EXT - LECTURA DIGITAL
//=========================================================================

#ifdef CAT_EXT
volatile uint8_t cat_key = 0;

uint8_t _digitalRead(uint8_t pin) {
  serialEvent();
  if (cat_key) {
    return (pin == BUTTONS) ? ((cat_key & 0x07) > 0)
           : (pin == DIT)   ? ~cat_key & 0x10
           : (pin == DAH)   ? ~cat_key & 0x20
                            : 0;
  }
  return digitalRead(pin);
}
#else
#define _digitalRead(x) digitalRead(x)
#endif

//=========================================================================
// SECCIÓN 6: CONFIGURACIÓN DE BOTONES
//=========================================================================

#ifdef ONEBUTTON_INV
uint8_t inv = 1;
#else
uint8_t inv = 0;
#endif

#ifdef THREEBUTTONROT
const unsigned long debounceDelay = 50;
const unsigned long initialDelay = 400;
const unsigned long repeatDelay = 150;
unsigned int buttonState = 3;
unsigned int lastButtonState = 3;
unsigned long lastDebounceTime = 0;
unsigned long lastRepeatTime = 0;
bool repeating = false;
#endif

//=========================================================================
// SECCIÓN 7: KEYER
//=========================================================================

#ifdef KEYER
#define DIT_L 0x01
#define DAH_L 0x02
#define DIT_PROC 0x04
#define PDLSWAP 0x08
#define IAMBICB 0x10
#define IAMBICA 0x00
#define SINGLE 2

volatile uint8_t keyerControl = IAMBICB;
volatile uint8_t keyer_mode = SINGLE;
volatile uint8_t keyer_swap = 0;
volatile uint8_t keyer_speed = 25;
static unsigned long ditTime;

void update_PaddleLatch() // Latch dit and/or dah press, called by keyer routine
{
    if(_digitalRead(DIT) == LOW) {
        keyerControl |= keyer_swap ? DAH_L : DIT_L;
    }
    if(_digitalRead(DAH) == LOW) {
        keyerControl |= keyer_swap ? DIT_L : DAH_L;
    }
}

void loadWPM (int wpm) // Calculate new time constants based on wpm value
{
#if(F_MCU != 20000000)
  ditTime = (1200ULL * F_MCU/16000000)/wpm;   //ditTime = 1200/wpm;  compensated for F_CPU clock (running in a 16MHz Arduino environment)
#else
  ditTime = (1200 * 5/4)/wpm;   //ditTime = 1200/wpm;  compensated for 20MHz clock (running in a 16MHz Arduino environment)
#endif
}
#endif

//=========================================================================
// SECCIÓN 8: CLASE I2C SECUNDARIA (PARA LCD/OLED I2C)
//=========================================================================

class I2C_ {
public:
#if (F_MCU > 20900000)
#ifdef OLED_SH1106
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 9; i++)                                             \
    asm("nop");
#else
#ifdef OLED_SSD1306
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 6; i++)                                             \
    asm("nop");
#else
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 7; i++)                                             \
    asm("nop");
#endif
#endif
#else
#ifdef OLED_SH1106
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 8; i++)                                             \
    asm("nop");
#else
#ifdef OLED_SSD1306
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 4; i++)                                             \
    asm("nop");
#else
#define _DELAY()                                                               \
  for (uint8_t i = 0; i != 5; i++)                                             \
    asm("nop");
#endif
#endif
#endif

#define _I2C_SDA (1 << 2)
#define _I2C_SCL (1 << 3)

#ifdef _I2C_DIRECT_IO
#define _I2C_INIT()                                                            \
  _I2C_SDA_HI();                                                               \
  _I2C_SCL_HI();                                                               \
  DDRD |= (_I2C_SDA | _I2C_SCL);
#define _I2C_SDA_HI() PORTD |= _I2C_SDA;
#define _I2C_SDA_LO() PORTD &= ~_I2C_SDA;
#define _I2C_SCL_HI()                                                          \
  PORTD |= _I2C_SCL;                                                           \
  _DELAY();
#define _I2C_SCL_LO()                                                          \
  PORTD &= ~_I2C_SCL;                                                          \
  _DELAY();
#else
#define _I2C_INIT()                                                            \
  PORTD &= ~_I2C_SDA;                                                          \
  PORTD &= ~_I2C_SCL;                                                          \
  _I2C_SDA_HI();                                                               \
  _I2C_SCL_HI();
#define _I2C_SDA_HI() DDRD &= ~_I2C_SDA;
#define _I2C_SDA_LO() DDRD |= _I2C_SDA;
#define _I2C_SCL_HI()                                                          \
  DDRD &= ~_I2C_SCL;                                                           \
  _DELAY();
#define _I2C_SCL_LO()                                                          \
  DDRD |= _I2C_SCL;                                                            \
  _DELAY();
#endif

#define _I2C_START()                                                           \
  _I2C_SDA_LO();                                                               \
  _DELAY();                                                                    \
  _I2C_SCL_LO();
#define _I2C_STOP()                                                            \
  _I2C_SDA_LO();                                                               \
  _I2C_SCL_HI();                                                               \
  _I2C_SDA_HI();
#define _I2C_SUSPEND()
#define _SendBit(data, bit)                                                    \
  if (data & 1 << bit) {                                                       \
    _I2C_SDA_HI();                                                             \
  } else {                                                                     \
    _I2C_SDA_LO();                                                             \
  }                                                                            \
  _I2C_SCL_HI();                                                               \
  _I2C_SCL_LO();

  inline void start() {
    _I2C_INIT();
    _I2C_START();
  };
  inline void stop() {
    _I2C_STOP();
    _I2C_SUSPEND();
  };
  inline void SendByte(uint8_t data) {
    _SendBit(data, 7);
    _SendBit(data, 6);
    _SendBit(data, 5);
    _SendBit(data, 4);
    _SendBit(data, 3);
    _SendBit(data, 2);
    _SendBit(data, 1);
    _SendBit(data, 0);
    _I2C_SDA_HI();
    _DELAY();
    _I2C_SCL_HI();
    _I2C_SCL_LO();
  }
  void SendRegister(uint8_t addr, uint8_t *data, uint8_t n) {
    start();
    SendByte(addr << 1);
    while (n--)
      SendByte(*data++);
    stop();
  }
  void begin() {};
  void beginTransmission(uint8_t addr) {
    start();
    SendByte(addr << 1);
  };
  bool write(uint8_t byte) {
    SendByte(byte);
    return 1;
  };
  uint8_t endTransmission() {
    stop();
    return 0;
  };
};
I2C_ Wire;

//=========================================================================
// SECCIÓN 9: DRIVER LCD HD44780
//=========================================================================

uint8_t backlight = 8;

class LCD : public Print {
public:
#define _dn 0
#define _en 4
#define _rs 4
#define RS_PULLUP 1

#ifdef RS_PULLUP
#define LCD_RS_HI()                                                            \
  DDRC &= ~(1 << _rs);                                                         \
  asm("nop");
#define LCD_RS_LO() DDRC |= 1 << _rs;
#else
#define LCD_RS_LO() PORTC &= ~(1 << _rs);
#define LCD_RS_HI() PORTC |= (1 << _rs);
#endif

#define LCD_EN_LO() PORTD &= ~(1 << _en);
#define LCD_EN_HI() PORTD |= (1 << _en);
#define LCD_PREP_NIBBLE(b) (PORTD & ~(0xf << _dn)) | (b) << _dn | 1 << _en

  uint8_t _cols;

  void begin(uint8_t x = 0, uint8_t y = 0) {
#ifdef LCD_I2C
#define PCF_ADDR 0x27
#define PCF_RS 0x01
#define PCF_RW 0x02
#define PCF_EN 0x04
#define PCF_BACKLIGHT 0x08
    Wire.beginTransmission(PCF_ADDR);
    Wire.write(0);
    Wire.endTransmission();
    delayMicroseconds(50000);
#else
    DDRD |= 0xf << _dn | 1 << _en;
    DDRC |= 1 << _rs;
    delayMicroseconds(50000);
    LCD_RS_LO();
    LCD_EN_LO();
#endif
    cmd(0x33);
    delayMicroseconds(4500);
    cmd(0x33);
    delayMicroseconds(4500);
    cmd(0x33);
    delayMicroseconds(150);
    cmd(0x32);
    cmd(0x28);
    cmd(0x0c);
    cmd(0x01);
    delay(3);
    cmd(0x06);
  }

#ifdef LCD_I2C
  void nib(uint8_t b, bool isData) {
    b = (b << 4) | ((backlight) ? PCF_BACKLIGHT : 0) | ((isData) ? PCF_RS : 0);
    Wire.write(b | PCF_EN);
    delayMicroseconds(4);
    Wire.write(b);
    delayMicroseconds(60);
    Wire.write(b);
  }
  void cmd(uint8_t b) {
    Wire.beginTransmission(PCF_ADDR);
    nib(b >> 4, false);
    nib(b, false);
    Wire.endTransmission();
  }
  size_t write(uint8_t b) {
    Wire.beginTransmission(PCF_ADDR);
    nib((b >> 4), true);
    nib((b), true);
    Wire.endTransmission();
  }
#else
  void pre() {
#ifdef _SERIAL
    if (!vox)
      if (cat_active) {
        Serial.flush();
        for (; millis() < rxend_event;)
          wdt_reset();
        PORTC |= 1 << 2;
        DDRC |= 1 << 2;
      }
    UCSR0B &= ~((1 << RXEN0) | (1 << TXEN0));
#endif
    noInterrupts();
  }

  void post() {
    if (backlight)
      PORTD |= 0x08;
    else
      PORTD &= ~0x08;
#ifdef _SERIAL
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
    if (!vox)
      if (cat_active) {
        PORTC &= ~(1 << 2);
      }
#endif
    interrupts();
  }

#ifdef RS_HIGH_ON_IDLE
  void cmd(uint8_t b) {
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4);
    PORTD = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_LO();
    LCD_EN_LO();
    PORTD = nibl;
    asm("nop");
    asm("nop");
    LCD_EN_LO();
    LCD_RS_HI();
    post();
    delayMicroseconds(60);
  }

  size_t write(uint8_t b) {
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4);
    PORTD = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;
    asm("nop");
    asm("nop");
    LCD_EN_LO();
    post();
    delayMicroseconds(60);
    return 1;
  }
#else
  void nib(uint8_t b) {
    pre();
    PORTD = LCD_PREP_NIBBLE(b);
    delayMicroseconds(4);
    LCD_EN_LO();
    post();
    delayMicroseconds(60);
  }

  void cmd(uint8_t b) {
    nib(b >> 4);
    nib(b & 0xf);
  }

  size_t write(uint8_t b) {
    pre();
    uint8_t nibh = LCD_PREP_NIBBLE(b >> 4);
    PORTD = nibh;
    uint8_t nibl = LCD_PREP_NIBBLE(b & 0xf);
    LCD_RS_HI();
    LCD_EN_LO();
    PORTD = nibl;
    LCD_RS_LO();
    LCD_RS_HI();
    LCD_EN_LO();
    LCD_RS_LO();
    post();
    delayMicroseconds(60);
    return 1;
  }
#endif
#endif

  void setCursor(uint8_t x, uint8_t y) {
#ifdef CONDENSED
    cmd(0x80 | (x + (uint8_t[]){0x00, 0x40, 0x00 + 20, 0x40 + 20}[y]));
#else
    cmd(0x80 | (x + y * 0x40));
#endif
  }
  void cursor() { cmd(0x0e); }
  void noCursor() { cmd(0x0c); }
  void noDisplay() { cmd(0x08); }
  void createChar(uint8_t l, uint8_t glyph[]) {
    cmd(0x40 | ((l & 0x7) << 3));
    for (int i = 0; i != 8; i++)
      write(glyph[i]);
  }
};

//=========================================================================
// SECCIÓN 10: FUENTE PARA LCD
//=========================================================================

const uint8_t font[] PROGMEM = {
    0x00,      0x00,      0x00,      0x00,      0x00,      0x00,      0x00,
    0x00,      0x00,      0x00,      0x00,      0x4f,      0x4f,      0x00,
    0x00,      0x00,      0x00,      0x07,      0x07,      0x00,      0x00,
    0x07,      0x07,      0x00,      0x14,      0x7f,      0x7f,      0x14,
    0x14,      0x7f,      0x7f,      0x14,      0x00,      0x24,      0x2e,
    0x6b,      0x6b,      0x3a,      0x12,      0x00,      0x00,      0x63,
    0x33,      0x18,      0x0c,      0x66,      0x63,      0x00,      0x00,
    0x32,      0x7f,      0x4d,      0x4d,      0x77,      0x72,      0x50,
    0x00,      0x00,      0x00,      0x04,      0x06,      0x03,      0x01,
    0x00,      0x00,      0x00,      0x1c,      0x3e,      0x63,      0x41,
    0x00,      0x00,      0x00,      0x00,      0x41,      0x63,      0x3e,
    0x1c,      0x00,      0x00,      0x08,      0x2a,      0x3e,      0x1c,
    0x1c,      0x3e,      0x2a,      0x08,      0x00,      0x08,      0x08,
    0x3e,      0x3e,      0x08,      0x08,      0x00,      0x00,      0x00,
    0x80,      0xe0,      0x60,      0x00,      0x00,      0x00,      0x00,
    0x08,      0x08,      0x08,      0x08,      0x08,      0x08,      0x00,
    0x00,      0x00,      0x00,      0x60,      0x60,      0x00,      0x00,
    0x00,      0x00,      0x40,      0x60,      0x30,      0x18,      0x0c,
    0x06,      0x02,      0x00,      0x3e,      0x7f,      0x49,      0x45,
    0x7f,      0x3e,      0x00,      0x00,      0x40,      0x44,      0x7f,
    0x7f,      0x40,      0x40,      0x00,      0x00,      0x62,      0x73,
    0x51,      0x49,      0x4f,      0x46,      0x00,      0x00,      0x22,
    0x63,      0x49,      0x49,      0x7f,      0x36,      0x00,      0x00,
    0x18,      0x18,      0x14,      0x16,      0x7f,      0x7f,      0x10,
    0x00,      0x27,      0x67,      0x45,      0x45,      0x7d,      0x39,
    0x00,      0x00,      0x3e,      0x7f,      0x49,      0x49,      0x7b,
    0x32,      0x00,      0x00,      0x03,      0x03,      0x79,      0x7d,
    0x07,      0x03,      0x00,      0x00,      0x36,      0x7f,      0x49,
    0x49,      0x7f,      0x36,      0x00,      0x00,      0x26,      0x6f,
    0x49,      0x49,      0x7f,      0x3e,      0x00,      0x00,      0x00,
    0x00,      0x24,      0x24,      0x00,      0x00,      0x00,      0x00,
    0x00,      0x80,      0xe4,      0x64,      0x00,      0x00,      0x00,
    0x00,      0x08,      0x1c,      0x36,      0x63,      0x41,      0x41,
    0x00,      0x00,      0x14,      0x14,      0x14,      0x14,      0x14,
    0x14,      0x00,      0x00,      0x41,      0x41,      0x63,      0x36,
    0x1c,      0x08,      0x00,      0x00,      0x02,      0x03,      0x51,
    0x59,      0x0f,      0x06,      0x00,      0x00,      0x3e,      0x7f,
    0x41,      0x4d,      0x4f,      0x2e,      0x00,      0x00,      0x7c,
    0x7e,      0x0b,      0x0b,      0x7e,      0x7c,      0x00,      0x00,
    0x7f,      0x7f,      0x49,      0x49,      0x7f,      0x36,      0x00,
    0x00,      0x3e,      0x7f,      0x41,      0x41,      0x63,      0x22,
    0x00,      0x00,      0x7f,      0x7f,      0x41,      0x63,      0x3e,
    0x1c,      0x00,      0x00,      0x7f,      0x7f,      0x49,      0x49,
    0x41,      0x41,      0x00,      0x00,      0x7f,      0x7f,      0x09,
    0x09,      0x01,      0x01,      0x00,      0x00,      0x3e,      0x7f,
    0x41,      0x49,      0x7b,      0x3a,      0x00,      0x00,      0x7f,
    0x7f,      0x08,      0x08,      0x7f,      0x7f,      0x00,      0x00,
    0x00,      0x41,      0x7f,      0x7f,      0x41,      0x00,      0x00,
    0x00,      0x20,      0x60,      0x41,      0x7f,      0x3f,      0x01,
    0x00,      0x00,      0x7f,      0x7f,      0x1c,      0x36,      0x63,
    0x41,      0x00,      0x00,      0x7f,      0x7f,      0x40,      0x40,
    0x40,      0x40,      0x00,      0x00,      0x7f,      0x7f,      0x06,
    0x0c,      0x06,      0x7f,      0x7f,      0x00,      0x7f,      0x7f,
    0x0e,      0x1c,      0x7f,      0x7f,      0x00,      0x00,      0x3e,
    0x7f,      0x41,      0x41,      0x7f,      0x3e,      0x00,      0x00,
    0x7f,      0x7f,      0x09,      0x09,      0x0f,      0x06,      0x00,
    0x00,      0x1e,      0x3f,      0x21,      0x61,      0x7f,      0x5e,
    0x00,      0x00,      0x7f,      0x7f,      0x19,      0x39,      0x6f,
    0x46,      0x00,      0x00,      0x26,      0x6f,      0x49,      0x49,
    0x7b,      0x32,      0x00,      0x00,      0x01,      0x01,      0x7f,
    0x7f,      0x01,      0x01,      0x00,      0x00,      0x3f,      0x7f,
    0x40,      0x40,      0x7f,      0x3f,      0x00,      0x00,      0x1f,
    0x3f,      0x60,      0x60,      0x3f,      0x1f,      0x00,      0x00,
    0x7f,      0x7f,      0x30,      0x18,      0x30,      0x7f,      0x7f,
    0x00,      0x63,      0x77,      0x1c,      0x1c,      0x77,      0x63,
    0x00,      0x00,      0x07,      0x0f,      0x78,      0x78,      0x0f,
    0x07,      0x00,      0x00,      0x61,      0x71,      0x59,      0x4d,
    0x47,      0x43,      0x00,      0x00,      0x00,      0x7f,      0x7f,
    0x41,      0x41,      0x00,      0x00,      0x00,      0x02,      0x06,
    0x0c,      0x18,      0x30,      0x60,      0x40,      0x00,      0x00,
    0x41,      0x41,      0x7f,      0x7f,      0x00,      0x00,      0x00,
    0x08,      0x0c,      0xfe,      0xfe,      0x0c,      0x08,      0x00,
    0x80,      0x80,      0x80,      0x80,      0x80,      0x80,      0x80,
    0x80,      0x00,      0x01,      0x03,      0x06,      0x04,      0x00,
    0x00,      0x00,      0x00,      0x20,      0x74,      0x54,      0x54,
    0x7c,      0x78,      0x00,      0x00,      0x7e,      0x7e,      0x48,
    0x48,      0x78,      0x30,      0x00,      0x00,      0x38,      0x7c,
    0x44,      0x44,      0x44,      0x00,      0x00,      0x00,      0x30,
    0x78,      0x48,      0x48,      0x7e,      0x7e,      0x00,      0x00,
    0x38,      0x7c,      0x54,      0x54,      0x5c,      0x18,      0x00,
    0x00,      0x00,      0x08,      0x7c,      0x7e,      0x0a,      0x0a,
    0x00,      0x00,      0x98,      0xbc,      0xa4,      0xa4,      0xfc,
    0x7c,      0x00,      0x00,      0x7e,      0x7e,      0x08,      0x08,
    0x78,      0x70,      0x00,      0x00,      0x00,      0x48,      0x7a,
    0x7a,      0x40,      0x00,      0x00,      0x00,      0x00,      0x80,
    0x80,      0x80,      0xfa,      0x7a,      0x00,      0x00,      0x7e,
    0x7e,      0x10,      0x38,      0x68,      0x40,      0x00,      0x00,
    0x00,      0x42,      0x7e,      0x7e,      0x40,      0x00,      0x00,
    0x00,      0x7c,      0x7c,      0x18,      0x38,      0x1c,      0x7c,
    0x78,      0x00,      0x7c,      0x7c,      0x04,      0x04,      0x7c,
    0x78,      0x00,      0x00,      0x38,      0x7c,      0x44,      0x44,
    0x7c,      0x38,      0x00,      0x00,      0xfc,      0xfc,      0x24,
    0x24,      0x3c,      0x18,      0x00,      0x00,      0x18,      0x3c,
    0x24,      0x24,      0xfc,      0xfc,      0x00,      0x00,      0x7c,
    0x7c,      0x04,      0x04,      0x0c,      0x08,      0x00,      0x00,
    0x48,      0x5c,      0x54,      0x54,      0x74,      0x24,      0x00,
    0x00,      0x04,      0x04,      0x3e,      0x7e,      0x44,      0x44,
    0x00,      0x00,      0x3c,      0x7c,      0x40,      0x40,      0x7c,
    0x7c,      0x00,      0x00,      0x1c,      0x3c,      0x60,      0x60,
    0x3c,      0x1c,      0x00,      0x00,      0x1c,      0x7c,      0x70,
    0x38,      0x70,      0x7c,      0x1c,      0x00,      0x44,      0x6c,
    0x38,      0x38,      0x6c,      0x44,      0x00,      0x00,      0x9c,
    0xbc,      0xa0,      0xe0,      0x7c,      0x3c,      0x00,      0x00,
    0x44,      0x64,      0x74,      0x5c,      0x4c,      0x44,      0x00,
    0x00,      0x08,      0x3e,      0x77,      0x41,      0x41,      0x00,
    0x00,      0x00,      0x00,      0x00,      0xff,      0xff,      0x00,
    0x00,      0x00,      0x00,      0x00,      0x41,      0x41,      0x77,
    0x3e,      0x08,      0x00,      0x00,      0x04,      0x02,      0x02,
    0x04,      0x04,      0x02,      0x00,      0b0000000, 0b1010101, 0b0101010,
    0b0101010, 0b0010100, 0b0010100, 0b0001000, 0b0001000, 0b00000,   0b00000,
    0b00000,   0b00000,   0b00000,   0b00000,   0b00000,   0b00000,   0b00000,
    0b00000,   0b11111,   0b00000,   0b00000,   0b00000,   0b00000,   0b00000,
    0b00000,   0b00000,   0b11111,   0b00000,   0b11111,   0b00000,   0b00000,
    0b00000,   0b00000,   0b00000,   0b11111,   0b00000,   0b11111,   0b00000,
    0b11111,   0b00000,   0b11100,   0b11110,   0b00101,   0b00101,   0b11110,
    0b11100,   0b00000,   0b00000,   0b11111,   0b11111,   0b10101,   0b10101,
    0b01010,   0b01010,   0b00000,   0b00000};

#define FONT_W 8
#define FONT_H 2
#define FONT_STRETCHV 1
#define FONT_STRETCHH 0

//=========================================================================
// SECCIÓN 11: INICIALIZACIÓN OLED
//=========================================================================

static const uint8_t oled_init_sequence[] PROGMEM = {
    0xD5, 0x80,
#ifdef CONDENSED
    0xA8, 0x3F,
#else
    0xA8, 0x1F,
#endif
    0xD3, 0x00,
#ifndef OLED_SH1106
    0x40, 0x8D, 0x14,       0x20, 0x02, 0xA4,
#endif
    0xA1, 0xC8,
#ifdef CONDENSED
    0xDA, 0x12,
#else
    0xDA, 0x02,
#endif
    0x81, 0x80, 0xDB,       0x40, 0xD9, 0xF1, 0xB0 | 0x0,
#ifdef OLED_SH1106
    0xAD, 0x8B, 0x30 | 0x2,
#endif
#ifdef INVERSE
    0xA7,
#else
    0xA6,
#endif
    0xAF};

class OLEDDevice : public Print {
public:
#define OLED_ADDR 0x3C
#define OLED_PAGES 4
#define OLED_COMMAND 0x00
#define OLED_DATA 0x40

  uint8_t oledX = 0, oledY = 0;
  uint8_t renderingFrame = 0xB0;
  bool wrap = false;

  void cmd(uint8_t b) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_COMMAND);
    Wire.write(b);
    Wire.endTransmission();
  }

  void begin(uint8_t cols, uint8_t rows, uint8_t charsize = 0) {
    Wire.begin();
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_COMMAND);
    for (uint8_t i = 0; i < sizeof(oled_init_sequence); i++) {
      Wire.write(pgm_read_byte(&oled_init_sequence[i]));
    }
    Wire.endTransmission();
    delayMicroseconds(100);
#ifdef CONDENSED
    for (uint8_t y = 0; y != rows; y++)
      for (uint8_t x = 0; x != cols; x++) {
        setCursor(x, y);
        write(' ');
      }
#endif
  }

  bool curs = false;
  void noCursor() { curs = false; }
  void cursor() { curs = true; }
  void noDisplay() { cmd(0xAE); }
  void createChar(uint8_t l, uint8_t glyph[]) {}

  void _setCursor(uint8_t x, uint8_t y) {
    oledX = x;
    oledY = y;
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_COMMAND);
    Wire.write(renderingFrame | (oledY & 0x07));
    uint8_t _oledX = oledX;
#ifdef OLED_SH1106
    _oledX += 2;
#endif
    Wire.write(0x10 | ((_oledX & 0xf0) >> 4));
    Wire.write(_oledX & 0x0f);
    Wire.endTransmission();
  }

  void drawCursor(bool en) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(OLED_DATA);
    Wire.write((en) ? 0xf0 : 0x00);
    Wire.endTransmission();
  }

  void setCursor(uint8_t x, uint8_t y) {
    if (curs)
      drawCursor(false);
    _setCursor(x * FONT_W, y * FONT_H);
    if (curs) {
      drawCursor(true);
      _setCursor(oledX, oledY);
    }
  }

  void newLine() {
    oledY += FONT_H;
    if (oledY > OLED_PAGES - FONT_H)
      oledY = OLED_PAGES - FONT_H;
    _setCursor(0, oledY);
  }

  size_t write(byte c) {
    if ((c == '\n') || (oledX > ((uint8_t)128 - FONT_W))) {
      if (wrap)
        newLine();
      return 1;
    }
    c = ((c < 9) ? (c + '~') : c) - ' ';

    uint16_t offset = ((uint16_t)c) * FONT_W / (FONT_STRETCHH + 1) * FONT_H;
    uint8_t line = FONT_H;
    do {
      if (FONT_STRETCHV)
        offset = ((uint16_t)c) * FONT_W / (FONT_STRETCHH + 1) * FONT_H /
                 (2 * FONT_STRETCHV);
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(OLED_DATA);
      for (uint8_t i = 0; i < (FONT_W / (FONT_STRETCHH + 1)); i++) {
        uint8_t b = pgm_read_byte(&(font[offset++]));
        if (FONT_STRETCHV) {
          uint8_t b2 = 0;
          if (line > 1)
            for (int i = 0; i != 4; i++)
              b2 |=
                  (b & (1 << i)) ? (1 << (i * 2)) | (1 << ((i * 2) + 1)) : 0x00;
          else
            for (int i = 0; i != 4; i++)
              b2 |= (b & (1 << (i + 4))) ? (1 << (i * 2)) | (1 << ((i * 2) + 1))
                                         : 0x00;
          Wire.write(b2);
          if (FONT_STRETCHH)
            Wire.write(b2);
        } else {
          Wire.write(b);
          if (FONT_STRETCHH)
            Wire.write(b);
        }
      }
      Wire.endTransmission();
      if (FONT_H == 1)
        oledX += FONT_W;
      else {
        if (line > 1)
          _setCursor(oledX, oledY + 1);
        else
          _setCursor(oledX + FONT_W, oledY - (FONT_H - 1));
      }
    } while (--line);
    return 1;
  }

  void bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
              const uint8_t bitmap[]) {
    uint16_t j = 0;
    for (uint8_t y = y0; y < y1; y++) {
      _setCursor(x0, y);
      Wire.beginTransmission(OLED_ADDR);
      Wire.write(OLED_DATA);
      for (uint8_t x = x0; x < x1; x++) {
        Wire.write(pgm_read_byte(&bitmap[j++]));
      }
      Wire.endTransmission();
    }
    setCursor(0, 0);
  }
};

//=========================================================================
// SECCIÓN 12: WRAPPER DE DISPLAY PARA CAT EXT
//=========================================================================

template <class parent> class Display : public parent {
public:
#ifdef CAT_EXT
  uint8_t x, y;
  bool curs;
  char text[2 * 16 + 1];
  Display() : parent() { clear(); };
  size_t write(uint8_t b) {
    if ((x < 16) && (y < 2)) {
      text[y * 16 + x] = ((b < 9) ? "> :*#AB"[b - 1] : b);
      x++;
    }
    return parent::write(b);
  }
  void setCursor(uint8_t _x, uint8_t _y) {
    x = _x;
    y = _y;
    parent::setCursor(_x, _y);
  }
  void cursor() {
    curs = true;
    parent::cursor();
  }
  void noCursor() {
    curs = false;
    parent::noCursor();
  }
  void clear() {
    for (uint8_t i = 0; i != 2 * 16; i++)
      text[i] = ' ';
    text[2 * 16] = '\0';
    x = 0;
    y = 0;
  }
#endif
};

//=========================================================================
// SECCIÓN 13: SELECCIÓN DE TIPO DE DISPLAY
//=========================================================================

#ifdef BLIND
class Blind : public Print {
public:
  size_t write(uint8_t b) {}
  void setCursor(uint8_t _x, uint8_t _y) {}
  void cursor() {}
  void noCursor() {}
  void begin(uint8_t x = 0, uint8_t y = 0) {}
  void noDisplay() {}
  void createChar(uint8_t l, uint8_t glyph[]) {}
};
Display<Blind> lcd;
#elif defined(OLED)
Display<OLEDDevice> lcd;
#else
Display<LCD> lcd;
#endif

//=========================================================================
// SECCIÓN 14: ENCODER ROTATORIO - INTERRUPCIÓN
//=========================================================================

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

#ifdef THREEBUTTONROT
void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
}
void CheckRotButton() {
  unsigned int reading = digitalRead(ROT_B) << 1 | digitalRead(ROT_A);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == 2 || buttonState == 1) {
        if (buttonState == 2)
          encoder_val++;
        else
          encoder_val--;
        lastRepeatTime = millis();
        repeating = false;
      } else {
        repeating = false;
      }
    }
  }
  if (buttonState == 2 || buttonState == 1) {
    unsigned long now = millis();
    if (!repeating && (now - lastRepeatTime > initialDelay)) {
      repeating = true;
      lastRepeatTime = now;
    }
    if (repeating && (now - lastRepeatTime > repeatDelay)) {
      if (buttonState == 2)
        encoder_val++;
      else
        encoder_val--;
      lastRepeatTime = now;
    }
  }
  lastButtonState = reading;
}
#else
void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |= (1 << PCINT22) | (1 << PCINT23);
  PCICR |= (1 << PCIE2);
  last_state = (_digitalRead(ROT_B) << 1) | _digitalRead(ROT_A);
  interrupts();
}
#endif

//=========================================================================
// SECCIÓN 15: CLASE I2C PRINCIPAL
//=========================================================================

class I2C {
public:
#if (F_MCU > 20900000)
#define I2C_DELAY 6
#else
#define I2C_DELAY 4
#endif

#define I2C_DDR DDRC
#define I2C_PIN PINC
#define I2C_PORT PORTC
#define I2C_SDA (1 << 4)
#define I2C_SCL (1 << 5)
#define DELAY(n)                                                               \
  for (uint8_t i = 0; i != n; i++)                                             \
    asm("nop");

#define I2C_SDA_GET() I2C_PIN &I2C_SDA
#define I2C_SCL_GET() I2C_PIN &I2C_SCL
#define I2C_SDA_HI() I2C_DDR &= ~I2C_SDA;
#define I2C_SDA_LO() I2C_DDR |= I2C_SDA;
#define I2C_SCL_HI()                                                           \
  I2C_DDR &= ~I2C_SCL;                                                         \
  DELAY(I2C_DELAY);
#define I2C_SCL_LO()                                                           \
  I2C_DDR |= I2C_SCL;                                                          \
  DELAY(I2C_DELAY);

  I2C() {
    I2C_PORT &= ~(I2C_SDA | I2C_SCL);
    I2C_SCL_HI();
    I2C_SDA_HI();
#ifndef RS_HIGH_ON_IDLE
    suspend();
#endif
  }
  ~I2C() {
    I2C_PORT &= ~(I2C_SDA | I2C_SCL);
    I2C_DDR &= ~(I2C_SDA | I2C_SCL);
  }

  inline void start() {
#ifdef RS_HIGH_ON_IDLE
    I2C_SDA_LO();
#else
    resume();
#endif
    I2C_SCL_LO();
    I2C_SDA_HI();
  }

  inline void stop() {
    I2C_SDA_LO();
    I2C_SCL_HI();
    I2C_SDA_HI();
    I2C_DDR &= ~(I2C_SDA | I2C_SCL);
#ifndef RS_HIGH_ON_IDLE
    suspend();
#endif
  }

#define SendBit(data, mask)                                                    \
  if (data & mask) {                                                           \
    I2C_SDA_HI();                                                              \
  } else {                                                                     \
    I2C_SDA_LO();                                                              \
  }                                                                            \
  I2C_SCL_HI();                                                                \
  I2C_SCL_LO();

  inline void SendByte(uint8_t data) {
    SendBit(data, 1 << 7);
    SendBit(data, 1 << 6);
    SendBit(data, 1 << 5);
    SendBit(data, 1 << 4);
    SendBit(data, 1 << 3);
    SendBit(data, 1 << 2);
    SendBit(data, 1 << 1);
    SendBit(data, 1 << 0);
    I2C_SDA_HI();
    DELAY(I2C_DELAY);
    I2C_SCL_HI();
    I2C_SCL_LO();
  }

  inline uint8_t RecvBit(uint8_t mask) {
    I2C_SCL_HI();
    uint16_t i = 60000;
    for (; !(I2C_SCL_GET()) && i; i--)
      ;
    uint8_t data = I2C_SDA_GET();
    I2C_SCL_LO();
    return data ? mask : 0;
  }

  inline uint8_t RecvByte(uint8_t last) {
    uint8_t data = 0;
    data |= RecvBit(1 << 7);
    data |= RecvBit(1 << 6);
    data |= RecvBit(1 << 5);
    data |= RecvBit(1 << 4);
    data |= RecvBit(1 << 3);
    data |= RecvBit(1 << 2);
    data |= RecvBit(1 << 1);
    data |= RecvBit(1 << 0);
    if (last) {
      I2C_SDA_HI();
      DELAY(I2C_DELAY);
      I2C_SCL_LO();
    } else {
      I2C_SDA_LO();
      I2C_SCL_HI();
      DELAY(I2C_DELAY);
      I2C_SCL_LO();
      I2C_SDA_HI();
    }
    return data;
  }

  inline void resume() {
#ifdef LCD_RS_PORTIO
    I2C_PORT &= ~I2C_SDA;
#endif
  }

  inline void suspend() { I2C_SDA_LO(); }

  void begin() {};
  void beginTransmission(uint8_t addr) {
    start();
    SendByte(addr << 1);
  };
  bool write(uint8_t byte) {
    SendByte(byte);
    return 1;
  };
  uint8_t endTransmission() {
    stop();
    return 0;
  };
};

I2C i2c;

//=========================================================================
// SECCIÓN 16: UTILIDADES MATEMÁTICAS
//=========================================================================

uint8_t log2(uint16_t x) {
  uint8_t y = 0;
  for (; x >>= 1;)
    y++;
  return y;
}

//=========================================================================
// SECCIÓN 17: CLASE SI5351 - SINTETIZADOR
//=========================================================================

class SI5351 {
public:
  volatile int32_t _fout;
  volatile uint8_t _div;
  volatile uint16_t _msa128min512;
  volatile uint32_t _msb128;
  volatile uint8_t pll_regs[8];
  volatile uint32_t fxtal = F_XTAL;

#define BB0(x) ((uint8_t)(x))
#define BB1(x) ((uint8_t)((x) >> 8))
#define BB2(x) ((uint8_t)((x) >> 16))
#define FAST __attribute__((optimize("Ofast")))

  inline void FAST freq_calc_fast(int16_t df) {
#define _MSC 0x10000
    uint32_t msb128 =
        _msb128 + ((int64_t)(_div * (int32_t)df) * _MSC * 128) / fxtal;
    uint16_t msp1 = _msa128min512 + msb128 / _MSC;
    uint16_t msp2 = msb128;
    pll_regs[4] = BB0(msp1);
    pll_regs[5] = ((_MSC & 0xF0000) >> (16 - 4));
    pll_regs[6] = BB1(msp2);
    pll_regs[7] = BB0(msp2);
  }

  inline void SendPLLRegisterBulk() {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(26 + 0 * 8 + 4);
    i2c.SendByte(pll_regs[4]);
    i2c.SendByte(pll_regs[5]);
    i2c.SendByte(pll_regs[6]);
    i2c.SendByte(pll_regs[7]);
    i2c.stop();
  }

  void SendRegister(uint8_t reg, uint8_t *data, uint8_t n) {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    while (n--)
      i2c.SendByte(*data++);
    i2c.stop();
  }
  void SendRegister(uint8_t reg, uint8_t val) { SendRegister(reg, &val, 1); }
  int16_t iqmsa;

  enum ms_t {
    PLLA = 0,
    PLLB = 1,
    MSNA = -2,
    MSNB = -1,
    MS0 = 0,
    MS1 = 1,
    MS2 = 2,
    MS3 = 3,
    MS4 = 4,
    MS5 = 5
  };

  void ms(int8_t n, uint32_t div_nom, uint32_t div_denom, uint8_t pll = PLLA,
          uint8_t _int = 0, uint16_t phase = 0, uint8_t rdiv = 0) {
    uint16_t msa = div_nom / div_denom;
    if (msa == 4)
      _int = 1;
    uint32_t msb =
        (_int) ? 0 : (((uint64_t)(div_nom % div_denom) * _MSC) / div_denom);
    uint32_t msc = (_int) ? 1 : _MSC;
    uint32_t msp1 = 128 * msa + 128 * msb / msc - 512;
    uint32_t msp2 = 128 * msb - 128 * msb / msc * msc;
    uint32_t msp3 = msc;
    uint8_t ms_reg2 = BB2(msp1) | (rdiv << 4) | ((msa == 4) * 0x0C);
    uint8_t ms_regs[8] = {BB1(msp3), BB0(msp3),
                          ms_reg2,   BB1(msp1),
                          BB0(msp1), BB2(((msp3 & 0x0F0000) << 4) | msp2),
                          BB1(msp2), BB0(msp2)};
    SendRegister(n * 8 + 42, ms_regs, 8);
    if (n < 0) {
      SendRegister(n + 16 + 8, 0x80 | (0x40 * _int));
    } else {
      SendRegister(n + 16, ((pll) * 0x20) | 0x0C | 3 | (0x40 * _int));
      SendRegister(n + 165, (!_int) * phase * msa / 90);
    }
  }

  void phase(int8_t n, uint32_t div_nom, uint32_t div_denom, uint16_t phase) {
    SendRegister(n + 165, phase * (div_nom / div_denom) / 90);
  }

  void reset() { SendRegister(177, 0xA0); }
  void oe(uint8_t mask) { SendRegister(3, ~mask); }

  void freq(int32_t fout, uint16_t i, uint16_t q) {
    uint8_t rdiv = 0;
    if (fout > 300000000) {
      i /= 3;
      q /= 3;
      fout /= 3;
    }
    if (fout < 500000) {
      rdiv = 7;
      fout *= 128;
    }
    uint16_t d;
    if (fout < 30000000)
      d = (16 * fxtal) / fout;
    else
      d = (32 * fxtal) / fout;
    if (fout < 3500000)
      d = (7 * fxtal) / fout;
    if (fout > 140000000)
      d = 4;
    if (d % 2)
      d++;
    if ((d * (fout - 5000) / fxtal) != (d * (fout + 5000) / fxtal))
      d += 2;
    uint32_t fvcoa = d * fout;
    ms(MSNA, fvcoa, fxtal);
    ms(MS0, fvcoa, fout, PLLA, 0, i, rdiv);
    ms(MS1, fvcoa, fout, PLLA, 0, q, rdiv);
#ifdef F_CLK2
    freqb(F_CLK2);
#else
    ms(MS2, fvcoa, fout, PLLA, 0, 0, rdiv);
#endif
    if (iqmsa != (((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90)) {
      iqmsa = ((int8_t)i - (int8_t)q) * ((int16_t)(fvcoa / fout)) / 90;
      reset();
    }
    oe(0b00000011);
    _fout = fout;
    _div = d;
    _msa128min512 = fvcoa / fxtal * 128 - 512;
    _msb128 = ((uint64_t)(fvcoa % fxtal) * _MSC * 128) / fxtal;
  }

  void freqb(uint32_t fout) {
    uint16_t d = (16 * fxtal) / fout;
    if (d % 2)
      d++;
    uint32_t fvcoa = d * fout;
    ms(MSNB, fvcoa, fxtal);
    ms(MS2, fvcoa, fout, PLLB, 0, 0, 0);
  }

  uint8_t RecvRegister(uint8_t reg) {
    i2c.start();
    i2c.SendByte(SI5351_ADDR << 1);
    i2c.SendByte(reg);
    i2c.stop();
    i2c.start();
    i2c.SendByte((SI5351_ADDR << 1) | 1);
    uint8_t data = i2c.RecvByte(true);
    i2c.stop();
    return data;
  }

  void powerDown() {
    SendRegister(3, 0b11111111);
    SendRegister(24, 0b00010000);
    SendRegister(25, 0b00000000);
    for (int addr = 16; addr != 24; addr++)
      SendRegister(addr, 0b10000000);
    SendRegister(187, 0);
    SendRegister(149, 0);
    SendRegister(183, 0b11010010);
  }

#define SI_CLK_OE 3
};
static SI5351 si5351;

//=========================================================================
// SECCIÓN 18: CONTROL DE FILTROS LPF
//=========================================================================

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
class PCA9536 {
public:
#define PCA9536_ADDR 0x41
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(PCA9536_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() { SendRegister(0x03, 0x00); }
  inline void write(uint8_t data) {
    init();
    SendRegister(0x01, data);
  }
};
PCA9536 ioext;
void set_latch(uint8_t io) {
#define LATCH_TIME 15
  for (int i = 0; i != 8; i++) {
    ioext.write((~(1 << i)) | 0x01);
    delay(LATCH_TIME);
  }
  ioext.write(0x00);
  ioext.write((1 << io) | 0x00);
  delay(LATCH_TIME);
  ioext.write(0x00);
}
static uint8_t prev_lpf_io = 0xff;
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io = (f > 8) ? 1 : (f > 4) ? 2 : 3;
  if (prev_lpf_io != lpf_io) {
    prev_lpf_io = lpf_io;
    set_latch(lpf_io);
  }
}
#endif

#if defined(LPF_SWITCHING_DL2MAN_USDX_REV3) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA)
class IOExpander16 {
public:
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
#define IOEXP16_ADDR 0x74
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2
#define IOEXP16_ADDR 0x24
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
#define IOEXP16_ADDR 0x20
#endif
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(IOEXP16_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() { write(0U); }
  inline void write(uint16_t data) {
    SendRegister(0x07, 0xff);
    SendRegister(0x06, 0xff);
    SendRegister(0x02, data);
    SendRegister(0x06, 0x00);
    SendRegister(0x03, data >> 8);
    SendRegister(0x07, 0x00);
  }
};
IOExpander16 ioext;
enum gpioext_t {
  IO0_0,
  IO0_1,
  IO0_2,
  IO0_3,
  IO0_4,
  IO0_5,
  IO0_6,
  IO0_7,
  IO1_0,
  IO1_1,
  IO1_2,
  IO1_3,
  IO1_4,
  IO1_5,
  IO1_6,
  IO1_7
};
void set_latch(uint8_t io, uint8_t common_io, bool latch = true) {
#define LATCH_TIME 30
  if (latch) {
    ioext.write((1U << io) | 0x0000);
    delay(LATCH_TIME);
    ioext.write(0x0000);
  } else {
    if (io == 0xff) {
      ioext.init();
      for (int i = 0; i != 16; i++)
        set_latch(i, common_io, latch);
    } else {
      ioext.write((~(1U << io)) | (1U << common_io));
      delay(LATCH_TIME);
      ioext.write(0x0000);
    }
  }
}
static uint8_t prev_lpf_io = 0xff;
inline void set_lpf(uint8_t f) {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  uint8_t lpf_io = (f > 26)   ? IO1_3
                   : (f > 20) ? IO1_4
                   : (f > 17) ? IO1_2
                   : (f > 12) ? IO1_5
                   : (f > 8)  ? IO1_1
                   : (f > 5)  ? IO1_6
                   : (f > 4)  ? IO1_0
                              : IO1_7;
#ifndef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_0, false);
    set_latch(lpf_io, IO0_0);
    prev_lpf_io = lpf_io;
  }
#else
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }
#endif
#else
  uint8_t lpf_io = (f > 12)  ? IO0_3
                   : (f > 8) ? IO0_5
                   : (f > 5) ? IO0_7
                   : (f > 4) ? IO1_1
                             : IO1_3;
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_1, false);
    set_latch(lpf_io, IO0_1);
    prev_lpf_io = lpf_io;
  }
#endif
}
#endif

#ifdef LPF_SWITCHING_WB2CBA_USDX_OCTOBAND
class MCP23008 {
public:
#define MCP23008_ADDR 0x20
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(MCP23008_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    SendRegister(0x09, 0x00);
    SendRegister(0x00, 0x00);
  }
  inline void write(uint16_t data) { SendRegister(0x09, data); }
};
MCP23008 ioext;
static uint8_t prev_lpf_io = 0xff;
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io = (f > 26)   ? 7
                   : (f > 20) ? 6
                   : (f > 17) ? 5
                   : (f > 12) ? 4
                   : (f > 8)  ? 3
                   : (f > 6)  ? 2
                   : (f > 4)  ? 1
                              : 0;
  if (prev_lpf_io == 0xff)
    ioext.init();
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }
}
#endif

#if defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {
  pinMode(PD5, OUTPUT);
  digitalWrite(PD5, (f >= LPF_SWITCHING_PE1DDA_USDXDUO));
}
#endif

#if !defined(LPF_SWITCHING_DL2MAN_USDX_REV1) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA) &&                           \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV3) &&                                \
    !defined(LPF_SWITCHING_WB2CBA_USDX_OCTOBAND) &&                            \
    !defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {}
#endif

//=========================================================================
// SECCIÓN 19: CONSTANTES DE PROCESAMIENTO DSP
//=========================================================================

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
#define MORE_MIC_GAIN 1     // Adds more microphone gain, improving overall SSB quality (critical for TX to work!)
#define TX_POWER_RAMP 1     // Smooth power ramping at TX start/end to eliminate clicks

// Improved magnitude approximation: error 0.95dB -> 0.4dB (-0.55dB improvement)
#define magn(i,q) ({ \
  int16_t _i = abs(i), _q = abs(q); \
  (_i > _q) ? (_i + (_q >> 2) + (_q >> 4)) : (_q + (_i >> 2) + (_i >> 4)); \
})

#define AF_OUT 1 // Habilita salida de audio por PWM

//=========================================================================
// SECCIÓN 20: FUNCIONES MATEMÁTICAS DSP
//=========================================================================

const uint16_t arctantab[33] PROGMEM = {
    0,   24,  48,  71,  95,  118, 142, 165, 187, 209, 231,
    253, 274, 295, 315, 335, 354, 373, 391, 409, 427, 444,
    460, 476, 492, 507, 521, 535, 549, 562, 575, 588, 600};

inline int16_t arctan3(int16_t q, int16_t i) {
  int16_t r;
  if (abs(q) > abs(i))
    r = _UA / 4 - pgm_read_word(&arctantab[(abs(i) * 32) / abs(q)]);
  else
    r = (i == 0) ? 0 : pgm_read_word(&arctantab[(abs(q) * 32) / abs(i)]);
  r = (i < 0) ? _UA / 2 - r : r;
  return (q < 0) ? -r : r;
}

//=========================================================================
// SECCIÓN 21: MACROS AGC
//=========================================================================

#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha)
#define HI(x) ((x) >> 8)
#define LO(x) ((x) & 0xFF)

//=========================================================================
// SECCIÓN 22: PROCESAMIENTO AGC
//=========================================================================

// Removed hardcoded DECAY_FACTOR - now uses settings.h defines based on AGC mode
#define HI(x)  ((x) >> 8)
#define LO(x)  ((x) & 0xFF)

static int16_t centiGain = 128;
static int16_t gain = 1024;

inline int16_t process_agc_fast(int16_t in) {
  int16_t out = (gain >= 1024) ? (gain >> 10) * in : in;
  int16_t accum = max(0, 1 - abs(out >> 10));
  if ((INT16_MAX - gain) > accum)
    gain = gain + accum;
  if (gain < 1)
    gain = 1;
  return out;
}

inline int16_t process_agc(int16_t in) {
  in = constrain(in, -4096, 4095);  // Pre-AGC limiter for extreme signals
  static bool small = true;
  static uint16_t decayCount = AGC_MEDIUM_DECAY; // Initial value
  int16_t out;

  if (centiGain >= 128)
    out = (centiGain >> 5) * in;
  else
    out = (centiGain >> 2) * (in >> 3);
  out >>= 2;

  if (HI(abs(out)) > HI(AGC_ATTACK_THRESHOLD)) {
    centiGain -= (centiGain >> 4);
  } else {
    if (HI(abs(out)) > HI(AGC_DECAY_THRESHOLD))
      small = false;
    if (--decayCount == 0) {
      if (small) {
        if (centiGain < (INT16_MAX - (INT16_MAX >> 4)))
          centiGain += (centiGain >> 4);
        else
          centiGain = INT16_MAX;
      }
      // Dynamic decay factor based on AGC mode
      decayCount = (agc == 2) ? AGC_MEDIUM_DECAY : (agc == 3) ? AGC_SLOW_DECAY : AGC_MEDIUM_DECAY;
      small = true;
    }
  }
  return out;
}

//=========================================================================
// SECCIÓN 23: REDUCCIÓN DE RUIDO
//=========================================================================

inline int16_t process_nr(int16_t in) {
  static int16_t ea1;
  int16_t tmp = ea1;
  ea1 = EA(tmp, in, 1 << (nr - 1));
  return ea1;
}

#if defined(NOISE_BLANKER_ENABLE) || defined(NOTCH_ENABLE) || defined(NR_ENABLE)
//=========================================================================
// SECCIÓN 24B: PROCESAMIENTO ADICIONAL DE SEÑAL
//=========================================================================

inline int16_t process_noise_blanker(int16_t in) {
#if NOISE_BLANKER_ENABLE
  int16_t abs_in = abs(in);
  if (noise_blank_count > 0) {
    noise_blank_count--;
    return last_valid_sample;
  }
  if (abs_in > noise_floor_slow + (noise_floor_fast << 1)) {
    noise_blank_count = 8;
    return last_valid_sample;
  }
  last_valid_sample = in;
  return in;
#else
  return in;
#endif
}

inline int16_t process_notch(int16_t in) {
#if NOTCH_ENABLE
  float x = (float)in;
  float y = notch_b0 * x + notch_b1 * notch_x1 + notch_b2 * notch_x2 -
            notch_a1 * notch_y1 - notch_a2 * notch_y2;
  notch_x2 = notch_x1;
  notch_x1 = x;
  notch_y2 = notch_y1;
  notch_y1 = y;
  return (int16_t)y;
#else
  return in;
#endif
}

inline void init_notch_filter() {
#if NOTCH_ENABLE
  float fs = 7812.5f;
  float f0 = (float)NOTCH_FREQ;
  float Q = 15.0f;
  float w0 = 2.0f * 3.14159f * f0 / fs;
  float alpha = sin(w0) / (2.0f * Q);
  float b0 = 1.0f + alpha;
  notch_b0 = 1.0f;
  notch_b1 = -2.0f * cos(w0);
  notch_b2 = 1.0f;
  notch_a1 = -2.0f * cos(w0);
  notch_a2 = 1.0f - alpha;
  notch_b0 /= b0;
  notch_b1 /= b0;
  notch_b2 /= b0;
  notch_a1 /= b0;
  notch_a2 /= b0;
#endif
}

inline uint16_t get_version_id() {
  uint16_t major = (uint16_t)(VERSION[0] - '1');
  uint16_t minor =
      (uint16_t)(VERSION[2] - '0') * 10u + (uint16_t)(VERSION[3] - '0');
  uint16_t suffix = (uint16_t)(VERSION[4] ? (VERSION[4] - 'a' + 1) : 0);
  return (major << 12) + (minor << 8) + suffix;
}

#endif

//=========================================================================
// SECCIÓN 24: PROCESAMIENTO LENTO DSP
//=========================================================================

#define MIC_ATTEN 0

int16_t vi[7];

inline int16_t slow_dsp(int16_t ac) {
  static uint8_t absavg256cnt;
  if (!(absavg256cnt--)) {
    _absavg256 = absavg256;
    absavg256 = 0;
  } else
    absavg256 += abs(ac);

  if (mode == AM) {
    ac = magn(i, q);
    static int32_t dc_avg = 0;
    dc_avg = (dc_avg * 63 + ac) / 64;
    ac = ac - dc_avg;
  } else if (mode == FM) {
    static int16_t prev_i = 0, prev_q = 0;
    int32_t product = (int32_t)i * prev_q - (int32_t)q * prev_i;
    int32_t magnitude_sq = (int32_t)i * i + (int32_t)q * q;
    if (magnitude_sq > 1000)
      ac = (product << 4) / (magnitude_sq >> 3);
    else
      ac = 0;
    prev_i = i;
    prev_q = q;
    static int16_t fm_lpf = 0;
    fm_lpf = (fm_lpf * 3 + ac) / 4;
    ac = fm_lpf;
    // Pre-emphasis HPF (300Hz) to restore natural voice
    static int16_t fm_hpf_z1 = 0;
    int16_t fm_hpf = ac - ((ac + fm_hpf_z1 * 15) >> 4);
    fm_hpf_z1 = fm_hpf;
    ac = fm_hpf;
  } else {
    // USB, LSB, CW - no additional processing needed here
  }

#ifdef FAST_AGC
  if (agc == 2) {
    ac = process_agc(ac);
    ac = ac >> (16 - volume);
  } else if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
  } else {
    if (volume <= 13)
      ac = ac >> (13 - volume);
    else
      ac = ac << (volume - 13);
  }
#else
  if (agc == 1) {
    ac = process_agc_fast(ac);
    ac = ac >> (16 - volume);
  } else {
    if (volume <= 13)
      ac = ac >> (13 - volume);
    else
      ac = ac << (volume - 13);
  }
#endif

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

//=========================================================================
// SECCIÓN 25: FILTROS VARIABLES (EXTERNAL)
//=========================================================================

extern int16_t filt_var(int16_t in);

//=========================================================================
// SECCIÓN 26: GENERACIÓN SSB
//=========================================================================

inline int16_t ssb(int16_t in)
{
  static int16_t dc, z1;

  int16_t i, q;
  uint8_t j;
  static int16_t v[16];
  for(j = 0; j != 15; j++) v[j] = v[j + 1];
#ifdef MORE_MIC_GAIN
//#define DIG_MODE  // optimization for digital modes: for super flat TX spectrum, (only down < 100Hz to cut-off DC components)
#ifdef DIG_MODE
  int16_t ac = in;
  dc = (ac + (7) * dc) / (7 + 1);  // hpf: slow average
  v[15] = (ac - dc) / 2;           // hpf (dc decoupling)  (-6dB gain to compensate for DC-noise)
#else
  int16_t ac = in * 2; //   6dB gain (justified since lpf/hpf is losing -3dB)
  ac = ac + z1;        // lpf
  z1 = (in - (8) * z1) / (8 + 1); // lpf

  // smooth clipping limiter 
  if (ac > 250) {
    ac = 250 + (ac - 250) / 2; 
  } else if (ac < -250) {
    ac = -250 - (-250 - ac) / 2;
  }

  dc = (ac + (2) * dc) / (2 + 1);
  v[15] = (ac - dc);
#endif //DIG_MODE
  i = v[7] * 2;  // 6dB gain for i, q  (to prevent quanitization issues in hilbert transformer and phase calculation, corrected for magnitude calc)
  q = ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 16) / 64 + (v[6] - v[8]); // Hilbert transform, 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection scenario; (Hilbert transform require 5 additional bits)

  uint16_t _amp = magn(i / 2, q / 2);  // -6dB gain (correction)
#else  // !MORE_MIC_GAIN
  //dc += (in - dc) / 2;       // fast moving average
  dc = (in + dc) / 2;        // average
  int16_t ac = (in - dc);   // DC decoupling
  //v[15] = ac;// - z1;        // high-pass (emphasis) filter
  v[15] = (ac + z1);// / 2;           // low-pass filter with notch at Fs/2
  z1 = ac;

  i = v[7];
  q = ((v[0] - v[14]) * 2 + (v[2] - v[12]) * 8 + (v[4] - v[10]) * 21 + (v[6] - v[8]) * 15) / 128 + (v[6] - v[8]) / 2; // Hilbert transform, 40dB side-band rejection in 400..1900Hz (@4kSPS) when used in image-rejection scenario; (Hilbert transform require 5 additional bits)

  uint16_t _amp = magn(i, q);
#endif  // MORE_MIC_GAIN

#ifdef CARRIER_COMPLETELY_OFF_ON_LOW
  _vox(_amp > vox_thresh);
#else
  if(vox) _vox(_amp > vox_thresh);
#endif

  _amp = _amp << (drive);
  _amp = ((_amp > 255) || (drive == 8)) ? 255 : _amp; // clip or when drive=8 use max output
  amp = (tx) ? lut[_amp] : 0;

#ifdef TX_POWER_RAMP
  static uint8_t prev_tx_state = 0;
  if (tx && !prev_tx_state) {
    start_tx_ramp(amp);
  }
  if (!tx && prev_tx_state) {
    start_tx_ramp(0);
  }
  prev_tx_state = tx;
#endif

  static int16_t prev_phase;
  int16_t phase = arctan3(q, i);

  int16_t dp = phase - prev_phase;  // phase difference and restriction
  //dp = (amp) ? dp : 0;  // dp = 0 when amp = 0
  prev_phase = phase;

  if(dp < 0) dp = dp + _UA; // make negative phase shifts positive: prevents negative frequencies and will reduce spurs on other sideband
#ifdef QUAD
  if(dp >= (_UA/2)){ dp = dp - _UA/2; quad = !quad; }
#endif

#ifdef MAX_DP
  if(dp > MAX_DP){ // dp should be less than half unit-angle in order to keep frequencies below F_SAMP_TX/2
    prev_phase = phase - (dp - MAX_DP);  // substract restdp
    dp = MAX_DP;
  }
#endif
  if(mode == USB)
    return dp * ( _F_SAMP_TX / _UA); // calculate frequency-difference based on phase-difference
  else
    return dp * (-_F_SAMP_TX / _UA);
}


//=========================================================================

inline void _vox(bool trigger)
{
  if(trigger){
    tx = (tx) ? 254 : 255; // hangtime = 255 / 4402 = 58ms (the time that TX at least stays on when not triggered again). tx == 255 when triggered first, 254 follows for subsequent triggers, until tx is off.
  } else {
    if(tx) tx--;
  }
}

inline uint8_t get_vox_thresh() {
  return vox_thresh;
}

void init_vox_thresh() {
#if MORE_MIC_GAIN
  vox_thresh = (1 << 2);
#else
  vox_thresh = (1 << 1);
#endif
}

//=========================================================================
// SECCIÓN 28: TRANSMISIÓN SSB
//=========================================================================

static int16_t _adc;

// TX Power Ramping - Smooth power transition at TX start/end
#ifdef TX_POWER_RAMP
static uint8_t tx_ramp_counter = 0;    // 0 = no ramping, 1-32 = ramping up/down
static uint8_t tx_ramp_target_amp = 0; // target amplitude for ramp
static uint8_t tx_ramp_current_amp = 0; // current amplitude during ramp

// Ramping curve: smooth S-curve for natural sound
// Values represent percentage of full power at each step
// Extended to 42 elements for symmetric UP/DOWN ramps
const uint8_t tx_ramp_curve[42] PROGMEM = {
    // UP ramp (indices 0-16): 0% -> 100%
    0,   1,   2,   4,   7,   11,  16,  22,  29, 37, 46, 55, 65, 74, 83, 91, 97,
    // Hold (indices 17-24): 100%
    100, 100, 100, 100, 100, 100, 100, 100,
    // DOWN ramp (indices 25-40): 100% -> 0% (mirror of UP)
    97, 91, 83, 74, 65, 55, 46, 37, 29, 22, 16, 11, 7, 4, 2, 1, 0};

inline void apply_tx_ramp(uint8_t *amp_ptr) {
#ifdef TX_POWER_RAMP
  if (tx_ramp_counter > 0) {
    uint8_t ramp_step = tx_ramp_counter;
    uint8_t ramp_percent;

    if (tx_ramp_counter <= 32) {
      // Ramping up: read indices 16->0
      ramp_percent = pgm_read_byte_near(&tx_ramp_curve[16 - (tx_ramp_counter - 1)]);
    } else {
      // Ramping down: read indices 25->40
      ramp_percent = pgm_read_byte_near(&tx_ramp_curve[24 + (tx_ramp_counter - 32)]);
    }

    *amp_ptr = (*amp_ptr * ramp_percent) / 100;
    tx_ramp_counter--;
  }
#endif
}

inline void start_tx_ramp(uint8_t target_amp) {
#ifdef TX_POWER_RAMP
  tx_ramp_target_amp = target_amp;
  tx_ramp_current_amp = target_amp;
  tx_ramp_counter = 32; // 32 steps of ramping
#endif
}
#endif

void dsp_tx() {
#ifdef MULTI_ADC
  int16_t adc = ADC;
  ADCSRA |= (1 << ADSC);
  ADCSRA |= (1 << ADSC);
  si5351.SendPLLRegisterBulk();

  // Apply TX power ramp if active
#ifdef TX_POWER_RAMP
  uint8_t out_amp = amp;
  apply_tx_ramp(&out_amp);
  OCR1BL = lut[out_amp];
#else
  OCR1BL = amp;
#endif

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

  // Apply TX power ramp if active
#ifdef TX_POWER_RAMP
  uint8_t out_amp = amp;
  apply_tx_ramp(&out_amp);
  OCR1BL = lut[out_amp];
#else
  OCR1BL = amp;
#endif
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

//=========================================================================
// SECCIÓN 29: TRANSMISIÓN CW
//=========================================================================

const uint32_t tones[] = { F_MCU * 700ULL / 20000000, F_MCU * 600ULL / 20000000, F_MCU * 700ULL / 20000000};

volatile int8_t p_sin = 0;
volatile int8_t n_cos = 448 / 4;

inline void process_minsky() {
  int8_t alpha127 = tones[cw_tone] * 798 / _F_SAMP_TX;
  p_sin += alpha127 * n_cos / 127;
  n_cos -= alpha127 * p_sin / 127;
}

const uint8_t ramp[] PROGMEM = {255, 254, 252, 249, 245, 239, 233, 226,
                                217, 208, 198, 187, 176, 164, 152, 139,
                                127, 115, 102, 90,  78,  67,  56,  46,
                                37,  28,  21,  15,  9,   5,   2};

void dsp_tx_cw() {
#ifdef KEY_CLICK
  static uint8_t cw_ramp_step = 0;  // Non-blocking ramp state

  if (cw_ramp_step > 0) {
    OCR1BL = lut[pgm_read_byte_near(&ramp[cw_ramp_step - 1])];
    cw_ramp_step--;
    process_minsky();
    OCR1AL = (p_sin >> (16 - volume)) + 128;
    return;  // Continue ramp in next ISR call
  }

  if (OCR1BL < lut[255]) {
    cw_ramp_step = 31;  // Start ramp
  }
#endif
  OCR1BL = lut[255];
  process_minsky();
  OCR1AL = (p_sin >> (16 - volume)) + 128;
}

//=========================================================================
// SECCIÓN 30: TRANSMISIÓN AM
//=========================================================================

void dsp_tx_am() {
  ADCSRA |= (1 << ADSC);
  OCR1BL = amp;
  int16_t adc = ADC - 512;
  int16_t in = (adc >> MIC_ATTEN);
  in = in << (drive-4);
#define AM_BASE 32
  in = max(0, min(255, (in + AM_BASE)));
  amp = in;
}

//=========================================================================
// SECCIÓN 31: TRANSMISIÓN FM
//=========================================================================

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

//=========================================================================
// SECCIÓN 32: CW DECODER
//=========================================================================

void stepsize_showcursor();

#ifdef CW_DECODER
const char m2c[] PROGMEM = "~ "
                           "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/"
                           "***H*7*G*8*90************?_****\"**.****@***'**-***"
                           "*****;!*)*****,****:****";

void printsym(bool submit = true) {
  if (sym < 128) {
    char ch = pgm_read_byte_near(m2c + sym);
    if (ch != '*') {
#ifdef CW_INTERMEDIATE
      out[15] = ch;
      cw_event = true;
      if (submit) {
        for (int i = 0; i != 15; i++)
          out[i] = out[i + 1];
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

inline void cw_decode() {
  int32_t in = _amp32;
  EA(avg, in, (1 << 8));
  realstate = (in > (avg * 1 / 2));
  if (realstate != realstatebefore) {
    laststarttime = millis();
  }
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

inline void dec2() {
  if (filteredstate != filteredstatebefore) {
    if (menumode == 0) {
      lcd.noCursor();
      lcd.setCursor(15, 1);
      lcd.print((filteredstate) ? 'R' : ' ');
      stepsize_showcursor();
    }

    if (filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration = millis() - startttimelow;
    } else {
      startttimelow = millis();
      highduration = millis() - starttimehigh;
      if (highduration < (hightimesavg << 1) || hightimesavg == 0)
        hightimesavg = (highduration + (hightimesavg << 1)) / 3;
      else if (highduration > (hightimesavg * 5))
        hightimesavg = highduration / 3;
    }

    if (filteredstate == LOW) {
      if (highduration > (hightimesavg + (hightimesavg >> 1)) &&
          highduration < (hightimesavg * 6)) {
        sym = (sym << 1) | 1;
        wpm = (wpm + (1200 / (highduration / 3) * 4 / 3)) >> 1;
      } else if (highduration > (hightimesavg >> 1) &&
                 highduration <= (hightimesavg + (hightimesavg >> 1))) {
        sym <<= 1;
      }
    }

    if (filteredstate == HIGH) {
      uint16_t lacktime = (wpm > 35)   ? 15
                          : (wpm > 30) ? 12
                          : (wpm > 25) ? 10
                                       : 10;
      uint16_t letter_space = (hightimesavg * lacktime) / 10;
      if (lowduration > letter_space && lowduration < (letter_space * 5))
        printsym();
      if (lowduration >= (letter_space * 5)) {
        printsym();
        printsym();
      }
    }

    if ((millis() - startttimelow) > (highduration * 6) && (sym > 1))
      printsym();

    filteredstatebefore = filteredstate;
  }
}
#endif

//=========================================================================
// SECCIÓN 33: RECEPCIÓN SDR
//=========================================================================

#define F_SAMP_PWM (78125 / 1)
#define F_SAMP_RX 62500
#define F_ADC_CONV (192307 / 2)
#undef R
#define R 4 // Rate change from 62500/2 kSPS to 7812.5SPS, providing 12dB gain

volatile uint8_t admux[3];
volatile int16_t ocomb, qh;
volatile uint8_t rx_state = 0;

#pragma GCC push_options
#pragma GCC optimize("Ofast")

#define NEW_RX 1

#ifdef NEW_RX
static uint8_t tc = 0;
void process(int16_t i_ac2, int16_t q_ac2) {
  static int16_t ac3;
#ifdef CAT_STREAMING
  if (cat_streaming) {
    uint8_t out = ac3 + 128;
    if (out == ';')
      out++;
    Serial.write(out);
  }
#endif
#ifdef AF_OUT
  static int16_t ozd1, ozd2;
  if (_init) {
    ac3 = 0;
    ozd1 = 0;
    ozd2 = 0;
    _init = 0;
  }
  int16_t od1 = ac3 - ozd1;
  ocomb = od1 - ozd2;
#endif
  if (tc++ == 0)
    interrupts();
#ifdef AF_OUT
  ozd2 = od1;
  ozd1 = ac3;
#endif
  {
    q_ac2 >>= att2;
    static int16_t v[14];
    int16_t qh = ((v[0] - q_ac2) + (v[2] - v[12]) * 4) / 64 +
                 ((v[4] - v[10]) + (v[6] - v[8])) / 8 +
                 ((v[4] - v[10]) * 5 - (v[6] - v[8])) / 128 + (v[6] - v[8]) / 2;
    v[0] = v[1];
    v[1] = v[2];
    v[2] = v[3];
    v[3] = v[4];
    v[4] = v[5];
    v[5] = v[6];
    v[6] = v[7];
    v[7] = v[8];
    v[8] = v[9];
    v[9] = v[10];
    v[10] = v[11];
    v[11] = v[12];
    v[12] = v[13];
    v[13] = q_ac2;
    i_ac2 >>= att2;
    static int16_t vi[7];
    int16_t id = vi[0];
    vi[0] = vi[1];
    vi[1] = vi[2];
    vi[2] = vi[3];
    vi[3] = vi[4];
    vi[4] = vi[5];
    vi[5] = vi[6];
    vi[6] = i_ac2;
    i = i_ac2;  // Current sample, synchronized with q (legacy parity fix)
    q = q_ac2;
    ac3 = slow_dsp(-id - qh);
  }
  tc--;
}

static int16_t i_s0za1, i_s0zb0, i_s0zb1, i_s1za1, i_s1zb0, i_s1zb1;
static int16_t q_s0za1, q_s0zb0, q_s0zb1, q_s1za1, q_s1zb0, q_s1zb1, q_ac2;
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

void sdr_rx_02() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_03;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}

void sdr_rx_04() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_05;
  i_s1zb1 = i_s1zb0;
  i_s1zb0 = (ac + (i_s0za1 + i_s0zb0) * 3 + i_s0zb1) >> M_SR;
  i_s0za1 = ac;
}

void sdr_rx_06() {
  int16_t ac = sdr_rx_common_i();
  func_ptr = sdr_rx_07;
  i_s0zb1 = i_s0zb0;
  i_s0zb0 = ac;
}

void sdr_rx_01() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_02;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}

void sdr_rx_03() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_04;
  q_s1zb1 = q_s1zb0;
  q_s1zb0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
}

void sdr_rx_05() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_06;
  q_s0zb1 = q_s0zb0;
  q_s0zb0 = ac;
}

void sdr_rx_07() {
  int16_t ac = sdr_rx_common_q();
  func_ptr = sdr_rx_00;
  int16_t q_s1za0 = (ac + (q_s0za1 + q_s0zb0) * 3 + q_s0zb1) >> M_SR;
  q_s0za1 = ac;
  q_ac2 = (q_s1za0 + (q_s1za1 + q_s1zb0) * 3 + q_s1zb1);
  q_s1za1 = q_s1za0;
}

static int16_t ozi1, ozi2;

inline int16_t sdr_rx_common_q() {
  ADMUX = admux[0];
  ADCSRA |= (1 << ADSC);
  return ADC - 511;
}

inline int16_t sdr_rx_common_i() {
  ADMUX = admux[1];
  ADCSRA |= (1 << ADSC);
  int16_t adc = ADC - 511;
  // 4-sample averaging for ~3dB noise floor reduction
  static int16_t adc_buf[4] = {0, 0, 0, 0};
  static uint8_t adc_idx = 0;
  adc_buf[adc_idx] = adc;
  adc_idx = (adc_idx + 1) & 3; // Circular buffer (modulo 4)
  int16_t ac = (adc_buf[0] + adc_buf[1] + adc_buf[2] + adc_buf[3]) >> 2;
#ifdef AF_OUT
  if (_init) {
    ocomb = 0;
    ozi1 = 0;
    ozi2 = 0;
  }
  ozi2 = ozi1 + ozi2;
  ozi1 = ocomb + ozi1;
  OCR1AL = min(max((ozi2 >> 5) + 128, 0), 255);
#endif
  return ac;
}
#endif

#pragma GCC pop_options

//=========================================================================
// SECCIÓN 34: CONTROL DE TIMERS Y ADC
//=========================================================================

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs) {
  DIDR0 |= (1 << adcpin);
  ADCSRA = 0;
  ADCSRB = 0;
  ADMUX = 0;
  ADMUX |= (adcpin & 0x0f);
  ADMUX |= ((ref1v1) ? (1 << REFS1) : 0) | (1 << REFS0);
  ADCSRA |= ((uint8_t)log2((uint8_t)(F_CPU / 13 / fs))) & 0x07;
  ADCSRA |= (1 << ADEN);
}

void adc_stop() {
  ADCSRA &= ~(1 << ADIE);
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
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

void timer2_stop() {
  TIMSK2 &= ~(1 << OCIE2A);
  delay(1);
}

ISR(TIMER2_COMPA_vect) { func_ptr(); }

//=========================================================================
// SECCIÓN 35: UTILIDADES DE LECTURA ANALÓGICA
//=========================================================================

#ifndef VSS_METER
int analogSafeRead(uint8_t pin, bool ref1v1 = false) {
  noInterrupts();
  for (; !(ADCSRA & (1 << ADIF));)
    ;
  uint8_t adcsra = ADCSRA;
  uint8_t admux = ADMUX;
  ADCSRA &= ~(1 << ADIE);
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  if (ref1v1)
    ADMUX &= ~(1 << REFS0);
  else
    ADMUX = (1 << REFS0);
  delay(1);
  int val = analogRead(pin);
  ADCSRA = adcsra;
  ADMUX = admux;
  interrupts();
  return val;
}
#else
uint16_t analogSafeRead(uint8_t adcpin, bool ref1v1 = false) {
  noInterrupts();
  uint8_t oldmux = ADMUX;
  ADMUX = (3 & 0x0f) | ((ref1v1) ? (1 << REFS1) : 0) | (1 << REFS0);
  for (; !(ADCSRA & (1 << ADIF));)
    ;
  delayMicroseconds(16);
  ADCSRA |= (1 << ADSC);
  for (; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = oldmux;
  uint16_t adc = ADC;
  interrupts();
  return adc;
}
#endif

//=========================================================================
// SECCIÓN 35B: MUESTREO DE MICRÓFONO PARA VOX
//=========================================================================

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

//=========================================================================
// SECCIÓN 36: S-METER
//=========================================================================

void stepsize_showcursor();

uint8_t smode = 2;  // Default S-meter showing "S" units
uint32_t max_absavg256 = 0;
int16_t dbm;
static int16_t smeter_cnt = 0;

int16_t smeter(int16_t ref = 0) {
  max_absavg256 = max(_absavg256, max_absavg256); // peak

  if ((smode) && ((++smeter_cnt % 2048) == 0)) {   // slowed down display slightly
    float rms = (float)max_absavg256 * (float)(1 << att2);
    if (dsp_cap == SDR)
      rms /= (256.0 * 1024.0 * (float)R * 8.0 * 500.0 * 1.414 / (0.707 * 1.1));
    else
      rms /= (256.0 * 1024.0 * (float)R * 2.0 * 100.0 * 120.0 / (1.750 * 5.0));

    dbm = 10 * log10((rms * rms) / 50) + 30 - ref;

    lcd.noCursor();
    if (smode == 1) { // dBm meter
      lcd.setCursor(9, 0);
      lcd.print((int16_t)dbm);
      lcd.print(F("dBm "));
    }
    if (smode == 2) { // S-meter
      uint8_t s =
          (dbm < -63) ? ((dbm - -127) / 6) : (((uint8_t)(dbm - -73)) / 10) * 10;
      lcd.setCursor(14, 0);
      if (s < 10) {
        lcd.print('S');
        lcd.print(s);
      } else
        lcd.print(s);
    }
    if (smode == 3) { // S-bar
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
    if (smode == 4) { // wpm-indicator
      lcd.setCursor(14, 0);
      if (mode == CW)
        lcd.print(wpm);
      lcd.print("  ");
    }
#endif
#ifdef VSS_METER
    if (smode == 5) { // Supply-voltage indicator
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
    if (smode == 6) { // clock-indicator
      uint32_t _s = (millis() * 16000000ULL / F_MCU) / 1000;
      uint8_t h = (_s / 3600) % 24;
      uint8_t m = (_s / 60) % 60;
      uint8_t s = _s % 60;
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
    max_absavg256 /= 2;  // Implement peak hold/decay for all meter types
  }
  return dbm;
}

//=========================================================================
// SECCIÓN 37: RECEPCIÓN
//=========================================================================

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
  else {
    _init = 1;
  }

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

void dummy() {}

//=========================================================================
// SECCIÓN 38: FRECUENCIAS POR BANDA
//=========================================================================

#define N_BANDS 11

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

#if PER_BAND_TRACKING
static int32_t band_freq[N_BANDS];
static uint8_t band_mode[N_BANDS];
static uint8_t band_filt[N_BANDS];
#endif

//=========================================================================
// SECCIÓN 39: PASOS DE FRECUENCIA
//=========================================================================

uint32_t stepsizes[10] = {10000000, 1000000, 500000, 100000, 10000, 1000, 500, 100, 10, 1};
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
    set_lpf(freq / 1000000); // Update LPF filter based on new frequency (MHz)
  }
  change = true;
}

void stepsize_showcursor() {
  lcd.setCursor(stepsize + 1, 1);
  lcd.cursor();
}

void stepsize_hidecursor() { lcd.noCursor(); }

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

//=========================================================================
// SECCIÓN 40: UTILIDADES DE PANTALLA
//=========================================================================

void inline lcd_blanks() { lcd.print(F("        ")); }

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
  lcd.print(F("uSDX"));
#endif
  lcd.print('\x01');
  lcd_blanks();
  lcd_blanks();
}

const char *vfosel_label[] = {"A", "B"};
const char *mode_label[5] = {"LSB", "USB", "CW ", "FM ", "AM "};

inline void display_vfo(int32_t f) {
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

//=========================================================================
// SECCIÓN 40B: MENU SYSTEM - EEPROM ACTIONS
//=========================================================================

#define UPDATE 0
#define UPDATE_MENU 1
#define SAVE 2
#define LOAD 3
#define SKIP 4
#define NEXT_MENU 5
#define NEXT_CH 6

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

void printmenuid(uint8_t menuid) {
  uint8_t msb = menuid >> 4;
  uint8_t lsb = menuid & 0x0F;

  if (msb > 1) {
    lcd.print('1');
    msb -= 10;
  }
  lcd.print(char('0' + msb));
  lcd.print('.');

  if (lsb > 9) {
    lcd.print('1');
    lsb -= 10;
  }
  lcd.print(char('0' + lsb));
  lcd.print(' ');
}

//=========================================================================
// SECCIÓN 40C: MENU SYSTEM - PARAMETER ACTIONS
//=========================================================================

#define _N(a) sizeof(a) / sizeof(a[0])

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

static uint8_t pwm_min =
    0; // PWM value for which PA reaches its minimum: 29 when C31 installed;   0
       // when C31 removed;   0 for biasing BS170 directly
#ifdef QCX
static uint8_t pwm_max = 255; // PWM value for which PA reaches its maximum: 96
                              // when C31 installed; 255 when C31 removed;
#else
static uint8_t pwm_max = 128; // PWM value for which PA reaches its maximum:
                              // 128 for biasing BS170 directly
#endif

#define N_BANDS 11
uint8_t prev_bandval = 3;
uint8_t bandval = 3;


const char *offon_label[2] = {"OFF", "ON"};
#if (F_MCU > 16000000)
const char *filt_label[N_FILT + 1] = {"Full", "3000", "2400", "1800",
                                      "500",  "200",  "100",  "50"};
#else
const char *filt_label[N_FILT + 1] = {"Full", "2400", "2000", "1500",
                                      "500",  "200",  "100",  "50"};
#endif

const char *band_label[N_BANDS] = {
    "x",   "80m", "60m", "40m", "30m", "20m",
    "17m", "15m", "12m", "10m", "x"}; // G8RDI mod - squeezing out every free
                                      // byte!
// const char* band_label[N_BANDS] = { "160m", "80m", "60m", "40m", "30m",
// "20m", "17m", "15m", "12m", "10m", "6m" };
const char *stepsize_label[] = {
    "10M", "1M",  ".5M", "100k", "10k",
    "1k",  ".5k", "100", "10",   "1"}; // GW8RDI 0 b4 0. removed to save memory
const char *att_label[] = {"0dB",   "-13dB", "-20dB", "-33dB",
                           "-40dB", "-53dB", "-60dB", "-73dB"};
#ifdef CLOCK
const char *smode_label[] = {"OFF", "dBm", "S", "Sbar", "wpm", "Vss", "time"};
#else
#ifdef VSS_METER
const char *smode_label[] = {"OFF", "dBm", "S", "Sbar", "wpm", "Vss"};
#else
const char *smode_label[] = {"OFF", "dBm", "S", "Sbar", "wpm"};
#endif
#endif
#ifdef SWR_METER
/// const char* swr_label[] = { "OFF", "FWD-SWR", "FWD-REF", "VFWD-VREF" };
const char *swr_label[] = {"OFF", "FwdSWR", "FwdRef",
                           "VFwdVREF"}; // GW8RDI mod - byte saving
#endif
const char *cw_tone_label[] = {"700", "600"};
#ifdef KEYER
const char *keyer_mode_label[] = {
    "IambicA", "IambicB",
    "Straight"}; // GW8RDI mod - byte saving was "Iambic A"
#endif
const char *agc_label[] = {"OFF", "Fast", "Slow"};

// #define _N(a) sizeof(a)/sizeof(a[0]) // Already defined

#define N_PARAMS 44 // number of (visible) parameters

#define I_PARAMS 5
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
#define N_ALL_PARAMS (N_PARAMS + I_PARAMS) // number of parameters

int8_t paramAction(uint8_t action, uint8_t id = ALL) // list of parameters
{
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
#endif // FAST_AGC
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
    paramAction(action, cw_offset, 0x23, F("CW Offset"), NULL, 300, 2000, false);
    break;
#endif
#ifdef SEMI_QSK
  case SEMIQSK:
    paramAction(action, semi_qsk,  0x24, F("Semi QSK"), offon_label, 0, 1,
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
    paramAction(action, keyer_mode,  0x26, F("Keyer Mode"), keyer_mode_label, 0,
                2, false);
    break;
  case KEY_PIN:
    paramAction(action, keyer_swap,  0x27, F("Keyer Swap"), offon_label, 0, 1,
                false);
    break;
#endif
  case KEY_TX:
    paramAction(action, practice,    0x28, F("Practice"), offon_label, 0, 1,
                false);
    break;
#ifdef VOX_ENABLE
  case VOX:
    paramAction(action, vox,        0x31, F("VOX"), offon_label, 0, 1, false);
    break;
  case VOXGAIN:
    paramAction(action, vox_thresh, 0x32, F("Noise Gate"), NULL, 0, 255, false);
    break;
#endif
  case DRIVE:
    paramAction(action, drive,   0x33, F("TX Drive"), NULL, 0, 8, false);
    break;
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
    paramAction(action, param_a, 0x93, F("ParamA"), NULL, 0, UINT16_MAX, false);
    break;
  case PARAM_B:
    paramAction(action, param_b, 0x94, F("ParamB"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
  case PARAM_C:
    paramAction(action, param_c, 0x95, F("ParamC"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
#endif
  case BACKL:
    paramAction(action, backlight, 0xA1, F("Backlight"), offon_label, 0, 1, false);
    break;
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
    /* #ifdef KEEP_BAND_DATA
            case BAND_DATA0:      // G8RDI mod - added
                    paramAction(action, freq_last[0], 0, NULL, NULL, 0, 0,
    false); paramAction(action, mode_last[0], 0, NULL, NULL, 0, 0, false);
    break; case BAND_DATA1:      // G8RDI mod - added paramAction(action,
    freq_last[1], 0, NULL, NULL, 0, 0, false); paramAction(action, mode_last[1],
    0, NULL, NULL, 0, 0, false); break; case BAND_DATA2:      // G8RDI mod -
    added paramAction(action, freq_last[2], 0, NULL, NULL, 0, 0, false);
                    paramAction(action, mode_last[2], 0, NULL, NULL, 0, 0,
    false); break; case BAND_DATA3:      // G8RDI mod - added
                    paramAction(action, freq_last[3], 0, NULL, NULL, 0, 0,
    false); paramAction(action, mode_last[3], 0, NULL, NULL, 0, 0, false);
    break; case BAND_DATA4:      // G8RDI mod - added paramAction(action,
    freq_last[4], 0, NULL, NULL, 0, 0, false); paramAction(action, mode_last[4],
    0, NULL, NULL, 0, 0, false); break; case BAND_DATA5:      // G8RDI mod -
    added paramAction(action, freq_last[5], 0, NULL, NULL, 0, 0, false);
                    paramAction(action, mode_last[5], 0, NULL, NULL, 0, 0,
    false); break; case BAND_DATA6:      // G8RDI mod - added
                    paramAction(action, freq_last[6], 0, NULL, NULL, 0, 0,
    false); paramAction(action, mode_last[6], 0, NULL, NULL, 0, 0, false);
    break; case BAND_DATA7:      // G8RDI mod - added paramAction(action,
    freq_last[7], 0, NULL, NULL, 0, 0, false); paramAction(action, mode_last[7],
    0, NULL, NULL, 0, 0, false); break; case BAND_DATA8:      // G8RDI mod -
    added paramAction(action, freq_last[8], 0, NULL, NULL, 0, 0, false);
                    paramAction(action, mode_last[8], 0, NULL, NULL, 0, 0,
    false); break; #endif */
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
    break; // keep iterating util menu item found
  }
  return id;
}

//=========================================================================
// SECCIÓN 41: EEPROM
//=========================================================================

//=========================================================================
// SECCIÓN 41B: INICIALIZACIÓN DE PINES
//=========================================================================

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

#ifdef CAL_IQ
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

//=========================================================================
// SECCIÓN 41C: CW MESSAGES
//=========================================================================

#ifdef CW_MESSAGE
const char cw_msg[1][48] PROGMEM = {CW_MSG1};
uint8_t cw_msg_interval = 5;
uint32_t cw_msg_event = 0;
uint8_t cw_msg_id = 0;
#endif

//=========================================================================
// SECCIÓN 41D: CAT INTERFACE
//=========================================================================

#ifdef CAT
#define CATCMD_SIZE 16
char CATcmd[CATCMD_SIZE];

void Command_GETFreqA() {
#ifdef _SERIAL
  if (!cat_active)
    return;
#endif
  char Catbuffer[32];
  uint32_t tf = freq;
  unsigned int g = tf / 1000000000lu;
  tf -= g * 1000000000lu;
  unsigned int m = tf / 1000000lu;
  tf -= m * 1000000lu;
  unsigned int k = tf / 1000lu;
  tf -= k * 1000lu;
  unsigned int h = tf;
  sprintf(Catbuffer, "FA%02u%03u", g, m);
  Serial.print(Catbuffer);
  sprintf(Catbuffer, "%03u%03u;", k, h);
  Serial.print(Catbuffer);
}

void Command_SETFreqA() {
  char Catbuffer[16];
  strncpy(Catbuffer, CATcmd + 2, 11);
  Catbuffer[11] = '\0';
  freq = (uint32_t)atol(Catbuffer);
  set_lpf(freq / 1000000); // Update LPF filter based on new frequency (MHz)
  change = true;
}

void Command_IF() {
#ifdef _SERIAL
  if (!cat_active)
    return;
#endif
  char Catbuffer[32];
  uint32_t tf = freq;
  unsigned int g = tf / 1000000000lu;
  tf -= g * 1000000000lu;
  unsigned int m = tf / 1000000lu;
  tf -= m * 1000000lu;
  unsigned int k = tf / 1000lu;
  tf -= k * 1000lu;
  unsigned int h = tf;
  sprintf(Catbuffer, "IF%02u%03u%03u%03u", g, m, k, h);
  Serial.print(Catbuffer);
  Serial.print("00000+000000");
  Serial.print("0000");
  Serial.print(mode + 1);
  Serial.print("0000000;");
}

void Command_AI() { Serial.print("AI0;"); }
void Command_AG0() { Serial.print("AG0;"); }
void Command_XT1() { Serial.print("XT1;"); }
void Command_RT1() { Serial.print("RT1;"); }
void Command_RC() {
  rit = 0;
  Serial.print("RC;");
}
void Command_FL0() { Serial.print("FL0;"); }

void Command_GetMD() {
  Serial.print("MD");
  Serial.print(mode + 1);
  Serial.print(';');
}

void Command_SetMD() {
  mode = CATcmd[2] - '1';
  vfomode[vfosel % 2] = mode;
  change = true;
  si5351.iqmsa = 0;
}

void Command_AI0() { Serial.print("AI0;"); }

void Command_RX() {
#ifdef TX_ENABLE
  switch_rxtx(0);
  semi_qsk_timeout = 0;
#endif
  Serial.print("RX0;");
}

void Command_TX0() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_TX1() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_TX2() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_RS() { Serial.print("RS0;"); }

void Command_VX(char mode) {
  char Catbuffer[16];
  sprintf(Catbuffer, "VX%c;", mode);
  Serial.print(Catbuffer);
}

void Command_ID() { Serial.print("ID020;"); }
void Command_PS() { Serial.print("PS1;"); }
void Command_PS1() {}

#ifdef CAT_EXT
void Command_UK(char k1, char k2) {
  cat_key = ((k1 - '0') << 4) | (k2 - '0');
  if (cat_key & 0x40) {
    encoder_val--;
    cat_key &= 0x3f;
  }
  if (cat_key & 0x80) {
    encoder_val++;
    cat_key &= 0x3f;
  }
  char Catbuffer[16];
  sprintf(Catbuffer, "UK%c%c;", k1, k2);
  Serial.print(Catbuffer);
}

void Command_UD() {
  char Catbuffer[40];
  sprintf(Catbuffer, "UD%02u%s;", (lcd.curs) ? lcd.y * 16 + lcd.x : 16 * 2 + 1,
          lcd.text);
  Serial.print(Catbuffer);
}

void Command_UA(char en) {
  char Catbuffer[16];
  sprintf(Catbuffer, "UA%01u;", (_cat_streaming = (en == '1')));
  Serial.print(Catbuffer);
  if (_cat_streaming) {
    Serial.print("US");
    cat_streaming = true;
  }
}
#endif

volatile uint8_t cat_ptr = 0;

void analyseCATcmd() {
  if ((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[2] == ';'))
    Command_GETFreqA();
  else if ((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[13] == ';'))
    Command_SETFreqA();
  else if ((CATcmd[0] == 'I') && (CATcmd[1] == 'F') && (CATcmd[2] == ';'))
    Command_IF();
  else if ((CATcmd[0] == 'I') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_ID();
  else if ((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_PS();
  else if ((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == '1'))
    Command_PS1();
  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == ';'))
    Command_AI();
  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == '0'))
    Command_AI0();
  else if ((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_GetMD();
  else if ((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[3] == ';'))
    Command_SetMD();
  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_RX();
  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_TX0();
  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '0'))
    Command_TX0();
  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '1'))
    Command_TX1();
  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '2'))
    Command_TX2();
  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'G') && (CATcmd[2] == '0'))
    Command_AG0();
  else if ((CATcmd[0] == 'X') && (CATcmd[1] == 'T') && (CATcmd[2] == '1'))
    Command_XT1();
  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'T') && (CATcmd[2] == '1'))
    Command_RT1();
  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'C') && (CATcmd[2] == ';'))
    Command_RC();
  else if ((CATcmd[0] == 'F') && (CATcmd[1] == 'L') && (CATcmd[2] == '0'))
    Command_FL0();
  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_RS();
  else if ((CATcmd[0] == 'V') && (CATcmd[1] == 'X') && (CATcmd[2] != ';'))
    Command_VX(CATcmd[2]);
#ifdef CAT_EXT
  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'K') && (CATcmd[4] == ';'))
    Command_UK(CATcmd[2], CATcmd[3]);
  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_UD();
#endif
#ifdef CAT_STREAMING
  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'A') && (CATcmd[3] == ';'))
    Command_UA(CATcmd[2]);
#endif
  else {
    Serial.print("?;");
  }
}

void serialEvent() {
  if (Serial.available()) {
    rxend_event = millis() + 10;
    char data = Serial.read();
    CATcmd[cat_ptr++] = data;
    if (data == ';') {
      CATcmd[cat_ptr] = '\0';
      cat_ptr = 0;
#ifdef _SERIAL
      if (!cat_active) {
        cat_active = 1;
        smode = 0;
      }
#endif
      analyseCATcmd();
      delay(10);
    } else if (cat_ptr > (CATCMD_SIZE - 1)) {
      Serial.print("E;");
      cat_ptr = 0;
    }
  }
}
#endif

//=========================================================================
// SECCIÓN 41E: INA219 POWER METER
//=========================================================================

#ifdef INA219_POWER_METER
#define INA219_ADDR 0x40
#define INA219_REG_CONFIG 0x00
#define INA219_REG_CURRENT 0x04
#define INA219_REG_BUSVOLTAGE 0x02
#define INA219_REG_POWER 0x03
#define INA219_REG_CALIBRATION 0x05
#define INA219_CONFIG_BVOLTAGERANGE_32V 0x0000
#define INA219_CONFIG_GAIN_8_320MV 0x1800
#define INA219_CONFIG_BADCRES_12BIT 0x0080
#define INA219_CONFIG_SADCRES_12BIT_1S_532US 0x0000
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

void ina219_write(uint8_t reg, uint16_t val) {
  i2c.start();
  i2c.SendByte(INA219_ADDR << 1);
  i2c.SendByte(reg);
  i2c.SendByte(val >> 8);
  i2c.SendByte(val & 0xff);
  i2c.stop();
}

uint16_t ina219_read(uint8_t reg) {
  uint16_t ret;
  i2c.start();
  i2c.SendByte(INA219_ADDR << 1);
  i2c.SendByte(reg);
  i2c.stop();
  i2c.start();
  i2c.SendByte((INA219_ADDR << 1) | 1);
  ret = i2c.RecvByte(false) << 8;
  ret |= i2c.RecvByte(true);
  i2c.stop();
  return ret;
}

void ina219_init() {
  ina219_write(INA219_REG_CALIBRATION, calshunt);
  ina219_write(INA219_REG_CONFIG, INA219_CONFIG_BVOLTAGERANGE_32V |
                                      INA219_CONFIG_GAIN_8_320MV |
                                      INA219_CONFIG_BADCRES_12BIT |
                                      INA219_CONFIG_SADCRES_12BIT_1S_532US |
                                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS);
}

#define SWR_AVERAGING_NUM 8

void readSWR() {
#ifdef INA219_POWER_METER
  float busvoltage = 0;
  float current_mA;
  float power_mW;
  ina219_init();
#endif

  float v_FWD = 0;
  float v_REF = 0;
  for (int i = 0; i < SWR_AVERAGING_NUM; i++) {
    v_FWD = v_FWD + (ref_V / 1023) * (int)analogRead(PIN_FWD);
    v_REF = v_REF + (ref_V / 1023) * (int)analogRead(PIN_REF);
    delay(5);
  }
  v_FWD = v_FWD / SWR_AVERAGING_NUM;
  v_REF = v_REF / SWR_AVERAGING_NUM;

  float p_FWD = sq(v_FWD);
  float p_REV = sq(v_REF);

  float vRatio = v_REF / v_FWD;
  float VSWR = (1 + vRatio) / (1 - vRatio);

  if ((VSWR > 9.99) || (VSWR < 1))
    VSWR = 9.99;

#ifdef INA219_POWER_METER
  busvoltage = (ina219_read(INA219_REG_BUSVOLTAGE) >> 3) * 0.004;
  current_mA = (int16_t)ina219_read(INA219_REG_CURRENT) / 10.0;
  power_mW = ina219_read(INA219_REG_POWER) * 2.0;
#endif

  if (swrmeter == 1) {
    lcd.setCursor(10, 0);
    lcd.print(VSWR);
    lcd.print("  ");
  } else if (swrmeter == 2) {
    lcd.setCursor(10, 0);
    lcd.print(p_FWD * calpwr);
    lcd.print("  ");
    lcd.setCursor(14, 0);
    lcd.print(p_REV * calpwr);
  } else if (swrmeter == 3) {
    lcd.setCursor(10, 0);
    lcd.print(v_FWD * 2);
    lcd.print(" ");
    lcd.setCursor(14, 0);
    lcd.print(v_REF * 2);
  }
#ifdef INA219_POWER_METER
  else if (swrmeter == 4) {
    lcd.setCursor(10, 0);
    lcd.print(p_FWD * calpwr);
    lcd.print(" ");
    lcd.setCursor(14, 0);
    if (busvoltage > 0)
      lcd.print((power_mW / 1000) / busvoltage * 100, 0);
    else
      lcd.print(0);
    lcd.print("%");
  } else if (swrmeter == 5) {
    lcd.setCursor(10, 0);
    lcd.print(current_mA);
    lcd.print(" ");
    lcd.setCursor(14, 0);
    lcd.print(busvoltage);
    lcd.print(" ");
    lcd.setCursor(10, 1);
    lcd.print((power_mW / 1000), 1);
  }
#endif
}
#endif

//=========================================================================
// SECCIÓN 42: GENERACIÓN DE TABLA LUT
//=========================================================================

void generate_lut() {
  for (int i = 0; i < 256; i++) {
    if (i == 0)
      lut[i] = 0;
    else {
      uint8_t val = (uint8_t)(i * 0.5);
      if (val < 128)
        val = 127 - 127 * cos(PI * val / 127) / 2;
      else
        val = 127 + 127 * cos(PI * (val - 127) / 127) / 2;
      lut[i] = val;
    }
  }
  for (int i = 0; i < 256; i++) {
    if (lut[i] > 255)
      lut[i] = 255;
    if (lut[i] < 0)
      lut[i] = 0;
  }
  for (int i = 0; i < 256; i++) {
    uint8_t out = (uint8_t)(pow((float)i / 255.0, 0.5) * 255.0);
    lut[i] = (out * 3 + lut[i]) / 4;
  }
}

void build_lut() { generate_lut(); }

// Missing variables for menu system
#ifdef CAL_IQ
uint8_t cal_iq_dummy = 0;
#endif
#ifdef DEBUG
int32_t sr = 0;
int16_t cpu_load = 0;
uint16_t param_a = 0;
int16_t param_b = 0;
int16_t param_c = 0;
#endif

//=========================================================================
// SECCIÓN 44: SETUP
//=========================================================================

void powerDown() { // Reduces power from 110mA to 70mA (back-light on) or 30mA
                   // (back-light off), remaining current is probably opamp
                   // quiescent current
  lcd.setCursor(0, 1);
  lcd.print(F("Power-off 73 :-)"));
  lcd_blanks();

  MCUSR = ~(1 << WDRF); // MSY be done before wdt_disable()
  wdt_disable(); // WDTON Fuse High bit need to be 1 (0xD1), if NOT it will
                 // override and set WDE=1; WDIE=0, meaning MCU will reset when
                 // watchdog timer is zero, and this seems to happen when
                 // wdt_disable() is called

  timer2_stop();
  timer1_stop();
  adc_stop();

  si5351.powerDown();

  delay(1500);

  // Disable external interrupts INT0, INT1, Pin Change
  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;
  // Disable internal interrupts
  TIMSK0 = 0;
  TIMSK1 = 0;
  TIMSK2 = 0;
  WDTCSR = 0;
  // Enable BUTTON Pin Change interrupt
  *digitalPinToPCMSK(BUTTONS) |= (1 << digitalPinToPCMSKbit(BUTTONS));
  *digitalPinToPCICR(BUTTONS) |= (1 << digitalPinToPCICRbit(BUTTONS));

  // Power-down sub-systems
  PRR = 0xff;

  lcd.noDisplay();
  PORTD &= ~0x08; // disable backlight

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  interrupts();
  sleep_bod_disable();
  // MCUCR |= (1<<BODS) | (1<<BODSE);  // turn bod off by settings BODS, BODSE;
  // note BODS is reset after three clock-cycles, so quickly go to sleep before
  // it is too late MCUCR &= ~(1<<BODSE);  // must be done right before sleep
  sleep_cpu(); // go to sleep mode, wake-up by either INT0, INT1, Pin Change,
               // TWI Addr Match, WDT, BOD
  sleep_disable();

  // void(* reset)(void) = 0; reset();   // soft reset by calling reset vector
  // (does not reset registers to defaults)
  do {
    wdt_enable(WDTO_15MS);
    for (;;)
      ;
  } while (0); // soft reset by trigger watchdog timeout
}

void setup() {
  digitalWrite(KEY_OUT, LOW);

  si5351.powerDown();

  MCUSR = 0;
  wdt_enable(WDTO_4S);

  ADMUX = (1 << REFS0);

  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  encoder_setup();

  initPins();

  delay(100);
  lcd.begin(16, 4);
  lcd.noCursor();

  uint8_t logo[] = {0x08, 0x04, 0x0a, 0x05, 0x0a, 0x04, 0x08, 0x00};
  lcd.createChar(0x01, logo);
  uint8_t vfo_a[] = {0x0c, 0x12, 0x1e, 0x12, 0x12, 0x00, 0x00, 0x00};
  lcd.createChar(0x06, vfo_a);
  uint8_t vfo_b[] = {0x1c, 0x12, 0x1c, 0x12, 0x1c, 0x00, 0x00, 0x00};
  lcd.createChar(0x07, vfo_b);

  generate_lut();
  init_vox_thresh();

  i2c.begin();

  si5351.powerDown();
  delay(50);
  si5351.freq(10000000, 0, 90);
  si5351.SendRegister(SI_CLK_OE, TX0RX0);

  show_banner();
  lcd.setCursor(7, 0);
  lcd.print(F(" R"));
  lcd.print(F(VERSION));
  lcd_blanks();

  paramAction(LOAD, VERS);

  if ((eeprom_version != get_version_id()) || (_digitalRead(BUTTONS))) {
    eeprom_version = get_version_id();
    paramAction(SAVE);
    lcd.setCursor(0, 1);
    lcd.print(F("Reset settings.."));
    lcd_blanks();
    delay(500);
    wdt_reset();
  } else {
    paramAction(LOAD);
  }

  si5351.iqmsa = 0;
  change = true;
  prev_bandval = bandval;
  vox = false;
  nr = 0;
  rit = false;
  freq = vfo[vfosel];
  mode = vfomode[vfosel];
  set_lpf(freq /
          1000000); // Initialize LPF filter based on startup frequency (MHz)

#ifdef TX_ENABLE
  build_lut();
#endif

  show_banner();

  start_rx();

  // Initialize audio output PWM
  timer1_start(F_SAMP_PWM);
  TCCR1A |= (1 << COM1A1);  // Enable audio output on OC1A (SIDETONE) - PWM mode
  TCCR1A &= ~(1 << COM1B1); // Disable KEY_OUT PWM during RX
  OCR1AL = 0x80;            // Set to midpoint (2.5V) for silence

#ifdef KEYER
  keyerState = IDLE;
  keyerControl = IAMBICB;
  loadWPM(keyer_speed);
#endif

  for (; !_digitalRead(DIT) ||
         ((mode == CW && keyer_mode != SINGLE) && (!_digitalRead(DAH)));) {
    // wait until DIT/DAH is released
  }

  change = true;
}

//=========================================================================
// SECCIÓN 44: LOOP PRINCIPAL
//=========================================================================

void loop() {
  static int8_t prev_menumode = 0;
  static uint8_t event = 0;

  enum event_t {
    BL = 0x10,
    BR = 0x20,
    BE = 0x30,
    SC = 0x01,
    DC = 0x02,
    PL = 0x04,
    PLC = 0x05,
    PT = 0x0C
  };

  if (change) {
    if (mode == CW) {
      si5351.freq(freq + cw_offset, rx_ph_q, 0);
    } else if (mode == LSB) {
      si5351.freq(freq, rx_ph_q, 0);
    } else {
      si5351.freq(freq, 0, rx_ph_q);
    }
#ifdef RIT_ENABLE
    if (rit) {
      si5351.freq_calc_fast(rit);
      si5351.SendPLLRegisterBulk();
    }
#endif

    change = false;
    if (menumode == 0) {
      display_vfo(freq);
    }
  }

  if (menumode == 0) {
    smeter();

    int8_t steps = encoder_val;
    if (steps)
      encoder_last_delta = steps;
    encoder_val = 0;

    if (steps) {
      process_encoder_tuning_step(steps);
      change = true;
    }

#ifdef THREEBUTTONROT
    CheckRotButton();
#endif

    // wdt_reset(); // Moved to end of loop

#ifdef TX_ENABLE
    uint8_t pin = ((mode == CW) && (keyer_swap)) ? DAH : DIT;
    if (!vox_tx) {
      if (!_digitalRead(pin)) {
        switch_rxtx(1);
        do {
          wdt_reset();
          delay((mode == CW) ? 10 : 100);
          if (inv ^ _digitalRead(BUTTONS))
            break;
        } while (!_digitalRead(pin));
        switch_rxtx(0);
      }
    }
#endif

#ifdef SEMI_QSK
    if ((semi_qsk_timeout) && (millis() > semi_qsk_timeout)) {
      switch_rxtx(0);
    }
#endif
  }

  if (inv ^
      _digitalRead(
          BUTTONS)) { // Left-/Right-/Rotary-button (while not already pressed)
    if (!((event & PL) ||
          (event &
           PLC))) { // hack: if there was long-push before, then fast forward
      uint16_t v = analogSafeRead(BUTTONS);
#ifdef CAT_EXT
      if (cat_key) {
        v = (cat_key & 0x04)   ? 512
            : (cat_key & 0x01) ? 870
            : (cat_key & 0x02) ? 1024
                               : 0;
      } // override analog value exercised by BUTTONS press
#endif // CAT_EXT
      event = SC;
      int32_t t0 = millis();
      for (; inv ^ _digitalRead(BUTTONS);) { // until released or long-press
        if ((millis() - t0) > 300) {
          event = PL;
          break;
        }
        wdt_reset();
      }
      delay(10); // debounce
      for (; (event != PL) &&
             ((millis() - t0) < 500);) { // until 2nd press or timeout
        if (inv ^ _digitalRead(BUTTONS)) {
          event = DC;
          break;
        }
        wdt_reset();
      }
      for (; inv ^ _digitalRead(BUTTONS);) { // until released, or encoder is
                                             // turned while longpress
        if (encoder_val && event == PL) {
          event = PT;
          break;
        }
#ifdef ONEBUTTON
        if (event == PL)
          break; // do not lock on longpress, so that L and R buttons can be
                 // used for tuning
#endif
        wdt_reset();
      } // Max. voltages at ADC3 for buttons L,R,E: 3.76V;4.55V;5V, thresholds
        // are in center
      event |=
          (v < (uint16_t)(4.2 * 1024.0 / 5.0)) ? BL
          : (v < (uint16_t)(4.8 * 1024.0 / 5.0))
              ? BR
              : BE; // determine which button pressed based on threshold levels
    } else {        // hack: fast forward handling
      event =
          (event & 0xf0) |
          ((encoder_val) ? PT : PLC /*PL*/); // only alternate between
                                             // push-long/turn when applicable
    }
    switch (event) {
#ifndef ONEBUTTON
    case BL | PL:  // Called when menu button pressed
    case BL | PLC: // or kept pressed
      menumode = 2;
      break;
    case BL | PT:
      menumode = 1;
      // if(menu == 0) menu = 1;
      break;
    case BL | SC:
#ifdef CW_MESSAGE
      if ((menumode == 1) && (menu >= CWMSG1) && (menu <= CWMSG6)) {
        cw_msg_event = millis();
        cw_msg_id = menu - CWMSG1;
        menumode = 0;
        break;
      }
#endif // CW_MESSAGE
      int8_t _menumode;
      if (menumode == 0) {
        _menumode = 1;
        if (menu == 0)
          menu = 1;
      } // short left-click while in default screen: enter menu mode
      if (menumode == 1) {
        _menumode = 2;
      } // short left-click while in menu: enter value selection screen
      if (menumode >= 2) {
        _menumode = 0;
        change = true;
        paramAction(SAVE, menu);
      } // short left-click while in value selection screen: save, and return to
        // default screen
      menumode = _menumode;
      break;
    case BL | DC:
      break;
    case BR | SC:
      if (!menumode) {
        int8_t prev_mode = mode;
        if (rit) {
          rit = 0;
          stepsize = prev_stepsize[mode == CW];
          change = true;
          break;
        }
        mode += 1;
        // encoder_val = 1;
        // paramAction(UPDATE, MODE); // Mode param //paramAction(UPDATE, mode,
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true); #define
        // MODE_CHANGE_RESETS  1
#ifdef MODE_CHANGE_RESETS
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500; // sets suitable stepsize
#endif
        if (mode > CW)
          mode = LSB; // skip all other modes (only LSB, USB, CW)
#ifdef MODE_CHANGE_RESETS
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0; // resets filter (to most BW) and NR on mode change
#else
        if (mode == CW) {
          nr = 0;
        }
        prev_stepsize[prev_mode == CW] = stepsize;
        stepsize =
            prev_stepsize[mode ==
                          CW]; // backup stepsize setting for previous mode,
                               // restore previous stepsize setting for current
                               // selected mode; filter settings captured for
                               // either CQ or other modes.
        prev_filt[prev_mode == CW] = filt;
        filt =
            prev_filt[mode == CW]; // backup filter setting for previous mode,
                                   // restore previous filter setting for
                                   // current selected mode; filter settings
                                   // captured for either CQ or other modes.
#endif
        // paramAction(UPDATE, MODE);
        vfomode[vfosel % 2] = mode;
        paramAction(SAVE, (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
        paramAction(SAVE, MODE);
        paramAction(SAVE, FILTER);
#if PER_BAND_TRACKING
        // uSDXOpen feature: Save mode and filter for current band
        band_mode[bandval] = mode;
        band_filt[bandval] = filt;
#endif
        si5351.iqmsa = 0; // enforce PLL reset
#ifdef CW_DECODER
        if ((prev_mode == CW) && (cwdec))
          show_banner();
#endif
        change = true;
      } else {
        if (menumode == 1) {
          menumode = 0;
        }
        if (menumode >= 2) {
          menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        }
      }
      break;
    case BR | DC:
      filt++;
      _init = true;
      if (mode == CW && filt > N_FILT)
        filt = 4;
      if (mode == CW && filt == 4)
        stepsize = STEP_500; // reset stepsize for 500Hz filter
      if (mode == CW && (filt == 5 || filt == 6) && stepsize < STEP_100)
        stepsize = STEP_100; // for CW BW 200, 100      -> step = 100 Hz
      if (mode == CW && filt == 7 && stepsize < STEP_10)
        stepsize = STEP_10; // for CW BW 50 -> step = 10 Hz
      if (mode != CW && filt > 3)
        filt = 0;
      encoder_val = 0;
      paramAction(UPDATE, FILTER);
      paramAction(SAVE, FILTER);
      wdt_reset();
      delay(1500);
      wdt_reset();
      change = true; // refresh display
      break;
    case BR | PL:
#ifdef SIMPLE_RX
      // Experiment: ISR-less sdr_rx():
      smode = 0;
      TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
      delay(100);
      lcd.setCursor(15, 1);
      lcd.print('X');
      static uint8_t x = 0;
      uint32_t next = 0;
      for (;;) {
        func_ptr();
#ifdef DEBUG
        numSamples++;
#endif
        if (!rx_state) {
          x++;
          if (x > 16) {
            loop();
            // lcd.setCursor(9, 0); lcd.print((int16_t)100); lcd.print(F("dBm
            // "));  // delays are taking too long!
            x = 0;
          }
        }
        // for(;micros() < next;);  next = micros() + 16;   // sync every
        // 1000000/62500=16ms (or later if missed)
      } //
#endif // SIMPLE_RX
#ifdef RIT_ENABLE
      rit = !rit;
      stepsize = (rit) ? STEP_10 : prev_stepsize[mode == CW];
      if (!rit) { // after RIT comes VFO A/B swap
#else
    {
#endif // RIT_ENABLE
        vfosel = !vfosel;
        freq = vfo[vfosel % 2]; // todo: share code with menumode
        mode = vfomode[vfosel % 2];
        set_lpf(freq / 1000000); // Update LPF filter when switching VFO
        // make more generic:
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500;
        // Preserve filt and nr settings - user controls these explicitly via menu
      }
      change = true;
      break;
// #define TUNING_DIAL  1
#ifdef TUNING_DIAL
    case BR | PLC: // while pressed long continues
    case BE | PLC:
      freq = freq + ((_step > 0) ? 1 : -1) * pow(2, abs(_step));
      change = true;
      break;
    case BR | PT:
      _step += encoder_val;
      encoder_val = 0;
      lcd.setCursor(0, 0);
      lcd.print(_step);
      lcd_blanks();
      break;
#endif // TUNING_DIAL
    case BE | SC:
      if (!menumode) {
        stepsize_change(+1);
      } else {
        int8_t _menumode;
        if (menumode == 1) {
          _menumode = 2;
        } // short encoder-click while in menu: enter value selection screen
        if (menumode == 2) {
          _menumode = 1;  // Return to menu selection screen first (smoother transition like legacy)
          change = true;
          paramAction(SAVE, menu);
        } // short encoder-click while in value selection screen: save, and
          // return to menu selection screen
#ifdef MENU_STR
        if (menumode == 3) {
          _menumode = 3;
          paramAction(NEXT_CH, menu);
        } // short encoder-click while in string edit mode: change position to
          // next character
#endif
        menumode = _menumode;
      }
      break;
    case BE | DC:
      // delay(100);
      //  uSDXOpen feature: Bidirectional band change based on encoder direction
      if (encoder_last_delta > 0) {
        // Encoder was turning up: increase band
        bandval++;
        if (bandval >= (N_BANDS - 1))
          bandval = 1; // excludes 6m, 160m
      } else {
        // Encoder was turning down: decrease band
        bandval--;
        if (bandval < 1)
          bandval = (N_BANDS - 2); // wrap to highest allowed band
      }
      // Preserve current stepsize - user adjusts with encoder button if needed
      change = true;
      encoder_last_delta = 0; // Reset after band change
      break;
    case BE | PL:
      stepsize_change(-1);
      break;
    case BE | PT:
      for (; _digitalRead(BUTTONS);) { // process encoder changes until released
        wdt_reset();
        if (encoder_val) {
          paramAction(UPDATE, VOLUME);
          if (volume < 0) {
            volume = 10;
            paramAction(SAVE, VOLUME);
            powerDown();
          } // powerDown when volume < 0
          paramAction(SAVE, VOLUME);
        }
      }
      change = true; // refresh display
      break;
#else // ONEBUTTON
    case BE | SC:
      int8_t _menumode;
      if (menumode == 0) {
        _menumode = 1;
        if (menu == 0)
          menu = 1;
      } // short enc-click while in default screen: enter menu mode
      if (menumode == 1) {
        _menumode = 2;
      } // short enc-click while in menu: enter value selection screen
      if (menumode == 2) {
        _menumode = 0;
        change = true;
        paramAction(SAVE, menu);
      } // short enc-click while in value selection screen: save, and return to
        // default screen
#ifdef MENU_STR
      if (menumode == 3) {
        _menumode = 3;
        paramAction(NEXT_CH, menu);
      } // short encoder-click while in string edit mode: change position to
        // next character
#endif
      menumode = _menumode;
      break;
    case BE | DC:
      if (!menumode) {
        int8_t prev_mode = mode;
        if (rit) {
          rit = 0;
          stepsize = prev_stepsize[mode == CW];
          change = true;
          break;
        }
        mode += 1;
        // encoder_val = 1;
        // paramAction(UPDATE, MODE); // Mode param //paramAction(UPDATE, mode,
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true); #define
        // MODE_CHANGE_RESETS  1
#ifdef MODE_CHANGE_RESETS
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500; // sets suitable stepsize
#endif
        if (mode > CW)
          mode = LSB; // skip all other modes (only LSB, USB, CW)
#ifdef MODE_CHANGE_RESETS
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0; // resets filter (to most BW) and NR on mode change
#else
        if (mode == CW) {
          nr = 0;
        }
        prev_stepsize[prev_mode == CW] = stepsize;
        stepsize =
            prev_stepsize[mode ==
                          CW]; // backup stepsize setting for previous mode,
                               // restore previous stepsize setting for current
                               // selected mode; filter settings captured for
                               // either CQ or other modes.
        prev_filt[prev_mode == CW] = filt;
        filt =
            prev_filt[mode == CW]; // backup filter setting for previous mode,
                                   // restore previous filter setting for
                                   // current selected mode; filter settings
                                   // captured for either CQ or other modes.
#endif
        // paramAction(UPDATE, MODE);
        vfomode[vfosel % 2] = mode;
        paramAction(SAVE, (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
        paramAction(SAVE, MODE);
        paramAction(SAVE, FILTER);
#if PER_BAND_TRACKING
        // uSDXOpen feature: Save mode and filter for current band
        band_mode[bandval] = mode;
        band_filt[bandval] = filt;
#endif
        si5351.iqmsa = 0; // enforce PLL reset
        if ((prev_mode == CW) && (cwdec))
          show_banner();
        change = true;
      } else {
        if (menumode == 1) {
          menumode = 0;
        } // short right-click while in menu: enter value selection screen
        if (menumode >= 2) {
          menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        } // short right-click while in value selection screen: save, and return
          // to menu screen
      }
      break;
    case BE | PL:
      stepsize_change(-1);
      break;
    case BE | PLC:
      menumode = 2;
      break;
    case BE | PT:
      menumode = 1;
      break;
#endif // ONEBUTTON
    }
  } else
    event = 0; // no button pressed: reset event

  if ((menumode) || (prev_menumode != menumode)) { // Show parameter and value
    int8_t encoder_change = encoder_val;
    // uSDXOpen feature: Track encoder direction for bidirectional band change
    if (encoder_change != 0)
      encoder_last_delta = encoder_change;

    if ((menumode == 1) && encoder_change) {
      menu += encoder_val; // Navigate through menu
#ifdef ONEBUTTON
      menu = max(0, min(menu, N_PARAMS));
#else
      // uSDXOpen feature: Menu cycling - wrap around at both ends
      if (menu > N_PARAMS)
        menu = 1; // Wrap to first menu item
      if (menu < 1)
        menu = N_PARAMS; // Wrap to last menu item
#endif
      menu = paramAction(NEXT_MENU,
                         menu); // auto probe next menu item (gaps may exist)
      encoder_val = 0;
    }
    if (encoder_change || (prev_menumode != menumode))
      paramAction(UPDATE_MENU, (menumode) ? menu : 0);
    prev_menumode = menumode;
    if (menumode == 2) {  // Match exact menumode 2 (parameter edit mode) like legacy
      if (encoder_change) {
        lcd.setCursor(0, 1);
        lcd.cursor();
        if (menu == MODE) {
          vfomode[vfosel % 2] = mode;
          paramAction(SAVE, (vfosel % 2) ? MODEB : MODEA);
          change = true;
          si5351.iqmsa = 0;
          if (mode != CW)
            stepsize = STEP_1k;
          else
            stepsize = STEP_500;
          // Preserve filt and nr settings - user controls these explicitly via menu
        }
        if (menu == BAND)
          change = true;
        if (menu == VFOSEL) {
          freq = vfo[vfosel % 2];
          mode = vfomode[vfosel % 2];
          set_lpf(freq /
                  1000000); // Update LPF filter when switching VFO in menu
          if (mode != CW)
            stepsize = STEP_1k;
          else
            stepsize = STEP_500;
          if (mode == CW) {
            filt = 4;
            nr = 0;
          } else
            filt = 0;
          change = true;
        }
        if (menu == ATT) {
          if (dsp_cap == SDR) {
            noInterrupts();
            adc_start(0, !(att & 0x01), F_ADC_CONV);
            admux[0] = ADMUX;
            adc_start(1, !(att & 0x01), F_ADC_CONV);
            admux[1] = ADMUX;
            interrupts();
          }
          digitalWrite(RX, !(att & 0x02));
          pinMode(AUDIO1, (att & 0x04) ? OUTPUT : INPUT);
          pinMode(AUDIO2, (att & 0x04) ? OUTPUT : INPUT);
        }
        if (menu == SIFXTAL)
          change = true;
        if (menu == IQ_ADJ)
          change = true;
#ifdef KEYER
        if (menu == KEY_WPM)
          loadWPM(keyer_speed);
        if (menu == KEY_MODE) {
          if (keyer_mode == 0)
            keyerControl = IAMBICA;
          if (keyer_mode == 1)
            keyerControl = IAMBICB;
          if (keyer_mode == 2)
            keyerControl = SINGLE;
        }
#endif
#ifdef TX_DELAY
        if (menu == TXDELAY)
          semi_qsk = (txdelay > 0);
#endif
      }
    }
  }

  if (prev_bandval != bandval) {
#if PER_BAND_TRACKING
    // uSDXOpen feature: Save current band settings before switching
    band_freq[prev_bandval] = vfo[vfosel % 2];
    band_mode[prev_bandval] = vfomode[vfosel % 2];
    band_filt[prev_bandval] = filt;

    // Restore settings for new band, or use defaults if first time
    if (band_freq[bandval] == 0) {
      freq = band[bandval]; // Use default frequency
    } else {
      freq = band_freq[bandval];
      vfomode[vfosel % 2] = band_mode[bandval];
      mode = band_mode[bandval];
      filt = band_filt[bandval];
    }
#else
    freq = band[bandval];
#endif
    set_lpf(freq / 1000000); // Set LPF filter based on frequency (MHz)
    prev_bandval = bandval;
  }
  vfo[vfosel % 2] = freq;

#ifdef VOX_ENABLE
  // VOX microphone sampling loop - CRITICAL for TX to work!
  if(vox){
    static uint16_t vox_adc;
    static uint8_t vox_sample;

    if(!vox_tx){ // VOX not active
#ifdef MULTI_ADC
      if(vox_sample++ == 16){  // take 16 samples, then process
        ssb(((int16_t)(vox_adc/16) - (512 - AF_BIAS)) >> MIC_ATTEN);   // sampling mic
        vox_sample = 0;
        vox_adc = 0;
      } else {
        vox_adc += analogSampleMic();
      }
#else
      ssb(((int16_t)(analogSampleMic()) - 512) >> MIC_ATTEN);   // sampling mic
#endif
      if(tx){  // TX triggered by audio -> TX
        vox_tx = 1;
        switch_rxtx(255);
      }
    } else if(!tx){  // VOX activated, no audio detected -> RX
      switch_rxtx(0);
      vox_tx = 0;
      delay(32);
    }
  }
#endif //VOX_ENABLE

  wdt_reset();
}

// Fin del archivo usdx_plus_orange.ino
