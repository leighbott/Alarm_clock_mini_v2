#include "input_manager.h"
#include "pins_config.h"
#include <Arduino.h>
#include <ESP32Encoder.h>

// ── Encoder instances ─────────────────────────────────────────────────────────
static ESP32Encoder enc1;
static ESP32Encoder enc2;

// ── Button state tracking ─────────────────────────────────────────────────────
struct ButtonState {
    uint8_t  pin;
    bool     last_raw;      // last debounced level
    bool     pressed_flag;  // single-shot press flag (cleared on read)
    bool     held;          // currently held
    uint32_t press_time;    // millis() at last press edge
};

static ButtonState btn1 = { PIN_ENC1_BTN, true, false, false, 0 };
static ButtonState btn2 = { PIN_ENC2_BTN, true, false, false, 0 };

// ── LVGL indev data ───────────────────────────────────────────────────────────
// Encoder 2 drives the LVGL navigation group (menus).
// Encoder 1 is used directly (LED brightness on main screen) and also
// navigates when in menus, so we register both but enc2 is the primary.

static int32_t enc1_last = 0;
static int32_t enc2_last = 0;

static lv_indev_t *lv_indev_enc1 = nullptr;
static lv_indev_t *lv_indev_enc2 = nullptr;

// Per-encoder LVGL indev read callback data
struct LvEncData {
    ESP32Encoder *enc;
    int32_t      *last_count;
    ButtonState  *btn;
};

static LvEncData lv_enc1_data = { &enc1, &enc1_last, &btn1 };
static LvEncData lv_enc2_data = { &enc2, &enc2_last, &btn2 };

// ── Debounce helper ───────────────────────────────────────────────────────────
static constexpr uint32_t DEBOUNCE_MS = 20;

static void update_button(ButtonState &b) {
    bool raw = (digitalRead(b.pin) == LOW); // active LOW
    if (raw != b.last_raw) {
        // Wait for debounce
        static uint32_t pending_time[2] = {0, 0};
        uint8_t idx = (&b == &btn1) ? 0 : 1;
        if (pending_time[idx] == 0) {
            pending_time[idx] = millis();
        }
        if (millis() - pending_time[idx] >= DEBOUNCE_MS) {
            b.last_raw = raw;
            pending_time[idx] = 0;
            if (raw) { // press edge
                b.pressed_flag = true;
                b.press_time   = millis();
            }
        }
    } else {
        uint8_t idx = (&b == &btn1) ? 0 : 1;
        static uint32_t pending_time_clear[2] = {0, 0};
        pending_time_clear[idx] = 0; // reset pending if stable
        (void)pending_time_clear; // suppress unused warning
    }
    b.held = b.last_raw;
}

// ── LVGL encoder read callback ────────────────────────────────────────────────
static void lv_enc_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    LvEncData *ed = (LvEncData *)lv_indev_get_user_data(indev);

    int32_t current = (int32_t)ed->enc->getCount();
    data->enc_diff  = (int16_t)(current - *ed->last_count);
    *ed->last_count = current;

    data->state = ed->btn->held ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ── Public API ────────────────────────────────────────────────────────────────
void input_manager_init() {
    // Encoders — half-quad mode via PCNT
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    enc1.attachHalfQuad(PIN_ENC1_A, PIN_ENC1_B);
    enc2.attachHalfQuad(PIN_ENC2_A, PIN_ENC2_B);
    enc1.setCount(0);
    enc2.setCount(0);

    // Buttons — active LOW, internal pullup
    pinMode(PIN_ENC1_BTN, INPUT_PULLUP);
    pinMode(PIN_ENC2_BTN, INPUT_PULLUP);

    // LVGL indev — encoder type
    lv_indev_enc1 = lv_indev_create();
    lv_indev_set_type(lv_indev_enc1, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(lv_indev_enc1, lv_enc_read_cb);
    lv_indev_set_user_data(lv_indev_enc1, &lv_enc1_data);

    lv_indev_enc2 = lv_indev_create();
    lv_indev_set_type(lv_indev_enc2, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(lv_indev_enc2, lv_enc_read_cb);
    lv_indev_set_user_data(lv_indev_enc2, &lv_enc2_data);
}

void input_manager_update() {
    update_button(btn1);
    update_button(btn2);
}

int32_t input_manager_get_count(EncoderID id) {
    return (id == ENC1) ? (int32_t)enc1.getCount()
                        : (int32_t)enc2.getCount();
}

bool input_manager_button_pressed(EncoderID id) {
    ButtonState &b = (id == ENC1) ? btn1 : btn2;
    if (b.pressed_flag) {
        b.pressed_flag = false;
        return true;
    }
    return false;
}

bool input_manager_button_held(EncoderID id) {
    return (id == ENC1) ? btn1.held : btn2.held;
}

lv_indev_t *input_manager_get_indev(EncoderID id) {
    return (id == ENC1) ? lv_indev_enc1 : lv_indev_enc2;
}
