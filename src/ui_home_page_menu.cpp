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

static const uint8_t FONT_SIZES[] = {14, 16, 20, 24, 32, 48};
static constexpr uint8_t FONT_SIZES_COUNT = sizeof(FONT_SIZES) / sizeof(FONT_SIZES[0]);

enum class HpState {
    SELECT = 0,
    EDIT_FONT_SIZE,
    EDIT_Y_POSITION,
};

static lv_obj_t *g_list_screen = nullptr;
static lv_obj_t *g_header_cancel_bg = nullptr;
static lv_obj_t *g_rows[UI_HOME_ELEMENT_COUNT_MAIN] = {nullptr};
static lv_obj_t *g_row_labels[UI_HOME_ELEMENT_COUNT_MAIN] = {nullptr};

static uint8_t g_selected_index = 0;
static HpState g_state = HpState::SELECT;
static int8_t g_font_idx = 1;

static uint8_t g_backup_font = 0;
static int16_t g_backup_x = 0;
static int16_t g_backup_y = 0;

static lv_timer_t *g_flash_timer = nullptr;
static bool g_flash_on = true;

static int16_t clamp_i16(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int16_t)v;
}

static uint8_t find_font_index(uint8_t size) {
    for (uint8_t i = 0; i < FONT_SIZES_COUNT; ++i) {
        if (FONT_SIZES[i] == size) return i;
    }
    return 1; // default 16
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

    g_header_cancel_bg = cancel_bg;
}

static void update_row_text(uint8_t i) {
    if (!g_row_labels[i]) return;
    int16_t x = 0, y = 0;
    ui_main_screen_get_element_pos(i, &x, &y);
    uint8_t fs = ui_main_screen_get_element_font_size(i);
    char buf[48];
    snprintf(buf, sizeof(buf), "%-10s F:%2u X:%3d Y:%3d",
             ui_main_screen_get_element_name(i), (unsigned)fs, (int)x, (int)y);
    lv_label_set_text(g_row_labels[i], buf);
}

static void update_row_focus() {
    for (uint8_t i = 0; i < UI_HOME_ELEMENT_COUNT_MAIN; ++i) {
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
    lv_obj_t *el = ui_main_screen_get_element(g_selected_index);
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
    lv_obj_t *el = ui_main_screen_get_element(g_selected_index);
    if (el) lv_obj_set_style_opa(el, LV_OPA_COVER, 0);
}

static void clamp_element_to_bounds(uint8_t index) {
    lv_obj_t *el = ui_main_screen_get_element(index);
    if (!el) return;
    int16_t x = 0, y = 0;
    ui_main_screen_get_element_pos(index, &x, &y);
    int32_t w = lv_obj_get_width(el);
    int32_t h = lv_obj_get_height(el);
    int16_t nx = clamp_i16(x, 0, DISP_W - w);
    int16_t ny = clamp_i16(y, 0, DISP_H - h);
    if (nx != x || ny != y) ui_main_screen_set_element_pos(index, nx, ny);
}

static void enter_edit_mode() {
    g_backup_font = ui_main_screen_get_element_font_size(g_selected_index);
    ui_main_screen_get_element_pos(g_selected_index, &g_backup_x, &g_backup_y);
    g_font_idx = find_font_index(g_backup_font);

    g_state = HpState::EDIT_FONT_SIZE;
    lv_screen_load(ui_main_screen_get_screen());
    start_flash();
}

static void exit_edit_cancel() {
    ui_main_screen_set_element_font_size(g_selected_index, g_backup_font);
    ui_main_screen_set_element_pos(g_selected_index, g_backup_x, g_backup_y);
    stop_flash();
    g_state = HpState::SELECT;
    lv_screen_load(g_list_screen);
    update_row_text(g_selected_index);
    update_row_focus();
}

static void exit_edit_commit() {
    AppSettings &s = storage_manager_get();
    UiElementConfig &e = s.home_elements[g_selected_index];
    e.font_size = ui_main_screen_get_element_font_size(g_selected_index);
    ui_main_screen_get_element_pos(g_selected_index, &e.x, &e.y);
    storage_manager_save_home_element(g_selected_index);

    stop_flash();
    g_state = HpState::SELECT;
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

    for (uint8_t i = 0; i < UI_HOME_ELEMENT_COUNT_MAIN; ++i) {
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

        g_rows[i] = row;
        g_row_labels[i] = label;
        update_row_text(i);
    }

    g_selected_index = 0;
    update_row_focus();
}

lv_obj_t *ui_home_page_menu_get_screen() {
    return g_list_screen;
}

void ui_home_page_menu_on_enter() {
    g_state = HpState::SELECT;
    for (uint8_t i = 0; i < UI_HOME_ELEMENT_COUNT_MAIN; ++i) update_row_text(i);
    update_row_focus();
}

bool ui_home_page_menu_is_editing() {
    return g_state != HpState::SELECT;
}

UiHomePageAction ui_home_page_menu_handle_inputs(int32_t enc1_delta,
                                                 int32_t enc2_delta,
                                                 bool enc1_pressed,
                                                 bool enc2_pressed) {
    switch (g_state) {
        case HpState::SELECT: {
            if (enc1_pressed) {
                return UiHomePageAction::CANCEL;
            }
            if (enc2_pressed) {
                enter_edit_mode();
                return UiHomePageAction::NONE;
            }
            if (enc2_delta != 0) {
                const int8_t direction = (enc2_delta > 0) ? 1 : -1;
                int16_t next = (int16_t)g_selected_index + direction;
                g_selected_index = (uint8_t)clamp_i16(next, 0, UI_HOME_ELEMENT_COUNT_MAIN - 1);
                update_row_focus();
            }
            return UiHomePageAction::NONE;
        }

        case HpState::EDIT_FONT_SIZE: {
            if (enc1_pressed) {
                exit_edit_cancel();
                return UiHomePageAction::NONE;
            }
            if (enc2_pressed) {
                g_state = HpState::EDIT_Y_POSITION;
                return UiHomePageAction::NONE;
            }
            if (enc1_delta != 0) {
                const int8_t direction = (enc1_delta > 0) ? 1 : -1;
                int16_t next = (int16_t)g_font_idx + direction;
                g_font_idx = (int8_t)clamp_i16(next, 0, FONT_SIZES_COUNT - 1);
                ui_main_screen_set_element_font_size(g_selected_index, FONT_SIZES[g_font_idx]);
                clamp_element_to_bounds(g_selected_index);
            }
            if (enc2_delta != 0) {
                int16_t x = 0, y = 0;
                ui_main_screen_get_element_pos(g_selected_index, &x, &y);
                lv_obj_t *el = ui_main_screen_get_element(g_selected_index);
                int32_t w = el ? lv_obj_get_width(el) : 0;
                int16_t nx = clamp_i16((int32_t)x + enc2_delta * POS_STEP, 0, DISP_W - w);
                ui_main_screen_set_element_pos(g_selected_index, nx, y);
            }
            return UiHomePageAction::NONE;
        }

        case HpState::EDIT_Y_POSITION: {
            if (enc1_pressed) {
                g_state = HpState::EDIT_FONT_SIZE;
                return UiHomePageAction::NONE;
            }
            if (enc2_pressed) {
                exit_edit_commit();
                return UiHomePageAction::NONE;
            }
            if (enc2_delta != 0) {
                int16_t x = 0, y = 0;
                ui_main_screen_get_element_pos(g_selected_index, &x, &y);
                lv_obj_t *el = ui_main_screen_get_element(g_selected_index);
                int32_t h = el ? lv_obj_get_height(el) : 0;
                int16_t ny = clamp_i16((int32_t)y + enc2_delta * POS_STEP, 0, DISP_H - h);
                ui_main_screen_set_element_pos(g_selected_index, x, ny);
            }
            return UiHomePageAction::NONE;
        }
    }

    return UiHomePageAction::NONE;
}
