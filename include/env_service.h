#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_seesaw.h>
#include "app_types.h"

class EnvService {
public:
    bool begin(TwoWire& wire);
    void update();
    EnvData getData() const;

private:
    bool readMoisture(uint16_t& value);

    TwoWire* _wire = nullptr;
    Adafruit_SHT31 _sht31 = Adafruit_SHT31();
    Adafruit_seesaw _soil;

    EnvData _data;
    bool readMoisture(uint16_t& value);
    bool isI2cDevicePresent(uint8_t address);
};
