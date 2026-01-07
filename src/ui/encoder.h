#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include "usdx_config.h"

void encoder_init();
int8_t encoder_read();
void encoder_reset();
bool encoder_button_pressed();
void encoder_process();

#endif
