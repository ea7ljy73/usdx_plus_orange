#ifndef SMETER_H
#define SMETER_H

#include <Arduino.h>
#include "usdx_config.h"

void smeter_init();
void smeter_update();
uint8_t smeter_read();
int16_t smeter_get_dbm();

#endif
