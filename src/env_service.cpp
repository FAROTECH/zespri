#include "env_service.h"
#include "board_config.h"

#define MOISTURE_I2C_ADDR 0x36

static constexpr uint16_t MOISTURE_DRY_CAL_RAW = 395U;
static constexpr uint16_t MOISTURE_WET_CAL_RAW = 1015U;

static constexpr uint8_t MOISTURE_DRY_THRESHOLD_PCT = 20U;
static constexpr uint8_t MOISTURE_WET_THRESHOLD_PCT = 60U;

static uint8_t computeMoisturePct(uint16_t raw) {
    if (raw == 0xFFFFU) {
        return 0xFFU;
    }

    if (MOISTURE_WET_CAL_RAW <= MOISTURE_DRY_CAL_RAW) {
        return 0xFFU;
    }

    if (raw <= MOISTURE_DRY_CAL_RAW) {
        return 0U;
    }

    if (raw >= MOISTURE_WET_CAL_RAW) {
        return 100U;
    }

    const uint32_t num = (uint32_t)(raw - MOISTURE_DRY_CAL_RAW) * 100U;
    const uint32_t den = (uint32_t)(MOISTURE_WET_CAL_RAW - MOISTURE_DRY_CAL_RAW);

    return (uint8_t)(num / den);
}

static MoistureState classifyMoistureState(uint8_t pct, bool present) {
    if (!present || pct == 0xFFU) {
        return MoistureState::INVALID;
    }

    if (pct < MOISTURE_DRY_THRESHOLD_PCT) {
        return MoistureState::DRY;
    }

    if (pct < MOISTURE_WET_THRESHOLD_PCT) {
        return MoistureState::MOIST;
    }

    return MoistureState::WET;
}

static void updateMoistureDerivedFields(EnvData& data) {
    if (!data.moisturePresent) {
        data.moisturePct = 0xFFU;
        data.moistureState = MoistureState::INVALID;
        return;
    }

    data.moisturePct = computeMoisturePct(data.moistureRaw);
    data.moistureState = classifyMoistureState(data.moisturePct, data.moisturePresent);
}

bool EnvService::begin(TwoWire& wire) {
    _wire = &wire;

    // SHT30
    _data.sht30Present = _sht31.begin(I2C_ADDR_SHT30);

    // Moisture sensor
    if (ENABLE_EXTERNAL_MOISTURE_STUB) {
        _data.moisturePresent = true;
        _data.moistureRaw = 32000;
        updateMoistureDerivedFields(_data);
    } else {
        _data.moisturePresent = _soil.begin(MOISTURE_I2C_ADDR);

        if (_data.moisturePresent) {
            uint16_t raw = 0;
            if (readMoisture(raw)) {
                _data.moistureRaw = raw;
            }
        }

        updateMoistureDerivedFields(_data);
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
        updateMoistureDerivedFields(_data);

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

    updateMoistureDerivedFields(_data);
}

EnvData EnvService::getData() const {
    return _data;
}