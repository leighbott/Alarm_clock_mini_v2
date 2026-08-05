#include "ui_time_date.h"

#include "manager_rtc.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;

static constexpr uint8_t FIELD_COUNT = 5;
static constexpr uint16_t YEAR_MIN = 2024;
static constexpr uint16_t YEAR_MAX = 2099;

static constexpr int CONTENT_PAD_X = 4;
static constexpr int CONTENT_GAP_X = 4;
static constexpr int COL_W = (DISP_W - (2 * CONTENT_PAD_X) - (4 * CONTENT_GAP_X)) / 5;
static constexpr int TITLE_H = 20;
static constexpr int VALUE_W = 78;
static constexpr int VALUE_H = 78;
static constexpr int VALUE_Y = 24;

enum class TimeField : uint8_t {
    HOUR = 0,
    MINUTE,
    DAY,
    MONTH,
    YEAR,
};

struct TimePickerState {
    uint8_t hour;
    uint8_t minute;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    TimeField selected;
};

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_value_widgets[FIELD_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *g_arc_widgets[FIELD_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *g_value_labels[FIELD_COUNT] = {nullptr, nullptr, nullptr, nullptr, nullptr};

static TimePickerState g_state = {0, 0, 1, 1, YEAR_MIN, TimeField::HOUR};

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

static void clamp_state(TimePickerState &state) {
    if (state.year < YEAR_MIN) state.year = YEAR_MIN;
    if (state.year > YEAR_MAX) state.year = YEAR_MAX;

    if (state.month < 1) state.month = 1;
    if (state.month > 12) state.month = 12;

    if (state.hour > 23) state.hour = 23;
    if (state.minute > 59) state.minute = 59;

    const uint8_t dim = days_in_month(state.year, state.month);
    if (state.day < 1) state.day = 1;
    if (state.day > dim) state.day = dim;
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

static uint8_t selected_index() {
    return (uint8_t)g_state.selected;
}

static const char *field_title(uint8_t idx) {
    static const char *titles[FIELD_COUNT] = {"Hour", "Min", "Day", "Month", "Year"};
    return titles[idx];
}

static const char *month_short_name(uint8_t month) {
    static const char *names[] = {
        "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    return (month >= 1 && month <= 12) ? names[month] : "---";
}

static void field_value_and_range(uint8_t idx, int &value, int &min_v, int &max_v, char *buf, size_t buf_len) {
    value = 0;
    min_v = 0;
    max_v = 100;

    switch ((TimeField)idx) {
        case TimeField::HOUR:
            value = (int)g_state.hour;
            min_v = 0;
            max_v = 23;
            std::snprintf(buf, buf_len, "%02u", (unsigned)g_state.hour);
            break;
        case TimeField::MINUTE:
            value = (int)g_state.minute;
            min_v = 0;
            max_v = 59;
            std::snprintf(buf, buf_len, "%02u", (unsigned)g_state.minute);
            break;
        case TimeField::DAY:
            value = (int)g_state.day;
            min_v = 1;
            max_v = (int)days_in_month(g_state.year, g_state.month);
            std::snprintf(buf, buf_len, "%02u", (unsigned)g_state.day);
            break;
        case TimeField::MONTH:
            value = (int)g_state.month;
            min_v = 1;
            max_v = 12;
            std::snprintf(buf, buf_len, "%s", month_short_name(g_state.month));
            break;
        case TimeField::YEAR:
            value = (int)g_state.year;
            min_v = YEAR_MIN;
            max_v = YEAR_MAX;
            std::snprintf(buf, buf_len, "%04u", (unsigned)g_state.year);
            break;
    }
}

static void update_focus() {
    for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
        if (!g_value_widgets[i]) continue;
        if (i == selected_index()) lv_obj_add_state(g_value_widgets[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_value_widgets[i], LV_STATE_FOCUSED);
    }
}

static void update_widgets() {
    clamp_state(g_state);

    char text[8];
    for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
        if (!g_arc_widgets[i] || !g_value_labels[i]) continue;

        int value;
        int min_v;
        int max_v;
        field_value_and_range(i, value, min_v, max_v, text, sizeof(text));

        if ((TimeField)i == TimeField::HOUR) {
            lv_arc_set_range(g_arc_widgets[i], 0, 11);
            lv_arc_set_value(g_arc_widgets[i], value % 12);
        } else {
            lv_arc_set_range(g_arc_widgets[i], min_v, max_v);
            lv_arc_set_value(g_arc_widgets[i], value);
        }
        lv_label_set_text(g_value_labels[i], text);
    }

    update_focus();
}

static void adjust_selected_value(int32_t delta) {
    if (delta == 0) return;

    switch (g_state.selected) {
        case TimeField::HOUR:
            g_state.hour = wrap_u8(g_state.hour, delta, 0, 23);
            break;
        case TimeField::MINUTE:
            g_state.minute = wrap_u8(g_state.minute, delta, 0, 59);
            break;
        case TimeField::DAY: {
            const uint8_t dim = days_in_month(g_state.year, g_state.month);
            g_state.day = wrap_u8(g_state.day, delta, 1, dim);
            break;
        }
        case TimeField::MONTH:
            g_state.month = wrap_u8(g_state.month, delta, 1, 12);
            clamp_state(g_state);
            break;
        case TimeField::YEAR:
            g_state.year = wrap_u16(g_state.year, delta, YEAR_MIN, YEAR_MAX);
            clamp_state(g_state);
            break;
    }

    update_widgets();
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
    lv_obj_set_style_pad_top(header, 2, 0);
    lv_obj_set_style_pad_left(header, 2, 0);
    lv_obj_set_style_pad_right(header, 2, 0);
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
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
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
}

static lv_obj_t *create_value_shell(lv_obj_t *parent, int x) {
    lv_obj_t *widget = lv_obj_create(parent);
    lv_obj_set_size(widget, VALUE_W, VALUE_H);
    lv_obj_set_pos(widget, x + ((COL_W - VALUE_W) / 2), VALUE_Y);
    lv_obj_set_style_bg_color(widget, lv_color_make(0x16, 0x16, 0x16), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(widget, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(widget, lv_color_make(0x3A, 0x3A, 0x3A), LV_PART_MAIN);
    lv_obj_set_style_outline_width(widget, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(widget, false);

    lv_obj_set_style_border_width(widget, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(widget, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(widget, 1, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(widget, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(widget, LV_OPA_70, LV_PART_MAIN | LV_STATE_FOCUSED);

    return widget;
}

static void create_field_column(lv_obj_t *parent, int x, uint8_t idx) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, COL_W, CONTENT_H);
    lv_obj_set_pos(col, x, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_scrollable(col, false);

    lv_obj_t *label = lv_label_create(col);
    lv_label_set_text(label, field_title(idx));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_make(0xDD, 0xDD, 0xDD), 0);
    lv_obj_set_size(label, COL_W, TITLE_H);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_clickable(label, false);
    lv_obj_set_click_focusable(label, false);

    lv_obj_t *widget = create_value_shell(col, 0);
    g_value_widgets[idx] = widget;

    lv_obj_t *arc = lv_arc_create(widget);
    lv_obj_set_size(arc, 58, 58);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(0x34, 0x34, 0x34), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_make(0x4D, 0xB1, 0xFF), LV_PART_INDICATOR);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_scrollable(arc, false);
    g_arc_widgets[idx] = arc;

    lv_obj_t *value_label = lv_label_create(widget);
    lv_label_set_text(value_label, "00");
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_center(value_label);
    lv_obj_set_clickable(value_label, false);
    lv_obj_set_click_focusable(value_label, false);
    g_value_labels[idx] = value_label;
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

    for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
        const int x = CONTENT_PAD_X + i * (COL_W + CONTENT_GAP_X);
        create_field_column(content, x, i);
    }

    update_widgets();
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
    g_state.selected = TimeField::HOUR;

    clamp_state(g_state);
    update_widgets();
}

UiTimeDateAction ui_time_date_handle_inputs(int32_t enc1_delta,
                                            int32_t enc2_delta,
                                            bool enc1_pressed,
                                            bool enc2_pressed) {
    if (enc1_pressed) {
        return UiTimeDateAction::CANCEL;
    }

    if (enc2_pressed) {
        clamp_state(g_state);
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
            int16_t next = (int16_t)selected_index() + direction;
            if (next < 0) next = (int16_t)FIELD_COUNT - 1;
            if (next >= FIELD_COUNT) next = 0;
            g_state.selected = (TimeField)next;
        }
        update_focus();
    }

    if (enc2_delta != 0) {
        adjust_selected_value(enc2_delta);
    }

    return UiTimeDateAction::NONE;
}
