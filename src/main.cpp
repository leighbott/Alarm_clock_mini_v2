#include <Arduino.h>
#include "lgfx_user.hpp"
#include <lvgl.h>
#include "input_manager.h"
#include "rtc_manager.h"
#include "sensor_manager.h"
#include "storage_manager.h"
#include "led_manager.h"
#include "audio_manager.h"
#include "ui_main_screen.h"

static LGFX display;

// ── LVGL draw buffers ─────────────────────────────────────────────────────────
static const uint32_t LV_BUF_ROWS  = 40;
static const uint32_t LV_BUF_BYTES = 320 * LV_BUF_ROWS * sizeof(uint16_t);
static uint8_t *lv_buf1 = nullptr;
static uint8_t *lv_buf2 = nullptr;

static uint32_t lv_tick_cb() { return (uint32_t)millis(); }

static void lv_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    display.startWrite();
    display.setAddrWindow(area->x1, area->y1, w, h);
    display.writePixels((lgfx::rgb565_t *)px_map, w * h);
    display.endWrite();
    lv_display_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // 1. NVS — before anything reads settings
    storage_manager_init();

    // 2. Audio + SD — must own SPI bus before LovyanGFX takes it over
    audio_manager_set_volume(storage_manager_get().alarm_volume);
    audio_manager_init();

    // 3. Display
    display.init();
    display.setRotation(1);
    display.setBrightness(128);

    // 4. LVGL
    lv_init();
    lv_tick_set_cb(lv_tick_cb);

    lv_buf1 = (uint8_t *)heap_caps_malloc(LV_BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_buf2 = (uint8_t *)heap_caps_malloc(LV_BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lv_buf1 || !lv_buf2) {
        heap_caps_free(lv_buf1); heap_caps_free(lv_buf2);
        lv_buf1 = (uint8_t *)malloc(LV_BUF_BYTES);
        lv_buf2 = (uint8_t *)malloc(LV_BUF_BYTES);
    }

    lv_display_t *lv_disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(lv_disp, lv_flush_cb);
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2, LV_BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 5. I2C peripherals
    rtc_manager_init();
    sensor_manager_init();

    // 6. LEDs — restore saved state
    {
        AppSettings &s = storage_manager_get();
        led_manager_init(s.led_front_brightness, s.led_back_brightness,
                         s.led_front_enabled,    s.led_back_enabled);
    }

    // 7. Input
    input_manager_init();

    // 8. Main screen
    ui_main_screen_init();
    ui_main_screen_update();   // first paint before loop starts

    Serial.println("Boot OK");
}

void loop() {
    input_manager_update();
    audio_manager_loop();

    // 1-second tick
    static uint32_t last_sec = 0;
    if (millis() - last_sec >= 1000) {
        last_sec = millis();
        ui_main_screen_update();
    }

    lv_task_handler();
    delay(5);
}
