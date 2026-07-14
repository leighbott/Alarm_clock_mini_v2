#pragma once

// =============================================================================
// Pin Definitions — Alarm Clock Mini V2
// Generated from pin_definitions.csv
// =============================================================================

// ── Display 1 (ST7789V, FSPI) ─────────────────────────────────────────────────
#define PIN_TFT_BLK     9    // Backlight PWM
#define PIN_TFT_RES     14   // Reset
#define PIN_TFT_DC      13   // Data/Command
#define PIN_TFT_CS      12   // Chip Select
#define PIN_TFT_MOSI    10   // MOSI / SDA (shared with SD)
#define PIN_TFT_SCK     11   // SCK / CLK  (shared with SD)

// ── SD Card (shared FSPI bus) ─────────────────────────────────────────────────
#define PIN_SD_MISO     18
#define PIN_SD_CS       38

// ── I2C — Wire1 (RTC, AHT20, BMP280) ─────────────────────────────────────────
#define PIN_I2C_SDA     47
#define PIN_I2C_SCL     48

// ── I2S Audio (MAX98357A) ─────────────────────────────────────────────────────
#define PIN_I2S_BCLK    41
#define PIN_I2S_LRCLK   42
#define PIN_I2S_DOUT    21

// ── LED Channels (LEDC PWM via MOSFET) ───────────────────────────────────────
#define PIN_LED_FRONT   16
#define PIN_LED_BACK    17

// ── Light Sensor (LDR — ADC) ─────────────────────────────────────────────────
#define PIN_LDR         15

// ── Rotary Encoder 1 ─────────────────────────────────────────────────────────
#define PIN_ENC1_A      1
#define PIN_ENC1_B      2
#define PIN_ENC1_BTN    43   // TX (IO43) — active LOW, internal pullup

// ── Rotary Encoder 2 ─────────────────────────────────────────────────────────
#define PIN_ENC2_A      40
#define PIN_ENC2_B      39
#define PIN_ENC2_BTN    44   // RX (IO44) — active LOW, internal pullup
