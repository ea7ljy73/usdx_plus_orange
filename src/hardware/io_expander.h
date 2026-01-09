#ifndef USDX_HARDWARE_IO_EXPANDER_H
#define USDX_HARDWARE_IO_EXPANDER_H

#include <Arduino.h>

void set_lpf(uint8_t f);

// Functions for LPF switching logic that might be used internally or by setup
// Since we hide implementation, we only expose set_lpf which is the main API.
// set_latch was found to be used only internally in previous grep.

#endif // USDX_HARDWARE_IO_EXPANDER_H
