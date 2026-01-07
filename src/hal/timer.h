#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <Arduino.h>

#define F_SAMP_RX     192307
#define F_SAMP_PWM    78125

void timer1_start(uint32_t fs);
void timer1_stop();
void timer2_start(uint32_t fs);
void timer2_stop();

#endif
