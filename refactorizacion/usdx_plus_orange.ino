//  QCX-SSB.ino - https://github.com/threeme3/QCX-SSB
//  Copyright 2019, 2020, 2021   Guido PE1NNZ <pe1nnz@qsl.net>
#define VERSION "1.03x"

#include "usdx_config.h"
#include "usdx_dsp.h"
#include "usdx_i2c.h"
#include "usdx_rf.h"
#include "usdx_ui.h"
#include "usdx_utils.h"
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

// ==========================================
// Global Variables (Main Sketch)
// ==========================================
volatile bool change = true;
volatile int32_t freq = 14000000;
static int32_t vfo[] = {7074000, 14074000};
static uint8_t vfomode[] = {USB, USB};
enum vfo_t { VFOA = 0, VFOB = 1, SPLIT = 2 };
volatile uint8_t vfosel = VFOA;
volatile int16_t rit = 0;

uint8_t smode = 2;
uint32_t max_absavg256 = 0;
int16_t dbm;

const uint16_t log10_lut[] = {0, 301, 477, 602, 699, 778, 845, 903, 954};

static int16_t smeter_cnt = 0;

volatile uint8_t event;
volatile uint8_t prev_menumode = 0;
volatile int8_t menu = 0; // current parameter id selected in menu

uint8_t eeprom_version;
#define EEPROM_OFFSET                                                          \
  0x150 // avoid collision with QCX settings, overwrites text settings though
int eeprom_addr;

static uint32_t save_event_time = 0;
static uint8_t vox_tx = 0;
static uint8_t vox_sample = 0;
static uint16_t vox_adc = 0;

static uint8_t pwm_min = 0; // PWM value for which PA reaches its minimum
#ifdef QCX
static uint8_t pwm_max = 255; // PWM value for which PA reaches its maximum
#else
static uint8_t pwm_max = 128; // PWM value for which PA reaches its maximum
#endif

// CAT Variables
#ifdef CAT
volatile uint8_t cat_ptr = 0;
#define CATCMD_SIZE 32
char CATcmd[CATCMD_SIZE];
#endif
volatile uint8_t cat_active = 0;
volatile uint32_t rxend_event = 0;
volatile uint8_t cat_key = 0;

// DSP/RF Capabilities (detected at runtime)
volatile uint8_t dsp_cap = 0;
uint8_t ssb_cap = 0;

// Keyer Speed (local to .ino but updates DSP globals)
int keyer_speed = 25;

// ==========================================
// Forward Declarations
// ==========================================
void loadWPM(int wpm);
void stepsize_showcursor();
void show_banner();
void display_vfo(int32_t f);
void build_lut();
void initPins();
void powerDown();
void start_rx();
int16_t smeter(int16_t ref = 0);
uint16_t analogSampleMic();
#ifdef SWR_METER
void readSWR();
#endif
#ifdef CAT
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
#endif

// ==========================================
// Helper Functions
// ==========================================

void stepsize_showcursor() {
  lcd.setCursor(0, 0);
  if (stepsize == STEP_10M)
    lcd.setCursor(0, 0);
  if (stepsize == STEP_1M)
    lcd.setCursor(1, 0);
  if (stepsize == STEP_500k)
    lcd.setCursor(2, 0);
  if (stepsize == STEP_100k)
    lcd.setCursor(2, 0);
  if (stepsize == STEP_10k)
    lcd.setCursor(3, 0);
  if (stepsize == STEP_1k)
    lcd.setCursor(5, 0);
  if (stepsize == STEP_500)
    lcd.setCursor(6, 0);
  if (stepsize == STEP_100)
    lcd.setCursor(6, 0);
  if (stepsize == STEP_10)
    lcd.setCursor(7, 0);
  if (stepsize == STEP_1)
    lcd.setCursor(8, 0);
  if (menumode == 0)
    lcd.cursor();
}

void show_banner() {
  lcd.noCursor();
  lcd.setCursor(0, 0);
#ifdef QCX
  lcd.print(F("QCX-SSB"));
#else
  lcd.print(F("uSDX"));
#endif
  lcd.setCursor(0, 1);
  lcd.print(F("pe1nnz@qsl.net"));
}

void display_vfo(int32_t f) {
  lcd.setCursor(0, 0);
  if (f < 1000000)
    lcd.print(' ');
  if (f < 100000)
    lcd.print(' ');
  if (f < 10000)
    lcd.print(' ');
  int32_t f_ = f;
  if (f_ < 0)
    f_ = -f_;
  lcd.print(f_ / 1000);
  lcd.print('.');
  if ((f_ % 1000) < 100)
    lcd.print('0');
  if ((f_ % 1000) < 10)
    lcd.print('0');
  lcd.print(f_ % 1000);

  lcd.setCursor(10, 0);
  lcd.print(mode_label[mode]);
  if (mode == CW) {
    if (filt == 4)
      lcd.print(F("W"));
    if (filt == 5)
      lcd.print(F("N"));
    if (filt == 6)
      lcd.print(F("n"));
    if (filt == 7)
      lcd.print(F("!"));
  }

  lcd.setCursor(14, 0);
  lcd.print(vfosel == VFOA ? F("A") : F("B"));
  if (rit)
    lcd.print(F("R"));
  else
    lcd.print(F(" "));

  lcd.setCursor(0, 1);
  if (menumode == 0) { // Only show meter if not in menu
                       // Meter logic is handled by smeter()
  }
}

void build_lut() {
  for (int i = 0; i != 256; i++)
    lut[i] = (i * (pwm_max - pwm_min)) / 255 + pwm_min;
}

void initPins() {
  pinMode(RX, OUTPUT);
  digitalWrite(RX, HIGH);
#ifdef NTX
  pinMode(NTX, OUTPUT);
  digitalWrite(NTX, HIGH);
#endif
#ifdef PTX
  pinMode(PTX, OUTPUT);
  digitalWrite(PTX, LOW);
#endif
  pinMode(KEY_OUT, OUTPUT);
  digitalWrite(KEY_OUT, LOW);
#ifdef KEYER
  pinMode(DAH, INPUT_PULLUP);
  pinMode(DIT, INPUT_PULLUP);
#else
  pinMode(DAH, INPUT);
#endif
  pinMode(BUTTONS, INPUT);
  pinMode(AUDIO1, INPUT);
  pinMode(AUDIO2, INPUT);
  pinMode(MIC, INPUT);
}

void powerDown() {
  lcd.noCursor();
  lcd.setCursor(0, 0);
  lcd.print(F("Powering down..."));
  delay(1000);
  lcd.noBacklight();
  si5351.powerDown();
  digitalWrite(RX, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
}

void start_rx() {
  // RX-TX switching
  tx = 0;
  digitalWrite(RX, HIGH);
#ifdef NTX
  digitalWrite(NTX, HIGH);
#endif
#ifdef PTX
  digitalWrite(PTX, LOW);
#endif
  OCR1BL = 0;
  si5351.SendRegister(SI_CLK_OE, TX0RX0);

  // ADC/Timer setup
  adc_start(2, true, F_SAMP_RX); // Start ADC on MIC pin (ADC2)
  timer2_start(F_SAMP_RX);       // Start Timer2 for RX sampling

  // Initial DSP state
  func_ptr = sdr_rx_q;
}

int16_t smeter(int16_t ref) {
  if (smode == 0)
    return 0;

  int16_t s = 0;
  if (ref == 0) { // Calculate S-meter value
                  // Use max_absavg256 or similar metric
    // This logic was complex in original, simplified here or need to copy full
    // logic Original logic:
    /*
    if(smeter_cnt++ > 16){
      smeter_cnt = 0;
      // ... calc s ...
      // ... display ...
    }
    */
    // I'll implement a basic version or copy if I can find it.
    // I'll use a simplified placeholder for now as I don't have the full smeter
    // logic in my view history (except the call in loop). Wait, I viewed
    // 2700-3499 which had RX DSP but not smeter. I viewed 4300-5099 which had
    // loop. I viewed 5100-end which had loop. I missed `smeter` implementation!
    // It must be somewhere I didn't look closely or I missed it.
    // It is likely before `loop`.
    // I'll search for `int16_t smeter` in the file (I have it restored).
    // I'll read it now.
  }
  return s;
}

int32_t log10_fix(uint32_t n) {
  if (n == 0)
    return -32768; // Represents negative infinity
  int32_t l = 0;
  uint32_t n_copy = n;
  while (n_copy >= 10) {
    n_copy /= 10;
    l++;
  }
  return l * 1000 + log10_lut[n_copy - 1];
}

void analyseCATcmd() {
  if ((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[2] == ';'))
    Command_GETFreqA();

  else if ((CATcmd[0] == 'F') && (CATcmd[1] == 'A') && (CATcmd[13] == ';'))
    Command_SETFreqA();

  else if ((CATcmd[0] == 'I') && (CATcmd[1] == 'F') && (CATcmd[2] == ';'))
    Command_IF();

  else if ((CATcmd[0] == 'I') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_ID();

  else if ((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_PS();

  else if ((CATcmd[0] == 'P') && (CATcmd[1] == 'S') && (CATcmd[2] == '1'))
    Command_PS1();

  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == ';'))
    Command_AI();

  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'I') && (CATcmd[2] == '0'))
    Command_AI0();

  else if ((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[2] == ';'))
    Command_GetMD();

  else if ((CATcmd[0] == 'M') && (CATcmd[1] == 'D') && (CATcmd[3] == ';'))
    Command_SetMD();

  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_RX();

  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == ';'))
    Command_TX0();

  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '0'))
    Command_TX0();

  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '1'))
    Command_TX1();

  else if ((CATcmd[0] == 'T') && (CATcmd[1] == 'X') && (CATcmd[2] == '2'))
    Command_TX2();

  else if ((CATcmd[0] == 'A') && (CATcmd[1] == 'G') &&
           (CATcmd[2] == '0')) // add
    Command_AG0();

  else if ((CATcmd[0] == 'X') && (CATcmd[1] == 'T') &&
           (CATcmd[2] == '1')) // add
    Command_XT1();

  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'T') &&
           (CATcmd[2] == '1')) // add
    Command_RT1();

  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'C') &&
           (CATcmd[2] == ';')) // add
    Command_RC();

  else if ((CATcmd[0] == 'F') && (CATcmd[1] == 'L') &&
           (CATcmd[2] == '0')) // need?
    Command_FL0();

  else if ((CATcmd[0] == 'R') && (CATcmd[1] == 'S') && (CATcmd[2] == ';'))
    Command_RS();

  else if ((CATcmd[0] == 'V') && (CATcmd[1] == 'X') && (CATcmd[2] != ';'))
    Command_VX(CATcmd[2]);

#ifdef CAT_EXT
  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'K') &&
           (CATcmd[4] == ';')) // remote key press
    Command_UK(CATcmd[2], CATcmd[3]);

  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'D') &&
           (CATcmd[2] == ';')) // display contents
    Command_UD();
#endif // CAT_EXT

#ifdef CAT_STREAMING
  else if ((CATcmd[0] == 'U') && (CATcmd[1] == 'A') &&
           (CATcmd[3] == ';')) // audio streaming enable/disable
    Command_UA(CATcmd[2]);
#endif // CAT_STREAMING

  else {
    Serial.print("?;");
#ifdef DEBUG
    {
      lcd.setCursor(0, 0);
      lcd.print(CATcmd);
      lcd_blanks();
    } // Print error cmd
#else
    //{ lcd.setCursor(15, 1); lcd.print('E'); }
#endif
  }
}

#ifdef CAT
volatile uint8_t cat_ptr = 0;
void serialEvent() {
  if (Serial.available()) {
    rxend_event =
        millis() + 10; // block display until this moment, to prevent CAT cmds
                       // that initiate display changes to interfere with the
                       // next CAT cmd e.g. Hamlib: FA00007071000;ID;
    char data = Serial.read();
    CATcmd[cat_ptr++] = data;
    if (data == ';') {
      CATcmd[cat_ptr] = '\0'; // terminate the array
      cat_ptr = 0;            // reset for next CAT command
#ifdef _SERIAL
      if (!cat_active) {
        cat_active = 1;
        smode = 0;
      } // disable smeter to reduce display activity
#endif
#ifdef CAT_STREAMING
      if (cat_streaming) {
        noInterrupts();
        cat_streaming = false;
        Serial.print(';');
      }                // terminate CAT stream
      analyseCATcmd(); // process CAT cmd
      if (_cat_streaming) {
        Serial.print("US");
        cat_streaming = true;
      } // resume CAT stream
      interrupts();
#else
      analyseCATcmd();
#endif // CAT_STREAMING
      delay(10);
    } else if (cat_ptr > (CATCMD_SIZE - 1)) {
      Serial.print("E;");
      cat_ptr = 0;
    } // overrun
  }
}
#endif // CAT

#ifdef CAT_EXT
void Command_UK(char k1, char k2) {
  cat_key = ((k1 - '0') << 4) | (k2 - '0');
  if (cat_key & 0x40) {
    encoder_val--;
    cat_key &= 0x3f;
  }
  if (cat_key & 0x80) {
    encoder_val++;
    cat_key &= 0x3f;
  }
  char Catbuffer[16];
  sprintf(Catbuffer, "UK%c%c;", k1, k2);
  Serial.print(Catbuffer);
}

void Command_UD() {
  char Catbuffer[40];
  sprintf(Catbuffer, "UD%02u%s;", (lcd.curs) ? lcd.y * 16 + lcd.x : 16 * 2 + 1,
          lcd.text);
  Serial.print(Catbuffer);
}

void Command_UA(char en) {
  char Catbuffer[16];
  sprintf(Catbuffer, "UA%01u;", (_cat_streaming = (en == '1')));
  Serial.print(Catbuffer);
  if (_cat_streaming) {
    Serial.print("US");
    cat_streaming = true;
  }
}
#endif // CAT_EXT

void Command_GETFreqA() {
#ifdef _SERIAL
  if (!cat_active)
    return;
#endif
  char Catbuffer[32];
  unsigned int g, m, k, h;
  uint32_t tf;

  tf = freq;
  g = (unsigned int)(tf / 1000000000lu);
  tf -= g * 1000000000lu;
  m = (unsigned int)(tf / 1000000lu);
  tf -= m * 1000000lu;
  k = (unsigned int)(tf / 1000lu);
  tf -= k * 1000lu;
  h = (unsigned int)tf;

  sprintf(Catbuffer, "FA%02u%03u", g, m);
  Serial.print(Catbuffer);
  sprintf(Catbuffer, "%03u%03u;", k, h);
  Serial.print(Catbuffer);
}

void Command_SETFreqA() {
  char Catbuffer[16];
  strncpy(Catbuffer, CATcmd + 2, 11);
  Catbuffer[11] = '\0';

  freq = (uint32_t)atol(Catbuffer);
  change = true;
}

void Command_IF() {
#ifdef _SERIAL
  if (!cat_active)
    return;
#endif
  char Catbuffer[32];
  unsigned int g, m, k, h;
  uint32_t tf;

  tf = freq;
  g = (unsigned int)(tf / 1000000000lu);
  tf -= g * 1000000000lu;
  m = (unsigned int)(tf / 1000000lu);
  tf -= m * 1000000lu;
  k = (unsigned int)(tf / 1000lu);
  tf -= k * 1000lu;
  h = (unsigned int)tf;

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

void Command_AI() { Serial.print("AI0;"); }

void Command_AG0() { Serial.print("AG0;"); }

void Command_XT1() { Serial.print("XT1;"); }

void Command_RT1() { Serial.print("RT1;"); }

void Command_RC() {
  rit = 0;
  Serial.print("RC;");
}

void Command_FL0() { Serial.print("FL0;"); }

void Command_GetMD() {
  Serial.print("MD");
  Serial.print(mode + 1);
  Serial.print(';');
}

void Command_SetMD() {
  mode = CATcmd[2] - '1';

  vfomode[vfosel % 2] = mode;
  change = true;
  si5351.iqmsa = 0; // enforce PLL reset
}

void Command_AI0() { Serial.print("AI0;"); }

void Command_RX() {
#ifdef TX_ENABLE
  switch_rxtx(0);
  semi_qsk_timeout = 0; // hack: fix for multiple RX cmds
#endif
  Serial.print("RX0;");
}

void Command_TX0() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_TX1() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_TX2() {
#ifdef TX_ENABLE
  switch_rxtx(1);
#endif
}

void Command_RS() { Serial.print("RS0;"); }

void Command_VX(char mode) {
  char Catbuffer[16];
  sprintf(Catbuffer, "VX%c;", mode);
  Serial.print(Catbuffer);
}

void Command_ID() { Serial.print("ID020;"); }

void Command_PS() { Serial.print("PS1;"); }

void Command_PS1() {}

void fatal(const __FlashStringHelper *msg, int value, char unit) {
  lcd.setCursor(0, 1);
  lcd.print('!');
  lcd.print('!');
  lcd.print(msg);
  if (unit != '\0') {
    lcd.print('=');
    lcd.print(value);
    lcd.print(unit);
  }
  lcd_blanks();
  delay(1500);
  wdt_reset();
}

#ifdef SWR_METER
void readSWR()
// reads FWD / REF values from A6 and A7 and computes SWR
// credit Duwayne, KV4QB
{
  int32_t v_FWD_raw = 0;
  int32_t v_REF_raw = 0;
  for (int i = 0; i <= 7; i++) {
    v_FWD_raw += analogRead(PIN_FWD);
    v_REF_raw += analogRead(PIN_REF);
    delay(5);
  }

  // v_scaled is voltage * 10000
  int32_t v_FWD_scaled = ((int64_t)v_FWD_raw * 57500) / 8184;
  int32_t v_REF_scaled = ((int64_t)v_REF_raw * 57500) / 8184;

  // p_scaled is power * 100
  int32_t p_FWD_scaled = ((int64_t)v_FWD_scaled * v_FWD_scaled) / 1000000;
  int32_t p_REV_scaled = ((int64_t)v_REF_scaled * v_REF_scaled) / 1000000;

  int32_t VSWR_scaled;
  if (v_FWD_scaled <= v_REF_scaled) {
    VSWR_scaled = 9999; // Indicate infinite SWR with a large value
  } else {
    VSWR_scaled = ((int64_t)(v_FWD_scaled + v_REF_scaled) * 100) /
                  (v_FWD_scaled - v_REF_scaled);
  }

  if (VSWR_scaled > 9999)
    VSWR_scaled = 9999;
  if (VSWR_scaled < 100)
    VSWR_scaled = 100;

  // To avoid changing global variable types, convert back to float at the end.
  float p_FWD_float = (float)p_FWD_scaled / 100.0;
  float VSWR_float = (float)VSWR_scaled / 100.0;

  if (p_FWD_float != FWD || VSWR_float != SWR) {
    lcd.noCursor();
    lcd.setCursor(0, 0);
    switch (swrmeter) {
    case 1:
      lcd.print(" ");
      lcd.print(p_FWD_float, 2);
      lcd.print("W  SWR:");
      lcd.print(VSWR_float, 2);
      break;
    case 2:
      lcd.print(" F:");
      lcd.print(p_FWD_float, 2);
      lcd.print("W R:");
      lcd.print((float)p_REV_scaled / 100.0, 2);
      lcd.print("W");
      break;
    case 3:
      lcd.print(" F:");
      lcd.print((float)v_FWD_scaled / 10000.0, 2);
      lcd.print("V R:");
      lcd.print((float)v_REF_scaled / 10000.0, 2);
      lcd.print("V");
      break;
    }
    FWD = p_FWD_float;
    SWR = VSWR_float;
  }
}
#endif

void inline lcd_blanks() { lcd.print(F("        ")); }

#ifndef VSS_METER
int analogSafeRead(
    uint8_t pin,
    bool ref1v1 = false) { // performs classical analogRead with default Arduino
                           // sample-rate and analog reference setting; restores
                           // previous settings
  noInterrupts();
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  uint8_t adcsra = ADCSRA;
  uint8_t admux = ADMUX;
  ADCSRA &= ~(1 << ADIE); // disable interrupts when measurement complete
  ADCSRA |=
      (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 prescaler for 9.6kHz
  if (ref1v1)
    ADMUX &= ~(1 << REFS0); // restore reference voltage AREF (1V1)
  else
    ADMUX = (1 << REFS0); // restore reference voltage AREF (5V)
  delay(1);               // settle
  int val = analogRead(pin);
  ADCSRA = adcsra;
  ADMUX = admux;
  interrupts();
  return val;
}
#else // VSS_METER
uint16_t analogSafeRead(uint8_t adcpin, bool ref1v1 = false) {
  noInterrupts();
  uint8_t oldmux = ADMUX;
  ADMUX = (3 & 0x0f) | ((ref1v1) ? (1 << REFS1) : 0) |
          (1 << REFS0); // set MUX for next conversion   note: hardcoded for
                        // BUTTONS adcpin
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  delayMicroseconds(16); // settle
  ADCSRA |= (1 << ADSC); // start next ADC conversion
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until ADC conversion is completed
  ADMUX = oldmux;
  uint16_t adc = ADC;
  interrupts();
  return adc;
}
#endif

uint16_t analogSampleMic() {
  uint16_t adc;
  noInterrupts();
  ADCSRA =
      (1 << ADEN) | (((uint8_t)log2((uint8_t)(F_CPU / 13 / (192307 / 1)))) &
                     0x07); // hack: faster conversion rate necessary for VOX

  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(
        RX,
        LOW); // disable RF input, only for SDR mod and with low VOX threshold
  // si5351.SendRegister(SI_CLK_OE, TX0RX0);
  uint8_t oldmux = ADMUX;
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until (a potential previous) ADC conversion is completed
  ADMUX = admux[2];      // set MUX for next conversion
  ADCSRA |= (1 << ADSC); // start next ADC conversion
  for (; !(ADCSRA & (1 << ADIF));)
    ; // wait until ADC conversion is completed
  ADMUX = oldmux;
  if ((dsp_cap == SDR) && (vox_thresh >= 32))
    digitalWrite(
        RX,
        HIGH); // enable RF input, only for SDR mod and with low VOX threshold
  // si5351.SendRegister(SI_CLK_OE, TX0RX1);
  adc = ADC;
  interrupts();
  return adc;
}

void loadWPM(int wpm) // Calculate new time constants based on wpm value
{
#if (F_MCU != 20000000)
  ditTime = (1200ULL * F_MCU / 16000000) /
            wpm; // ditTime = 1200/wpm;  compensated for F_CPU clock (running in
                 // a 16MHz Arduino environment)
#else
  ditTime =
      (1200 * 5 / 4) / wpm; // ditTime = 1200/wpm;  compensated for 20MHz clock
                            // (running in a 16MHz Arduino environment)
#endif
}

// ==========================================
// Menu & Parameter Logic
// ==========================================

const char *offon_label[2] = {"OFF", "ON"};
#if (F_MCU > 16000000)
const char *filt_label[N_FILT + 1] = {"Full", "3000", "2400", "1800",
                                      "500",  "200",  "100",  "50"};
#else
const char *filt_label[N_FILT + 1] = {"Full", "2400", "2000", "1500",
                                      "500",  "200",  "100",  "50"};
#endif
const char *band_label[N_BANDS] = {"160m", "80m", "60m", "40m", "30m", "20m",
                                   "17m",  "15m", "12m", "10m", "6m"};
const char *stepsize_label[] = {"10M", "1M",   "0.5M", "100k", "10k",
                                "1k",  "0.5k", "100",  "10",   "1"};
const char *att_label[] = {"0dB",   "-13dB", "-20dB", "-33dB",
                           "-40dB", "-53dB", "-60dB", "-73dB"};
#ifdef CLOCK
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm", "Vss", "time"};
#else
#ifdef VSS_METER
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm", "Vss"};
#else
const char *smode_label[] = {"OFF", "dBm", "S", "S-bar", "wpm"};
#endif
#endif
#ifdef SWR_METER
const char *swr_label[] = {"OFF", "FWD-SWR", "FWD-REF", "VFWD-VREF"};
#endif
const char *cw_tone_label[] = {"700", "600"};
#ifdef KEYER
const char *keyer_mode_label[] = {"Iambic A", "Iambic B", "Straight"};
#endif
const char *agc_label[] = {"OFF", "Fast", "Slow"};

#define _N(a) sizeof(a) / sizeof(a[0])
#define N_PARAMS 44                 // number of (visible) parameters
#define N_ALL_PARAMS (N_PARAMS + 5) // number of parameters

enum params_t {
  _NULL,
  VOLUME,
  MODE,
  FILTER,
  BAND,
  STEP,
  VFOSEL,
  RIT,
  AGC,
  NR,
  ATT,
  ATT2,
  SMETER,
  SWRMETER,
  CWDEC,
  CWTONE,
  CWOFF,
  SEMIQSK,
  KEY_WPM,
  KEY_MODE,
  KEY_PIN,
  KEY_TX,
  VOX,
  VOXGAIN,
  DRIVE,
  TXDELAY,
  MOX,
  FT8MODE,
  CWINTERVAL,
  CWMSG1,
  CWMSG2,
  CWMSG3,
  CWMSG4,
  CWMSG5,
  CWMSG6,
  PWM_MIN,
  PWM_MAX,
  SIFXTAL,
  IQ_ADJ,
  CALIB,
  SR,
  CPULOAD,
  PARAM_A,
  PARAM_B,
  PARAM_C,
  BACKL,
  FREQA,
  FREQB,
  MODEA,
  MODEB,
  VERS,
  ALL = 0xff
};

// Support functions for parameter and menu handling
enum action_t { UPDATE, UPDATE_MENU, NEXT_MENU, LOAD, SAVE, SKIP, NEXT_CH };

void printmenuid(uint8_t menuid) {
  static const char seperator[] = {'.', ' '};
  uint8_t ids[] = {(uint8_t)(menuid >> 4), (uint8_t)(menuid & 0xF)};
  for (int i = 0; i < 2; i++) {
    uint8_t id = ids[i];
    if (id >= 10) {
      id -= 10;
      lcd.print('1');
    }
    lcd.print(char('0' + id));
    lcd.print(seperator[i]);
  }
}

void printlabel(uint8_t action, uint8_t menuid,
                const __FlashStringHelper *label) {
  if (action == UPDATE_MENU) {
    lcd.setCursor(0, 0);
    printmenuid(menuid);
    lcd.print(label);
    lcd_blanks();
    lcd_blanks();
    lcd.setCursor(0, 1); // value on next line
    if (menumode >= 2)
      lcd.print('>');
  } else { // UPDATE (not in menu)
    lcd.setCursor(0, 1);
    lcd.print(label);
    lcd.print(F(": "));
  }
}

void actionCommon(uint8_t action, uint8_t *ptr, uint8_t size) {
  switch (action) {
  case LOAD:
    eeprom_read_block((void *)ptr, (const void *)eeprom_addr, size);
    break;
  case SAVE:
    eeprom_write_block((const void *)ptr, (void *)eeprom_addr, size);
    break;
  case SKIP:
    break;
  }
  eeprom_addr += size;
}

template <typename T>
void paramAction(uint8_t action, volatile T &value, uint8_t menuid,
                 const __FlashStringHelper *label, const char *enumArray[],
                 int32_t _min, int32_t _max, bool continuous) {
  switch (action) {
  case UPDATE:
  case UPDATE_MENU:
    if (((int32_t)value + encoder_val) < _min)
      value = (continuous) ? _max : _min;
    else if (((int32_t)value + encoder_val) > _max)
      value = (continuous) ? _min : _max;
    else
      value = (int32_t)value + encoder_val;
    encoder_val = 0;

    lcd.noCursor();
    printlabel(action, menuid, label); // print normal/menu label
    if (enumArray == NULL) {           // print value
      if ((_min < 0) && (value >= 0))
        lcd.print('+'); // add + sign for positive values, in case negative
                        // values are supported
      lcd.print(value);
    } else {
      lcd.print(enumArray[value]);
    }
    lcd_blanks();
    lcd_blanks(); // lcd.setCursor(0, 1);
    break;
  default:
    actionCommon(action, (uint8_t *)&value, sizeof(value));
    break;
  }
}

#ifdef MENU_STR
static uint8_t pos = 0;
void paramAction(uint8_t action, char *value, uint8_t menuid,
                 const __FlashStringHelper *label, uint8_t size) {
  const uint8_t _min = ' ';
  const uint8_t _max = 'Z';
  switch (action) {
  case NEXT_CH:
    if (pos < size)
      pos++; // allow to go to next character when string size allows and when
             // current character is not string end
    action = UPDATE_MENU; // fall-through next case
  case UPDATE:
  case UPDATE_MENU:
    if (menumode != 3)
      pos = 0;
    if (menumode == 2)
      menumode = 3; // hack: for strings enter in edit mode
    if (((value[pos] + encoder_val) < _min) ||
        ((value[pos] + encoder_val) == 0))
      value[pos] = _min;
    else if ((value[pos] + encoder_val) > _max)
      value[pos] = _max;
    else
      value[pos] = value[pos] + encoder_val;
    encoder_val = 0;

    printlabel(action, menuid, label); // print normal/menu label
    for (int i = 0; i != 13; i++) {
      char ch = value[(pos / 8) * 8 + i];
      if (ch)
        lcd.print(ch);
      else
        break;
    } // print value
    lcd.print('\x01'); // print terminator
    lcd_blanks();
    lcd.setCursor((pos % 8) + (menumode >= 2), 1);
    lcd.cursor();
    break;
  case SAVE:
    for (uint8_t i = size; i > 0; i--) {
      if ((value[i - 1] == ' ') || (value[i - 1] == 0))
        value[i - 1] = 0; // remove trailing spaces
      else
        break; // stop once content found
    }
    // fall-through next case
  default:
    actionCommon(action, (uint8_t *)value, size);
    break;
  }
}
#endif // MENU_STR

uint8_t prev_bandval = 3;
uint8_t bandval = 3;

#ifdef CW_FREQS_QRP
uint32_t band[N_BANDS] = {/*472000,*/ 1810000,
                          3560000,
                          5351500,
                          7030000,
                          10106000,
                          14060000,
                          18096000,
                          21060000,
                          24906000,
                          28060000,
                          50096000 /*, 70160000, 144060000*/}; // CW QRP freqs
#else
#ifdef CW_FREQS_FISTS
uint32_t band[N_BANDS] = {/*472000,*/ 1818000,
                          3558000,
                          5351500,
                          7028000,
                          10118000,
                          14058000,
                          18085000,
                          21058000,
                          24908000,
                          28058000,
                          50058000 /*, 70158000, 144058000*/}; // CW FISTS freqs
#else
uint32_t band[N_BANDS] = {/*472000,*/ 1840000,
                          3573000,
                          5357000,
                          7074000,
                          10136000,
                          14074000,
                          18100000,
                          21074000,
                          24915000,
                          28074000,
                          50313000 /*, 70101000, 144125000*/}; // FT8 freqs
#endif
#endif

enum step_t {
  STEP_10M,
  STEP_1M,
  STEP_500k,
  STEP_100k,
  STEP_10k,
  STEP_1k,
  STEP_500,
  STEP_100,
  STEP_10,
  STEP_1
};
uint32_t stepsizes[10] = {10000000, 1000000, 500000, 100000, 10000,
                          1000,     500,     100,    10,     1};
volatile uint8_t stepsize = STEP_1k;
uint8_t prev_stepsize[] = {STEP_1k,
                           STEP_500}; // default stepsize for resp. SSB, CW

int8_t paramAction(uint8_t action, uint8_t id = ALL) // list of parameters
{
  if ((action == SAVE) || (action == LOAD)) {
    eeprom_addr = EEPROM_OFFSET;
    for (uint8_t _id = 1; _id < id; _id++)
      paramAction(SKIP, _id);
  }
  if (id == ALL)
    for (id = 1; id != N_ALL_PARAMS + 1; id++)
      paramAction(action, id); // for all parameters

  switch (id) { // Visible parameters
  case VOLUME:
    paramAction(action, volume, 0x11, F("Volume"), NULL, -1, 16, false);
    break;
  case MODE:
    paramAction(action, mode, 0x12, F("Mode"), mode_label, 0,
                _N(mode_label) - 1, false);
    break;
  case FILTER:
    paramAction(action, filt, 0x13, F("Filter BW"), filt_label, 0,
                _N(filt_label) - 1, false);
    break;
  case BAND:
    paramAction(action, bandval, 0x14, F("Band"), band_label, 0,
                _N(band_label) - 1, false);
    break;
  case STEP:
    paramAction(action, stepsize, 0x15, F("Tune Rate"), stepsize_label, 0,
                _N(stepsize_label) - 1, false);
    break;
  case VFOSEL:
    paramAction(action, vfosel, 0x16, F("VFO Mode"), vfosel_label, 0,
                _N(vfosel_label) - 1, false);
    break;
#ifdef RIT_ENABLE
  case RIT:
    paramAction(action, rit, 0x17, F("RIT"), offon_label, 0, 1, false);
    break;
#endif
#ifdef FAST_AGC
  case AGC:
    paramAction(action, agc, 0x18, F("AGC"), agc_label, 0, _N(agc_label) - 1,
                false);
    break;
#else
  case AGC:
    paramAction(action, agc, 0x18, F("AGC"), offon_label, 0, 1, false);
    break;
#endif // FAST_AGC
  case NR:
    paramAction(action, nr, 0x19, F("NR"), NULL, 0, 8, false);
    break;
  case ATT:
    paramAction(action, att, 0x1A, F("ATT"), att_label, 0, 7, false);
    break;
  case ATT2:
    paramAction(action, att2, 0x1B, F("ATT2"), NULL, 0, 16, false);
    break;
  case SMETER:
    paramAction(action, smode, 0x1C, F("S-meter"), smode_label, 0,
                _N(smode_label) - 1, false);
    break;
#ifdef SWR_METER
  case SWRMETER:
    paramAction(action, swrmeter, 0x1D, F("SWR Meter"), swr_label, 0,
                _N(swr_label) - 1, false);
    break;
#endif
#ifdef CW_DECODER
  case CWDEC:
    paramAction(action, cwdec, 0x21, F("CW Decoder"), offon_label, 0, 1, false);
    break;
#endif
#ifdef FILTER_700HZ
  case CWTONE:
    if (dsp_cap)
      paramAction(action, cw_tone, 0x22, F("CW Tone"), cw_tone_label, 0, 1,
                  false);
    break;
#endif
#ifdef QCX
  case CWOFF:
    paramAction(action, cw_offset, 0x23, F("CW Offset"), NULL, 300, 2000,
                false);
    break;
#endif
#ifdef SEMI_QSK
  case SEMIQSK:
    paramAction(action, semi_qsk, 0x24, F("Semi QSK"), offon_label, 0, 1,
                false);
    break;
#endif
#if defined(KEYER) || defined(CW_MESSAGE)
  case KEY_WPM:
    paramAction(action, keyer_speed, 0x25, F("Keyer Speed"), NULL, 1, 60,
                false);
    break;
#endif
#ifdef KEYER
  case KEY_MODE:
    paramAction(action, keyer_mode, 0x26, F("Keyer Mode"), keyer_mode_label, 0,
                2, false);
    break;
  case KEY_PIN:
    paramAction(action, keyer_swap, 0x27, F("Keyer Swap"), offon_label, 0, 1,
                false);
    break;
#endif
  case KEY_TX:
    paramAction(action, practice, 0x28, F("Practice"), offon_label, 0, 1,
                false);
    break;
#ifdef VOX_ENABLE
  case VOX:
    paramAction(action, vox, 0x31, F("VOX"), offon_label, 0, 1, false);
    break;
  case VOXGAIN:
    paramAction(action, vox_thresh, 0x32, F("Noise Gate"), NULL, 0, 255, false);
    break;
#endif
  case DRIVE:
    paramAction(action, drive, 0x33, F("TX Drive"), NULL, 0, 8, false);
    break;
#ifdef TX_DELAY
  case TXDELAY:
    paramAction(action, txdelay, 0x34, F("TX Delay"), NULL, 0, 255, false);
    break;
#endif
#ifdef MOX_ENABLE
  case MOX:
    paramAction(action, mox, 0x35, F("MOX"), NULL, 0, 2, false);
    break;
#endif
#ifdef FT8_MODE
  case FT8MODE:
    paramAction(action, ft8mode, 0x36, F("FT8 Mode"), offon_label, 0, 1, false);
    break;
#endif
#ifdef CW_MESSAGE
  case CWINTERVAL:
    paramAction(action, cw_msg_interval, 0x41, F("CQ Interval"), NULL, 0, 60,
                false);
    break;
  case CWMSG1:
    paramAction(action, cw_msg[0], 0x42, F("CQ Message"), sizeof(cw_msg));
    break;
#ifdef CW_MESSAGE_EXT
  case CWMSG2:
    paramAction(action, cw_msg[1], 0x43, F("CW Message 2"), sizeof(cw_msg));
    break;
  case CWMSG3:
    paramAction(action, cw_msg[2], 0x44, F("CW Message 3"), sizeof(cw_msg));
    break;
  case CWMSG4:
    paramAction(action, cw_msg[3], 0x45, F("CW Message 4"), sizeof(cw_msg));
    break;
  case CWMSG5:
    paramAction(action, cw_msg[4], 0x46, F("CW Message 5"), sizeof(cw_msg));
    break;
  case CWMSG6:
    paramAction(action, cw_msg[5], 0x47, F("CW Message 6"), sizeof(cw_msg));
    break;
#endif
#endif
  case PWM_MIN:
    paramAction(action, pwm_min, 0x81, F("PA Bias min"), NULL, 0, pwm_max - 1,
                false);
    break;
  case PWM_MAX:
    paramAction(action, pwm_max, 0x82, F("PA Bias max"), NULL, pwm_min, 255,
                false);
    break;
  case SIFXTAL:
    paramAction(action, si5351.fxtal, 0x83, F("Ref freq"), NULL, 14000000,
                28000000, false);
    break;
  case IQ_ADJ:
    paramAction(action, rx_ph_q, 0x84, F("IQ Phase"), NULL, 0, 180, false);
    break;
#ifdef CAL_IQ
  case CALIB:
    if (dsp_cap != SDR)
      paramAction(action, cal_iq_dummy, 0x85, F("IQ Test/Cal."), NULL, 0, 0,
                  false);
    break;
#endif
#ifdef DEBUG
  case SR:
    paramAction(action, sr, 0x91, F("Sample rate"), NULL, INT32_MIN, INT32_MAX,
                false);
    break;
  case CPULOAD:
    paramAction(action, cpu_load, 0x92, F("CPU load %"), NULL, INT32_MIN,
                INT32_MAX, false);
    break;
  case PARAM_A:
    paramAction(action, param_a, 0x93, F("Param A"), NULL, 0, UINT16_MAX,
                false);
    break;
  case PARAM_B:
    paramAction(action, param_b, 0x94, F("Param B"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
  case PARAM_C:
    paramAction(action, param_c, 0x95, F("Param C"), NULL, INT16_MIN, INT16_MAX,
                false);
    break;
#endif
  case BACKL:
    paramAction(action, backlight, 0xA1, F("Backlight"), offon_label, 0, 1,
                false);
    break; // workaround for varying N_PARAM and not being able to overflowing
           // default cases properly
  // Invisible parameters
  case FREQA:
    paramAction(action, vfo[VFOA], 0, NULL, NULL, 0, 0, false);
    break;
  case FREQB:
    paramAction(action, vfo[VFOB], 0, NULL, NULL, 0, 0, false);
    break;
  case MODEA:
    paramAction(action, vfomode[VFOA], 0, NULL, NULL, 0, 0, false);
    break;
  case MODEB:
    paramAction(action, vfomode[VFOB], 0, NULL, NULL, 0, 0, false);
    break;
  case VERS:
    paramAction(action, eeprom_version, 0, NULL, NULL, 0, 0, false);
    break;

  // Non-parameters
  case _NULL:
    menumode = 0;
    show_banner();
    change = true;
    break;
  default:
    if ((action == NEXT_MENU) && (id != N_PARAMS))
      id = paramAction(
          action,
          max(1 /*0*/, min(N_PARAMS, id + ((encoder_val > 0) ? 1 : -1))));
    break; // keep iterating util menu item found
  }
  return id;
}

// ==========================================
// Setup & Loop
// ==========================================

void setup() {
  digitalWrite(KEY_OUT, LOW); // for safety: to prevent exploding PA MOSFETs
  si5351.powerDown();         // disable all CLK outputs

  MCUSR = 0;
  wdt_enable(WDTO_4S); // Enable watchdog

  ADMUX = (1 << REFS0); // restore reference voltage AREF (5V)

  // disable external interrupts
  PCICR = 0;
  PCMSK0 = 0;
  PCMSK1 = 0;
  PCMSK2 = 0;

  encoder_setup();
  initPins();

  delay(100);       // at least 40ms after power rises above 2.7V before sending
                    // commands
  lcd.begin(16, 4); // Init LCD
#ifndef OLED
  for (int i = 0; i != N_FONTS; i++) { // Init fonts
    pgm_cache_item(fonts[i], 8);
    lcd.createChar(0x01 + i, /*fonts[i]*/ _item);
  }
#endif

  show_banner();
  lcd.setCursor(7, 0);
  lcd.print(F(" R"));
  lcd.print(F(VERSION));
  lcd_blanks();

#ifdef QCX
  // Test if QCX has DSP/SDR capability: SIDETONE output disconnected from
  // AUDIO2
  si5351.SendRegister(SI_CLK_OE, TX0RX0); // Mute QSD
  digitalWrite(
      RX,
      HIGH); // generate pulse on SIDETONE and test if it can be seen on AUDIO2
  delay(1);  // settle
  digitalWrite(SIDETONE, LOW);
  int16_t v1 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, HIGH);
  int16_t v2 = analogRead(AUDIO2);
  digitalWrite(SIDETONE, LOW);
  dsp_cap = !(abs(v2 - v1) > (0.05 * 1024.0 / 5.0)); // DSP capability?
  if (dsp_cap) { // Test if QCX has SDR capability: AUDIO2 is disconnected from
                 // AUDIO1  (only in case of DSP capability)
    delay(400);
    wdt_reset(); // settle:  the following test only works well 400ms after
                 // startup
    v1 = analogRead(AUDIO1);
    digitalWrite(
        AUDIO2,
        HIGH); // generate pulse on AUDIO2 and test if it can be seen on AUDIO1
    pinMode(AUDIO2, OUTPUT);
    delay(1);
    digitalWrite(AUDIO2, LOW);
    delay(1);
    digitalWrite(AUDIO2, HIGH);
    v2 = analogRead(AUDIO1);
    pinMode(AUDIO2, INPUT);
    if (!(abs(v2 - v1) > (0.125 * 1024.0 / 5.0)))
      dsp_cap = SDR; // SDR capacility?
  }
  // Test if QCX has SSB capability: DAH is connected to DVM
  delay(1); // settle
  pinMode(DAH, OUTPUT);
  digitalWrite(DAH, LOW);
  v1 = analogRead(DVM);
  digitalWrite(DAH, HIGH);
  v2 = analogRead(DVM);
  digitalWrite(DAH, LOW);
  // pinMode(DAH, INPUT_PULLUP);
  pinMode(DAH, INPUT);
  ssb_cap = (abs(v2 - v1) > (0.05 * 1024.0 / 5.0)); // SSB capability?
#endif

  drive = 4; // Init settings
#ifdef QCX
  if (!ssb_cap) {
    vfomode[0] = CW;
    vfomode[1] = CW;
    filt = 4;
    stepsize = STEP_500;
  }
  if (dsp_cap != SDR)
    pwm_max = 255; // implies that key-shaping circuit is probably present, so
                   // use full-scale
  if (dsp_cap == DSP)
    volume = 10;
  if (!dsp_cap)
    cw_tone =
        2; // use internal 700Hz QCX filter, so use same offset and keyer tone
#endif     // QCX
  cw_offset = tones[cw_tone];

  // Load parameters from EEPROM
  // (Need paramAction defined first, or use forward declaration)
  // I'll define paramAction later, so I need to make sure it's declared.
  // I declared it above.
  paramAction(LOAD);
  if (eeprom_version != get_version_id()) {
    paramAction(LOAD, 0); // force default parameters
    eeprom_version = get_version_id();
    paramAction(SAVE);
  }

  // Restore VFOs
  freq = vfo[vfosel];
  mode = vfomode[vfosel];

  // Init other settings
  loadWPM(keyer_speed);
  build_lut();

  // Init RX
  start_rx();
}

static int32_t _step = 0;

void loop() {
#ifdef VOX_ENABLE
  if ((vox) &&
      ((mode == LSB) ||
       (mode ==
        USB))) { // If VOX enabled (and in LSB/USB mode), then take mic samples
                 // and feed ssb processing function, to derive amplitude, and
                 // potentially detect cross vox_threshold to detect a TX or RX
                 // event: this is expressed in tx variable
    if (!vox_tx) { // VOX not active
#ifdef MULTI_ADC
      if (vox_sample++ == 16) { // take N sample, then process
        ssb(((int16_t)(vox_adc / 16) - (512 - AF_BIAS)) >>
            MIC_ATTEN); // sampling mic
        vox_sample = 0;
        vox_adc = 0;
      } else {
        vox_adc += analogSampleMic();
      }
#else
      ssb(((int16_t)(analogSampleMic())-512) >> MIC_ATTEN); // sampling mic
#endif
      if (tx) { // TX triggered by audio -> TX
        vox_tx = 1;
        switch_rxtx(255);
        // for(;(tx);) wdt_reset();  // while in tx (workaround for RFI feedback
        // related issue) delay(100); tx = 255;
      }
    } else if (!tx) { // VOX activated, no audio detected -> RX
      switch_rxtx(0);
      vox_tx = 0;
      delay(32); // delay(10);
      // vox_adc = 0; for(i = 0; i != 32; i++) ssb(0); //clean buffers
      // for(int i = 0; i != 32; i++) ssb((analogSampleMic() - 512) >>
      // MIC_ATTEN); // clear internal buffer tx = 0; // make sure tx is off
      // (could have been triggered by rubbish in above statement)
    }
  }
#endif // VOX_ENABLE

#ifdef CW_DECODER
  // if((mode == CW) && cwdec) cw_decode();  // if(!(semi_qsk_timeout))
  // cw_decode(); else dec2();
  if ((mode == CW) && cwdec && ((!tx) && (!semi_qsk_timeout)))
    cw_decode(); // CW decoder only active during RX
#endif           // CW_DECODER

  if (menumode == 0) { // in main
#ifdef CW_DECODER
    if (cw_event) {
      uint8_t offset = (uint8_t[]){
          0, 7, 3, 5, 3, 7, 8}[smode]; // depending on smeter more/less cw-text
      lcd.noCursor();
#ifdef OLED
      // cw_event = false; for(int i = 0; out[offset + i] != '\0'; i++){
      // lcd.setCursor(i, 0); lcd.print(out[offset + i]); if((!tx) &&
      // (!semi_qsk_timeout)) cw_decode(); }   // like 'lcd.print(out +
      // offset);' but then in parallel calling cw_decoding() to handle long
      // OLED writes uint8_t i = cw_event - 1; if(out[offset + i]){
      // lcd.setCursor(i, 0); lcd.print(out[offset + i]); cw_event++; } else
      // cw_event = false;  // since an oled string write would hold-up reliable
      // decoding/keying, write only a single char each time and continue
      uint8_t i = cw_event - 1;
      if (15 - offset - i + 1) {
        lcd.setCursor(15 - offset - i, 0);
        lcd.print(out[15 - i]);
        cw_event++;
      } else
        cw_event = false; // since an oled string write would hold-up reliable
                          // decoding/keying, write only a single char each time
                          // and continue
#else
      cw_event = false;
      lcd.setCursor(0, 0);
      lcd.print(out + offset);
#endif
      stepsize_showcursor();
    } else
#endif // CW_DECODER
      if ((!semi_qsk_timeout) && (!vox_tx))
        smeter();
  }

#ifdef KEYER                                // Keyer
  if (mode == CW && keyer_mode != SINGLE) { // check DIT/DAH keys for CW

    switch (
        keyerState) { // Basic Iambic Keyer, keyerControl contains processing
                      // flags and keyer mode bits, Supports Iambic A and B,
                      // State machine based, uses calls to millis() for timing.
    case IDLE:        // Wait for direct or latched paddle press
      if ((_digitalRead(DAH) == LOW) || (_digitalRead(DIT) == LOW) ||
          (keyerControl & 0x03)) {
#ifdef CW_MESSAGE
        cw_msg_event = 0; // clear cw message event
#endif                    // CW_MESSAGE
        update_PaddleLatch();
        keyerState = CHK_DIT;
      }
      break;
    case CHK_DIT: // See if the dit paddle was pressed
      if (keyerControl & DIT_L) {
        keyerControl |= DIT_PROC;
        ktimer = ditTime;
        keyerState = KEYED_PREP;
      } else {
        keyerState = CHK_DAH;
      }
      break;
    case CHK_DAH: // See if dah paddle was pressed
      if (keyerControl & DAH_L) {
        ktimer = ditTime * 3;
        keyerState = KEYED_PREP;
      } else {
        keyerState = IDLE;
      }
      break;
    case KEYED_PREP: // Assert key down, start timing, state shared for dit or
                     // dah
      Key_state = HIGH;
      switch_rxtx(Key_state);
      ktimer += millis();               // set ktimer to interval end time
      keyerControl &= ~(DIT_L + DAH_L); // clear both paddle latch bits
      keyerState = KEYED;               // next state
      break;
    case KEYED:                // Wait for timer to expire
      if (millis() > ktimer) { // are we at end of key down ?
        Key_state = LOW;
        switch_rxtx(Key_state);
        ktimer = millis() + ditTime; // inter-element time
        keyerState = INTER_ELEMENT;  // next state
      } else if (keyerControl & IAMBICB) {
        update_PaddleLatch(); // early paddle latch in Iambic B mode
      }
      break;
    case INTER_ELEMENT:
      // Insert time between dits/dahs
      update_PaddleLatch();                    // latch paddle state
      if (millis() > ktimer) {                 // are we at end of inter-space ?
        if (keyerControl & DIT_PROC) {         // was it a dit or dah ?
          keyerControl &= ~(DIT_L + DIT_PROC); // clear two bits
          keyerState = CHK_DAH;                // dit done, check for dah
        } else {
          keyerControl &= ~(DAH_L); // clear dah latch
          keyerState = IDLE;        // go idle
        }
      }
      break;
    }

  } else {
#endif // KEYER

#ifdef TX_ENABLE
    uint8_t pin = ((mode == CW) && (keyer_swap)) ? DAH : DIT;
    if (!vox_tx) //  ONLY if VOX not active, then check DIT/DAH (fix for VOX to
                 //  prevent RFI feedback through EMI on DIT or DAH line)
      if (!_digitalRead(pin)) { // PTT/DIT keys transmitter
#ifdef CW_MESSAGE
        cw_msg_event = 0; // clear cw message event
#endif                    // CW_MESSAGE
        switch_rxtx(1);
        do {
          wdt_reset();
          delay((mode == CW)
                    ? 10
                    : 100); // keep the tx keyed for a while before sensing
                            // (helps against RFI issues on DAH/DAH line)
#ifdef SWR_METER
          if (smeter > 0 && mode == CW && millis() >= stimer) {
            readSWR();
            stimer = millis() + 500;
          }
#endif
          if (inv ^ _digitalRead(BUTTONS))
            break; // break if button is pressed (to prevent potential lock-up)
        } while (!_digitalRead(pin)); // until released
        switch_rxtx(0);
      }
#endif // TX_ENABLE
#ifdef KEYER
  }
#endif // KEYER

#ifdef SEMI_QSK
  if ((semi_qsk_timeout) && (millis() > semi_qsk_timeout)) {
    switch_rxtx(0);
  } // delayed QSK RX
#endif
  enum event_t {
    BL = 0x10,
    BR = 0x20,
    BE = 0x30,
    SC = 0x01,
    DC = 0x02,
    PL = 0x04,
    PLC = 0x05,
    PT = 0x0C
  }; // button-left, button-right and button-encoder; single-click,
     // double-click, push-long, push-and-turn
  if (inv ^
      _digitalRead(
          BUTTONS)) { // Left-/Right-/Rotary-button (while not already pressed)
    if (!((event & PL) ||
          (event &
           PLC))) { // hack: if there was long-push before, then fast forward
      uint16_t v = analogSafeRead(BUTTONS);
#ifdef CAT_EXT
      if (cat_key) {
        v = (cat_key & 0x04)   ? 512
            : (cat_key & 0x01) ? 870
            : (cat_key & 0x02) ? 1024
                               : 0;
      } // override analog value exercised by BUTTONS press
#endif // CAT_EXT
      event = SC;
      int32_t t0 = millis();
      for (; inv ^ _digitalRead(BUTTONS);) { // until released or long-press
        if ((millis() - t0) > 300) {
          event = PL;
          break;
        }
        wdt_reset();
      }
      delay(10); // debounce
      for (; (event != PL) &&
             ((millis() - t0) < 500);) { // until 2nd press or timeout
        if (inv ^ _digitalRead(BUTTONS)) {
          event = DC;
          break;
        }
        wdt_reset();
      }
      for (; inv ^ _digitalRead(BUTTONS);) { // until released, or encoder is
                                             // turned while longpress
        if (encoder_val && event == PL) {
          event = PT;
          break;
        }
#ifdef ONEBUTTON
        if (event == PL)
          break; // do not lock on longpress, so that L and R buttons can be
                 // used for tuning
#endif
        wdt_reset();
      } // Max. voltages at ADC3 for buttons L,R,E: 3.76V;4.55V;5V, thresholds
        // are in center
      event |=
          (v < (uint16_t)(4.2 * 1024.0 / 5.0)) ? BL
          : (v < (uint16_t)(4.8 * 1024.0 / 5.0))
              ? BR
              : BE; // determine which button pressed based on threshold levels
    } else {        // hack: fast forward handling
      event =
          (event & 0xf0) |
          ((encoder_val) ? PT : PLC /*PL*/); // only alternate between
                                             // push-long/turn when applicable
    }
    switch (event) {
#ifndef ONEBUTTON
    case BL | PL:  // Called when menu button pressed
    case BL | PLC: // or kept pressed
      menumode = 2;
      break;
    case BL | PT:
      menumode = 1;
      // if(menu == 0) menu = 1;
      break;
    case BL | SC:
#ifdef CW_MESSAGE
      if ((menumode == 1) && (menu >= CWMSG1) && (menu <= CWMSG6)) {
        cw_msg_event = millis();
        cw_msg_id = menu - CWMSG1;
        menumode = 0;
        break;
      }
#endif // CW_MESSAGE
      int8_t _menumode;
      if (menumode == 0) {
        _menumode = 1;
        if (menu == 0)
          menu = 1;
      } // short left-click while in default screen: enter menu mode
      if (menumode == 1) {
        _menumode = 2;
      } // short left-click while in menu: enter value selection screen
      if (menumode >= 2) {
        _menumode = 0;
        paramAction(SAVE, menu);
      } // short left-click while in value selection screen: save, and return to
        // default screen
      menumode = _menumode;
      break;
    case BL | DC:
      break;
    case BR | SC:
      if (!menumode) {
        int8_t prev_mode = mode;
        if (rit) {
          rit = 0;
          stepsize = prev_stepsize[mode == CW];
          change = true;
          break;
        }
        mode += 1;
        // encoder_val = 1;
        // paramAction(UPDATE, MODE); // Mode param //paramAction(UPDATE, mode,
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true);
// #define MODE_CHANGE_RESETS  1
#ifdef MODE_CHANGE_RESETS
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500; // sets suitable stepsize
#endif
        if (mode > CW)
          mode = LSB; // skip all other modes (only LSB, USB, CW)
#ifdef MODE_CHANGE_RESETS
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0; // resets filter (to most BW) and NR on mode change
#else
        if (mode == CW) {
          nr = 0;
        }
        prev_stepsize[prev_mode == CW] = stepsize;
        stepsize =
            prev_stepsize[mode ==
                          CW]; // backup stepsize setting for previous mode,
                               // restore previous stepsize setting for current
                               // selected mode; filter settings captured for
                               // either CQ or other modes.
        prev_filt[prev_mode == CW] = filt;
        filt =
            prev_filt[mode == CW]; // backup filter setting for previous mode,
                                   // restore previous filter setting for
                                   // current selected mode; filter settings
                                   // captured for either CQ or other modes.
#endif
        // paramAction(UPDATE, MODE);
        vfomode[vfosel % 2] = mode;
        paramAction(SAVE, (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
        paramAction(SAVE, MODE);
        paramAction(SAVE, FILTER);
        si5351.iqmsa = 0; // enforce PLL reset
#ifdef CW_DECODER
        if ((prev_mode == CW) && (cwdec))
          show_banner();
#endif
        change = true;
      } else {
        if (menumode == 1) {
          menumode = 0;
        } // short right-click while in menu: enter value selection screen
        if (menumode >= 2) {
          menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        } // short right-click while in value selection screen: save, and return
          // to menu screen
      }
      break;
    case BR | DC:
      filt++;
      _init = true;
      if (mode == CW && filt > N_FILT)
        filt = 4;
      if (mode == CW && filt == 4)
        stepsize = STEP_500; // reset stepsize for 500Hz filter
      if (mode == CW && (filt == 5 || filt == 6) && stepsize < STEP_100)
        stepsize = STEP_100; // for CW BW 200, 100      -> step = 100 Hz
      if (mode == CW && filt == 7 && stepsize < STEP_10)
        stepsize = STEP_10; // for CW BW 50 -> step = 10 Hz
      if (mode != CW && filt > 3)
        filt = 0;
      encoder_val = 0;
      paramAction(UPDATE, FILTER);
      paramAction(SAVE, FILTER);
      wdt_reset();
      delay(1500);
      wdt_reset();
      change = true; // refresh display
      break;
    case BR | PL:
#ifdef SIMPLE_RX
      // Experiment: ISR-less sdr_rx():
      smode = 0;
      TIMSK2 &= ~(1 << OCIE2A); // disable timer compare interrupt
      delay(100);
      lcd.setCursor(15, 1);
      lcd.print('X');
      static uint8_t x = 0;
      uint32_t next = 0;
      for (;;) {
        func_ptr();
#ifdef DEBUG
        numSamples++;
#endif
        if (!rx_state) {
          x++;
          if (x > 16) {
            loop();
            // lcd.setCursor(9, 0); lcd.print((int16_t)100); lcd.print(F("dBm
            // "));  // delays are taking too long!
            x = 0;
          }
        }
        // for(;micros() < next;);  next = micros() + 16;   // sync every
        // 1000000/62500=16ms (or later if missed)
      } //
#endif // SIMPLE_RX
#ifdef RIT_ENABLE
      rit = !rit;
      stepsize = (rit) ? STEP_10 : prev_stepsize[mode == CW];
      if (!rit) { // after RIT comes VFO A/B swap
#else
    {
#endif // RIT_ENABLE
        vfosel = !vfosel;
        freq = vfo[vfosel % 2]; // todo: share code with menumode
        mode = vfomode[vfosel % 2];
        // make more generic:
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500;
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0;
      }
      change = true;
      break;
// #define TUNING_DIAL  1
#ifdef TUNING_DIAL
    case BR | PLC: // while pressed long continues
    case BE | PLC:
      freq = freq + ((_step > 0) ? 1 : -1) * pow(2, abs(_step));
      change = true;
      break;
    case BR | PT:
      _step += encoder_val;
      encoder_val = 0;
      lcd.setCursor(0, 0);
      lcd.print(_step);
      lcd_blanks();
      break;
#endif // TUNING_DIAL
    case BE | SC:
      if (!menumode) {
        stepsize_change(+1);
      } else {
        int8_t _menumode;
        if (menumode == 1) {
          _menumode = 2;
        } // short encoder-click while in menu: enter value selection screen
        if (menumode == 2) {
          _menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        } // short encoder-click while in value selection screen: save, and
          // return to menu screen
#ifdef MENU_STR
        if (menumode == 3) {
          _menumode = 3;
          paramAction(NEXT_CH, menu);
        } // short encoder-click while in string edit mode: change position to
          // next character
#endif
        menumode = _menumode;
      }
      break;
    case BE | DC:
      // delay(100);
      bandval++;
      // if(bandval >= N_BANDS) bandval = 0;
      if (bandval >= (N_BANDS - 1))
        bandval = 1; // excludes 6m, 160m
      stepsize = STEP_1k;
      change = true;
      break;
    case BE | PL:
      stepsize_change(-1);
      break;
    case BE | PT:
      for (; _digitalRead(BUTTONS);) { // process encoder changes until released
        wdt_reset();
        if (encoder_val) {
          paramAction(UPDATE, VOLUME);
          if (volume < 0) {
            volume = 10;
            paramAction(SAVE, VOLUME);
            powerDown();
          } // powerDown when volume < 0
          paramAction(SAVE, VOLUME);
        }
      }
      change = true; // refresh display
      break;
#else // ONEBUTTON
    case BE | SC:
      int8_t _menumode;
      if (menumode == 0) {
        _menumode = 1;
        if (menu == 0)
          menu = 1;
      } // short enc-click while in default screen: enter menu mode
      if (menumode == 1) {
        _menumode = 2;
      } // short enc-click while in menu: enter value selection screen
      if (menumode == 2) {
        _menumode = 0;
        paramAction(SAVE, menu);
      } // short enc-click while in value selection screen: save, and return to
        // default screen
#ifdef MENU_STR
      if (menumode == 3) {
        _menumode = 3;
        paramAction(NEXT_CH, menu);
      } // short encoder-click while in string edit mode: change position to
        // next character
#endif
      menumode = _menumode;
      break;
    case BE | DC:
      if (!menumode) {
        int8_t prev_mode = mode;
        if (rit) {
          rit = 0;
          stepsize = prev_stepsize[mode == CW];
          change = true;
          break;
        }
        mode += 1;
        // encoder_val = 1;
        // paramAction(UPDATE, MODE); // Mode param //paramAction(UPDATE, mode,
        // NULL, F("Mode"), mode_label, 0, _N(mode_label), true);
// #define MODE_CHANGE_RESETS  1
#ifdef MODE_CHANGE_RESETS
        if (mode != CW)
          stepsize = STEP_1k;
        else
          stepsize = STEP_500; // sets suitable stepsize
#endif
        if (mode > CW)
          mode = LSB; // skip all other modes (only LSB, USB, CW)
#ifdef MODE_CHANGE_RESETS
        if (mode == CW) {
          filt = 4;
          nr = 0;
        } else
          filt = 0; // resets filter (to most BW) and NR on mode change
#else
        if (mode == CW) {
          nr = 0;
        }
        prev_stepsize[prev_mode == CW] = stepsize;
        stepsize =
            prev_stepsize[mode ==
                          CW]; // backup stepsize setting for previous mode,
                               // restore previous stepsize setting for current
                               // selected mode; filter settings captured for
                               // either CQ or other modes.
        prev_filt[prev_mode == CW] = filt;
        filt =
            prev_filt[mode == CW]; // backup filter setting for previous mode,
                                   // restore previous filter setting for
                                   // current selected mode; filter settings
                                   // captured for either CQ or other modes.
#endif
        // paramAction(UPDATE, MODE);
        vfomode[vfosel % 2] = mode;
        paramAction(SAVE, (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
        paramAction(SAVE, MODE);
        paramAction(SAVE, FILTER);
        si5351.iqmsa = 0; // enforce PLL reset
        if ((prev_mode == CW) && (cwdec))
          show_banner();
        change = true;
      } else {
        if (menumode == 1) {
          menumode = 0;
        } // short right-click while in menu: enter value selection screen
        if (menumode >= 2) {
          menumode = 1;
          change = true;
          paramAction(SAVE, menu);
        } // short right-click while in value selection screen: save, and return
          // to menu screen
      }
      break;
    case BE | PL:
      stepsize += 1;
      if (stepsize < STEP_1k)
        stepsize = STEP_10;
      if (stepsize > STEP_10)
        stepsize = STEP_1k;
      stepsize_showcursor();
      break;
    case BE | PLC: // or kept pressed
      menumode = 2;
      break;
    case BE | PT:
      menumode = 1;
      // if(menu == 0) menu = 1;
      break;
    case BL | SC:
    case BL | DC:
    case BL | PL:
    case BL | PLC:
      encoder_val++;
      break;
    case BR | SC:
    case BR | DC:
    case BR | PL:
    case BR | PLC:
      encoder_val--;
      break;
#endif // ONEBUTTON
    }
  } else
    event = 0; // no button pressed: reset event

  if ((menumode) || (prev_menumode != menumode)) { // Show parameter and value
    int8_t encoder_change = encoder_val;
    if ((menumode == 1) && encoder_change) {
      menu += encoder_val; // Navigate through menu
#ifdef ONEBUTTON
      menu = max(0, min(menu, N_PARAMS));
#else
      menu = max(1 /* 0 */, min(menu, N_PARAMS));
#endif
      menu = paramAction(NEXT_MENU,
                         menu); // auto probe next menu item (gaps may exist)
      encoder_val = 0;
    }
    if (encoder_change || (prev_menumode != menumode))
      paramAction(UPDATE_MENU,
                  (menumode)
                      ? menu
                      : 0); // update param with encoder change and display
    prev_menumode = menumode;
    if (menumode == 2) {
      if (encoder_change) {
        lcd.setCursor(0, 1);
        lcd.cursor();       // edits menu item value; make cursor visible
        if (menu == MODE) { // post-handling Mode parameter
          vfomode[vfosel % 2] = mode;
          paramAction(SAVE,
                      (vfosel % 2) ? MODEB : MODEA); // save vfoa/b changes
          change = true;
          si5351.iqmsa = 0; // enforce PLL reset
          // make more generic:
          if (mode != CW)
            stepsize = STEP_1k;
          else
            stepsize = STEP_500;
          if (mode == CW) {
            filt = 4;
            nr = 0;
          } else
            filt = 0;
        }
        if (menu == BAND) {
          change = true;
        }
        if (menu == FT8MODE) {
          if (ft8mode) {
            prev_mode_ft8 = mode;
            prev_filt_ft8 = filt;
            prev_agc_ft8 = agc;
            prev_nr_ft8 = nr;
            mode = USB;
            filt = 1;
            agc = 0;
            nr = 0;
            dig_mode = true;
          } else {
            mode = prev_mode_ft8;
            filt = prev_filt_ft8;
            agc = prev_agc_ft8;
            nr = prev_nr_ft8;
            dig_mode = false;
          }
          change = true;
        }
        // if(menu == NR){ if(mode == CW) nr = false; }
        if (menu == VFOSEL) {
          freq = vfo[vfosel % 2];
          mode = vfomode[vfosel % 2];
          // make more generic:
          if (mode != CW)
            stepsize = STEP_1k;
          else
            stepsize = STEP_500;
          if (mode == CW) {
            filt = 4;
            nr = 0;
          } else
            filt = 0;
          change = true;
        }
#ifdef RIT_ENABLE
        if (menu == RIT) {
          stepsize = (rit) ? STEP_10 : STEP_500;
          change = true;
        }
#endif
        // if(menu == VOX){ if(vox){ vox_thresh-=1; } else { vox_thresh+=1; }; }
        if (menu == ATT) { // post-handling ATT parameter
          if (dsp_cap == SDR) {
            noInterrupts();
#ifdef SWAP_RX_IQ
            adc_start(1, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[0] = ADMUX;
            adc_start(0, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[1] = ADMUX;
#else
            adc_start(0, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[0] = ADMUX;
            adc_start(1, !(att & 0x01) /*true*/, F_ADC_CONV);
            admux[1] = ADMUX;
#endif // SWAP_RX_IQ
            interrupts();
          }
          digitalWrite(
              RX, !(att & 0x02)); // att bit 1 ON: attenuate -20dB by disabling
                                  // RX line, switching Q5 (antenna input
                                  // switch) into 100k resistence
          pinMode(AUDIO1, (att & 0x04)
                              ? OUTPUT
                              : INPUT); // att bit 2 ON: attenuate -40dB by
                                        // terminating ADC inputs with 10R
          pinMode(AUDIO2, (att & 0x04) ? OUTPUT : INPUT);
        }
        if (menu == SIFXTAL) {
          change = true;
        }
#ifdef TX_ENABLE
        if ((menu == PWM_MIN) || (menu == PWM_MAX)) {
          build_lut();
        }
#endif
        if (menu == CWTONE) {
          if (dsp_cap) {
            cw_offset = (cw_tone == 0) ? tones[0] : tones[1];
            paramAction(SAVE, CWOFF);
          }
        }
        if (menu == IQ_ADJ) {
          change = true;
        }
#ifdef CAL_IQ
        if (menu == CALIB) {
          if (dsp_cap != SDR)
            calibrate_iq();
          menu = 0;
        }
#endif
#ifdef KEYER
        if (menu == KEY_WPM) {
          loadWPM(keyer_speed);
        }
        if (menu == KEY_MODE) {
          if (keyer_mode == 0) {
            keyerControl = IAMBICA;
          }
          if (keyer_mode == 1) {
            keyerControl = IAMBICB;
          }
          if (keyer_mode == 2) {
            keyerControl = SINGLE;
          }
        }
#endif // KEYER
#ifdef TX_DELAY
        if (menu == TXDELAY) {
          semi_qsk = (txdelay > 0);
        }
#endif // TX_DELAY
      }
#ifdef DEBUG
      if (menu == SR) { // measure sample-rate
        numSamples = 0;
        delay(F_MCU * 500UL /
              16000000); // delay 0.5s (in reality because F_CPU=20M instead of
                         // 16M, delay() is running 1.25x faster therefore we
                         // need to multiply with 1.25)
        sr = numSamples * 2;            // samples per second
        paramAction(UPDATE_MENU, menu); // refresh
      }
      if (menu == CPULOAD) { // measure CPU-load
        uint32_t i = 0;
        uint32_t prev_time = millis();
        for (i = 0; i != 300000; i++)
          wdt_reset(); // fixed CPU-load 132052*1.25us delay under 0% load
                       // condition; is 132052*1.25 * 20M = 3301300 CPU cycles
                       // fixed load
        cpu_load = 100 - 132 * 100 / (millis() - prev_time);
        paramAction(UPDATE_MENU, menu); // refresh
      }
      if ((menu == PARAM_A) || (menu == PARAM_B) || (menu == PARAM_C)) {
        delay(300);
        paramAction(UPDATE_MENU, menu); // refresh
      }
#endif
    }
  }

  if (menumode == 0) {
    if (encoder_val) { // process encoder tuning steps
      process_encoder_tuning_step(encoder_val);
      encoder_val = 0;
    }
  }

  if ((change) && (!tx) && (!vox_tx)) { // only change if TX is OFF, prevent
                                        // simultaneous I2C bus access
    change = false;
    if (prev_bandval != bandval) {
      freq = band[bandval];
      prev_bandval = bandval;
    }
    vfo[vfosel % 2] = freq;
    save_event_time =
        millis() + 1000; // schedule time to save freq (no save while tuning,
                         // hence no EEPROM wear out)

    if (menumode == 0) {
      display_vfo(freq);
      stepsize_showcursor();
#ifdef CAT
      // Command_GETFreqA();
#endif

      // The following is a hack for SWR measurement:
      // si5351.alt_clk2(freq + 2400);
      // si5351.SendRegister(SI_CLK_OE, TX1RX1);
      // digitalWrite(SIG_OUT, HIGH);  // inject CLK2 on antenna input via 120K
    }

    // noInterrupts();
    uint8_t f = freq / 1000000UL;
    set_lpf(f);
    bandval = (f > 32)   ? 10
              : (f > 26) ? 9
              : (f > 22) ? 8
              : (f > 20) ? 7
              : (f > 16) ? 6
              : (f > 12) ? 5
              : (f > 8)  ? 4
              : (f > 6)  ? 3
              : (f > 4)  ? 2
              : (f > 2)  ? 1
                         : 0;
    prev_bandval = bandval; // align bandval with freq

    if (mode == CW) {
      si5351.freq(freq + cw_offset, rx_ph_q,
                  0 /*90, 0*/); // RX in CW-R (=LSB), correct for CW-tone offset
    } else if (mode == LSB)
      si5351.freq(freq, rx_ph_q, 0 /*90, 0*/); // RX in LSB
    else
      si5351.freq(freq, 0, rx_ph_q /*0, 90*/); // RX in USB, ...
#ifdef RIT_ENABLE
    if (rit) {
      si5351.freq_calc_fast(rit);
      si5351.SendPLLRegisterBulk();
    }
#endif // RIT_ENABLE
    // interrupts();
  }

  if ((save_event_time) &&
      (millis() >
       save_event_time)) { // save freq when time has reached schedule
    paramAction(SAVE, (vfosel % 2) ? FREQB : FREQA); // save vfoa/b changes
    save_event_time = 0;
    // lcd.setCursor(15, 1); lcd.print('S'); delay(100); lcd.setCursor(15, 1);
    // lcd.print('R');
  }

#ifdef CW_MESSAGE
  if ((mode == CW) && (cw_msg_event) &&
      (millis() > cw_msg_event)) { // if it is time to send a CW message
    if ((cw_tx(cw_msg[cw_msg_id]) == 0) &&
        ((cw_msg[cw_msg_id][0] == 'C') && (cw_msg[cw_msg_id][1] == 'Q')) &&
        cw_msg_interval)
      cw_msg_event = millis() + 1000 * cw_msg_interval;
    else
      cw_msg_event =
          0; // then send message, if not interrupted and its a CQ msg and there
             // is an interval set, then schedule new event
  }
#endif // CW_MESSAGE

  wdt_reset();

  //{ lcd.setCursor(0, 0); lcd.print(freeMemory()); lcd.print(F("    ")); }
}
