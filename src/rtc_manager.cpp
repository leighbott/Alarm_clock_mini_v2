#include "rtc_manager.h"
#include "pins_config.h"
#include <Arduino.h>
#include <Wire.h>

static RTC_DS3231 rtc;
static bool       rtc_ok = false;

void rtc_manager_init() {
    Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (!rtc.begin(&Wire1)) {
        Serial.println("RTC: DS3231 not found");
        rtc_ok = false;
        return;
    }

    if (rtc.lostPower()) {
        Serial.println("RTC: lost power — time is not set");
        // Set a sane default so the rest of the system doesn't crash
        rtc.adjust(DateTime(2025, 1, 1, 0, 0, 0));
    }

    rtc_ok = true;
    Serial.println("RTC: OK");
}

bool rtc_manager_is_ok() {
    return rtc_ok;
}

DateTime rtc_manager_get_time() {
    if (!rtc_ok) {
        return DateTime(2025, 1, 1, 0, 0, 0);
    }
    return rtc.now();
}

void rtc_manager_set_time(const DateTime &dt) {
    if (!rtc_ok) return;
    rtc.adjust(dt);
}
