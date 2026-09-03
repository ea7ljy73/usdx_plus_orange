// cw.h - uSDX Plus Orange v2
// CW: Iambic keyer + decoder. Extracted from v1 Section 09.
// Decoder writes to cw_line[] buffer (UI renders it); keyer drives switch_rxtx.
// Same behavior as legacy, decoupled from LCD.

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
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
// Decoder - faithful port of usdx-legazy OLD_CW (audio stage + timing stage)
// Writes cw_line[] (decoupled from LCD, v2 design).
// ---------------------------------------------------------------------------
const char m2c[] PROGMEM = "~ "
                           "ETIANMSURWDKGOHVF*L*PJBXCYZQ**54S3***2**+***J16=/"
                           "***H*7*G*8*90************?_****\"**.****@***'**-***"
                           "*****;!*)*****,****:****";

// audio stage (legacy 2301-2330)
static int32_t  avg             = 256;
static bool     realstate       = LOW;
static bool     realstatebefore = LOW;
static uint8_t  nbtime          = 16; // ms noise blanker
static uint32_t laststarttime   = 0;

// timing stage (legacy 2324-2330)
static unsigned long hightimesavg        = 0;
static unsigned long lowduration;
static unsigned long highduration;
static unsigned long starttimehigh       = 0;
static unsigned long startttimelow       = 0;
static uint8_t       sym                 = 1;
static bool          filteredstate       = LOW;
static bool          filteredstatebefore = LOW;

char             cw_line[] = "                "; // 16 chars + null
volatile uint8_t cw_event  = 0;
volatile uint8_t wpm       = 25;

extern volatile uint32_t _amp32; // audio amplitude feed (rx.h)

void dec2(); // forward (timing decoder)

#ifndef EA
#define EA(y, x, one_over_alpha) (y) = (y) + ((x) - (y)) / (one_over_alpha);
#endif

void printsym(bool submit = true) { // legacy 2308-2317
  if(sym < 128) {
    char ch = pgm_read_byte_near(m2c + sym);
    if(ch != '*') {
#ifdef CW_INTERMEDIATE
      cw_line[15] = ch;
      cw_event    = true;
      if(submit) { // only shift when submit is true, otherwise update last char only
        for(int i = 0; i != 15; i++)
          cw_line[i] = cw_line[i + 1];
        cw_line[15] = ' ';
      }
#else
      for(int i = 0; i != 15; i++)
        cw_line[i] = cw_line[i + 1];
      cw_line[15] = ch;
      cw_event    = true;
#endif
    }
  }
  if(submit)
    sym = 1;
}

// audio-amplitude decoder (legacy 2333-2357): _amp32 -> threshold -> noise
// blanker -> filteredstate -> dec2(). Called from loop() during RX CW.
void cw_decode() {
  int32_t in = _amp32;
  EA(avg, in, (1 << 8));
  realstate = (in > (avg * 1 / 2)); // threshold

  // here we clean up the state with a noise blanker
  if(realstate != realstatebefore)
    laststarttime = millis();
  if((millis() - laststarttime) > nbtime) {
    if(realstate != filteredstate)
      filteredstate = realstate;
  } else
    avg += avg / 100; // keep threshold above noise spikes (increase threshold with 1%)

  dec2();
  realstatebefore = realstate;
}

// timing decoder (legacy OLD_CW dec2, 2437-2483)
void dec2() {
  if(filteredstate != filteredstatebefore) { // then we do want to have some durations on high and low
    if(filteredstate == HIGH) {
      starttimehigh = millis();
      lowduration   = (millis() - startttimelow);
      if((sym > 1) && lowduration > (hightimesavg * 2)) { // letter space
        printsym();
        wpm = (1200 / hightimesavg * 4 / 3);
      }
      if(lowduration >= hightimesavg * (5)) { // word space
        sym = 1;
        printsym(); // (print additional space)
      }
    }
    if(filteredstate == LOW) {
      startttimelow = millis();
      highduration  = (millis() - starttimehigh);
      if(highduration < (2 * hightimesavg) || hightimesavg == 0)
        hightimesavg = (highduration + hightimesavg + hightimesavg) / 3; // now we know avg dit time (rolling 3 avg)
      if(highduration > (5 * hightimesavg))
        hightimesavg = highduration / 3; // if speed decrease fast ..
      if(highduration > (hightimesavg / 2)) { // dit (0) or dash (1)
        sym = (sym << 1) | (highduration > (hightimesavg * 2));
#if defined(CW_INTERMEDIATE) && !defined(OLED) && !defined(LCD_I2C) && (F_MCU >= 20000000)
        printsym(false);
#endif
      }
    }
  }
  if(((millis() - startttimelow) > hightimesavg * (6)) && (sym > 1))
    printsym(); // write if no more letters
  filteredstatebefore = filteredstate;
}

// feed the decoder with the current key state (from switch_rxtx)
void cw_set_keyed(uint8_t keyed) { filteredstate = keyed; }

// ---------------------------------------------------------------------------
// CW messages (legacy 2243-2294, CW_MESSAGE)
// ---------------------------------------------------------------------------
#ifdef CW_MESSAGE
extern uint8_t inv; // button polarity (menu.h)
volatile uint8_t cw_msg_interval = 5; // number of seconds CW message is repeated
volatile uint32_t cw_msg_event    = 0;
volatile uint8_t  cw_msg_id       = 0; // selected message
char              cw_msg[1][48]   = {CW_MSG1};

uint8_t delayWithKeySense(uint32_t ms) { // legacy 2246-2256
  uint32_t event = millis() + ms;
  for(; millis() < event;) {
    wdt_reset();
    if(inv ^ digitalRead(BUTTONS) || !digitalRead(DAH) || !digitalRead(DIT)) {
      for(; inv ^ digitalRead(BUTTONS);)
        wdt_reset(); // wait until buttons released
      return 1;      // stop when button/key pressed
    }
  }
  return 0;
}

int cw_tx(char ch) { // legacy 2266-2286: transmit message in CW
  char sym;
  for(uint8_t j = 0; (sym = pgm_read_byte_near(m2c + j)); j++) { // lookup msg[i] in m2c
    if(sym == ch) {
      wdt_reset();
      uint8_t k = 0x80;
      for(; !(j & k); k >>= 1);
      k >>= 1; // shift start of cw code to MSB
      if(k == 0)
        delay(ditTime * 4); // space -> add word space
      else {
        for(; k; k >>= 1) { // send dit/dah one by one
          switch_rxtx(1);   // key-on tx
          if(delayWithKeySense(ditTime * ((j & k) ? 3 : 1))) {
            switch_rxtx(0);
            return 1;
          }
          switch_rxtx(0); // key-off tx
          if(delayWithKeySense(ditTime))
            return 1; // add symbol space
        }
        if(delayWithKeySense(ditTime * 2))
          return 1; // add letter space
      }
      break; // next character
    }
  }
  return 0;
}

int cw_tx(char* msg) { // legacy 2288-2294
  for(uint8_t i = 0; msg[i]; i++) {
    if(cw_tx(msg[i]))
      return 1;
  }
  return 0;
}
#endif //CW_MESSAGE