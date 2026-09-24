#include "ui_home_page_menu.h"

#include "manager_storage.h"
#include "ui_main_screen.h"

#include <lvgl.h>
#include <stdio.h>

namespace {

static constexpr int DISP_W = 428;
static constexpr int DISP_H = 142;
static constexpr int HEADER_H = 34;
static constexpr int CONTENT_Y = 36;
static constexpr int CONTENT_H = DISP_H - CONTENT_Y;
static constexpr int ROW_H = 20;
static constexpr int POS_STEP = 2; // px per encoder detent
static constexpr int POS_LIMIT_X = 800; // allow moving elements off the visible display
static constexpr int POS_LIMIT_Y = 400;

static constexpr uint8_t BACKGROUND_INDEX = UI_HOME_ELEMENT_COUNT_MAIN;
static constexpr uint8_t RESET_ALL_INDEX = BACKGROUND_INDEX + 1;
static constexpr uint8_t ROW_COUNT = RESET_ALL_INDEX + 1;

// List display order for the 11 movable elements (storage/element index values).
// "Weekday" (element 10) is shown above "Day/Month" (element 3) in the list,
// while NVS storage indices stay unchanged to avoid reshuffling saved configs.
static const uint8_t ELEMENT_ROW_ORDER[UI_HOME_ELEMENT_COUNT_MAIN] = {0, 1, 2, 10, 3, 4, 5, 6, 7, 8, 9};

static const uint8_t RESET_FONT_SIZE[UI_HOME_ELEMENT_COUNT_MAIN] = {48, 16, 16, 24, 32, 16, 16, 16, 16, 16, 24};
static const int16_t RESET_X[UI_HOME_ELEMENT_COUNT_MAIN]        = {169, 272, 276, 222, 110, 152, 38, 395, 0, 0, 98};
static const int16_t RESET_Y[UI_HOME_ELEMENT_COUNT_MAIN]        = {44, 50, 72, 115, 0, 28, 0, 18, 18, 0, 100};

static const uint8_t FONT_SIZES[] = {16, 24, 32, 48, 64, 80};
static constexpr uint8_t FONT_SIZES_COUNT = sizeof(FONT_SIZES) / sizeof(FONT_SIZES[0]);

// 12-step rainbow (hue 0..330, full sat/val) + 10 grey levels (10%-100%), RGB565.
static const uint16_t COLOR_PALETTE[] = {
    0x0000, // black
    0xF800, // red
    0xFC00, // orange
    0xFFE0, // yellow
    0x87E0, // chartreuse
    0x07E0, // green
    0x07F0, // spring green
    0x07FF, // cyan
    0x043F, // azure
    0x001F, // blue
    0x841F, // violet
    0xF81F, // magenta
    0xF810, // rose
    0x1082, // grey ~10%
    0x2104, // grey ~20%
    0x3186, // grey ~30%
    0x4208, // grey ~40%
    0x528A, // grey ~50%
    0x630C, // grey ~60%
    0x738E, // grey ~70%
    0x8410, // grey ~80%
    0x9492, // grey ~90%
    0xFFFF, // grey 100% (white)
};
static constexpr uint8_t COLOR_PALETTE_COUNT = sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]);

enum class HpState {
    LIST_SCROLL = 0,
    FONT_COLOR,
    POSITION_XY,
    BACKGROUND_COLOR,
};

static lv_obj_t *g_list_screen = nullptr;
static lv_obj_t *g_rows[ROW_COUNT] = {nullptr};
static lv_obj_t *g_row_labels[ROW_COUNT] = {nullptr};
static lv_obj_t *g_row_swatches[ROW_COUNT] = {nullptr};

static uint8_t g_selected_index = 0;
static HpState g_state = HpState::LIST_SCROLL;
static int8_t g_font_idx = 0;
static int8_t g_color_idx = 0;

static UiElementConfig g_backup;
static uint16_t g_background_backup = 0x0000;

static lv_timer_t *g_flash_timer = nullptr;
static bool g_flash_on = true;

static int16_t clamp_i16(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int16_t)v;
}

static lv_color_t unpack_rgb565(uint16_t v) {
    uint8_t r = (uint8_t)((v >> 8) & 0xF8); r |= (uint8_t)(r >> 5);
    uint8_t g = (uint8_t)((v >> 3) & 0xFC); g |= (uint8_t)(g >> 6);
    uint8_t b = (uint8_t)((v << 3) & 0xF8); b |= (uint8_t)(b >> 5);
    return lv_color_make(r, g, b);
}

static uint8_t find_font_index(uint8_t size) {
    for (uint8_t i = 0; i < FONT_SIZES_COUNT; ++i) {
        if (FONT_SIZES[i] == size) return i;
    }
    return 0;
}

static uint8_t find_closest_color_index(uint16_t color_rgb565) {
    for (uint8_t i = 0; i < COLOR_PALETTE_COUNT; ++i) {
        if (COLOR_PALETTE[i] == color_rgb565) return i;
    }
    return 0;
}

// Maps a list row position to its underlying element/storage index (identity
// for BACKGROUND_INDEX/RESET_ALL_INDEX, which aren't reordered).
static uint8_t element_index_for_row(uint8_t row) {
    if (row < UI_HOME_ELEMENT_COUNT_MAIN) return ELEMENT_ROW_ORDER[row];
    return row;
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

}

static void update_row_text(uint8_t i) {
    if (!g_row_labels[i]) return;

    if (i == RESET_ALL_INDEX) {
        lv_label_set_text(g_row_labels[i], "Reset All");
        if (g_row_swatches[i]) lv_obj_set_hidden(g_row_swatches[i], true);
        return;
    }

    if (i == BACKGROUND_INDEX) {
        lv_label_set_text(g_row_labels[i], "Background");
        if (g_row_swatches[i]) {
            lv_obj_set_hidden(g_row_swatches[i], false);
            lv_obj_set_style_bg_color(g_row_swatches[i],
                                      unpack_rgb565(ui_main_screen_get_background_color()), 0);
        }
        return;
    }

    uint8_t element_index = element_index_for_row(i);
    int16_t x = 0, y = 0;
    ui_main_screen_get_element_pos(element_index, &x, &y);
    uint8_t fs = ui_main_screen_get_element_font_size(element_index);
    bool visible = ui_main_screen_get_element_visible(element_index);
    char buf[48];
    snprintf(buf, sizeof(buf), "%-10s F:%2u X:%3d Y:%3d %s",
             ui_main_screen_get_element_name(element_index), (unsigned)fs, (int)x, (int)y,
             visible ? "ON" : "OFF");
    lv_label_set_text(g_row_labels[i], buf);

    if (g_row_swatches[i]) {
        lv_obj_set_hidden(g_row_swatches[i], false);
        lv_obj_set_style_bg_color(g_row_swatches[i], unpack_rgb565(ui_main_screen_get_element_color(element_index)), 0);
    }
}

static void update_row_focus() {
    for (uint8_t i = 0; i < ROW_COUNT; ++i) {
        if (!g_rows[i]) continue;
        if (i == g_selected_index) {
            lv_obj_add_state(g_rows[i], LV_STATE_FOCUSED);
            lv_obj_scroll_to_view(g_rows[i], LV_ANIM_OFF);
        } else {
            lv_obj_clear_state(g_rows[i], LV_STATE_FOCUSED);
        }
    }
}

static void flash_timer_cb(lv_timer_t *timer) {
    (void)timer;
    lv_obj_t *el = ui_main_screen_get_element(element_index_for_row(g_selected_index));
    if (el) {
        g_flash_on = !g_flash_on;
        lv_obj_set_style_opa(el, g_flash_on ? LV_OPA_COVER : LV_OPA_50, 0);
    }
}

static void start_flash() {
    g_flash_on = true;
    if (g_flash_timer) lv_timer_del(g_flash_timer);
    g_flash_timer = lv_timer_create(flash_timer_cb, 200, nullptr);
}

static void stop_flash() {
    if (g_flash_timer) {
        lv_timer_del(g_flash_timer);
        g_flash_timer = nullptr;
    }
    lv_obj_t *el = ui_main_screen_get_element(element_index_for_row(g_selected_index));
    if (el) lv_obj_set_style_opa(el, LV_OPA_COVER, 0);
}

static void clamp_element_to_bounds(uint8_t index) {
    lv_obj_t *el = ui_main_screen_get_element(index);
    if (!el) return;
    int16_t x = 0, y = 0;
    ui_main_screen_get_element_pos(index, &x, &y);
    int32_t w = lv_obj_get_width(el);
    int32_t h = lv_obj_get_height(el);
    int16_t nx = clamp_i16(x, 0, POS_LIMIT_X - w);
    int16_t ny = clamp_i16(y, 0, POS_LIMIT_Y - h);
    if (nx != x || ny != y) ui_main_screen_set_element_pos(index, nx, ny);
}

// Restores one live home-screen element from a raw (possibly sentinel) config.
static void apply_element_config(uint8_t index, const UiElementConfig &e) {
    if (e.font_size != 0 && e.x >= 0 && e.y >= 0) {
        ui_main_screen_set_element_font_size(index, e.font_size);
        ui_main_screen_set_element_pos(index, e.x, e.y);
    } else {
        ui_main_screen_set_element_font_size(index, ui_main_screen_get_element_default_font_size(index));
        int16_t dx = 0, dy = 0;
        ui_main_screen_get_element_default_pos(index, &dx, &dy);
        ui_main_screen_set_element_pos(index, dx, dy);
    }
    ui_main_screen_set_element_visible(index, e.visible != 0);
    ui_main_screen_set_element_color(index, e.color_rgb565);
}

static void reset_all_elements() {
    AppSettings &s = storage_manager_get();
    for (uint8_t i = 0; i < UI_HOME_ELEMENT_COUNT_MAIN; ++i) {
        UiElementConfig &e = s.home_elements[i];
        e.font_size = RESET_FONT_SIZE[i];
        e.x = RESET_X[i];
        e.y = RESET_Y[i];
        e.visible = 1;
        e.color_rgb565 = 0xFFFF;
        apply_element_config(i, e);
        storage_manager_save_home_element(i);
    }
    ui_main_screen_set_background_color(0x0000);
    s.home_background_color_rgb565 = 0x0000;
    storage_manager_save_home_background();
    for (uint8_t row = 0; row < ROW_COUNT; ++row) update_row_text(row);
}

static void enter_edit_mode() {
    const AppSettings &s = storage_manager_get();
    const uint8_t element_index = element_index_for_row(g_selected_index);
    g_backup = s.home_elements[element_index];

    g_font_idx = find_font_index(ui_main_screen_get_element_font_size(element_index));
    g_color_idx = find_closest_color_index(ui_main_screen_get_element_color(element_index));
    g_state = HpState::POSITION_XY;
    lv_screen_load(ui_main_screen_get_screen());
    start_flash();
}

static void enter_background_edit_mode() {
    g_background_backup = ui_main_screen_get_background_color();
    g_color_idx = find_closest_color_index(g_background_backup);
    g_state = HpState::BACKGROUND_COLOR;
    lv_screen_load(ui_main_screen_get_screen());
}

static void exit_background_edit(bool save) {
    if (save) {
        AppSettings &s = storage_manager_get();
        s.home_background_color_rgb565 = ui_main_screen_get_background_color();
        storage_manager_save_home_background();
    } else {
        ui_main_screen_set_background_color(g_background_backup);
    }
    g_state = HpState::LIST_SCROLL;
    lv_screen_load(g_list_screen);
    update_row_text(BACKGROUND_INDEX);
    update_row_focus();
}

static void exit_edit_cancel() {
    apply_element_config(element_index_for_row(g_selected_index), g_backup);
    stop_flash();
    g_state = HpState::LIST_SCROLL;
    lv_screen_load(g_list_screen);
    update_row_text(g_selected_index);
    update_row_focus();
}

static void exit_edit_commit() {
    AppSettings &s = storage_manager_get();
    const uint8_t element_index = element_index_for_row(g_selected_index);
    UiElementConfig &e = s.home_elements[element_index];
    e.font_size = ui_main_screen_get_element_font_size(element_index);
    ui_main_screen_get_element_pos(element_index, &e.x, &e.y);
    e.visible = ui_main_screen_get_element_visible(element_index) ? 1 : 0;
    e.color_rgb565 = ui_main_screen_get_element_color(element_index);
    storage_manager_save_home_element(element_index);

    stop_flash();
    g_state = HpState::LIST_SCROLL;
    lv_screen_load(g_list_screen);
    update_row_text(g_selected_index);
    update_row_focus();
}

} // namespace

void ui_home_page_menu_init() {
    if (g_list_screen) return;

    g_list_screen = lv_obj_create(nullptr);
    lv_obj_set_size(g_list_screen, DISP_W, DISP_H);
    lv_obj_set_style_pad_all(g_list_screen, 0, 0);
    lv_obj_set_style_border_width(g_list_screen, 0, 0);
    lv_obj_set_scrollable(g_list_screen, false);

    apply_header_base(g_list_screen, "Home Page");

    lv_obj_t *content = lv_obj_create(g_list_screen);
    lv_obj_set_size(content, DISP_W, CONTENT_H);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 2, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);

    for (uint8_t i = 0; i < ROW_COUNT; ++i) {
        lv_obj_t *row = lv_obj_create(content);
        lv_obj_set_size(row, DISP_W - 8, ROW_H - 2);
        lv_obj_set_pos(row, 0, i * ROW_H);
        lv_obj_set_style_bg_color(row, lv_color_make(0x22, 0x22, 0x22), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row, 3, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        lv_obj_set_scrollable(row, false);

        lv_obj_set_style_border_width(row, 2, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(row, lv_color_white(), LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_t *label = lv_label_create(row);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_clickable(label, false);
        lv_obj_set_click_focusable(label, false);

        lv_obj_t *swatch = lv_obj_create(row);
        lv_obj_set_size(swatch, 10, 10);
        lv_obj_align(swatch, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_radius(swatch, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(swatch, 1, 0);
        lv_obj_set_style_border_color(swatch, lv_color_make(0x80, 0x80, 0x80), 0);
        lv_obj_set_scrollable(swatch, false);
        lv_obj_set_clickable(swatch, false);
        lv_obj_set_click_focusable(swatch, false);

        g_rows[i] = row;
        g_row_labels[i] = label;
        g_row_swatches[i] = swatch;
        update_row_text(i);
    }

    g_selected_index = 0;
    update_row_focus();
}

lv_obj_t *ui_home_page_menu_get_screen() {
    return g_list_screen;
}

void ui_home_page_menu_on_enter() {
    g_state = HpState::LIST_SCROLL;
    for (uint8_t i = 0; i < ROW_COUNT; ++i) update_row_text(i);
    update_row_focus();
}

bool ui_home_page_menu_is_editing() {
    return g_state != HpState::LIST_SCROLL;
}

UiHomePageAction ui_home_page_menu_handle_inputs(int32_t enc1_delta,
                                                 int32_t enc2_delta,
                                                 bool enc1_pressed,
                                                 bool enc2_pressed) {
    switch (g_state) {
        case HpState::LIST_SCROLL: {
            if (enc1_pressed) {
                return UiHomePageAction::CANCEL;
            }
            if (enc2_pressed) {
                if (g_selected_index == RESET_ALL_INDEX) {
                    reset_all_elements();
                } else if (g_selected_index == BACKGROUND_INDEX) {
                    enter_background_edit_mode();
                } else {
                    enter_edit_mode();
                }
                return UiHomePageAction::NONE;
            }
            if (enc1_delta != 0) {
                const int8_t direction = (enc1_delta > 0) ? 1 : -1;
                int16_t next = (int16_t)g_selected_index + direction;
                g_selected_index = (uint8_t)clamp_i16(next, 0, ROW_COUNT - 1);
                update_row_focus();
            }
            if (enc2_delta != 0 && g_selected_index < UI_HOME_ELEMENT_COUNT_MAIN) {
                const uint8_t element_index = element_index_for_row(g_selected_index);
                const bool visible = enc2_delta > 0;
                ui_main_screen_set_element_visible(element_index, visible);
                storage_manager_get().home_elements[element_index].visible = visible ? 1 : 0;
                storage_manager_save_home_element(element_index);
                update_row_text(g_selected_index);
            }
            return UiHomePageAction::NONE;
        }

        case HpState::FONT_COLOR: {
            if (enc1_pressed) {
                g_state = HpState::POSITION_XY;
                return UiHomePageAction::NONE;
            }
            if (enc2_pressed) {
                exit_edit_commit();
                return UiHomePageAction::NONE;
            }
            if (enc1_delta != 0) {
                const int8_t direction = (enc1_delta > 0) ? 1 : -1;
                int16_t next = (int16_t)g_color_idx + direction;
                if (next < 0) next = COLOR_PALETTE_COUNT - 1;
                if (next >= COLOR_PALETTE_COUNT) next = 0;
                g_color_idx = (int8_t)next;
                ui_main_screen_set_element_color(element_index_for_row(g_selected_index), COLOR_PALETTE[g_color_idx]);
            }
            if (enc2_delta != 0) {
                const int8_t direction = (enc2_delta > 0) ? 1 : -1;
                int16_t next = (int16_t)g_font_idx + direction;
                if (next < 0) next = FONT_SIZES_COUNT - 1;
                if (next >= FONT_SIZES_COUNT) next = 0;
                g_font_idx = (int8_t)next;
                ui_main_screen_set_element_font_size(element_index_for_row(g_selected_index), FONT_SIZES[g_font_idx]);
                clamp_element_to_bounds(element_index_for_row(g_selected_index));
            }
            return UiHomePageAction::NONE;
        }

        case HpState::POSITION_XY: {
            if (enc1_pressed) {
                exit_edit_cancel();
                return UiHomePageAction::NONE;
            }
            if (enc2_pressed) {
                g_state = HpState::FONT_COLOR;
                return UiHomePageAction::NONE;
            }
            if (enc1_delta != 0) {
                const uint8_t element_index = element_index_for_row(g_selected_index);
                int16_t x = 0, y = 0;
                ui_main_screen_get_element_pos(element_index, &x, &y);
                lv_obj_t *el = ui_main_screen_get_element(element_index);
                int32_t h = el ? lv_obj_get_height(el) : 0;
                int16_t ny = clamp_i16((int32_t)y + enc1_delta * POS_STEP, 0, POS_LIMIT_Y - h);
                ui_main_screen_set_element_pos(element_index, x, ny);
            }
            if (enc2_delta != 0) {
                const uint8_t element_index = element_index_for_row(g_selected_index);
                int16_t x = 0, y = 0;
                ui_main_screen_get_element_pos(element_index, &x, &y);
                lv_obj_t *el = ui_main_screen_get_element(element_index);
                int32_t w = el ? lv_obj_get_width(el) : 0;
                int16_t nx = clamp_i16((int32_t)x + enc2_delta * POS_STEP, 0, POS_LIMIT_X - w);
                ui_main_screen_set_element_pos(element_index, nx, y);
            }
            return UiHomePageAction::NONE;
        }

        case HpState::BACKGROUND_COLOR: {
            if (enc1_pressed) {
                exit_background_edit(false);
                return UiHomePageAction::NONE;
            }
            if (enc2_pressed) {
                exit_background_edit(true);
                return UiHomePageAction::NONE;
            }
            const int32_t delta = enc1_delta != 0 ? enc1_delta : enc2_delta;
            if (delta != 0) {
                int16_t next = (int16_t)g_color_idx + (delta > 0 ? 1 : -1);
                if (next < 0) next = COLOR_PALETTE_COUNT - 1;
                if (next >= COLOR_PALETTE_COUNT) next = 0;
                g_color_idx = (int8_t)next;
                ui_main_screen_set_background_color(COLOR_PALETTE[g_color_idx]);
            }
            return UiHomePageAction::NONE;
        }
    }

    return UiHomePageAction::NONE;
}

