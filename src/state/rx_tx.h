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
void start_rx();
void switch_rxtx(uint8_t tx_enable);

extern uint8_t rx_state;
extern void (*func_ptr)();
extern uint8_t admux[3];

#endif
