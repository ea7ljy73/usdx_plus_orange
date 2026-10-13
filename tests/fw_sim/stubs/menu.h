// Stub menu.h para cat.h: solo el estado del menu y los ganchos de UI/VFO.
#pragma once
#include <stdint.h>
#define MENU_MAIN 0
#define MENU_SELECT 1
struct MenuStub { uint8_t state = MENU_MAIN; };
extern MenuStub menu;
void display_vfo(void);
void display_vfo_line1(void);
void vfo_save_current(void);
void set_lpf(uint8_t f);
void bandval_align(void);
