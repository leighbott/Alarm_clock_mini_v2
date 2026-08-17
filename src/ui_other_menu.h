#pragma once

#include <lvgl.h>

enum class UiOtherAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
    ENTER_LED1,
    ENTER_LED2,
};

void ui_other_init();
lv_obj_t *ui_other_get_screen();

void ui_other_on_enter();

// Refreshes tile preview colors; call after returning from the LED color submenu.
void ui_other_refresh_previews();

UiOtherAction ui_other_handle_inputs(int32_t enc1_delta,
                                     int32_t enc2_delta,
                                     bool enc1_pressed,
                                     bool enc2_pressed);
