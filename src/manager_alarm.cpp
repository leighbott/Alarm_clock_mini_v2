#include "manager_alarm.h"

#include "manager_audio.h"
#include "manager_led.h"
#include "manager_rtc.h"
#include "manager_storage.h"
#include "ui_alarm_active_window.h"
#include "ui_main_screen.h"

#include <Arduino.h>

namespace {

static AlarmState g_state = AlarmState::IDLE;

static uint8_t g_base_audio_volume = 0;
static uint8_t g_base_led_front = 0;
static uint8_t g_base_led_back = 0;
static bool g_base_led_front_on = false;
static bool g_base_led_back_on = false;

static uint32_t g_ramp_start_ms = 0;
static uint32_t g_active_start_ms = 0;
static uint32_t g_last_match_stamp = 0;
static uint32_t g_snooze_until_epoch = 0;

static bool g_any_btn_prev_held = false;
static uint32_t g_press_start_ms = 0;
static bool g_hold_dismiss_latched = false;

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t ramp_from_one_to_end(uint8_t end_value, uint32_t elapsed_ms, uint32_t total_ms) {
    if (total_ms == 0U) return end_value;
    if (elapsed_ms >= total_ms) return end_value;
    if (end_value <= 1U) return end_value;

    return (uint8_t)(1U + ((uint32_t)(end_value - 1U) * elapsed_ms) / total_ms);
}

static bool repeat_allows_today(uint8_t repeat_mask, uint8_t rtc_dow) {
    // repeat_mask: bit7=once, bit0..6=Sun..Sat
    if (repeat_mask & 0x80U) {
        return true;
    }
    if (rtc_dow > 6) return false;
    return (repeat_mask & (1U << rtc_dow)) != 0;
}

static uint32_t epoch_minutes(const DateTime &dt) {
    return dt.unixtime() / 60U;
}

static void restore_pre_alarm_outputs() {
    audio_manager_set_volume(g_base_audio_volume);

    if (led_manager_is_front_on() != g_base_led_front_on) {
        led_manager_toggle_front();
    }
    if (led_manager_is_back_on() != g_base_led_back_on) {
        led_manager_toggle_back();
    }

    led_manager_set_front(g_base_led_front);
    led_manager_set_back(g_base_led_back);
}

static void apply_alarm_outputs(uint8_t volume_pct, uint8_t led_value) {
    if (!led_manager_is_front_on()) {
        led_manager_toggle_front();
    }
    if (!led_manager_is_back_on()) {
        led_manager_toggle_back();
    }

    led_manager_set_front(led_value);
    led_manager_set_back(led_value);
    audio_manager_set_volume(volume_pct);
}

static void stop_alarm(bool return_home) {
    audio_manager_stop();
    restore_pre_alarm_outputs();

    g_state = AlarmState::ARMED;
    g_any_btn_prev_held = false;
    g_hold_dismiss_latched = false;
    ui_alarm_active_window_set_hold_progress(0.0f);

    if (return_home) {
        lv_screen_load(ui_main_screen_get_screen());
    }
}

static void trigger_alarm_now() {
    const AppSettings &s = storage_manager_get();

    g_base_audio_volume = audio_manager_get_volume();
    g_base_led_front = led_manager_get_front();
    g_base_led_back = led_manager_get_back();
    g_base_led_front_on = led_manager_is_front_on();
    g_base_led_back_on = led_manager_is_back_on();

    g_ramp_start_ms = millis();
    g_active_start_ms = 0;
    g_hold_dismiss_latched = false;
    g_any_btn_prev_held = false;

    const uint8_t start_vol = 1;
    const uint8_t start_led = 1;
    apply_alarm_outputs(start_vol, start_led);

    const char *alarm_path = (s.alarm_mp3[0] != '\0') ? s.alarm_mp3 : "/test.mp3";
    audio_manager_play_loop(alarm_path);

    ui_alarm_active_window_set_hint(s.snooze_enabled);
    ui_alarm_active_window_set_hold_progress(0.0f);
    ui_alarm_active_window_show();

    g_state = AlarmState::RAMP_UP;
}

static void maybe_complete_once_alarm_for_today(const DateTime &now) {
    AppSettings &s = storage_manager_get();
    if (s.repeat_mode & 0x80U) {
        s.alarm_enabled = false;
        storage_manager_save_alarm();
    }
    g_last_match_stamp = epoch_minutes(now);
}

} // namespace

void alarm_manager_init() {
    const AppSettings &s = storage_manager_get();
    g_state = s.alarm_enabled ? AlarmState::ARMED : AlarmState::IDLE;

    ui_alarm_active_window_init();
    ui_alarm_active_window_set_hold_progress(0.0f);
}

void alarm_manager_check_trigger() {
    AppSettings &s = storage_manager_get();

    if (!s.alarm_enabled) {
        g_state = AlarmState::IDLE;
        return;
    }

    if (g_state == AlarmState::IDLE) {
        g_state = AlarmState::ARMED;
    }

    DateTime now = rtc_manager_get_time();

    if (g_state == AlarmState::SNOOZING) {
        if (now.unixtime() >= g_snooze_until_epoch) {
            trigger_alarm_now();
        }
        return;
    }

    if (g_state != AlarmState::ARMED) {
        return;
    }

    const uint32_t minute_stamp = epoch_minutes(now);
    if (g_last_match_stamp == minute_stamp) {
        return;
    }

    if ((uint8_t)now.hour() != s.alarm_hour || (uint8_t)now.minute() != s.alarm_minute) {
        return;
    }

    if (!repeat_allows_today(s.repeat_mode, (uint8_t)now.dayOfTheWeek())) {
        g_last_match_stamp = minute_stamp;
        return;
    }

    trigger_alarm_now();
    maybe_complete_once_alarm_for_today(now);
}

void alarm_manager_update(bool enc1_held, bool enc2_held) {
    const AppSettings &s = storage_manager_get();

    if (g_state == AlarmState::IDLE || g_state == AlarmState::ARMED || g_state == AlarmState::SNOOZING) {
        return;
    }

    const uint32_t now_ms = millis();

    if (g_state == AlarmState::RAMP_UP || g_state == AlarmState::ACTIVE) {
        const uint16_t vol_ramp_min = clamp_u16(s.alarm_vol_ramp_min, 0, 30);
        const uint16_t sun_ramp_min = clamp_u16(s.alarm_sun_ramp_min, 0, 30);
        const uint32_t vol_ramp_ms = (uint32_t)vol_ramp_min * 60U * 1000U;
        const uint32_t sun_ramp_ms = (uint32_t)sun_ramp_min * 60U * 1000U;
        const uint32_t elapsed = now_ms - g_ramp_start_ms;

        const uint8_t vol_end = clamp_u8(s.alarm_volume, 0, 100);
        const uint8_t led_end = clamp_u8(s.alarm_end_brightness, 0, 255);

        const uint8_t vol_now = ramp_from_one_to_end(vol_end, elapsed, vol_ramp_ms);
        const uint8_t led_now = ramp_from_one_to_end(led_end, elapsed, sun_ramp_ms);

        if (elapsed >= vol_ramp_ms && elapsed >= sun_ramp_ms) {
            g_state = AlarmState::ACTIVE;
            if (g_active_start_ms == 0) {
                g_active_start_ms = now_ms;
            }
        } else {
            g_state = AlarmState::RAMP_UP;
        }

        apply_alarm_outputs(vol_now, led_now);

        if (g_active_start_ms != 0 && (now_ms - g_active_start_ms) >= (5U * 60U * 1000U)) {
            stop_alarm(true);
            return;
        }
    }

    const bool any_held = enc1_held || enc2_held;

    if (any_held && !g_any_btn_prev_held) {
        g_press_start_ms = now_ms;
        g_hold_dismiss_latched = false;
        ui_alarm_active_window_set_hold_progress(0.0f);
    }

    if (any_held) {
        const uint32_t held_ms = now_ms - g_press_start_ms;
        const uint32_t hold_target_ms = (uint32_t)clamp_u8(s.hold_dismiss_sec, 1, 10) * 1000U;
        const float progress = (hold_target_ms == 0) ? 1.0f
            : ((held_ms >= hold_target_ms) ? 1.0f : (float)held_ms / (float)hold_target_ms);
        ui_alarm_active_window_set_hold_progress(progress);

        if (!g_hold_dismiss_latched && held_ms >= hold_target_ms) {
            g_hold_dismiss_latched = true;
            stop_alarm(true);
            return;
        }
    }

    if (!any_held && g_any_btn_prev_held) {
        const uint32_t held_ms = now_ms - g_press_start_ms;
        ui_alarm_active_window_set_hold_progress(0.0f);

        // Tap-to-snooze if quick release and snooze enabled.
        if (!g_hold_dismiss_latched && s.snooze_enabled && held_ms <= 300U) {
            audio_manager_stop();
            restore_pre_alarm_outputs();
            g_state = AlarmState::SNOOZING;
            g_snooze_until_epoch = rtc_manager_get_time().unixtime() + ((uint32_t)clamp_u16(s.snooze_duration_min, 1, 120) * 60U);
            lv_screen_load(ui_main_screen_get_screen());
            g_any_btn_prev_held = false;
            return;
        }
    }

    g_any_btn_prev_held = any_held;
}

AlarmState alarm_manager_get_state() {
    return g_state;
}

bool alarm_manager_is_alarm_active() {
    return g_state == AlarmState::RAMP_UP || g_state == AlarmState::ACTIVE;
}
