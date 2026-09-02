// hw.h - uSDX Plus Orange v2
// Hardware: pins, ADC, timers (PWM + sample-rate ISR), ISR dispatch.
// Extracted from v1 initPins()/adc_start()/timer1_start()/switch_rxtx().

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
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
  OCR2A = ((F_CPU / 64) / fs) - 1;
  TIMSK2 |= (1 << OCIE2A);
}

// ---------------------------------------------------------------------------
// Microphone sampling for VOX detection
// ---------------------------------------------------------------------------
uint16_t analogSampleMic() {
  uint16_t adc;
  noInterrupts();
  ADCSRA         = (1 << ADEN) | (((uint8_t)log2((uint8_t)(F_CPU / 13 / (192307 / 1)))) & 0x07);
  uint8_t oldmux = ADMUX;
  for(; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = admux[2];
  ADCSRA |= (1 << ADSC);
  for(; !(ADCSRA & (1 << ADIF));)
    ;
  ADMUX = oldmux;
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

  digitalWrite(PTX, LOW);
  pinMode(PTX, OUTPUT);

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

void switch_rxtx(uint8_t tx_enable) {
  TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
  delayMicroseconds(20);    // allow RX ISR to finalize
  noInterrupts();

  if(txdelay && tx_enable && (!tx)) { // key-up TX relay in advance
    digitalWrite(RX, LOW);            // disable RX
    digitalWrite(PTX, HIGH);          // enable TX
    interrupts();
    delay(txdelay);
    noInterrupts();
  }
  tx = tx_enable;
  cw_set_keyed(tx_enable); // feed CW decoder with actual key state (RX side)

  // Set sample rate for this direction: TX @F_SAMP_TX (4.8k), RX @F_SAMP_RX
  OCR2A = ((F_CPU / 64) / ((tx_enable) ? F_SAMP_TX : F_SAMP_RX)) - 1;

  if(tx_enable) {
    // enable KEY_OUT PWM early while PLL settles
    TCCR1A |= (1 << COM1B1); // KEY_OUT PWM (PA amplitude signal)
    if(practice) {
      digitalWrite(RX, LOW);                  // TX (disable RX)
      si5351.SendRegister(SI_CLK_OE, TX0RX0); // RF disabled (practice)
    } else {
      digitalWrite(RX, LOW);   // TX (disable RX)
      digitalWrite(PTX, HIGH); // TX (enable TX)
      if(mode == CW) {
        si5351.freq_calc_fast(-CW_OFFSET); // TX at carrier for CW
        si5351.SendPLLRegisterBulk();
      } else {
        si5351.freq_calc_fast(0); // restore base freq (undo RIT offset)
        si5351.SendPLLRegisterBulk();
      }
      si5351.SendRegister(SI_CLK_OE, TX1RX0); // enable RF output on CLK0
    }
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
    default:
      func_ptr = dsp_tx_fm;
      break;
    }
    OCR1AL = 0x80; // SIDETONE at 2.5V
    if(mode != CW)
      TCCR1A &= ~(1 << COM1A1); // disable SIDETONE PWM (SSB TX interference)
    else
      TCCR1A |= (1 << COM1A1); // CW needs SIDETONE
    TIMSK2 |= (1 << OCIE2A);   // enable timer ISR
    return;
  }

  // RX
#ifdef KEY_CLICK
  if(OCR1BL != 0) { // ramp down amplitude to prevent key clicks
    for(uint16_t i = 0; i != 31; i++) {
      OCR1BL = lut[pgm_read_byte_near(&ramp[i])];
      delayMicroseconds(60);
    }
  }
#endif
  TCCR1A |= (1 << COM1A1);  // enable SIDETONE
  TCCR1A &= ~(1 << COM1B1); // disable KEY_OUT PWM (prevents RX interference)
  digitalWrite(KEY_OUT, LOW);
  OCR1BL = 0;
  si5351.SendRegister(SI_CLK_OE, TX0RX1);
  digitalWrite(RX, !(att == 2)); // RX (enable unless ATT full)
  digitalWrite(PTX, LOW);        // disable TX
  si5351.freq_calc_fast(0);      // restore RX base frequency
  si5351.SendPLLRegisterBulk();  // restore PLL RX frequency
  func_ptr = sdr_rx_00;          // RX ISR start phase
  rx_state = 0;
  TIMSK2 |= (1 << OCIE2A);
}