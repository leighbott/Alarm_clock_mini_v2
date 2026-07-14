#pragma once

#include <lvgl.h>

// ── Encoder IDs ───────────────────────────────────────────────────────────────
enum EncoderID { ENC1 = 0, ENC2 = 1 };

void input_manager_init();

// Called every loop — reads encoder deltas and button states, feeds LVGL
void input_manager_update();

// Returns raw encoder count (for direct LED brightness use on main screen)
int32_t input_manager_get_count(EncoderID id);

// Returns true the first time called after a button press (single-shot)
bool input_manager_button_pressed(EncoderID id);

// Returns true while the button is held
bool input_manager_button_held(EncoderID id);

// Returns the raw LVGL indev handle (used to assign to groups in menus)
lv_indev_t *input_manager_get_indev(EncoderID id);
