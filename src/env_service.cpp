#include "env_service.h"
#include "board_config.h"

#define MOISTURE_I2C_ADDR 0x36

bool EnvService::begin(TwoWire& wire) {
    _wire = &wire;

    // SHT30
    _data.sht30Present = _sht31.begin(I2C_ADDR_SHT30);

    // Moisture sensor
    if (ENABLE_EXTERNAL_MOISTURE_STUB) {
        _data.moisturePresent = true;
        _data.moistureRaw = 32000;
    } else {
        _data.moisturePresent = _soil.begin(MOISTURE_I2C_ADDR);

        if (_data.moisturePresent) {
            uint16_t raw = 0;
            if (readMoisture(raw)) {
                _data.moistureRaw = raw;
            }
        }
    }

    return true;
}

bool EnvService::readMoisture(uint16_t& value) {
    if (!_data.moisturePresent) {
        return false;
    }

    uint16_t raw = _soil.touchRead(0);

    if (raw == 0xFFFF) {
        return false;
    }

    value = raw;
    return true;
}

void EnvService::update() {
    // SHT30
    if (_data.sht30Present) {
        const float t = _sht31.readTemperature();
        const float h = _sht31.readHumidity();

        if (!isnan(t)) {
            _data.temperatureC = t;
        }

        if (!isnan(h)) {
            _data.humidityRH = h;
        }
    }

    // Moisture
    if (ENABLE_EXTERNAL_MOISTURE_STUB) {
        static int step = 0;
        static const uint16_t demoValues[] = {
            28500, 29200, 30100, 31500, 32700, 34000, 33300, 31800
        };

        _data.moisturePresent = true;
        _data.moistureRaw = demoValues[step];

        step++;
        if (step >= (int)(sizeof(demoValues) / sizeof(demoValues[0]))) {
            step = 0;
        }
        return;
    }

    if (_data.moisturePresent) {
        uint16_t raw = 0;
        if (readMoisture(raw)) {
            _data.moistureRaw = raw;
        }
    }
}

EnvData EnvService::getData() const {
    return _data;
}