#pragma once

#include <stdint.h>

void brightness_manager_init();
void brightness_manager_update();
void brightness_manager_note_home_input();

uint16_t brightness_manager_get_last_ldr_raw();
uint8_t brightness_manager_get_current_brightness();

// Temporarily forces the LCD backlight to a raw value, bypassing boost/auto/manual selection.
void brightness_manager_set_preview_override(uint8_t raw_brightness);
void brightness_manager_clear_preview_override();