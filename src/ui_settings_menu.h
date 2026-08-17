#pragma once

#include <lvgl.h>

enum class UiNavState {
    HOME = 0,
    SETTINGS_MAIN,
    SETTINGS_TIME_DATE,
    SETTINGS_ALARM,
    SETTINGS_DISPLAY,
    SETTINGS_OTHER,
    SETTINGS_LED_COLOR,
};

// Build settings screens and bind to the provided Home screen.
void settings_menu_init(lv_obj_t *home_screen);

// Current top-level UI state.
UiNavState settings_menu_get_state();

// True only when Home is active.
bool settings_menu_is_home();

// Router transitions.
void settings_menu_open_main();
void settings_menu_return_home();

// Process input events for non-Home states.
void settings_menu_handle_inputs(int32_t enc1_delta,
                                 int32_t enc2_delta,
                                 bool enc1_pressed,
                                 bool enc2_pressed);