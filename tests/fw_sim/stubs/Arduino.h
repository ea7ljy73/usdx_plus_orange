// Stub Arduino.h para simulacion host de cat.h / cw.h / UI (sin hardware).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t byte;
#define HIGH 1
#define LOW 0
#define DEC 10
#define PROGMEM
#define pgm_read_byte_near(p) (*(const uint8_t*)(p))
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define F(s) (s)
struct __FlashStringHelper {};

// ---- tiempo y pines simulados (los mueve el test) ----
extern uint32_t sim_millis;
extern uint8_t  sim_pin[32];              // nivel por pin (digitalRead)
extern void     sim_on_ms(void);          // gancho: llamado por cada ms que avanza
static inline uint32_t millis(void) { return sim_millis; }
static inline uint8_t  digitalRead(uint8_t p) { return sim_pin[p & 31]; }
static inline void     wdt_reset(void) {
  sim_millis++; // los bucles de espera del firmware (millis() < t) avanzan aqui
  sim_on_ms();
}
static inline void delay(unsigned long ms) {
  while(ms--) {
    sim_millis++;
    sim_on_ms();
  }
}
static inline void noInterrupts(void) {}
static inline void interrupts(void) {}
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

// ---- Serial grabador (CAT) ----
struct SerialStub {
  char out[512];
  char in[128];
  int  in_r, in_w;
  void reset() { out[0] = 0; in_r = in_w = 0; }
  void feed(const char* s) { for(; *s; s++) in[in_w++] = *s; }
  int  available() { return in_w - in_r; }
  char read() { return in[in_r++]; }
  void acc(const char* s) { strncat(out, s, sizeof(out) - strlen(out) - 1); }
  void print(char c) { char b[2] = {c, 0}; acc(b); }
  void print(const char* s) { acc(s); }
  void print(uint8_t v, int) { char b[8]; snprintf(b, sizeof b, "%u", v); acc(b); }
  void print(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); acc(b); }
};
extern SerialStub Serial;

// ---- LCD 16x2 con posicion real de cursor ----
struct LcdStub {
  char row[2][17];
  int  x, y;
  void clear() { memset(row[0], ' ', 16); memset(row[1], ' ', 16); row[0][16] = row[1][16] = 0; x = y = 0; }
  void setCursor(int cx, int cy) { x = cx; y = cy; }
  void put(char c) { if(x < 16 && y < 2) row[y][x] = c; x++; }
  void print(char c) { put(c); }
  void print(const char* s) { for(; *s; s++) put(*s); }
  void print(int v) { char b[16]; snprintf(b, sizeof b, "%d", v); print(b); }
  void cursor() {}
  void noCursor() {}
};
extern LcdStub lcd;
