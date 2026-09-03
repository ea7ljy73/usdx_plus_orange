// cw.h - uSDX Plus Orange v2
// CW: Iambic keyer + decoder. Extracted from v1 Section 09.
// Decoder writes to cw_line[] buffer (UI renders it); keyer drives switch_rxtx.
// Same behavior as legacy, decoupled from LCD.

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <stdint.h>

#include "usdx_settings.h"

// ---------------------------------------------------------------------------
// Keyer (Iambic A/B state machine) - faithful port of v1
// ---------------------------------------------------------------------------
#define DIT_L 0x01    // Dit latch
#define DAH_L 0x02    // Dah latch
#define DIT_PROC 0x04 // Dit is being processed
#define IAMBICB 0x10  // 0=Iambic A, 1=Iambic B
#define IAMBICA 0x00  // Iambic A (no bit set)
#define SINGLE 2      // Keyer Mode 2 = straight paddle
#define IDLE 0
#define CHK_DIT 1
#define CHK_DAH 2
#define KEYED_PREP 3
#define KEYED 4
#define INTER_ELEMENT 5

volatile unsigned long ditTime; // ms per dit (extern for hw.h Semi-QSK)
static uint8_t         keyerControl;
static uint8_t         keyerState;
volatile uint8_t       keyer_swap = 0; // exposed for the menu (Keyer Swap)
static int             Key_state  = LOW;
static unsigned long   ktimer;

extern volatile uint8_t keyer_speed; // wpm (menu)
extern volatile uint8_t keyer_mode;  // 0=IambicA,1=IambicB,2=Straight
extern void             switch_rxtx(uint8_t tx_enable);

void loadWPM(int wpm) {
#if(F_MCU != 20000000)
  ditTime = (1200ULL * F_MCU / 16000000) / wpm;
#else
  ditTime = (1200 * 5 / 4) / wpm; // 20MHz clock
#endif
}

// Set keyerControl according to selected mode (legacy usdx-legazy:5639-5642)
void keyer_set_mode(uint8_t mode) {
  if(mode == 0)
    keyerControl = IAMBICA;
  if(mode == 1)
    keyerControl = IAMBICB;
  if(mode == 2)
    keyerControl = SINGLE;
  keyerState = IDLE;
}

void update_PaddleLatch() {
  if(digitalRead(DIT) == LOW)
    keyerControl |= keyer_swap ? DAH_L : DIT_L;
  if(digitalRead(DAH) == LOW)
    keyerControl |= keyer_swap ? DIT_L : DAH_L;
}

void keyer_process() { // call from loop() when mode==CW
  switch(keyerState) {
  case IDLE:
    if((digitalRead(DAH) == LOW) || (digitalRead(DIT) == LOW) || (keyerControl & 0x03)) {
      update_PaddleLatch();
      keyerState = CHK_DIT;
    }
    break;
  case CHK_DIT:
    if(keyerControl & DIT_L) {
      keyerControl |= DIT_PROC;
      ktimer     = ditTime;
      keyerState = KEYED_PREP;
    } else {
      keyerState = CHK_DAH;
    }
    break;
  case CHK_DAH:
    if(keyerControl & DAH_L) {
      ktimer     = ditTime * 3;
      keyerState = KEYED_PREP;
    } else {
      keyerState = IDLE;
    }
    break;
  case KEYED_PREP:
    Key_state = HIGH;
    switch_rxtx(Key_state);
    ktimer += millis(); // interval end time
    keyerControl &= ~(DIT_L + DAH_L);
    keyerState = KEYED;
    break;
  case KEYED:
    if(millis() > ktimer) {
      Key_state = LOW;
      switch_rxtx(Key_state);
      ktimer     = millis() + ditTime; // inter-element time
      keyerState = INTER_ELEMENT;
    } else if(keyerControl & IAMBICB) {
      update_PaddleLatch(); // early latch in Iambic B
    }
    break;
  case INTER_ELEMENT:
    update_PaddleLatch();
    if(millis() > ktimer) {
      if(keyerControl & DIT_PROC) {
        keyerControl &= ~(DIT_L + DIT_PROC);
        keyerState = CHK_DAH;
      } else {
        keyerControl &= ~(DAH_L);
        keyerState = IDLE;
      }
    }
    break;
  }
}

// ---------------------------------------------------------------------------
// Decoder (from v1 OLD_CW path) - writes cw_line[]
// ---------------------------------------------------------------------------
const char m2c[] PROGMEM = "~ "
                           "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/"
                           "***H*7*G*8*90************?_****\"**.****@***'**-***"
                           "*****;!*)*****,****:****";

static unsigned long hightimesavg = 0;
static unsigned long lowduration;
static unsigned long highduration;
static unsigned long starttimehigh       = 0;
static unsigned long startttimelow       = 0;
static uint8_t       sym                 = 1;
static uint8_t       filteredstate       = 0;
static uint8_t       filteredstatebefore = 1;

char             cw_line[] = "                "; // 16 chars + null
volatile uint8_t cw_event  = 0;
volatile uint8_t wpm       = 25;

void printsym() {
  if(sym < 128) {
    char ch = pgm_read_byte_near(m2c + sym);
    if(ch != '*') {
      for(int i = 0; i != 15; i++)
        cw_line[i] = cw_line[i + 1];
      cw_line[15] = ch;
      cw_event    = 1;
    }
  }
  sym = 1;
}

void cw_decode() {
  if(filteredstate != filteredstatebefore) {
    if(filteredstate) { // keyed (HIGH): ended a low
      starttimehigh = millis();
      lowduration   = (millis() - startttimelow);
      if((sym > 1) && lowduration > (hightimesavg * 2)) { // letter space
        printsym();
        wpm = (1200 / hightimesavg * 4 / 3);
      }
      if(lowduration >= hightimesavg * 5) { // word space
        sym = 1;
        printsym();
      }
    } else { // not keyed (LOW): ended a high
      startttimelow = millis();
      highduration  = (millis() - starttimehigh);
      if(highduration < (2 * hightimesavg) || hightimesavg == 0)
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3;
      if(highduration > (5 * hightimesavg))
        hightimesavg = highduration / 3;
      if(highduration > (hightimesavg / 2))
        sym = (sym << 1) | (highduration > (hightimesavg * 2)); // dit(0)/dah(1)
    }
  }
  if(((millis() - startttimelow) > hightimesavg * 6) && (sym > 1))
    printsym();
  filteredstatebefore = filteredstate;
}

// feed the decoder with the current key state (from switch_rxtx)
void cw_set_keyed(uint8_t keyed) { filteredstate = keyed; }