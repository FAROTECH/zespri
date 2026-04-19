#include "pulse_counter.h"
#include "board_config.h"

bool PulseCounterPcf8574::begin(TwoWire& wire, uint8_t addr, uint8_t bitIndex) {
    _wire = &wire;
    _addr = addr;
    _bitIndex = bitIndex;

    uint8_t portValue = 0xFF;
    if (!readPort(portValue)) {
        return false;
    }

    _lastPort = portValue;
    _data.lineState = ((_lastPort >> _bitIndex) & 0x01u) != 0;
    _initialized = true;
    return true;
}

bool PulseCounterPcf8574::readPort(uint8_t& value) {
    _wire->requestFrom((int)_addr, 1);
    if (_wire->available() < 1) {
        return false;
    }
    value = _wire->read();
    return true;
}

void PulseCounterPcf8574::update(uint32_t nowMs) {
    if (!_initialized) {
        return;
    }

    uint8_t portValue = 0xFF;
    if (!readPort(portValue)) {
        return;
    }

    bool prevState = ((_lastPort >> _bitIndex) & 0x01u) != 0;
    bool currState = ((portValue >> _bitIndex) & 0x01u) != 0;

    // open collector con pull-up: idle HIGH, impulso = LOW.
    // Conteggio sul fronte HIGH -> LOW.
    if (prevState && !currState) {
        _data.pulseCount++;
        _data.lastPulseMs = nowMs;
    }

    _data.lineState = currState;
    _data.liters = static_cast<float>(_data.pulseCount) / WATER_METER_PULSES_PER_LITER;
    _lastPort = portValue;
}

WaterMeterData PulseCounterPcf8574::getData() const {
    return _data;
}
