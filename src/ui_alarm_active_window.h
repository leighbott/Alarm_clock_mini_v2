#pragma once

#include <lvgl.h>

void ui_alarm_active_window_init();
void ui_alarm_active_window_show();
void ui_alarm_active_window_hide();

void ui_alarm_active_window_set_hint(bool snooze_enabled);
void ui_alarm_active_window_set_hold_progress(float progress_0_to_1);
void ui_alarm_active_window_update_clock();

lv_obj_t *ui_alarm_active_window_get_screen();
