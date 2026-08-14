#include "env_service.h"
#include "board_config.h"

#define MOISTURE_I2C_ADDR 0x36

/*
 * Temporary calibration.
 *
 * These values MUST be replaced after field calibration
 * using the actual soil and installation conditions.
 */
static constexpr uint16_t MOISTURE_DRY_CAL_RAW = 395U;
static constexpr uint16_t MOISTURE_WET_CAL_RAW = 1015U;

static constexpr uint8_t MOISTURE_DRY_THRESHOLD_PCT = 20U;
static constexpr uint8_t MOISTURE_WET_THRESHOLD_PCT = 60U;


static uint8_t computeMoisturePct(uint16_t raw)
{
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

    const uint32_t num =
        static_cast<uint32_t>(raw - MOISTURE_DRY_CAL_RAW) * 100U;

    const uint32_t den =
        static_cast<uint32_t>(MOISTURE_WET_CAL_RAW - MOISTURE_DRY_CAL_RAW);

    return static_cast<uint8_t>(num / den);
}


static MoistureState classifyMoistureState(uint8_t pct, bool present)
{
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


static void updateMoistureDerivedFields(EnvData& data)
{
    if (!data.moisturePresent || data.moistureRaw == 0xFFFFU) {
        data.moisturePct = 0xFFU;
        data.moistureState = MoistureState::INVALID;
        return;
    }

    data.moisturePct = computeMoisturePct(data.moistureRaw);

    data.moistureState =
        classifyMoistureState(data.moisturePct, data.moisturePresent);
}


bool EnvService::begin(TwoWire& wire)
{
    _wire = &wire;

    // -------------------------------------------------------------------------
    // SHT temperature / humidity sensor @ 0x44
    // -------------------------------------------------------------------------

    _data.sht30Present = _sht31.begin(I2C_ADDR_SHT30);

    if (!_data.sht30Present) {
        _data.temperatureC = NAN;
        _data.humidityRH = NAN;
    }

    // -------------------------------------------------------------------------
    // Soil moisture sensor
    // -------------------------------------------------------------------------

    if (ENABLE_EXTERNAL_MOISTURE_STUB) {

        _data.moisturePresent = true;
        _data.moistureRaw = 32000U;

    } else {

        /*
        * Probe the expected I2C address before invoking the Seesaw library.
        *
        * Adafruit_seesaw::begin() retries internally and may repeatedly call
        * Wire.begin() when the device is absent, producing misleading warnings.
        */
        if (isI2cDevicePresent(MOISTURE_I2C_ADDR)) {

            _data.moisturePresent =
                _soil.begin(MOISTURE_I2C_ADDR);

            if (_data.moisturePresent) {

                uint16_t raw = 0xFFFFU;

                if (readMoisture(raw)) {
                    _data.moistureRaw = raw;
                } else {
                    _data.moistureRaw = 0xFFFFU;
                }
            }

        } else {

            _data.moisturePresent = false;
            _data.moistureRaw = 0xFFFFU;
        }
    }

    updateMoistureDerivedFields(_data);

    /*
     * Environment service is considered available if at least one
     * environmental sensor was successfully detected.
     */
    return _data.sht30Present || _data.moisturePresent;
}


bool EnvService::readMoisture(uint16_t& value)
{
    if (!_data.moisturePresent) {
        return false;
    }

    const uint16_t raw = _soil.touchRead(0);

    if (raw == 0xFFFFU) {
        return false;
    }

    value = raw;
    return true;
}


void EnvService::update()
{
    // -------------------------------------------------------------------------
    // SHT
    // -------------------------------------------------------------------------

    if (_data.sht30Present) {

        const float temperature = _sht31.readTemperature();
        const float humidity = _sht31.readHumidity();

        /*
         * Never retain stale measurements as if they were current.
         */
        _data.temperatureC =
            isnan(temperature) ? NAN : temperature;

        _data.humidityRH =
            isnan(humidity) ? NAN : humidity;
    }

    // -------------------------------------------------------------------------
    // Moisture
    // -------------------------------------------------------------------------

    if (ENABLE_EXTERNAL_MOISTURE_STUB) {

        static size_t step = 0;

        static const uint16_t demoValues[] = {
            28500,
            29200,
            30100,
            31500,
            32700,
            34000,
            33300,
            31800
        };

        _data.moisturePresent = true;
        _data.moistureRaw = demoValues[step];

        step =
            (step + 1U) %
            (sizeof(demoValues) / sizeof(demoValues[0]));

    } else if (_data.moisturePresent) {

        uint16_t raw = 0xFFFFU;

        if (readMoisture(raw)) {
            _data.moistureRaw = raw;
        } else {
            /*
             * Do not keep an old measurement after a failed acquisition.
             */
            _data.moistureRaw = 0xFFFFU;
        }
    }

    updateMoistureDerivedFields(_data);
}


EnvData EnvService::getData() const
{
    return _data;
}

bool EnvService::isI2cDevicePresent(uint8_t address)
{
    if (_wire == nullptr) {
        return false;
    }

    _wire->beginTransmission(address);
    return (_wire->endTransmission(true) == 0);
}
