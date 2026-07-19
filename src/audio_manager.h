#pragma once

#include <stdint.h>
#include <stdbool.h>

bool audio_manager_init();          // returns true if SD + audio init OK

void audio_manager_loop();          // must be called every loop() iteration

void audio_manager_play(const char *path);   // play an MP3 from SD
void audio_manager_play_beep();              // play /beeps/1000hz.wav (fallback)
void audio_manager_stop();

void    audio_manager_set_volume(uint8_t pct);  // 0–100 %
uint8_t audio_manager_get_volume();             // last set volume (0–100 %)

bool audio_manager_is_sd_ok();
bool audio_manager_is_playing();
