#include "lora_service.h"
#include "board_config.h"

LoraService* LoraService::_instance = nullptr;

void LoraService::onPacketSentThunk() {
    if (_instance != nullptr) {
        _instance->onPacketSent();
    }
}

void LoraService::onPacketSent() {
    _packetSent = true;
}

bool LoraService::begin() {
    _ready = false;
    _lastError = RADIOLIB_ERR_NONE;
    _packetSent = false;

    if (_radio != nullptr) {
        delete _radio;
        _radio = nullptr;
    }

    if (_module != nullptr) {
        delete _module;
        _module = nullptr;
    }

    if (PIN_LORA_RXEN >= 0) {
        pinMode(PIN_LORA_RXEN, OUTPUT);
        digitalWrite(PIN_LORA_RXEN, HIGH);
    }

    _spi.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

    _module = new Module(
        PIN_LORA_NSS,
        PIN_LORA_DIO1,
        PIN_LORA_RST,
        PIN_LORA_BUSY,
        _spi
    );

    _radio = new SX1262(_module);

    _lastError = _radio->begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER_DBM
    );

    if (_lastError != RADIOLIB_ERR_NONE) {
        _ready = false;
        return false;
    }

    _instance = this;
    _radio->setPacketSentAction(onPacketSentThunk);

    _ready = true;
    return true;
}

bool LoraService::send(const uint8_t* data, size_t len) {
    if (!_ready || _radio == nullptr || data == nullptr || len == 0) {
        _lastError = ERR_NOT_READY_OR_INVALID_ARGS;
        return false;
    }

    _packetSent = false;

    _lastError = _radio->startTransmit(data, len);
    if (_lastError != RADIOLIB_ERR_NONE) {
        _radio->finishTransmit();
        return false;
    }

    const uint32_t t0 = millis();
    while (!_packetSent && (millis() - t0) < TX_TIMEOUT_MS) {
        delay(1);
    }

    _radio->finishTransmit();

    if (!_packetSent) {
        _lastError = RADIOLIB_ERR_TX_TIMEOUT;
        return false;
    }

    _lastError = RADIOLIB_ERR_NONE;
    return true;
}

bool LoraService::isReady() const {
    return _ready;
}

int LoraService::getLastError() const {
    return _lastError;
}