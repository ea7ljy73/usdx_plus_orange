// Stub display.h for host menu simulation: LCD recorder + encoder/button fakes.
#pragma once
#include "Arduino.h"

#define BUTTONS 17
#define BUTTONS_ADC ((BUTTONS == 17) ? 3 : (BUTTONS & 0x0f))
#define ROT_A 6
#define ROT_B 7

volatile int16_t encoder_val = 0;
volatile uint8_t encoder_pressed = 0;
volatile uint8_t last_state = 0;

// scripted ADC value returned by analogSafeRead + call counter
extern uint16_t sim_buttons_adc;
extern int      sim_analog_reads;

static inline uint16_t analogSafeRead(uint8_t) {
  sim_analog_reads++;
  return sim_buttons_adc;
}

// LCD call recorder
struct LcdStub {
  char   line[2][64];
  int    renders;
  void   clear_lines() {
    line[0][0] = 0;
    line[1][0] = 0;
  }
  void setCursor(int x, int y) {
    (void)x;
    (void)y;
  }
  void acc(const char* s) {
    strncat(line[1], s, sizeof(line[1]) - strlen(line[1]) - 1);
  }
  size_t print(char c) {
    char b[2] = {c, 0};
    acc(b);
    return 1;
  }
  size_t print(int v) {
    char b[16];
    snprintf(b, sizeof b, "%d", v);
    acc(b);
    return 1;
  }
  size_t print(long v) {
    char b[16];
    snprintf(b, sizeof b, "%ld", v);
    acc(b);
    return 1;
  }
  size_t print(const char* s) {
    acc(s ? s : "?");
    return 1;
  }
  size_t print(const __FlashStringHelper* s) {
    acc((const char*)s);
    return 1;
  }
  void cursor() {}
  void noCursor() {}
  void noDisplay() {}
  void write(uint8_t v) {
    char b[8];
    snprintf(b, sizeof b, "<%u>", v);
    acc(b);
  }
};
extern LcdStub lcd;
