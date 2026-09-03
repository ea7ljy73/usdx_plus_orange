// usdx_settings.h - uSDX Plus Orange v2 (modular)
// Configuración: modelo de hardware + features (mismo estilo que
// usdx-legazy/usdx_settings.h). Cada feature activa/inactiva documentada.
// El código compila SIEMPRE la ruta activa (sin #ifdef de hardware dentro
// de los módulos); aquí se centraliza qué se compila.
//
// El conjunto ACTIVO es EXACTAMENTE el de la build activa de usdx-legazy
// (verificado variable a variable). Única excepción documentada: DIAG (el v2
// no porta la suite de diagnóstico de arranque de legacy).

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
// HARDWARE MODEL (WHITE_BUTTONS activo; alternativas comentadas como legacy)
// =============================================================================
#define WHITE_BUTTONS 1
// #define BLACK_BRICK  1
// #define RED_CORNERS  1
// #define RED_BUTTONS  1
// #define TRUSDX       1

// =============================================================================
// CONSTANTES DE RELOJ / SI5351
// =============================================================================
#define F_MCU 20000000  // 20MHz (overclock uSDX+); 16MHz para Uno/Nano stock
// #define F_MCU 16000000
#define F_XTAL 27000000 // cristal SI5351 27MHz
// #define F_XTAL 25004000  // 25MHz SI5351 break-out (WB2CBA-uSDX, uSDXDuO)
// #define F_XTAL 25000000  // 25MHz TCXO
#define SI5351_ADDR 0x60

// =============================================================================
// FEATURES - ACTIVAS (igual que la build activa de usdx-legazy)
// =============================================================================
#define KEYER          1   // CW keyer
#define CAT            1   // CAT-interface
#define CW_DECODER     1   // CW decoder
#define TX_ENABLE      1   // transmit (RX-only si se quita)
#define KEY_CLICK      1   // envelope shaping anti key-clicks
#define SEMI_QSK       1   // RX mute corto tras keying
#define RIT_ENABLE     1   // RIT
#define VOX_ENABLE     1   // VOX
#define CW_MESSAGE     1   // mensajes CW predefinidos (CQ Interval / Message)
#define CW_INTERMEDIATE 1  // CW decoder muestra caracteres intermedios
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1 // filtro 8-bandas con TCA/PCA9555

// =============================================================================
// FEATURES - INACTIVAS (comentadas, igual que legacy)
// =============================================================================
// #define DIAG           1   // startup diagnostics (v2 NO lo porta; divergencia documentada)
// #define OLED_SSD1306   1
// #define OLED_SH1106    1
// #define LCD_I2C        1
// #define QCX            1
// #define CAT_EXT        1
// #define CAT_STREAMING  1
// #define MOX_ENABLE     1
// #define FAST_AGC       1
// #define VSS_METER      1
// #define SWR_METER      1
// #define INA219_POWER_METER 1
// #define ONEBUTTON      1
// #define DEBUG          1
// #define TESTBENCH      1
// #define CW_FREQS_QRP   1
// #define CW_FREQS_FISTS 1
// #define CW_MESSAGE_EXT 1
// #define CONDENSED      1
// #define SWAP_ROTARY    1
// #define TX_CLK0_CLK1   1
// #define F_CLK2         12000000
// #define NTX            11  // PB3 LOW on TX (PTT externo)
// #define PTX            11  // PB3 HIGH on TX (PTT externo)
// #define TX_DELAY       1   // delay pre-key de relés

// El offset CW TX/RX es RUNTIME (cw_offset = tones[cw_tone], legacy 5079);
// NO hay constante CW_OFFSET compilable.

// =============================================================================
// VFO / PERSISTENCIA
// =============================================================================
#define KEEP_BAND_DATA 1 // memoria de banda por frecuencia/modo (v1/v2)

// SWAP_ROTARY: INACTIVO (igual que usdx-legazy). Encoder ROT_A=PD6, ROT_B=PD7.
// Si el sentido sale invertido en hardware, invertir aquí ROT_A<->ROT_B.

// =============================================================================
// PINES (WHITE_BUTTONS) - mismos que v1 orange / usdx-legazy
// =============================================================================
#define ROT_A 6    // PD6 (pin 12)
#define ROT_B 7    // PD7 (pin 13)
#define LCD_D4 0   // PD0 (pin 2)
#define LCD_D5 1   // PD1 (pin 3)
#define LCD_D6 2   // PD2 (pin 4)
#define LCD_D7 3   // PD3 (pin 5)
#define LCD_EN 4   // PD4 (pin 6)
#define LCD_RS 18  // PC4 (pin 27) [comparte SI5351 SDA]
#define KEY_OUT 10 // PB2 (pin 16)
#define SIDETONE 9 // PB1 (pin 15)
#define RX 8       // PB0 (pin 14)
#define DAH 12     // PB4 (pin 18)
#define DIT 13     // PB5 (pin 19)
#define BUTTONS 17 // PC3/A3 (pin 26)
#define AUDIO1 14  // PC0/A0 (pin 23)
#define AUDIO2 15  // PC1/A1 (pin 24)
#define DVM 16     // PC2/A2 (pin 25)
#define SCL 15     // AUX SCL alias para DVM read
#define SIG_OUT 7  // PD7 alias reservado

// =============================================================================
// CONSTANTES DE TX (SI_CLK_OE) - WHITE_BUTTONS (PA en CLK2)
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
