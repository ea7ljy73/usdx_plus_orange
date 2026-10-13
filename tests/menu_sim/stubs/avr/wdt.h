// Stub avr/wdt.h for host simulation.
#pragma once
static inline void wdt_enable(int) {}
static inline void wdt_disable(void) {}
#define WDTO_4S 0
