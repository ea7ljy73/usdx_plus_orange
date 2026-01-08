//  QCX-SSB.ino - https://github.com/threeme3/QCX-SSB
//
//  Copyright 2019, 2020, 2021   Guido PE1NNZ <pe1nnz@qsl.net>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions: The above copyright
//  notice and this permission notice shall be included in all copies or
//  substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
//  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//  TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
//  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. Modifications by
//  EA7LJY - 02112025
//  1. F_XTAL : 27000000
//  2. S-Meter mode : 2 (S-Meter)
//  3. Noise Reduce: default 2
//  4. Change version 1.03x

#define VERSION "1.03x"

#include "usdx_settings.h"
#include <EEPROM.h>

// QCX pin defintions
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
// #define NTX   11        //PB3    (pin 17)
// #define PTX   11        //PB3    (pin 17)

#ifdef SWAP_ROTARY
#undef ROT_A
#undef ROT_B
#define ROT_A 7 // PD7    (pin 13)
#define ROT_B 6 // PD6    (pin 12)
#endif

#if (defined(OLED_SSD1306) || defined(OLED_SH1106))
#define OLED 1
#endif

#if (defined(CAT) || defined(TESTBENCH)) && !(OLED)
#define _SERIAL                                                                \
  1 // Coexistence support for serial port and LCD on the same pins
#endif

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1
#endif

#ifdef TX_CLK0_CLK1
#ifdef F_CLK2
#define TX1RX0 0b11111000
#define TX1RX1 0b11111000
#define TX0RX1 0b11111000
#define TX0RX0 0b11111011
#else //! F_CLK2
#define TX1RX0 0b11111100
#define TX1RX1 0b11111100
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111
#endif // F_CLK2
#else  //! TX_CLK0_CLK1
#define TX1RX0 0b11111011
#define TX1RX1 0b11111000
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111
#endif // TX_CLK0_CLK1

#if defined(F_CLK2) && !defined(TX_CLK0_CLK1)
#error "TX_CLK0_CLK1 must be enabled in order to use F_CLK2."
#endif

#ifndef TX_ENABLE
#undef KEYER
#undef TX_DELAY
#undef SEMI_QSK
#undef RIT_ENABLE
#undef VOX_ENABLE
#undef MOX_ENABLE
#endif //! TX_ENABLE

#ifdef SWR_METER
float FWD;
float SWR;
float ref_V = 5 * 1.15;
static uint32_t stimer;
#define PIN_FWD A6
#define PIN_REF A7
#endif

/*
// UCX installation: On blank chip, use (standard Arduino Uno) fuse settings
(E:FD, H:DE, L:FF), and use customized Optiboot bootloader for 20MHz clock, then
upload via serial interface (with RX, TX and DTR lines connected to pin 1, 2, 3
respectively)
// UCX pin defintions
+#define SDA     3         //PD3    (pin 5)
+#define SCL     4         //PD4    (pin 6)
+#define ROT_A   6         //PD6    (pin 12)
+#define ROT_B   7         //PD7    (pin 13)
+#define RX      8         //PB0    (pin 14)
+#define SIDETONE 9        //PB1    (pin 15)
+#define KEY_OUT 10        //PB2    (pin 16)
+#define NTX     11        //PB3    (pin 17)
+#define DAH     12        //PB4    (pin 18)
+#define DIT     13        //PB5    (pin 19)
+#define AUDIO1  14        //PC0/A0 (pin 23)
+#define AUDIO2  15        //PC1/A1 (pin 24)
+#define DVM     16        //PC2/A2 (pin 25)
+#define BUTTONS 17        //PC3/A3 (pin 26)
// In addition set:
#define OLED  1
#define ONEBUTTON  1
#define ONEBUTTON_INV 1
#undef DEBUG
adjust I2C and I2C_ ports,
ssb_cap=1; dsp_cap=2;
#define _DELAY() for(uint8_t i = 0; i != 5; i++) asm("nop");
#define F_XTAL 20004000
#define F_CPU F_XTAL
*/

// FUSES = { .low = 0xFF, .high = 0xD6, .extended = 0xFD };   // Fuse settings
// should be set at programming (Arduino IDE > Tools > Burn bootloader)

// #if(ARDUINO < 10810)
//    #error "Unsupported Arduino IDE version, use Arduino IDE 1.8.10 or later
//    from https://www.arduino.cc/en/software"
// #endif
#if !(defined(ARDUINO_ARCH_AVR))
#error                                                                         \
    "Unsupported architecture, select Arduino IDE > Tools > Board > Arduino AVR Boards > Arduino Uno."
#endif
#if (F_CPU != 16000000)
#error                                                                         \
    "Unsupported clock frequency, Arduino IDE must specify 16MHz clock; alternate crystal frequencies may be specified with F_MCU."
#endif
#undef F_CPU
#define F_CPU                                                                  \
  20007000 // Actual crystal frequency of 20MHz XTAL1, note that this
           // declaration is just informative and does not correct the timing in
           // Arduino functions like delay(); hence a 1.25 factor needs to be
           // added for correction.
#ifndef F_MCU
#define F_MCU 20000000 // 20MHz ATMEGA328P crystal
#endif

extern char __bss_end;
static int freeMemory() {
  char *sp = reinterpret_cast<char *>(SP);
  return sp - &__bss_end;
} // see: http://www.nongnu.org/avr-libc/user-manual/malloc.html

#ifdef CAT_EXT
volatile uint8_t cat_key = 0;
uint8_t _digitalRead(
    uint8_t pin) { // reads pin or (via CAT) artificially overriden pins
  serialEvent();   // allows CAT update
  if (cat_key) {
    return (pin == BUTTONS) ? ((cat_key & 0x07) > 0)
           : (pin == DIT)   ? ~cat_key & 0x10
           : (pin == DAH)   ? ~cat_key & 0x20
                            : 0;
  } // overrides digitalRead(DIT, DAH, BUTTONS);
  return digitalRead(pin);
}
#else
#define _digitalRead(x) digitalRead(x)
#endif // CAT_EXT

// #define ONEBUTTON_INV 1 // Encoder button goes from PC3 to GND (instead PC3
// to 5V, with 10k pull down)
#ifdef ONEBUTTON_INV
uint8_t inv = 1;
#else
uint8_t inv = 0;
#endif

// #ifdef KEYER
//  Iambic Morse Code Keyer Sketch, Contribution by Uli, DL2DBG. Copyright (c)
//  2009 Steven T. Elliott Source: http://openqrp.org/?p=343,  Trimmed by Bill
//  Bishop - wrb[at]wrbishop.com.  This library is free software; you can
//  redistribute it and/or modify it under the terms of the GNU Lesser General
//  Public License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version. This
//  library is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
//  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
//  more details: Free Software Foundation, Inc., 59 Temple Place, Suite 330,
//  Boston, MA  02111-1307  USA.

// keyerControl bit definitions
#define DIT_L 0x01    // Dit latch
#define DAH_L 0x02    // Dah latch
#define DIT_PROC 0x04 // Dit is being processed
#define PDLSWAP 0x08  // 0 for normal, 1 for swap
#define IAMBICB 0x10  // 0 for Iambic A, 1 for Iambic B
#define IAMBICA 0x00  // 0 for Iambic A, 1 for Iambic B
#define SINGLE 2      // Keyer Mode 0 1 -> Iambic2  2 ->SINGLE

int keyer_speed = 25;
static unsigned long ditTime; // No. milliseconds per dit
static uint8_t keyerControl;
static uint8_t keyerState;
static uint8_t keyer_mode = 2; //->  SINGLE
static uint8_t keyer_swap = 0; //->  DI/DAH

static uint32_t ktimer;
static int Key_state;
int debounce;

enum KSTYPE {
  IDLE,
  CHK_DIT,
  CHK_DAH,
  KEYED_PREP,
  KEYED,
  INTER_ELEMENT
}; // State machine states

void update_PaddleLatch() // Latch dit and/or dah press, called by keyer routine
{
  if (_digitalRead(DIT) == LOW) {
    keyerControl |= keyer_swap ? DAH_L : DIT_L;
  }
  if (_digitalRead(DAH) == LOW) {
    keyerControl |= keyer_swap ? DIT_L : DAH_L;
  }
}

void loadWPM(int wpm) // Calculate new time constants based on wpm value
{
#if (F_MCU != 20000000)
  ditTime = (1200ULL * F_MCU / 16000000) /
            wpm; // ditTime = 1200/wpm;  compensated for F_CPU clock (running in
                 // a 16MHz Arduino environment)
#else
  ditTime =
      (1200 * 5 / 4) / wpm; // ditTime = 1200/wpm;  compensated for 20MHz clock
                            // (running in a 16MHz Arduino environment)
#endif
}
// #endif //KEYER
static uint8_t practice = false; // Practice mode

volatile uint8_t cat_active = 0;
volatile uint32_t rxend_event = 0;
volatile uint8_t vox = 0;

#include <avr/sleep.h>
#include <avr/wdt.h>

// #define _I2C_DIRECT_IO    1 // Enables communications that is not using the
// standard I/O pull-down approach with pull-up resistors, instead I/O is
// directly driven with 0V/5V
#include "src/hardware/wire.h"

uint8_t backlight = 8;
// #define RS_HIGH_ON_IDLE   1   // Experimental LCD support where RS line is
// high on idle periods to comply with SDA I2C standard.

#include "src/driver/display.h"

volatile int8_t encoder_val = 0;
volatile int8_t encoder_step = 0;
static uint8_t last_state;
ISR(PCINT2_vect) { // Interrupt on rotary encoder turn
  // noInterrupts();
  // PCMSK2 &= ~((1 << PCINT22) | (1 << PCINT23));  // mask ROT_A, ROT_B
  // interrupts
  switch (
      last_state =
          (last_state << 4) | (_digitalRead(ROT_B) << 1) |
          _digitalRead(
              ROT_A)) { // transition  (see:
                        // https://www.allaboutcircuits.com/projects/how-to-use-a-rotary-encoder-in-a-mcu-based-project/
                        // )
// #define ENCODER_ENHANCED_RESOLUTION  1
#ifdef ENCODER_ENHANCED_RESOLUTION // Option: enhance encoder from 24 to 96
                                   // steps/revolution, see: appendix 1,
                                   // https://www.sdr-kits.net/documents/PA0KLT_Manual.pdf
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
    //    case 0x31: case 0x10: case 0x02: case 0x23: if(encoder_step < 0)
    //    encoder_step = 0; encoder_step++; if(encoder_step >  3){ encoder_step
    //    = 0; encoder_val++; } break;  // encoder processing for additional
    //    immunity to weared-out rotary encoders case 0x32: case 0x20: case
    //    0x01: case 0x13: if(encoder_step > 0) encoder_step = 0;
    //    encoder_step--; if(encoder_step < -3){ encoder_step = 0;
    //    encoder_val--; } break;
  case 0x23:
    encoder_val++;
    break;
  case 0x32:
    encoder_val--;
    break;
#endif
  }
  // PCMSK2 |= (1 << PCINT22) | (1 << PCINT23);  // allow ROT_A, ROT_B
  // interrupts interrupts();
}
void encoder_setup() {
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  PCMSK2 |=
      (1 << PCINT22) |
      (1
       << PCINT23); // interrupt-enable for ROT_A, ROT_B pin changes; see
                    // https://github.com/EnviroDIY/Arduino-SDI-12/wiki/2b.-Overview-of-Interrupts
  PCICR |= (1 << PCIE2);
  last_state = (_digitalRead(ROT_B) << 1) | _digitalRead(ROT_A);
  interrupts();
}
/*
class Encoder {
public:
  volatile int8_t val = 0;
  volatile int8_t step = 0;
  uint8_t last_state;
  Encoder(){
    pinMode(ROT_A, INPUT_PULLUP);
    pinMode(ROT_B, INPUT_PULLUP);
    PCMSK2 |= (1 << PCINT22) | (1 << PCINT23); // interrupt-enable for ROT_A,
ROT_B pin changes; see
https://github.com/EnviroDIY/Arduino-SDI-12/wiki/2b.-Overview-of-Interrupts
    PCICR |= (1 << PCIE2);
    last_state = (_digitalRead(ROT_B) << 1) | _digitalRead(ROT_A);
    sei();
  }
  void event(){
    switch(last_state = (last_state << 4) | (_digitalRead(ROT_B) << 1) |
_digitalRead(ROT_A)){ //transition  (see:
https://www.allaboutcircuits.com/projects/how-to-use-a-rotary-encoder-in-a-mcu-based-project/
) case 0x31: case 0x10: case 0x02: case 0x23: if(step < 0) step = 0; step++;
if(step >  3){ step = 0; val++; } break; case 0x32: case 0x20: case 0x01: case
0x13: if(step > 0) step = 0; step--; if(step < -3){ step = 0; val--; } break;
    }
  }
};
Encoder enc;
ISR(PCINT2_vect){  // Interrupt on rotary encoder turn
  enc.event();
}*/

// I2C communication starts with a START condition, multiple single
// byte-transfers (MSB first) followed by an ACK/NACK and stops with a STOP
// condition; during data-transfer SDA may only change when SCL is LOW, during a
// START/STOP condition SCL is HIGH and SDA goes DOWN for a START and UP for a
// STOP. https://www.ti.com/lit/an/slva704/slva704.pdf
#include "src/hardware/soft_i2c.h"

// #define log2(n) (log(n) / log(2))
uint8_t log2(uint16_t x) {
  uint8_t y = 0;
  for (; x >>= 1;)
    y++;
  return y;
}

// /*
I2C i2c;
#include "src/driver/si5351.h"

#ifdef LPF_SWITCHING_DL2MAN_USDX_REV1
class PCA9536 {
public:
#define PCA9536_ADDR                                                           \
  0x41 // PCA9536   https://www.ti.com/lit/ds/symlink/pca9536.pdf
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(PCA9536_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    SendRegister(0x03, 0x00);
  } // configuration cmd: IO0-IO7 as output
  inline void write(uint8_t data) {
    init();
    SendRegister(0x01, data);
  } // output port cmd: write bits D7-D0 to IO7-IO0
};
PCA9536 ioext;

void set_latch(
    uint8_t io) { // reset all latches and set latch k to corresponding GPIO,
                  // all relays share a common (ground) GPIO
#define LATCH_TIME 15 // set/reset time latch relay
  for (int i = 0; i != 8; i++) {
    ioext.write((~(1 << i)) | 0x01);
    delay(LATCH_TIME);
  }
  ioext.write(0x00); // reset all latches
  ioext.write((1 << io) | 0x00);
  delay(LATCH_TIME);
  ioext.write(0x00); // set latch wired to io port
}

static uint8_t prev_lpf_io = 0xff;
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io =
      (f > 8)   ? 1
      : (f > 4) ? 2
                : /*(f <= 4)*/ 3; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io != lpf_io) {
    prev_lpf_io = lpf_io;
    set_latch(lpf_io);
  }; // set relay
}
#endif // LPF_SWITCHING_DL2MAN_USDX_REV1

#if defined(LPF_SWITCHING_DL2MAN_USDX_REV3) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2) ||                                 \
    defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA)
class IOExpander16 {
public:
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
#define IOEXP16_ADDR                                                           \
  0x74 // PCA9539 with A1..A0 set to 0
       // https://www.nxp.com/docs/en/data-sheet/PCA9539_PCA9539R.pdf
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV2
#define IOEXP16_ADDR                                                           \
  0x24 // TCA/PCA9555 with A2=1 A1..A0=0
       // https://www.ti.com/lit/ds/symlink/tca9555.pdf
#endif
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
#define IOEXP16_ADDR                                                           \
  0x20 // TCA/PCA9555 with A2=0 A1..A0=0
       // https://www.ti.com/lit/ds/symlink/tca9555.pdf
#endif
  inline void SendRegister(uint8_t reg, uint8_t val) {
    i2c.begin();
    i2c.beginTransmission(IOEXP16_ADDR);
    i2c.write(reg);
    i2c.write(val);
    i2c.endTransmission();
  }
  inline void init() {
    write(0U);
  } // IO0, IO1 as input, IO0 to 0, IO0 as output, IO1 to 0, IO1 as output
  inline void write(uint16_t data) {
    SendRegister(0x07, 0xff);
    SendRegister(0x06, 0xff); /*Common last!*/
    SendRegister(0x02, data);
    SendRegister(0x06, 0x00); /*Common first!*/
    SendRegister(0x03, data >> 8);
    SendRegister(0x07, 0x00);
  } // output port cmd: write bits D15-D0 to IO1.7-0.0;
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

void set_latch(
    uint8_t io, uint8_t common_io,
    bool latch = true) { // reset all latches and set latch k to corresponding
                         // GPIO, all relays share a common (ground) GPIO
#define LATCH_TIME 30    // set/reset time latch relay
  if (latch) {
    ioext.write((1U << io) | 0x0000);
    delay(LATCH_TIME);
    ioext.write(0x0000); // set latch wired to io port
  } else {
    if (io == 0xff) {
      ioext.init();
      for (int io = 0; io != 16; io++)
        set_latch(io, common_io, latch);
    } // reset all latches
    else {
      ioext.write((~(1U << io)) | (1U << common_io));
      delay(LATCH_TIME);
      ioext.write(0x0000);
    } // reset latch wired to io port
  }
}

static uint8_t prev_lpf_io = 0xff; // inits and resets all latches
inline void set_lpf(uint8_t f) {
#ifdef LPF_SWITCHING_DL2MAN_USDX_REV3
  uint8_t lpf_io =
      (f > 26)   ? IO1_3
      : (f > 20) ? IO1_4
      : (f > 17) ? IO1_2
      : (f > 12) ? IO1_5
      : (f > 8)  ? IO1_1
      : (f > 5)  ? IO1_6
      : (f > 4)
          ? IO1_0
          : /*(f <= 4)*/ IO1_7; // cut-off freq in MHz to IO port of LPF relay
#ifndef LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_0, false);
    set_latch(lpf_io, IO0_0);
    prev_lpf_io = lpf_io;
  }; // set relay (latched)
#else
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }; // set relay (non-latched)
#endif // LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH
#else  // LPF_SWITCHING_DL2MAN_USDX_REV2 LPF_SWITCHING_DL2MAN_USDX_REV2_BETA
  uint8_t lpf_io =
      (f > 12)  ? IO0_3
      : (f > 8) ? IO0_5
      : (f > 5) ? IO0_7
      : (f > 4)
          ? IO1_1
          : /*(f <= 4)*/ IO1_3; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io != lpf_io) {
    set_latch(prev_lpf_io, IO0_1, false);
    set_latch(lpf_io, IO0_1);
    prev_lpf_io = lpf_io;
  }; // set relay

#endif
}
#endif // LPF_SWITCHING_DL2MAN_USDX_REV3 LPF_SWITCHING_DL2MAN_USDX_REV2
       // REV2_BETA

#ifdef LPF_SWITCHING_WB2CBA_USDX_OCTOBAND
class MCP23008 {
public:
#define MCP23008_ADDR                                                          \
  0x20 // MCP23008 with A1..A0 set to 0
       // https://ww1.microchip.com/downloads/en/DeviceDoc/21919e.pdf
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
  } // GP0-7 to 0, GP0-7 as output
  inline void write(uint16_t data) {
    SendRegister(0x09, data);
  } // output port cmd: write bits D7-D0 to GP7-GP0
};
MCP23008 ioext;

static uint8_t prev_lpf_io = 0xff; // inits and resets all latches
inline void set_lpf(uint8_t f) {
  uint8_t lpf_io =
      (f > 26)   ? 7
      : (f > 20) ? 6
      : (f > 17) ? 5
      : (f > 12) ? 4
      : (f > 8)  ? 3
      : (f > 6)  ? 2
      : (f > 4)  ? 1
                : /*(f <= 4)*/ 0; // cut-off freq in MHz to IO port of LPF relay
  if (prev_lpf_io == 0xff) {
    ioext.init();
  }
  if (prev_lpf_io != lpf_io) {
    ioext.write(1U << lpf_io);
    prev_lpf_io = lpf_io;
  }; // set relay (non-latched)
}
#endif // LPF_SWITCHING_WB2CBA_USDX_OCTOBAND

#if defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {
  pinMode(PD5, OUTPUT);
  digitalWrite(PD5, (f >= LPF_SWITCHING_PE1DDA_USDXDUO));
}
#endif // LPF_SWITCHING_PE1DDA_USDXDUO

#if !defined(LPF_SWITCHING_DL2MAN_USDX_REV1) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2_BETA) &&                           \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV2) &&                                \
    !defined(LPF_SWITCHING_DL2MAN_USDX_REV3) &&                                \
    !defined(LPF_SWITCHING_WB2CBA_USDX_OCTOBAND) &&                            \
    !defined(LPF_SWITCHING_PE1DDA_USDXDUO)
inline void set_lpf(uint8_t f) {} // dummy
#endif

#include "src/dsp/dsp.h"
ISR(TIMER2_COMPA_vect) // Timer2 COMPA interrupt
{
  func_ptr();
#ifdef DEBUG
  numSamples++;
#endif
}

#pragma GCC pop_options // end of DSP section

/*ISR (TIMER2_COMPA_vect  ,ISR_NAKED) {
asm("push r24         \n\t"
    "lds r24,  0\n\t"
    "sts 0xB4, r24    \n\t"
    "pop r24          \n\t"
    "reti             \n\t");
}*/

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs) {
  DIDR0 |= (1 << adcpin);   // disable digital input
  ADCSRA = 0;               // clear ADCSRA register
  ADCSRB = 0;               // clear ADCSRB register
  ADMUX = 0;                // clear ADMUX register
  ADMUX |= (adcpin & 0x0f); // set analog input pin
  ADMUX |= ((ref1v1) ? (1 << REFS1) : 0) |
           (1 << REFS0); // If reflvl == true, set AREF=1.1V (Internal ref);
                         // otherwise AREF=AVCC=(5V)
  ADCSRA |=
      ((uint8_t)log2((uint8_t)(F_CPU / 13 / fs))) &
      0x07; // ADC Prescaler (for normal conversions non-auto-triggered): ADPS =
            // log2(F_CPU / 13 / Fs) - 1; ADSP=0..7 resulting in resp.
            // conversion rate of 1536, 768, 384, 192, 96, 48, 24, 12 kHz
  // ADCSRA |= (1 << ADIE);  // enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN); // enable ADC
  // ADCSRA |= (1 << ADSC);  // start ADC measurements
#ifdef ADC_NR
  //  set_sleep_mode(SLEEP_MODE_ADC);  // ADC NR sleep destroys the timer2
  //  integrity, therefore Idle sleep is better alternative (keeping clkIO as an
  //  active clock domain)
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
#endif
}

void adc_stop() {
  // ADCSRA &= ~(1 << ADATE); // disable auto trigger
  ADCSRA &= ~(1 << ADIE); // disable interrupts when measurement complete
  ADCSRA |=
      (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 prescaler for 9.6kHz
#ifdef ADC_NR
  sleep_disable();
#endif
  ADMUX = (1 << REFS0); // restore reference voltage AREF (5V)
}

void timer1_start(uint32_t fs) { // Timer 1: OC1A and OC1B in PWM mode
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1) |
            (1 << WGM11); // Clear OC1A/OC1B on compare match, set OC1A/OC1B at
                          // BOTTOM (non-inverting mode)
  TCCR1B |= (1 << CS10) | (1 << WGM13) |
            (1 << WGM12); // Mode 14 - Fast PWM;  CS10: clkI/O/1 (No prescaling)
  ICR1H = 0x00;
  ICR1L = min(255, F_CPU / fs); // PWM value range (fs>78431):  Fpwm = F_CPU /
                                // [Prescaler * (1 + TOP)]
  // TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10); // Clear OC1A/OC1B
  // on compare match, set OC1A/OC1B at BOTTOM (non-inverting mode) TCCR1B |= (1
  // << CS10) | (1 << WGM12); // Mode 5 - Fast PWM, 8-bit;  CS10: clkI/O/1 (No
  // prescaling)
  OCR1AH = 0x00;
  OCR1AL = 0x00; // OC1A (SIDETONE) PWM duty-cycle (span defined by ICR).
  OCR1BH = 0x00;
  OCR1BL = 0x00; // OC1B (KEY_OUT) PWM duty-cycle (span defined by ICR).
}

void timer1_stop() {
  OCR1AL = 0x00;
  OCR1BL = 0x00;
}

void timer2_start(uint32_t fs) { // Timer 2: interrupt mode
  ASSR &= ~(1 << AS2); // Timer 2 clocked from CLK I/O (like Timer 0 and 1)
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2 = 0;
  TCCR2A |= (1 << WGM21);  // WGM21: Mode 2 - CTC (Clear Timer on Compare Match)
  TCCR2B |= (1 << CS22);   // Set C22 bits for 64 prescaler
  TIMSK2 |= (1 << OCIE2A); // enable timer compare interrupt TIMER2_COMPA_vect
  uint8_t ocr =
      ((F_CPU / 64) / fs) - 1; // OCRn = (F_CPU / pre-scaler / fs) - 1;
  OCR2A = ocr;
}

void timer2_stop() {        // Stop Timer 2 interrupt
  TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
  delay(1); // wait until potential in-flight interrupts are finished
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Below a radio-specific implementation based on the above components
// (seperation of concerns)
//
// Feel free to replace it with your own custom radio implementation :-)

void inline lcd_blanks() { lcd.print(F("        ")); }

#define N_FONTS 8
const byte fonts[N_FONTS][8] PROGMEM = {
    {0b01000, // 1; logo
     0b00100, 0b01010, 0b00101, 0b01010, 0b00100, 0b01000, 0b00000},
    {0b00000, // 2; s-meter, 0 bars
     0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
    {0b10000, // 3; s-meter, 1 bars
     0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000},
    {0b10000, // 4; s-meter, 2 bars
     0b10000, 0b10100, 0b10100, 0b10100, 0b10100, 0b10100, 0b10100},
    {0b10000, // 5; s-meter, 3 bars
     0b10000, 0b10101, 0b10101, 0b10101, 0b10101, 0b10101, 0b10101},
    {0b01100, // 6; vfo-a
     0b10010, 0b11110, 0b10010, 0b10010, 0b00000, 0b00000, 0b00000},
    {0b11100, // 7; vfo-b
     0b10010, 0b11100, 0b10010, 0b11100, 0b00000, 0b00000, 0b00000},
    {0b00000, // 8; TBD
     0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}};

#ifndef VSS_METER
int analogSafeRead(
    uint8_t pin,
    bool ref1v1 = false) { // performs classical analogRead with default Arduino
                           // sample-rate and analog reference setting; restores
                           // previous settings
  noInterrupts();
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  uint8_t adcsra = ADCSRA;
  uint8_t admux = ADMUX;
  ADCSRA &= ~(1 << ADIE); // disable interrupts when measurement complete
  ADCSRA |=
      (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 prescaler for 9.6kHz
  if (ref1v1)
    ADMUX &= ~(1 << REFS0); // restore reference voltage AREF (1V1)
  else
    ADMUX = (1 << REFS0); // restore reference voltage AREF (5V)
  delay(1);               // settle
  int val = analogRead(pin);
  ADCSRA = adcsra;
  ADMUX = admux;
  interrupts();
  return val;
}
#else // VSS_METER
uint16_t analogSafeRead(uint8_t adcpin, bool ref1v1 = false) {
  noInterrupts();
  uint8_t oldmux = ADMUX;
  ADMUX = (3 & 0x0f) | ((ref1v1) ? (1 << REFS1) : 0) |
          (1 << REFS0); // set MUX for next conversion   note: hardcoded for
                        // BUTTONS adcpin
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  delayMicroseconds(16); // settle
  ADCSRA |= (1 << ADSC); // start next ADC conversion
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until ADC conversion is completed
  ADMUX = oldmux;
  uint16_t adc = ADC;
  interrupts();
  return adc;
}
#endif

uint16_t analogSampleMic() {
  uint16_t adc;
  noInterrupts();
  ADCSRA =
      (1 << ADEN) | (((uint8_t)log2((uint8_t)(F_CPU / 13 / (192307 / 1)))) &
                     0x07); // hack: faster conversion rate necessary for VOX

  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(
        RX,
        LOW); // disable RF input, only for SDR mod and with low VOX threshold
  // si5351.SendRegister(SI_CLK_OE, TX0RX0);
  uint8_t oldmux = ADMUX;
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  ADMUX = admux[2];      // set MUX for next conversion
  ADCSRA |= (1 << ADSC); // start next ADC conversion
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until ADC conversion is completed
  ADMUX = oldmux;
  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(
        RX,
        HIGH); // enable RF input, only for SDR mod and with low VOX threshold
  // si5351.SendRegister(SI_CLK_OE, TX0RX1);
  adc = ADC;
  interrupts();
  return adc;
}

volatile bool change = true;
volatile int32_t freq = 14000000;
static int32_t vfo[] = {7074000, 14074000};
static uint8_t vfomode[] = {USB, USB};
enum vfo_t { VFOA = 0, VFOB = 1, SPLIT = 2 };
volatile uint8_t vfosel = VFOA;
volatile int16_t rit = 0;

// We measure the average amplitude of the signal (see slow_dsp()) but the
// S-meter should be based on RMS value. So we multiply by 0.707/0.639 in an
// attempt to roughly compensate, although that only really works if the input
// is a sine wave
uint8_t smode = 2;
uint32_t max_absavg256 = 0;
int16_t dbm;

const uint16_t log10_lut[] = {0, 301, 477, 602, 699, 778, 845, 903, 954};

int32_t log10_fix(uint32_t n) {
  if (n == 0)
    return -32768; // Represents negative infinity
  int32_t l = 0;
  uint32_t n_copy = n;
  while (n_copy >= 10) {
    n_copy /= 10;
    l++;
  }
  return l * 1000 + log10_lut[n_copy - 1];
}

static int16_t smeter_cnt = 0;

int16_t smeter(int16_t ref = 0) {
  max_absavg256 = max(_absavg256, max_absavg256); // peak

  if ((smode) && ((++smeter_cnt % 2048) == 0)) { // slowed down display slightly

    int32_t log_val = log10_fix(max_absavg256);
    if (log_val > -32768) {
      int32_t dbm_scaled;
      if (dsp_cap == SDR) {
        // dbm = 20 * log10(max_absavg256) + 6 * att2 - 184.6
        dbm_scaled = (20 * log_val) / 1000 + 6 * att2 - 185;
      } else {
        // dbm = 20 * log10(max_absavg256) + 6 * att2 - 176.2
        dbm_scaled = (20 * log_val) / 1000 + 6 * att2 - 176;
      }
      dbm = dbm_scaled - ref;
    } else {
      dbm = -127 - ref; // Minimum S-meter reading
    }

    lcd.noCursor();
    if (smode == 1) { // dBm meter
      lcd.setCursor(9, 0);
      lcd.print((int16_t)dbm);
      lcd.print(F("dBm "));
    }
    if (smode == 2) { // S-meter
      uint8_t s =
          (dbm < -63)
              ? ((dbm - -127) / 6)
              : (((uint8_t)(dbm - -73)) / 10) *
                    10; // dBm to S (modified to work correctly above S9)
      lcd.setCursor(14, 0);
      if (s < 10) {
        lcd.print('S');
      }
      lcd.print(s);
    }
    if (smode == 3) { // S-bar
      int8_t s = (dbm < -63)
                     ? ((dbm - -127) / 6)
                     : (((uint8_t)(dbm - -73)) / 10) *
                           10; // dBm to S (modified to work correctly above S9)
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
#endif // CW_DECODER
#ifdef VSS_METER
    if (smode ==
        5) { // Supply-voltage indicator; add resistor of value R_VSS (see
             // below) between 12V supply input and pin 26 (PC3)   Contribution
             // by Jeff WB4LCG: https://groups.io/g/ucx/message/4470
#define R_VSS                                                                  \
  1000 // for 1000kOhm from VSS to PC3 (and 10kOhm to GND). Correct this value
       // until VSS is matching
      uint8_t vss10 = (uint32_t)analogSafeRead(BUTTONS, true) * (R_VSS + 10) *
                      11 /
                      (10 * 1024); // use for a 1.1V ADC range VSS measurement
      // uint8_t vss10 = (uint32_t)analogSafeRead(BUTTONS, false) * (R_VSS + 10)
      // * 50 / (10 * 1024);  // use for a 5V ADC range VSS measurement (use for
      // 100k value of R_VSS)
      lcd.setCursor(10, 0);
      lcd.print(vss10 / 10);
      lcd.print('.');
      lcd.print(vss10 % 10);
      lcd.print("V ");
    }
#endif // VSS_METER
#ifdef CLOCK
    if (smode == 6) { // clock-indicator
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
#endif // CLOCK
    stepsize_showcursor();
    max_absavg256 /= 2; // Implement peak hold/decay for all meter types
  }
  return dbm;
}

void start_rx() {
  _init = 1;
  rx_state = 0;
  func_ptr = sdr_rx_00; // enable RX DSP/SDR
  adc_start(2, true, F_ADC_CONV * 4);
  admux[2] = ADMUX; // Note that conversion-rate for TX is factors more
  if (dsp_cap == SDR) {
// #define SWAP_RX_IQ 1    // Swap I/Q ADC inputs, flips RX sideband
#ifdef SWAP_RX_IQ
    adc_start(1, !(att == 1) /*true*/, F_ADC_CONV);
    admux[0] = ADMUX;
    adc_start(0, !(att == 1) /*true*/, F_ADC_CONV);
    admux[1] = ADMUX;
#else
    adc_start(0, !(att == 1) /*true*/, F_ADC_CONV);
    admux[0] = ADMUX;
    adc_start(1, !(att == 1) /*true*/, F_ADC_CONV);
    admux[1] = ADMUX;
#endif     // SWAP_RX_IQ
  } else { // ANALOG, DSP
    adc_start(0, false, F_ADC_CONV);
    admux[0] = ADMUX;
    admux[1] = ADMUX;
  }
  timer1_start(F_SAMP_PWM);
  timer2_start(F_SAMP_RX);
  TCCR1A &= ~(1 << COM1B1);
  digitalWrite(KEY_OUT, LOW); // disable KEY_OUT PWM
}

int16_t _centiGain = 0;

uint8_t txdelay = 0;
uint8_t semi_qsk = false;
uint32_t semi_qsk_timeout = 0;

void switch_rxtx(uint8_t tx_enable) {
  TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
  // delay(1);
  delayMicroseconds(20); // wait until potential RX interrupt is finalized
  noInterrupts();
#ifdef TX_DELAY
#ifdef SEMI_QSK
  if (!(semi_qsk_timeout))
#endif
    if ((txdelay) && (tx_enable) && (!(tx)) &&
        (!(practice))) {     // key-up TX relay in advance before actual
                             // transmission
      digitalWrite(RX, LOW); // TX (disable RX)
#ifdef NTX
      digitalWrite(NTX, LOW); // TX (enable TX)
#endif                        // NTX
#ifdef PTX
      digitalWrite(PTX, HIGH); // TX (enable TX)
#endif                         // PTX
      lcd.setCursor(15, 1);
      lcd.print('D'); // note that this enables interrupts again.
      interrupts();   // hack.. to allow delay()
      delay(F_MCU / 16000000 * txdelay);
      noInterrupts(); // end of hack
    }
#endif // TX_DELAY
  tx = tx_enable;
  if (tx_enable) {          // tx
    _centiGain = centiGain; // backup AGC setting
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
  } else { // rx
    if ((mode == CW) && (!(semi_qsk_timeout))) {
#ifdef SEMI_QSK
#ifdef KEYER
      semi_qsk_timeout = millis() + ditTime * 8;
#else
      semi_qsk_timeout =
          millis() + 8 * 8; // no keyer? assume dit-time of 20 WPM
#endif // KEYER
#endif // SEMI_QSK
      if (semi_qsk)
        func_ptr = dummy;
      else
        func_ptr = sdr_rx_00;
    } else {
      centiGain = _centiGain; // restore AGC setting
#ifdef SEMI_QSK
      semi_qsk_timeout = 0;
#endif
      func_ptr = sdr_rx_00;
    }
  }
  if ((!dsp_cap) && (!tx_enable) && vox)
    func_ptr = dummy; // hack: for SSB mode, disable dsp_rx during vox mode
                      // enabled as it slows down the vox loop too much!
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
#endif // CW_DECODER

  if (tx_enable) { // tx
    if (practice) {
      digitalWrite(RX, LOW); // TX (disable RX)
      lcd.setCursor(15, 1);
      lcd.print('P');
      si5351.SendRegister(SI_CLK_OE, TX0RX0);
      // Do not enable PWM (KEY_OUT), do not enble CLK2
    } else {
      digitalWrite(RX, LOW); // TX (disable RX)
#ifdef NTX
      digitalWrite(NTX, LOW); // TX (enable TX)
#endif                        // NTX
#ifdef PTX
      digitalWrite(PTX, HIGH); // TX (enable TX)
#endif                         // PTX
      lcd.setCursor(15, 1);
      lcd.print('T');
      if (mode == CW) {
        si5351.freq_calc_fast(-cw_offset);
        si5351.SendPLLRegisterBulk();
      } // for CW, TX at freq
#ifdef RIT_ENABLE
      else if (rit) {
        si5351.freq_calc_fast(0);
        si5351.SendPLLRegisterBulk();
      }
#endif // RIT_ENABLE
      si5351.SendRegister(SI_CLK_OE, TX1RX0);
      OCR1AL = 0x80; // make sure SIDETONE is set at 2.5V
      if ((!mox) && (mode != CW))
        TCCR1A &= ~(
            1
            << COM1A1); // disable SIDETONE, prevent interference during SSB TX
      TCCR1A |= (1 << COM1B1); // enable KEY_OUT PWM
#ifdef _SERIAL
      if (cat_active) {
        DDRC &= ~(1 << 2);
      } // disable PC2, so that ADC2 can be used as mic input
#endif
    }
  } else { // rx
#ifdef KEY_CLICK
    if (OCR1BL != 0) {
      for (uint16_t i = 0; i != 31; i++) { // ramp down of amplitude: soft
                                           // falling edge to prevent key clicks
        OCR1BL = lut[pgm_read_byte_near(&ramp[i])];
        delayMicroseconds(60);
      }
    }
#endif                       // KEY_CLICK
    TCCR1A |= (1 << COM1A1); // enable SIDETONE (was disabled to prevent
                             // interference during ssb tx)
    TCCR1A &= ~(1 << COM1B1);
    digitalWrite(KEY_OUT,
                 LOW); // disable KEY_OUT PWM, prevents interference during RX
    OCR1BL = 0;        // make sure PWM (KEY_OUT) is set to 0%
#ifdef QUAD
#ifdef TX_CLK0_CLK1
    si5351.SendRegister(16, 0x0f); // disable invert on CLK0
    si5351.SendRegister(17, 0x0f); // disable invert on CLK1
#else
    si5351.SendRegister(18, 0x0f); // disable invert on CLK2
#endif // TX_CLK0_CLK1
#endif // QUAD
    si5351.SendRegister(SI_CLK_OE, TX0RX1);
#ifdef SEMI_QSK
    if ((!semi_qsk_timeout) ||
        (!semi_qsk)) // enable RX when no longer in semi-qsk phase; so RX and
                     // NTX/PTX outputs are switching only when in RX mode
#endif               // SEMI_QSK
    {
      digitalWrite(RX, !(att == 2)); // RX (enable RX when attenuator not on)
#ifdef NTX
      digitalWrite(NTX, HIGH); // RX (disable TX)
#endif                         // NTX
#ifdef PTX
      digitalWrite(PTX, LOW); // TX (disable TX)
#endif                        // PTX
    }
#ifdef RIT_ENABLE
    si5351.freq_calc_fast(rit);
    si5351.SendPLLRegisterBulk(); // restore original PLL RX frequency
#else
    si5351.freq_calc_fast(0);
    si5351.SendPLLRegisterBulk(); // restore original PLL RX frequency
#endif // RIT_ENABLE
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
      } // enable PC2, so that ADC2 is pulled-down so that CAT TX is not
        // disrupted via mic input
#endif
  }
  OCR2A = ((F_CPU / 64) / ((tx_enable) ? F_SAMP_TX : F_SAMP_RX)) - 1;
  TIMSK2 |= (1 << OCIE2A); // enable timer compare interrupt TIMER2_COMPA_vect
}

uint8_t rx_ph_q = 90;

#ifdef QCX
#define CAL_IQ 1
#ifdef CAL_IQ
int16_t cal_iq_dummy = 0;
// RX I/Q calibration procedure: terminate with 50 ohm, enable CW filter, adjust
// R27, R24, R17 subsequently to its minimum side-band rejection value in dB
void calibrate_iq() {
  smode = 1;
  lcd.setCursor(0, 0);
  lcd_blanks();
  lcd_blanks();
  digitalWrite(SIG_OUT, true); // loopback on
  si5351.freq(freq, 0, 90);    // RX in USB
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
  lcd_blanks();                 // cleanup dbmeter
  digitalWrite(SIG_OUT, false); // loopback off
  si5351.SendRegister(SI_CLK_OE, TX0RX1);
  change = true; // restore original frequency setting
}
#endif
#endif // QCX

uint8_t prev_bandval = 3;
uint8_t bandval = 3;
#define N_BANDS 11

#ifdef CW_FREQS_QRP
uint32_t band[N_BANDS] = {/*472000,*/ 1810000,
                          3560000,
                          5351500,
                          7030000,
                          10106000,
                          14060000,
                          18096000,
                          21060000,
                          24906000,
                          28060000,
                          50096000 /*, 70160000, 144060000*/}; // CW QRP freqs
#else
#ifdef CW_FREQS_FISTS
uint32_t band[N_BANDS] = {/*472000,*/ 1818000,
                          3558000,
                          5351500,
                          7028000,
                          10118000,
                          14058000,
                          18085000,
                          21058000,
                          24908000,
                          28058000,
                          50058000 /*, 70158000, 144058000*/}; // CW FISTS freqs
#else
uint32_t band[N_BANDS] = {/*472000,*/ 1840000,
                          3573000,
                          5357000,
                          7074000,
                          10136000,
                          14074000,
                          18100000,
                          21074000,
                          24915000,
                          28074000,
                          50313000 /*, 70101000, 144125000*/}; // FT8 freqs
#endif
#endif

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
uint32_t stepsizes[10] = {10000000, 1000000, 500000, 100000, 10000,
                          1000,     500,     100,    10,     1};
volatile uint8_t stepsize = STEP_1k;
uint8_t prev_stepsize[] = {STEP_1k,
                           STEP_500}; // default stepsize for resp. SSB, CW

void process_encoder_tuning_step(int8_t steps) {
  int32_t stepval = stepsizes[stepsize];
  // if(stepsize < STEP_100) freq %= 1000; // when tuned and stepsize > 100Hz
  // then forget fine-tuning details
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
  lcd.setCursor(stepsize + 1, 1); // display stepsize with cursor
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
  // lcd.print(F("uSDX"));
  lcd.print(F(ARID));
#endif // QCX
  lcd.print('\x01');
  lcd_blanks();
  lcd_blanks();
}

const char *vfosel_label[] = {"A", "B" /*, "Split"*/};
const char *mode_label[5] = {"LSB", "USB", "CW ", "FM ", "AM "};

inline void display_vfo(int32_t f) {
  lcd.setCursor(0, 1);
  lcd.print((rit) ? ' '
            : ((vfosel % 2) | ((vfosel == SPLIT) & tx))
                ? '\x07'
                : '\x06'); // RIT, VFO A/B

  int32_t scale = 10e6;
  if (rit) {
    f = rit;
    scale = 1e3; // RIT frequency
    lcd.print(F("RIT "));
    lcd.print(rit < 0 ? '-' : '+');
  } else {
    if (f / scale == 0) {
      lcd.print(' ');
      scale /= 10;
    } // Initial space instead of zero
  }
  for (; scale != 1; f %= scale, scale /= 10) {
    lcd.print(abs(f / scale));
    if (scale == (int32_t)1e3 || scale == (int32_t)1e6)
      lcd.print(','); // Thousands separator
  }

  lcd.print(' ');
  lcd.print(mode_label[mode]);
  lcd.print(' ');
  lcd.setCursor(15, 1);
  lcd.print((vox) ? 'V' : 'R');
}

volatile uint8_t event;
// volatile uint8_t menumode = 0;  // 0=not in menu, 1=selects menu item,
// 2=selects parameter value
volatile uint8_t prev_menumode = 0;
volatile int8_t menu = 0; // current parameter id selected in menu

#define pgm_cache_item(addr, sz)                                               \
  byte _item[sz];                                                              \
  memcpy_P(_item, addr, sz); // copy array item from PROGMEM to SRAM
#define get_version_id()                                                       \
  ((VERSION[0] - '1') * 2048 +                                                 \
   ((VERSION[2] - '0') * 10 + (VERSION[3] - '0')) * 32 +                       \
   ((VERSION[4]) ? (VERSION[4] - 'a' + 1) : 0) *                               \
       1) // converts VERSION string with (fixed) format "9.99z" into uint16_t
          // (max. values shown here, z may be removed)

uint8_t eeprom_version;
#define EEPROM_OFFSET                                                          \
  0x150 // avoid collision with QCX settings, overwrites text settings though
#define FT8_EEPROM_ADDR (EEPROM_OFFSET + N_ALL_PARAMS + 2)
#define PREV_MODE_FT8_EEPROM_ADDR (FT8_EEPROM_ADDR + 1)
#define PREV_FILT_FT8_EEPROM_ADDR (PREV_MODE_FT8_EEPROM_ADDR + 1)
#define PREV_AGC_FT8_EEPROM_ADDR (PREV_FILT_FT8_EEPROM_ADDR + 1)
#define PREV_NR_FT8_EEPROM_ADDR (PREV_AGC_FT8_EEPROM_ADDR + 1)
int eeprom_addr;

// Support functions for parameter and menu handling
enum action_t { UPDATE, UPDATE_MENU, NEXT_MENU, LOAD, SAVE, SKIP, NEXT_CH };

// output menuid in x.y format
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
    lcd.setCursor(0, 1); // value on next line
    if (menumode >= 2)
      lcd.print('>');
  } else { // UPDATE (not in menu)
    lcd.setCursor(0, 1);
    lcd.print(label);
    lcd.print(F(": "));
  }
}

void actionCommon(uint8_t action, uint8_t *ptr, uint8_t size) {
  // uint8_t n;
  switch (action) {
  case LOAD:
    // for(n = size; n; --n) *ptr++ = eeprom_read_byte((uint8_t
    // *)eeprom_addr++);
    eeprom_read_block((void *)ptr, (const void *)eeprom_addr, size);
    break;
  case SAVE:
    // noInterrupts();
    // for(n = size; n; --n){ wdt_reset(); eeprom_write_byte((uint8_t
    // *)eeprom_addr++, *ptr++); }
    eeprom_write_block((const void *)ptr, (void *)eeprom_addr, size);
    // interrupts();
    break;
  case SKIP:
    // eeprom_addr += size;
    break;
  }
  eeprom_addr += size;
}

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
    printlabel(action, menuid, label); // print normal/menu label
    if (enumArray == NULL) {           // print value
      if ((_min < 0) && (value >= 0))
        lcd.print('+'); // add + sign for positive values, in case negative
                        // values are supported
      lcd.print(value);
    } else {
      lcd.print(enumArray[value]);
    }
    lcd_blanks();
    lcd_blanks(); // lcd.setCursor(0, 1);
    // if(action == UPDATE) paramAction(SAVE, value, menuid, label, enumArray,
    // _min, _max, continuous, init_val);
    break;
  default:
    actionCommon(action, (uint8_t *)&value, sizeof(value));
    break;
  }
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
      pos++; // allow to go to next character when string size allows and when
             // current character is not string end
    action = UPDATE_MENU; // fall-through next case
  case UPDATE:
  case UPDATE_MENU:
    if (menumode != 3)
      pos = 0;
    if (menumode == 2)
      menumode = 3; // hack: for strings enter in edit mode
    if (((value[pos] + encoder_val) < _min) ||
        ((value[pos] + encoder_val) == 0))
      value[pos] = _min;
    else if ((value[pos] + encoder_val) > _max)
      value[pos] = _max;
    else
      value[pos] = value[pos] + encoder_val;
    encoder_val = 0;

    printlabel(action, menuid, label); // print normal/menu label
    for (int i = 0; i != 13; i++) {
      char ch = value[(pos / 8) * 8 + i];
      if (ch)
        lcd.print(ch);
      else
        break;
    } // print value
    // lcd.print(&value[(pos / 8) * 8]); // print value
    lcd.print('\x01'); // print terminator
    lcd_blanks();
    lcd.setCursor((pos % 8) + (menumode >= 2), 1);
    lcd.cursor();
    break;
  case SAVE:
    for (uint8_t i = size; i > 0; i--) {
      if ((value[i - 1] == ' ') || (value[i - 1] == 0))
        value[i - 1] = 0; // remove trailing spaces
      else
        break; // stop once content found
    }
    // fall-through next case
  default:
    actionCommon(action, (uint8_t *)value, size);
    break;
  }
}
#endif // MENU_STR

static uint32_t save_event_time = 0;
static uint8_t vox_tx = 0;
static uint8_t vox_sample = 0;
static uint16_t vox_adc = 0;

static uint8_t pwm_min =
    0; // PWM value for which PA reaches its minimum: 29 when C31 installed;   0
       // when C31 removed;   0 for biasing BS170 directly
#ifdef QCX
static uint8_t pwm_max = 255; // PWM value for which PA reaches its maximum: 96
                              // when C31 installed; 255 when C31 removed;
#else
static uint8_t pwm_max = 128; // PWM value for which PA reaches its maximum: 128
                              // for biasing BS170 directly
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

#define _N(a) sizeof(a) / sizeof(a[0])

#define N_PARAMS 44 // number of (visible) parameters

#define N_ALL_PARAMS (N_PARAMS + 5) // number of parameters

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
  case DRIVE:
    paramAction(action, drive, 0x33, F("TX Drive"), NULL, 0, 8, false);
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
#ifdef FT8_MODE
  case FT8MODE: {
    uint8_t prev_ft8mode = ft8mode;
    paramAction(action, ft8mode, 0x36, F("FT8 Mode"), offon_label, 0, 1, false);
    if (action == UPDATE_MENU && prev_ft8mode != ft8mode) {
      EEPROM.write(FT8_EEPROM_ADDR, ft8mode);
      if (ft8mode) {
        prev_mode_ft8 = mode;
        prev_filt_ft8 = filt;
        prev_agc_ft8 = agc;
        prev_nr_ft8 = nr;
        EEPROM.write(PREV_MODE_FT8_EEPROM_ADDR, prev_mode_ft8);
        EEPROM.write(PREV_FILT_FT8_EEPROM_ADDR, prev_filt_ft8);
        EEPROM.write(PREV_AGC_FT8_EEPROM_ADDR, prev_agc_ft8);
        EEPROM.write(PREV_NR_FT8_EEPROM_ADDR, prev_nr_ft8);
        mode = USB;
        filt = 1;
        agc = 0;
        nr = 0;
        dig_mode = true;
      } else {
        mode = prev_mode_ft8;
        filt = prev_filt_ft8;
        agc = prev_agc_ft8;
        nr = prev_nr_ft8;
        dig_mode = false;
      }
      change = true;
    }
  } break;
#endif
#ifdef CW_MESSAGE
  case CWINTERVAL:
    paramAction(action, cw_msg_interval, 0x41, F("CQ Interval"), NULL, 0, 60,
                false);
    break;
  case CWMSG1:
    paramAction(action, cw_msg[0], 0x42, F("CQ Message"), sizeof(cw_msg));
    break;
#ifdef CW_MESSAGE_EXT
  case CWMSG2:
    paramAction(action, cw_msg[1], 0x43, F("CW Message 2"), sizeof(cw_msg));
    break;
  case CWMSG3:
    paramAction(action, cw_msg[2], 0x44, F("CW Message 3"), sizeof(cw_msg));
    break;
  case CWMSG4:
    paramAction(action, cw_msg[3], 0x45, F("CW Message 4"), sizeof(cw_msg));
    break;
  case CWMSG5:
    paramAction(action, cw_msg[4], 0x46, F("CW Message 5"), sizeof(cw_msg));
    break;
  case CWMSG6:
    paramAction(action, cw_msg[5], 0x47, F("CW Message 6"), sizeof(cw_msg));
    break;
#endif
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
    break; // workaround for varying N_PARAM and not being able to overflowing
           // default cases properly
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
    break; // keep iterating util menu item found
  }
  return id;
}

void initPins() {
  // initialize
  digitalWrite(SIG_OUT, LOW);
  digitalWrite(RX, HIGH);
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(SIDETONE, LOW);

  // pins
  pinMode(SIDETONE, OUTPUT);
  pinMode(SIG_OUT, OUTPUT);
  pinMode(RX, OUTPUT);
  pinMode(KEY_OUT, OUTPUT);
#ifdef ONEBUTTON
  pinMode(BUTTONS, INPUT_PULLUP); // rotary button
#else
  pinMode(BUTTONS, INPUT); // L/R/rotary button
#endif
  pinMode(DIT, INPUT_PULLUP);
  pinMode(DAH, INPUT); // pull-up DAH 10k via AVCC
  // pinMode(DAH, INPUT_PULLUP); // Could this replace D4? But leaks noisy VCC
  // into mic input!

  digitalWrite(AUDIO1,
               LOW); // when used as output, help can mute RX leakage into AREF
  digitalWrite(AUDIO2, LOW);
  pinMode(AUDIO1, INPUT);
  pinMode(AUDIO2, INPUT);

#ifdef NTX
  digitalWrite(NTX, HIGH);
  pinMode(NTX, OUTPUT);
#endif // NTX
#ifdef PTX
  digitalWrite(PTX, LOW);
  pinMode(PTX, OUTPUT);
#endif // PTX
#ifdef SWR_METER
  pinMode(PIN_FWD, INPUT);
  pinMode(PIN_REF, INPUT);
#endif
#ifdef OLED // assign unused LCD pins
  pinMode(PD4, OUTPUT);
  pinMode(PD5, OUTPUT);
#endif
}

#include "src/interface/cat.h"

void fatal(const __FlashStringHelper *msg, int value = 0, char unit = '\0') {
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

// refresh LUT based on pwm_min, pwm_max
void build_lut() {
  for (uint16_t i = 0; i != 256; i++) // refresh LUT based on pwm_min, pwm_max
    lut[i] = (i * (pwm_max - pwm_min)) / 255 + pwm_min;
  // lut[i] = min(pwm_max, (float)106*log(i) + pwm_min);  // compressed
  // microphone output: drive=0, pwm_min=115, pwm_max=220
}

#ifdef SWR_METER
void readSWR()
// reads FWD / REF values from A6 and A7 and computes SWR
// credit Duwayne, KV4QB
/* This should similar as PE1DDA's (more direct) approach:
    busvoltage = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getPower_mW();
    Vinc = analogRead(3);
    Vref = analogRead(2);
    SWR = (Vinc + Vref) / (Vinc - Vref);
    Vinc = ((Vinc * 5.0) / 1024.0) + 0.5;
    pwr = ((((Vinc) * (Vinc)) - 0.25 ) * k);
    Eff = (pwr) / ((power_mW) / 1000) * 100; */
{
  int32_t v_FWD_raw = 0;
  int32_t v_REF_raw = 0;
  for (int i = 0; i <= 7; i++) {
    v_FWD_raw += analogRead(PIN_FWD);
    v_REF_raw += analogRead(PIN_REF);
    delay(5);
  }

  // v_scaled is voltage * 10000
  int32_t v_FWD_scaled = ((int64_t)v_FWD_raw * 57500) / 8184;
  int32_t v_REF_scaled = ((int64_t)v_REF_raw * 57500) / 8184;

  // p_scaled is power * 100
  int32_t p_FWD_scaled = ((int64_t)v_FWD_scaled * v_FWD_scaled) / 1000000;
  int32_t p_REV_scaled = ((int64_t)v_REF_scaled * v_REF_scaled) / 1000000;

  int32_t VSWR_scaled;
  if (v_FWD_scaled <= v_REF_scaled) {
    VSWR_scaled = 9999; // Indicate infinite SWR with a large value
  } else {
    VSWR_scaled = ((int64_t)(v_FWD_scaled + v_REF_scaled) * 100) /
                  (v_FWD_scaled - v_REF_scaled);
  }

  if (VSWR_scaled > 9999)
    VSWR_scaled = 9999;
  if (VSWR_scaled < 100)
    VSWR_scaled = 100;

  // To avoid changing global variable types, convert back to float at the end.
  float p_FWD_float = (float)p_FWD_scaled / 100.0;
  float VSWR_float = (float)VSWR_scaled / 100.0;

  if (p_FWD_float != FWD || VSWR_float != SWR) {
    lcd.noCursor();
    lcd.setCursor(0, 0);
    switch (swrmeter) {
    case 1:
      lcd.print(" ");
      lcd.print(p_FWD_float, 2);
      lcd.print("W  SWR:");
      lcd.print(VSWR_float, 2);
      break;
    case 2:
      lcd.print(" F:");
      lcd.print(p_FWD_float, 2);
      lcd.print("W R:");
      lcd.print((float)p_REV_scaled / 100.0, 2);
      lcd.print("W");
      break;
    case 3:
      lcd.print(" F:");
      lcd.print((float)v_FWD_scaled / 10000.0, 2);
      lcd.print("V R:");
      lcd.print((float)v_REF_scaled / 10000.0, 2);
      lcd.print("V");
      break;
    }
    FWD = p_FWD_float;
    SWR = VSWR_float;
  }
}
#endif
void setup() {

  digitalWrite(KEY_OUT, LOW); // for safety: to prevent exploding PA MOSFETs, in
                              // case there was something still biasing them.
  si5351.powerDown(); // disable all CLK outputs (especially needed for si5351
                      // variants that has CLK2 enabled by default, such as
                      // Si5351A-B04486-GT)

  // uint8_t mcusr = MCUSR;
  MCUSR = 0;
  // wdt_disable();
  wdt_enable(WDTO_4S); // Enable watchdog
  uint32_t t0, t1;
#ifdef DEBUG
  // Benchmark dsp_tx() ISR (this needs to be done in beginning of setup()
  // otherwise when VERSION containts 5 chars, mis-alignment impact performance
  // by a few percent)
  func_ptr = dsp_tx;
  t0 = micros();
  TIMER2_COMPA_vect();
  // func_ptr();
  t1 = micros();
  uint16_t load_tx = (float)(t1 - t0) * (float)F_SAMP_TX * 100.0 / 1000000.0 *
                     16000000.0 / (float)F_CPU;
  // benchmark sdr_rx_00() ISR
  func_ptr = sdr_rx_00;
  rx_state = 0;
  uint16_t load_rx[8];
  uint16_t load_rx_avg = 0;
  uint16_t i;
  for (i = 0; i != 8; i++) {
    rx_state = i;
    t0 = micros();
    TIMER2_COMPA_vect();
    // func_ptr();
    t1 = micros();
    load_rx[i] = (float)(t1 - t0) * (float)F_SAMP_RX * 100.0 / 1000000.0 *
                 16000000.0 / (float)F_CPU;
    load_rx_avg += load_rx[i];
  }
  load_rx_avg /= 8;

  // adc_stop();  // recover general ADC settings so that analogRead is working
  // again
#endif                  // DEBUG
  ADMUX = (1 << REFS0); // restore reference voltage AREF (5V)

  // disable external interrupts
  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  encoder_setup();

  initPins();

  delay(100);       // at least 40ms after power rises above 2.7V before sending
                    // commands
  lcd.begin(16, 4); // Init LCD
#ifndef OLED
  for (i = 0; i != N_FONTS; i++) { // Init fonts
    pgm_cache_item(fonts[i], 8);
    lcd.createChar(0x01 + i, /*fonts[i]*/ _item);
  }
#endif

  show_banner();
  lcd.setCursor(7, 0);
  lcd.print(F(" R"));
  lcd.print(F(VERSION));
  lcd_blanks();

#ifdef QCX
  // Test if QCX has DSP/SDR capability: SIDETONE output disconnected from
  // AUDIO2
  si5351.SendRegister(SI_CLK_OE, TX0RX0); // Mute QSD
  digitalWrite(
      RX,
      HIGH); // generate pulse on SIDETONE and test if it can be seen on AUDIO2
  delay(1);  // settle
  digitalWrite(SIDETONE, LOW);
  int16_t v1 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, HIGH);
  int16_t v2 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, LOW);
  dsp_cap = !(abs(v2 - v1) > (0.05 * 1024.0 / 5.0)); // DSP capability?
  if (dsp_cap) { // Test if QCX has SDR capability: AUDIO2 is disconnected from
                 // AUDIO1  (only in case of DSP capability)
    delay(400);
    wdt_reset(); // settle:  the following test only works well 400ms after
                 // startup
    v1 = analogRead(AUDIO1);
    digitalWrite(
        AUDIO2,
        HIGH); // generate pulse on AUDIO2 and test if it can be seen on AUDIO1
    pinMode(AUDIO2, OUTPUT);
    delay(1);
    digitalWrite(AUDIO2, LOW);
    delay(1);
    digitalWrite(AUDIO2, HIGH);
    v2 = analogRead(AUDIO1);
    pinMode(AUDIO2, INPUT);
    if (!(abs(v2 - v1) > (0.125 * 1024.0 / 5.0)))
      dsp_cap = SDR; // SDR capacility?
  }
  // Test if QCX has SSB capability: DAH is connected to DVM
  delay(1); // settle
  pinMode(DAH, OUTPUT);
  digitalWrite(DAH, LOW);
  v1 = analogRead(DVM);
  digitalWrite(DAH, HIGH);
  v2 = analogRead(DVM);
  digitalWrite(DAH, LOW);
  // pinMode(DAH, INPUT_PULLUP);
  pinMode(DAH, INPUT);
  ssb_cap = (abs(v2 - v1) > (0.05 * 1024.0 / 5.0)); // SSB capability?

  // ssb_cap = 0; dsp_cap = ANALOG;  // force standard QCX capability
  // ssb_cap = 1; dsp_cap = ANALOG;  // force SSB and standard QCX-RX capability
  // ssb_cap = 1; dsp_cap = DSP;     // force SSB and DSP capability
  // ssb_cap = 1; dsp_cap = SDR;     // force SSB and SDR capability
#endif // QCX

#ifdef DEBUG
  /*if((mcusr & WDRF) && (!(mcusr & EXTRF)) && (!(mcusr & BORF))){
      lcd.setCursor(0, 1); lcd.print(F("!!Watchdog RESET")); lcd_blanks();
      delay(1500); wdt_reset();
    }
    if((mcusr & BORF) && (!(mcusr & WDRF))){
      lcd.setCursor(0, 1); lcd.print(F("!!Brownout RESET")); lcd_blanks();  //
    Brow-out reset happened, CPU voltage not stable or make sure Brown-Out
    threshold is set OK (make sure E fuse is set to FD) delay(1500);
    wdt_reset();
    }
    if(mcusr & PORF){
      lcd.setCursor(0, 1); lcd.print(F("!!Power-On RESET")); lcd_blanks();
      delay(1500); wdt_reset();
    }*/
  /*if(mcusr & EXTRF){
  lcd.setCursor(0, 1); lcd.print(F("Power-On")); lcd_blanks();
    delay(1); wdt_reset();
  }*/

  // Measure CPU loads
  if (!(load_tx <= 100)) {
    fatal(F("CPU_tx"), load_tx, '%');
  }

  if (!(load_rx_avg <= 100)) {
    fatal(F("CPU_rx"), load_rx_avg, '%');
  }
  /*for(i = 0; i != 8; i++){
    if(!(load_rx[i] <= 100)){   // and specify individual timings for each of
  the eight alternating processing functions
      //fatal(F("CPU_rx"), load_rx[i], '%');
      lcd.setCursor(0, 1); lcd.print(F("!!CPU_rx")); lcd.print(i);
  lcd.print('='); lcd.print(load_rx[i]); lcd.print('%'); lcd_blanks();
    }
  }*/
#endif

#ifdef DIAG
  // Measure VDD (+5V); should be ~5V
  si5351.SendRegister(SI_CLK_OE, TX0RX0); // Mute QSD
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(RX, LOW); // mute RX
  delay(100);            // settle
  float vdd = 2.0 * (float)analogRead(AUDIO2) * 5.0 / 1024.0;
  digitalWrite(RX, HIGH);
  if (!(vdd > 4.8 && vdd < 5.2)) {
    fatal(F("V5.0"), vdd, 'V');
  }

  // Measure VEE (+3.3V); should be ~3.3V
  float vee = (float)analogRead(SCL) * 5.0 / 1024.0;
  if (!(vee > 3.2 && vee < 3.8)) {
    fatal(F("V3.3"), vee, 'V');
  }

  // Measure AVCC via AREF and using internal 1.1V reference fed to ADC; should
  // be ~5V
  analogRead(6);    // setup almost proper ADC readout
  bitSet(ADMUX, 3); // Switch to channel 14 (Vbg=1.1V)
  delay(1);         // delay improves accuracy
  bitSet(ADCSRA, ADSC);
  for (; bit_is_set(ADCSRA, ADSC);)
    ;
  float avcc = 1.1 * 1023.0 / ADC;
  if (!(avcc > 4.6 && avcc < 5.2)) {
    fatal(F("Vavcc"), avcc, 'V');
  }

  // Report no SSB capability
  if (!ssb_cap) {
    fatal(F("No MIC input..."));
  }

  // Test microphone polarity
  /*if((ssb_cap) && (!_digitalRead(DAH))){
    fatal(F("MIC in rev.pol"));
  }*/

  // Measure DVM bias; should be ~VAREF/2
#ifdef _SERIAL
  DDRC &= ~(1 << 2); // disable PC2, so that ADC2 can be used as mic input
#else
  PORTD |= 1 << 1;
  DDRD |= 1 << 1; // keep PD1 HIGH so that in case diode is installed to PC2 it
                  // is kept blocked (otherwise ADC2 input is pulled down!)
#endif
  delay(10);
#ifdef TX_ENABLE
  float dvm = (float)analogRead(DVM) * 5.0 / 1024.0;
  if ((ssb_cap) && !(dvm > 1.8 && dvm < 3.2)) {
    fatal(F("Vadc2"), dvm, 'V');
  }
#endif

  // Measure AUDIO1, AUDIO2 bias; should be ~VAREF/2
  if (dsp_cap == SDR) {
    float audio1 = (float)analogRead(AUDIO1) * 5.0 / 1024.0;
    if (!(audio1 > 1.8 && audio1 < 3.2)) {
      fatal(F("Vadc0"), audio1, 'V');
    }
    float audio2 = (float)analogRead(AUDIO2) * 5.0 / 1024.0;
    if (!(audio2 > 1.8 && audio2 < 3.2)) {
      fatal(F("Vadc1"), audio2, 'V');
    }
  }

#ifdef TX_ENABLE
  // Measure I2C Bus speed for Bulk Transfers
  // si5351.freq(freq, 0, 90);
  wdt_reset();
  t0 = micros();
  for (i = 0; i != 1000; i++)
    si5351.SendPLLRegisterBulk();
  t1 = micros();
  uint32_t speed = (1000000 * 8 * 7) / (t1 - t0); // speed in kbit/s
  if (false) {
    fatal(F("i2cspeed"), speed, 'k');
  }

  // Measure I2C Bit-Error Rate (BER); should be error free for a thousand
  // random bulk PLLB writes
  si5351.freq(freq, 0,
              90); // freq needs to be set in order to use freq_calc_fast()
  wdt_reset();
  uint16_t i2c_error = 0; // number of I2C byte transfer errors
  for (i = 0; i != 1000; i++) {
    si5351.freq_calc_fast(i);
    // for(int j = 0; j != 8; j++) si5351.pll_regs[j] = rand();
    si5351.SendPLLRegisterBulk();
#define SI_SYNTH_PLL_A 26
#ifdef NEW_TX
    for (int j = 4; j != 8; j++)
      if (si5351.RecvRegister(SI_SYNTH_PLL_A + j) != si5351.pll_regs[j])
        i2c_error++;
#else
    for (int j = 3; j != 8; j++)
      if (si5351.RecvRegister(SI_SYNTH_PLL_A + j) != si5351.pll_regs[j])
        i2c_error++;
#endif // NEW_TX
  }
  wdt_reset();
  if (i2c_error) {
    fatal(F("BER_i2c"), i2c_error, ' ');
  }
#endif // TX_ENABLE
#endif // DIAG

  drive = 4; // Init settings
#ifdef QCX
  if (!ssb_cap) {
    vfomode[0] = CW;
    vfomode[1] = CW;
    filt = 4;
    stepsize = STEP_500;
  }
  if (dsp_cap != SDR)
    pwm_max = 255; // implies that key-shaping circuit is probably present, so
                   // use full-scale
  if (dsp_cap == DSP)
    volume = 10;
  if (!dsp_cap)
    cw_tone =
        2; // use internal 700Hz QCX filter, so use same offset and keyer tone
#endif     // QCX
  cw_offset = tones[cw_tone];
  // freq = bands[band];

  // Load parameters from EEPROM, reset to factory defaults when stored values
  // are from a different version
  paramAction(LOAD, VERS);
  if ((eeprom_version != get_version_id()) ||
      _digitalRead(BUTTONS)) { // EEPROM clean: if rotary-key pressed or version
                               // signature in EEPROM does NOT corresponds with
                               // this firmware
    eeprom_version = get_version_id();
    // for(int n = 0; n != 1024; n++){ eeprom_write_byte((uint8_t *) n, 0);
    // wdt_reset(); } //clean EEPROM eeprom_write_dword((uint32_t
    // *)EEPROM_OFFSET/3, 0x000000);
    paramAction(SAVE); // save default parameter values
    lcd.setCursor(0, 1);
    lcd.print(F("Reset settings.."));
    delay(500);
    wdt_reset();
  } else {
    paramAction(LOAD); // load all parameters
    ft8mode = EEPROM.read(FT8_EEPROM_ADDR);
    if (ft8mode > 1)
      ft8mode = 0;
    prev_mode_ft8 = EEPROM.read(PREV_MODE_FT8_EEPROM_ADDR);
    prev_filt_ft8 = EEPROM.read(PREV_FILT_FT8_EEPROM_ADDR);
    prev_agc_ft8 = EEPROM.read(PREV_AGC_FT8_EEPROM_ADDR);
    prev_nr_ft8 = EEPROM.read(PREV_NR_FT8_EEPROM_ADDR);
    if (ft8mode) {
      mode = USB;
      filt = 1;
      agc = 0;
      nr = 0;
      dig_mode = true;
    }
  }
  // if(abs((int32_t)F_XTAL - (int32_t)si5351.fxtal) > 50000){ si5351.fxtal =
  // F_XTAL; }  // if F_XTAL frequency deviates too much with actual setting ->
  // use default
  si5351.iqmsa = 0; // enforce PLL reset
  change = true;
  prev_bandval = bandval;
  vox = false; // disable VOX
  nr = 2;      // disable NR
  smode = 2;   // SMeter
  rit = false; // disable RIT
  freq = vfo[vfosel % 2];
  mode = vfomode[vfosel % 2];

#ifdef TX_ENABLE
  build_lut();
#endif

  show_banner(); // remove release number

  start_rx();

#if defined(CAT) || defined(TESTBENCH)
#ifdef CAT_STREAMING
#define BAUD 115200 // Baudrate used for serial communications
#else
#define BAUD                                                                   \
  38400 // 38400 //115200 //4800 //Baudrate used for serial communications (CAT,
        // TESTBENCH)
#endif
  Serial.begin(16000000ULL * BAUD / F_MCU); // corrected for F_CPU=20M
  Command_IF();
#if !defined(OLED) && defined(TESTBENCH)
  smode = 0; // In case of LCD, turn of smeter
#endif
#endif // CAT TESTBENCH

#ifdef KEYER
  keyerState = IDLE;
  keyerControl = IAMBICB; // Or 0 for IAMBICA
  loadWPM(keyer_speed);   // Fix speed at 15 WPM
#endif                    // KEYER

  for (; !_digitalRead(DIT) ||
         ((mode == CW && keyer_mode != SINGLE) && (!_digitalRead(DAH)));) {
    fatal(F("Check PTT/key"));
  } // wait until DIH/DAH/PTT is released to prevent TX on startup
}

static int32_t _step = 0;

void loop() {
#ifdef VOX_ENABLE
  if ((vox) &&
      ((mode == LSB) ||
       (mode ==
        USB))) { // If VOX enabled (and in LSB/USB mode), then take mic samples
                 // and feed ssb processing function, to derive amplitude, and
                 // potentially detect cross vox_threshold to detect a TX or RX
                 // event: this is expressed in tx variable
    if (!vox_tx) { // VOX not active
#ifdef MULTI_ADC
      if (vox_sample++ == 16) { // take N sample, then process
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >>
            MIC_ATTEN); // sampling mic
        vox_sample = 0;
        vox_adc = 0;
      } else {
        vox_adc += analogSampleMic();
      }
#else
      ssb(((int16_t)(analogSampleMic()) - 512) >> MIC_ATTEN); // sampling mic
#endif
      if (tx) { // TX triggered by audio -> TX
        vox_tx = 1;
        switch_rxtx(255);
        // for(;(tx);) wdt_reset();  // while in tx (workaround for RFI feedback
        // related issue) delay(100); tx = 255;
      }
    } else if (!tx) { // VOX activated, no audio detected -> RX
      switch_rxtx(0);
      vox_tx = 0;
      delay(32); // delay(10);
      // vox_adc = 0; for(i = 0; i != 32; i++) ssb(0); //clean buffers
      // for(int i = 0; i != 32; i++) ssb((analogSampleMic() - 512) >>
      // MIC_ATTEN); // clear internal buffer tx = 0; // make sure tx is off
      // (could have been triggered by rubbish in above statement)
    }
  }
#endif // VOX_ENABLE

#ifdef CW_DECODER
  // if((mode == CW) && cwdec) cw_decode();  // if(!(semi_qsk_timeout))
  // cw_decode(); else dec2();
  if ((mode == CW) && cwdec && ((!tx) && (!semi_qsk_timeout)))
    cw_decode(); // CW decoder only active during RX
#endif           // CW_DECODER

  if (menumode == 0) { // in main
#ifdef CW_DECODER
    if (cw_event) {
      uint8_t offset = (uint8_t[]){
          0, 7, 3, 5, 3, 7, 8}[smode]; // depending on smeter more/less cw-text
      lcd.noCursor();
#ifdef OLED
      // cw_event = false; for(int i = 0; out[offset + i] != '\0'; i++){
      // lcd.setCursor(i, 0); lcd.print(out[offset + i]); if((!tx) &&
      // (!semi_qsk_timeout)) cw_decode(); }   // like 'lcd.print(out +
      // offset);' but then in parallel calling cw_decoding() to handle long
      // OLED writes uint8_t i = cw_event - 1; if(out[offset + i]){
      // lcd.setCursor(i, 0); lcd.print(out[offset + i]); cw_event++; } else
      // cw_event = false;  // since an oled string write would hold-up reliable
      // decoding/keying, write only a single char each time and continue
      uint8_t i = cw_event - 1;
      if (15 - offset - i + 1) {
        lcd.setCursor(15 - offset - i, 0);
        lcd.print(out[15 - i]);
        cw_event++;
      } else
        cw_event = false; // since an oled string write would hold-up reliable
                          // decoding/keying, write only a single char each time
                          // and continue
#else
      cw_event = false;
      lcd.setCursor(0, 0);
      lcd.print(out + offset);
#endif
      stepsize_showcursor();
    } else
#endif // CW_DECODER
      if ((!semi_qsk_timeout) && (!vox_tx))
        smeter();
  }

#ifdef KEYER                                // Keyer
  if (mode == CW && keyer_mode != SINGLE) { // check DIT/DAH keys for CW

    switch (
        keyerState) { // Basic Iambic Keyer, keyerControl contains processing
                      // flags and keyer mode bits, Supports Iambic A and B,
                      // State machine based, uses calls to millis() for timing.
    case IDLE:        // Wait for direct or latched paddle press
      if ((_digitalRead(DAH) == LOW) || (_digitalRead(DIT) == LOW) ||
          (keyerControl & 0x03)) {
#ifdef CW_MESSAGE
        cw_msg_event = 0; // clear cw message event
#endif                    // CW_MESSAGE
        update_PaddleLatch();
        keyerState = CHK_DIT;
      }
      break;
    case CHK_DIT: // See if the dit paddle was pressed
      if (keyerControl & DIT_L) {
        keyerControl |= DIT_PROC;
        ktimer = ditTime;
        keyerState = KEYED_PREP;
      } else {
        keyerState = CHK_DAH;
      }
      break;
    case CHK_DAH: // See if dah paddle was pressed
      if (keyerControl & DAH_L) {
        ktimer = ditTime * 3;
        keyerState = KEYED_PREP;
      } else {
        keyerState = IDLE;
      }
      break;
    case KEYED_PREP: // Assert key down, start timing, state shared for dit or
                     // dah
      Key_state = HIGH;
      switch_rxtx(Key_state);
      ktimer += millis();               // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L); // clear both paddle latch bits
      keyerState = KEYED;               // next state
      break;
    case KEYED:                // Wait for timer to expire
      if (millis() > ktimer) { // are we at end of key down ?
        Key_state = LOW;
        switch_rxtx(Key_state);
        ktimer = millis() + ditTime; // inter-element time
        keyerState = INTER_ELEMENT;  // next state
      } else if (keyerControl & IAMBICB) {
        update_PaddleLatch(); // early paddle latch in Iambic B mode
      }
      break;
    case INTER_ELEMENT:
      // Insert time between dits/dahs
      update_PaddleLatch();                    // latch paddle state
      if (millis() > ktimer) {                 // are we at end of inter-space ?
        if (keyerControl & DIT_PROC) {         // was it a dit or dah ?
          keyerControl &= ~(DIT_L + DIT_PROC); // clear two bits
          keyerState = CHK_DAH;                // dit done, check for dah
        } else {
          keyerControl &= ~(DAH_L); // clear dah latch
          keyerState = IDLE;        // go idle
        }
      }
      break;
    }

  } else {
#endif // KEYER

#ifdef TX_ENABLE
    uint8_t pin = ((mode == CW) && (keyer_swap)) ? DAH : DIT;
    if (!vox_tx) //  ONLY if VOX not active, then check DIT/DAH (fix for VOX to
                 //  prevent RFI feedback through EMI on DIT or DAH line)
      if (!_digitalRead(pin)) { // PTT/DIT keys transmitter
#ifdef CW_MESSAGE
        cw_msg_event = 0; // clear cw message event
#endif                    // CW_MESSAGE
        switch_rxtx(1);
        do {
          wdt_reset();
          delay((mode == CW)
                    ? 10
                    : 100); // keep the tx keyed for a while before sensing
                            // (helps against RFI issues on DAH/DAH line)
#ifdef SWR_METER
          if (smeter > 0 && mode == CW && millis() >= stimer) {
            readSWR();
            stimer = millis() + 500;
          }
#endif
          if (inv ^ _digitalRead(BUTTONS))
            break; // break if button is pressed (to prevent potential lock-up)
        } while (!_digitalRead(pin)); // until released
        switch_rxtx(0);
      }
#endif // TX_ENABLE
#ifdef KEYER
  }
#endif // KEYER

#ifdef SEMI_QSK
  if ((semi_qsk_timeout) && (millis() > semi_qsk_timeout)) {
    switch_rxtx(0);
  } // delayed QSK RX
#endif
  enum event_t {
    BL = 0x10,
    BR = 0x20,
    BE = 0x30,
    SC = 0x01,
    DC = 0x02,
    PL = 0x04,
    PLC = 0x05,
    PT = 0x0C
  }; // button-left, button-right and button-encoder; single-click,
     // double-click, push-long, push-and-turn
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
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true);
// #define MODE_CHANGE_RESETS  1
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
        si5351.iqmsa = 0; // enforce PLL reset
#ifdef CW_DECODER
        if ((prev_mode == CW) && (cwdec))
          show_banner();
#endif
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
        // make more generic:
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500;
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0;
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
          _menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        } // short encoder-click while in value selection screen: save, and
          // return to menu screen
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
      bandval++;
      // if(bandval >= N_BANDS) bandval = 0;
      if (bandval >= (N_BANDS - 1))
        bandval = 1; // excludes 6m, 160m
      stepsize = STEP_1k;
      change = true;
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
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true);
// #define MODE_CHANGE_RESETS  1
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
      stepsize += 1;
      if (stepsize < STEP_1k)
        stepsize = STEP_10;
      if (stepsize > STEP_10)
        stepsize = STEP_1k;
      stepsize_showcursor();
      break;
    case BE | PLC: // or kept pressed
      menumode = 2;
      break;
    case BE | PT:
      menumode = 1;
      // if(menu == 0) menu = 1;
      break;
    case BL | SC:
    case BL | DC:
    case BL | PL:
    case BL | PLC:
      encoder_val++;
      break;
    case BR | SC:
    case BR | DC:
    case BR | PL:
    case BR | PLC:
      encoder_val--;
      break;
#endif // ONEBUTTON
    }
  } else
    event = 0; // no button pressed: reset event

  if ((menumode) || (prev_menumode != menumode)) { // Show parameter and value
    int8_t encoder_change = encoder_val;
    if ((menumode == 1) && encoder_change) {
      menu += encoder_val; // Navigate through menu
#ifdef ONEBUTTON
      menu = max(0, min(menu, N_PARAMS));
#else
      menu = max(1 /* 0 */, min(menu, N_PARAMS));
#endif
      menu = paramAction(NEXT_MENU,
                         menu); // auto probe next menu item (gaps may exist)
      encoder_val = 0;
    }
    if (encoder_change || (prev_menumode != menumode))
      paramAction(UPDATE_MENU,
                  (menumode)
                      ? menu
                      : 0); // update param with encoder change and display
    prev_menumode = menumode;
    if (menumode == 2) {
      if (encoder_change) {
        lcd.setCursor(0, 1);
        lcd.cursor();       // edits menu item value; make cursor visible
        if (menu == MODE) { // post-handling Mode parameter
          vfomode[vfosel % 2] = mode;
          paramAction(SAVE,
                      (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
          change = true;
          si5351.iqmsa = 0; // enforce PLL reset
          // make more generic:
          if (mode != CW)
            stepsize = STEP_1k;
          else
            stepsize = STEP_500;
          if (mode == CW) {
            filt = 4;
            nr = 0;
          } else
            filt = 0;
        }
        if (menu == BAND) {
          change = true;
        }

        // if(menu == NR){ if(mode == CW) nr = false; }
        if (menu == VFOSEL) {
          freq = vfo[vfosel % 2];
          mode = vfomode[vfosel % 2];
          // make more generic:
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
#ifdef RIT_ENABLE
        if (menu == RIT) {
          stepsize = (rit) ? STEP_10 : STEP_500;
          change = true;
        }
#endif
        // if(menu == VOX){ if(vox){ vox_thresh-=1; } else { vox_thresh+=1; }; }
        if (menu == ATT) { // post-handling ATT parameter
          if (dsp_cap == SDR) {
            noInterrupts();
#ifdef SWAP_RX_IQ
            adc_start(1, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[0] = ADMUX;
            adc_start(0, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[1] = ADMUX;
#else
            adc_start(0, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[0] = ADMUX;
            adc_start(1, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[1] = ADMUX;
#endif // SWAP_RX_IQ
            interrupts();
          }
          digitalWrite(
              RX, !(att & 0x02)); // att bit 1 ON: attenuate -20dB by disabling
                                  // RX line, switching Q5 (antenna input
                                  // switch) into 100k resistence
          pinMode(AUDIO1, (att & 0x04)
                              ? OUTPUT
                              : INPUT); // att bit 2 ON: attenuate -40dB by
                                        // terminating ADC inputs with 10R
          pinMode(AUDIO2, (att & 0x04) ? OUTPUT : INPUT);
        }
        if (menu == SIFXTAL) {
          change = true;
        }
#ifdef TX_ENABLE
        if ((menu == PWM_MIN) || (menu == PWM_MAX)) {
          build_lut();
        }
#endif
        if (menu == CWTONE) {
          if (dsp_cap) {
            cw_offset = (cw_tone == 0) ? tones[0] : tones[1];
            paramAction(SAVE, CWOFF);
          }
        }
        if (menu == IQ_ADJ) {
          change = true;
        }
#ifdef CAL_IQ
        if (menu == CALIB) {
          if (dsp_cap != SDR)
            calibrate_iq();
          menu = 0;
        }
#endif
#ifdef KEYER
        if (menu == KEY_WPM) {
          loadWPM(keyer_speed);
        }
        if (menu == KEY_MODE) {
          if (keyer_mode == 0) {
            keyerControl = IAMBICA;
          }
          if (keyer_mode == 1) {
            keyerControl = IAMBICB;
          }
          if (keyer_mode == 2) {
            keyerControl = SINGLE;
          }
        }
#endif // KEYER
#ifdef TX_DELAY
        if (menu == TXDELAY) {
          semi_qsk = (txdelay > 0);
        }
#endif // TX_DELAY
      }
#ifdef DEBUG
      if (menu == SR) { // measure sample-rate
        numSamples = 0;
        delay(F_MCU * 500UL /
              16000000); // delay 0.5s (in reality because F_CPU=20M instead of
                         // 16M, delay() is running 1.25x faster therefore we
                         // need to multiply with 1.25)
        sr = numSamples * 2;            // samples per second
        paramAction(UPDATE_MENU, menu); // refresh
      }
      if (menu == CPULOAD) { // measure CPU-load
        uint32_t i = 0;
        uint32_t prev_time = millis();
        for (i = 0; i != 300000; i++)
          wdt_reset(); // fixed CPU-load 132052*1.25us delay under 0% load
                       // condition; is 132052*1.25 * 20M = 3301300 CPU cycles
                       // fixed load
        cpu_load = 100 - 132 * 100 / (millis() - prev_time);
        paramAction(UPDATE_MENU, menu); // refresh
      }
      if ((menu == PARAM_A) || (menu == PARAM_B) || (menu == PARAM_C)) {
        delay(300);
        paramAction(UPDATE_MENU, menu); // refresh
      }
#endif
    }
  }

  if (menumode == 0) {
    if (encoder_val) { // process encoder tuning steps
      process_encoder_tuning_step(encoder_val);
      encoder_val = 0;
    }
  }

  if ((change) && (!tx) && (!vox_tx)) { // only change if TX is OFF, prevent
                                        // simultaneous I2C bus access
    change = false;
    if (prev_bandval != bandval) {
      freq = band[bandval];
      prev_bandval = bandval;
    }
    vfo[vfosel % 2] = freq;
    save_event_time =
        millis() + 1000; // schedule time to save freq (no save while tuning,
                         // hence no EEPROM wear out)

    if (menumode == 0) {
      display_vfo(freq);
      stepsize_showcursor();
#ifdef CAT
      // Command_GETFreqA();
#endif

      // The following is a hack for SWR measurement:
      // si5351.alt_clk2(freq + 2400);
      // si5351.SendRegister(SI_CLK_OE, TX1RX1);
      // digitalWrite(SIG_OUT, HIGH);  // inject CLK2 on antenna input via 120K
    }

    // noInterrupts();
    uint8_t f = freq / 1000000UL;
    set_lpf(f);
    bandval = (f > 32)   ? 10
              : (f > 26) ? 9
              : (f > 22) ? 8
              : (f > 20) ? 7
              : (f > 16) ? 6
              : (f > 12) ? 5
              : (f > 8)  ? 4
              : (f > 6)  ? 3
              : (f > 4)  ? 2
              : (f > 2)  ? 1
                         : 0;
    prev_bandval = bandval; // align bandval with freq

    if (mode == CW) {
      si5351.freq(freq + cw_offset, rx_ph_q,
                  0 /*90, 0*/); // RX in CW-R (=LSB), correct for CW-tone offset
    } else if (mode == LSB)
      si5351.freq(freq, rx_ph_q, 0 /*90, 0*/); // RX in LSB
    else
      si5351.freq(freq, 0, rx_ph_q /*0, 90*/); // RX in USB, ...
#ifdef RIT_ENABLE
    if (rit) {
      si5351.freq_calc_fast(rit);
      si5351.SendPLLRegisterBulk();
    }
#endif // RIT_ENABLE
    // interrupts();
  }

  if ((save_event_time) &&
      (millis() >
       save_event_time)) { // save freq when time has reached schedule
    paramAction(SAVE, (vfosel % 2) ? FREQB : FREQA); // save vfoa/b changes
    save_event_time = 0;
    // lcd.setCursor(15, 1); lcd.print('S'); delay(100); lcd.setCursor(15, 1);
    // lcd.print('R');
  }

#ifdef CW_MESSAGE
  if ((mode == CW) && (cw_msg_event) &&
      (millis() > cw_msg_event)) { // if it is time to send a CW message
    if ((cw_tx(cw_msg[cw_msg_id]) == 0) &&
        ((cw_msg[cw_msg_id][0] == 'C') && (cw_msg[cw_msg_id][1] == 'Q')) &&
        cw_msg_interval)
      cw_msg_event = millis() + 1000 * cw_msg_interval;
    else
      cw_msg_event =
          0; // then send message, if not interrupted and its a CQ msg and there
             // is an interval set, then schedule new event
  }
#endif // CW_MESSAGE

  wdt_reset();

  //{ lcd.setCursor(0, 0); lcd.print(freeMemory()); lcd.print(F("    ")); }
}

/* BACKLOG:
code definitions and re-use for comb, integrator, dc decoupling, arctan
refactor main()
agc based on rms256, agc/smeter after filter
noisefree integrator (rx audio out) in lower range
raised cosine tx amp for cw, 4ms tau seems enough:
http://fermi.la.asu.edu/w9cf/articles/click/index.html 32 bin fft dynamic range
cw att extended agc Split undersampling, IF-offset K2/TS480 CAT control faster
RX-TX switch to support CW usdx API demo code scan move last bit of arrays into
flash?
https://web.archive.org/web/20180324010832/https://www.microchip.com/webdoc/AVRLibcReferenceManual/FAQ_1faq_rom_array.html
u-law in RX path?:
http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
Arduino library?
1. RX bias offset correction by measurement avg, 2. charge decoupling cap. by
resetting to 0V and setting 5V for a certain amount of (charge) time add 1K
(500R?) to GND at TTL RF output to keep zero-level below BS170 threshold
additional PWM output for potential BOOST conversion
squelch gating
more buttons
s-meter offset vs DC bal.
keyer with interrupt-driven timers (to reduce jitter)

Analyse assembly:
/home/guido/Downloads/arduino-1.8.10/hardware/tools/avr/bin/avr-g++ -S -g -Os -w
-std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections
-fno-threadsafe-statics -Wno-error=narrowing -MMD -mmcu=atmega328p
-DF_CPU=16000000L -DARDUINO=10810 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR
-I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/cores/arduino
-I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/variants/standard
/tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp -o
/tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp.txt

Rewire/code I/Q clk pins so that a Div/1 and Div/2 scheme is used instead of 0
and 90 degrees phase shift 10,11,13,12   10,11,12,13  (pin) Q- I+ Q+ I-   Q- I+
Q+ I- 90 deg.shift  div/2@S1(pin2)

50MHz LSB OK, USB NOK

atmega328p signature: https://forum.arduino.cc/index.php?topic=341799.15
https://www.eevblog.com/forum/microcontrollers/bootloader-on-smd-atmega328p-au/msg268938/#msg268938
https://www.avrfreaks.net/forum/undocumented-signature-row-contents

Alain k1fm AGC sens issue:  https://groups.io/g/ucx/message/3998
https://groups.io/g/ucx/message/3999 txdelay when vox is on (disregading the
tx>0 state due to ssb() overrule, instead use RX-digitalinput) Adrian: issue
#41, set cursor just after writing 'R' when smeter is off, and (menumode == 0)
Konstantinos: backup/restore vfofilt settings when changing vfo.
Bob: 2mA for clk0/1 during RX
Uli: accuracate voltages during diag

agc behind filter
vcc adc extend. power/curr measurement
swr predistort eff calc
block ptt while in vox mode

adc bias error and potential error correction
noise burst on tx
https://groups.io/g/ucx/topic/81030243#6265

for (size_t i = 0; i < 9; i++) id[i] = boot_signature_byte_get(0x0E + i + (i >
5));



*/
