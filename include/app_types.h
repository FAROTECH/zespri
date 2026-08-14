#pragma once
#include <Arduino.h>

struct WaterMeterData {
    uint32_t pulseCount = 0;
    float liters = 0.0f;
    bool lineState = true;           // true = idle/high
    uint32_t lastPulseMs = 0;
};

enum class MoistureState : uint8_t {
    INVALID = 0,
    DRY     = 1,
    MOIST   = 2,
    WET     = 3
};

struct EnvData {
    bool sht30Present = false;

    float temperatureC = NAN;
    float humidityRH = NAN;

    bool moisturePresent = false;
    uint16_t moistureRaw = 0xFFFF;

    uint8_t moisturePct = 0xFF;
    MoistureState moistureState = MoistureState::INVALID;
};

struct GpsData {
    bool valid = false;
    bool fix = false;
    uint32_t satellites = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitudeMeters = 0.0;
    double hdop = 0.0;
    uint32_t ageMs = 0;
    char utc[21] = {0};              // HH:MM:SS
};

struct BatteryData {
    bool valid = false;
    uint16_t millivolts = 0;
    bool low = false;
    bool critical = false;
    bool solarPresent = false;
    bool charging = false;
};

struct DeviceSnapshot {
    uint32_t uptimeMs = 0;
    WaterMeterData water;
    EnvData env;
    GpsData gps;
    BatteryData battery;
};
