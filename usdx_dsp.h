#ifndef USDX_DSP_H
#define USDX_DSP_H

#include "usdx_config.h"
#include <Arduino.h>

// ==========================================
// Enums and Constants
// ==========================================
enum mode_t { LSB, USB, CW, FM, AM };

// ==========================================
// Global Variables (DSP & State)
// ==========================================
extern volatile uint8_t mode;
extern volatile uint8_t tx;
extern volatile uint8_t filt;
extern volatile uint8_t vox_thresh;
extern volatile uint8_t drive;
extern volatile uint8_t quad;
extern volatile bool dig_mode;
extern volatile uint8_t mox;
extern volatile int8_t volume;
extern volatile uint8_t agc;
extern volatile uint8_t nr;
extern volatile uint8_t att;
extern volatile uint8_t att2;
extern volatile uint8_t cw_tone;
extern volatile uint32_t cw_offset;
extern volatile uint8_t cat_streaming;
extern volatile uint8_t menumode;
extern volatile uint8_t ft8mode;
extern volatile uint8_t prev_mode_ft8;
extern volatile uint8_t prev_filt_ft8;
extern volatile uint8_t prev_agc_ft8;
extern volatile uint8_t prev_nr_ft8;
extern volatile uint8_t cwdec;
extern volatile uint8_t cw_event;
extern volatile uint16_t numSamples;
extern uint8_t lut[256];
extern volatile uint8_t amp;
extern volatile uint8_t practice;
extern volatile uint8_t wpm;
extern volatile unsigned long ditTime;
extern uint8_t txdelay;
extern uint8_t semi_qsk;
extern volatile uint8_t vox;
extern volatile uint8_t cat_active;
extern volatile uint8_t dsp_cap;
extern volatile uint32_t _absavg256;
extern uint32_t semi_qsk_timeout;

#ifdef CW_MESSAGE
#ifdef CW_MESSAGE_EXT
extern char cw_msg[6][48];
#else
extern char cw_msg[1][48];
#endif
extern uint8_t cw_msg_interval;
extern uint32_t cw_msg_event;
extern uint8_t cw_msg_id;
#endif

// ==========================================
// Function Prototypes
// ==========================================

// Hardware Control (ADC/Timers)
void adc_start(uint8_t adcpin, bool ref1v1, uint32_t fs);
void adc_stop();
void timer1_start(uint32_t fs);
void timer1_stop();
void timer2_start(uint32_t fs);
// void timer2_stop(); // Not explicitly defined in original, but good to have?

// DSP Functions
void dsp_tx();
void dsp_rx();
void dsp_tx_cw();
void dsp_tx_am();
void dsp_tx_fm();

// CW Functions
int cw_tx(char ch);
int cw_tx(char *msg);
void cw_decode();
void printsym(bool submit = true);

// Helper for switching RX/TX (needed by cw_tx)
void switch_rxtx(uint8_t tx_enable);

// Inline DSP helpers (exposed if needed, otherwise could be static in cpp)
// Keeping them internal to cpp if possible, but some might be needed.
// For now, I'll keep them in cpp unless I see external usage.

#endif // USDX_DSP_H
