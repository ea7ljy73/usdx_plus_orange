#include "display.h"

#ifdef OLED
#include <SSD1306AsciiWire.h>
#else
#include <LiquidCrystal.h>
#endif

void display_init()
{
#ifdef OLED
  lcd.begin(&Adafruit128x64, OLED_ADDR);
  lcd.setFont(Adafruit5x7);
#else
  lcd.begin(16, 2);
#endif

  lcd.setCursor(0, 0);
  lcd.print(F("uSDX Plus Orange"));
  lcd.setCursor(0, 1);
  lcd.print(F(" v" VERSION));
  delay(1500);
  lcd.clear();
}

void display_vfo(int32_t f)
{
  lcd.setCursor(0, 1);
  lcd.print((rit) ? ' ' : ((vfosel%2)|((vfosel==SPLIT) & tx)) ? '\x07' : '\x06');

  int32_t scale=10e6;
  if(rit){
    f = rit;
    scale=1e3;
    lcd.print(F("RIT ")); lcd.print(rit < 0 ? '-' : '+');
  } else {
    if(f/scale == 0){ lcd.print(' '); scale/=10; }
  }
  for(; scale!=1; f%=scale, scale/=10){
    lcd.print(abs(f/scale));
    if(scale == (int32_t)1e3 || scale == (int32_t)1e6) lcd.print(',');
  }

  lcd.print(' '); lcd.print(mode_label[mode]); lcd.print(' ');
  lcd.setCursor(15, 1); lcd.print((vox) ? 'V' : 'R');
}

void display_freq(int32_t f)
{
  lcd.setCursor(0, 0);
  int32_t scale = 1000000UL;
  if(f < 0) { lcd.print('-'); f = -f; }
  for(; scale > 1; f %= scale, scale /= 10) {
    lcd.print(f / scale);
    if(scale == 1000 || scale == 1000000) lcd.print(',');
  }
  lcd.print(F(" kHz"));
}

void display_smeter(uint8_t s)
{
  lcd.setCursor(14, 0);
  if(s < 10) lcd.print('S');
  lcd.print(s);
}

void display_swr(float swr)
{
  lcd.setCursor(4, 0);
  lcd.print(F("SWR:")); lcd.print(swr, 1);
}

void display_vss(float vss)
{
  lcd.setCursor(10, 0);
  int ivss = (int)(vss * 10);
  lcd.print(ivss / 10); lcd.print('.'); lcd.print(ivss % 10); lcd.print("V ");
}

void display_clock()
{
  static uint32_t t = 0;
  if(millis() - t > 1000){
    t = millis();
    uint8_t h = t / 3600000;
    uint8_t m = (t % 3600000) / 60000;
    uint8_t s = (t % 60000) / 1000;
    lcd.setCursor(8, 0);
    lcd.print(h/10); lcd.print(h%10); lcd.print(':');
    lcd.print(m/10); lcd.print(m%10); lcd.print(':');
    lcd.print(s/10); lcd.print(s%10); lcd.print("  ");
  }
}

void display_clear()
{
  lcd.clear();
}

void display_showmsg(const char* msg)
{
  lcd.setCursor(0, 0);
  lcd.print(msg);
  lcd.print(F("                "));
}

void display_showvalue(const char* msg, int16_t value, const char* unit)
{
  lcd.setCursor(0, 1);
  lcd.print('!'); lcd.print('!');
  lcd.print(msg);
  lcd.print('=');
  lcd.print(value);
  lcd.print(unit);
}
