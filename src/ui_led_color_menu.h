#pragma once

#include <lvgl.h>

#include "manager_led.h"

enum class UiLedColorAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

void ui_led_color_menu_init();
lv_obj_t *ui_led_color_menu_get_screen();

// Call right before switching to this screen; captures pre-edit strip state
// (used by CANCEL) and forces a full-brightness live preview.
void ui_led_color_menu_on_enter(LedStrip strip);

UiLedColorAction ui_led_color_menu_handle_inputs(int32_t enc1_delta,
                                                 int32_t enc2_delta,
                                                 bool enc1_pressed,
                                                 bool enc2_pressed);
