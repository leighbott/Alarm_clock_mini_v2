#include "manager_led.h"
#include "pins_config.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

static Adafruit_NeoPixel strip_front(LED_STRIP_COUNT, PIN_LED_FRONT, NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel strip_back(LED_STRIP_COUNT, PIN_LED_BACK, NEO_GRB + NEO_KHZ800);

static uint8_t  front_brightness = 0;
static uint8_t  back_brightness  = 0;
static bool     front_on         = false;
static bool     back_on          = false;
static uint16_t front_hue        = 0;
static uint8_t  front_sat        = 100;
static uint16_t back_hue         = 0;
static uint8_t  back_sat         = 100;

static uint8_t gamma_correct(uint8_t brightness) {
    if (brightness == 0) return 0;

    const float normalized = (float)brightness / 255.0f;
    int corrected = (int)lroundf(powf(normalized, 2.2f) * 255.0f);
    if (corrected < 0) corrected = 0;
    if (corrected > 255) corrected = 255;
    return (uint8_t)corrected;
}

static uint32_t hsv_color(Adafruit_NeoPixel &strip, uint16_t hue_deg, uint8_t sat_pct, uint8_t value) {
    const uint16_t hue16 = (uint32_t)hue_deg * 65535UL / 360UL;
    const uint8_t  sat8  = (uint16_t)sat_pct * 255U / 100U;
    return strip.gamma32(strip.ColorHSV(hue16, sat8, gamma_correct(value)));
}

static void render(Adafruit_NeoPixel &strip, bool on, uint8_t brightness, uint16_t hue, uint8_t sat) {
    const uint32_t color = on ? hsv_color(strip, hue, sat, brightness) : 0;
    for (uint16_t i = 0; i < strip.numPixels(); ++i) {
        strip.setPixelColor(i, color);
    }
    strip.show();
}

static void apply_front() {
    render(strip_front, front_on, front_brightness, front_hue, front_sat);
}

static void apply_back() {
    render(strip_back, back_on, back_brightness, back_hue, back_sat);
}

void led_manager_init(uint8_t init_front_brightness, uint8_t init_back_brightness,
                      bool init_front_on, bool init_back_on,
                      uint16_t init_front_hue, uint8_t init_front_sat,
                      uint16_t init_back_hue, uint8_t init_back_sat) {
    front_brightness = init_front_brightness;
    back_brightness  = init_back_brightness;
    front_on         = init_front_on;
    back_on          = init_back_on;
    front_hue        = init_front_hue;
    front_sat        = init_front_sat;
    back_hue         = init_back_hue;
    back_sat         = init_back_sat;

    strip_front.begin();
    strip_back.begin();

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

void led_manager_set_hue_sat_front(uint16_t hue, uint8_t sat) {
    front_hue = hue;
    front_sat = sat;
    apply_front();
}

void led_manager_set_hue_sat_back(uint16_t hue, uint8_t sat) {
    back_hue = hue;
    back_sat = sat;
    apply_back();
}

uint16_t led_manager_get_hue_front() { return front_hue; }
uint8_t  led_manager_get_sat_front() { return front_sat; }
uint16_t led_manager_get_hue_back()  { return back_hue;  }
uint8_t  led_manager_get_sat_back()  { return back_sat;  }

void led_manager_set_hsv(LedStrip strip, uint16_t hue, uint8_t sat, uint8_t brightness) {
    if (strip == LED_STRIP_FRONT) {
        front_hue = hue;
        front_sat = sat;
        front_brightness = brightness;
        front_on = brightness > 0;
        apply_front();
    } else {
        back_hue = hue;
        back_sat = sat;
        back_brightness = brightness;
        back_on = brightness > 0;
        apply_back();
    }
}

