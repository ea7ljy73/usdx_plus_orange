#ifndef HAL_H
#define HAL_H

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

/**
 * @brief Configures and starts the ADC.
 * Sets reference, prescaler, and input channel.
 * @param adcpin Analog pin number (0-7).
 * @param ref1v1 True for 1.1V internal ref, False for 5V (AVCC).
 * @param fs Sampling rate in Hz.
 */
void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs);

void adc_stop();

/**
 * @brief Starts Timer 1 for PWM generation.
 * Controls KEY_OUT and SIDETONE.
 * @param fs PWM frequency in Hz.
 */
void timer1_start(uint32_t fs);

void timer1_stop();

/**
 * @brief Starts Timer 2 for Audio Sampling interrupts.
 * @param fs Sampling rate in Hz.
 */
void timer2_start(uint32_t fs);

void timer2_stop();

void lcd_blanks();

#define N_FONTS 8
extern const byte fonts[N_FONTS][8] PROGMEM;

#ifndef VSS_METER
int analogSafeRead(uint8_t pin, bool ref1v1 = false);
#else // VSS_METER
uint16_t analogSafeRead(uint8_t adcpin, bool ref1v1 = false);
#endif

#endif // HAL_H
