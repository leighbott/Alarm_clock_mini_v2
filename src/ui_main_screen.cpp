#include "ui_main_screen.h"
#include "manager_alarm.h"
#include "manager_brightness.h"
#include "manager_rtc.h"
#include "manager_sensor.h"
#include "manager_storage.h"
#include <lvgl.h>
#include <Arduino.h>
#include <math.h>

// Colours — not constexpr (LVGL colour functions are runtime)
#define COL_BG      lv_color_black()
#define COL_PRIMARY lv_color_white()
#define COL_DIM     lv_color_make(0x88, 0x88, 0x88)
#define COL_ACCENT  lv_color_make(0xFF, 0xB0, 0x00)

// ── Widget handles ────────────────────────────────────────────────────────────
static lv_obj_t *lbl_time     = nullptr;   // "12:34"
static lv_obj_t *lbl_ampm     = nullptr;   // "AM" / "PM"
static lv_obj_t *lbl_secs     = nullptr;   // "56"
static lv_obj_t *lbl_date     = nullptr;   // "Monday, 14th Jul"
static lv_obj_t *lbl_alarm    = nullptr;   // "Alarm  07:00" / "Alarm  OFF"
static lv_obj_t *lbl_until    = nullptr;   // "in 14h 26m"
static lv_obj_t *lbl_temp     = nullptr;   // "23.4°C"
static lv_obj_t *lbl_hum      = nullptr;   // "48%"
static lv_obj_t *lbl_ldr_raw  = nullptr;   // "LDR: 4095"
static lv_obj_t *lbl_brightness = nullptr; // "BR: 255"
static lv_obj_t *main_screen  = nullptr;

static bool colon_visible = true;

// ── Helpers ───────────────────────────────────────────────────────────────────
static const char *ordinal(uint8_t d) {
    if (d >= 11 && d <= 13) return "th";
    switch (d % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

static const char *day_name(uint8_t dow) {
    // RTClib: 0=Sunday
    static const char *days[] = {
        "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
    };
    return (dow < 7) ? days[dow] : "---";
}

static const char *month_name(uint8_t m) {
    static const char *months[] = {
        "","Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    return (m >= 1 && m <= 12) ? months[m] : "---";
}

static bool is_same_calendar_day(const DateTime &a, const DateTime &b) {
    return a.year() == b.year() && a.month() == b.month() && a.day() == b.day();
}

// ── Init ──────────────────────────────────────────────────────────────────────
void ui_main_screen_init() {
    lv_obj_t *scr = lv_screen_active();
    main_screen = scr;
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Left cluster: time/date/alarm
    lbl_time = lv_label_create(scr);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_time, COL_PRIMARY, 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 8, -2);

    lbl_ampm = lv_label_create(scr);
    lv_label_set_text(lbl_ampm, "AM");
    lv_obj_set_style_text_font(lbl_ampm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_ampm, COL_PRIMARY, 0);
    lv_obj_align_to(lbl_ampm, lbl_time, LV_ALIGN_OUT_RIGHT_TOP, 6, 10);

    lbl_secs = lv_label_create(scr);
    lv_label_set_text(lbl_secs, "00");
    lv_obj_set_style_text_font(lbl_secs, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_secs, COL_DIM, 0);
    lv_obj_align_to(lbl_secs, lbl_ampm, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    lbl_date = lv_label_create(scr);
    lv_label_set_text(lbl_date, "---");
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_date, COL_DIM, 0);
    lv_obj_set_width(lbl_date, 280);
    lv_obj_set_style_text_align(lbl_date, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_LEFT, 10, 54);

    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, 276, 1);
    lv_obj_set_style_bg_color(line, COL_DIM, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_30, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 10, 70);

    lbl_alarm = lv_label_create(scr);
    lv_label_set_text(lbl_alarm, "Alarm  OFF");
    lv_obj_set_style_text_font(lbl_alarm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_alarm, COL_ACCENT, 0);
    lv_obj_set_width(lbl_alarm, 276);
    lv_obj_set_style_text_align(lbl_alarm, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lbl_alarm, LV_ALIGN_TOP_LEFT, 10, 76);

    lbl_until = lv_label_create(scr);
    lv_label_set_text(lbl_until, "");
    lv_obj_set_style_text_font(lbl_until, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_until, COL_DIM, 0);
    lv_obj_set_width(lbl_until, 276);
    lv_obj_set_style_text_align(lbl_until, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(lbl_until, LV_ALIGN_TOP_LEFT, 10, 96);

    lv_obj_t *line2 = lv_obj_create(scr);
    lv_obj_set_size(line2, 410, 1);
    lv_obj_set_style_bg_color(line2, COL_DIM, 0);
    lv_obj_set_style_bg_opa(line2, LV_OPA_30, 0);
    lv_obj_set_style_border_width(line2, 0, 0);
    lv_obj_set_style_pad_all(line2, 0, 0);
    lv_obj_align(line2, LV_ALIGN_TOP_MID, 0, 116);

    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--.-\xc2\xb0\x43");  // "--.-°C"
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_temp, COL_PRIMARY, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_RIGHT, -10, 10);

    lbl_hum = lv_label_create(scr);
    lv_label_set_text(lbl_hum, "--%");
    lv_obj_set_style_text_font(lbl_hum, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_hum, COL_PRIMARY, 0);
    lv_obj_align(lbl_hum, LV_ALIGN_TOP_RIGHT, -10, 42);

    lbl_brightness = lv_label_create(scr);
    lv_label_set_text(lbl_brightness, "BR: 000");
    lv_obj_set_style_text_font(lbl_brightness, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_brightness, COL_DIM, 0);
    lv_obj_align(lbl_brightness, LV_ALIGN_BOTTOM_LEFT, 10, -4);

    lbl_ldr_raw = lv_label_create(scr);
    lv_label_set_text(lbl_ldr_raw, "LDR: 0000");
    lv_obj_set_style_text_font(lbl_ldr_raw, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_ldr_raw, COL_DIM, 0);
    lv_obj_align(lbl_ldr_raw, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
}

// ── Update (call every second) ────────────────────────────────────────────────
void ui_main_screen_update() {
    char buf[64];

    // ── Time with flashing colon ──────────────────────────────────────────────
    colon_visible = !colon_visible;

    if (rtc_manager_is_ok()) {
        DateTime now = rtc_manager_get_time();

        // 12-hour conversion
        uint8_t h12 = now.hour() % 12;
        if (h12 == 0) h12 = 12;
        bool is_pm = now.hour() >= 12;

        if (colon_visible) {
            snprintf(buf, sizeof(buf), "%d:%02d", h12, now.minute());
        } else {
            snprintf(buf, sizeof(buf), "%d %02d", h12, now.minute());
        }
        lv_label_set_text(lbl_time, buf);
        lv_label_set_text(lbl_ampm, is_pm ? "PM" : "AM");

        snprintf(buf, sizeof(buf), "%02d", now.second());
        lv_label_set_text(lbl_secs, buf);

        // ── Date ──────────────────────────────────────────────────────────────
        snprintf(buf, sizeof(buf), "%s, %d%s %s",
                 day_name(now.dayOfTheWeek()),
                 now.day(), ordinal(now.day()),
                 month_name(now.month()));
        lv_label_set_text(lbl_date, buf);

        // ── Alarm & time-until ────────────────────────────────────────────────
        DateTime next_alarm;
        bool is_snoozed = false;
        if (alarm_manager_get_next_alarm_time(now, &next_alarm, &is_snoozed)) {
            snprintf(buf, sizeof(buf), "Alarm  %02d:%02d", next_alarm.hour(), next_alarm.minute());
            lv_label_set_text(lbl_alarm, buf);
            lv_obj_set_style_text_color(lbl_alarm, COL_ACCENT, 0);

            if (!is_snoozed && !is_same_calendar_day(now, next_alarm)) {
                snprintf(buf, sizeof(buf), "on %s", day_name(next_alarm.dayOfTheWeek()));
            } else {
                uint32_t diff_seconds = 0;
                if (next_alarm.unixtime() > now.unixtime()) {
                    diff_seconds = next_alarm.unixtime() - now.unixtime();
                }
                const uint32_t diff_minutes = (diff_seconds + 59U) / 60U;
                const uint32_t h = diff_minutes / 60U;
                const uint32_t m = diff_minutes % 60U;
                if (h > 0U) snprintf(buf, sizeof(buf), "in %luh %lum", (unsigned long)h, (unsigned long)m);
                else        snprintf(buf, sizeof(buf), "in %lum",       (unsigned long)m);
            }
            lv_label_set_text(lbl_until, buf);
        } else {
            lv_label_set_text(lbl_alarm, "Alarm  OFF");
            lv_obj_set_style_text_color(lbl_alarm, COL_DIM, 0);
            lv_label_set_text(lbl_until, "");
        }
    } else {
        lv_label_set_text(lbl_time, "RTC ERR");
        lv_label_set_text(lbl_secs, "--");
        lv_label_set_text(lbl_date, "RTC ERROR");
    }

    // ── Sensors ───────────────────────────────────────────────────────────────
    sensor_manager_update();
    const SensorData &sd = sensor_manager_get();

    if (!isnan(sd.temperature))
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0\x43", sd.temperature);
    else
        snprintf(buf, sizeof(buf), "--.-\xc2\xb0\x43");
    lv_label_set_text(lbl_temp, buf);

    if (!isnan(sd.humidity))
        snprintf(buf, sizeof(buf), "%.0f%%", sd.humidity);
    else
        snprintf(buf, sizeof(buf), "--%%" );
    lv_label_set_text(lbl_hum, buf);

    snprintf(buf, sizeof(buf), "BR: %03u", brightness_manager_get_current_brightness());
    lv_label_set_text(lbl_brightness, buf);

    snprintf(buf, sizeof(buf), "LDR: %04u", brightness_manager_get_last_ldr_raw());
    lv_label_set_text(lbl_ldr_raw, buf);
}

lv_obj_t *ui_main_screen_get_screen() {
    return main_screen;
}
