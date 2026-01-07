#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "usdx_config.h"

void display_init();
void display_update();
void display_vfo(int32_t f);
void display_freq(int32_t f);
void display_smeter(uint8_t s);
void display_swr(float swr);
void display_vss(float vss);
void display_clock();
void display_clear();
void display_showmsg(const char* msg);
void display_showvalue(const char* msg, int16_t value, const char* unit);

inline void display_blanks() {
  lcd.print(F("        "));
}

#endif
