#include "ui_other_menu.h"

#include "manager_led.h"
#include "manager_storage.h"

#include <lvgl.h>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;
static constexpr int TILE_GAP = 10;
static constexpr int TILE_W = (DISP_W - (3 * TILE_GAP)) / 2;
static constexpr int TILE_H = CONTENT_H - (2 * TILE_GAP);
static constexpr int SWATCH_SIZE = 28;

static lv_obj_t *g_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_header_accept_bg = nullptr;
static lv_timer_t *g_header_flash_timer = nullptr;
static lv_timer_t *g_pending_action_timer = nullptr;
static UiOtherAction g_pending_action = UiOtherAction::NONE;
static UiOtherAction g_deferred_action = UiOtherAction::NONE;

static lv_obj_t *g_tiles[2] = {nullptr, nullptr};
static lv_obj_t *g_swatches[2] = {nullptr, nullptr};
static uint8_t g_selected_tile = 0;

// Baseline captured when the Other menu is entered, restored on CANCEL.
static bool g_base_front_on = false;
static uint8_t g_base_front_brightness = 0;
static uint16_t g_base_front_hue = 0;
static uint8_t g_base_front_sat = 100;
static bool g_base_back_on = false;
static uint8_t g_base_back_brightness = 0;
static uint16_t g_base_back_hue = 0;
static uint8_t g_base_back_sat = 100;

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
    g_deferred_action = UiOtherAction::NONE;
}

static void queue_action_after_flash(UiOtherAction action) {
    g_deferred_action = action;
    if (g_pending_action_timer) lv_timer_del(g_pending_action_timer);
    g_pending_action_timer = lv_timer_create(pending_action_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_pending_action_timer, 1);
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

static lv_obj_t *create_led_tile(lv_obj_t *parent, int x, int y, const char *text, lv_obj_t **out_swatch) {
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_bg_color(tile, lv_color_make(0x2A, 0x2A, 0x2A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tile, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(tile, false);

    lv_obj_set_style_border_width(tile, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(tile, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_clickable(label, false);
    lv_obj_set_click_focusable(label, false);

    lv_obj_t *swatch = lv_obj_create(tile);
    lv_obj_set_size(swatch, SWATCH_SIZE, SWATCH_SIZE);
    lv_obj_set_style_radius(swatch, SWATCH_SIZE / 2, 0);
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, lv_color_white(), 0);
    lv_obj_set_style_pad_all(swatch, 0, 0);
    lv_obj_set_scrollable(swatch, false);
    lv_obj_align(swatch, LV_ALIGN_BOTTOM_MID, 0, -10);
    if (out_swatch) *out_swatch = swatch;

    return tile;
}

static void update_tile_focus() {
    for (uint8_t i = 0; i < 2; ++i) {
        if (!g_tiles[i]) continue;
        if (i == g_selected_tile) lv_obj_add_state(g_tiles[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_tiles[i], LV_STATE_FOCUSED);
    }
}

} // namespace

void ui_other_init() {
    if (g_screen) return;

    g_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_screen, 0, 0);
    lv_obj_set_style_border_width(g_screen, 0, 0);
    lv_obj_set_scrollable(g_screen, false);

    apply_header_base(g_screen, "Other");

    lv_obj_t *content = lv_obj_create(g_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    const int x0 = TILE_GAP;
    const int x1 = TILE_GAP + TILE_W + TILE_GAP;
    const int y0 = TILE_GAP;

    g_tiles[0] = create_led_tile(content, x0, y0, "LED 1", &g_swatches[0]);
    g_tiles[1] = create_led_tile(content, x1, y0, "LED 2", &g_swatches[1]);

    g_selected_tile = 0;
    update_tile_focus();
    ui_other_refresh_previews();
}

lv_obj_t *ui_other_get_screen() {
    return g_screen;
}

void ui_other_refresh_previews() {
    if (g_swatches[0]) {
        lv_obj_set_style_bg_color(g_swatches[0],
                                  lv_color_hsv_to_rgb(led_manager_get_hue_front(), led_manager_get_sat_front(), 100), 0);
    }
    if (g_swatches[1]) {
        lv_obj_set_style_bg_color(g_swatches[1],
                                  lv_color_hsv_to_rgb(led_manager_get_hue_back(), led_manager_get_sat_back(), 100), 0);
    }
}

void ui_other_on_enter() {
    hide_header_flash();

    g_base_front_on = led_manager_is_front_on();
    g_base_front_brightness = led_manager_get_front();
    g_base_front_hue = led_manager_get_hue_front();
    g_base_front_sat = led_manager_get_sat_front();
    g_base_back_on = led_manager_is_back_on();
    g_base_back_brightness = led_manager_get_back();
    g_base_back_hue = led_manager_get_hue_back();
    g_base_back_sat = led_manager_get_sat_back();

    ui_other_refresh_previews();
}

UiOtherAction ui_other_handle_inputs(int32_t enc1_delta,
                                     int32_t enc2_delta,
                                     bool enc1_pressed,
                                     bool enc2_pressed) {
    if (g_pending_action != UiOtherAction::NONE) {
        UiOtherAction out = g_pending_action;
        g_pending_action = UiOtherAction::NONE;
        return out;
    }

    if (g_pending_action_timer) {
        return UiOtherAction::NONE;
    }

    if (enc1_pressed) {
        // Undo any LED edits made while inside this menu.
        led_manager_set_hue_sat_front(g_base_front_hue, g_base_front_sat);
        led_manager_set_front(g_base_front_brightness);
        if (led_manager_is_front_on() != g_base_front_on) led_manager_toggle_front();

        led_manager_set_hue_sat_back(g_base_back_hue, g_base_back_sat);
        led_manager_set_back(g_base_back_brightness);
        if (led_manager_is_back_on() != g_base_back_on) led_manager_toggle_back();

        trigger_header_flash(false);
        queue_action_after_flash(UiOtherAction::CANCEL);
        return UiOtherAction::NONE;
    }

    if (enc2_pressed) {
        AppSettings &s = storage_manager_get();
        s.led1_hue = led_manager_get_hue_front();
        s.led1_sat = led_manager_get_sat_front();
        s.led2_hue = led_manager_get_hue_back();
        s.led2_sat = led_manager_get_sat_back();
        storage_manager_save_leds();

        trigger_header_flash(true);
        queue_action_after_flash(UiOtherAction::ACCEPT);
        return UiOtherAction::NONE;
    }

    if (enc1_delta != 0) {
        g_selected_tile = (g_selected_tile == 0) ? 1 : 0;
        update_tile_focus();
    }

    if (enc2_delta != 0) {
        return (g_selected_tile == 0) ? UiOtherAction::ENTER_LED1 : UiOtherAction::ENTER_LED2;
    }

    return UiOtherAction::NONE;
}
