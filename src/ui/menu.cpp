#include "menu.h"
#include "display.h"
#include "../state/state.h"
#include "../state/bands.h"

void menu_init()
{
  menumode = 0;
  menu = 0;
}

void menu_enter()
{
  if(menumode == 0){
    prev_menumode = 1;
    menumode = 1;
    menu = 0;
    lcd.noCursor();
  }
}

void menu_exit()
{
  menumode = 0;
  prev_menumode = 0;
  menu = 0;
  state_update();
}

void menu_next()
{
  if(menumode == 1){
    menu++;
    if(menu >= N_PARAMS) menu = 0;
  }
}

void menu_prev()
{
  if(menumode == 1){
    if(menu == 0) menu = N_PARAMS - 1;
    else menu--;
  }
}

bool menu_is_active()
{
  return menumode != 0;
}

uint8_t menu_get_mode()
{
  return menumode;
}

void printmenuid(uint8_t menuid)
{
  static const char seperator[] = {'.', ' '};
  uint8_t ids[] = {(uint8_t)(menuid >> 4), (uint8_t)(menuid & 0xF)};
  for(int i = 0; i < 2; i++){
    uint8_t id = ids[i];
    if(id >= 10){ id -= 10; lcd.print('1'); }
    lcd.print(char('0' + id));
    lcd.print(seperator[i]);
  }
}

void printlabel(uint8_t action, uint8_t menuid, const __FlashStringHelper* label)
{
  if(action == UPDATE_MENU){
    lcd.setCursor(0, 0);
    printmenuid(menuid);
    lcd.print(label); display_blanks(); display_blanks();
    lcd.setCursor(0, 1);
    if(menumode >= 2) lcd.print('>');
  } else {
    lcd.setCursor(0, 1); lcd.print(label); lcd.print(F(": "));
  }
}

template<typename T> void paramAction(uint8_t action, volatile T& value, uint8_t menuid, const __FlashStringHelper* label, const char* const enumArray[], int32_t _min, int32_t _max, bool continuous)
{
  switch(action){
    case UPDATE:
    case UPDATE_MENU:
      if(((int32_t)value + encoder_val) < _min) value = (continuous) ? _max : _min;
      else if(((int32_t)value + encoder_val) > _max) value = (continuous) ? _min : _max;
      else value = (int32_t)value + encoder_val;
      encoder_val = 0;

      lcd.noCursor();
      printlabel(action, menuid, label);
      if(enumArray == NULL){
        if((_min < 0) && (value >= 0)) lcd.print('+');
        lcd.print(value);
      } else {
        lcd.print(enumArray[value]);
      }
      display_blanks(); display_blanks();
      break;
    default:
      break;
  }
}

void menu_process()
{
  if(menumode == 0){
    display_vfo(freq);
    return;
  }

  switch(menu){
    case VOLUME:
      paramAction(UPDATE_MENU, volume, (VOLUME << 4) | 0, F("Volume"), NULL, 0, 31, true);
      break;
    case MODE:
      paramAction(UPDATE_MENU, mode, (MODE << 4) | 0, F("Mode"), mode_label, LSB, AM, false);
      break;
    case FILTER:
      paramAction(UPDATE_MENU, filt, (FILTER << 4) | 0, F("Filter"), filt_label, 0, 7, false);
      break;
    case BAND:
      paramAction(UPDATE_MENU, band, (BAND << 4) | 0, F("Band"), band_label, 0, N_BANDS-1, false);
      break;
    case STEP:
      paramAction(UPDATE_MENU, step_index, (STEP << 4) | 0, F("Step"), stepsize_label, 0, N_STEPSIZES-1, false);
      break;
    case VFOSEL:
      paramAction(UPDATE_MENU, vfosel, (VFOSEL << 4) | 0, F("VFO"), vfosel_label, 0, 1, false);
      break;
    case RIT:
      paramAction(UPDATE_MENU, rit, (RIT << 4) | 0, F("RIT"), NULL, -1000, 1000, true);
      break;
    case AGC:
      paramAction(UPDATE_MENU, agc, (AGC << 4) | 0, F("AGC"), agc_label, 0, 2, false);
      break;
    case NR:
      paramAction(UPDATE_MENU, nr, (NR << 4) | 0, F("NR"), offon_label, 0, 1, false);
      break;
    case ATT:
      paramAction(UPDATE_MENU, att, (ATT << 4) | 0, F("ATT"), att_label, 0, 3, false);
      break;
    case CWTONE:
      paramAction(UPDATE_MENU, cw_tone, (CWTONE << 4) | 0, F("CW Tone"), cw_tone_label, 0, 2, false);
      break;
    case CWSPEED:
      paramAction(UPDATE_MENU, keyer_speed, (CWSPEED << 4) | 0, F("CW Speed"), NULL, 5, 50, true);
      break;
    case DRIVE:
      paramAction(UPDATE_MENU, drive, (DRIVE << 4) | 0, F("Drive"), NULL, 0, 8, false);
      break;
    case VOX:
      paramAction(UPDATE_MENU, vox, (VOX << 4) | 0, F("VOX"), offon_label, 0, 1, false);
      break;
    default:
      break;
  }
}

void menu_update_param(params_t param)
{
  menu = param;
  menumode = 1;
}
