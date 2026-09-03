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
#include "si5351.h"
#include "usdx_settings.h"
#include "usdx_filter.h"
#include "vfo.h"

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
// Defined in the .ino as an array of PROGMEM-string pointers named MENU_LABELS.
extern const char* const MENU_LABELS[];

struct MenuParam {
  uint8_t            label;       // index into MENU_LABELS (PROGMEM)
  void*              value;       // pointer to the target variable
  uint8_t            type;        // param_type_t
  int32_t            min, max;    // numeric range; enums use index
  const void* const* enum_labels; // PROGMEM array of __FlashStringHelper* (P_ENUM)
  uint8_t            eslot;       // eeprom slot, 0 = not persisted
  void (*on_change)();            // post-handling callback
};

// helper: write a flash label to the LCD via its id (defined in .ino)
void menu_print_label(uint8_t id);

extern volatile uint8_t mode;                        // LSB/USB/CW/FM/AM (defined in main .ino)
void                    stepsize_change(int8_t val); // defined in main .ino
#define LSB 0
#define USB 1
#define CW 2
#define FM 3
#define AM 4

// legacy button polarity: inv=0 => pressed when BUTTONS reads HIGH (usdx-legazy:162)
uint8_t inv = 0;

// helpers wired from main .ino / vfo.h (used by the button engine)
extern void        vfo_apply(void);
extern void        vfo_save_current(void);
extern void        vfo_recall_band(int8_t b);
extern void        set_lpf(uint8_t f);
extern void        save_menu_eslot(uint8_t eslot);
extern void        show_banner(void);
extern void        powerDown(void);
extern uint8_t          prev_stepsize[2];
extern uint8_t          prev_filt[2];
extern volatile uint8_t vfosel;
extern volatile uint8_t cwdec;
extern uint8_t           vfomode[2];
extern volatile int32_t  freq;
extern volatile int32_t  rit;
extern volatile int8_t  volume;
extern volatile uint8_t  bandval;
extern volatile uint8_t  nr;
extern volatile uint8_t  filt;
extern volatile uint8_t  stepsize;
extern volatile uint8_t  _init;
extern int32_t           vfo[2];
extern SI5351            si5351;
#ifdef CW_MESSAGE
extern volatile uint32_t cw_msg_event;
extern volatile uint8_t  cw_msg_id;
extern char              cw_msg[1][48];
#endif

#define N_MENU_ITEMS 32 // declared capacity; MENU_COUNT computed from table
#define MENU_IDX_CWMSG 22 // index of the CQ Message entry (legacy CWMSG1)

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
  uint8_t text_pos = 0; // string edit cursor position (legacy 'pos')
  uint8_t text_len = 0; // string edit max length

  void begin() {
    state = MENU_MAIN;
    index = 0;
    text_pos = 0;
    text_len = 0;
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
  void edit_text(int32_t delta);
  void commit();
  void print_value();
};

extern Menu menu;

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------
inline void Menu::move(int8_t delta) { // legacy 5557-5563: clamp, no wrap
  index += delta;
  if(index < 0)
    index = 0;
  if(index >= MENU_COUNT)
    index = MENU_COUNT - 1;
}

inline void Menu::commit() {
  MenuParam p;
  get_cur(p);
  if(p.eslot && p.value) {
    uint8_t sz = (p.type == P_T16) ? 2 : (p.type == P_T32) ? 4 : (p.type == P_TEXT) ? 48 : 1;
    if(p.type == P_TEXT) { // trim trailing spaces (legacy 4070-4074)
      char* s = (char*)p.value;
      for(uint8_t i = sz; i > 0; i--) {
        if((s[i - 1] == ' ') || (s[i - 1] == 0))
          s[i - 1] = 0;
        else
          break;
      }
    }
    menu_eeprom_save(p.eslot, p.value, sz);
  }
  if(p.on_change)
    p.on_change();
  text_pos = 0;
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
      lcd.print((const __FlashStringHelper*)pgm_read_ptr(&((const void* const*)p.enum_labels)[v - p.min]));
  } else if(p.type == P_TEXT) {
    char* s = (char*)p.value;
    for(int i = 0; i != 13; i++) { // legacy 4064: 13-char window
      char ch = s[(text_pos / 8) * 8 + i];
      if(ch)
        lcd.print(ch);
      else
        break;
    }
    lcd.print('\x01'); // terminator glyph (legacy 4066)
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
    index = 0; // first param = Volume (legacy 5343)
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

inline void Menu::edit_text(int32_t delta) { // legacy 4048-4069 (MENU_STR)
  MenuParam p;
  get_cur(p);
  if(!p.value)
    return;
  char* s = (char*)p.value;
  text_len = 47;
  if(text_pos > text_len)
    text_pos = text_len;
  if(delta) {
    int8_t c = s[text_pos];
    if(c == 0)
      c = ' '; // edit past end -> start at space
    int nv = c + delta;
    if((nv < ' ') || (nv == 0))
      nv = ' ';
    else if(nv > 'Z')
      nv = 'Z';
    s[text_pos] = nv;
    if(s[text_pos + 1] == 0 && nv != ' ')
      s[text_pos + 1] = 0;
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
      if(v < p.min) // legacy 4024-4026: wrap (continuous)
        v = p.max;
      if(v > p.max)
        v = p.min;
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
    else if(state == MENU_EDIT_TEXT)
      edit_text(enc);
    if(state != MENU_MAIN)
      render();
    return;
  }
  // --- button event engine: faithful port of usdx-legazy:5294-5482 ---
  enum event_t { BL_ = 0x10, BR_ = 0x20, BE_ = 0x30, SC_ = 0x01, DC_ = 0x02, PL_ = 0x04, PLC_ = 0x05, PT_ = 0x0C };
  static uint8_t event = 0;
  if(inv ^ digitalRead(BUTTONS)) {   // Left-/Right-/Rotary-button (while not already pressed)
    if(!((event & PL_) || (event & PLC_))) { // hack: if there was long-push before, then fast forward
      uint16_t v    = analogSafeRead(BUTTONS_ADC);
      event         = SC_;
      int32_t  t0   = millis();
      for(; inv ^ digitalRead(BUTTONS);) {   // until released or long-press
        if((millis() - t0) > 300) {
          event = PL_;
          break;
        }
        wdt_reset();
      }
      delay(10); // debounce
      for(; (event != PL_) && ((millis() - t0) < 500);) { // until 2nd press or timeout
        if(inv ^ digitalRead(BUTTONS)) {
          event = DC_;
          break;
        }
        wdt_reset();
      }
      for(; inv ^ digitalRead(BUTTONS);) { // until released, or encoder is turned while longpress
        if(encoder_val && event == PL_) {
          event = PT_;
          break;
        }
        wdt_reset();
      }
      event |= (v < (uint16_t)(4.2 * 1024.0 / 5.0)) ? BL_ : (v < (uint16_t)(4.8 * 1024.0 / 5.0)) ? BR_ : BE_;
    } else { // hack: fast forward handling
      event = (event & 0xf0) | ((encoder_val) ? PT_ : PLC_);
    }
    switch(event) {
    case BL_ | PL_:  // Called when menu button pressed
    case BL_ | PLC_: // or kept pressed
      state = MENU_EDIT;
      render();
      break;
    case BL_ | PT_:
      state = MENU_SELECT;
      render();
      break;
    case BL_ | SC_:
#ifdef CW_MESSAGE
      if((state == MENU_SELECT) && (index == MENU_IDX_CWMSG)) { // trigger CQ message
        cw_msg_event  = millis();
        cw_msg_id     = 0;
        state         = MENU_MAIN;
        break;
      }
#endif
      if(state == MENU_MAIN) { state = MENU_SELECT; index = 0; } // enter menu (Volume first, legacy 5343)
      else if(state == MENU_SELECT) { state = MENU_EDIT; }
      else if(state >= MENU_EDIT) { commit(); state = MENU_MAIN; }
      if(state == MENU_EDIT)
        lcd.cursor();
      else
        lcd.noCursor();
      if(state != MENU_MAIN)
        render();
      break;
    case BL_ | DC_:
      break;
    case BR_ | SC_:
      if(state == MENU_MAIN) {
        int8_t prev_mode = mode;
        if(rit) {
          rit      = 0;
          stepsize = prev_stepsize[mode == CW];
          break;
        }
        mode += 1;
        if(mode > CW)
          mode = LSB; // skip all other modes (only LSB, USB, CW)
        if(mode == CW)
          nr = 0;
        prev_stepsize[prev_mode == CW] = stepsize;
        stepsize                      = prev_stepsize[mode == CW];
        prev_filt[prev_mode == CW]    = filt;
        filt                          = prev_filt[mode == CW];
        vfomode[vfosel % 2]           = mode;
        save_menu_eslot(2); // MODE
        save_menu_eslot(3); // FILTER
        vfo_save_current(); // persist vfomode/band
        si5351.iqmsa = 0;   // enforce PLL reset
        vfo_apply();        // re-apply freq with new mode phase/offset
#ifdef CW_DECODER
        if((prev_mode == CW) && (cwdec))
          show_banner();
#endif
      } else {
        if(state == MENU_SELECT)
          state = MENU_MAIN;
        if(state >= MENU_EDIT) {
          state = MENU_SELECT;
          commit();
        }
        if(state == MENU_SELECT)
          render();
      }
      break;
    case BR_ | DC_:
      filt++;
      _init = true;
      if(mode == CW && filt > N_FILT)
        filt = 4;
      if(mode == CW && filt == 4)
        stepsize = STEP_500;
      if(mode == CW && (filt == 5 || filt == 6) && stepsize < STEP_100)
        stepsize = STEP_100;
      if(mode == CW && filt == 7 && stepsize < STEP_10)
        stepsize = STEP_10;
      if(mode != CW && filt > 3)
        filt = 0;
      encoder_val = 0;
      save_menu_eslot(3); // FILTER
      wdt_reset();
      delay(1500);
      wdt_reset();
      break;
    case BR_ | PL_:
#ifdef RIT_ENABLE
      rit = !rit;
      stepsize = (rit) ? STEP_10 : prev_stepsize[mode == CW];
      if(!rit) { // after RIT comes VFO A/B swap
#else
      {
#endif
        vfosel = !vfosel;
        freq   = vfo[vfosel % 2];
        mode   = vfomode[vfosel % 2];
        if(mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500;
        if(mode == CW) {
          filt = 4;
          nr   = 0;
        } else
          filt = 0;
      }
      vfo_apply();
      break;
    case BE_ | SC_:
      if(state == MENU_MAIN) {
        stepsize_change(+1);
      } else {
        if(state == MENU_SELECT)
          state = MENU_EDIT;
        else if(state == MENU_EDIT) {
          commit();
          state = MENU_SELECT;
        }
#ifdef MENU_STR
        else if(state == MENU_EDIT_TEXT) {
          if(text_pos < 47)
            text_pos++; // NEXT_CH (legacy 5458); stay in text edit
        }
#endif
        render();
      }
      break;
    case BE_ | DC_:
      bandval++;
      if(bandval >= (N_BANDS - 1))
        bandval = 1; // excludes 6m, 160m
      stepsize = STEP_1k;
      vfo_recall_band(bandval);
      set_lpf(freq / 1000000UL);
      break;
    case BE_ | PL_:
      stepsize_change(-1);
      break;
    case BE_ | PT_: // push-and-turn: volume loop + powerDown (legacy 5472-5482)
      for(; inv ^ digitalRead(BUTTONS);) { // until released
        wdt_reset();
        if(encoder_val) {
          int32_t nv = volume + encoder_val; // paramAction(UPDATE, VOLUME) legacy 5476
          encoder_val = 0;
          if(nv < -1)
            nv = 16;
          if(nv > 16)
            nv = -1;
          volume = nv;
          if(volume < 0) {
            volume = 10;
            save_menu_eslot(1);
            powerDown();
          }
          save_menu_eslot(1);
        }
      }
      break;
    default: // PLC / PT / others: fast-forward already handled; ignore
      break;
    }
    event = 0;
  }
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
  if(state == MENU_EDIT)
    lcd.cursor(); // blink cursor on edited value
  else
    lcd.noCursor(); // navigation: no cursor (radio stepsize cursor returns via display_vfo)
}