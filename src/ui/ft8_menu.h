#ifndef FT8_MENU_H
#define FT8_MENU_H

#include <Arduino.h>
#include "usdx_config.h"

void ft8_menu_init();
void ft8_menu_enter();
void ft8_menu_exit();
void ft8_menu_process();
bool ft8_menu_is_active();
void ft8_menu_save_state();
void ft8_menu_restore_state();

#endif
