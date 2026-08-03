#pragma once

#include <cstdint>
#include <lvgl.h>

enum class UiDisplayAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

enum class UiDisplayField : uint8_t {
    MIN_BRIGHTNESS = 0,
    MANUAL_BRIGHTNESS,
    AUTO_BRIGHTNESS,
    DISPLAY_BOOST,
};

struct UiDisplayState {
    bool min_brightness_off;
    uint8_t manual_brightness_percent;
    bool auto_brightness;
    uint8_t boost_brightness_percent;
    UiDisplayField selected_field;
};

void ui_display_init();
lv_obj_t *ui_display_get_screen();
void ui_display_on_enter();
UiDisplayAction ui_display_handle_inputs(int32_t enc1_delta,
                                         int32_t enc2_delta,
                                         bool enc1_pressed,
                                         bool enc2_pressed);