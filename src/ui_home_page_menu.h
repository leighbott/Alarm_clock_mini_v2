#pragma once

#include <lvgl.h>

enum class UiHomePageAction {
    NONE = 0,
    CANCEL,
    ACCEPT,
};

void ui_home_page_menu_init();

// The list-selection screen (shown for SELECT state).
lv_obj_t *ui_home_page_menu_get_screen();

void ui_home_page_menu_on_enter();

// While editing, the actual Home screen is displayed with a live flash preview;
// this reports whether that's currently the case so the router knows which
// screen to keep loaded.
bool ui_home_page_menu_is_editing();

UiHomePageAction ui_home_page_menu_handle_inputs(int32_t enc1_delta,
                                                 int32_t enc2_delta,
                                                 bool enc1_pressed,
                                                 bool enc2_pressed);
