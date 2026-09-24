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
static constexpr uint8_t UI_HOME_ELEMENT_COUNT_MAIN = 10;

lv_obj_t   *ui_main_screen_get_element(uint8_t index);
const char *ui_main_screen_get_element_name(uint8_t index);
uint8_t     ui_main_screen_get_element_font_size(uint8_t index);
void        ui_main_screen_set_element_font_size(uint8_t index, uint8_t size);
void        ui_main_screen_get_element_pos(uint8_t index, int16_t *x, int16_t *y);
void        ui_main_screen_set_element_pos(uint8_t index, int16_t x, int16_t y);

// Applies saved NVS customization (font/position) on top of the built-in layout.
void ui_main_screen_apply_customization();
