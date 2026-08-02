#pragma once

#include <stdint.h>

void    led_manager_init(uint8_t front_brightness, uint8_t back_brightness,
                         bool front_on, bool back_on);

// Brightness: 0–255
void    led_manager_set_front(uint8_t brightness);
void    led_manager_set_back(uint8_t brightness);

uint8_t led_manager_get_front();
uint8_t led_manager_get_back();

void    led_manager_toggle_front();
void    led_manager_toggle_back();

bool    led_manager_is_front_on();
bool    led_manager_is_back_on();
