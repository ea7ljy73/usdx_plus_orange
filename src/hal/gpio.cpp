/**
 * @file gpio.cpp
 * @brief Implementación GPIO para uSDX
 */

#include "gpio.h"

// Variable externa para override CAT (declarada en usdx_plus_orange.ino)
extern volatile uint8_t cat_key;

// ============================================================================
// IMPLEMENTACIÓN
// ============================================================================

uint8_t gpio_read(uint8_t pin) {
#ifdef CAT_EXT
  // CAT puede override pins DIT, DAH, BUTTONS
  if (cat_key) {
    if (pin == BUTTONS) {
      return (cat_key & 0x07) > 0;
    }
    if (pin == DIT) {
      return ~cat_key & 0x10;
    }
    if (pin == DAH) {
      return ~cat_key & 0x20;
    }
    return 0;
  }
#endif
  return digitalRead(pin);
}
