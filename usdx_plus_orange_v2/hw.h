// hw.h - uSDX Plus Orange v2
// Hardware: pins, ADC, timers (PWM + sample-rate ISR), ISR dispatch.
// Extracted from v1 initPins()/adc_start()/timer1_start()/switch_rxtx().

#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <stdint.h>

#include "rx.h"
#include "si5351.h"
#include "tx.h"
#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Function pointer ISR dispatch (TX/RX DSP bodies run here)
// ---------------------------------------------------------------------------
typedef void (*func_t)(void);
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
  TCCR2A = 0;
  TCCR2B = 0;
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
  pinMode(BUTTONS, INPUT);
  pinMode(DIT, INPUT_PULLUP);
  pinMode(DAH, INPUT);

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

  if(tx_enable) {
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
    digitalWrite(RX, LOW);   // disable RX
    digitalWrite(PTX, HIGH); // enable TX
    TIMSK2 |= (1 << OCIE2A); // enable timer ISR
    TCCR1A |= (1 << COM1A1); // enable SIDETONE PWM
    return;
  }

  // RX
  TCCR1A &= ~(1 << COM1A1); // disable SIDETONE PWM
  TCCR1A &= ~(1 << COM1B1); // disable KEY_OUT PWM
  digitalWrite(KEY_OUT, LOW);
  OCR1BL = 0;
  si5351.SendRegister(SI_CLK_OE, TX0RX1);
  digitalWrite(RX, !(att == 2)); // RX (enable unless ATT full)
  digitalWrite(PTX, LOW);        // disable TX
  si5351.freq_calc_fast(0);
  si5351.SendPLLRegisterBulk(); // restore RX frequency
  func_ptr = sdr_rx_00;         // RX ISR start phase
  rx_state = 0;
  TIMSK2 |= (1 << OCIE2A);
}