#ifndef RX_TX_H
#define RX_TX_H

#include <Arduino.h>
#include "usdx_config.h"

void rx_tx_init();
void rx_enable();
void tx_enable();
void tx_disable();
void toggle_tx();
bool is_tx();

#endif
