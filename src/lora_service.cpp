#include "lora_service.h"
#include "board_config.h"
#include "lorawan_config.h"

bool LoraService::begin(const LorawanProvisioning& cfg) {
    _ready = false;
    _joined = false;
    _lastError = RADIOLIB_ERR_NONE;
    _cfg = cfg;

    delete _node;   _node = nullptr;
    delete _radio;  _radio = nullptr;
    delete _module; _module = nullptr;

    Serial.println("[LORAWAN] radio begin...");

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

    _lastError = _radio->begin();
    if (_lastError != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORAWAN] radio begin FAIL err=%d\n", _lastError);
        return false;
    }

    const LoRaWANBand_t* band = nullptr;
    switch (_cfg.region) {
        case LorawanRegion::EU868:
            band = &EU868;
            break;
        default:
            _lastError = -32020;
            Serial.println("[LORAWAN] unsupported region");
            return false;
    }

    _node = new LoRaWANNode(_radio, band);
    if (_node == nullptr) {
        _lastError = -32000;
        Serial.println("[LORAWAN] node alloc FAIL");
        return false;
    }

    uint64_t joinEUI = 0;
    uint64_t devEUI = 0;

    for (int i = 0; i < 8; i++) {
        joinEUI = (joinEUI << 8) | _cfg.joinEui[i];
        devEUI  = (devEUI  << 8) | _cfg.devEui[i];
    }

    _lastError = _node->beginOTAA(
        joinEUI,
        devEUI,
        nullptr,
        _cfg.appKey
    );

    if (_lastError != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORAWAN] beginOTAA FAIL err=%d\n", _lastError);
        return false;
    }

    Serial.println("[LORAWAN] stack ready");
    _ready = true;
    return true;
}

bool LoraService::tryRestoreSession() {
    if (_node == nullptr) {
        _lastError = -32010;
        return false;
    }

    bool restored = persist.loadSession(_node);

    if (restored) {
        _joined = true;
        _lastError = RADIOLIB_ERR_NONE;
        Serial.println("[LORAWAN] session restore OK");
        return true;
    }

    _joined = false;
    _lastError = -32013;
    Serial.println("[LORAWAN] no restorable session, OTAA required");
    return false;
}

bool LoraService::persistAfterJoinOrRestore() {
    if (_node == nullptr) {
        _lastError = -32011;
        return false;
    }

    bool saved = persist.saveSession(_node);
    if (saved) {
        Serial.println("[LORAWAN] session persisted");
        return true;
    }

    _lastError = -32014;
    Serial.println("[LORAWAN] session persist FAIL");
    return false;
}

bool LoraService::persistAfterUplink() {
    if (_node == nullptr) {
        _lastError = -32012;
        return false;
    }

    bool saved = persist.saveSession(_node);
    if (saved) {
        Serial.println("[LORAWAN] session updated");
        return true;
    }

    _lastError = -32015;
    Serial.println("[LORAWAN] session update FAIL");
    return false;
}

bool LoraService::join() {
    if (!_ready || _node == nullptr) {
        _lastError = -32001;
        return false;
    }

    // Prima provo a ripristinare una sessione valida già salvata.
    if (tryRestoreSession()) {
        return true;
    }

    Serial.println("[LORAWAN] join start");

    _lastError = _node->activateOTAA();

    // Persisto SEMPRE dopo il tentativo di join,
    // anche se fallisce, per non perdere lo stato/nonces.
    bool persistOk = persistAfterJoinOrRestore();
    Serial.printf(
        "[LORAWAN] persist after join attempt: %s | joinErr=%d\n",
        persistOk ? "OK" : "FAIL",
        _lastError
    );

    if ((_lastError == RADIOLIB_LORAWAN_NEW_SESSION) ||
        (_lastError == RADIOLIB_LORAWAN_SESSION_RESTORED)) {
        _joined = true;
        Serial.printf("[LORAWAN] join success state=%d\n", _lastError);
        return true;
    }

    _joined = false;
    Serial.printf("[LORAWAN] join fail err=%d\n", _lastError);
    return false;
}

bool LoraService::sendUplink(const uint8_t* data,
                             size_t len,
                             uint8_t fport,
                             bool confirmed) {
    if (!_joined) {
        _lastError = -32002;
        Serial.println("[LORAWAN] uplink blocked: node not joined");
        return false;
    }

    if (_node == nullptr || data == nullptr || len == 0) {
        _lastError = -32003;
        Serial.printf("[LORAWAN] uplink FAIL precheck err=%d\n", _lastError);
        return false;
    }

    Serial.printf("[LORAWAN] uplink start port=%u bytes=%u confirmed=%s\n",
                  fport,
                  (unsigned)len,
                  confirmed ? "YES" : "NO");

    int16_t downlinkLen = _node->sendReceive(
        data,
        len,
        fport,
        confirmed
    );

    if (downlinkLen >= 0) {
        _lastError = RADIOLIB_ERR_NONE;
        Serial.printf("[LORAWAN] uplink ok downlinkWin=%d\n", downlinkLen);
        persistAfterUplink();
        return true;
    }

    _lastError = downlinkLen;
    Serial.printf("[LORAWAN] uplink FAIL err=%d\n", _lastError);
    return false;
}

bool LoraService::isReady() const {
    return _ready;
}

bool LoraService::isJoined() const {
    return _joined;
}

int LoraService::getLastError() const {
    return _lastError;
}