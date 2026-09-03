// hw.h - uSDX Plus Orange v2
// Hardware: pins, ADC, timers (PWM + sample-rate ISR), ISR dispatch.
// Extracted from v1 initPins()/adc_start()/timer1_start()/switch_rxtx().

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdint.h>

#include "cw.h" // cw_set_keyed for decoder feeding
#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Function pointer ISR dispatch (TX/RX DSP bodies run here)
// Definition owned by rx.h (typedef func_t + extern). hw.h defines the symbol.
// ---------------------------------------------------------------------------
volatile func_t func_ptr;

// no-op ISR target (Semi-QSK mutes RX while waiting, legacy 2187)
void dummy() {}

// ---------------------------------------------------------------------------
// ADC setup
// ---------------------------------------------------------------------------
// admux[] is owned by rx.h (ADC channel selectors for I/Q/mic)

uint8_t log2(uint16_t x) {
  uint8_t y = 0;
  for(; x >>= 1;)
    y++;
  return y;
}

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs) {
  DIDR0 |= (1 << adcpin);
  ADCSRA = 0;
  ADCSRB = 0;
  ADMUX  = 0;
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

// ---------------------------------------------------------------------------
// Timer1 PWM (SIDETONE + KEY_OUT envelope)
// ---------------------------------------------------------------------------
void timer1_start(uint32_t fs) {
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1A |= (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
  TCCR1B |= (1 << CS10) | (1 << WGM13) | (1 << WGM12); // Fast PWM, no prescaler
  ICR1H  = 0x00;
  ICR1L  = min(255, F_CPU / fs);
  OCR1AH = 0x00;
  OCR1AL = 0;
  OCR1BH = 0x00;
  OCR1BL = 0;
}

// ---------------------------------------------------------------------------
// Sample-rate timer2 ISR (calls func_ptr)
// ---------------------------------------------------------------------------
ISR(TIMER2_COMPA_vect) { func_ptr(); }

void timer2_start(uint32_t fs) {
  ASSR &= ~(1 << AS2); // Timer2 clocked from CLK I/O (like Timer0/1)
  TCCR2A = 0;
  TCCR2B = 0;
  TCNT2  = 0;
  TCCR2A |= (1 << WGM21); // CTC
  TCCR2B |= (1 << CS22);  // 64 prescaler
  TIMSK2 |= (1 << OCIE2A);               // enable before OCR2A (legacy 3411)
  OCR2A = ((F_CPU / 64) / fs) - 1;
}

// Legacy parity (usdx-legazy:3394,3413): stop timers cleanly
void timer1_stop() {
  OCR1AL = 0x00;
  OCR1BL = 0x00;
}
void timer2_stop() { // Stop Timer2 interrupt
  TIMSK2 &= ~(1 << OCIE2A);
  delay(1); // wait until potential in-flight interrupts are finished
}

// legacy powerDown (usdx-legazy:3874-3920): sleep until BUTTONS pin-change wakes
void powerDown() {
  lcd.setCursor(0, 1);
  lcd.print("Power-off 73 :-)");
  lcd.print("                ");

  MCUSR = ~(1 << WDRF); // may be done before wdt_disable()
  wdt_disable();

  timer2_stop();
  timer1_stop();
  adc_stop();

  si5351.powerDown();

  delay(1500);

  // Disable external interrupts INT0, INT1, Pin Change
  PCICR  = 0;
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
  sleep_cpu(); // go to sleep, wake-up by Pin Change, ...
  sleep_disable();

  do {
    wdt_enable(WDTO_15MS);
    for(;;)
      ;
  } while(0); // soft reset by triggering watchdog timeout
}

// ---------------------------------------------------------------------------
// Microphone sampling for VOX detection
// ---------------------------------------------------------------------------
uint16_t analogSampleMic() {
  uint16_t adc;
  noInterrupts();
  ADCSRA = (1 << ADEN) | (((uint8_t)log2((uint8_t)(F_CPU / 13 / (192307 / 1)))) & 0x07);

  if(vox_thresh >= 32) digitalWrite(RX, LOW); // disable RF input (legacy 3533, SDR always)
  uint8_t oldmux = ADMUX;
  for(; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = admux[2];
  ADCSRA |= (1 << ADSC);
  for(; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = oldmux;
  if(vox_thresh >= 32) digitalWrite(RX, HIGH); // enable RF input (legacy 3541)
  adc   = ADC;
  interrupts();
  return adc;
}

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------
void initPins() {
  digitalWrite(SIDETONE, LOW);
  digitalWrite(RX, HIGH);
  digitalWrite(KEY_OUT, LOW);

  pinMode(SIDETONE, OUTPUT);
  pinMode(RX, OUTPUT);
  pinMode(KEY_OUT, OUTPUT);
  pinMode(BUTTONS, INPUT); // L/R/rotary button (v1: no internal pullup - HW has it)
  pinMode(DIT, INPUT_PULLUP);
  pinMode(DAH, INPUT); // pull-up DAH 10k via AVCC (v1)

  digitalWrite(AUDIO1, LOW);
  digitalWrite(AUDIO2, LOW);
  pinMode(AUDIO1, INPUT);
  pinMode(AUDIO2, INPUT);

  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
}

// ---------------------------------------------------------------------------
// start_rx() - arm RX DSP (usdx-legazy 3617 parity)
// ---------------------------------------------------------------------------
void start_rx() {
  _init    = 1;
  rx_state = 0;
  func_ptr = sdr_rx_00; // enable RX DSP/SDR
  adc_start(2, true, F_ADC_CONV * 4);
  admux[2] = ADMUX; // mic (4x slower for TX, like legacy)
  adc_start(0, !(att == 1), F_ADC_CONV);
  admux[0] = ADMUX;
  adc_start(1, !(att == 1), F_ADC_CONV);
  admux[1] = ADMUX;
  timer1_start(F_SAMP_PWM);
  timer2_start(F_SAMP_RX);
  TCCR1A &= ~(1 << COM1B1);
  digitalWrite(KEY_OUT, LOW); // disable KEY_OUT PWM
}

// ---------------------------------------------------------------------------
// switch_rxtx - TX/RX switching (WHITE_BUTTONS config active)
// ---------------------------------------------------------------------------
extern volatile uint8_t tx; // from tx.h
extern volatile uint8_t vox_tx;
extern volatile uint8_t txdelay;
extern volatile uint8_t practice;

// Semi-QSK (defined in main .ino / rx.h)
extern volatile uint32_t semi_qsk_timeout;
extern volatile uint8_t  semi_qsk;
extern volatile uint8_t  cwdec; // CW decoder enable (main .ino)

void switch_rxtx(uint8_t tx_enable) {
  TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
  delayMicroseconds(20);    // allow RX ISR to finalize
  noInterrupts();
#ifdef TX_DELAY
#ifdef SEMI_QSK
  if(!(semi_qsk_timeout))
#endif
    if((txdelay) && (tx_enable) && (!(tx)) && (!(practice))) { // key-up TX relay in advance (legacy 3655)
      digitalWrite(RX, LOW);                                  // TX (disable RX)
#ifdef PTX
      digitalWrite(PTX, HIGH); // TX (enable TX)
#endif
      lcd.setCursor(15, 1);
      lcd.print('D'); // note that this enables interrupts again.
      interrupts();   // hack.. to allow delay()
      delay(F_MCU / 16000000 * txdelay);
      noInterrupts(); // end of hack
    }
#endif //TX_DELAY
  tx = tx_enable;
  if(tx_enable) { // tx
    _centiGain = centiGain; // backup AGC setting (legacy 3671)
#ifdef SEMI_QSK
    semi_qsk_timeout = 0;
#endif
    switch(mode) {
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
    if((mode == CW) && (!(semi_qsk_timeout))) {
#ifdef SEMI_QSK
#ifdef KEYER
      semi_qsk_timeout = millis() + ditTime * 8;
#else
      semi_qsk_timeout = millis() + 8 * 8; // no keyer? assume dit-time of 20 WPM
#endif //KEYER
#endif //SEMI_QSK
      if(semi_qsk)
        func_ptr = dummy;
      else
        func_ptr = sdr_rx_00;
    } else {
      centiGain = _centiGain; // restore AGC setting (legacy 3693)
#ifdef SEMI_QSK
      semi_qsk_timeout = 0;
#endif
      func_ptr = sdr_rx_00;
    }
  }
  interrupts();
  if(tx_enable)
    ADMUX = admux[2]; // mic input for TX DSP (legacy 3702)
  else
    _init = 1; // reset RX accumulators after TX (legacy 3703)
  rx_state = 0;
#ifdef CW_DECODER
  if((cwdec) && (mode == CW)) {
    filteredstate = tx_enable;
    dec2(); // feed CW decoder with actual key state (legacy 3706)
  }
#endif //CW_DECODER

  if(tx_enable) { // tx
    if(practice) {
      digitalWrite(RX, LOW); // TX (disable RX)
      lcd.setCursor(15, 1);
      lcd.print('P');
      si5351.SendRegister(SI_CLK_OE, TX0RX0);
      // Do not enable PWM (KEY_OUT), do not enable CLK2 (legacy 3714)
    } else {
      digitalWrite(RX, LOW); // TX (disable RX)
#ifdef PTX
      digitalWrite(PTX, HIGH); // TX (enable TX)
#endif
      lcd.setCursor(15, 1);
      lcd.print('T');
      if(mode == CW) { // for CW, TX at carrier (legacy 3725)
        si5351.freq_calc_fast(-cw_offset);
        si5351.SendPLLRegisterBulk();
      }
#ifdef RIT_ENABLE
      else if(rit) {
        si5351.freq_calc_fast(0);
        si5351.SendPLLRegisterBulk();
      }
#endif //RIT_ENABLE
      si5351.SendRegister(SI_CLK_OE, TX1RX0);
      OCR1AL = 0x80; // make sure SIDETONE is set at 2.5V
      if((!mox) && (mode != CW))
        TCCR1A &= ~(1 << COM1A1); // disable SIDETONE, prevent interference during SSB TX
      TCCR1A |= (1 << COM1B1);    // enable KEY_OUT PWM
#ifdef _SERIAL
      if(cat_active) {
        DDRC &= ~(1 << 2); // disable PC2, so that ADC2 can be used as mic input
      }
#endif
    }
  } else { // rx
#ifdef KEY_CLICK
    if(OCR1BL != 0) {
      for(uint16_t i = 0; i != 31; i++) { // ramp down of amplitude: soft falling edge to prevent key clicks
        OCR1BL = lut[pgm_read_byte_near(&ramp[i])];
        delayMicroseconds(60);
      }
    }
#endif //KEY_CLICK
    TCCR1A |= (1 << COM1A1);                          // enable SIDETONE (was disabled to prevent interference during ssb tx)
    TCCR1A &= ~(1 << COM1B1);
    digitalWrite(KEY_OUT, LOW);                       // disable KEY_OUT PWM, prevents interference during RX
    OCR1BL = 0;                                       // make sure PWM (KEY_OUT) is set to 0%
    si5351.SendRegister(SI_CLK_OE, TX0RX1);
#ifdef SEMI_QSK
    if((!semi_qsk_timeout) || (!semi_qsk)) // enable RX when no longer in semi-qsk phase
#endif //SEMI_QSK
    {
      digitalWrite(RX, !(att == 2)); // RX (enable RX when attenuator not on)
#ifdef PTX
      digitalWrite(PTX, LOW); // TX (disable TX)
#endif
    }
#ifdef RIT_ENABLE
    si5351.freq_calc_fast(rit);
    si5351.SendPLLRegisterBulk(); // restore original PLL RX frequency
#else
    si5351.freq_calc_fast(0);
    si5351.SendPLLRegisterBulk(); // restore original PLL RX frequency
#endif //RIT_ENABLE
    lcd.setCursor(15, 1);
    lcd.print((vox) ? 'V' : 'R');
#ifdef _SERIAL
    if(!vox)
      if(cat_active) {
        DDRC |= (1 << 2); // enable PC2, so that ADC2 is pulled-down so that CAT TX is not disrupted via mic input
      }
#endif
  }
  OCR2A = ((F_CPU / 64) / ((tx_enable) ? F_SAMP_TX : F_SAMP_RX)) - 1;
  TIMSK2 |= (1 << OCIE2A); // enable timer compare interrupt TIMER2_COMPA_vect
}
