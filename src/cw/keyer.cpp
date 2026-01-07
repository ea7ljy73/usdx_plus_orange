#include "keyer.h"
#include "../hal/gpio.h"
#include <Arduino.h>

static unsigned long ditTime;
static uint8_t keyerControl;
static uint8_t keyerState;
static uint32_t ktimer;
static int Key_state;
static int debounce;

void update_PaddleLatch()
{
    if(digitalRead(DIT) == LOW) {
        keyerControl |= keyer_swap ? DAH_L : DIT_L;
    }
    if(digitalRead(DAH) == LOW) {
        keyerControl |= keyer_swap ? DIT_L : DAH_L;
    }
}

void loadWPM(int wpm)
{
#if(F_MCU != 20000000)
    ditTime = (1200ULL * F_MCU/16000000)/wpm;
#else
    ditTime = (1200 * 5/4)/wpm;
#endif
}

void keyer()
{
    static uint8_t iAmbic = 0;

    if(menumode != 0) return;

    update_PaddleLatch();

    switch(keyerState)
    {
        case IDLE:
            if((keyerControl & DIT_L) || (keyerControl & DAH_L))
            {
                keyerState = KEYED_PREP;
            }
            Key_state = HIGH;
            digitalWrite(KEY_OUT, Key_state);
            ktimer = millis();
            debounce = 0;
            break;

        case CHK_DIT:
            if(keyerControl & DIT_L)
            {
                keyerState = KEYED_PREP;
            }
            else if(keyerControl & DAH_L)
            {
                keyerState = CHK_DAH;
            }
            else
            {
                if((millis() - ktimer) > (ditTime/2))
                {
                    keyerState = IDLE;
                }
            }
            break;

        case CHK_DAH:
            if(keyerControl & DAH_L)
            {
                keyerState = KEYED_PREP;
            }
            else if(keyerControl & DIT_L)
            {
                keyerState = CHK_DIT;
            }
            else
            {
                if((millis() - ktimer) > (ditTime*2))
                {
                    keyerState = IDLE;
                }
            }
            break;

        case KEYED_PREP:
            ktimer = millis();

            if(keyerControl & DIT_L)
            {
                keyerControl |= DIT_PROC;
                keyerControl &= ~DIT_L;
                iAmbic = 0;
            }
            else if(keyerControl & DAH_L)
            {
                keyerControl &= ~DAH_L;
                iAmbic = 1;
            }

            keyerState = KEYED;
            break;

        case KEYED:
            if((millis() - ktimer) > (ditTime + (ditTime/2) - 2))
            {
                Key_state = HIGH;
                digitalWrite(KEY_OUT, Key_state);
                keyerControl &= ~DIT_PROC;
                keyerState = INTER_ELEMENT;
                ktimer = millis();
            }
            else
            {
                Key_state = LOW;
                digitalWrite(KEY_OUT, Key_state);
            }
            break;

        case INTER_ELEMENT:
            update_PaddleLatch();

            if(keyer_mode == SINGLE)
            {
                keyerState = IDLE;
            }
            else
            {
                if(keyerControl & DIT_PROC)
                {
                    keyerState = CHK_DIT;
                }
                else if((keyerControl & DIT_L) || (keyerControl & DAH_L))
                {
                    if(keyerControl & IAMBICB)
                    {
                        if(keyerControl & DIT_L)
                        {
                            keyerControl |= DIT_PROC;
                            keyerControl &= ~DIT_L;
                        }
                        if(keyerControl & DAH_L)
                        {
                            keyerControl &= ~DAH_L;
                        }
                    }
                    keyerState = CHK_DIT;
                }
                else
                {
                    if(iAmbic == 0)
                    {
                        if((millis() - ktimer) > (ditTime))
                        {
                            keyerState = IDLE;
                        }
                    }
                    else
                    {
                        if((millis() - ktimer) > (ditTime * 3))
                        {
                            keyerState = IDLE;
                        }
                    }
                }
            }
            break;
    }
}

uint8_t getKeyerState()
{
    return keyerState;
}
