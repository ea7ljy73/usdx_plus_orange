// usdx_settings.h - User Configuration
// uSDX Plus Orange (based on usdxWHITEBUTTONS v4.00d by GW8RDI)
// Edit this file to customize hardware, callsign, and features.

#pragma once

// =============================================================================
// HARDWARE MODEL - Enable exactly ONE by removing the "//" in front
// =============================================================================
// #define BLACK_BRICK 1    // Backlight PD3 0x08, SWR, no rotary swap
// #define RED_CORNERS 1    // Backlight PD5 0x20 or PD3 0x08, SWR, rotary swap
// #define RED_BUTTONS 1    // Small HF uSDX, no SWR, SMD inductors
#define WHITE_BUTTONS 1 // Small black unit with white/red buttons, no SWR
// #define TRUSDX 1         // DL2MAN/PE1NNZ clone, OLED, latching relays

// =============================================================================
// OPERATOR CONFIGURATION
// =============================================================================
// *** CALLSIGN: max 5 chars + 2 trailing spaces in PADDED version! ***
//#define MY_CALLSIGN "EA7LJY"
//#define MY_CALLSIGN_PADDED "EA7LJY  " // Keep exactly 2 trailing spaces
//#define MY_PREFIX ""                  // Visiting country prefix, e.g. "DL/"
//#define MY_NAME "JULIAN"              // For CW messages
//#define CALLSIGN_LENGTH 6             // Match length of MY_CALLSIGN

#define MY_CALLSIGN "CALLID"
#define MY_CALLSIGN_PADDED "uSDX    " // Keep exactly 2 trailing spaces
#define MY_PREFIX ""                  // Visiting country prefix, e.g. "DL/"
#define MY_NAME "MY_NAME"              // For CW messages
#define CALLSIGN_LENGTH 6             // Match length of MY_CALLSIGN

// =============================================================================
// HARDWARE SETTINGS
// =============================================================================
#define SI5351_ADDR 0x60 // SI5351A I2C address (0x60 most common, 0x62 alt)

// Crystal frequency - set to match your SI5351 chip (NOT the MCU crystal)
// Model-specific defaults are applied in the .ino; override here if needed:
// #define F_XTAL  27005000  // 27MHz with calibration offset
// #define F_XTAL  25004000  // 25MHz with calibration offset (WB2CBA-uSDX)
// #define F_XTAL  25000000  // 25MHz TCXO (Red Corners, Black Brick)
// #define F_XTAL  27000000  // 27MHz standard (White Buttons, Red Buttons)

// MCU crystal frequency (only change if running 16MHz Arduino Uno/Nano stock):
// #define F_MCU   16000000  // 16MHz; default assumed is 20MHz

// =============================================================================
// DISPLAY OPTIONS (choose at most one)
// =============================================================================
// #define OLED_SSD1306  1  // OLED SSD1306 128x32/128x64 via SDA(PD2)/SCL(PD3)
// #define OLED_SH1106   1  // OLED SH1106 1.3" via SDA(PD2)/SCL(PD3)
// #define LCD_I2C       1  // I2C LCD PCF8574 module, slow
// #define CONDENSED     1  // 4-line display mode (for OLED and LCD2004)

// =============================================================================
// FILTER BANK (LPF switching via I2C GPIO expander)
// =============================================================================
#define LPF_SWITCHING_DL2MAN_USDX_REV3 1 // 8-band latching relays (IM43)
// #define LPF_SWITCHING_DL2MAN_USDX_REV3_NOLATCH 1  // 8-band non-latching
// #define LPF_SWITCHING_DL2MAN_USDX_REV2 1           // 5-band latching
// #define LPF_SWITCHING_DL2MAN_USDX_REV2_BETA 1      // 5-band PCA9539PW
// #define LPF_SWITCHING_DL2MAN_USDX_REV1 1           // 3-band PCA9536D
// #define LPF_SWITCHING_WB2CBA_USDX_OCTOBAND 1       // 8-band MCP23008
// #define LPF_SWITCHING_PE1DDA_USDXDUO 14             // 2-band relay on PD5

// =============================================================================
// TX / RX FEATURES
// =============================================================================
#define TX_ENABLE 1  // Enable TX; comment out for RX-only
#define SEMI_QSK 1   // Mute RX briefly after CW key-up
#define RIT_ENABLE 1 // Receive Incremental Tuning (+/- offset)
#define VOX_ENABLE 1 // Voice-activated TX
// #define MOX_ENABLE    1  // Monitor-on-TX (audio feedback during TX)

// =============================================================================
// MEMORY / BAND FEATURES
// =============================================================================
#define KEEP_BAND_DATA 1       // Remember frequency and mode per band
#define SHOW_USB_LSB_CW_ONLY 1 // Menu cycles only LSB / USB / CW modes

// =============================================================================
// CAT INTERFACE
// =============================================================================
#define CAT 1      // Enable CAT control (TS-480 subset)
#define CAT_FAST 1 // 115200 baud (else 38400)
// #define CAT_EXT       1  // Extended: remote button/screen control
// #define CAT_STREAMING 1  // Audio/IQ streaming over CAT

// =============================================================================
// CW FEATURES
// =============================================================================
// #define KEYER         1  // Iambic CW keyer (disabling saves memory for CAT)
#define KEY_CLICK     1  // TX envelope shaping to reduce key clicks
// #define FILTER_700HZ  1  // 700Hz CW tone filter selectable in menu
#define CW_DECODER 1 // CW decoder display
// #define CW_INTERMEDIATE 1  // Show intermediate Morse sequences (LCD only)
// #define CW_FREQS_QRP  1  // Default to QRP CW frequencies on band change
// #define CW_FREQS_FISTS 1 // Default to FISTS CW frequencies on band change
// #define CW_VOLUME     1  // Separate CW volume level in menu

// =============================================================================
// CW MESSAGES
// =============================================================================
// #define CW_MESSAGE    1  // Predefined CW messages (uses memory, check fit)
// #define CW_MESSAGE_EXT 1 // Additional CW messages (see below)
#define CW_MESSAGE_LENGTH 48
#define CW_STD_MSG "CQ CQ DE " MY_CALLSIGN " +"
#define CW_MSG1 "CQ CQ DE " MY_CALLSIGN " +"
#define CW_MSG2 "CQ CQ DE " MY_PREFIX MY_CALLSIGN " +"
#define CW_MSG3 MY_PREFIX MY_CALLSIGN
#define CW_MSG4 "GE TKS 5NN 5NN NAME IS " MY_NAME " HW?"
#define CW_MSG5 "FB RPTR TX 5W ANT EFW 73 CUAGN"
#define CW_MSG6 "73 GL TU EE"

// =============================================================================
// DSP / NOISE REDUCTION
// =============================================================================
// #define NR_FIR        1  // FIR noise reduction (needs space - disable CW
// msgs) #define FAST_AGC      1  // Fast AGC mode option (good for CW) #define
// FM_ARCTAN     1  // FM differentiator (experimental) #define AM_MOD_MAGN_SQRT
// 1  // More accurate AM magnitude (sqrt method)

// =============================================================================
// DIAGNOSTICS / DEBUG
// =============================================================================
// #define DIAG          1  // Hardware diagnostics on startup (+1308 bytes)
// #define DEBUG_G8RDI   1  // Show error codes on LCD (changes callsign to
// DEBUG)

// =============================================================================
// TX PTT OUTPUT
// =============================================================================
#define PTX 11 // HIGH on TX: PTT output on PB3 (pin 17)
// #define NTX 11           // LOW on TX: PTT output on PB3 (alternative logic)
// #define TX_DELAY  1      // Delay before TX for relay switching

// =============================================================================
// ADVANCED / RARELY CHANGED
// =============================================================================
// #define QCX           1  // Older QCX hardware support
// #define ONEBUTTON     1  // Single-button control mode
// #define TX_CLK0_CLK1  1  // uSDXDuO: PA driven by CLK0/CLK1 not CLK2
// #define F_CLK2  12000000 // Fixed CLK2 output (only with TX_CLK0_CLK1)
