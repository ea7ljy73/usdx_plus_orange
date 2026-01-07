#include "decoder.h"
#include "../ui/display.h"

#ifdef CW_DECODER

static int32_t avg = 256;
static uint8_t sym;
static uint32_t amp32 = 0;
static char out[] = "                ";

void printsym(bool submit)
{
    if(sym < 128)
    {
        char ch = pgm_read_byte_near(m2c + sym);
        if(ch != '*')
        {
            for(int i = 0; i != 15; i++)
                out[i] = out[i + 1];
            out[15] = ch;
        }
    }
    if(submit)
        sym = 1;
}

bool realstate = LOW;
bool realstatebefore = LOW;
bool filteredstate = LOW;
bool filteredstatebefore = LOW;
static uint8_t nbtime = 16;
static uint32_t starttimehigh;
static uint32_t highduration;
static uint32_t hightimesavg;
static uint32_t lowtimesavg;
static uint32_t startttimelow;
static uint32_t lowduration;
static uint32_t laststarttime = 0;
static uint8_t cw_wpm = 25;

void cw_decode()
{
    int32_t in = _amp32;
    EMA(avg, in, (1 << 8));
    realstate = (in > (avg * 1 / 2));

    if(realstate != realstatebefore)
    {
        laststarttime = millis();
    }

#ifdef NB_SCALED_TO_WPM
    if((millis() - laststarttime) > min(1200 / (20 * 2), max(1200 / (40 * 2), hightimesavg / 6)))
#else
    if((millis() - laststarttime) > nbtime)
#endif
    {
        if(realstate != filteredstate)
        {
            filteredstate = realstate;
        }
    }
    else
    {
        avg += avg / 100;
    }

    dec2();
    realstatebefore = realstate;
}

#ifdef NEW_CW
void dec2()
{
    if(filteredstate != filteredstatebefore)
    {
        if(menumode == 0)
        {
            lcd.noCursor();
            lcd.setCursor(15, 1);
            lcd.print(filteredstate ? 'R' : ' ');
        }

        if(filteredstate == HIGH)
        {
            starttimehigh = millis();
            lowduration = (millis() - startttimelow);
        }

        if(filteredstate == LOW)
        {
            startttimelow = millis();
            highduration = (millis() - starttimehigh);
            if(highduration < (2 * hightimesavg) || hightimesavg == 0)
            {
                hightimesavg = (highduration + hightimesavg + hightimesavg) / 3;
            }
            if(highduration > (5 * hightimesavg))
            {
                hightimesavg = highduration / 3;
            }
        }
    }

    if(filteredstate != filteredstatebefore)
    {
        if(filteredstate == LOW)
        {
            if(highduration < (hightimesavg + hightimesavg / 2) && highduration > (hightimesavg * 6 / 10))
            {
                sym = (sym << 1) | (0);
            }
            if(highduration > (hightimesavg + hightimesavg / 2) && highduration < (hightimesavg * 6))
            {
                sym = (sym << 1) | (1);
                cw_wpm = (cw_wpm + (1200 / ((highduration) / 3) * 4 / 3)) / 2;
            }
        }

        if(filteredstate == HIGH)
        {
            uint16_t lacktime = 10;
            if(cw_wpm > 25) lacktime = 10;
            if(cw_wpm > 30) lacktime = 12;
            if(cw_wpm > 35) lacktime = 15;

            if(lowduration > (hightimesavg * (lacktime * 1 / 10)) && lowduration < hightimesavg * (lacktime * 5 / 10))
            {
                printsym();
            }
            if(lowduration >= hightimesavg * (lacktime * 5 / 10))
            {
                printsym();
                printsym();
            }
        }
    }

    if((millis() - startttimelow) > (highduration * 6) && (sym > 1))
    {
        printsym();
    }

    filteredstatebefore = filteredstate;
}
#else // OLD_CW
void dec2()
{
    if(filteredstate != filteredstatebefore)
    {
        if(menumode == 0)
        {
            lcd.noCursor();
            lcd.setCursor(15, 1);
            lcd.print(filteredstate ? 'R' : ' ');
        }

        if(filteredstate == HIGH)
        {
            starttimehigh = millis();
            lowduration = (millis() - startttimelow);

            if((sym > 1) && lowduration > (hightimesavg * 2))
            {
                printsym();
                cw_wpm = (1200 / hightimesavg * 4 / 3);
            }
            if(lowduration >= hightimesavg * 5)
            {
                sym = 1;
                printsym();
            }
        }

        if(filteredstate == LOW)
        {
            startttimelow = millis();
            highduration = (millis() - starttimehigh);
            if(highduration < (2 * hightimesavg) || hightimesavg == 0)
            {
                hightimesavg = (highduration + hightimesavg + hightimesavg) / 3;
            }
            if(highduration > (5 * hightimesavg))
            {
                hightimesavg = highduration / 3;
            }
            if(highduration > (hightimesavg / 2))
            {
                sym = (sym << 1) | (highduration > (hightimesavg * 2));
            }
        }
    }

    if(((millis() - startttimelow) > hightimesavg * 6) && (sym > 1))
    {
        printsym();
    }

    filteredstatebefore = filteredstate;
}
#endif // NEW_CW

#endif // CW_DECODER
