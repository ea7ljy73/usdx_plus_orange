// Stub avr/pgmspace.h for host simulation.
#pragma once
#include <string.h>
#define PROGMEM
#define PGM_P const char*
static inline void* memcpy_P(void* d, PGM_P s, size_t n) { return memcpy(d, s, n); }
#define pgm_read_ptr(p) (*(p))
#define pgm_read_byte(p) (*(const unsigned char*)(p))
#define pgm_read_word(p) (*(const unsigned short*)(p))
#define pgm_read_dword(p) (*(const unsigned long*)(p))
