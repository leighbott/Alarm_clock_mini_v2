#include "ui_alarm_active_window.h"

#include "manager_rtc.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HALF_W = DISP_W / 2;

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_lbl_time = nullptr;
static lv_obj_t *g_lbl_ampm = nullptr;
static lv_obj_t *g_lbl_date = nullptr;
static lv_obj_t *g_circle = nullptr;
static lv_obj_t *g_arc = nullptr;
static lv_obj_t *g_lbl_hint = nullptr;

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

static void update_time_date() {
    if (!g_lbl_time || !g_lbl_ampm || !g_lbl_date) return;

    DateTime now = rtc_manager_get_time();
    uint8_t h12 = now.hour() % 12;
    if (h12 == 0) h12 = 12;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%u:%02u", h12, (unsigned)now.minute());
    lv_label_set_text(g_lbl_time, buf);
    lv_label_set_text(g_lbl_ampm, (now.hour() >= 12) ? "PM" : "AM");

    std::snprintf(buf, sizeof(buf), "%s, %u%s %s",
                  day_name(now.dayOfTheWeek()),
                  (unsigned)now.day(),
                  ordinal((uint8_t)now.day()),
                  month_name((uint8_t)now.month()));
    lv_label_set_text(g_lbl_date, buf);
}

} // namespace

void ui_alarm_active_window_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_bg_color(g_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    lv_obj_t *left = lv_obj_create(g_screen);
    lv_obj_set_size(left, HALF_W, DISP_H);
    lv_obj_set_pos(left, 0, 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_scrollable(left, false);

    g_lbl_time = lv_label_create(left);
    lv_label_set_text(g_lbl_time, "12:00");
    lv_obj_set_style_text_font(g_lbl_time, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(g_lbl_time, lv_color_white(), 0);
    lv_obj_align(g_lbl_time, LV_ALIGN_TOP_MID, -12, 10);

    g_lbl_ampm = lv_label_create(left);
    lv_label_set_text(g_lbl_ampm, "AM");
    lv_obj_set_style_text_font(g_lbl_ampm, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_lbl_ampm, lv_color_white(), 0);
    lv_obj_align_to(g_lbl_ampm, g_lbl_time, LV_ALIGN_OUT_RIGHT_TOP, 4, 8);

    g_lbl_date = lv_label_create(left);
    lv_label_set_text(g_lbl_date, "Monday, 1st Jan");
    lv_obj_set_width(g_lbl_date, HALF_W - 10);
    lv_obj_set_style_text_font(g_lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_lbl_date, lv_color_make(0xC0, 0xC0, 0xC0), 0);
    lv_obj_set_style_text_align(g_lbl_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(g_lbl_date, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t *right = lv_obj_create(g_screen);
    lv_obj_set_size(right, HALF_W, DISP_H);
    lv_obj_set_pos(right, HALF_W, 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_scrollable(right, false);

    g_circle = lv_obj_create(right);
    lv_obj_set_size(g_circle, 124, 124);
    lv_obj_center(g_circle);
    lv_obj_set_style_radius(g_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_circle, lv_color_make(0x18, 0x18, 0x18), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_circle, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_circle, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_circle, lv_color_make(0x44, 0x44, 0x44), LV_PART_MAIN);
    lv_obj_set_scrollable(g_circle, false);

    g_arc = lv_arc_create(g_circle);
    lv_obj_set_size(g_arc, 108, 108);
    lv_obj_center(g_arc);
    lv_arc_set_rotation(g_arc, 270);
    lv_arc_set_bg_angles(g_arc, 0, 360);
    lv_arc_set_range(g_arc, 0, 100);
    lv_arc_set_value(g_arc, 0);
    lv_obj_set_style_arc_width(g_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc, lv_color_make(0x2E, 0x2E, 0x2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc, lv_color_make(0xFF, 0x70, 0x00), LV_PART_INDICATOR);
    lv_obj_remove_style(g_arc, nullptr, LV_PART_KNOB);
    lv_obj_set_clickable(g_arc, false);
    lv_obj_set_scrollable(g_arc, false);

    g_lbl_hint = lv_label_create(g_circle);
    lv_label_set_text(g_lbl_hint, "Hold to dismiss\nTap to snooze");
    lv_obj_set_width(g_lbl_hint, 96);
    lv_obj_set_style_text_font(g_lbl_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_lbl_hint, lv_color_white(), 0);
    lv_obj_set_style_text_align(g_lbl_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(g_lbl_hint);

    update_time_date();
}

void ui_alarm_active_window_show() {
    if (!g_screen) ui_alarm_active_window_init();
    update_time_date();
    lv_screen_load(g_screen);
}

void ui_alarm_active_window_hide() {
    // Router will load desired screen after this; no-op by design.
}

void ui_alarm_active_window_set_hint(bool snooze_enabled) {
    if (!g_lbl_hint) return;
    lv_label_set_text(g_lbl_hint,
                      snooze_enabled ? "Hold to dismiss\nTap to snooze"
                                     : "Hold to dismiss");
}

void ui_alarm_active_window_set_hold_progress(float progress_0_to_1) {
    if (!g_arc) return;

    if (progress_0_to_1 < 0.0f) progress_0_to_1 = 0.0f;
    if (progress_0_to_1 > 1.0f) progress_0_to_1 = 1.0f;

    const int value = (int)(progress_0_to_1 * 100.0f + 0.5f);
    lv_arc_set_value(g_arc, value);
    update_time_date();
}

lv_obj_t *ui_alarm_active_window_get_screen() {
    return g_screen;
}
