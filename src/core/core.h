#ifndef CORE_H
#define CORE_H

#include "../hardware/wire.h"
#include <Arduino.h>

// Forward declarations for circular dependencies
void stepsize_showcursor();
void switch_rxtx(uint8_t tx_enable);

// Core Application Logic
// Includes globals, helpers, menu system, and ISRs
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
#include "../hardware/wire.h"

uint8_t backlight = 8;
// #define RS_HIGH_ON_IDLE   1   // Experimental LCD support where RS line is
// high on idle periods to comply with SDA I2C standard.

#include "../driver/display.h"

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
#include "../hardware/soft_i2c.h"

// #define log2(n) (log(n) / log(2))
uint8_t log2(uint16_t x) {
  uint8_t y = 0;
  for (; x >>= 1;)
    y++;
  return y;
}

// /*
I2C i2c;
#include "../driver/si5351.h"

#include "../hardware/io_expander.h"

#include "../dsp/dsp.h"
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

#include "../hal/hal.h"

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

#include "../interface/cat.h"

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

#endif // CORE_H
