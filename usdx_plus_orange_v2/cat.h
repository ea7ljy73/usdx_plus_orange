// cat.h - uSDX Plus Orange v2
// Kenwood TS-480 CAT interface. Faithful subset of v1 Section 12.
// Provides cat_serial_event() called from serialEvent in the .ino.

#pragma once

#include "si5351.h"
#include "usdx_settings.h"
#include <Arduino.h>
#include <stdint.h>

#define CATCMD_SIZE 40
char CATcmd[CATCMD_SIZE];

volatile uint8_t cat_active = 0;
volatile uint8_t cat_ptr    = 0;

extern volatile int32_t freq;
extern volatile uint8_t mode;
extern volatile uint8_t prev_mode;
extern volatile uint8_t changedModeCAT;
extern volatile int32_t rit;
extern volatile uint8_t vfosel;
extern uint8_t          vfomode[2];
extern volatile uint32_t semi_qsk_timeout;
extern volatile uint8_t  smode;
extern volatile uint32_t rxend_event;
extern void             switch_rxtx(uint8_t tx_enable);
extern void             vfo_apply(void);

// --- response formatter (no sprintf, saves flash) --------------------------
static inline void cat_print(char c) { Serial.print(c); }
static inline void cat_print_u32(uint32_t v, uint8_t digits) {
  char    buf[10];
  uint8_t i = digits;
  buf[i]    = 0;
  while(i) {
    buf[--i] = '0' + (v % 10);
    v /= 10;
  }
  Serial.print(buf);
}
static inline void cat_print_u8(uint8_t v) { Serial.print(v, DEC); }

static void Command_GETFreqA() {
  uint32_t tf = freq;
  cat_print('F');
  cat_print('A');
  cat_print_u32(tf / 1000000000lu, 2);
  tf %= 1000000000lu;
  cat_print_u32(tf / 1000000lu, 3);
  tf %= 1000000lu;
  cat_print_u32(tf / 1000lu, 3);
  tf %= 1000lu;
  cat_print_u32(tf, 3);
  cat_print(';');
}

static void Command_IF() {
  uint32_t tf = freq;
  Serial.print("IF");
  cat_print_u32(tf / 1000000000lu, 2);
  tf %= 1000000000lu;
  cat_print_u32(tf / 1000000lu, 3);
  tf %= 1000000lu;
  cat_print_u32(tf / 1000lu, 3);
  tf %= 1000lu;
  cat_print_u32(tf, 3);
  Serial.print("00000+0000000000");
  cat_print_u8(mode + 1);
  Serial.print("0000000;"); // full TS-480 IF frame (legacy 4547)
}

static void Command_SETFreqA() { // legacy 4512-4520: no range check
  if(CATcmd[2] != ';') { // 'FAxxxx...;'
    freq = (uint32_t)atol(CATcmd + 2);
    vfo_apply(); // mode-dependent IQ phase + CW offset (legacy change handler)
  }
}
static void Command_AI() { Serial.print("AI0;"); }
static void Command_AI0() { Serial.print("AI0;"); } // legacy parity
static void Command_AG0() { Serial.print("AG0;"); }
static void Command_XT1() { Serial.print("XT1;"); }
static void Command_RT1() { Serial.print("RT1;"); }
static void Command_RC() {
  rit = 0;
  Serial.print("RC;");
}
static void Command_FL0() { Serial.print("FL0;"); }
static void Command_TX0() { switch_rxtx(1); } // legacy parity (TX on)
static void Command_TX1() { switch_rxtx(1); }
static void Command_TX2() { switch_rxtx(1); }
static void Command_PS1() { /* no-op (legacy parity) */ }
static void Command_VX(char c) {
  Serial.print("VX");
  Serial.print(c);
  Serial.print(';');
}
static void Command_GetMD() {
  Serial.print("MD");
  cat_print_u8(mode + 1);
  cat_print(';');
}
static void Command_SetMD() { // legacy 4589-4596: no range check
  mode = CATcmd[2] - '1';
  vfomode[vfosel % 2] = mode; // legacy 4593
  si5351.iqmsa = 0;           // enforce PLL reset (legacy 4595)
  vfo_apply();
}
static void Command_RX() {
  switch_rxtx(0);
  semi_qsk_timeout = 0; // hack for multiple RX cmds (legacy 4607)
  Serial.print("RX0;");
}
static void Command_TX() { switch_rxtx(1); }
static void Command_PS() { Serial.print("PS1;"); }
static void Command_RS() { Serial.print("RS0;"); }
static void Command_ID() { Serial.print("ID020;"); }

static void analyseCATcmd() {
  char c0 = CATcmd[0], c1 = CATcmd[1], c2 = CATcmd[2];
  if(c0 == 'F' && c1 == 'A') {
    (c2 == ';') ? Command_GETFreqA() : Command_SETFreqA();
  } else if(c0 == 'I' && c1 == 'F' && c2 == ';') {
    Command_IF();
  } else if(c0 == 'M' && c1 == 'D') {
    if(c2 == ';')
      Command_GetMD();
    else if(CATcmd[3] == ';')
      Command_SetMD();
  } else if(c0 == 'R' && c1 == 'X' && c2 == ';') {
    Command_RX();
  } else if(c0 == 'T' && c1 == 'X' && (c2 == ';' || c2 == '0' || c2 == '1' || c2 == '2')) {
    Command_TX();
  } else if(c0 == 'R' && c1 == 'T') {
    if(c2 == '1')
      Command_RT1();
    else if(c2 == 'C')
      Command_RC();
  } else if(c0 == 'I' && c1 == 'D' && c2 == ';') {
    Command_ID();
  } else if(c0 == 'P' && c1 == 'S') {
    if(c2 == ';')
      Command_PS();
    else if(c2 == '1')
      Command_PS1();
  } else if(c0 == 'A' && c1 == 'I') {
    if(c2 == '0' || c2 == ';')
      Command_AI0(); // AI and AI0 both → AI0 (legacy parity)
    else
      Command_AI();
  } else if(c0 == 'A' && c1 == 'G' && c2 == '0') {
    Command_AG0();
  } else if(c0 == 'X' && c1 == 'T' && c2 == '1') {
    Command_XT1();
  } else if(c0 == 'F' && c1 == 'L' && c2 == '0') {
    Command_FL0();
  } else if(c0 == 'V' && c1 == 'X' && CATcmd[3] == ';') {
    Command_VX(c2);
  } else if(c0 == 'R' && c1 == 'S' && c2 == ';') {
    Command_RS();
  } else {
    Serial.print("?;"); // legacy 4424-4431
  }
}

// called from serialEvent() in the .ino
static void cat_serial_event() {
  if(Serial.available()) {
    rxend_event = millis() + 10; // block display to prevent CAT interleave (legacy 4438)
    char data         = Serial.read();
    CATcmd[cat_ptr++] = data;
    if(data == ';') {
      CATcmd[cat_ptr] = '\0';
      cat_ptr         = 0;
      if(!cat_active) {
        cat_active = 1;
        smode      = 0; // disable smeter to reduce display activity (legacy 4445)
      }
      analyseCATcmd();
      delay(10); // legacy 4455
    } else if(cat_ptr > (CATCMD_SIZE - 1)) {
      Serial.print("E;");
      cat_ptr = 0;
    }
  }
}
