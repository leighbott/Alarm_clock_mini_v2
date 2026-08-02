#pragma once

#include <RTClib.h>

void     rtc_manager_init();

// Returns false if RTC is not detected or lost power
bool     rtc_manager_is_ok();

DateTime rtc_manager_get_time();
void     rtc_manager_set_time(const DateTime &dt);
