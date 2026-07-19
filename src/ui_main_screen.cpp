#include "ui_main_screen.h"
#include "rtc_manager.h"
#include "sensor_manager.h"
#include "storage_manager.h"
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

// ── Init ──────────────────────────────────────────────────────────────────────
void ui_main_screen_init() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // ── Time (large) ──────────────────────────────────────────────────────────
    lbl_time = lv_label_create(scr);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_time, COL_PRIMARY, 0);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, -20, 8);

    // ── AM/PM (above seconds, to the right of time) ────────────────────────
    lbl_ampm = lv_label_create(scr);
    lv_label_set_text(lbl_ampm, "AM");
    lv_obj_set_style_text_font(lbl_ampm, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_ampm, COL_PRIMARY, 0);
    lv_obj_align_to(lbl_ampm, lbl_time, LV_ALIGN_OUT_RIGHT_TOP, 6, 4);

    // ── Seconds (below AM/PM) ────────────────────────────────────────────
    lbl_secs = lv_label_create(scr);
    lv_label_set_text(lbl_secs, "00");
    lv_obj_set_style_text_font(lbl_secs, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_secs, COL_DIM, 0);
    lv_obj_align_to(lbl_secs, lbl_ampm, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 2);

    // ── Date ─────────────────────────────────────────────────────────────────
    lbl_date = lv_label_create(scr);
    lv_label_set_text(lbl_date, "---");
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_date, COL_DIM, 0);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 72);

    // ── Divider ───────────────────────────────────────────────────────────────
    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, 280, 1);
    lv_obj_set_style_bg_color(line, COL_DIM, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_30, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 98);

    // ── Next alarm ────────────────────────────────────────────────────────────
    lbl_alarm = lv_label_create(scr);
    lv_label_set_text(lbl_alarm, "Alarm  OFF");
    lv_obj_set_style_text_font(lbl_alarm, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_alarm, COL_ACCENT, 0);
    lv_obj_align(lbl_alarm, LV_ALIGN_TOP_MID, 0, 110);

    // ── Time until alarm ──────────────────────────────────────────────────────
    lbl_until = lv_label_create(scr);
    lv_label_set_text(lbl_until, "");
    lv_obj_set_style_text_font(lbl_until, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_until, COL_DIM, 0);
    lv_obj_align(lbl_until, LV_ALIGN_TOP_MID, 0, 138);

    // ── Divider ───────────────────────────────────────────────────────────────
    lv_obj_t *line2 = lv_obj_create(scr);
    lv_obj_set_size(line2, 280, 1);
    lv_obj_set_style_bg_color(line2, COL_DIM, 0);
    lv_obj_set_style_bg_opa(line2, LV_OPA_30, 0);
    lv_obj_set_style_border_width(line2, 0, 0);
    lv_obj_set_style_pad_all(line2, 0, 0);
    lv_obj_align(line2, LV_ALIGN_TOP_MID, 0, 162);

    // ── Temperature ───────────────────────────────────────────────────────────
    lbl_temp = lv_label_create(scr);
    lv_label_set_text(lbl_temp, "--.-\xc2\xb0\x43");  // "--.-°C"
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_temp, COL_PRIMARY, 0);
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_LEFT, 30, 175);

    // ── Humidity ──────────────────────────────────────────────────────────────
    lbl_hum = lv_label_create(scr);
    lv_label_set_text(lbl_hum, "--%");
    lv_obj_set_style_text_font(lbl_hum, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_hum, COL_PRIMARY, 0);
    lv_obj_align(lbl_hum, LV_ALIGN_TOP_RIGHT, -30, 175);
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
        const AppSettings &s = storage_manager_get();
        if (s.alarm_enabled) {
            snprintf(buf, sizeof(buf), "Alarm  %02d:%02d", s.alarm_hour, s.alarm_minute);
            lv_label_set_text(lbl_alarm, buf);
            lv_obj_set_style_text_color(lbl_alarm, COL_ACCENT, 0);

            // Calculate minutes until alarm
            int32_t now_mins   = now.hour()     * 60 + now.minute();
            int32_t alarm_mins = s.alarm_hour   * 60 + s.alarm_minute;
            int32_t diff       = alarm_mins - now_mins;
            if (diff <= 0) diff += 24 * 60;

            int32_t h = diff / 60;
            int32_t m = diff % 60;
            if (h > 0) snprintf(buf, sizeof(buf), "in %ldh %ldm", h, m);
            else        snprintf(buf, sizeof(buf), "in %ldm",          m);
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
}
