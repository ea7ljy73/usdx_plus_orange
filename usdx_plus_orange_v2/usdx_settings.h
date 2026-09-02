// usdx_settings.h - uSDX Plus Orange v2 (modular)
// Solo configuración: hardware model, constantes, features.
// El código compila siempre la ruta activa; aquí se definen los pins/constantes.

#pragma once

#include <stdint.h>

// =============================================================================
// MODOS DE OPERACION
// =============================================================================
enum mode_t { LSB, USB, CW, FM, AM };
#define CW_MODE CW
#define AM_MODE AM
#define FM_MODE FM

// =============================================================================
// HARDWARE MODEL (WHITE_BUTTONS activo)
// =============================================================================
#define WHITE_BUTTONS 1

// =============================================================================
// CONSTANTES DE RELOJ / SI5351
// =============================================================================
#define F_MCU 20000000  // MCU a 20MHz (overclock; 16MHz para Uno/Nano stock)
#define F_XTAL 27000000 // Si5351 crystal (27MHz standard White Buttons)

#define SI5351_ADDR 0x60

// =============================================================================
// FEATURES (equivalentes a los activos en v1)
// =============================================================================
#define TX_ENABLE 1
#define SEMI_QSK 1
#define RIT_ENABLE 1
#define VOX_ENABLE 1

#define KEY_CLICK 1
#define CW_DECODER 1
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1

#define CAT 1
#define CAT_FAST 1

#define KEEP_BAND_DATA 1

#define PTX 11
#define TX_DELAY 1

// =============================================================================
// CONSTANTES DE TX (SI_CLK_OE), para WHITE_BUTTONS (CLK2 driven PA)
// =============================================================================
#define TX1RX0 0b11111011
#define TX1RX1 0b11111000
#define TX0RX1 0b11111100
#define TX0RX0 0b11111111

// =============================================================================
// CALLSIGN / OPERADOR
// =============================================================================
#define MY_CALLSIGN "CALLID"
#define MY_CALLSIGN_PADDED "uSDX    "
#define MY_PREFIX ""
#define MY_NAME "MY_NAME"
#define CALLSIGN_LENGTH 6

#define CW_MESSAGE_LENGTH 48
#define CW_STD_MSG "CQ CQ DE " MY_CALLSIGN " +"
#define CW_MSG1 "CQ CQ DE " MY_CALLSIGN " +"
#define CW_MSG2 "CQ CQ DE " MY_PREFIX MY_CALLSIGN " +"
#define CW_MSG3 MY_PREFIX MY_CALLSIGN
#define CW_MSG4 "GE TKS 5NN 5NN NAME IS " MY_NAME " HW?"
#define CW_MSG5 "FB RPTR TX 5W ANT EFW 73 CUAGN"
#define CW_MSG6 "73 GL TU EE"