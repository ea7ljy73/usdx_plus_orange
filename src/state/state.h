#ifndef STATE_H
#define STATE_H

#include <Arduino.h>
#include "usdx_config.h"

void state_init();
void state_update();
void state_save_all();
void state_load_all();
void state_reset();

void bandswitch(uint8_t band);
void mode_switch(uint8_t _mode);
void vfo_update();
void frequency_update(int32_t _f);

extern volatile uint8_t _init;

#endif
