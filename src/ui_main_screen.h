#pragma once

// Initialise the main screen — call once after LVGL is ready.
void ui_main_screen_init();

// Call every second to refresh time, date, sensor readings and alarm info.
void ui_main_screen_update();
