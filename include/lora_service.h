#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

class LoraService {
public:
    bool begin();
    bool send(const uint8_t* data, size_t len);

    bool isReady() const;
    int getLastError() const;

private:
    static constexpr int ERR_NOT_READY_OR_INVALID_ARGS = -10001;
    static constexpr uint32_t TX_TIMEOUT_MS = 5000UL;

    static void onPacketSentThunk();
    void onPacketSent();

    SPIClass _spi = SPIClass(VSPI);
    Module* _module = nullptr;
    SX1262* _radio = nullptr;

    volatile bool _packetSent = false;
    bool _ready = false;
    int _lastError = RADIOLIB_ERR_NONE;

    static LoraService* _instance;
};