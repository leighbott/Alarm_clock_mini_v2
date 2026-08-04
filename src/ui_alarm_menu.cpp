#include "ui_alarm_menu.h"

#include "manager_storage.h"

#include <cstdio>
#include <string.h>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;

static constexpr uint8_t FIELD_COUNT = 12;
static constexpr uint8_t VISIBLE_COLS = 4;
static constexpr uint8_t REPEAT_ONCE = 0x80;

static constexpr int CONTENT_PAD_X = 6;
static constexpr int CONTENT_GAP_X = 4;
static constexpr int COL_W = (DISP_W - (2 * CONTENT_PAD_X) - ((VISIBLE_COLS - 1) * CONTENT_GAP_X)) / VISIBLE_COLS;
static constexpr int TITLE_H = 28;
static constexpr int VALUE_W = 96;
static constexpr int VALUE_H = 68;
static constexpr int VALUE_Y = 32;

enum class AlarmField : uint8_t {
    ENABLED = 0,
    HOUR,
    MINUTE,
    SOUND,
    END_VOL,
    END_SUN,
    VOL_RAMP,
    SUN_RAMP,
    SNOOZE,
    SNOOZE_DURATION,
    HOLD_TO_DISMISS,
    REPEAT,
};

struct UiAlarmState {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t end_vol_pct;
    uint8_t end_sun_pct;
    uint8_t vol_ramp_min;
    uint8_t sun_ramp_min;
    bool snooze_enabled;
    uint8_t snooze_min;
    uint8_t hold_sec;
    uint8_t repeat_mask;
    char sound_path[64];
    AlarmField selected;
};

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_value_widgets[VISIBLE_COLS] = {nullptr};
static lv_obj_t *g_title_labels[VISIBLE_COLS] = {nullptr};
static lv_obj_t *g_value_labels[VISIBLE_COLS] = {nullptr};
static lv_obj_t *g_value_arcs[VISIBLE_COLS] = {nullptr};

static lv_obj_t *g_repeat_overlay = nullptr;
static lv_obj_t *g_repeat_chip[8] = {nullptr}; // 0=Once, 1..7=Mon..Sun
static lv_obj_t *g_repeat_chip_lbl[8] = {nullptr};
static bool g_repeat_open = false;
static uint8_t g_repeat_focus = 0;

static UiAlarmState g_state = {
    false, 7, 0, 80, 78, 5, 20, true, 9, 3, REPEAT_ONCE, "/test.mp3", AlarmField::ENABLED
};

static uint8_t g_window_start = 0;

static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t wrap_u8(uint8_t value, int32_t delta, uint8_t min_v, uint8_t max_v) {
    const int32_t range = (int32_t)max_v - (int32_t)min_v + 1;
    int32_t normalized = ((int32_t)value - (int32_t)min_v + delta) % range;
    if (normalized < 0) normalized += range;
    return (uint8_t)((int32_t)min_v + normalized);
}

static uint8_t raw_to_percent(uint8_t raw_value) {
    if (raw_value == 0) return 0;
    const int scaled = (int)((raw_value * 100U + 127U) / 255U);
    if (scaled < 0) return 0;
    if (scaled > 100) return 100;
    return (uint8_t)scaled;
}

static uint8_t percent_to_raw(uint8_t percent_value) {
    if (percent_value == 0) return 0;
    const int scaled = (int)((percent_value * 255U + 50U) / 100U);
    if (scaled < 0) return 0;
    if (scaled > 255) return 255;
    return (uint8_t)scaled;
}

static void clamp_state() {
    g_state.hour = clamp_u8(g_state.hour, 0, 23);
    g_state.minute = clamp_u8(g_state.minute, 0, 59);
    g_state.end_vol_pct = clamp_u8(g_state.end_vol_pct, 0, 100);
    g_state.end_sun_pct = clamp_u8(g_state.end_sun_pct, 0, 100);
    g_state.vol_ramp_min = clamp_u8(g_state.vol_ramp_min, 0, 30);
    g_state.sun_ramp_min = clamp_u8(g_state.sun_ramp_min, 0, 30);
    g_state.snooze_min = clamp_u8(g_state.snooze_min, 1, 30);
    g_state.hold_sec = clamp_u8(g_state.hold_sec, 1, 10);
    if (g_state.sound_path[0] == '\0') {
        strncpy(g_state.sound_path, "/test.mp3", sizeof(g_state.sound_path) - 1);
        g_state.sound_path[sizeof(g_state.sound_path) - 1] = '\0';
    }
}

static void repeat_mask_normalize() {
    const uint8_t day_bits = (uint8_t)(g_state.repeat_mask & 0x7F);
    if (g_state.repeat_mask & REPEAT_ONCE) {
        g_state.repeat_mask = REPEAT_ONCE;
    } else if (day_bits == 0) {
        g_state.repeat_mask = REPEAT_ONCE;
    }
}

static const char *field_title(uint8_t idx) {
    static const char *titles[FIELD_COUNT] = {
        "Enabled", "Hour", "Minute", "Sound",
        "End Vol Lvl", "End Sun Lvl", "Vol Ramp", "Sun Ramp",
        "Snooze", "Snooze Dur", "Hold Dismiss", "Repeat"
    };
    return titles[idx];
}

static lv_color_t toggle_color(bool on) {
    return on ? lv_color_make(0x00, 0x9A, 0x3A) : lv_color_make(0xB0, 0x20, 0x20);
}

static bool field_uses_arc(AlarmField field) {
    return field == AlarmField::HOUR ||
           field == AlarmField::MINUTE ||
           field == AlarmField::END_VOL ||
           field == AlarmField::END_SUN;
}

static uint8_t repeat_slot_to_day_bit(uint8_t slot) {
    switch (slot) {
        case 1: return 1; // Mon
        case 2: return 2; // Tue
        case 3: return 3; // Wed
        case 4: return 4; // Thu
        case 5: return 5; // Fri
        case 6: return 6; // Sat
        case 7: return 0; // Sun
        default: return 0;
    }
}

static void field_value_text(uint8_t idx, char *buf, size_t len, lv_color_t &color) {
    color = lv_color_white();

    switch ((AlarmField)idx) {
        case AlarmField::ENABLED:
            std::snprintf(buf, len, "%s", g_state.enabled ? "ON" : "OFF");
            color = toggle_color(g_state.enabled);
            break;
        case AlarmField::HOUR:
            std::snprintf(buf, len, "%02u", (unsigned)g_state.hour);
            break;
        case AlarmField::MINUTE:
            std::snprintf(buf, len, "%02u", (unsigned)g_state.minute);
            break;
        case AlarmField::SOUND:
            std::snprintf(buf, len, "test.mp3");
            break;
        case AlarmField::END_VOL:
            std::snprintf(buf, len, "%u%%", (unsigned)g_state.end_vol_pct);
            break;
        case AlarmField::END_SUN:
            std::snprintf(buf, len, "%u%%", (unsigned)g_state.end_sun_pct);
            break;
        case AlarmField::VOL_RAMP:
            std::snprintf(buf, len, "%umin", (unsigned)g_state.vol_ramp_min);
            break;
        case AlarmField::SUN_RAMP:
            std::snprintf(buf, len, "%umin", (unsigned)g_state.sun_ramp_min);
            break;
        case AlarmField::SNOOZE:
            std::snprintf(buf, len, "%s", g_state.snooze_enabled ? "ON" : "OFF");
            color = toggle_color(g_state.snooze_enabled);
            break;
        case AlarmField::SNOOZE_DURATION:
            std::snprintf(buf, len, "%umin", (unsigned)g_state.snooze_min);
            break;
        case AlarmField::HOLD_TO_DISMISS:
            std::snprintf(buf, len, "%us", (unsigned)g_state.hold_sec);
            break;
        case AlarmField::REPEAT:
            std::snprintf(buf, len, "Select");
            color = lv_color_make(0x6D, 0xC4, 0xFF);
            break;
    }
}

static uint8_t selected_index() {
    return (uint8_t)g_state.selected;
}

static void update_window_for_selection() {
    const uint8_t sel = selected_index();
    if (sel < g_window_start) {
        g_window_start = sel;
    } else if (sel >= (uint8_t)(g_window_start + VISIBLE_COLS)) {
        g_window_start = (uint8_t)(sel - (VISIBLE_COLS - 1));
    }

    const uint8_t max_start = FIELD_COUNT - VISIBLE_COLS;
    if (g_window_start > max_start) g_window_start = max_start;
}

static void update_focus() {
    const uint8_t sel = selected_index();
    for (uint8_t i = 0; i < VISIBLE_COLS; ++i) {
        if (!g_value_widgets[i]) continue;
        const uint8_t idx = (uint8_t)(g_window_start + i);
        if (idx == sel) lv_obj_add_state(g_value_widgets[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_value_widgets[i], LV_STATE_FOCUSED);
    }
}

static void update_repeat_overlay_widgets() {
    if (!g_repeat_overlay) return;

    repeat_mask_normalize();

    for (uint8_t i = 0; i < 8; ++i) {
        if (!g_repeat_chip[i] || !g_repeat_chip_lbl[i]) continue;

        bool on = false;
        if (i == 0) {
            on = (g_state.repeat_mask & REPEAT_ONCE) != 0;
        } else {
            const uint8_t bit = repeat_slot_to_day_bit(i);
            on = ((g_state.repeat_mask & REPEAT_ONCE) == 0) && ((g_state.repeat_mask & (1U << bit)) != 0);
        }

        lv_obj_set_style_bg_color(g_repeat_chip[i],
                                  on ? lv_color_make(0x00, 0x88, 0x36) : lv_color_make(0x44, 0x44, 0x44),
                                  LV_PART_MAIN);
        lv_obj_set_style_border_width(g_repeat_chip[i], (i == g_repeat_focus) ? 2 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_repeat_chip[i],
                                      (i == g_repeat_focus) ? lv_color_white() : lv_color_make(0x66, 0x66, 0x66),
                                      LV_PART_MAIN);
        lv_obj_set_style_text_color(g_repeat_chip_lbl[i], lv_color_white(), 0);
    }
}

static void open_repeat_overlay() {
    if (!g_repeat_overlay) return;
    g_repeat_open = true;
    g_repeat_focus = 0;
    lv_obj_set_hidden(g_repeat_overlay, false);
    update_repeat_overlay_widgets();
}

static void close_repeat_overlay() {
    if (!g_repeat_overlay) return;
    g_repeat_open = false;
    lv_obj_set_hidden(g_repeat_overlay, true);
    update_focus();
}

static void toggle_repeat_focus() {
    if (g_repeat_focus == 0) {
        g_state.repeat_mask = REPEAT_ONCE;
    } else {
        g_state.repeat_mask &= (uint8_t)~REPEAT_ONCE;
        const uint8_t bit = repeat_slot_to_day_bit(g_repeat_focus);
        const uint8_t mask_bit = (uint8_t)(1U << bit);
        if (g_state.repeat_mask & mask_bit) g_state.repeat_mask &= (uint8_t)~mask_bit;
        else g_state.repeat_mask |= mask_bit;

        if ((g_state.repeat_mask & 0x7F) == 0) {
            g_state.repeat_mask = REPEAT_ONCE;
        }
    }
}

static void save_state() {
    AppSettings &s = storage_manager_get();
    s.alarm_enabled = g_state.enabled;
    s.alarm_hour = g_state.hour;
    s.alarm_minute = g_state.minute;
    s.alarm_volume = g_state.end_vol_pct;
    s.alarm_end_brightness = percent_to_raw(g_state.end_sun_pct);
    s.alarm_vol_ramp_min = g_state.vol_ramp_min;
    s.alarm_sun_ramp_min = g_state.sun_ramp_min;
    s.alarm_sun_lead_min = g_state.sun_ramp_min;
    s.snooze_enabled = g_state.snooze_enabled;
    s.snooze_duration_min = g_state.snooze_min;
    s.hold_dismiss_sec = g_state.hold_sec;
    s.repeat_mode = g_state.repeat_mask;

    strncpy(s.alarm_mp3, "/test.mp3", sizeof(s.alarm_mp3) - 1);
    s.alarm_mp3[sizeof(s.alarm_mp3) - 1] = '\0';

    storage_manager_save_alarm();
}

static void adjust_selected_field(int32_t delta) {
    if (delta == 0) return;

    switch (g_state.selected) {
        case AlarmField::ENABLED:
            g_state.enabled = (delta > 0);
            break;
        case AlarmField::HOUR:
            g_state.hour = wrap_u8(g_state.hour, delta, 0, 23);
            break;
        case AlarmField::MINUTE:
            g_state.minute = wrap_u8(g_state.minute, delta, 0, 59);
            break;
        case AlarmField::SOUND:
            strncpy(g_state.sound_path, "/test.mp3", sizeof(g_state.sound_path) - 1);
            g_state.sound_path[sizeof(g_state.sound_path) - 1] = '\0';
            break;
        case AlarmField::END_VOL: {
            int16_t v = (int16_t)g_state.end_vol_pct + (int16_t)delta;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            g_state.end_vol_pct = (uint8_t)v;
            break;
        }
        case AlarmField::END_SUN: {
            int16_t v = (int16_t)g_state.end_sun_pct + (int16_t)delta;
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            g_state.end_sun_pct = (uint8_t)v;
            break;
        }
        case AlarmField::VOL_RAMP: {
            int16_t v = (int16_t)g_state.vol_ramp_min + (int16_t)delta;
            if (v < 0) v = 0;
            if (v > 30) v = 30;
            g_state.vol_ramp_min = (uint8_t)v;
            break;
        }
        case AlarmField::SUN_RAMP: {
            int16_t v = (int16_t)g_state.sun_ramp_min + (int16_t)delta;
            if (v < 0) v = 0;
            if (v > 30) v = 30;
            g_state.sun_ramp_min = (uint8_t)v;
            break;
        }
        case AlarmField::SNOOZE:
            g_state.snooze_enabled = (delta > 0);
            break;
        case AlarmField::SNOOZE_DURATION: {
            int16_t v = (int16_t)g_state.snooze_min + (int16_t)delta;
            if (v < 1) v = 1;
            if (v > 30) v = 30;
            g_state.snooze_min = (uint8_t)v;
            break;
        }
        case AlarmField::HOLD_TO_DISMISS: {
            int16_t v = (int16_t)g_state.hold_sec + (int16_t)delta;
            if (v < 1) v = 1;
            if (v > 10) v = 10;
            g_state.hold_sec = (uint8_t)v;
            break;
        }
        case AlarmField::REPEAT:
            break;
    }
}

static void render_fields() {
    clamp_state();
    update_window_for_selection();

    for (uint8_t slot = 0; slot < VISIBLE_COLS; ++slot) {
        const uint8_t idx = (uint8_t)(g_window_start + slot);
        if (!g_title_labels[slot] || !g_value_labels[slot] || !g_value_arcs[slot]) continue;

        lv_label_set_text(g_title_labels[slot], field_title(idx));

        const AlarmField field = (AlarmField)idx;
        const bool use_arc = field_uses_arc(field);
        lv_obj_set_hidden(g_value_arcs[slot], !use_arc);

        char value[48];
        lv_color_t value_color;
        field_value_text(idx, value, sizeof(value), value_color);
        if (use_arc) {
            int32_t arc_min = 0;
            int32_t arc_max = 100;
            int32_t arc_value = 0;

            switch (field) {
                case AlarmField::HOUR:
                    arc_min = 0;
                    arc_max = 23;
                    arc_value = g_state.hour;
                    std::snprintf(value, sizeof(value), "%02u", (unsigned)g_state.hour);
                    break;
                case AlarmField::MINUTE:
                    arc_min = 0;
                    arc_max = 59;
                    arc_value = g_state.minute;
                    std::snprintf(value, sizeof(value), "%02u", (unsigned)g_state.minute);
                    break;
                case AlarmField::END_VOL:
                    arc_min = 0;
                    arc_max = 100;
                    arc_value = g_state.end_vol_pct;
                    std::snprintf(value, sizeof(value), "%u", (unsigned)g_state.end_vol_pct);
                    break;
                case AlarmField::END_SUN:
                    arc_min = 0;
                    arc_max = 100;
                    arc_value = g_state.end_sun_pct;
                    std::snprintf(value, sizeof(value), "%u", (unsigned)g_state.end_sun_pct);
                    break;
                default:
                    break;
            }

            lv_arc_set_range(g_value_arcs[slot], arc_min, arc_max);
            lv_arc_set_value(g_value_arcs[slot], arc_value);
            lv_obj_set_style_text_font(g_value_labels[slot], &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(g_value_labels[slot], lv_color_white(), 0);
            lv_label_set_text(g_value_labels[slot], value);
        } else {
            lv_obj_set_style_text_font(g_value_labels[slot], &lv_font_montserrat_20, 0);
            lv_label_set_text(g_value_labels[slot], value);
            lv_obj_set_style_text_color(g_value_labels[slot], value_color, 0);
        }
    }

    update_focus();
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

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *lbl_accept = lv_label_create(header);
    lv_label_set_text(lbl_accept, "Accept");
    lv_obj_set_style_text_font(lbl_accept, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_accept, lv_color_white(), 0);
    lv_obj_align(lbl_accept, LV_ALIGN_RIGHT_MID, -10, 0);
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

static void create_column(lv_obj_t *parent, int x, uint8_t slot) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, COL_W, CONTENT_H);
    lv_obj_set_pos(col, x, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_scrollable(col, false);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, "-");
    lv_obj_set_size(title_label, COL_W, TITLE_H);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_make(0xD0, 0xD0, 0xD0), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_clickable(title_label, false);
    lv_obj_set_click_focusable(title_label, false);

    lv_obj_t *widget = create_value_shell(col, 0);

    lv_obj_t *arc = lv_arc_create(widget);
    lv_obj_set_size(arc, 58, 58);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(0x34, 0x34, 0x34), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_make(0x4D, 0xB1, 0xFF), LV_PART_INDICATOR);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    lv_obj_set_scrollable(arc, false);

    lv_obj_t *value_label = lv_label_create(widget);
    lv_label_set_text(value_label, "-");
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_set_width(value_label, VALUE_W - 8);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(value_label);

    g_value_widgets[slot] = widget;
    g_value_arcs[slot] = arc;
    g_title_labels[slot] = title_label;
    g_value_labels[slot] = value_label;
}

static void build_repeat_overlay(lv_obj_t *parent) {
    g_repeat_overlay = lv_obj_create(parent);
    lv_obj_set_size(g_repeat_overlay, DISP_W, DISP_H);
    lv_obj_set_pos(g_repeat_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_repeat_overlay, lv_color_make(0x08, 0x08, 0x08), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_repeat_overlay, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_repeat_overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_repeat_overlay, lv_color_make(0x55, 0x55, 0x55), LV_PART_MAIN);
    lv_obj_set_scrollable(g_repeat_overlay, false);

    lv_obj_t *title = lv_label_create(g_repeat_overlay);
    lv_label_set_text(title, "Repeat Pattern");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    static const char *chip_text[8] = {"Once", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const int y = 64;
    int x = 8;
    for (uint8_t i = 0; i < 8; ++i) {
        g_repeat_chip[i] = lv_obj_create(g_repeat_overlay);
        const int chip_w = (i == 0) ? 76 : 48;
        lv_obj_set_size(g_repeat_chip[i], chip_w, 26);
        lv_obj_set_pos(g_repeat_chip[i], x, y);
        lv_obj_set_style_radius(g_repeat_chip[i], 6, LV_PART_MAIN);
        lv_obj_set_scrollable(g_repeat_chip[i], false);

        g_repeat_chip_lbl[i] = lv_label_create(g_repeat_chip[i]);
        lv_label_set_text(g_repeat_chip_lbl[i], chip_text[i]);
        lv_obj_set_style_text_font(g_repeat_chip_lbl[i], &lv_font_montserrat_16, 0);
        lv_obj_center(g_repeat_chip_lbl[i]);

        x += chip_w + 4;
    }

    lv_obj_set_hidden(g_repeat_overlay, true);
}

} // namespace

void ui_alarm_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    apply_header_base(g_screen, "Alarm");

    lv_obj_t *content = lv_obj_create(g_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    for (uint8_t i = 0; i < VISIBLE_COLS; ++i) {
        const int x = CONTENT_PAD_X + i * (COL_W + CONTENT_GAP_X);
        create_column(content, x, i);
    }

    build_repeat_overlay(g_screen);
    render_fields();
}

lv_obj_t *ui_alarm_get_screen() {
    return g_screen;
}

void ui_alarm_on_enter() {
    const AppSettings &s = storage_manager_get();
    g_state.enabled = s.alarm_enabled;
    g_state.hour = s.alarm_hour;
    g_state.minute = s.alarm_minute;
    g_state.end_vol_pct = clamp_u8(s.alarm_volume, 0, 100);
    g_state.end_sun_pct = raw_to_percent(s.alarm_end_brightness);
    g_state.vol_ramp_min = (uint8_t)((s.alarm_vol_ramp_min > 30) ? 30 : s.alarm_vol_ramp_min);
    g_state.sun_ramp_min = (uint8_t)((s.alarm_sun_ramp_min > 30) ? 30 : s.alarm_sun_ramp_min);
    g_state.snooze_enabled = s.snooze_enabled;
    g_state.snooze_min = (uint8_t)((s.snooze_duration_min > 30) ? 30 : s.snooze_duration_min);
    g_state.hold_sec = s.hold_dismiss_sec;
    g_state.repeat_mask = s.repeat_mode;
    strncpy(g_state.sound_path, (s.alarm_mp3[0] != '\0') ? s.alarm_mp3 : "/test.mp3", sizeof(g_state.sound_path) - 1);
    g_state.sound_path[sizeof(g_state.sound_path) - 1] = '\0';

    g_state.selected = AlarmField::ENABLED;
    g_window_start = 0;
    repeat_mask_normalize();
    close_repeat_overlay();
    render_fields();
}

UiAlarmAction ui_alarm_handle_inputs(int32_t enc1_delta,
                                     int32_t enc2_delta,
                                     bool enc1_pressed,
                                     bool enc2_pressed) {
    if (g_repeat_open) {
        if (enc1_pressed) {
            close_repeat_overlay();
            render_fields();
            return UiAlarmAction::NONE;
        }

        if (enc2_pressed) {
            close_repeat_overlay();
            save_state();
            render_fields();
            return UiAlarmAction::ACCEPT;
        }

        if (enc1_delta != 0) {
            const int8_t direction = (enc1_delta > 0) ? 1 : -1;
            int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
            while (steps-- > 0) {
                int16_t next = (int16_t)g_repeat_focus + direction;
                if (next < 0) next = 7;
                if (next > 7) next = 0;
                g_repeat_focus = (uint8_t)next;
            }
            update_repeat_overlay_widgets();
        }

        if (enc2_delta != 0) {
            toggle_repeat_focus();
            update_repeat_overlay_widgets();
        }

        return UiAlarmAction::NONE;
    }

    if (enc1_pressed) {
        return UiAlarmAction::CANCEL;
    }

    if (enc1_delta != 0) {
        const int8_t direction = (enc1_delta > 0) ? 1 : -1;
        int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
        while (steps-- > 0) {
            int16_t next = (int16_t)selected_index() + direction;
            if (next < 0) next = FIELD_COUNT - 1;
            if (next >= FIELD_COUNT) next = 0;
            g_state.selected = (AlarmField)next;
        }
    }

    if (enc2_delta != 0) {
        if (g_state.selected == AlarmField::REPEAT) {
            open_repeat_overlay();
            return UiAlarmAction::NONE;
        }
        adjust_selected_field(enc2_delta);
    }

    if (enc2_pressed) {
        save_state();
        return UiAlarmAction::ACCEPT;
    }

    render_fields();
    return UiAlarmAction::NONE;
}
