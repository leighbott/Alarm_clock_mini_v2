#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "pins_config.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX() {
        // ── SPI bus ───────────────────────────────────────────────────────────
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;   // FSPI on ESP32-S3
            cfg.spi_mode   = 0;
            cfg.freq_write = 40000000;    // 40 MHz write
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;       // 4-wire SPI — dedicated DC pin
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = PIN_TFT_SCK;
            cfg.pin_mosi   = PIN_TFT_MOSI;
            cfg.pin_miso   = PIN_SD_MISO; // shared with SD card — must be set here
            cfg.pin_dc     = PIN_TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ── Panel ─────────────────────────────────────────────────────────────
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = PIN_TFT_CS;
            cfg.pin_rst          = PIN_TFT_RES;
            cfg.pin_busy         = -1;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;   // ST7789V typically needs inversion
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;   // shared with SD card
            _panel_instance.config(cfg);
        }

        // ── Backlight ─────────────────────────────────────────────────────────
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl       = PIN_TFT_BLK;
            cfg.invert       = false;
            cfg.freq         = 44100;
            cfg.pwm_channel  = 0;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};
