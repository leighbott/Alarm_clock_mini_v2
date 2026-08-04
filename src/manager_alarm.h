#pragma once

#include <stdint.h>

enum class AlarmState : uint8_t {
    IDLE = 0,
    ARMED,
    RAMP_UP,
    ACTIVE,
    SNOOZING,
};

void alarm_manager_init();

// Call around 1 Hz from loop for RTC trigger checks.
void alarm_manager_check_trigger();

// Call every loop; handles ramping, active-window updates, and input actions.
void alarm_manager_update(bool enc1_held, bool enc2_held);

AlarmState alarm_manager_get_state();
bool alarm_manager_is_alarm_active();
bool alarm_manager_is_alarm_screen_visible();
