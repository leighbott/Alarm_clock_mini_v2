#pragma once

#include <lvgl.h>
#include <stdint.h>

// Initialise the main screen — call once after LVGL is ready.
void ui_main_screen_init();

// Call every second to refresh time, date, sensor readings and alarm info.
void ui_main_screen_update();

// Returns the Home screen object.
lv_obj_t *ui_main_screen_get_screen();

// ── Home Page customization accessors ─────────────────────────────────────────
static constexpr uint8_t UI_HOME_ELEMENT_COUNT_MAIN = 11;

lv_obj_t   *ui_main_screen_get_element(uint8_t index);
const char *ui_main_screen_get_element_name(uint8_t index);
uint8_t     ui_main_screen_get_element_font_size(uint8_t index);
void        ui_main_screen_set_element_font_size(uint8_t index, uint8_t size);
void        ui_main_screen_get_element_pos(uint8_t index, int16_t *x, int16_t *y);
void        ui_main_screen_set_element_pos(uint8_t index, int16_t x, int16_t y);
bool        ui_main_screen_get_element_visible(uint8_t index);
void        ui_main_screen_set_element_visible(uint8_t index, bool visible);
uint16_t    ui_main_screen_get_element_color(uint8_t index); // resolved RGB565 (never sentinel)
void        ui_main_screen_set_element_color(uint8_t index, uint16_t color_rgb565); // 0xFFFF = default

// Built-in factory defaults, used by the Reset All action.
uint8_t     ui_main_screen_get_element_default_font_size(uint8_t index);
void        ui_main_screen_get_element_default_pos(uint8_t index, int16_t *x, int16_t *y);
uint16_t    ui_main_screen_get_element_default_color(uint8_t index);
void        ui_main_screen_reset_element(uint8_t index);
uint16_t    ui_main_screen_get_background_color();
void        ui_main_screen_set_background_color(uint16_t color_rgb565);

// Applies saved NVS customization (font/position/color/visibility) on top of the built-in layout.
void ui_main_screen_apply_customization();
