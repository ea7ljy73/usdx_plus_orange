#include "ft8_menu.h"
#include "menu.h"
#include "encoder.h"
#include "display.h"
#include "../state/state.h"

void ft8_menu_init()
{
  ft8mode = 0;
}

void ft8_menu_enter()
{
  ft8_menu_save_state();
  ft8mode = 1;
  mode = USB;
  filt = 0;
  agc = 1;
  nr = 0;
  freq = 50313000UL;
  vfo[0] = freq;
  vfo[1] = freq;
  frequency_update(freq);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("FT8 Mode"));
}

void ft8_menu_exit()
{
  ft8_menu_restore_state();
  ft8mode = 0;
  lcd.clear();
}

void ft8_menu_process()
{
  if(encoder_button_pressed()){
    delay(20);
    while(encoder_button_pressed());
    delay(20);
    ft8_menu_exit();
  }

  lcd.setCursor(0, 0);
  lcd.print(F("FT8    "));
  display_vfo(freq);
}

bool ft8_menu_is_active()
{
  return ft8mode != 0;
}

void ft8_menu_save_state()
{
  prev_mode_ft8 = mode;
  prev_filt_ft8 = filt;
  prev_agc_ft8 = agc;
  prev_nr_ft8 = nr;
}

void ft8_menu_restore_state()
{
  mode = prev_mode_ft8;
  filt = prev_filt_ft8;
  agc = prev_agc_ft8;
  nr = prev_nr_ft8;
  mode_switch(mode);
}
