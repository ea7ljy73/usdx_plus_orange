/**
 * @file adc.cpp
 * @brief Implementación ADC para uSDX
 *
 * Implementación optimizada para bajo ruido y alta precisión.
 * El ADC del ATmega328P tiene mejor SNR con prescaler de 32-64
 * y referencia interna de 1.1V para señales de micrófono.
 */

#include "adc.h"

// Tabla de mapeo de pines a canales ADC
const uint8_t adc_pin_to_channel[] PROGMEM = {
  0x00,  // A0 -> ADC0
  0x01,  // A1 -> ADC1
  0x02,  // A2 -> ADC2
  0x03,  // A3 -> ADC3
  0x04,  // A4 -> ADC4
  0x05,  // A5 -> ADC5
  0x06,  // A6 -> ADC6
  0x07,  // A7 -> ADC7
};

// Canales ADC usados en el sistema
#define ADMUX_MIC   0  // PC0/A0 - Micrófono
#define ADMUX_AUD2  1  // PC1/A1 - Audio I/Q
#define ADMUX_DVM   2  // PC2/A2 - DVM

void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs) {
  (void)fs;
  uint8_t channel;

  // Mapear pin a canal
  if (adcpin >= A0 && adcpin <= A7) {
    channel = pgm_read_byte(&adc_pin_to_channel[adcpin - A0]);
  } else {
    channel = adcpin & 0x07;
  }

  // Deshabilitar ADC temporalmente para configurar
  ADCSRA = 0;

  // Primera conversión con referencia deseada (requerido por datasheet)
#if defined(ADMUX)
  if (ref1v1) {
    ADMUX = _BV(REFS1) | _BV(REFS0) | channel;  // 1.1V ref, noise reduction
  } else {
    ADMUX = _BV(REFS0) | channel;  // VCC ref (5V)
  }
#endif

  // Esperar a que la referencia se estabilice (~10ms mínimo según datasheet)
  // Nota: En ISR esto se hace con la primera conversión
  delayMicroseconds(100);

  // Configurar prescaler optimizado (32 para mejor SNR a 27MHz)
  // Esto da F_ADC = 27MHz/32 = 843.75 kHz
  // Con oversampling efectivo, esto es óptimo para audio
  adc_set_prescaler(ADC_PRESCALER);

  // Habilitar ADC en modo free-running con interrupción
  // ADATE: Auto-trigger enable
  // ADIE: Interrupt enable
  // ADSC: Start conversion
#if defined(ADCSRA)
  ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADATE) | _BV(ADIE) | (ADC_PRESCALER & 0x07);
#endif

  // Trigger source: Timer1 compare match B para sincronización
#if defined(ADCSRB)
  ADCSRB = _BV(ADTS2) | _BV(ADTS1) | _BV(ADTS0);
  ADCSRA |= _BV(ADATE);
#endif

  // Primera conversión Dummy para estabilizar
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC));  // Esperar a que termine
  (void)ADC;  // Descartar primer resultado
}

void adc_stop(void) {
#if defined(ADCSRA)
  ADCSRA &= ~(_BV(ADIE) | _BV(ADATE) | _BV(ADSC));
  ADCSRA &= ~_BV(ADEN);
#endif
}
