#pragma once

#include <stdint.h>
#include <stdbool.h>

// ── All persistent settings ───────────────────────────────────────────────────
struct AppSettings {
    // Alarm
    bool     alarm_enabled;
    uint8_t  alarm_hour;
    uint8_t  alarm_minute;
    char     alarm_mp3[64];          // path on SD, e.g. "/alarm.mp3"
    uint8_t  alarm_volume;           // 0–100 %
    uint8_t  alarm_end_brightness;   // LED brightness at end of sunrise, 0–255
    uint16_t alarm_vol_ramp_min;     // volume ramp duration, minutes
    uint16_t alarm_sun_ramp_min;     // sunrise ramp duration, minutes
    uint16_t alarm_sun_lead_min;     // sunrise lead-in before alarm, minutes
    bool     snooze_enabled;
    uint16_t snooze_duration_min;    // snooze duration, minutes
    uint8_t  hold_dismiss_sec;       // hold-to-dismiss duration, seconds
    uint8_t  repeat_mode;            // 0 = once, 1 = weekdays

    // Display
    bool     auto_brightness;
    bool     min_brightness_off;     // true = screen off at minimum, false = value 1
    uint8_t  boost_brightness;       // 0–255

    // LEDs (last known state — restored on boot)
    uint8_t  led_front_brightness;   // 0–255
    uint8_t  led_back_brightness;    // 0–255
    bool     led_front_enabled;
    bool     led_back_enabled;
};

// ── API ───────────────────────────────────────────────────────────────────────
void              storage_manager_init();   // load from NVS (call before UI init)

AppSettings&      storage_manager_get();   // direct mutable reference

void              storage_manager_save_all();          // write everything to NVS
void              storage_manager_save_alarm();        // alarm fields only
void              storage_manager_save_display();      // display fields only
void              storage_manager_save_leds();         // LED fields only
