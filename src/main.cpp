#include <Arduino.h>
#include "lgfx_user.hpp"
#include <lvgl.h>
#include "input_manager.h"
#include "rtc_manager.h"

static LGFX display;

// ── LVGL draw buffers (allocated from PSRAM) ──────────────────────────────────
static const uint32_t LV_BUF_ROWS = 40;
static const uint32_t LV_BUF_BYTES = 320 * LV_BUF_ROWS * sizeof(uint16_t);
static uint8_t *lv_buf1 = nullptr;
static uint8_t *lv_buf2 = nullptr;

// ── LVGL tick source ──────────────────────────────────────────────────────────
static uint32_t lv_tick_cb() { return (uint32_t)millis(); }

// ── LVGL flush callback ───────────────────────────────────────────────────────
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

    // ── Display ───────────────────────────────────────────────────────────────
    display.init();
    display.setRotation(1);       // landscape: 320 wide, 240 tall
    display.setBrightness(128);

    // ── LVGL ──────────────────────────────────────────────────────────────────
    lv_init();
    lv_tick_set_cb(lv_tick_cb);

    lv_buf1 = (uint8_t *)heap_caps_malloc(LV_BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    lv_buf2 = (uint8_t *)heap_caps_malloc(LV_BUF_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!lv_buf1 || !lv_buf2) {
        Serial.println("WARN: PSRAM alloc failed, falling back to heap");
        heap_caps_free(lv_buf1); heap_caps_free(lv_buf2);
        lv_buf1 = (uint8_t *)malloc(LV_BUF_BYTES);
        lv_buf2 = (uint8_t *)malloc(LV_BUF_BYTES);
    }
    if (!lv_buf1 || !lv_buf2) {
        Serial.println("ERROR: LVGL buffer alloc failed (heap and PSRAM)");
        while (1) {}
    }
    Serial.printf("LVGL bufs: buf1=%p buf2=%p (PSRAM free: %u)\n",
                  lv_buf1, lv_buf2, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    lv_display_t *lv_disp = lv_display_create(320, 240);
    lv_display_set_flush_cb(lv_disp, lv_flush_cb);
    lv_display_set_buffers(lv_disp, lv_buf1, lv_buf2, LV_BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // ── RTC ───────────────────────────────────────────────────────────────────
    rtc_manager_init();

    // ── Input ─────────────────────────────────────────────────────────────────
    input_manager_init();

    // ── Test label ────────────────────────────────────────────────────────────
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    Serial.println("LVGL OK");
}

void loop() {
    input_manager_update();
    lv_task_handler();

    // ── Stage 5 diagnostic — remove after verification ────────────────────────
    static uint32_t last_print = 0;
    if (millis() - last_print >= 1000) {
        last_print = millis();
        if (rtc_manager_is_ok()) {
            DateTime now = rtc_manager_get_time();
            Serial.printf("RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
        } else {
            Serial.println("RTC ERROR");
        }
    }

    delay(5);
}
