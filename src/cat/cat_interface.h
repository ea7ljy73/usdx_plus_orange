#ifndef CAT_INTERFACE_H
#define CAT_INTERFACE_H

#include <Arduino.h>
#include "usdx_config.h"

#define CATCMD_SIZE 32

extern char CATcmd[CATCMD_SIZE];
extern volatile uint8_t cat_ptr;
extern volatile uint8_t cat_active;

void cat_init();
void cat_process();
void cat_send_response(const char* response);
void analyseCATcmd();

void Command_GETFreqA();
void Command_SETFreqA();
void Command_IF();
void Command_ID();
void Command_PS();
void Command_PS1();
void Command_AI();
void Command_AI0();
void Command_GetMD();
void Command_SetMD();
void Command_RX();
void Command_TX0();
void Command_TX1();
void Command_TX2();
void Command_RS();
void Command_RC();
void Command_AG0();
void Command_XT1();
void Command_RT1();
void Command_FL0();
void Command_VX(char mode);

#ifdef CAT_EXT
void Command_UK(char k1, char k2);
void Command_UD();
void Command_UA(char en);
#endif

#ifdef CAT_STREAMING
extern volatile uint8_t cat_streaming;
extern volatile uint8_t _cat_streaming;
#endif

#endif
