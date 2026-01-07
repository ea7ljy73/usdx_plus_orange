#ifndef CW_KEYER_H
#define CW_KEYER_H

#include <Arduino.h>
#include "usdx_config.h"

void update_PaddleLatch();
void loadWPM(int wpm);
void keyer();
uint8_t getKeyerState();

#endif // CW_KEYER_H
