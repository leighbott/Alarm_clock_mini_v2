#include <Arduino.h>
#include "lgfx_user.hpp"

static LGFX display;

void setup() {
    Serial.begin(115200);
    delay(500);

    display.init();
    display.setRotation(1);         // landscape: 320 wide, 240 tall
    display.setBrightness(128);     // 50% backlight
    display.fillScreen(TFT_BLUE);   // solid colour test

    Serial.println("Display OK");
}

void loop() {
    // nothing yet
}
