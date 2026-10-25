// Stub menu.h para cat.h: estado del menu, ganchos de UI/VFO y el tipo de la
// tabla MENU[] (la tabla REAL se extrae del .ino con gen_cat_menu.py).
#pragma once
#include <stdint.h>
#include <string.h>
#define MENU_MAIN 0
#define MENU_SELECT 1
#define LSB 0
#define USB 1
#define CW 2
#define PGM_P const void*
#define memcpy_P(d, s, n) memcpy((d), (s), (n))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
struct MenuStub { uint8_t state = MENU_MAIN; };
extern MenuStub menu;
enum param_type_t { P_T8S, P_T8, P_T16, P_T32, P_ENUM, P_TEXT };
struct MenuParam {
  uint8_t            label;
  void*              value;
  uint8_t            type;
  int32_t            min, max;
  const char* const* enum_labels;
  uint8_t            eslot;
  void (*on_change)();
};
extern const MenuParam MENU[];
extern const int8_t    MENU_COUNT;
void menu_eeprom_save(uint8_t eslot, const void* ptr, uint8_t size);
void save_menu_eslot(uint8_t eslot);
void display_vfo(void);
void display_vfo_line1(void);
void vfo_save_current(void);
void set_lpf(uint8_t f);
void bandval_align(void);
