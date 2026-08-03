#include "ui_settings_menu.h"
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
static lv_obj_t *g_tiles[4] = {nullptr, nullptr, nullptr, nullptr};
static uint8_t g_selected_tile = 0;

static ChildScreen g_children[4] = {
    {nullptr, UiNavState::SETTINGS_TIME_DATE, "Time & Date"},
    {nullptr, UiNavState::SETTINGS_ALARM, "Alarm"},
    {nullptr, UiNavState::SETTINGS_DISPLAY, "Display"},
    {nullptr, UiNavState::SETTINGS_OTHER, "Other"},
};

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
    g_tiles[2] = create_tile(content, x0, y1, "Display", lv_color_make(0x1C, 0x5D, 0xB5));
    g_tiles[3] = create_tile(content, x1, y1, "Other", lv_color_make(0x66, 0x66, 0x66));

    g_selected_tile = 0;
    update_tile_focus();
}

static lv_obj_t *build_child_screen(const char *title) {
    lv_obj_t *screen = lv_obj_create(nullptr);
    lv_obj_set_size(screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_scrollable(screen, false);

    apply_header_base(screen, title);

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
    if (idx >= 4 || !g_children[idx].screen) return;

    if (g_children[idx].state == UiNavState::SETTINGS_TIME_DATE) {
        ui_time_date_on_enter();
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
    g_children[0].screen = ui_time_date_get_screen();
    for (uint8_t i = 1; i < 4; ++i) {
        g_children[i].screen = build_child_screen(g_children[i].title);
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
            if (enc1_pressed) {
                route_to_home();
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

            (void)enc2_pressed; // explicit no-op in main settings
            return;
        }

        case UiNavState::SETTINGS_TIME_DATE: {
            const UiTimeDateAction action = ui_time_date_handle_inputs(
                enc1_delta,
                enc2_delta,
                enc1_pressed,
                enc2_pressed);
            if (action != UiTimeDateAction::NONE) {
                route_to_main_settings();
            }
            return;
        }

        case UiNavState::SETTINGS_ALARM:
        case UiNavState::SETTINGS_DISPLAY:
        case UiNavState::SETTINGS_OTHER:
            if (enc1_pressed || enc2_pressed) {
                route_to_main_settings();
            }
            (void)enc1_delta; // rotations intentionally ignored in child menus
            (void)enc2_delta;
            return;

        case UiNavState::HOME:
        default:
            return;
    }
}