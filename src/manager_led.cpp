#include "manager_led.h"
#include "pins_config.h"
#include <Arduino.h>
#include <math.h>

static constexpr uint32_t LED_FREQ       = 1000;  // Hz
static constexpr uint8_t  LED_RESOLUTION = 8;     // bits (0–255 duty)
static constexpr uint8_t  CH_FRONT       = 1;     // LEDC channel for front LED
static constexpr uint8_t  CH_BACK        = 2;     // LEDC channel for back LED

static uint8_t front_brightness = 0;
static uint8_t back_brightness  = 0;
static bool    front_on         = false;
static bool    back_on          = false;

static uint8_t gamma_correct(uint8_t brightness) {
    if (brightness == 0) return 0;

    const float normalized = (float)brightness / 255.0f;
    int corrected = (int)lroundf(powf(normalized, 2.2f) * 255.0f);
    if (corrected < 0) corrected = 0;
    if (corrected > 255) corrected = 255;
    return (uint8_t)corrected;
}

static void apply_front() {
    ledcWrite(CH_FRONT, front_on ? gamma_correct(front_brightness) : 0);
}

static void apply_back() {
    ledcWrite(CH_BACK, back_on ? gamma_correct(back_brightness) : 0);
}

void led_manager_init(uint8_t init_front_brightness, uint8_t init_back_brightness,
                      bool init_front_on, bool init_back_on) {
    front_brightness = init_front_brightness;
    back_brightness  = init_back_brightness;
    front_on         = init_front_on;
    back_on          = init_back_on;

    // Arduino ESP32 core v2.x API
    ledcSetup(CH_FRONT, LED_FREQ, LED_RESOLUTION);
    ledcSetup(CH_BACK,  LED_FREQ, LED_RESOLUTION);
    ledcAttachPin(PIN_LED_FRONT, CH_FRONT);
    ledcAttachPin(PIN_LED_BACK,  CH_BACK);

    apply_front();
    apply_back();

    Serial.println("LED: OK");
}

void led_manager_set_front(uint8_t brightness) {
    front_brightness = brightness;
    if (front_on) apply_front();
}

void led_manager_set_back(uint8_t brightness) {
    back_brightness = brightness;
    if (back_on) apply_back();
}

uint8_t led_manager_get_front() { return front_brightness; }
uint8_t led_manager_get_back()  { return back_brightness;  }

void led_manager_toggle_front() {
    front_on = !front_on;
    apply_front();
}

void led_manager_toggle_back() {
    back_on = !back_on;
    apply_back();
}

bool led_manager_is_front_on() { return front_on; }
bool led_manager_is_back_on()  { return back_on;  }
