/**
 * @file gpio.h
 * @brief GPIO Abstraction Layer para uSDX
 *
 * Proporciona funciones para control de pines digitales
 * con soporte para inversión configurable.
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <Arduino.h>

// ============================================================================
// CONFIGURACIÓN DE INVERSIÓN
// ============================================================================

#ifdef ONEBUTTON_INV
#define BUTTON_INV 1
#else
#define BUTTON_INV 0
#endif

// ============================================================================
// DECLARACIONES DE FUNCIONES
// ============================================================================

/**
 * @brief Inicializa un pin como entrada con pull-up
 * @param pin Número de pin Arduino
 */
inline void gpio_input_pullup(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
}

/**
 * @brief Inicializa un pin como salida
 * @param pin Número de pin Arduino
 */
inline void gpio_output(uint8_t pin) {
  pinMode(pin, OUTPUT);
}

/**
 * @brief Lee un pin digital (con soporte CAT override)
 * @param pin Número de pin Arduino
 * @return Estado del pin (HIGH/LOW)
 */
uint8_t gpio_read(uint8_t pin);

/**
 * @brief Lee un pin digital directo (sin override CAT)
 * @param pin Número de pin Arduino
 * @return Estado del pin (HIGH/LOW)
 */
inline uint8_t gpio_read_direct(uint8_t pin) {
  return digitalRead(pin);
}

/**
 * @brief Escribe un valor en un pin digital
 * @param pin Número de pin Arduino
 * @param value Valor a escribir (HIGH/LOW)
 */
inline void gpio_write(uint8_t pin, uint8_t value) {
  digitalWrite(pin, value);
}

/**
 * @brief Lee el estado de un botón (con inversión configurable)
 * @param pin Número de pin del botón
 * @return Estado del botón (0 = pulsado, 1 = libre con pull-up)
 */
inline uint8_t gpio_read_button(uint8_t pin) {
  uint8_t state = digitalRead(pin);
#if BUTTON_INV
  return state ^ 1;
#else
  return state;
#endif
}

// ============================================================================
// DEFINICIONES DE PINES UTILIZADOS
// ============================================================================

#define GPIO_ROT_A       ROT_A
#define GPIO_ROT_B       ROT_B
#define GPIO_BUTTONS     BUTTONS
#define GPIO_RX          RX
#define GPIO_SIDETONE    SIDETONE
#define GPIO_KEY_OUT     KEY_OUT
#define GPIO_DAH         DAH
#define GPIO_DIT         DIT
#define GPIO_LCD_EN      LCD_EN
#define GPIO_LCD_RS      LCD_RS

#endif // HAL_GPIO_H
