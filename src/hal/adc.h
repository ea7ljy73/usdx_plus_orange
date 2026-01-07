/**
 * @file adc.h
 * @brief ADC Abstraction Layer para uSDX
 *
 * Proporciona funciones para lectura del ADC con prescaler configurable.
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <Arduino.h>

// ============================================================================
// CONSTANTES
// ============================================================================

// Frecuencia de muestreo objetivo
#define F_ADC_CONV 125000  // 125 kHz para conversión rápida

// Prescaler del ADC
// PS=16 -> Fs=62.5kHz con F_CPU=20MHz
#define ADC_PRESCALER 4

// Referencias de voltaje
#define ADC_REF_1V1 1.1
#define ADC_REF_VCC 5.0

// ============================================================================
// DECLARACIONES DE FUNCIONES
// ============================================================================

/**
 * @brief Inicia una conversión ADC en un pin específico
 * @param adcpin Pin analógico a leer
 * @param ref1v1 Usar referencia de 1.1V si true, VCC si false
 * @param fs Frecuencia de muestreo deseada
 */
void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs);

/**
 * @brief Detiene el ADC
 */
void adc_stop(void);

/**
 * @brief Lee el valor actual del ADC (ISR context)
 * @return Valor del ADC (0-1023)
 */
inline int16_t adc_read(void) {
  return ADC;
}

/**
 * @brief Lee un pin analógico directamente
 * @param pin Pin analógico
 * @return Valor leído (0-1023)
 */
inline uint16_t adc_read_pin(uint8_t pin) {
  return analogRead(pin);
}

/**
 * @brief Configura el prescaler del ADC
 * @param prescaler Valor del prescaler (divisor)
 */
inline void adc_set_prescaler(uint8_t prescaler) {
#if defined(ADCSRA)
  ADCSRA = (ADCSRA & 0xF8) | (prescaler & 0x07);
#endif
}

/**
 * @brief Habilita las interrupciones del ADC
 */
inline void adc_enable_irq(void) {
#if defined(ADCSRA)
  ADCSRA |= (1 << ADIE);
#endif
}

/**
 * @brief Deshabilita las interrupciones del ADC
 */
inline void adc_disable_irq(void) {
#if defined(ADCSRA)
  ADCSRA &= ~(1 << ADIE);
#endif
}

#endif // HAL_ADC_H
