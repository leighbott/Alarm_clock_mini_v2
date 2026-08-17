#include "manager_storage.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

static Preferences prefs;
static AppSettings settings;

// NVS namespace — max 15 chars
static constexpr char NS[] = "alarm_clk";

// ── Helpers ───────────────────────────────────────────────────────────────────
static void load_alarm() {
    settings.alarm_enabled       = prefs.getBool  ("alm_en",       settings.alarm_enabled);
    settings.alarm_hour          = prefs.getUChar  ("alm_hr",       settings.alarm_hour);
    settings.alarm_minute        = prefs.getUChar  ("alm_min",      settings.alarm_minute);
    settings.alarm_volume        = prefs.getUChar  ("alm_vol",      settings.alarm_volume);
    settings.alarm_end_brightness= prefs.getUChar  ("alm_end_brt",  settings.alarm_end_brightness);
    settings.alarm_vol_ramp_min  = prefs.getUShort ("alm_vol_ramp", settings.alarm_vol_ramp_min);
    settings.alarm_sun_ramp_min  = prefs.getUShort ("alm_sun_ramp", settings.alarm_sun_ramp_min);
    settings.alarm_sun_lead_min  = prefs.getUShort ("alm_sun_lead", settings.alarm_sun_lead_min);
    settings.snooze_enabled      = prefs.getBool   ("snz_en",       settings.snooze_enabled);
    settings.snooze_duration_min = prefs.getUShort ("snz_dur",      settings.snooze_duration_min);
    settings.hold_dismiss_sec    = prefs.getUChar  ("hold_sec",     settings.hold_dismiss_sec);
    settings.repeat_mode         = prefs.getUChar  ("alm_rep",      settings.repeat_mode);

    String mp3 = prefs.getString("alm_mp3", settings.alarm_mp3);
    strncpy(settings.alarm_mp3, mp3.c_str(), sizeof(settings.alarm_mp3) - 1);
    settings.alarm_mp3[sizeof(settings.alarm_mp3) - 1] = '\0';
    if (settings.alarm_mp3[0] == '\0') {
        strncpy(settings.alarm_mp3, "/test.mp3", sizeof(settings.alarm_mp3) - 1);
        settings.alarm_mp3[sizeof(settings.alarm_mp3) - 1] = '\0';
    }
}

static void save_alarm() {
    prefs.putBool  ("alm_en",       settings.alarm_enabled);
    prefs.putUChar ("alm_hr",       settings.alarm_hour);
    prefs.putUChar ("alm_min",      settings.alarm_minute);
    prefs.putUChar ("alm_vol",      settings.alarm_volume);
    prefs.putUChar ("alm_end_brt",  settings.alarm_end_brightness);
    prefs.putUShort("alm_vol_ramp", settings.alarm_vol_ramp_min);
    prefs.putUShort("alm_sun_ramp", settings.alarm_sun_ramp_min);
    prefs.putUShort("alm_sun_lead", settings.alarm_sun_lead_min);
    prefs.putBool  ("snz_en",       settings.snooze_enabled);
    prefs.putUShort("snz_dur",      settings.snooze_duration_min);
    prefs.putUChar ("hold_sec",     settings.hold_dismiss_sec);
    prefs.putUChar ("alm_rep",      settings.repeat_mode);
    prefs.putString("alm_mp3",      settings.alarm_mp3);
}

void storage_manager_load_display() {
    settings.min_brightness_off = prefs.getBool ("min_br_off", settings.min_brightness_off);
    settings.manual_brightness  = prefs.getUChar("manual_br",  settings.manual_brightness);
    settings.auto_brightness    = prefs.getBool ("auto_br",    settings.auto_brightness);
    settings.boost_brightness   = prefs.getUChar("boost_br",   settings.boost_brightness);
    settings.ldr_max_raw        = prefs.getFloat("ldr_max_raw", settings.ldr_max_raw);
}

static void save_display() {
    prefs.putBool ("min_br_off", settings.min_brightness_off);
    prefs.putUChar("manual_br",  settings.manual_brightness);
    prefs.putBool ("auto_br",    settings.auto_brightness);
    prefs.putUChar("boost_br",   settings.boost_brightness);
    prefs.putFloat("ldr_max_raw", settings.ldr_max_raw);
}

static void load_leds() {
    settings.led_front_brightness = prefs.getUChar ("led_fr_br", settings.led_front_brightness);
    settings.led_back_brightness  = prefs.getUChar ("led_bk_br", settings.led_back_brightness);
    settings.led_front_enabled    = prefs.getBool  ("led_fr_en", settings.led_front_enabled);
    settings.led_back_enabled     = prefs.getBool  ("led_bk_en", settings.led_back_enabled);
    settings.led1_hue             = prefs.getUShort("led1_hue",  settings.led1_hue);
    settings.led1_sat             = prefs.getUChar ("led1_sat",  settings.led1_sat);
    settings.led2_hue             = prefs.getUShort("led2_hue",  settings.led2_hue);
    settings.led2_sat             = prefs.getUChar ("led2_sat",  settings.led2_sat);
}

static void save_leds() {
    prefs.putUChar ("led_fr_br", settings.led_front_brightness);
    prefs.putUChar ("led_bk_br", settings.led_back_brightness);
    prefs.putBool  ("led_fr_en", settings.led_front_enabled);
    prefs.putBool  ("led_bk_en", settings.led_back_enabled);
    prefs.putUShort("led1_hue",  settings.led1_hue);
    prefs.putUChar ("led1_sat",  settings.led1_sat);
    prefs.putUShort("led2_hue",  settings.led2_hue);
    prefs.putUChar ("led2_sat",  settings.led2_sat);
}

// ── Public API ────────────────────────────────────────────────────────────────
void storage_manager_init() {
    // Apply defaults by direct field assignment (avoids C++ constexpr char[] issues)
    settings.alarm_enabled        = false;
    settings.alarm_hour           = 7;
    settings.alarm_minute         = 0;
    strncpy(settings.alarm_mp3, "/test.mp3", sizeof(settings.alarm_mp3) - 1);
    settings.alarm_mp3[sizeof(settings.alarm_mp3) - 1] = '\0';
    settings.alarm_volume         = 80;
    settings.alarm_end_brightness = 200;
    settings.alarm_vol_ramp_min   = 5;
    settings.alarm_sun_ramp_min   = 20;
    settings.alarm_sun_lead_min   = 20;
    settings.snooze_enabled       = true;
    settings.snooze_duration_min  = 9;
    settings.hold_dismiss_sec     = 3;
    settings.repeat_mode          = 0x80;   // once
    settings.min_brightness_off   = false;
    settings.manual_brightness    = 128;
    settings.auto_brightness      = true;
    settings.boost_brightness     = 200;
    settings.ldr_max_raw          = 2500.0f;
    settings.led_front_brightness = 128;
    settings.led_back_brightness  = 128;
    settings.led_front_enabled    = false;
    settings.led_back_enabled     = false;
    settings.led1_hue             = 0;
    settings.led1_sat             = 100;
    settings.led2_hue             = 0;
    settings.led2_sat             = 100;

    prefs.begin(NS, false);              // read-write mode

    load_alarm();
    storage_manager_load_display();
    load_leds();

    Serial.println("Storage: settings loaded from NVS");
}

AppSettings& storage_manager_get() {
    return settings;
}

void storage_manager_save_all() {
    save_alarm();
    save_display();
    save_leds();
}

void storage_manager_save_alarm() {
    save_alarm();
}

void storage_manager_save_display() {
    save_display();
}

void storage_manager_save_leds() {
    save_leds();
}
