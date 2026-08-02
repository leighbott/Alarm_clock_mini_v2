#pragma once

#include <cstdint>
#include <lvgl.h>

enum class UiTimeDateAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

void ui_time_date_init();
lv_obj_t *ui_time_date_get_screen();
void ui_time_date_on_enter();
UiTimeDateAction ui_time_date_handle_inputs(int32_t enc1_delta,
                                            int32_t enc2_delta,
                                            bool enc1_pressed,
                                            bool enc2_pressed);
