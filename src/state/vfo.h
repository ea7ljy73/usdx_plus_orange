#ifndef VFO_H
#define VFO_H

#include <Arduino.h>
#include "usdx_config.h"

void vfo_init();
void vfo_tune(int8_t direction);
void vfo_step_size(uint8_t step);
void vfo_swap();
void vfo_sel(uint8_t sel);
void vfo_update();
void rit_increment(int8_t delta);
void rit_clear();

#endif
