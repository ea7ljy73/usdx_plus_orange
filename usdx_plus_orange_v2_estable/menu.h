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

// Parameter types. P_T8S = signed byte (volume -1..16), P_T8 = unsigned byte
// (pwm_min/max, rx_ph_q, vox_thresh, ... match legacy's uint8_t templating).
enum param_type_t { P_T8S, P_T8, P_T16, P_T32, P_ENUM, P_TEXT };

// Program labels: stored in a single PROGMEM table (indexed by id) to save RAM.
// Defined in the .ino as an array of PROGMEM-string pointers named MENU_LABELS.
extern const char* const MENU_LABELS[];

struct MenuParam {
  uint8_t            label;       // index into MENU_LABELS (PROGMEM)
  void*              value;       // pointer to the target variable
  uint8_t            type;        // param_type_t
  int32_t            min, max;    // numeric range; enums use index
  const char* const* enum_labels; // PROGMEM array of pointers to PROGMEM strings (P_ENUM)
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
extern void        on_band(void);
extern void        set_lpf(uint8_t f);
extern void        display_vfo(void);
extern void        display_vfo_line1(void);
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

// button events (legacy 5294): BL/BR/BE x SC/DC/PL
enum event_t { BL_ = 0x10, BR_ = 0x20, BE_ = 0x30, SC_ = 0x01, DC_ = 0x02, PL_ = 0x04 };

// ---------------------------------------------------------------------------
// Menu machine
// ---------------------------------------------------------------------------
class Menu {
public:
  uint8_t state = MENU_MAIN;
  int8_t  index = 0;
  uint8_t text_pos = 0; // string edit cursor position (legacy 'pos')
  uint8_t text_len = 0; // string edit max length
  uint8_t saved_flag = 0; // after EDIT->save, next SELECT click exits to main

  void begin() {
    state = MENU_MAIN;
    index = 0;
    text_pos = 0;
    text_len = 0;
    saved_flag = 0;
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
  void handle_event(uint8_t ev);
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
      lcd.print((const __FlashStringHelper*)pgm_read_ptr(&p.enum_labels[v - p.min]));
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
    case P_T8S:
      v = *(int8_t*)p.value;
      break;
    case P_T8:
      v = *(uint8_t*)p.value;
      break;
    case P_T16:
      v = *(int16_t*)p.value;
      break;
    default:
      v = *(int32_t*)p.value;
      break;
    }
    if((p.min < 0) && (v >= 0))
      lcd.print('+'); // legacy 4032: + prefix for positive when min<0
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
  // dynamic ranges like legacy paramAction PWM_MIN/PWM_MAX (4211-4212)
  extern volatile uint8_t pwm_min, pwm_max;
  if(p.value == (void*)&pwm_min)
    p.max = pwm_max - 1;
  else if(p.value == (void*)&pwm_max)
    p.min = pwm_min;
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
    case P_T8S:
      v = *(int8_t*)p.value;
      break;
    case P_T8:
      v = *(uint8_t*)p.value;
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
      case P_T8S:
        *(int8_t*)p.value = (int8_t)v;
        break;
      case P_T8:
        *(uint8_t*)p.value = (uint8_t)v;
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
  // --- button: NON-BLOCKING state machine. BL/BR fire instantly on release
  // (SC/PL). The DIAL (BE) additionally detects double-click (DC -> band
  // change, legacy 5463) and hold+turn (PT -> volume, legacy 5472). ---
  enum btn_st_t { B_IDLE = 0, B_HOLD = 1, B_DCWAIT = 2 };
  static uint8_t  b_state      = B_IDLE;
  static uint32_t b_t0         = 0;
  static uint16_t b_v          = 0;
  static uint8_t  b_pending    = 0; // BE single click waiting in DC window
  static uint32_t b_dc_deadline = 0;
  static uint8_t  b_is_dc      = 0;
  static uint8_t  b_pt_done    = 0; // dial hold+turn adjusted volume this hold

  // --- button state machine: ALWAYS runs so the dial hold (PT volume) is
  // tracked even while the encoder turns. BL/BR fire on release (SC/PL); the
  // dial (BE) adds double-click (DC -> band) and hold+turn (PT -> volume). ---
  uint8_t pressed = inv ^ digitalRead(BUTTONS); // inv=0 => pressed=HIGH
  if(b_state == B_IDLE) {
    if(pressed) {
      b_state = B_HOLD;
      b_t0    = millis();
      b_v     = analogSafeRead(BUTTONS_ADC);
      b_is_dc = 0;
      b_pt_done = 0;
    }
  } else if(b_state == B_HOLD) {
    uint8_t type = (b_v < (uint16_t)(4.2 * 1024.0 / 5.0)) ? BL_ : (b_v < (uint16_t)(4.8 * 1024.0 / 5.0)) ? BR_ : BE_;
    if(!pressed) { // released -> dispatch
      uint32_t dur = millis() - b_t0;
      b_state      = B_IDLE;
      if(b_is_dc) {
        handle_event(BE_ | DC_); // dial 2nd click -> band change
      } else if(type == BE_ && b_pt_done) {
        ; // PT volume was adjusted this hold: nothing else on release
      } else if(type == BE_ && dur > 400) {
        handle_event(BE_ | PL_); // dial long press (no turn) -> stepsize_change(-1)
      } else if(type == BE_) {
        b_pending      = BE_ | SC_;
        b_dc_deadline  = millis() + 400; // look for 2nd click
        b_state        = B_DCWAIT;
      } else {
        handle_event(((dur > 400) ? PL_ : SC_) | type); // BL/BR instant
      }
    } else if((millis() - b_t0) > 400 && type == BE_) {
      // dial held long + turning -> PT: volume adjust while held (legacy 5472)
      if(encoder_val) {
        int32_t nv = volume + encoder_val;
        encoder_val = 0;
        if(nv < -1)
          nv = 16;
        if(nv > 16)
          nv = -1;
        volume = nv;
        b_pt_done = 1;
        // show "Volume N" while adjusting (legacy paramAction UPDATE, 4030-4036)
        lcd.setCursor(0, 0);
        lcd.print("Volume ");
        lcd.print((int)volume);
        lcd.print("       ");
        if(volume < 0) {
          volume = 10;
          save_menu_eslot(1);
          powerDown();
        }
        save_menu_eslot(1);
      }
    }
  } else if(b_state == B_DCWAIT) {
    if(pressed) { // 2nd click -> will fire DC on release
      b_state = B_HOLD;
      b_t0    = millis();
      b_v     = analogSafeRead(BUTTONS_ADC);
      b_is_dc = 1;
    } else if(millis() > b_dc_deadline) {
      b_state = B_IDLE;
      handle_event(b_pending); // single dial click -> stepsize_change(+1)
    }
  }

  // --- encoder: menu navigation (PT already consumed it for volume if held) ---
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
  }
}

inline void Menu::handle_event(uint8_t ev) {
  switch(ev) {
    case BL_ | PL_: // menu button long press -> fast edit
      state = MENU_EDIT;
      render();
      break;
    case BL_ | SC_: // 1: enter menu, 2: edit, 3: save (stay in select), 4: exit
#ifdef CW_MESSAGE
      if((state == MENU_SELECT) && (index == MENU_IDX_CWMSG)) { // trigger CQ message
        cw_msg_event = millis();
        cw_msg_id    = 0;
        state        = MENU_MAIN;
        break;
      }
#endif
      if(state == MENU_MAIN) { state = MENU_SELECT; index = 0; saved_flag = 0; }
      else if(state == MENU_SELECT) {
        if(saved_flag) { state = MENU_MAIN; saved_flag = 0; }
        else { state = MENU_EDIT; }
      } else if(state == MENU_EDIT) { commit(); state = MENU_SELECT; saved_flag = 1; }
      if(state == MENU_EDIT)
        lcd.cursor();
      else
        lcd.noCursor();
      if(state == MENU_MAIN)
        display_vfo(); // exit to main: show main screen immediately
      else
        render();
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
        display_vfo_line1(); // update mode label immediately (light)
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
          saved_flag = 1; // EDIT->save: next BL exits to main (legacy 5381)
        }
        if(state == MENU_MAIN)
          display_vfo(); // BR exits menu: show main screen immediately
        else
          render();
      }
      break;
    case BR_ | PL_: // RIT toggle + VFO A/B swap (legacy 5423-5437)
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
      display_vfo_line1();
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
          saved_flag = 1;
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
    case BE_ | DC_: // dial double-click -> band change (legacy 5463-5470)
      bandval++;
      if(bandval >= (N_BANDS - 1))
        bandval = 1; // excludes 6m, 160m
      stepsize = STEP_1k;
      on_band(); // freq = band[bandval] + set_lpf + vfo_apply
      break;
    case BE_ | PL_:
      stepsize_change(-1);
      break;
    default:
      break;
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
