#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>
#include "app_types.h"

class GpsService {
public:
    bool begin(HardwareSerial& serial, uint32_t baudrate, int rxPin, int txPin);
    void update();
    GpsData getData() const;

    // diagnostica
    uint32_t getRxBytes() const;
    uint32_t getLastByteMs() const;

private:
    HardwareSerial* _serial = nullptr;
    TinyGPSPlus _gps;

    GpsData _data;

    uint32_t _rxBytes = 0;
    uint32_t _lastByteMs = 0;
};