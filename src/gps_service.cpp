#include "gps_service.h"

bool GpsService::begin(HardwareSerial& serial, uint32_t baudrate, int rxPin, int txPin) {
    _serial = &serial;
    _serial->begin(baudrate, SERIAL_8N1, rxPin, txPin);

    _rxBytes = 0;
    _lastByteMs = 0;

    return true;
}

void GpsService::update() {
    if (!_serial) return;

    while (_serial->available() > 0) {
        char c = (char)_serial->read();
        _gps.encode(c);

        // DIAGNOSTICA
        _rxBytes++;
        _lastByteMs = millis();
    }

    // FIX
    _data.fix = _gps.location.isValid();

    // SAT
    if (_gps.satellites.isValid()) {
        _data.satellites = _gps.satellites.value();
    }

    // POSIZIONE
    if (_gps.location.isValid()) {
        _data.latitude = _gps.location.lat();
        _data.longitude = _gps.location.lng();
    }

    // ALT
    if (_gps.altitude.isValid()) {
        _data.altitudeMeters = _gps.altitude.meters();
    }

    // HDOP
    if (_gps.hdop.isValid()) {
        _data.hdop = _gps.hdop.hdop();
    }

    // UTC
    if (_gps.time.isValid() && _gps.date.isValid()) {
        snprintf(_data.utc, sizeof(_data.utc),
                 "%02d:%02d:%02d",
                 _gps.time.hour(),
                 _gps.time.minute(),
                 _gps.time.second());
    }

    // AGE
    _data.ageMs = _gps.location.age();
}

GpsData GpsService::getData() const {
    return _data;
}

// DIAGNOSTICA
uint32_t GpsService::getRxBytes() const {
    return _rxBytes;
}

uint32_t GpsService::getLastByteMs() const {
    return _lastByteMs;
}