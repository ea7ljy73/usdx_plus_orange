#ifndef USDX_UTILS_H
#define USDX_UTILS_H

#include "usdx_config.h"
#include <Arduino.h>

extern volatile uint8_t cat_key;

uint8_t _digitalRead(uint8_t pin);
int freeMemory();

#endif // USDX_UTILS_H
