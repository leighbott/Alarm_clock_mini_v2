#include "ui_quick_alarm_menu.h"

#include "manager_storage.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;
static constexpr uint8_t FIELD_COUNT = 3;
static constexpr int CONTENT_PAD_X = 10;
static constexpr int CONTENT_GAP_X = 10;
static constexpr int COL_W = (DISP_W - (2 * CONTENT_PAD_X) - ((FIELD_COUNT - 1) * CONTENT_GAP_X)) / FIELD_COUNT;
static constexpr int TITLE_H = 34;
static constexpr int VALUE_W = 96;
static constexpr int VALUE_H = 68;
static constexpr int VALUE_Y = 32;

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_header_accept_bg = nullptr;
static lv_timer_t *g_header_flash_timer = nullptr;
static lv_timer_t *g_pending_action_timer = nullptr;
static UiQuickAlarmAction g_pending_action = UiQuickAlarmAction::NONE;
static UiQuickAlarmAction g_deferred_action = UiQuickAlarmAction::NONE;

static lv_obj_t *g_value_widgets[FIELD_COUNT] = {nullptr};
static lv_obj_t *g_title_labels[FIELD_COUNT] = {nullptr};
static lv_obj_t *g_value_labels[FIELD_COUNT] = {nullptr};
static lv_obj_t *g_arc_widgets[FIELD_COUNT] = {nullptr};

static UiQuickAlarmState g_state = {false, 7, 0, UiQuickAlarmField::ENABLED};

static void hide_header_flash() {
    if (g_header_cancel_bg) lv_obj_set_style_bg_opa(g_header_cancel_bg, LV_OPA_TRANSP, 0);
    if (g_header_accept_bg) lv_obj_set_style_bg_opa(g_header_accept_bg, LV_OPA_TRANSP, 0);
}

static void header_flash_timer_cb(lv_timer_t *timer) {
    (void)timer;
    hide_header_flash();
    g_header_flash_timer = nullptr;
}

static void trigger_header_flash(bool accept) {
    hide_header_flash();
    if (accept) {
        if (g_header_accept_bg) lv_obj_set_style_bg_opa(g_header_accept_bg, LV_OPA_COVER, 0);
    } else {
        if (g_header_cancel_bg) lv_obj_set_style_bg_opa(g_header_cancel_bg, LV_OPA_COVER, 0);
    }

    if (g_header_flash_timer) lv_timer_del(g_header_flash_timer);
    g_header_flash_timer = lv_timer_create(header_flash_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_header_flash_timer, 1);
}

static void pending_action_timer_cb(lv_timer_t *timer) {
    (void)timer;
    g_pending_action_timer = nullptr;
    g_pending_action = g_deferred_action;
    g_deferred_action = UiQuickAlarmAction::NONE;
}

static void queue_action_after_flash(UiQuickAlarmAction action) {
    g_deferred_action = action;
    if (g_pending_action_timer) lv_timer_del(g_pending_action_timer);
    g_pending_action_timer = lv_timer_create(pending_action_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_pending_action_timer, 1);
}

static uint8_t selected_index() {
    return (uint8_t)g_state.selected_field;
}

static uint8_t wrap_u8(uint8_t value, int32_t delta, uint8_t min_v, uint8_t max_v) {
    const int32_t range = (int32_t)max_v - (int32_t)min_v + 1;
    int32_t normalized = ((int32_t)value - (int32_t)min_v + delta) % range;
    if (normalized < 0) normalized += range;
    return (uint8_t)((int32_t)min_v + normalized);
}

static const char *field_title(uint8_t idx) {
    static const char *titles[FIELD_COUNT] = {"Alarm", "Hour", "Minute"};
    return titles[idx];
}

static void adjust_selected_field(int32_t delta) {
    if (delta == 0) return;

    switch (g_state.selected_field) {
        case UiQuickAlarmField::ENABLED:
            g_state.enabled = delta > 0;
            break;
        case UiQuickAlarmField::HOUR:
            g_state.hour = wrap_u8(g_state.hour, delta, 0, 23);
            break;
        case UiQuickAlarmField::MINUTE:
            g_state.minute = wrap_u8(g_state.minute, delta, 0, 59);
            break;
    }
}

static void update_focus() {
    const uint8_t sel = selected_index();
    for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
        if (!g_value_widgets[i]) continue;
        if (i == sel) lv_obj_add_state(g_value_widgets[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_value_widgets[i], LV_STATE_FOCUSED);
    }
}

static void update_widgets() {
    for (uint8_t idx = 0; idx < FIELD_COUNT; ++idx) {
        if (!g_value_widgets[idx] || !g_title_labels[idx] || !g_value_labels[idx]) continue;

        lv_label_set_text(g_title_labels[idx], field_title(idx));

        char value[16];
        switch ((UiQuickAlarmField)idx) {
            case UiQuickAlarmField::ENABLED:
                std::snprintf(value, sizeof(value), "%s", g_state.enabled ? "ON" : "OFF");
                lv_obj_set_style_bg_color(g_value_widgets[idx],
                                          g_state.enabled ? lv_color_make(0x00, 0x9A, 0x3A)
                                                           : lv_color_make(0xB0, 0x20, 0x20),
                                          LV_PART_MAIN);
                break;
            case UiQuickAlarmField::HOUR:
                std::snprintf(value, sizeof(value), "%02u", (unsigned)g_state.hour);
                lv_obj_set_style_bg_color(g_value_widgets[idx], lv_color_make(0x16, 0x16, 0x16), LV_PART_MAIN);
                if (g_arc_widgets[idx]) {
                    lv_arc_set_rotation(g_arc_widgets[idx], 270);
                    lv_arc_set_range(g_arc_widgets[idx], 0, 12);
                    lv_arc_set_value(g_arc_widgets[idx], g_state.hour % 12);
                }
                break;
            case UiQuickAlarmField::MINUTE:
                std::snprintf(value, sizeof(value), "%02u", (unsigned)g_state.minute);
                lv_obj_set_style_bg_color(g_value_widgets[idx], lv_color_make(0x16, 0x16, 0x16), LV_PART_MAIN);
                if (g_arc_widgets[idx]) {
                    lv_arc_set_rotation(g_arc_widgets[idx], 270);
                    lv_arc_set_range(g_arc_widgets[idx], 0, 59);
                    lv_arc_set_value(g_arc_widgets[idx], g_state.minute);
                }
                break;
        }
        lv_label_set_text(g_value_labels[idx], value);
    }

    update_focus();
}

static void save_state_to_storage() {
    AppSettings &settings = storage_manager_get();
    settings.alarm_enabled = g_state.enabled;
    settings.alarm_hour = g_state.hour;
    settings.alarm_minute = g_state.minute;
    storage_manager_save_alarm();
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

    lv_obj_t *cancel_bg = lv_obj_create(header);
    lv_obj_set_size(cancel_bg, 96, 24);
    lv_obj_align(cancel_bg, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(cancel_bg, lv_color_make(0xB0, 0x20, 0x20), 0);
    lv_obj_set_style_bg_opa(cancel_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(cancel_bg, 4, 0);
    lv_obj_set_style_border_width(cancel_bg, 0, 0);
    lv_obj_set_style_pad_all(cancel_bg, 0, 0);
    lv_obj_set_scrollable(cancel_bg, false);

    lv_obj_t *lbl_cancel = lv_label_create(cancel_bg);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_center(lbl_cancel);
    lv_obj_set_clickable(lbl_cancel, false);
    lv_obj_set_click_focusable(lbl_cancel, false);

    lv_obj_t *lbl_title = lv_label_create(header);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_clickable(lbl_title, false);
    lv_obj_set_click_focusable(lbl_title, false);

    lv_obj_t *accept_bg = lv_obj_create(header);
    lv_obj_set_size(accept_bg, 96, 24);
    lv_obj_align(accept_bg, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(accept_bg, lv_color_make(0x00, 0x9A, 0x3A), 0);
    lv_obj_set_style_bg_opa(accept_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(accept_bg, 4, 0);
    lv_obj_set_style_border_width(accept_bg, 0, 0);
    lv_obj_set_style_pad_all(accept_bg, 0, 0);
    lv_obj_set_scrollable(accept_bg, false);

    lv_obj_t *lbl_accept = lv_label_create(accept_bg);
    lv_label_set_text(lbl_accept, "Accept");
    lv_obj_set_style_text_font(lbl_accept, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_accept, lv_color_white(), 0);
    lv_obj_center(lbl_accept);
    lv_obj_set_clickable(lbl_accept, false);
    lv_obj_set_click_focusable(lbl_accept, false);

    g_header_cancel_bg = cancel_bg;
    g_header_accept_bg = accept_bg;
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

    lv_obj_t *widget = lv_obj_create(col);
    lv_obj_set_size(widget, VALUE_W, VALUE_H);
    lv_obj_set_pos(widget, (COL_W - VALUE_W) / 2, VALUE_Y);
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

    if (slot == (uint8_t)UiQuickAlarmField::HOUR || slot == (uint8_t)UiQuickAlarmField::MINUTE) {
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
        g_arc_widgets[slot] = arc;
    }

    lv_obj_t *value_label = lv_label_create(widget);
    lv_label_set_text(value_label, "-");
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_set_width(value_label, VALUE_W - 8);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(value_label);

    g_value_widgets[slot] = widget;
    g_title_labels[slot] = title_label;
    g_value_labels[slot] = value_label;
}

} // namespace

void ui_quick_alarm_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    apply_header_base(g_screen, "Quick Alarm");

    lv_obj_t *content = lv_obj_create(g_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    for (uint8_t i = 0; i < FIELD_COUNT; ++i) {
        const int x = CONTENT_PAD_X + i * (COL_W + CONTENT_GAP_X);
        create_column(content, x, i);
    }

    update_widgets();
}

lv_obj_t *ui_quick_alarm_get_screen() {
    return g_screen;
}

void ui_quick_alarm_on_enter() {
    const AppSettings &settings = storage_manager_get();
    g_state.enabled = settings.alarm_enabled;
    g_state.hour = settings.alarm_hour;
    g_state.minute = settings.alarm_minute;
    g_state.selected_field = UiQuickAlarmField::ENABLED;
    update_widgets();
}

UiQuickAlarmAction ui_quick_alarm_handle_inputs(int32_t enc1_delta,
                                                int32_t enc2_delta,
                                                bool enc1_pressed,
                                                bool enc2_pressed) {
    if (g_pending_action != UiQuickAlarmAction::NONE) {
        UiQuickAlarmAction out = g_pending_action;
        g_pending_action = UiQuickAlarmAction::NONE;
        return out;
    }

    if (g_pending_action_timer) {
        return UiQuickAlarmAction::NONE;
    }

    if (enc1_pressed) {
        trigger_header_flash(false);
        queue_action_after_flash(UiQuickAlarmAction::CANCEL);
        return UiQuickAlarmAction::NONE;
    }

    if (enc1_delta != 0) {
        const int8_t direction = (enc1_delta > 0) ? 1 : -1;
        int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
        while (steps-- > 0) {
            int16_t next = (int16_t)selected_index() + direction;
            if (next < 0) next = FIELD_COUNT - 1;
            if (next >= FIELD_COUNT) next = 0;
            g_state.selected_field = (UiQuickAlarmField)next;
        }
    }

    if (enc2_delta != 0) {
        adjust_selected_field(enc2_delta);
    }

    update_widgets();

    if (enc2_pressed) {
        trigger_header_flash(true);
        save_state_to_storage();
        queue_action_after_flash(UiQuickAlarmAction::ACCEPT);
        return UiQuickAlarmAction::NONE;
    }

    return UiQuickAlarmAction::NONE;
}
