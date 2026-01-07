#ifndef CW_DECODER_H
#define CW_DECODER_H

#include <Arduino.h>
#include "usdx_config.h"

#ifdef CW_DECODER
void cw_decode();
void dec2();
void printsym(bool submit = true);
#endif

#endif // CW_DECODER_H
