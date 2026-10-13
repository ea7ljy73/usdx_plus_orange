// Stub Arduino.h for host simulation of menu.h (no hardware).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define INPUT_PULLUP 1
#define OUTPUT 2

// ---- simulated time / pins (driven by the test) ----
extern uint32_t sim_millis;
extern uint8_t  sim_buttons_digital; // digitalRead(BUTTONS)
static inline uint32_t millis(void) { return sim_millis; }
static inline uint8_t  digitalRead(uint8_t) { return sim_buttons_digital; }
static inline void     pinMode(uint8_t, uint8_t) {}
static inline void     delay(unsigned long ms) { sim_millis += ms; }
static inline void     delayMicroseconds(unsigned int) {}
static inline void     noInterrupts(void) {}
static inline void     interrupts(void) {}
static inline void     wdt_reset(void) {}

struct __FlashStringHelper {};
#define F(s) ((const char*)(s))

// min/max like Arduino
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

static inline char* ltoa(long v, char* b, int) {
  snprintf(b, 12, "%ld", v);
  return b;
}
