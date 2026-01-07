#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "usdx_config.h"

void menu_init();
void menu_process();
void menu_enter();
void menu_exit();
void menu_next();
void menu_prev();
bool menu_is_active();
uint8_t menu_get_mode();
void menu_update_param(params_t param);
void menu_action(uint8_t action);

extern const char* const vfosel_label[];
extern const char* const mode_label[];
extern const char* const offon_label[];
extern const char* const filt_label[];
extern const char* const band_label[];
extern const char* const stepsize_label[];
extern const char* const att_label[];
extern const char* const smode_label[];
extern const char* const swr_label[];
extern const char* const cw_tone_label[];
extern const char* const keyer_mode_label[];
extern const char* const agc_label[];
extern const char* const cap_label[];

#endif
