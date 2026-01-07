#ifndef BANDS_H
#define BANDS_H

#include <Arduino.h>
#include "usdx_config.h"

extern const uint32_t stepsizes[N_STEPSIZES];
extern uint8_t step_index;

void band_next();
void band_prev();
uint8_t band_get_index(uint32_t freq);
uint32_t band_get_center(uint8_t band_index);

#endif
