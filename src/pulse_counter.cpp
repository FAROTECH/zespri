#include "pulse_counter.h"
#include "board_config.h"

bool PulseCounterPcf8574::begin(TwoWire& wire, uint8_t addr, uint8_t bitIndex) {
    if (bitIndex > 7U) {
        return false;
    }

    _wire = &wire;
    _addr = addr;
    _bitIndex = bitIndex;

    uint8_t portValue = 0xFFU;
    if (!readPort(portValue)) {
        _initialized = false;
        return false;
    }

    _lastPort = portValue;

    _data.pulseCount = 0U;
    _data.lastPulseMs = 0U;
    _data.lineState = (((_lastPort >> _bitIndex) & 0x01U) != 0U);
    _data.liters = 0.0f;

    _initialized = true;
    return true;
}

bool PulseCounterPcf8574::readPort(uint8_t& value) {
    if (_wire == nullptr) {
        return false;
    }

    const int requested = _wire->requestFrom(static_cast<int>(_addr), 1);
    if (requested != 1) {
        return false;
    }

    if (_wire->available() < 1) {
        return false;
    }

    value = static_cast<uint8_t>(_wire->read());
    return true;
}

void PulseCounterPcf8574::update(uint32_t nowMs) {
    if (!_initialized) {
        return;
    }

    uint8_t portValue = 0xFFU;
    if (!readPort(portValue)) {
        return;
    }

    const bool prevState = (((_lastPort >> _bitIndex) & 0x01U) != 0U);
    const bool currState = (((portValue >> _bitIndex) & 0x01U) != 0U);

    // Open collector con pull-up:
    // idle HIGH, impulso attivo LOW.
    // Conteggio sul fronte di discesa HIGH -> LOW.
    if (prevState && !currState) {
        const uint32_t deltaMs = nowMs - _data.lastPulseMs;

        if ((_data.pulseCount == 0U) || (deltaMs >= WATER_METER_DEBOUNCE_MS)) {
            _data.pulseCount++;
            _data.lastPulseMs = nowMs;
        }
    }

    _data.lineState = currState;
    _data.liters = static_cast<float>(_data.pulseCount) / WATER_METER_PULSES_PER_LITER;
    _lastPort = portValue;
}

WaterMeterData PulseCounterPcf8574::getData() const {
    return _data;
}