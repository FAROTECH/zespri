#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "app_types.h"

class PulseCounterPcf8574 {
public:
    bool begin(TwoWire& wire, uint8_t addr, uint8_t bitIndex);
    void update(uint32_t nowMs);
    WaterMeterData getData() const;

private:
    bool readPort(uint8_t& value);

    TwoWire* _wire = nullptr;
    uint8_t _addr = 0;
    uint8_t _bitIndex = 0;
    uint8_t _lastPort = 0xFF;
    bool _initialized = false;
    WaterMeterData _data;
};
