#pragma once

struct SensorData {
    float    temperature;   // °C  — NAN if sensor failed
    float    humidity;      // %   — NAN if sensor failed
    float    pressure;      // hPa — NAN if sensor failed
    bool     aht20_ok;
    bool     bmp280_ok;
};

void              sensor_manager_init();

// Call once per second — reads both sensors
void              sensor_manager_update();

const SensorData& sensor_manager_get();
