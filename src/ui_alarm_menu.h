#pragma once

#include <cstdint>
#include <lvgl.h>

enum class UiAlarmAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

void ui_alarm_init();
lv_obj_t *ui_alarm_get_screen();
void ui_alarm_on_enter();
UiAlarmAction ui_alarm_handle_inputs(int32_t enc1_delta,
                                     int32_t enc2_delta,
                                     bool enc1_pressed,
                                     bool enc2_pressed);
