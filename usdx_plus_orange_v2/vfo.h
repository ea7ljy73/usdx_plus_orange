// vfo.h - uSDX Plus Orange v2
// VFO + band memory. Persists last frequency/mode per band (EEPROM).
// Mirrors v1 KEEP_BAND_DATA behavior, simplified and modular.

#pragma once

#include "usdx_settings.h"
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#define N_BANDS 11              // legacy usdx-legazy (160m..6m)
#define BANDCOUNT (N_BANDS - 2) // persisted bands (exclude 160m & 6m, like legacy)

// FT8-style default band frequencies (same as legacy, index 0..10)
const uint32_t band[N_BANDS] PROGMEM = {1840000,  3573000,  5357000,  7074000,  10136000, 14074000,
                                        18100000, 21074000, 24915000, 28074000, 50313000};

// enum step_t + stepsizes (PROGMEM) + stepsize var
enum step_t { STEP_10M, STEP_1M, STEP_500k, STEP_100k, STEP_10k, STEP_1k, STEP_500, STEP_100, STEP_10, STEP_1 };
const uint32_t stepsizes[10] PROGMEM = {10000000, 1000000, 500000, 100000, 10000, 1000, 500, 100, 10, 1};

extern volatile int32_t freq;
extern volatile uint8_t mode;
extern volatile uint8_t bandval;
extern volatile uint8_t stepsize;
extern int32_t vfo[2];     // VFO A/B freq (owned by main .ino, legacy parity)
extern uint8_t vfomode[2]; // VFO A/B mode (owned by main .ino, legacy parity)
extern void (*vfo_apply_freq)(int32_t); // hook to si5351.freq (set in .ino)
extern int32_t vfo_cache_freq;          // last applied freq

// persisted last freq/mode per band
static int32_t freq_last[BANDCOUNT];
static uint8_t mode_last[BANDCOUNT];

// EEPROM layout after menu region (menu slots: 0x150 + eslot*8, eslot up to 31
// -> 0x248). VFO starts at 0x250 to avoid overlap.
#define EEPROM_VFO_OFF 0x250
#define EEPROM_VFO_BANDSIZE (BANDCOUNT * (sizeof(int32_t) + 1))
#define EEPROM_VFO_AB_OFF (EEPROM_VFO_OFF + EEPROM_VFO_BANDSIZE)

// load freq/mode memory from EEPROM (call at setup)
void vfo_eeprom_load() {
  eeprom_read_block(freq_last, (const void*)EEPROM_VFO_OFF, sizeof(freq_last));
  eeprom_read_block(mode_last, (const void*)(EEPROM_VFO_OFF + sizeof(freq_last)), sizeof(mode_last));
  // VFO A/B state (legacy FREQA/FREQB/MODEA/MODEB parity): apply only sane values
  int32_t vtmp[2];
  uint8_t mtmp[2];
  eeprom_read_block(vtmp, (const void*)EEPROM_VFO_AB_OFF, sizeof(vtmp));
  eeprom_read_block(mtmp, (const void*)(EEPROM_VFO_AB_OFF + sizeof(vtmp)), sizeof(mtmp));
  for(uint8_t k = 0; k < 2; k++) {
    if(vtmp[k] >= 100000L && vtmp[k] <= 60000000L)
      vfo[k] = vtmp[k]; // else keep compiled defaults (virgin EEPROM)
    if(mtmp[k] <= 4)
      vfomode[k] = mtmp[k];
  }
}

void vfo_eeprom_save() {
  eeprom_update_block(freq_last, (void*)EEPROM_VFO_OFF, sizeof(freq_last));
  eeprom_update_block(mode_last, (void*)(EEPROM_VFO_OFF + sizeof(freq_last)), sizeof(mode_last));
  eeprom_update_block(vfo, (void*)EEPROM_VFO_AB_OFF, sizeof(int32_t) * 2);
  eeprom_update_block(vfomode, (void*)(EEPROM_VFO_AB_OFF + sizeof(int32_t) * 2), 2);
}

// save current freq/mode into slot for current band + VFO A/B state
void vfo_save_current() {
  if(bandval >= 1 && bandval <= BANDCOUNT) {
    uint8_t b    = bandval - 1;
    freq_last[b] = freq;
    mode_last[b] = mode;
  }
  vfo_eeprom_save(); // VFO A/B always persisted (legacy FREQA/B parity)
}

// recall/apply band memory for a band (or default if none stored)
void vfo_recall_band(int8_t b) {
  if(b < 1 || b > BANDCOUNT)
    return;
  uint8_t idx = b - 1;
  int32_t f   = freq_last[idx];
  if(f < 100000 || f > 60000000)
    f = (int32_t)pgm_read_dword(&band[b]); // default band freq
  uint8_t ml = mode_last[idx];
  if(b > 3) // 20m+ bands are USB by convention (v1: mode_last default)
    mode = (ml <= 4) ? ml : USB; // 0xFF = unset; LSB(0) kept as LSB
  else
    mode = (ml <= 4) ? ml : LSB;
  freq    = f;
  bandval = b;
  if(vfo_apply_freq)
    vfo_apply_freq(f);
  vfo_cache_freq = f;
}

// apply current freq to hardware (used on tune)
void vfo_apply() {
  if(vfo_apply_freq)
    vfo_apply_freq(freq);
  vfo_cache_freq = freq;
}
