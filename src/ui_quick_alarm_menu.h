#pragma once

#include <cstdint>
#include <lvgl.h>

enum class UiQuickAlarmAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

enum class UiQuickAlarmField : uint8_t {
    ENABLED = 0,
    HOUR,
    MINUTE,
};

struct UiQuickAlarmState {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    UiQuickAlarmField selected_field;
};

void ui_quick_alarm_init();
lv_obj_t *ui_quick_alarm_get_screen();
void ui_quick_alarm_on_enter();
UiQuickAlarmAction ui_quick_alarm_handle_inputs(int32_t enc1_delta,
                                                int32_t enc2_delta,
                                                bool enc1_pressed,
                                                bool enc2_pressed);
