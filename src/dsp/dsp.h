#ifndef DSP_H
#define DSP_H

#include "../driver/display.h"
#include "../hardware/wire.h"
#include <Arduino.h>

// Enums
enum dsp_cap_t { ANALOG, DSP, SDR };
enum mode_t { LSB, USB, CW, FM, AM };

// Global Variables
extern volatile uint8_t mode;
extern volatile uint16_t numSamples;
extern volatile uint8_t tx;
extern volatile uint8_t filt;
extern uint8_t lut[256];
extern volatile uint8_t amp;
extern volatile uint8_t vox_thresh;
extern volatile uint8_t drive;
extern volatile uint8_t quad;
extern volatile bool dig_mode;
extern volatile int8_t mox;
extern volatile int8_t volume;
extern volatile uint16_t acc;
extern volatile uint32_t cw_offset;
extern volatile uint8_t cw_tone;
extern const uint32_t tones[];
extern volatile uint8_t menumode;
extern volatile uint8_t ft8mode;
extern volatile uint8_t prev_mode_ft8;
extern volatile uint8_t prev_filt_ft8;
extern volatile uint8_t prev_agc_ft8;
extern volatile uint8_t prev_nr_ft8;
extern volatile uint8_t cat_streaming;
extern volatile uint8_t _cat_streaming;
extern volatile uint8_t agc;
extern volatile uint8_t nr;
extern volatile uint8_t att;
extern volatile uint8_t att2;
extern volatile uint8_t _init;

#ifdef CW_DECODER
extern volatile uint8_t cwdec;
// extern volatile uint8_t cw_event; // Not strictly needing extern if internal?
// It is used in .ino? Check usages. Usually UI uses it.
extern volatile uint8_t cw_event;
#endif

#ifdef QCX
extern uint8_t dsp_cap;
extern uint8_t ssb_cap;
#else
extern const uint8_t ssb_cap;
extern const uint8_t dsp_cap;
#endif

// CW Message Globals (used in menu?)
#ifdef CW_MESSAGE
#define MENU_STR 1
extern char cw_msg[6][48];
extern uint8_t cw_msg_interval;
extern uint32_t cw_msg_event;
extern uint8_t cw_msg_id;
#endif

// Macros required by Core/INO
#define MIC_ATTEN 0
#define N_FILT 7
#define F_SAMP_TX 4800
#define F_SAMP_RX 62500
#define F_ADC_CONV (192307 / 2)
#define F_SAMP_PWM (78125 / 1)

// Arrays
extern uint8_t prev_filt[2]; // defined as prev_filt[] = {0, 4} (size 2?)
                             // Wait, line 886: uint8_t prev_filt[] = {0, 4};
extern const uint8_t ramp[32];
extern volatile uint8_t admux[3];
extern char out[17]; // "                " + null? Check size.

// Globals used as loop counters (Legacy)
extern volatile int16_t i, q;
extern uint8_t wpm;
extern volatile uint32_t _absavg256;
extern int16_t centiGain;
extern volatile uint8_t rx_state;
extern bool filteredstate;
extern unsigned long ditTime;
extern uint8_t inv;

// Function Pointers
typedef void (*func_t)(void);
extern volatile func_t func_ptr;

// Functions
void dsp_tx();
void dsp_tx_cw();
void dsp_tx_am();
void dsp_tx_fm();
int cw_tx(char ch);
int cw_tx(char *msg);
void cw_decode();
void dec2();
int16_t ssb(int16_t in);
void sdr_rx_00();
void dummy();

#endif // DSP_H
