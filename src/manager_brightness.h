#pragma once

#include <stdint.h>

void brightness_manager_init();
void brightness_manager_update();
void brightness_manager_note_home_input();

uint16_t brightness_manager_get_last_ldr_raw();
uint8_t brightness_manager_get_current_brightness();