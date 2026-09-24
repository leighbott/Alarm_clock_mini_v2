#pragma once

#include <cstdint>
#include <lvgl.h>

enum class UiDisplayAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

enum class UiDisplayField : uint8_t {
    AUTO_BRIGHTNESS = 0,
    MIN_BRIGHTNESS,
    MANUAL_BRIGHTNESS,
    DISPLAY_BOOST,
    LDR_MAX_RAW,
    BOOST_DURATION,
};

struct UiDisplayState {
    bool min_brightness_off;
    uint8_t manual_brightness_percent;
    bool auto_brightness;
    uint8_t boost_brightness_percent;
    uint16_t ldr_max_raw;
    uint16_t boost_duration_ms;
    UiDisplayField selected_field;
};

void ui_display_init();
lv_obj_t *ui_display_get_screen();
void ui_display_on_enter();
UiDisplayAction ui_display_handle_inputs(int32_t enc1_delta,
                                         int32_t enc2_delta,
                                         bool enc1_pressed,
                                         bool enc2_pressed);