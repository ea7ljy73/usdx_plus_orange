#include "cat_interface.h"
#include "../state/state.h"
#include "../drivers/si5351.h"

char CATcmd[CATCMD_SIZE];
volatile uint8_t cat_ptr = 0;

#ifdef CAT_STREAMING
volatile uint8_t cat_streaming = 0;
volatile uint8_t _cat_streaming = 0;
#endif

void cat_init()
{
#ifdef CAT
  Serial.begin(38400);
#endif
}

void cat_process()
{
#ifdef CAT
  if(Serial.available()){
    rxend_event = millis() + 10;
    char data = Serial.read();
    CATcmd[cat_ptr++] = data;
    if(data == ';'){
      CATcmd[cat_ptr] = '\0';
      cat_ptr = 0;
#ifdef _SERIAL
      if(!cat_active){ cat_active = 1; smode = 0; }
#endif
#ifdef CAT_STREAMING
      if(cat_streaming){ noInterrupts(); cat_streaming = false; Serial.print(';'); }
      analyseCATcmd();
      if(_cat_streaming){ Serial.print("US"); cat_streaming = true; }
      interrupts();
#else
      analyseCATcmd();
#endif
      delay(10);
    } else if(cat_ptr > (CATCMD_SIZE - 1)){
      Serial.print("E;");
      cat_ptr = 0;
    }
  }
#endif
}

void cat_send_response(const char* response)
{
  Serial.print(response);
}

void analyseCATcmd()
{
  if((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[2] == ';'))
    Command_GETFreqA();
  else if((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[13] == ';'))
    Command_SETFreqA();
  else if((CATcmd[0] == 'I') && (CATcmd[1] == 'F') && (CATcmd[2] == ';'))
    Command_IF();
  else if((CATcmd[0] == 'I') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_ID();
  else if((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_PS();
  else if((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == '1'))
    Command_PS1();
  else if((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == ';'))
    Command_AI();
  else if((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == '0'))
    Command_AI0();
  else if((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_GetMD();
  else if((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[3] == ';'))
    Command_SetMD();
  else if((CATcmd[0] == 'R') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_RX();
  else if((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_TX0();
  else if((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '0'))
    Command_TX0();
  else if((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '1'))
    Command_TX1();
  else if((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '2'))
    Command_TX2();
  else if((CATcmd[0] == 'A') && (CATcmd[1] == 'G') && (CATcmd[2] == '0'))
    Command_AG0();
  else if((CATcmd[0] == 'X') && (CATcmd[1] == 'T') && (CATcmd[2] == '1'))
    Command_XT1();
  else if((CATcmd[0] == 'R') && (CATcmd[1] == 'T') && (CATcmd[2] == '1'))
    Command_RT1();
  else if((CATcmd[0] == 'R') && (CATcmd[1] == 'C') && (CATcmd[2] == ';'))
    Command_RC();
  else if((CATcmd[0] == 'F') && (CATcmd[1] == 'L') && (CATcmd[2] == '0'))
    Command_FL0();
  else if((CATcmd[0] == 'R') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_RS();
  else if((CATcmd[0] == 'V') && (CATcmd[1] == 'X') && (CATcmd[2] != ';'))
    Command_VX(CATcmd[2]);
#ifdef CAT_EXT
  else if((CATcmd[0] == 'U') && (CATcmd[1] == 'K') && (CATcmd[4] == ';'))
    Command_UK(CATcmd[2], CATcmd[3]);
  else if((CATcmd[0] == 'U') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_UD();
#endif
#ifdef CAT_STREAMING
  else if((CATcmd[0] == 'U') && (CATcmd[1] == 'A') && (CATcmd[3] == ';'))
    Command_UA(CATcmd[2]);
#endif
  else {
    Serial.print("?;");
  }
}

void Command_GETFreqA()
{
#ifdef _SERIAL
  if(!cat_active) return;
#endif
  char Catbuffer[32];
  uint32_t tf = freq;

  uint8_t g = tf / 1000000000UL; tf %= 1000000000UL;
  uint8_t m = tf / 1000000UL; tf %= 1000000UL;
  uint16_t k = tf / 1000UL; tf %= 1000UL;
  uint16_t h = tf;

  sprintf(Catbuffer, "FA%02u%03u%03u%03u;", g, m, k, h);
  Serial.print(Catbuffer);
}

void Command_SETFreqA()
{
  char Catbuffer[16];
  strncpy(Catbuffer, CATcmd + 2, 11);
  Catbuffer[11] = '\0';

  freq = atol(Catbuffer);
  change = true;
  frequency_update(freq);
}

void Command_IF()
{
#ifdef _SERIAL
  if(!cat_active) return;
#endif
  char Catbuffer[32];
  uint32_t tf = freq;

  uint8_t g = tf / 1000000000UL; tf %= 1000000000UL;
  uint8_t m = tf / 1000000UL; tf %= 1000000UL;
  uint16_t k = tf / 1000UL; tf %= 1000UL;
  uint16_t h = tf;

  sprintf(Catbuffer, "IF%02u%03u%03u%03u", g, m, k, h);
  Serial.print(Catbuffer);
  sprintf(Catbuffer, "00000+000000");
  Serial.print(Catbuffer);
  sprintf(Catbuffer, "0000");
  Serial.print(Catbuffer);
  Serial.print(mode + 1);
  sprintf(Catbuffer, "0000000;");
  Serial.print(Catbuffer);
}

void Command_ID()
{
  Serial.print("ID020;");
}

void Command_PS()
{
  Serial.print("PS1;");
}

void Command_PS1()
{
}

void Command_AI()
{
  Serial.print("AI0;");
}

void Command_AI0()
{
  Serial.print("AI0;");
}

void Command_XT1()
{
  Serial.print("XT1;");
}

void Command_RT1()
{
  Serial.print("RT1;");
}

void Command_RC()
{
  rit = 0;
  Serial.print("RC;");
}

void Command_FL0()
{
  Serial.print("FL0;");
}

void Command_GetMD()
{
  Serial.print("MD");
  Serial.print(mode + 1);
  Serial.print(';');
}

void Command_SetMD()
{
  mode = CATcmd[2] - '1';
  vfomode[vfosel % 2] = mode;
  change = true;
  si5351.iqmsa = 0;
}

void Command_RX()
{
#ifdef TX_ENABLE
  tx_disable();
  semi_qsk_timeout = 0;
#endif
  Serial.print("RX0;");
}

void Command_TX0()
{
#ifdef TX_ENABLE
  tx_enable();
#endif
}

void Command_TX1()
{
#ifdef TX_ENABLE
  tx_enable();
#endif
}

void Command_TX2()
{
#ifdef TX_ENABLE
  tx_enable();
#endif
}

void Command_RS()
{
  Serial.print("RS0;");
}

void Command_VX(char mode)
{
  char Catbuffer[16];
  sprintf(Catbuffer, "VX%c;", mode);
  Serial.print(Catbuffer);
}

#ifdef CAT_EXT
void Command_UK(char k1, char k2)
{
  cat_key = ((k1 - '0') << 4) | (k2 - '0');
  if(cat_key & 0x40){ encoder_val--; cat_key &= 0x3f; }
  if(cat_key & 0x80){ encoder_val++; cat_key &= 0x3f; }
  char Catbuffer[16];
  sprintf(Catbuffer, "UK%c%c;", k1, k2);
  Serial.print(Catbuffer);
}

void Command_UD()
{
  char Catbuffer[40];
  sprintf(Catbuffer, "UD%02u%s;", (lcd.curs) ? lcd.y * 16 + lcd.x : 16 * 2 + 1, lcd.text);
  Serial.print(Catbuffer);
}

void Command_UA(char en)
{
  char Catbuffer[16];
  sprintf(Catbuffer, "UA%01u;", (_cat_streaming = (en == '1')));
  Serial.print(Catbuffer);
  if(_cat_streaming){ Serial.print("US"); cat_streaming = true; }
}
#endif
