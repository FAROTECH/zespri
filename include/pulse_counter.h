#pragma once

#include <Arduino.h>
#include <Wire.h>

struct WaterMeterData {
    uint32_t pulseCount = 0U;
    uint32_t lastPulseMs = 0U;
    bool lineState = true;   // open collector con pull-up: idle HIGH
    float liters = 0.0f;
};

class PulseCounterPcf8574 {
public:
    bool begin(TwoWire& wire, uint8_t addr, uint8_t bitIndex);
    void update(uint32_t nowMs);
    WaterMeterData getData() const;

private:
    bool readPort(uint8_t& value);

    TwoWire* _wire = nullptr;
    uint8_t _addr = 0U;
    uint8_t _bitIndex = 0U;
    uint8_t _lastPort = 0xFFU;
    bool _initialized = false;
    WaterMeterData _data;
};