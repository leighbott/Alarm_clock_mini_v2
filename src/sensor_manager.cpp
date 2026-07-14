#include "sensor_manager.h"
#include "pins_config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <math.h>

static Adafruit_AHTX0  aht;
static Adafruit_BMP280 bmp(&Wire1);  // Wire instance passed via constructor
static SensorData      data = { NAN, NAN, NAN, false, false };

void sensor_manager_init() {
    // Wire1 already started by rtc_manager_init() — do not call begin() again

    // AHT20 — I2C address 0x38
    if (aht.begin(&Wire1)) {
        data.aht20_ok = true;
        Serial.println("Sensor: AHT20 OK");
    } else {
        Serial.println("Sensor: AHT20 not found");
    }

    // BMP280 — I2C address 0x76
    if (bmp.begin(0x76)) {
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,   // temperature
                        Adafruit_BMP280::SAMPLING_X16,  // pressure
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        data.bmp280_ok = true;
        Serial.println("Sensor: BMP280 OK");
    } else {
        Serial.println("Sensor: BMP280 not found");
    }
}

void sensor_manager_update() {
    // AHT20
    if (data.aht20_ok) {
        sensors_event_t hum_ev, temp_ev;
        if (aht.getEvent(&hum_ev, &temp_ev)) {
            data.temperature = temp_ev.temperature;
            data.humidity    = hum_ev.relative_humidity;
        } else {
            data.temperature = NAN;
            data.humidity    = NAN;
        }
    }

    // BMP280 — use its temperature only as fallback if AHT20 absent
    if (data.bmp280_ok) {
        data.pressure = bmp.readPressure() / 100.0f; // Pa → hPa
        if (!data.aht20_ok) {
            data.temperature = bmp.readTemperature();
        }
    }
}

const SensorData& sensor_manager_get() {
    return data;
}
