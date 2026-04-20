#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <LoRaWAN_ESP32.h>
#include "lorawan_provisioning.h"

class LoraService {
public:
    bool begin(const LorawanProvisioning& cfg);
    bool join();
    bool sendUplink(const uint8_t* data, size_t len, uint8_t fport = 1, bool confirmed = false);

    bool isReady() const;
    bool isJoined() const;
    int getLastError() const;

private:
    bool persistAfterJoinOrRestore();
    bool persistAfterUplink();
    bool tryRestoreSession();

private:
    SPIClass _spi = SPIClass(VSPI);

    Module* _module = nullptr;
    SX1262* _radio = nullptr;
    LoRaWANNode* _node = nullptr;

    LorawanProvisioning _cfg{};

    bool _ready = false;
    bool _joined = false;
    int _lastError = RADIOLIB_ERR_NONE;
};