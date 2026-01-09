//  QCX-SSB.ino - https://github.com/threeme3/QCX-SSB
//
//  Copyright 2019, 2020, 2021   Guido PE1NNZ <pe1nnz@qsl.net>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions: The above copyright
//  notice and this permission notice shall be included in all copies or
//  substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS",
//  WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//  TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
//  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
//  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. Modifications by
//  EA7LJY - 02112025
//  1. F_XTAL : 27000000
//  2. S-Meter mode : 2 (S-Meter)
//  3. Noise Reduce: default 2
//  4. Change version 1.03x

#define VERSION "1.03x"

#include "src/usdx_settings.h"
#include <EEPROM.h>

// QCX pin definitions moved to src/usdx_settings.h

#ifndef TX_ENABLE
#undef KEYER
#undef TX_DELAY
#undef SEMI_QSK
#undef RIT_ENABLE
#undef VOX_ENABLE
#undef MOX_ENABLE
#endif //! TX_ENABLE

#ifdef SWR_METER
float FWD;
float SWR;
float ref_V = 5 * 1.15;
static uint32_t stimer;
#define PIN_FWD A6
#define PIN_REF A7
#endif

/*
// UCX installation: On blank chip, use (standard Arduino Uno) fuse settings
(E:FD, H:DE, L:FF), and use customized Optiboot bootloader for 20MHz clock, then
upload via serial interface (with RX, TX and DTR lines connected to pin 1, 2, 3
respectively)
// UCX pin defintions
+#define SDA     3         //PD3    (pin 5)
+#define SCL     4         //PD4    (pin 6)
+#define ROT_A   6         //PD6    (pin 12)
+#define ROT_B   7         //PD7    (pin 13)
+#define RX      8         //PB0    (pin 14)
+#define SIDETONE 9        //PB1    (pin 15)
+#define KEY_OUT 10        //PB2    (pin 16)
+#define NTX     11        //PB3    (pin 17)
+#define DAH     12        //PB4    (pin 18)
+#define DIT     13        //PB5    (pin 19)
+#define AUDIO1  14        //PC0/A0 (pin 23)
+#define AUDIO2  15        //PC1/A1 (pin 24)
+#define DVM     16        //PC2/A2 (pin 25)
+#define BUTTONS 17        //PC3/A3 (pin 26)
// In addition set:
#define OLED  1
#define ONEBUTTON  1
#define ONEBUTTON_INV 1
#undef DEBUG
adjust I2C and I2C_ ports,
ssb_cap=1; dsp_cap=2;
#define _DELAY() for(uint8_t i = 0; i != 5; i++) asm("nop");
#define F_XTAL 20004000
#define F_CPU F_XTAL
*/

// FUSES = { .low = 0xFF, .high = 0xD6, .extended = 0xFD };   // Fuse settings
// should be set at programming (Arduino IDE > Tools > Burn bootloader)

// #if(ARDUINO < 10810)
//    #error "Unsupported Arduino IDE version, use Arduino IDE 1.8.10 or later
//    from https://www.arduino.cc/en/software"
// #endif
#if !(defined(ARDUINO_ARCH_AVR))
#error                                                                         \
    "Unsupported architecture, select Arduino IDE > Tools > Board > Arduino AVR Boards > Arduino Uno."
#endif
#if (F_CPU != 16000000)
#error                                                                         \
    "Unsupported clock frequency, Arduino IDE must specify 16MHz clock; alternate crystal frequencies may be specified with F_MCU."
#endif
#undef F_CPU
#define F_CPU                                                                  \
  20007000 // Actual crystal frequency of 20MHz XTAL1, note that this
           // declaration is just informative and does not correct the timing in
           // Arduino functions like delay(); hence a 1.25 factor needs to be
           // added for correction.
#ifndef F_MCU
#define F_MCU 20000000 // 20MHz ATMEGA328P crystal
#endif

#include "src/core/core.h"
void setup() {

  digitalWrite(KEY_OUT, LOW); // for safety: to prevent exploding PA MOSFETs, in
                              // case there was something still biasing them.
  si5351.powerDown(); // disable all CLK outputs (especially needed for si5351
                      // variants that has CLK2 enabled by default, such as
                      // Si5351A-B04486-GT)

  // uint8_t mcusr = MCUSR;
  MCUSR = 0;
  // wdt_disable();
  wdt_enable(WDTO_4S); // Enable watchdog
  uint32_t t0, t1;
#ifdef DEBUG
  // Benchmark dsp_tx() ISR (this needs to be done in beginning of setup()
  // otherwise when VERSION containts 5 chars, mis-alignment impact performance
  // by a few percent)
  func_ptr = dsp_tx;
  t0 = micros();
  TIMER2_COMPA_vect();
  // func_ptr();
  t1 = micros();
  uint16_t load_tx = (float)(t1 - t0) * (float)F_SAMP_TX * 100.0 / 1000000.0 *
                     16000000.0 / (float)F_CPU;
  // benchmark sdr_rx_00() ISR
  func_ptr = sdr_rx_00;
  rx_state = 0;
  uint16_t load_rx[8];
  uint16_t load_rx_avg = 0;
  uint16_t i;
  for (i = 0; i != 8; i++) {
    rx_state = i;
    t0 = micros();
    TIMER2_COMPA_vect();
    // func_ptr();
    t1 = micros();
    load_rx[i] = (float)(t1 - t0) * (float)F_SAMP_RX * 100.0 / 1000000.0 *
                 16000000.0 / (float)F_CPU;
    load_rx_avg += load_rx[i];
  }
  load_rx_avg /= 8;

  // adc_stop();  // recover general ADC settings so that analogRead is working
  // again
#endif                  // DEBUG
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
  for (i = 0; i != N_FONTS; i++) { // Init fonts
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

  // ssb_cap = 0; dsp_cap = ANALOG;  // force standard QCX capability
  // ssb_cap = 1; dsp_cap = ANALOG;  // force SSB and standard QCX-RX capability
  // ssb_cap = 1; dsp_cap = DSP;     // force SSB and DSP capability
  // ssb_cap = 1; dsp_cap = SDR;     // force SSB and SDR capability
#endif // QCX

#ifdef DEBUG
  /*if((mcusr & WDRF) && (!(mcusr & EXTRF)) && (!(mcusr & BORF))){
      lcd.setCursor(0, 1); lcd.print(F("!!Watchdog RESET")); lcd_blanks();
      delay(1500); wdt_reset();
    }
    if((mcusr & BORF) && (!(mcusr & WDRF))){
      lcd.setCursor(0, 1); lcd.print(F("!!Brownout RESET")); lcd_blanks();  //
    Brow-out reset happened, CPU voltage not stable or make sure Brown-Out
    threshold is set OK (make sure E fuse is set to FD) delay(1500);
    wdt_reset();
    }
    if(mcusr & PORF){
      lcd.setCursor(0, 1); lcd.print(F("!!Power-On RESET")); lcd_blanks();
      delay(1500); wdt_reset();
    }*/
  /*if(mcusr & EXTRF){
  lcd.setCursor(0, 1); lcd.print(F("Power-On")); lcd_blanks();
    delay(1); wdt_reset();
  }*/

  // Measure CPU loads
  if (!(load_tx <= 100)) {
    fatal(F("CPU_tx"), load_tx, '%');
  }

  if (!(load_rx_avg <= 100)) {
    fatal(F("CPU_rx"), load_rx_avg, '%');
  }
  /*for(i = 0; i != 8; i++){
    if(!(load_rx[i] <= 100)){   // and specify individual timings for each of
  the eight alternating processing functions
      //fatal(F("CPU_rx"), load_rx[i], '%');
      lcd.setCursor(0, 1); lcd.print(F("!!CPU_rx")); lcd.print(i);
  lcd.print('='); lcd.print(load_rx[i]); lcd.print('%'); lcd_blanks();
    }
  }*/
#endif

#ifdef DIAG
  // Measure VDD (+5V); should be ~5V
  si5351.SendRegister(SI_CLK_OE, TX0RX0); // Mute QSD
  digitalWrite(KEY_OUT, LOW);
  digitalWrite(RX, LOW); // mute RX
  delay(100);            // settle
  float vdd = 2.0 * (float)analogRead(AUDIO2) * 5.0 / 1024.0;
  digitalWrite(RX, HIGH);
  if (!(vdd > 4.8 && vdd < 5.2)) {
    fatal(F("V5.0"), vdd, 'V');
  }

  // Measure VEE (+3.3V); should be ~3.3V
  float vee = (float)analogRead(SCL) * 5.0 / 1024.0;
  if (!(vee > 3.2 && vee < 3.8)) {
    fatal(F("V3.3"), vee, 'V');
  }

  // Measure AVCC via AREF and using internal 1.1V reference fed to ADC; should
  // be ~5V
  analogRead(6);    // setup almost proper ADC readout
  bitSet(ADMUX, 3); // Switch to channel 14 (Vbg=1.1V)
  delay(1);         // delay improves accuracy
  bitSet(ADCSRA, ADSC);
  for (; bit_is_set(ADCSRA, ADSC);)
    ;
  float avcc = 1.1 * 1023.0 / ADC;
  if (!(avcc > 4.6 && avcc < 5.2)) {
    fatal(F("Vavcc"), avcc, 'V');
  }

  // Report no SSB capability
  if (!ssb_cap) {
    fatal(F("No MIC input..."));
  }

  // Test microphone polarity
  /*if((ssb_cap) && (!_digitalRead(DAH))){
    fatal(F("MIC in rev.pol"));
  }*/

  // Measure DVM bias; should be ~VAREF/2
#ifdef _SERIAL
  DDRC &= ~(1 << 2); // disable PC2, so that ADC2 can be used as mic input
#else
  PORTD |= 1 << 1;
  DDRD |= 1 << 1; // keep PD1 HIGH so that in case diode is installed to PC2 it
                  // is kept blocked (otherwise ADC2 input is pulled down!)
#endif
  delay(10);
#ifdef TX_ENABLE
  float dvm = (float)analogRead(DVM) * 5.0 / 1024.0;
  if ((ssb_cap) && !(dvm > 1.8 && dvm < 3.2)) {
    fatal(F("Vadc2"), dvm, 'V');
  }
#endif

  // Measure AUDIO1, AUDIO2 bias; should be ~VAREF/2
  if (dsp_cap == SDR) {
    float audio1 = (float)analogRead(AUDIO1) * 5.0 / 1024.0;
    if (!(audio1 > 1.8 && audio1 < 3.2)) {
      fatal(F("Vadc0"), audio1, 'V');
    }
    float audio2 = (float)analogRead(AUDIO2) * 5.0 / 1024.0;
    if (!(audio2 > 1.8 && audio2 < 3.2)) {
      fatal(F("Vadc1"), audio2, 'V');
    }
  }

#ifdef TX_ENABLE
  // Measure I2C Bus speed for Bulk Transfers
  // si5351.freq(freq, 0, 90);
  wdt_reset();
  t0 = micros();
  for (i = 0; i != 1000; i++)
    si5351.SendPLLRegisterBulk();
  t1 = micros();
  uint32_t speed = (1000000 * 8 * 7) / (t1 - t0); // speed in kbit/s
  if (false) {
    fatal(F("i2cspeed"), speed, 'k');
  }

  // Measure I2C Bit-Error Rate (BER); should be error free for a thousand
  // random bulk PLLB writes
  si5351.freq(freq, 0,
              90); // freq needs to be set in order to use freq_calc_fast()
  wdt_reset();
  uint16_t i2c_error = 0; // number of I2C byte transfer errors
  for (i = 0; i != 1000; i++) {
    si5351.freq_calc_fast(i);
    // for(int j = 0; j != 8; j++) si5351.pll_regs[j] = rand();
    si5351.SendPLLRegisterBulk();
#define SI_SYNTH_PLL_A 26
#ifdef NEW_TX
    for (int j = 4; j != 8; j++)
      if (si5351.RecvRegister(SI_SYNTH_PLL_A + j) != si5351.pll_regs[j])
        i2c_error++;
#else
    for (int j = 3; j != 8; j++)
      if (si5351.RecvRegister(SI_SYNTH_PLL_A + j) != si5351.pll_regs[j])
        i2c_error++;
#endif // NEW_TX
  }
  wdt_reset();
  if (i2c_error) {
    fatal(F("BER_i2c"), i2c_error, ' ');
  }
#endif // TX_ENABLE
#endif // DIAG

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
  // freq = bands[band];

  // Load parameters from EEPROM, reset to factory defaults when stored values
  // are from a different version
  paramAction(LOAD, VERS);
  if ((eeprom_version != get_version_id()) ||
      _digitalRead(BUTTONS)) { // EEPROM clean: if rotary-key pressed or version
                               // signature in EEPROM does NOT corresponds with
                               // this firmware
    eeprom_version = get_version_id();
    // for(int n = 0; n != 1024; n++){ eeprom_write_byte((uint8_t *) n, 0);
    // wdt_reset(); } //clean EEPROM eeprom_write_dword((uint32_t
    // *)EEPROM_OFFSET/3, 0x000000);
    paramAction(SAVE); // save default parameter values
    lcd.setCursor(0, 1);
    lcd.print(F("Reset settings.."));
    delay(500);
    wdt_reset();
  } else {
    paramAction(LOAD); // load all parameters
    ft8mode = EEPROM.read(FT8_EEPROM_ADDR);
    if (ft8mode > 1)
      ft8mode = 0;
    prev_mode_ft8 = EEPROM.read(PREV_MODE_FT8_EEPROM_ADDR);
    prev_filt_ft8 = EEPROM.read(PREV_FILT_FT8_EEPROM_ADDR);
    prev_agc_ft8 = EEPROM.read(PREV_AGC_FT8_EEPROM_ADDR);
    prev_nr_ft8 = EEPROM.read(PREV_NR_FT8_EEPROM_ADDR);
    if (ft8mode) {
      mode = USB;
      filt = 1;
      agc = 0;
      nr = 0;
      dig_mode = true;
    }
  }
  // if(abs((int32_t)F_XTAL - (int32_t)si5351.fxtal) > 50000){ si5351.fxtal =
  // F_XTAL; }  // if F_XTAL frequency deviates too much with actual setting ->
  // use default
  si5351.iqmsa = 0; // enforce PLL reset
  change = true;
  prev_bandval = bandval;
  vox = false; // disable VOX
  nr = 2;      // disable NR
  smode = 2;   // SMeter
  rit = false; // disable RIT
  freq = vfo[vfosel % 2];
  mode = vfomode[vfosel % 2];

#ifdef TX_ENABLE
  build_lut();
#endif

  show_banner(); // remove release number

  start_rx();

#if defined(CAT) || defined(TESTBENCH)
#ifdef CAT_STREAMING
#define BAUD 115200 // Baudrate used for serial communications
#else
#define BAUD                                                                   \
  38400 // 38400 //115200 //4800 //Baudrate used for serial communications (CAT,
        // TESTBENCH)
#endif
  Serial.begin(16000000ULL * BAUD / F_MCU); // corrected for F_CPU=20M
  Command_IF();
#if !defined(OLED) && defined(TESTBENCH)
  smode = 0; // In case of LCD, turn of smeter
#endif
#endif // CAT TESTBENCH

#ifdef KEYER
  keyerState = IDLE;
  keyerControl = IAMBICB; // Or 0 for IAMBICA
  loadWPM(keyer_speed);   // Fix speed at 15 WPM
#endif                    // KEYER

  for (; !_digitalRead(DIT) ||
         ((mode == CW && keyer_mode != SINGLE) && (!_digitalRead(DAH)));) {
    fatal(F("Check PTT/key"));
  } // wait until DIH/DAH/PTT is released to prevent TX on startup
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
      ssb(((int16_t)(analogSampleMic()) - 512) >> MIC_ATTEN); // sampling mic
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

/* BACKLOG:
code definitions and re-use for comb, integrator, dc decoupling, arctan
refactor main()
agc based on rms256, agc/smeter after filter
noisefree integrator (rx audio out) in lower range
raised cosine tx amp for cw, 4ms tau seems enough:
http://fermi.la.asu.edu/w9cf/articles/click/index.html 32 bin fft dynamic range
cw att extended agc Split undersampling, IF-offset K2/TS480 CAT control faster
RX-TX switch to support CW usdx API demo code scan move last bit of arrays into
flash?
https://web.archive.org/web/20180324010832/https://www.microchip.com/webdoc/AVRLibcReferenceManual/FAQ_1faq_rom_array.html
u-law in RX path?:
http://dystopiancode.blogspot.com/2012/02/pcm-law-and-u-law-companding-algorithms.html
Arduino library?
1. RX bias offset correction by measurement avg, 2. charge decoupling cap. by
resetting to 0V and setting 5V for a certain amount of (charge) time add 1K
(500R?) to GND at TTL RF output to keep zero-level below BS170 threshold
additional PWM output for potential BOOST conversion
squelch gating
more buttons
s-meter offset vs DC bal.
keyer with interrupt-driven timers (to reduce jitter)

Analyse assembly:
/home/guido/Downloads/arduino-1.8.10/hardware/tools/avr/bin/avr-g++ -S -g -Os -w
-std=gnu++11 -fpermissive -fno-exceptions -ffunction-sections -fdata-sections
-fno-threadsafe-statics -Wno-error=narrowing -MMD -mmcu=atmega328p
-DF_CPU=16000000L -DARDUINO=10810 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR
-I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/cores/arduino
-I/home/guido/Downloads/arduino-1.8.10/hardware/arduino/avr/variants/standard
/tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp -o
/tmp/arduino_build_483134/sketch/QCX-SSB.ino.cpp.txt

Rewire/code I/Q clk pins so that a Div/1 and Div/2 scheme is used instead of 0
and 90 degrees phase shift 10,11,13,12   10,11,12,13  (pin) Q- I+ Q+ I-   Q- I+
Q+ I- 90 deg.shift  div/2@S1(pin2)

50MHz LSB OK, USB NOK

atmega328p signature: https://forum.arduino.cc/index.php?topic=341799.15
https://www.eevblog.com/forum/microcontrollers/bootloader-on-smd-atmega328p-au/msg268938/#msg268938
https://www.avrfreaks.net/forum/undocumented-signature-row-contents

Alain k1fm AGC sens issue:  https://groups.io/g/ucx/message/3998
https://groups.io/g/ucx/message/3999 txdelay when vox is on (disregading the
tx>0 state due to ssb() overrule, instead use RX-digitalinput) Adrian: issue
#41, set cursor just after writing 'R' when smeter is off, and (menumode == 0)
Konstantinos: backup/restore vfofilt settings when changing vfo.
Bob: 2mA for clk0/1 during RX
Uli: accuracate voltages during diag

agc behind filter
vcc adc extend. power/curr measurement
swr predistort eff calc
block ptt while in vox mode

adc bias error and potential error correction
noise burst on tx
https://groups.io/g/ucx/topic/81030243#6265

for (size_t i = 0; i < 9; i++) id[i] = boot_signature_byte_get(0x0E + i + (i >
5));



*/
