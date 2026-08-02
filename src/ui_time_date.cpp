#include "ui_time_date.h"

#include "manager_rtc.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;

static constexpr uint8_t TIME_ROLLER_COUNT = 5;
static constexpr uint16_t YEAR_MIN = 2024;
static constexpr uint16_t YEAR_MAX = 2099;

static constexpr int ROLLER_PAD_X = 4;
static constexpr int ROLLER_GAP_X = 4;
static constexpr int ROLLER_COL_W = (DISP_W - (2 * ROLLER_PAD_X) - (4 * ROLLER_GAP_X)) / 5;
static constexpr int ROLLER_LABEL_H = 16;
static constexpr int ROLLER_Y = 18;
static constexpr int ROLLER_H = CONTENT_H - ROLLER_Y - 8;

struct TimePickerState {
    uint8_t hour;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    uint8_t selected_roller_idx;
};

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_rollers[TIME_ROLLER_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static TimePickerState g_state = {0, 0, 1, 1, YEAR_MIN, 0};

static char g_hour_options[128];
static char g_minute_options[256];
static char g_day_options[160];
static char g_month_options[64];
static char g_year_options[640];

static bool is_leap_year(uint16_t year) {
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static uint8_t days_in_month(uint16_t year, uint8_t month) {
    switch (month) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        case 2:
            return is_leap_year(year) ? 29 : 28;
        default:
            return 31;
    }
}

static void clamp_date_to_valid(TimePickerState &state) {
    if (state.year < YEAR_MIN) state.year = YEAR_MIN;
    if (state.year > YEAR_MAX) state.year = YEAR_MAX;

    if (state.month < 1) state.month = 1;
    if (state.month > 12) state.month = 12;

    if (state.hour > 23) state.hour = 23;
    if (state.minute > 59) state.minute = 59;

    const uint8_t dim = days_in_month(state.year, state.month);
    if (state.day < 1) state.day = 1;
    if (state.day > dim) state.day = dim;

    if (state.selected_roller_idx >= TIME_ROLLER_COUNT) state.selected_roller_idx = 0;
}

static uint8_t wrap_u8(uint8_t value, int32_t delta, uint8_t min_v, uint8_t max_v) {
    const int32_t range = (int32_t)max_v - (int32_t)min_v + 1;
    int32_t normalized = ((int32_t)value - (int32_t)min_v + delta) % range;
    if (normalized < 0) normalized += range;
    return (uint8_t)((int32_t)min_v + normalized);
}

static uint16_t wrap_u16(uint16_t value, int32_t delta, uint16_t min_v, uint16_t max_v) {
    const int32_t range = (int32_t)max_v - (int32_t)min_v + 1;
    int32_t normalized = ((int32_t)value - (int32_t)min_v + delta) % range;
    if (normalized < 0) normalized += range;
    return (uint16_t)((int32_t)min_v + normalized);
}

static void build_numeric_options(char *buf,
                                  size_t buf_size,
                                  int min_v,
                                  int max_v,
                                  int width) {
    if (!buf || buf_size == 0) return;

    size_t used = 0;
    buf[0] = '\0';

    for (int v = min_v; v <= max_v; ++v) {
        const bool is_last = (v == max_v);
        const int written = std::snprintf(
            buf + used,
            buf_size - used,
            is_last ? "%0*d" : "%0*d\n",
            width,
            v);
        if (written <= 0 || (size_t)written >= (buf_size - used)) {
            break;
        }
        used += (size_t)written;
    }
}

static void highlight_selected_roller() {
    for (uint8_t i = 0; i < TIME_ROLLER_COUNT; ++i) {
        if (!g_rollers[i]) continue;
        if (i == g_state.selected_roller_idx) lv_obj_add_state(g_rollers[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_rollers[i], LV_STATE_FOCUSED);
    }
}

static void update_day_roller_options() {
    if (!g_rollers[2]) return;

    build_numeric_options(g_day_options,
                          sizeof(g_day_options),
                          1,
                          days_in_month(g_state.year, g_state.month),
                          2);
    lv_roller_set_options(g_rollers[2], g_day_options, LV_ROLLER_MODE_NORMAL);

    clamp_date_to_valid(g_state);
    lv_roller_set_selected(g_rollers[2], (uint16_t)(g_state.day - 1), LV_ANIM_OFF);
}

static void update_rollers_from_state() {
    if (!g_rollers[0] || !g_rollers[1] || !g_rollers[2] || !g_rollers[3] || !g_rollers[4]) {
        return;
    }

    clamp_date_to_valid(g_state);

    lv_roller_set_selected(g_rollers[0], g_state.hour, LV_ANIM_OFF);
    lv_roller_set_selected(g_rollers[1], g_state.minute, LV_ANIM_OFF);
    lv_roller_set_selected(g_rollers[3], (uint16_t)(g_state.month - 1), LV_ANIM_OFF);
    lv_roller_set_selected(g_rollers[4], (uint16_t)(g_state.year - YEAR_MIN), LV_ANIM_OFF);

    update_day_roller_options();
    lv_roller_set_selected(g_rollers[2], (uint16_t)(g_state.day - 1), LV_ANIM_OFF);
    highlight_selected_roller();
}

static void sync_state_from_rollers() {
    if (!g_rollers[0] || !g_rollers[1] || !g_rollers[2] || !g_rollers[3] || !g_rollers[4]) {
        return;
    }

    g_state.hour = (uint8_t)lv_roller_get_selected(g_rollers[0]);
    g_state.minute = (uint8_t)lv_roller_get_selected(g_rollers[1]);
    g_state.day = (uint8_t)(lv_roller_get_selected(g_rollers[2]) + 1);
    g_state.month = (uint8_t)(lv_roller_get_selected(g_rollers[3]) + 1);
    g_state.year = (uint16_t)(YEAR_MIN + lv_roller_get_selected(g_rollers[4]));

    clamp_date_to_valid(g_state);
}

static void adjust_selected_roller_value(int32_t delta) {
    if (delta == 0) return;

    switch (g_state.selected_roller_idx) {
        case 0:
            g_state.hour = wrap_u8(g_state.hour, delta, 0, 23);
            break;
        case 1:
            g_state.minute = wrap_u8(g_state.minute, delta, 0, 59);
            break;
        case 2: {
            const uint8_t dim = days_in_month(g_state.year, g_state.month);
            g_state.day = wrap_u8(g_state.day, delta, 1, dim);
            break;
        }
        case 3:
            g_state.month = wrap_u8(g_state.month, delta, 1, 12);
            clamp_date_to_valid(g_state);
            break;
        case 4:
            g_state.year = wrap_u16(g_state.year, delta, YEAR_MIN, YEAR_MAX);
            clamp_date_to_valid(g_state);
            break;
        default:
            break;
    }

    update_rollers_from_state();
}

static void apply_header_base(lv_obj_t *screen, const char *title) {
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_set_size(header, DISP_W, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_scrollable(header, false);

    lv_obj_t *lbl_cancel = lv_label_create(header);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_align(lbl_cancel, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_clickable(lbl_cancel, false);
    lv_obj_set_click_focusable(lbl_cancel, false);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_clickable(lbl_title, false);
    lv_obj_set_click_focusable(lbl_title, false);

    lv_obj_t *lbl_accept = lv_label_create(header);
    lv_label_set_text(lbl_accept, "Accept");
    lv_obj_set_style_text_font(lbl_accept, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_accept, lv_color_white(), 0);
    lv_obj_align(lbl_accept, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_clickable(lbl_accept, false);
    lv_obj_set_click_focusable(lbl_accept, false);

    lv_obj_t *divider = lv_obj_create(screen);
    lv_obj_set_size(divider, DISP_W, 1);
    lv_obj_set_pos(divider, 0, HEADER_H - 1);
    lv_obj_set_style_bg_color(divider, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_50, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_pad_all(divider, 0, 0);
    lv_obj_set_scrollable(divider, false);
}

static void create_time_roller(lv_obj_t *parent,
                               int x,
                               const char *label_text,
                               uint8_t idx) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, ROLLER_COL_W, CONTENT_H);
    lv_obj_set_pos(col, x, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_scrollable(col, false);

    lv_obj_t *label = lv_label_create(col);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_make(0xDD, 0xDD, 0xDD), 0);
    lv_obj_set_size(label, ROLLER_COL_W, ROLLER_LABEL_H);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_clickable(label, false);
    lv_obj_set_click_focusable(label, false);

    lv_obj_t *roller = lv_roller_create(col);
    lv_obj_set_size(roller, ROLLER_COL_W, ROLLER_H);
    lv_obj_set_pos(roller, 0, ROLLER_Y);
    lv_obj_set_style_bg_color(roller, lv_color_make(0x18, 0x18, 0x18), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(roller, lv_color_make(0x44, 0x44, 0x44), LV_PART_MAIN);
    lv_obj_set_style_pad_ver(roller, 2, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_make(0xAA, 0xAA, 0xAA), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_make(0x30, 0x30, 0x30), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_80, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(roller, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(roller, 1, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(roller, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(roller, LV_OPA_70, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_scrollable(roller, false);
    lv_obj_set_clickable(roller, false);
    lv_obj_set_click_focusable(roller, false);
    lv_roller_set_visible_row_count(roller, 3);

    g_rollers[idx] = roller;
}

static lv_obj_t *build_screen() {
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_set_size(screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_scrollable(screen, false);

    apply_header_base(screen, "Set Time & Date");

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    static const char *labels[TIME_ROLLER_COUNT] = {"Hour", "Min", "Day", "Mon", "Year"};
    for (uint8_t i = 0; i < TIME_ROLLER_COUNT; ++i) {
        const int x = ROLLER_PAD_X + i * (ROLLER_COL_W + ROLLER_GAP_X);
        create_time_roller(content, x, labels[i], i);
    }

    build_numeric_options(g_hour_options, sizeof(g_hour_options), 0, 23, 2);
    build_numeric_options(g_minute_options, sizeof(g_minute_options), 0, 59, 2);
    build_numeric_options(g_month_options, sizeof(g_month_options), 1, 12, 2);
    build_numeric_options(g_year_options, sizeof(g_year_options), YEAR_MIN, YEAR_MAX, 4);

    lv_roller_set_options(g_rollers[0], g_hour_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_options(g_rollers[1], g_minute_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_options(g_rollers[3], g_month_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_options(g_rollers[4], g_year_options, LV_ROLLER_MODE_NORMAL);

    update_day_roller_options();

    return screen;
}

} // namespace

void ui_time_date_init() {
    if (g_screen) return;
    g_screen = build_screen();
}

lv_obj_t *ui_time_date_get_screen() {
    return g_screen;
}

void ui_time_date_on_enter() {
    const DateTime now = rtc_manager_get_time();
    g_state.hour = (uint8_t)now.hour();
    g_state.minute = (uint8_t)now.minute();
    g_state.day = (uint8_t)now.day();
    g_state.month = (uint8_t)now.month();
    g_state.year = (uint16_t)now.year();
    g_state.selected_roller_idx = 0;

    update_rollers_from_state();
}

UiTimeDateAction ui_time_date_handle_inputs(int32_t enc1_delta,
                                            int32_t enc2_delta,
                                            bool enc1_pressed,
                                            bool enc2_pressed) {
    if (enc1_pressed) {
        return UiTimeDateAction::CANCEL;
    }

    if (enc2_pressed) {
        sync_state_from_rollers();
        const DateTime new_dt(
            g_state.year,
            g_state.month,
            g_state.day,
            g_state.hour,
            g_state.minute,
            0);
        rtc_manager_set_time(new_dt);
        return UiTimeDateAction::ACCEPT;
    }

    if (enc1_delta != 0) {
        const int8_t direction = (enc1_delta > 0) ? 1 : -1;
        int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
        while (steps-- > 0) {
            int16_t next = (int16_t)g_state.selected_roller_idx + direction;
            if (next < 0) next = (int16_t)TIME_ROLLER_COUNT - 1;
            if (next >= TIME_ROLLER_COUNT) next = 0;
            g_state.selected_roller_idx = (uint8_t)next;
        }
        highlight_selected_roller();
    }

    if (enc2_delta != 0) {
        adjust_selected_roller_value(enc2_delta);
    }

    return UiTimeDateAction::NONE;
}
