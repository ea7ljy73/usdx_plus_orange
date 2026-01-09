#ifndef CAT_H
#define CAT_H

#include "../usdx_settings.h"
#include <Arduino.h>

#ifdef CAT

#define CATCMD_SIZE 32
extern char CATcmd[CATCMD_SIZE];

// Forward declarations
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
void Command_AG0();
void Command_XT1();
void Command_RT1();
void Command_RC();
void Command_FL0();
void Command_RS();
void Command_VX(char mode);

#ifdef CAT_EXT
void Command_UK(char k1, char k2);
void Command_UD();
void Command_UA(char en);
#endif

void analyseCATcmd();
void serialEvent();

#endif // CAT

#endif // CAT_H
