#include "ui_settings_menu.h"
#include "ui_alarm_menu.h"
#include "ui_display_menu.h"
#include "ui_other_menu.h"
#include "ui_time_date.h"

#include <lvgl.h>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;
static constexpr int GRID_PAD_X = 8;
static constexpr int GRID_PAD_Y = 4;
static constexpr int GRID_GAP_X = 6;
static constexpr int GRID_GAP_Y = 6;
static constexpr int TILE_W = (DISP_W - (2 * GRID_PAD_X) - GRID_GAP_X) / 2;
static constexpr int TILE_H = (CONTENT_H - (2 * GRID_PAD_Y) - GRID_GAP_Y) / 2;

struct ChildScreen {
    lv_obj_t *screen;
    UiNavState state;
    const char *title;
};

static UiNavState g_state = UiNavState::HOME;
static lv_obj_t *g_home_screen = nullptr;
static lv_obj_t *g_main_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_header_accept_bg = nullptr;
static lv_timer_t *g_header_flash_timer = nullptr;
static lv_timer_t *g_home_route_timer = nullptr;
static bool g_route_home_after_flash = false;
static lv_obj_t *g_other_cancel_bg = nullptr;
static lv_obj_t *g_other_accept_bg = nullptr;
static lv_timer_t *g_other_flash_timer = nullptr;
static lv_timer_t *g_other_route_timer = nullptr;
static bool g_other_route_pending = false;
static bool g_other_screen_ready = false;
static lv_obj_t *g_tiles[4] = {nullptr, nullptr, nullptr, nullptr};
static uint8_t g_selected_tile = 0;

static ChildScreen g_children[4] = {
    {nullptr, UiNavState::SETTINGS_TIME_DATE, "Time & Date"},
    {nullptr, UiNavState::SETTINGS_ALARM, "Alarm"},
    {nullptr, UiNavState::SETTINGS_OTHER, "Other"},
    {nullptr, UiNavState::SETTINGS_DISPLAY, "Display"},
};

static void route_to_home();
static void route_to_main_settings();

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

static void home_route_timer_cb(lv_timer_t *timer) {
    (void)timer;
    g_home_route_timer = nullptr;
    if (g_route_home_after_flash) {
        g_route_home_after_flash = false;
        route_to_home();
    }
}

static void queue_route_home_after_flash() {
    g_route_home_after_flash = true;
    if (g_home_route_timer) lv_timer_del(g_home_route_timer);
    g_home_route_timer = lv_timer_create(home_route_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_home_route_timer, 1);
}

static void hide_other_flash() {
    if (g_other_cancel_bg) lv_obj_set_style_bg_opa(g_other_cancel_bg, LV_OPA_TRANSP, 0);
    if (g_other_accept_bg) lv_obj_set_style_bg_opa(g_other_accept_bg, LV_OPA_TRANSP, 0);
}

static void other_flash_timer_cb(lv_timer_t *timer) {
    (void)timer;
    hide_other_flash();
    g_other_flash_timer = nullptr;
}

static void trigger_other_flash(bool accept) {
    hide_other_flash();
    if (accept) {
        if (g_other_accept_bg) lv_obj_set_style_bg_opa(g_other_accept_bg, LV_OPA_COVER, 0);
    } else {
        if (g_other_cancel_bg) lv_obj_set_style_bg_opa(g_other_cancel_bg, LV_OPA_COVER, 0);
    }

    if (g_other_flash_timer) lv_timer_del(g_other_flash_timer);
    g_other_flash_timer = lv_timer_create(other_flash_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_other_flash_timer, 1);
}

static void other_route_timer_cb(lv_timer_t *timer) {
    (void)timer;
    g_other_route_timer = nullptr;
    if (!g_other_route_pending) return;

    g_other_route_pending = false;
    route_to_main_settings();
}

static void queue_other_route_after_flash(bool accept) {
    g_other_route_pending = true;
    (void)accept;
    if (g_other_route_timer) lv_timer_del(g_other_route_timer);
    g_other_route_timer = lv_timer_create(other_route_timer_cb, 120, nullptr);
    lv_timer_set_repeat_count(g_other_route_timer, 1);
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
    lv_label_set_text(lbl_cancel, "Home");
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
    lv_label_set_text(lbl_accept, "Home");
    lv_obj_set_style_text_font(lbl_accept, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_accept, lv_color_white(), 0);
    lv_obj_center(lbl_accept);
    lv_obj_set_clickable(lbl_accept, false);
    lv_obj_set_click_focusable(lbl_accept, false);

    if (screen == g_main_screen) {
        g_header_cancel_bg = cancel_bg;
        g_header_accept_bg = accept_bg;
    }
}

static lv_obj_t *create_tile(lv_obj_t *parent,
                             int x,
                             int y,
                             const char *text,
                             lv_color_t bg_color) {
    lv_obj_t *tile = lv_obj_create(parent);
    lv_obj_set_size(tile, TILE_W, TILE_H);
    lv_obj_set_pos(tile, x, y);
    lv_obj_set_style_bg_color(tile, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(tile, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(tile, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tile, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(tile, false);

    lv_obj_set_style_border_width(tile, 3, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_border_color(tile, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(tile, 1, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(tile, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_outline_opa(tile, LV_OPA_60, LV_PART_MAIN | LV_STATE_FOCUSED);

    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    lv_obj_set_clickable(label, false);
    lv_obj_set_click_focusable(label, false);

    return tile;
}

static void update_tile_focus() {
    for (uint8_t i = 0; i < 4; ++i) {
        if (!g_tiles[i]) continue;
        if (i == g_selected_tile) lv_obj_add_state(g_tiles[i], LV_STATE_FOCUSED);
        else lv_obj_clear_state(g_tiles[i], LV_STATE_FOCUSED);
    }
}

static void build_main_settings_screen() {
    g_main_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_main_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_main_screen, 0, 0);
    lv_obj_set_style_border_width(g_main_screen, 0, 0);
    lv_obj_set_scrollable(g_main_screen, false);

    apply_header_base(g_main_screen, "Settings");

    lv_obj_t *content = lv_obj_create(g_main_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    const int x0 = GRID_PAD_X;
    const int x1 = GRID_PAD_X + TILE_W + GRID_GAP_X;
    const int y0 = GRID_PAD_Y;
    const int y1 = GRID_PAD_Y + TILE_H + GRID_GAP_Y;

    g_tiles[0] = create_tile(content, x0, y0, "Time & Date", lv_color_make(0x24, 0x9B, 0x3A));
    g_tiles[1] = create_tile(content, x1, y0, "Alarm", lv_color_make(0xB0, 0x22, 0x22));
    g_tiles[2] = create_tile(content, x1, y1, "Other", lv_color_make(0x66, 0x66, 0x66));
    g_tiles[3] = create_tile(content, x0, y1, "Display", lv_color_make(0x1C, 0x5D, 0xB5));

    g_selected_tile = 0;
    update_tile_focus();
}

static lv_obj_t *build_child_screen(const char *title,
                                    lv_obj_t **out_cancel_bg,
                                    lv_obj_t **out_accept_bg) {
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_set_size(screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_scrollable(screen, false);

    apply_header_base(screen, title);

    if (out_cancel_bg) *out_cancel_bg = nullptr;
    if (out_accept_bg) *out_accept_bg = nullptr;

    lv_obj_t *header = lv_obj_get_child(screen, 0);
    if (header) {
        if (out_cancel_bg) *out_cancel_bg = lv_obj_get_child(header, 0);
        if (out_accept_bg) *out_accept_bg = lv_obj_get_child(header, 2);
    }

    lv_obj_t *content = lv_obj_create(screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_scrollable(content, false);

    return screen;
}

static void route_to_home() {
    if (!g_home_screen) return;
    g_state = UiNavState::HOME;
    lv_screen_load(g_home_screen);
}

static void route_to_main_settings() {
    if (!g_main_screen) return;
    g_state = UiNavState::SETTINGS_MAIN;
    update_tile_focus();
    lv_screen_load(g_main_screen);
}

static void route_to_child(uint8_t idx) {
    if (idx >= 4) return;

    if (g_children[idx].state == UiNavState::SETTINGS_DISPLAY && !g_children[idx].screen) {
        ui_display_init();
        g_children[idx].screen = ui_display_get_screen();
    }

    if (g_children[idx].state == UiNavState::SETTINGS_OTHER && !g_children[idx].screen) {
        ui_other_init();
        g_children[idx].screen = ui_other_get_screen();
    }

    if (!g_children[idx].screen) return;

    if (g_children[idx].state == UiNavState::SETTINGS_TIME_DATE) {
        ui_time_date_on_enter();
    }

    if (g_children[idx].state == UiNavState::SETTINGS_ALARM) {
        ui_alarm_on_enter();
    }

    if (g_children[idx].state == UiNavState::SETTINGS_DISPLAY) {
        ui_display_on_enter();
    }

    if (g_children[idx].state == UiNavState::SETTINGS_OTHER) {
        ui_other_on_enter();
    }

    g_state = g_children[idx].state;
    lv_screen_load(g_children[idx].screen);
}

} // namespace

void settings_menu_init(lv_obj_t *home_screen) {
    g_home_screen = home_screen;
    g_state = UiNavState::HOME;

    build_main_settings_screen();
    ui_time_date_init();
    ui_alarm_init();
    ui_other_init();
    g_children[1].screen = ui_alarm_get_screen();
    g_children[0].screen = ui_time_date_get_screen();
    g_children[2].screen = ui_other_get_screen();
    for (uint8_t i = 1; i < 4; ++i) {
        if (g_children[i].state == UiNavState::SETTINGS_DISPLAY ||
            g_children[i].state == UiNavState::SETTINGS_ALARM) {
            continue;
        }
        if (g_children[i].state == UiNavState::SETTINGS_OTHER) {
            g_children[i].screen = build_child_screen(g_children[i].title, &g_other_cancel_bg, &g_other_accept_bg);
        } else {
            g_children[i].screen = build_child_screen(g_children[i].title, nullptr, nullptr);
        }
    }
}

UiNavState settings_menu_get_state() {
    return g_state;
}

bool settings_menu_is_home() {
    return g_state == UiNavState::HOME;
}

void settings_menu_open_main() {
    route_to_main_settings();
}

void settings_menu_return_home() {
    route_to_home();
}

void settings_menu_handle_inputs(int32_t enc1_delta,
                                 int32_t enc2_delta,
                                 bool enc1_pressed,
                                 bool enc2_pressed) {
    switch (g_state) {
        case UiNavState::SETTINGS_MAIN: {
            if (g_route_home_after_flash || g_home_route_timer) {
                return;
            }

            if (enc1_pressed) {
                trigger_header_flash(false);
                queue_route_home_after_flash();
                return;
            }

            if (enc2_pressed) {
                trigger_header_flash(true);
                queue_route_home_after_flash();
                return;
            }

            if (enc1_delta != 0) {
                const int8_t direction = (enc1_delta > 0) ? 1 : -1;
                int32_t steps = (enc1_delta > 0) ? enc1_delta : -enc1_delta;
                while (steps-- > 0) {
                    int16_t next = (int16_t)g_selected_tile + direction;
                    if (next < 0) next = 3;
                    if (next > 3) next = 0;
                    g_selected_tile = (uint8_t)next;
                }
                update_tile_focus();
            }

            if (enc2_delta != 0) {
                route_to_child(g_selected_tile);
            }
            return;
        }

        case UiNavState::SETTINGS_TIME_DATE: {
            const UiTimeDateAction action = ui_time_date_handle_inputs(
                enc1_delta,
                enc2_delta,
                enc1_pressed,
                enc2_pressed);
            if (action == UiTimeDateAction::CANCEL) {
                route_to_main_settings();
            } else if (action == UiTimeDateAction::ACCEPT) {
                route_to_main_settings();
            }
            return;
        }

        case UiNavState::SETTINGS_DISPLAY: {
            const UiDisplayAction action = ui_display_handle_inputs(
                enc1_delta,
                enc2_delta,
                enc1_pressed,
                enc2_pressed);
            if (action == UiDisplayAction::CANCEL) {
                route_to_main_settings();
            } else if (action == UiDisplayAction::ACCEPT) {
                route_to_main_settings();
            }
            return;
        }

        case UiNavState::SETTINGS_ALARM: {
            const UiAlarmAction action = ui_alarm_handle_inputs(
                enc1_delta,
                enc2_delta,
                enc1_pressed,
                enc2_pressed);
            if (action == UiAlarmAction::CANCEL) {
                route_to_main_settings();
            } else if (action == UiAlarmAction::ACCEPT) {
                route_to_main_settings();
            }
            return;
        }

        case UiNavState::SETTINGS_OTHER: {
            if (g_other_route_pending || g_other_route_timer) {
                return;
            }

            const UiOtherAction action = ui_other_handle_inputs(
                enc1_delta,
                enc2_delta,
                enc1_pressed,
                enc2_pressed);
            if (action == UiOtherAction::CANCEL) {
                route_to_main_settings();
            } else if (action == UiOtherAction::ACCEPT) {
                route_to_main_settings();
            }
            return;
        }

        case UiNavState::HOME:
        default:
            return;
    }
}