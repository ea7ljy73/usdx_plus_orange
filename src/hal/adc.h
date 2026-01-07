/**
 * @file adc.h
 * @brief ADC Abstraction Layer para uSDX
 *
 * Proporciona funciones para lectura del ADC optimizado para baja temperatura
 * y bajo ruido. El ADC del ATmega328P funciona mejor con prescaler de 32 o 64
 * para mantener el reloj del ADC entre 50-200 kHz (óptimo para SNR).
 */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#include <Arduino.h>

// ============================================================================
// CONSTANTES - OPTIMIZADO PARA BAJO RUIDO
// ============================================================================

// Frecuencia de muestreo objetivo para audio (DSP)
// Con prescaler 32 y F_CPU=27MHz: F_ADC = 843.75 kHz
// Con oversampling 4x: F_effectiva ≈ 210 kHz / 4 = 52 kHz
#define F_ADC_CONV 843750

// Prescaler del ADC - OPTIMIZADO
// Valores válidos: 2, 4, 8, 16, 32, 64, 128
// Para 27MHz: prescaler 32 -> F_ADC = 843.75 kHz (óptimo)
// Prescaler 64 -> F_ADC = 421.875 kHz (más conservativo, menos ruido)
#define ADC_PRESCALER 32

// Para referencia de 1.1V interna, mejor para señales de micrófono
// que típicamente son menores a 100mVpp
#define ADC_REF_1V1 1.1
#define ADC_REF_VCC 5.0

// Oversampling para mejor resolución
#define ADC_OVERSAMPLE 4

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
