#include "lora_service.h"

#include "board_config.h"
#include "lorawan_config.h"

#include <Preferences.h>
#include <string.h>


// -----------------------------------------------------------------------------
// Persistent complete LoRaWAN session
// -----------------------------------------------------------------------------

static constexpr const char* SESSION_NVS_NAMESPACE =
    "lwsession";

static constexpr const char* SESSION_NVS_KEY =
    "session";

static constexpr uint32_t SESSION_MAGIC =
    0x4C575331UL;     // "LWS1"

static constexpr uint16_t SESSION_FORMAT_VERSION =
    1;


/*
 * RadioLib's session buffer already contains its own integrity information.
 *
 * The wrapper below additionally allows us to identify our NVS record and
 * reject incompatible session-buffer sizes after future RadioLib upgrades.
 */
struct PersistentLoRaSession {
    uint32_t magic;
    uint16_t formatVersion;
    uint16_t bufferSize;

    uint8_t buffer[
        RADIOLIB_LORAWAN_SESSION_BUF_SIZE
    ];
};


// -----------------------------------------------------------------------------
// Begin
// -----------------------------------------------------------------------------

bool LoraService::begin(
    const LorawanProvisioning& cfg
)
{
    _ready = false;
    _joined = false;

    _lastError =
        RADIOLIB_ERR_NONE;

    _cfg = cfg;

    delete _node;
    _node = nullptr;

    delete _radio;
    _radio = nullptr;

    delete _module;
    _module = nullptr;

    // -------------------------------------------------------------------------
    // Radio control
    // -------------------------------------------------------------------------

    if (PIN_LORA_RXEN >= 0) {

        pinMode(
            PIN_LORA_RXEN,
            OUTPUT
        );

        digitalWrite(
            PIN_LORA_RXEN,
            HIGH
        );
    }

    // -------------------------------------------------------------------------
    // SPI
    // -------------------------------------------------------------------------

    _spi.begin(
        PIN_LORA_SCK,
        PIN_LORA_MISO,
        PIN_LORA_MOSI,
        PIN_LORA_NSS
    );

    // -------------------------------------------------------------------------
    // RadioLib module
    // -------------------------------------------------------------------------

    _module =
        new Module(
            PIN_LORA_NSS,
            PIN_LORA_DIO1,
            PIN_LORA_RST,
            PIN_LORA_BUSY,
            _spi
        );

    _radio =
        new SX1262(
            _module
        );

    _lastError =
        _radio->begin();

    if (_lastError !=
        RADIOLIB_ERR_NONE) {

        Serial.printf(
            "[LORAWAN] radio begin FAIL err=%d\n",
            _lastError
        );

        return false;
    }

    // -------------------------------------------------------------------------
    // Region
    // -------------------------------------------------------------------------

    const LoRaWANBand_t* band =
        nullptr;

    switch (_cfg.region) {

        case LorawanRegion::EU868:

            band =
                &EU868;

            break;

        default:

            _lastError =
                -32020;

            Serial.println(
                "[LORAWAN] unsupported region"
            );

            return false;
    }

    // -------------------------------------------------------------------------
    // Node
    // -------------------------------------------------------------------------

    _node =
        new LoRaWANNode(
            _radio,
            band
        );

    if (_node == nullptr) {

        _lastError =
            -32000;

        Serial.println(
            "[LORAWAN] node alloc FAIL"
        );

        return false;
    }

    // -------------------------------------------------------------------------
    // OTAA identifiers
    // -------------------------------------------------------------------------

    uint64_t joinEUI = 0;
    uint64_t devEUI = 0;

    for (int i = 0; i < 8; ++i) {

        joinEUI =
            (joinEUI << 8) |
            _cfg.joinEui[i];

        devEUI =
            (devEUI << 8) |
            _cfg.devEui[i];
    }

    // -------------------------------------------------------------------------
    // Configure OTAA
    // -------------------------------------------------------------------------

    _lastError =
        _node->beginOTAA(
            joinEUI,
            devEUI,
            nullptr,
            _cfg.appKey
        );

    if (_lastError !=
        RADIOLIB_ERR_NONE) {

        Serial.printf(
            "[LORAWAN] beginOTAA FAIL err=%d\n",
            _lastError
        );

        return false;
    }

    _ready = true;

    return true;
}


// -----------------------------------------------------------------------------
// Full session NVS
// -----------------------------------------------------------------------------

bool LoraService::loadFullSessionFromNvs()
{
    if (_node == nullptr) {

        return false;
    }

    Preferences prefs;

    if (!prefs.begin(
            SESSION_NVS_NAMESPACE,
            true
        )) {

        return false;
    }

    const size_t storedSize =
        prefs.getBytesLength(
            SESSION_NVS_KEY
        );

    if (storedSize !=
        sizeof(PersistentLoRaSession)) {

        prefs.end();

        return false;
    }

    PersistentLoRaSession saved{};

    const size_t readSize =
        prefs.getBytes(
            SESSION_NVS_KEY,
            &saved,
            sizeof(saved)
        );

    prefs.end();

    if (readSize !=
        sizeof(saved)) {

        return false;
    }

    if (saved.magic !=
        SESSION_MAGIC) {

        Serial.println(
            "[LORAWAN] NVS session rejected: bad magic"
        );

        return false;
    }

    if (saved.formatVersion !=
        SESSION_FORMAT_VERSION) {

        Serial.println(
            "[LORAWAN] NVS session rejected: unsupported format"
        );

        return false;
    }

    if (saved.bufferSize !=
        RADIOLIB_LORAWAN_SESSION_BUF_SIZE) {

        Serial.println(
            "[LORAWAN] NVS session rejected: RadioLib session size changed"
        );

        return false;
    }

    /*
     * setBufferSession() validates RadioLib's internal session
     * signature/checksum before restoring it.
     */
    const int16_t state =
        _node->setBufferSession(
            saved.buffer
        );

    if (state !=
        RADIOLIB_ERR_NONE) {

        Serial.printf(
            "[LORAWAN] NVS session restore FAIL err=%d\n",
            state
        );

        return false;
    }

    Serial.println(
        "[LORAWAN] full session restored from NVS"
    );

    return true;
}


bool LoraService::saveFullSessionToNvs()
{
    if (_node == nullptr) {

        return false;
    }

    uint8_t* sessionBuffer =
        _node->getBufferSession();

    if (sessionBuffer == nullptr) {

        Serial.println(
            "[LORAWAN] session buffer unavailable"
        );

        return false;
    }

    PersistentLoRaSession saved{};

    saved.magic =
        SESSION_MAGIC;

    saved.formatVersion =
        SESSION_FORMAT_VERSION;

    saved.bufferSize =
        RADIOLIB_LORAWAN_SESSION_BUF_SIZE;

    memcpy(
        saved.buffer,
        sessionBuffer,
        RADIOLIB_LORAWAN_SESSION_BUF_SIZE
    );

    Preferences prefs;

    if (!prefs.begin(
            SESSION_NVS_NAMESPACE,
            false
        )) {

        Serial.println(
            "[LORAWAN] NVS session open FAIL"
        );

        return false;
    }

    const size_t written =
        prefs.putBytes(
            SESSION_NVS_KEY,
            &saved,
            sizeof(saved)
        );

    prefs.end();

    if (written !=
        sizeof(saved)) {

        Serial.println(
            "[LORAWAN] NVS session write FAIL"
        );

        return false;
    }

    return true;
}


// -----------------------------------------------------------------------------
// Restore
// -----------------------------------------------------------------------------

bool LoraService::tryRestoreSession()
{
    if (_node == nullptr) {

        _lastError = -32010;

        return false;
    }

    // -------------------------------------------------------------------------
    // First try the native LoRaWAN_ESP32 RTC/session restore
    // -------------------------------------------------------------------------

    const bool rtcRestored =
        persist.loadSession(_node);

    if (rtcRestored) {

        /*
         * Even after restoring the buffers, RadioLib must activate
         * the restored session internally.
         */
        const int16_t state =
            _node->activateOTAA();

        if (state !=
            RADIOLIB_LORAWAN_SESSION_RESTORED) {

            Serial.printf(
                "[LORAWAN] RTC session activation FAIL err=%d\n",
                state
            );

            _joined = false;
            _lastError = state;

            return false;
        }

        _joined = true;
        _lastError =
            RADIOLIB_LORAWAN_SESSION_RESTORED;

        Serial.println(
            "[LORAWAN] session restored by LoRaWAN_ESP32"
        );

        saveFullSessionToNvs();

        return true;
    }

    // -------------------------------------------------------------------------
    // Full ANDROMEDA NVS session restore
    // -------------------------------------------------------------------------

    if (loadFullSessionFromNvs()) {

        /*
         * setBufferSession() restores the serialized state, but
         * activateOTAA() is what switches RadioLib back to the
         * active-session state.
         *
         * Since valid nonces + session are already present,
         * this does NOT perform a new OTAA exchange.
         */
        const int16_t state =
            _node->activateOTAA();

        if (state !=
            RADIOLIB_LORAWAN_SESSION_RESTORED) {

            Serial.printf(
                "[LORAWAN] restored session activation FAIL err=%d\n",
                state
            );

            _joined = false;
            _lastError = state;

            return false;
        }

        _joined = true;

        _lastError =
            RADIOLIB_LORAWAN_SESSION_RESTORED;

        Serial.println(
            "[LORAWAN] full session activated from NVS"
        );

        /*
         * Synchronize LoRaWAN_ESP32's RTC/nonces representation too.
         */
        persist.saveSession(_node);

        return true;
    }

    _joined = false;

    _lastError =
        RADIOLIB_ERR_NETWORK_NOT_JOINED;

    return false;
}


// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

bool LoraService::persistAfterJoinOrRestore()
{
    if (_node == nullptr) {

        _lastError =
            -32011;

        return false;
    }

    const bool helperSaved =
        persist.saveSession(
            _node
        );

    const bool fullSaved =
        saveFullSessionToNvs();

    if (helperSaved &&
        fullSaved) {

        return true;
    }

    Serial.printf(
        "[LORAWAN] session persist FAIL helper=%s fullNvs=%s\n",
        helperSaved ? "OK" : "FAIL",
        fullSaved ? "OK" : "FAIL"
    );

    return false;
}


bool LoraService::persistAfterUplink()
{
    if (_node == nullptr) {

        _lastError =
            -32012;

        return false;
    }

    /*
     * Persist AFTER every successful uplink.
     *
     * The RadioLib session contains frame counters and MAC/session state,
     * so this ensures that a sudden reset or power loss comes back with
     * the latest known session state.
     *
     * For the current validation phase correctness is preferred over
     * minimizing NVS writes. Flash-write optimization can be introduced
     * later once the final field uplink period is established.
     */
    const bool helperSaved =
        persist.saveSession(
            _node
        );

    const bool fullSaved =
        saveFullSessionToNvs();

    if (helperSaved &&
        fullSaved) {

        return true;
    }

    Serial.printf(
        "[LORAWAN] session update FAIL helper=%s fullNvs=%s\n",
        helperSaved ? "OK" : "FAIL",
        fullSaved ? "OK" : "FAIL"
    );

    return false;
}


// -----------------------------------------------------------------------------
// Join / restore
// -----------------------------------------------------------------------------

bool LoraService::join()
{
    if (!_ready ||
        _node == nullptr) {

        _lastError =
            -32001;

        return false;
    }

    // -------------------------------------------------------------------------
    // Existing session
    // -------------------------------------------------------------------------

    if (tryRestoreSession()) {

        return true;
    }

    // -------------------------------------------------------------------------
    // New OTAA session
    // -------------------------------------------------------------------------

    _lastError =
        _node->activateOTAA();

    if ((_lastError ==
             RADIOLIB_LORAWAN_NEW_SESSION) ||
        (_lastError ==
             RADIOLIB_LORAWAN_SESSION_RESTORED)) {

        _joined = true;

        /*
         * Save BOTH:
         *
         *  - LoRaWAN_ESP32 RTC/nonces persistence;
         *  - complete RadioLib session in NVS.
         */
        const bool persistOk =
            persistAfterJoinOrRestore();

        if (!persistOk) {

            Serial.println(
                "[LORAWAN] WARNING: joined, but complete persistence failed"
            );
        }

        return true;
    }

    /*
     * Even after an unsuccessful join attempt, allow
     * LoRaWAN_ESP32 to save updated nonce information.
     */
    persist.saveSession(
        _node
    );

    _joined = false;

    return false;
}


// -----------------------------------------------------------------------------
// Uplink
// -----------------------------------------------------------------------------

bool LoraService::sendUplink(
    const uint8_t* data,
    size_t len,
    uint8_t fport,
    bool confirmed
)
{
    if (!_joined) {

        _lastError =
            -32002;

        Serial.println(
            "[LORAWAN] uplink blocked: node not joined"
        );

        return false;
    }

    if (_node == nullptr ||
        data == nullptr ||
        len == 0) {

        _lastError =
            -32003;

        Serial.printf(
            "[LORAWAN] uplink FAIL precheck err=%d\n",
            _lastError
        );

        return false;
    }

    const int16_t downlinkLen =
        _node->sendReceive(
            data,
            len,
            fport,
            confirmed
        );

    if (downlinkLen >= 0) {

        _lastError =
            RADIOLIB_ERR_NONE;

        /*
         * Frame counters and MAC/session state may have changed.
         * Persist immediately.
         */
        if (!persistAfterUplink()) {

            Serial.println(
                "[LORAWAN] WARNING: uplink succeeded but session persistence failed"
            );
        }

        return true;
    }

    _lastError =
        downlinkLen;

    Serial.printf(
        "[LORAWAN] uplink FAIL err=%d\n",
        _lastError
    );

    return false;
}


// -----------------------------------------------------------------------------
// State
// -----------------------------------------------------------------------------

bool LoraService::isReady() const
{
    return _ready;
}


bool LoraService::isJoined() const
{
    return _joined;
}


int LoraService::getLastError() const
{
    return _lastError;
}
