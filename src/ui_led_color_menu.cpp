#include "ui_led_color_menu.h"

#include "manager_led.h"

#include <cstdio>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;

// Field shell layout mirrors the value-shell pattern used by ui_display_menu.cpp.
static constexpr int FIELD_TITLE_H = 20;
static constexpr int FIELD_W = 84;
static constexpr int FIELD_H = 70;
static constexpr int FIELD_Y = FIELD_TITLE_H + 4;
static constexpr int COL_W = 150;
static constexpr int COL_GAP = 20;
static constexpr int COLS_START_X = (DISP_W - (2 * COL_W + COL_GAP)) / 2;

static constexpr int HUE_CIRCLE_SIZE = 58;
static constexpr int SAT_ARC_SIZE = 58;

enum class Field : uint8_t { HUE = 0, SAT = 1 };

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_header_accept_bg = nullptr;
static lv_obj_t *g_title_label = nullptr;
static lv_timer_t *g_header_flash_timer = nullptr;
static lv_timer_t *g_pending_action_timer = nullptr;
static UiLedColorAction g_pending_action = UiLedColorAction::NONE;
static UiLedColorAction g_deferred_action = UiLedColorAction::NONE;

static lv_obj_t *g_hue_shell = nullptr;
static lv_obj_t *g_hue_circle = nullptr;
static lv_obj_t *g_sat_shell = nullptr;
static lv_obj_t *g_sat_arc = nullptr;
static lv_obj_t *g_sat_value_label = nullptr;

static LedStrip g_strip = LED_STRIP_FRONT;
static Field g_field = Field::HUE;
static uint16_t g_hue = 0;     // 0-359, live edit value
static uint8_t g_sat = 100;    // 0-100, live edit value

// Pre-edit snapshot for CANCEL restore
static bool g_prev_on = false;
static uint8_t g_prev_brightness = 0;
static uint16_t g_prev_hue = 0;
static uint8_t g_prev_sat = 100;

static bool strip_is_on() {
    return g_strip == LED_STRIP_FRONT ? led_manager_is_front_on() : led_manager_is_back_on();
}

static uint8_t strip_brightness() {
    return g_strip == LED_STRIP_FRONT ? led_manager_get_front() : led_manager_get_back();
}

static void strip_set_hue_sat(uint16_t hue, uint8_t sat) {
    if (g_strip == LED_STRIP_FRONT) led_manager_set_hue_sat_front(hue, sat);
    else led_manager_set_hue_sat_back(hue, sat);
}

static void strip_set_brightness(uint8_t brightness) {
    if (g_strip == LED_STRIP_FRONT) led_manager_set_front(brightness);
    else led_manager_set_back(brightness);
}

static void strip_set_on(bool on) {
    const bool now_on = strip_is_on();
    if (now_on == on) return;
    if (g_strip == LED_STRIP_FRONT) led_manager_toggle_front();
    else led_manager_toggle_back();
}

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
    g_deferred_action = UiLedColorAction::NONE;
}

static void queue_action_after_flash(UiLedColorAction action) {
    g_deferred_action = action;
    if (g_pending_action_timer) lv_timer_del(g_pending_action_timer);
    g_pending_action_timer = lv_timer_create(pending_action_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_pending_action_timer, 1);
}

static void apply_header_base(lv_obj_t *screen) {
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
    lv_label_set_text(lbl_title, "LED Color");
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
    g_title_label = lbl_title;
}

static void update_hue_circle() {
    if (g_hue_circle) lv_obj_set_style_bg_color(g_hue_circle, lv_color_hsv_to_rgb(g_hue, g_sat, 100), 0);
}

static void update_sat_arc() {
    lv_arc_set_value(g_sat_arc, g_sat);
    if (g_sat_value_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)g_sat);
        lv_label_set_text(g_sat_value_label, buf);
    }
}

static void update_field_focus() {
    if (g_hue_shell) {
        if (g_field == Field::HUE) lv_obj_add_state(g_hue_shell, LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_hue_shell, LV_STATE_FOCUSED);
    }
    if (g_sat_shell) {
        if (g_field == Field::SAT) lv_obj_add_state(g_sat_shell, LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_sat_shell, LV_STATE_FOCUSED);
    }
}

static void apply_live_preview() {
    strip_set_hue_sat(g_hue, g_sat);
    update_hue_circle();
}

// Value-shell box: same style as the field boxes used across the other menus.
static lv_obj_t *create_field_shell(lv_obj_t *parent, int x) {
    lv_obj_t *widget = lv_obj_create(parent);
    lv_obj_set_size(widget, FIELD_W, FIELD_H);
    lv_obj_set_pos(widget, x, FIELD_Y);
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

static lv_obj_t *create_field_title(lv_obj_t *parent, int col_x, const char *text) {
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, text);
    lv_obj_set_size(title, COL_W, FIELD_TITLE_H);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_make(0xD0, 0xD0, 0xD0), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, col_x, 0);
    lv_obj_set_clickable(title, false);
    lv_obj_set_click_focusable(title, false);
    return title;
}

} // namespace

void ui_led_color_menu_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    apply_header_base(g_screen);

    lv_obj_t *content = lv_obj_create(g_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    const int hue_col_x = COLS_START_X;
    const int sat_col_x = COLS_START_X + COL_W + COL_GAP;

    create_field_title(content, hue_col_x, "Hue");
    g_hue_shell = create_field_shell(content, hue_col_x + ((COL_W - FIELD_W) / 2));

    g_hue_circle = lv_obj_create(g_hue_shell);
    lv_obj_set_size(g_hue_circle, HUE_CIRCLE_SIZE, HUE_CIRCLE_SIZE);
    lv_obj_set_style_radius(g_hue_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_hue_circle, 1, 0);
    lv_obj_set_style_border_color(g_hue_circle, lv_color_white(), 0);
    lv_obj_set_style_pad_all(g_hue_circle, 0, 0);
    lv_obj_set_scrollable(g_hue_circle, false);
    lv_obj_set_clickable(g_hue_circle, false);
    lv_obj_center(g_hue_circle);

    create_field_title(content, sat_col_x, "Sat");
    g_sat_shell = create_field_shell(content, sat_col_x + ((COL_W - FIELD_W) / 2));

    g_sat_arc = lv_arc_create(g_sat_shell);
    lv_obj_set_size(g_sat_arc, SAT_ARC_SIZE, SAT_ARC_SIZE);
    lv_obj_center(g_sat_arc);
    lv_arc_set_rotation(g_sat_arc, 270);
    lv_arc_set_bg_angles(g_sat_arc, 0, 360);
    lv_arc_set_range(g_sat_arc, 0, 100);
    lv_arc_set_mode(g_sat_arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(g_sat_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_sat_arc, lv_color_make(0x34, 0x34, 0x34), LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_sat_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_sat_arc, lv_color_make(0x4D, 0xB1, 0xFF), LV_PART_INDICATOR);
    lv_obj_remove_style(g_sat_arc, nullptr, LV_PART_KNOB);
    lv_obj_set_clickable(g_sat_arc, false);
    lv_obj_set_scrollable(g_sat_arc, false);

    g_sat_value_label = lv_label_create(g_sat_shell);
    lv_label_set_text(g_sat_value_label, "-");
    lv_obj_set_style_text_font(g_sat_value_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_sat_value_label, lv_color_white(), 0);
    lv_obj_set_width(g_sat_value_label, FIELD_W - 8);
    lv_obj_set_style_text_align(g_sat_value_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(g_sat_value_label);
}

lv_obj_t *ui_led_color_menu_get_screen() {
    return g_screen;
}

void ui_led_color_menu_on_enter(LedStrip strip) {
    hide_header_flash();

    g_strip = strip;
    g_field = Field::HUE;

    if (g_title_label) {
        lv_label_set_text(g_title_label, strip == LED_STRIP_FRONT ? "Front LED Color" : "Rear LED Color");
    }

    g_prev_on = strip_is_on();
    g_prev_brightness = strip_brightness();
    g_prev_hue = (strip == LED_STRIP_FRONT) ? led_manager_get_hue_front() : led_manager_get_hue_back();
    g_prev_sat = (strip == LED_STRIP_FRONT) ? led_manager_get_sat_front() : led_manager_get_sat_back();

    g_hue = g_prev_hue;
    g_sat = g_prev_sat;

    strip_set_hue_sat(g_hue, g_sat);
    strip_set_brightness(255);
    strip_set_on(true);

    update_hue_circle();
    update_sat_arc();
    update_field_focus();
}

UiLedColorAction ui_led_color_menu_handle_inputs(int32_t enc1_delta,
                                                 int32_t enc2_delta,
                                                 bool enc1_pressed,
                                                 bool enc2_pressed) {
    if (g_pending_action != UiLedColorAction::NONE) {
        UiLedColorAction out = g_pending_action;
        g_pending_action = UiLedColorAction::NONE;
        return out;
    }

    if (g_pending_action_timer) {
        return UiLedColorAction::NONE;
    }

    if (enc1_pressed) {
        // Restore pre-edit strip state.
        strip_set_hue_sat(g_prev_hue, g_prev_sat);
        strip_set_brightness(g_prev_brightness);
        strip_set_on(g_prev_on);

        trigger_header_flash(false);
        queue_action_after_flash(UiLedColorAction::CANCEL);
        return UiLedColorAction::NONE;
    }

    if (enc2_pressed) {
        // Keep the new hue/sat, restore pre-edit on/off + brightness.
        strip_set_hue_sat(g_hue, g_sat);
        strip_set_brightness(g_prev_brightness);
        strip_set_on(g_prev_on);

        trigger_header_flash(true);
        queue_action_after_flash(UiLedColorAction::ACCEPT);
        return UiLedColorAction::NONE;
    }

    if (enc1_delta != 0) {
        g_field = (g_field == Field::HUE) ? Field::SAT : Field::HUE;
        update_field_focus();
    }

    if (enc2_delta != 0) {
        if (g_field == Field::HUE) {
            int32_t next = (int32_t)g_hue + enc2_delta * 4;
            next %= 360;
            if (next < 0) next += 360;
            g_hue = (uint16_t)next;
            update_hue_circle();
        } else {
            int32_t next = (int32_t)g_sat + enc2_delta * 2;
            if (next < 0) next = 0;
            if (next > 100) next = 100;
            g_sat = (uint8_t)next;
            update_sat_arc();
        }
        apply_live_preview();
    }

    return UiLedColorAction::NONE;
}
