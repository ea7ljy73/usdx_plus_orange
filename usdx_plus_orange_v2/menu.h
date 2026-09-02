// menu.h - uSDX Plus Orange v2
// Declarative parameter menu (replaces the legacy 49-case switch + NEXT_MENU
// probing + inline post-handling in loop). Same user-visible behavior.
//
// Design: a single table MENU[] describes every parameter (label, target var,
// type, range/enum, EEPROM slot, optional on_change callback). The menu
// machine navigates/edits/saves uniformly; effects live next to their param.
// Encoder/button state is read from the display module globals (same as v1).

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#include "display.h"
#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Menu states (identical to legacy menumode values)
// ---------------------------------------------------------------------------
#define MENU_MAIN 0      // not in menu (default screen)
#define MENU_SELECT 1    // navigating parameters
#define MENU_EDIT 2      // editing parameter value
#define MENU_EDIT_TEXT 3 // editing a text value

// Parameter types
enum param_type_t { P_T8, P_T16, P_T32, P_ENUM, P_TEXT };

// Program labels: stored in a single PROGMEM table (indexed by id) to save RAM.
// Defined in the .ino as an array of PSTR pointers named MENU_LABELS.
extern const char* const MENU_LABELS[];

struct MenuParam {
  uint8_t            label;       // index into MENU_LABELS (PROGMEM)
  void*              value;       // pointer to the target variable
  uint8_t            type;        // param_type_t
  int32_t            min, max;    // numeric range; enums use index
  const char* const* enum_labels; // PROGMEM pointer array (P_ENUM only)
  uint8_t            eslot;       // eeprom slot, 0 = not persisted
  void (*on_change)();            // post-handling callback
};

// helper: write a flash label to the LCD via its id (defined in .ino)
void menu_print_label(uint8_t id);

#define N_MENU_ITEMS 32 // declared capacity; MENU_COUNT computed from table

// ---------------------------------------------------------------------------
// Forward declarations (table + eeprom helpers live in menu.cpp / .ino)
// ---------------------------------------------------------------------------
extern const MenuParam MENU[]; // defined in the .ino below
#define _N(a) ((sizeof(a) / sizeof((a)[0])))

// Item count: must match the table size in the .ino (updated when params change)
extern const int8_t MENU_COUNT;

// eeprom helpers (implemented in eeprom.h / .ino)
void menu_eeprom_load(uint8_t eslot, void* ptr, uint8_t size);
void menu_eeprom_save(uint8_t eslot, const void* ptr, uint8_t size);

// ---------------------------------------------------------------------------
// Menu machine
// ---------------------------------------------------------------------------
class Menu {
public:
  uint8_t state = MENU_MAIN;
  int8_t  index = 0;

  void begin() {
    state = MENU_MAIN;
    index = 0;
  }

  // Processing: should be called from loop(). Reads encoder_val / buttons.
  void process();

  // Render current state on LCD (label + value)
  void render();

  // Fetch current table entry (PROGMEM) into a local struct
  void get_cur(MenuParam& p) { memcpy_P(&p, (PGM_P)&MENU[index], sizeof(MenuParam)); }

private:
  void move(int8_t delta);
  void select_mode();
  void edit_value(int32_t delta);
  void commit();
  void print_value();
};

extern Menu menu;

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------
inline void Menu::move(int8_t delta) {
  index += delta;
  if(index < 0)
    index = MENU_COUNT - 1;
  if(index >= MENU_COUNT)
    index = 0;
}

inline void Menu::commit() {
  MenuParam p;
  get_cur(p);
  if(p.eslot && p.value) {
    uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : 1;
    menu_eeprom_save(p.eslot, p.value, sz);
  }
  if(p.on_change)
    p.on_change();
}

inline void Menu::print_value() {
  MenuParam p;
  get_cur(p);
  if(!p.value)
    return;
  lcd.print(' ');
  if(p.type == P_ENUM) {
    uint8_t v = *(uint8_t*)p.value;
    if(p.enum_labels && v <= p.max)
      lcd.print((const char*)pgm_read_ptr(&p.enum_labels[v - p.min]));
  } else if(p.type == P_TEXT) {
    lcd.print((char*)p.value);
  } else {
    int32_t v = 0;
    switch(p.type) {
    case P_T8:
      v = *(int8_t*)p.value;
      break;
    case P_T16:
      v = *(int16_t*)p.value;
      break;
    default:
      v = *(int32_t*)p.value;
      break;
    }
    char b[12];
    ltoa(v, b, 10);
    lcd.print(b);
  }
}

inline void Menu::select_mode() {
  switch(state) {
  case MENU_MAIN:
    state = MENU_SELECT;
    move(1);
    break;
  case MENU_SELECT:
    state = MENU_EDIT;
    break;
  case MENU_EDIT:
    commit();
    state = MENU_MAIN;
    break;
  default:
    state = MENU_MAIN;
    break;
  }
}

inline void Menu::edit_value(int32_t delta) {
  MenuParam p;
  get_cur(p);
  if(!p.value)
    return;
  if(p.type == P_ENUM || p.type == P_TEXT) {
    uint8_t v = *(uint8_t*)p.value;
    if(delta) {
      int32_t nv = (int32_t)v + delta;
      if(nv < p.min)
        nv = p.max;
      if(nv > p.max)
        nv = p.min;
      *(uint8_t*)p.value = (uint8_t)nv;
      if(p.on_change)
        p.on_change();
    }
  } else {
    int32_t v = 0;
    switch(p.type) {
    case P_T8:
      v = *(int8_t*)p.value;
      break;
    case P_T16:
      v = *(int16_t*)p.value;
      break;
    default:
      v = *(int32_t*)p.value;
      break;
    }
    if(delta) {
      v += delta;
      if(v < p.min)
        v = p.min;
      if(v > p.max)
        v = p.max;
      switch(p.type) {
      case P_T8:
        *(int8_t*)p.value = (int8_t)v;
        break;
      case P_T16:
        *(int16_t*)p.value = (int16_t)v;
        break;
      default:
        *(int32_t*)p.value = v;
        break;
      }
      if(p.on_change)
        p.on_change();
    }
  }
}

inline void Menu::process() {
  // --- encoder ---
  int32_t enc = encoder_val;
  if(enc) {
    encoder_val = 0;
    if(state == MENU_SELECT)
      move(enc);
    else if(state == MENU_EDIT)
      edit_value(enc);
    if(state != MENU_MAIN)
      render();
    return;
  }
  // --- button (short press state machine) ---
  static uint8_t btn_last = 0;
  uint8_t        btn      = !digitalRead(BUTTONS);
  if(btn && !btn_last) { // rising edge
    select_mode();
    if(state == MENU_EDIT)
      lcd.cursor();
    else
      lcd.noCursor();
    if(state != MENU_MAIN)
      render();
  }
  btn_last = btn;
}

inline void Menu::render() {
  MenuParam p;
  get_cur(p);
  lcd.setCursor(0, 0);
  menu_print_label(p.label);
  lcd.print("               ");
  lcd.setCursor(0, 1);
  if(state == MENU_EDIT)
    lcd.print('>');
  print_value();
  lcd.print("               ");
}