#include "ui_display_menu.h"

#include "manager_storage.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;
static constexpr int COL_COUNT = 4;
static constexpr int CONTENT_PAD_X = 6;
static constexpr int CONTENT_GAP_X = 4;
static constexpr int COL_W = (DISP_W - (2 * CONTENT_PAD_X) - ((COL_COUNT - 1) * CONTENT_GAP_X)) / COL_COUNT;
static constexpr int TITLE_H = 34;
static constexpr int VALUE_W = 84;
static constexpr int VALUE_H = 70;
static constexpr int VALUE_Y = 32;

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_header_accept_bg = nullptr;
static lv_timer_t *g_header_flash_timer = nullptr;
static lv_timer_t *g_pending_action_timer = nullptr;
static UiDisplayAction g_pending_action = UiDisplayAction::NONE;
static UiDisplayAction g_deferred_action = UiDisplayAction::NONE;
static lv_obj_t *g_value_widgets[COL_COUNT] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *g_toggle_labels[2] = {nullptr, nullptr};
static lv_obj_t *g_toggle_widgets[2] = {nullptr, nullptr};
static lv_obj_t *g_arc_widgets[2] = {nullptr, nullptr};
static lv_obj_t *g_arc_labels[2] = {nullptr, nullptr};

static UiDisplayState g_state = {false, 50, true, 78, UiDisplayField::MIN_BRIGHTNESS};

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
    g_deferred_action = UiDisplayAction::NONE;
}

static void queue_action_after_flash(UiDisplayAction action) {
    g_deferred_action = action;
    if (g_pending_action_timer) lv_timer_del(g_pending_action_timer);
    g_pending_action_timer = lv_timer_create(pending_action_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_pending_action_timer, 1);
}

static uint8_t selected_index() {
    return (uint8_t)g_state.selected_field;
}

static uint8_t minimum_percent() {
    return g_state.min_brightness_off ? 0 : 1;
}

static uint8_t raw_to_percent(uint8_t raw_value) {
    if (raw_value == 0) return 0;
    if (raw_value == 1) return 1;

    const int scaled = (int)((raw_value * 100U + 127U) / 255U);
    if (scaled < 1) return 1;
    if (scaled > 100) return 100;
    return (uint8_t)scaled;
}

static uint8_t percent_to_raw(uint8_t percent_value) {
    if (percent_value == 0) return 0;
    if (percent_value == 1) return 1;

    const int scaled = (int)((percent_value * 255U + 50U) / 100U);
    if (scaled < 1) return 1;
    if (scaled > 255) return 255;
    return (uint8_t)scaled;
}

static void clamp_state() {
    const uint8_t minimum = minimum_percent();
    if (g_state.manual_brightness_percent < minimum) {
        g_state.manual_brightness_percent = minimum;
    }
    if (g_state.boost_brightness_percent < minimum) {
        g_state.boost_brightness_percent = minimum;
    }
    if (g_state.manual_brightness_percent > 100) {
        g_state.manual_brightness_percent = 100;
    }
    if (g_state.boost_brightness_percent > 100) {
        g_state.boost_brightness_percent = 100;
    }
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

static void update_focus() {
    for (uint8_t i = 0; i < COL_COUNT; ++i) {
        if (!g_value_widgets[i]) continue;
        if (i == selected_index()) lv_obj_add_state(g_value_widgets[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_value_widgets[i], LV_STATE_FOCUSED);
    }
}

static void update_widgets() {
    clamp_state();

    if (g_toggle_labels[0]) {
        lv_label_set_text(g_toggle_labels[0], g_state.min_brightness_off ? "OFF" : "LOW");
    }

    if (g_toggle_labels[1]) {
        lv_label_set_text(g_toggle_labels[1], g_state.auto_brightness ? "ON" : "OFF");
        lv_obj_set_style_text_color(g_toggle_labels[1], lv_color_white(), 0);
    }

    if (g_toggle_widgets[1]) {
        lv_obj_set_style_bg_color(g_toggle_widgets[1],
                                  g_state.auto_brightness ? lv_color_make(0x00, 0x9A, 0x3A)
                                                          : lv_color_make(0xB0, 0x20, 0x20),
                                  LV_PART_MAIN);
    }

    char buf[8];

    if (g_arc_widgets[0] && g_arc_labels[0]) {
        lv_arc_set_value(g_arc_widgets[0], g_state.manual_brightness_percent);
        std::snprintf(buf, sizeof(buf), "%u", g_state.manual_brightness_percent);
        lv_label_set_text(g_arc_labels[0], buf);
    }

    if (g_arc_widgets[1] && g_arc_labels[1]) {
        lv_arc_set_value(g_arc_widgets[1], g_state.boost_brightness_percent);
        std::snprintf(buf, sizeof(buf), "%u", g_state.boost_brightness_percent);
        lv_label_set_text(g_arc_labels[1], buf);
    }

    update_focus();
}

static void save_state_to_storage() {
    AppSettings &settings = storage_manager_get();
    settings.min_brightness_off = g_state.min_brightness_off;
    settings.manual_brightness = percent_to_raw(g_state.manual_brightness_percent);
    settings.auto_brightness = g_state.auto_brightness;
    settings.boost_brightness = percent_to_raw(g_state.boost_brightness_percent);
    storage_manager_save_display();
}

static void adjust_selected_field(int32_t delta) {
    if (delta == 0) return;

    const uint8_t magnitude = (uint8_t)((delta > 0) ? delta : -delta);
    const uint8_t minimum = minimum_percent();

    switch (g_state.selected_field) {
        case UiDisplayField::MIN_BRIGHTNESS:
            g_state.min_brightness_off = delta < 0;
            break;

        case UiDisplayField::MANUAL_BRIGHTNESS:
            if (delta > 0) {
                uint16_t next = (uint16_t)g_state.manual_brightness_percent + magnitude;
                g_state.manual_brightness_percent = (next > 100) ? 100 : (uint8_t)next;
            } else {
                int16_t next = (int16_t)g_state.manual_brightness_percent - magnitude;
                g_state.manual_brightness_percent = (next < minimum) ? minimum : (uint8_t)next;
            }
            break;

        case UiDisplayField::AUTO_BRIGHTNESS:
            g_state.auto_brightness = delta > 0;
            break;

        case UiDisplayField::DISPLAY_BOOST:
            if (delta > 0) {
                uint16_t next = (uint16_t)g_state.boost_brightness_percent + magnitude;
                g_state.boost_brightness_percent = (next > 100) ? 100 : (uint8_t)next;
            } else {
                int16_t next = (int16_t)g_state.boost_brightness_percent - magnitude;
                g_state.boost_brightness_percent = (next < minimum) ? minimum : (uint8_t)next;
            }
            break;
    }

    clamp_state();
}

static lv_obj_t *create_column(lv_obj_t *parent,
                               int x,
                               const char *title,
                               uint8_t idx,
                               bool use_arc) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, COL_W, CONTENT_H);
    lv_obj_set_pos(col, x, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_scrollable(col, false);

    lv_obj_t *title_label = lv_label_create(col);
    lv_label_set_text(title_label, title);
    lv_obj_set_size(title_label, COL_W, TITLE_H);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_make(0xD0, 0xD0, 0xD0), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_clickable(title_label, false);
    lv_obj_set_click_focusable(title_label, false);

    lv_obj_t *widget = create_value_shell(col, 0);
    g_value_widgets[idx] = widget;

    if (use_arc) {
        const uint8_t arc_idx = (idx == (uint8_t)UiDisplayField::MANUAL_BRIGHTNESS) ? 0 : 1;

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
        g_arc_widgets[arc_idx] = arc;

        lv_obj_t *value_label = lv_label_create(widget);
        lv_label_set_text(value_label, "0");
        lv_obj_set_style_text_font(value_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
        lv_obj_center(value_label);
        lv_obj_set_clickable(value_label, false);
        lv_obj_set_click_focusable(value_label, false);
        g_arc_labels[arc_idx] = value_label;
    } else {
        const uint8_t toggle_idx = (idx == (uint8_t)UiDisplayField::MIN_BRIGHTNESS) ? 0 : 1;

        lv_obj_t *value_label = lv_label_create(widget);
        lv_label_set_text(value_label, "OFF");
        lv_obj_set_style_text_font(value_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
        lv_obj_center(value_label);
        lv_obj_set_clickable(value_label, false);
        lv_obj_set_click_focusable(value_label, false);
        g_toggle_labels[toggle_idx] = value_label;
        g_toggle_widgets[toggle_idx] = widget;
    }

    return col;
}

} // namespace

void ui_display_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    apply_header_base(g_screen, "Display");

    lv_obj_t *content = lv_obj_create(g_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    create_column(content, CONTENT_PAD_X + (0 * (COL_W + CONTENT_GAP_X)), "Minimum\nBrightness", (uint8_t)UiDisplayField::MIN_BRIGHTNESS, false);
    create_column(content, CONTENT_PAD_X + (1 * (COL_W + CONTENT_GAP_X)), "Manual\nBrightness", (uint8_t)UiDisplayField::MANUAL_BRIGHTNESS, true);
    create_column(content, CONTENT_PAD_X + (2 * (COL_W + CONTENT_GAP_X)), "Auto\nBrightness", (uint8_t)UiDisplayField::AUTO_BRIGHTNESS, false);
    create_column(content, CONTENT_PAD_X + (3 * (COL_W + CONTENT_GAP_X)), "Display\nBoost", (uint8_t)UiDisplayField::DISPLAY_BOOST, true);

    update_widgets();
}

lv_obj_t *ui_display_get_screen() {
    return g_screen;
}

void ui_display_on_enter() {
    const AppSettings &settings = storage_manager_get();
    g_state.min_brightness_off = settings.min_brightness_off;
    g_state.manual_brightness_percent = raw_to_percent(settings.manual_brightness);
    g_state.auto_brightness = settings.auto_brightness;
    g_state.boost_brightness_percent = raw_to_percent(settings.boost_brightness);
    g_state.selected_field = UiDisplayField::MIN_BRIGHTNESS;
    update_widgets();
}

UiDisplayAction ui_display_handle_inputs(int32_t enc1_delta,
                                         int32_t enc2_delta,
                                         bool enc1_pressed,
                                         bool enc2_pressed) {
    if (g_pending_action != UiDisplayAction::NONE) {
        UiDisplayAction out = g_pending_action;
        g_pending_action = UiDisplayAction::NONE;
        return out;
    }

    if (g_pending_action_timer) {
        return UiDisplayAction::NONE;
    }

    if (enc1_pressed) {
        trigger_header_flash(false);
        queue_action_after_flash(UiDisplayAction::CANCEL);
        return UiDisplayAction::NONE;
    }

    if (enc1_delta != 0) {
        const int8_t direction = (enc1_delta > 0) ? 1 : -1;
        int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
        while (steps-- > 0) {
            int16_t next = (int16_t)selected_index() + direction;
            if (next < 0) next = COL_COUNT - 1;
            if (next >= COL_COUNT) next = 0;
            g_state.selected_field = (UiDisplayField)next;
        }
    }

    if (enc2_delta != 0) {
        adjust_selected_field(enc2_delta);
    }

    update_widgets();

    if (enc2_pressed) {
        trigger_header_flash(true);
        save_state_to_storage();
        queue_action_after_flash(UiDisplayAction::ACCEPT);
        return UiDisplayAction::NONE;
    }

    return UiDisplayAction::NONE;
}