#include <Arduino.h>
#include <Wire.h>
#include <vector>

#include "board_config.h"
#include "app_types.h"
#include "pulse_counter.h"
#include "env_service.h"
#include "payload_builder.h"

#if ENABLE_LORAWAN
#include "lora_service.h"
#include "lorawan_config.h"
#include "lorawan_provisioning.h"
#endif

#if ENABLE_GPS
#include "gps_service.h"
#endif


// -----------------------------------------------------------------------------
// Services
// -----------------------------------------------------------------------------

static PulseCounterPcf8574 g_waterCounter;
static EnvService g_env;

#if ENABLE_LORAWAN
static LoraService g_lora;
static LorawanProvisioning g_lwCfg;
static bool g_lwLoadedFromNvs = false;
#endif

#if ENABLE_GPS
static HardwareSerial GpsSerial(1);
static GpsService g_gps;
#endif


// -----------------------------------------------------------------------------
// Runtime state
// -----------------------------------------------------------------------------

static bool g_waterInterfaceAvailable = false;

static uint32_t g_lastWaterPollMs = 0;
static uint32_t g_lastEnvSampleMs = 0;
static uint32_t g_lastStatusMs = 0;

#if ENABLE_LORAWAN
static uint32_t g_lastTxMs = 0;
#endif

static uint32_t g_lastLoggedPulseCount = 0;
static float g_lastLoggedLiters = 0.0f;


// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

static const char* moistureStateToString(MoistureState state)
{
    switch (state) {

        case MoistureState::DRY:
            return "DRY";

        case MoistureState::MOIST:
            return "MOIST";

        case MoistureState::WET:
            return "WET";

        case MoistureState::INVALID:
        default:
            return "INVALID";
    }
}


static bool parseUnsignedInteger(
    const String& text,
    uint32_t& value
)
{
    if (text.isEmpty()) {
        return false;
    }

    uint64_t result = 0;

    for (size_t i = 0; i < text.length(); ++i) {

        const char c = text[i];

        if (c < '0' || c > '9') {
            return false;
        }

        result =
            result * 10ULL +
            static_cast<uint64_t>(c - '0');

        if (result > 0xFFFFFFFFULL) {
            return false;
        }
    }

    value = static_cast<uint32_t>(result);

    return true;
}


// -----------------------------------------------------------------------------
// I2C diagnostics
// -----------------------------------------------------------------------------

static void scanI2CBus()
{
    Serial.println();
    Serial.println("I2C scan start");

    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; ++addr) {

        Wire.beginTransmission(addr);

        const uint8_t err =
            Wire.endTransmission(true);

        if (err == 0) {

            Serial.print("Found device at 0x");

            if (addr < 0x10) {
                Serial.print('0');
            }

            Serial.println(addr, HEX);

            ++count;
        }
    }

    Serial.printf(
        "Total devices: %u\n",
        count
    );

    Serial.println("I2C scan end");
    Serial.println();
}


// -----------------------------------------------------------------------------
// Battery
// -----------------------------------------------------------------------------

static BatteryData readBatteryData()
{
    BatteryData data{};

    /*
     * ANDROMEDA:
     *
     * VBAT
     *  |
     * 10k
     *  |
     *  +---- V_CHECK ---- GPIO35
     *  |
     * 10k
     *  |
     * GND
     *
     * Therefore:
     *
     * V_CHECK = VBAT / 2
     */

    static constexpr uint8_t NUM_SAMPLES = 16;

    uint32_t adcRawSum = 0;
    uint32_t adcMvSum = 0;

    for (uint8_t i = 0; i < NUM_SAMPLES; ++i) {

        adcRawSum += analogRead(
            PIN_BATTERY_ADC
        );

        adcMvSum += analogReadMilliVolts(
            PIN_BATTERY_ADC
        );

        delay(2);
    }

    const uint32_t adcRaw =
        adcRawSum / NUM_SAMPLES;

    const uint32_t vCheckMv =
        adcMvSum / NUM_SAMPLES;

    const uint32_t batteryMv =
        static_cast<uint32_t>(
            static_cast<float>(vCheckMv) *
            BATTERY_DIVIDER_RATIO
        );

    /*
     * Plausibility check.
     *
     * Reject obviously invalid/open measurements.
     */
    if (batteryMv < 2500UL ||
        batteryMv > 4500UL) {

        data.valid = false;
        data.millivolts = 0;
        data.low = false;
        data.critical = false;
        data.solarPresent = false;
        data.charging = false;

        Serial.printf(
            "BATT ADC raw=%lu vcheck=%lu mV vbat=%lu mV INVALID\n",
            static_cast<unsigned long>(adcRaw),
            static_cast<unsigned long>(vCheckMv),
            static_cast<unsigned long>(batteryMv)
        );

        return data;
    }

    data.valid = true;

    data.millivolts =
        static_cast<uint16_t>(
            batteryMv
        );

    data.low =
        batteryMv <
        BATTERY_LOW_MV;

    data.critical =
        batteryMv <
        BATTERY_CRITICAL_MV;

    /*
     * Not observable with current ANDROMEDA hardware.
     */
    data.solarPresent = false;
    data.charging = false;

    Serial.printf(
        "BATT ADC raw=%lu vcheck=%lu mV vbat=%lu mV\n",
        static_cast<unsigned long>(adcRaw),
        static_cast<unsigned long>(vCheckMv),
        static_cast<unsigned long>(batteryMv)
    );

    return data;
}


// -----------------------------------------------------------------------------
// Snapshot
// -----------------------------------------------------------------------------

static DeviceSnapshot buildSnapshot()
{
    DeviceSnapshot snap;

    snap.uptimeMs = millis();

    snap.water =
        g_waterCounter.getData();

    snap.env =
        g_env.getData();

#if ENABLE_GPS

    snap.gps =
        g_gps.getData();

#else

    snap.gps =
        GpsData{};

#endif

    snap.battery =
        readBatteryData();

    return snap;
}


// -----------------------------------------------------------------------------
// LoRaWAN helpers
// -----------------------------------------------------------------------------

#if ENABLE_LORAWAN

static const char* loraStatusToString(int status)
{
    switch (status) {

        case RADIOLIB_ERR_NONE:
            return "OK";

        case RADIOLIB_LORAWAN_NEW_SESSION:
            return "NEW_SESSION";

        case RADIOLIB_LORAWAN_SESSION_RESTORED:
            return "SESSION_RESTORED";

        /*
         * RadioLib: no Join Accept was received.
         * Kept numeric here to avoid depending on the exact
         * symbolic-name availability of the installed RadioLib release.
         */
        case -1116:
            return "NO_JOIN_ACCEPT";

        default:
            return "ERROR";
    }
}


static void printLoraStatus()
{
    const int status =
        g_lora.getLastError();

    Serial.println();
    Serial.println(
        "===== LORAWAN STATUS ====="
    );

    Serial.println(
        "Compiled        : YES"
    );

    Serial.printf(
        "Radio ready     : %s\n",
        g_lora.isReady()
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Joined          : %s\n",
        g_lora.isJoined()
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Last status     : %d (%s)\n",
        status,
        loraStatusToString(status)
    );

    Serial.printf(
        "Provision source: %s\n",
        g_lwLoadedFromNvs
            ? "NVS"
            : "FACTORY"
    );

    Serial.printf(
        "Provision valid : %s\n",
        g_lwCfg.valid
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Provision ver.  : %lu\n",
        static_cast<unsigned long>(
            g_lwCfg.version
        )
    );

    Serial.printf(
        "Region          : %s\n",
        g_lwCfg.region ==
                LorawanRegion::EU868
            ? "EU868"
            : "UNKNOWN"
    );

    Serial.print(
        "JoinEUI         : "
    );

    Serial.println(
        LorawanProvisioningStore::toHex(
            g_lwCfg.joinEui,
            sizeof(g_lwCfg.joinEui)
        )
    );

    Serial.print(
        "DevEUI          : "
    );

    Serial.println(
        LorawanProvisioningStore::toHex(
            g_lwCfg.devEui,
            sizeof(g_lwCfg.devEui)
        )
    );

    Serial.print(
        "AppKey          : "
    );

    Serial.println(
        LorawanProvisioningStore::toHex(
            g_lwCfg.appKey,
            sizeof(g_lwCfg.appKey)
        )
    );

    Serial.printf(
        "TX enabled      : %s\n",
        g_lwCfg.txEnable
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Uplink period   : %lu ms (%lu s)\n",
        static_cast<unsigned long>(
            g_lwCfg.uplinkPeriodMs
        ),
        static_cast<unsigned long>(
            g_lwCfg.uplinkPeriodMs / 1000UL
        )
    );

    Serial.printf(
        "FPort           : %u\n",
        static_cast<unsigned int>(
            g_lwCfg.uplinkFPort
        )
    );

    Serial.printf(
        "Confirmed       : %s\n",
        g_lwCfg.uplinkConfirmed
            ? "YES"
            : "NO"
    );

    Serial.println(
        "=========================="
    );

    Serial.println();
}


static bool sendLoraUplink(
    const DeviceSnapshot& snap,
    const char* reason
)
{
    if (!g_lora.isReady()) {

        Serial.println(
            "LoRaWAN send blocked: radio not ready"
        );

        return false;
    }

    if (!g_lora.isJoined()) {

        Serial.println(
            "LoRaWAN send blocked: device not joined"
        );

        return false;
    }

    const std::vector<uint8_t> payload =
        PayloadBuilder::buildBinary(
            snap
        );

    Serial.printf(
        "[LORAWAN] %s payload=",
        reason
    );

    Serial.println(
        PayloadBuilder::toHex(payload)
    );

    const bool sent =
        g_lora.sendUplink(
            payload.data(),
            payload.size(),
            g_lwCfg.uplinkFPort,
            g_lwCfg.uplinkConfirmed
        );

    const int status =
        g_lora.getLastError();

    Serial.printf(
        "[LORAWAN] %s %s len=%u fport=%u status=%s (%d)\n",
        reason,
        sent ? "OK" : "FAIL",
        static_cast<unsigned int>(
            payload.size()
        ),
        static_cast<unsigned int>(
            g_lwCfg.uplinkFPort
        ),
        loraStatusToString(status),
        status
    );

    return sent;
}

#endif


// -----------------------------------------------------------------------------
// Status output
// -----------------------------------------------------------------------------

static void printSnapshot(
    const DeviceSnapshot& s
)
{
    Serial.println(
        "===== ANDROMEDA STATUS ====="
    );

    Serial.printf(
        "UPTIME %lu ms\n",
        static_cast<unsigned long>(
            s.uptimeMs
        )
    );

    // -------------------------------------------------------------------------
    // Water interface
    // -------------------------------------------------------------------------

    if (g_waterInterfaceAvailable) {

        Serial.printf(
            "WATER IF=OK pulses=%lu liters=%.3f line=%s lastPulseMs=%lu\n",
            static_cast<unsigned long>(
                s.water.pulseCount
            ),
            s.water.liters,
            s.water.lineState
                ? "HIGH"
                : "LOW",
            static_cast<unsigned long>(
                s.water.lastPulseMs
            )
        );

        const uint32_t pulseDelta =
            s.water.pulseCount -
            g_lastLoggedPulseCount;

        const float literDelta =
            s.water.liters -
            g_lastLoggedLiters;

        Serial.printf(
            "WATERD pulses=%lu liters=%.3f\n",
            static_cast<unsigned long>(
                pulseDelta
            ),
            literDelta
        );

    } else {

        Serial.println(
            "WATER IF=FAIL"
        );
    }

    // -------------------------------------------------------------------------
    // Temperature / humidity
    // -------------------------------------------------------------------------

    if (!s.env.sht30Present) {

        Serial.println(
            "SHT    NOT PRESENT"
        );

    } else {

        Serial.print(
            "SHT    "
        );

        if (isnan(
                s.env.temperatureC
            )) {

            Serial.print(
                "temp=INVALID"
            );

        } else {

            Serial.printf(
                "temp=%.2f C",
                s.env.temperatureC
            );
        }

        Serial.print(" ");

        if (isnan(
                s.env.humidityRH
            )) {

            Serial.print(
                "hum=INVALID"
            );

        } else {

            Serial.printf(
                "hum=%.2f %%RH",
                s.env.humidityRH
            );
        }

        Serial.println();
    }

    // -------------------------------------------------------------------------
    // Soil moisture
    // -------------------------------------------------------------------------

    if (!s.env.moisturePresent ||
        s.env.moistureRaw ==
            0xFFFFU) {

        Serial.println(
            "MOIST  INVALID"
        );

    } else {

        Serial.printf(
            "MOIST  raw=%u pct=%d state=%s\n",
            s.env.moistureRaw,
            s.env.moisturePct ==
                    0xFF
                ? -1
                : static_cast<int>(
                    s.env.moisturePct
                ),
            moistureStateToString(
                s.env.moistureState
            )
        );
    }

#if ENABLE_GPS

    Serial.printf(
        "GPS    fix=%s sats=%lu lat=%.7f lon=%.7f alt=%.2f hdop=%.2f\n",
        s.gps.fix
            ? "YES"
            : "NO",
        static_cast<unsigned long>(
            s.gps.satellites
        ),
        s.gps.latitude,
        s.gps.longitude,
        s.gps.altitudeMeters,
        s.gps.hdop
    );

#endif

    // -------------------------------------------------------------------------
    // Battery
    // -------------------------------------------------------------------------

    if (s.battery.valid) {

        Serial.printf(
            "BATT   mv=%u low=%s critical=%s solar=%s charging=%s\n",
            s.battery.millivolts,
            s.battery.low
                ? "YES"
                : "NO",
            s.battery.critical
                ? "YES"
                : "NO",
            s.battery.solarPresent
                ? "YES"
                : "NO",
            s.battery.charging
                ? "YES"
                : "NO"
        );

    } else {

        Serial.println(
            "BATT   INVALID / NOT IMPLEMENTED"
        );
    }

    // -------------------------------------------------------------------------
    // Payload
    // -------------------------------------------------------------------------

    const std::vector<uint8_t> payload =
        PayloadBuilder::buildBinary(
            s
        );

    Serial.print(
        "PAYLOAD v4 HEX="
    );

    Serial.println(
        PayloadBuilder::toHex(
            payload
        )
    );

    // -------------------------------------------------------------------------
    // LoRaWAN compact status
    // -------------------------------------------------------------------------

#if ENABLE_LORAWAN

    const int status =
        g_lora.getLastError();

    Serial.printf(
        "LORA   ready=%s joined=%s tx=%s period=%lus status=%s (%d)\n",
        g_lora.isReady()
            ? "YES"
            : "NO",
        g_lora.isJoined()
            ? "YES"
            : "NO",
        g_lwCfg.txEnable
            ? "YES"
            : "NO",
        static_cast<unsigned long>(
            g_lwCfg.uplinkPeriodMs /
            1000UL
        ),
        loraStatusToString(
            status
        ),
        status
    );

#else

    Serial.println(
        "LORA   DISABLED"
    );

#endif

    Serial.println(
        "============================"
    );

    Serial.println();

    g_lastLoggedPulseCount =
        s.water.pulseCount;

    g_lastLoggedLiters =
        s.water.liters;
}


// -----------------------------------------------------------------------------
// Serial commands
// -----------------------------------------------------------------------------

static void printHelp()
{
    Serial.println();
    Serial.println(
        "ANDROMEDA commands:"
    );

    Serial.println();

    Serial.println(
        "  status             Refresh sensors and show device status"
    );

#if ENABLE_LORAWAN

    Serial.println(
        "  lora               Show complete LoRaWAN status"
    );

    Serial.println(
        "  lora join          Join network if not already joined"
    );

    Serial.println(
        "  lora send          Send one telemetry uplink now"
    );

    Serial.println(
        "  lora tx off        Disable periodic LoRaWAN uplinks"
    );

    Serial.println(
        "  lora tx on         Enable periodic LoRaWAN uplinks"
    );

    Serial.println(
        "  lora period <sec>  Set periodic uplink interval"
    );

#endif

    Serial.println(
        "  i2c scan           Scan the I2C bus"
    );

    Serial.println(
        "  reboot             Reboot the device"
    );

    Serial.println(
        "  help               Show this help"
    );

    Serial.println();
}


static void processCommand(
    const String& command
)
{
    // -------------------------------------------------------------------------
    // General
    // -------------------------------------------------------------------------

    if (command == "i2c scan") {

        scanI2CBus();
        return;
    }

    if (command == "status") {

        /*
         * Force a fresh environmental sample so the
         * interactive status command reflects the current
         * sensor values rather than only the last periodic
         * acquisition.
         */
        g_env.update();

        g_lastEnvSampleMs =
            millis();

        printSnapshot(
            buildSnapshot()
        );

        return;
    }

#if ENABLE_LORAWAN

    // -------------------------------------------------------------------------
    // LoRaWAN status
    // -------------------------------------------------------------------------

    if (command == "lora") {

        printLoraStatus();
        return;
    }

    // -------------------------------------------------------------------------
    // LoRaWAN manual join
    // -------------------------------------------------------------------------

    if (command == "lora join") {

        if (!g_lora.isReady()) {

            Serial.println(
                "LoRaWAN join blocked: radio not ready"
            );

            return;
        }

        if (g_lora.isJoined()) {

            Serial.println(
                "LoRaWAN: already joined"
            );

            return;
        }

        Serial.println(
            "LoRaWAN: joining..."
        );

        const bool joined =
            g_lora.join();

        const int status =
            g_lora.getLastError();

        Serial.printf(
            "LoRaWAN join: %s status=%s (%d)\n",
            joined
                ? "OK"
                : "FAIL",
            loraStatusToString(
                status
            ),
            status
        );

        return;
    }

    // -------------------------------------------------------------------------
    // LoRaWAN manual uplink
    // -------------------------------------------------------------------------

    if (command == "lora send") {

        /*
         * Acquire fresh environmental data before an
         * explicitly requested uplink.
         */
        g_env.update();

        g_lastEnvSampleMs =
            millis();

        sendLoraUplink(
            buildSnapshot(),
            "manual uplink"
        );

        return;
    }

    // -------------------------------------------------------------------------
    // Periodic TX enable / disable
    // -------------------------------------------------------------------------

    if (command == "lora tx off") {

        g_lwCfg.txEnable =
            false;

        const bool saved =
            LorawanProvisioningStore::save(
                g_lwCfg
            );

        Serial.printf(
            "LoRaWAN periodic TX: OFF%s\n",
            saved
                ? " (saved to NVS)"
                : " (NVS SAVE FAILED)"
        );

        if (saved) {
            g_lwLoadedFromNvs = true;
        }

        return;
    }


    if (command == "lora tx on") {

        g_lwCfg.txEnable =
            true;

        const bool saved =
            LorawanProvisioningStore::save(
                g_lwCfg
            );

        /*
         * Start a complete new period from now.
         */
        g_lastTxMs =
            millis();

        Serial.printf(
            "LoRaWAN periodic TX: ON%s\n",
            saved
                ? " (saved to NVS)"
                : " (NVS SAVE FAILED)"
        );

        if (saved) {
            g_lwLoadedFromNvs = true;
        }

        return;
    }

    // -------------------------------------------------------------------------
    // Dynamic uplink period
    // -------------------------------------------------------------------------

    if (command.startsWith(
            "lora period "
        )) {

        String valueText =
            command.substring(
                String(
                    "lora period "
                ).length()
            );

        valueText.trim();

        uint32_t seconds = 0;

        if (!parseUnsignedInteger(
                valueText,
                seconds
            )) {

            Serial.println(
                "Invalid period."
            );

            Serial.println(
                "Usage: lora period <seconds>"
            );

            return;
        }

        /*
         * LorawanProvisioningStore::isSane()
         * already rejects periods shorter than
         * 10000 ms. Make the constraint explicit
         * at the CLI as well.
         */
        if (seconds < 10UL) {

            Serial.println(
                "Invalid period: minimum is 10 seconds."
            );

            return;
        }

        /*
         * One day is more than enough for this
         * installation and protects against obvious
         * accidental input.
         */
        if (seconds > 86400UL) {

            Serial.println(
                "Invalid period: maximum is 86400 seconds."
            );

            return;
        }

        const uint32_t oldPeriodMs =
            g_lwCfg.uplinkPeriodMs;

        g_lwCfg.uplinkPeriodMs =
            seconds * 1000UL;

        const bool saved =
            LorawanProvisioningStore::save(
                g_lwCfg
            );

        if (!saved) {

            /*
             * Restore the runtime value if NVS
             * persistence failed.
             */
            g_lwCfg.uplinkPeriodMs =
                oldPeriodMs;

            Serial.println(
                "LoRaWAN period update FAILED: NVS save failed."
            );

            return;
        }

        g_lwLoadedFromNvs =
            true;

        /*
         * Start the new interval from the instant
         * the configuration changes.
         */
        g_lastTxMs =
            millis();

        Serial.printf(
            "LoRaWAN uplink period: %lu seconds (saved to NVS)\n",
            static_cast<unsigned long>(
                seconds
            )
        );

        return;
    }

#endif

    // -------------------------------------------------------------------------
    // Reboot
    // -------------------------------------------------------------------------

    if (command == "reboot") {

        Serial.println(
            "Rebooting..."
        );

        delay(100);

        ESP.restart();

        return;
    }

    // -------------------------------------------------------------------------
    // Help
    // -------------------------------------------------------------------------

    if (command == "help") {

        printHelp();
        return;
    }

    Serial.println(
        "Unknown command"
    );
}


// -----------------------------------------------------------------------------
// Non-blocking serial input
// -----------------------------------------------------------------------------

static void handleSerial()
{
    static String line;

    while (Serial.available() > 0) {

        const char c =
            static_cast<char>(
                Serial.read()
            );

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {

            line.trim();

            if (!line.isEmpty()) {

                processCommand(
                    line
                );
            }

            line = "";

            continue;
        }

        /*
         * Bound command length so malformed serial
         * input cannot grow String indefinitely.
         */
        if (line.length() < 80) {

            line += c;
        }
    }
}


// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup()
{
    analogReadResolution(12);

    analogSetPinAttenuation(
        PIN_BATTERY_ADC,
        ADC_11db
    );

    Serial.begin(115200);

    /*
     * Short startup delay only.
     *
     * During development:
     *   1. open the serial monitor;
     *   2. press RESET / EN;
     *
     * No long boot delay is required.
     */
    delay(1500);

    Serial.println();

    Serial.println(
        "ANDROMEDA @ ZESPRI"
    );

    Serial.println(
        "Booting..."
    );

#if ENABLE_LORAWAN

    Serial.println(
        "BUILD : LORAWAN ENABLED"
    );

#else

    Serial.println(
        "BUILD : LOCAL TELEMETRY ONLY"
    );

#endif

    // -------------------------------------------------------------------------
    // I2C
    // -------------------------------------------------------------------------

    pinMode(
        PIN_I2C_SDA,
        INPUT_PULLUP
    );

    pinMode(
        PIN_I2C_SCL,
        INPUT_PULLUP
    );

    Serial.printf(
        "I2C lines before init: SDA=%d SCL=%d\n",
        digitalRead(
            PIN_I2C_SDA
        ),
        digitalRead(
            PIN_I2C_SCL
        )
    );

    Wire.begin(
        PIN_I2C_SDA,
        PIN_I2C_SCL
    );

    Wire.setClock(
        I2C_FREQUENCY_HZ
    );

    Wire.setTimeOut(
        I2C_TIMEOUT_MS
    );

    Serial.printf(
        "I2C lines after init : SDA=%d SCL=%d\n",
        digitalRead(
            PIN_I2C_SDA
        ),
        digitalRead(
            PIN_I2C_SCL
        )
    );

    // -------------------------------------------------------------------------
    // Water meter interface
    // -------------------------------------------------------------------------

    g_waterInterfaceAvailable =
        g_waterCounter.begin(
            Wire,
            I2C_ADDR_PCF8574,
            PCF8574_WATER_INPUT_BIT
        );

    Serial.printf(
        "WATER IF : %s\n",
        g_waterInterfaceAvailable
            ? "OK"
            : "FAIL"
    );

    // -------------------------------------------------------------------------
    // Environment sensors
    // -------------------------------------------------------------------------

    const bool envAvailable =
        g_env.begin(
            Wire
        );

    /*
     * Immediate first acquisition.
     */
    g_env.update();

    const EnvData env =
        g_env.getData();

    Serial.printf(
        "SHT   : %s\n",
        env.sht30Present
            ? "OK"
            : "FAIL"
    );

    Serial.printf(
        "MOIST : %s\n",
        env.moisturePresent
            ? "OK"
            : "FAIL"
    );

    Serial.printf(
        "ENV   : %s\n",
        envAvailable
            ? "AVAILABLE"
            : "FAIL"
    );

    // -------------------------------------------------------------------------
    // GPS
    // -------------------------------------------------------------------------

#if ENABLE_GPS

    g_gps.begin(
        GpsSerial,
        GPS_BAUDRATE,
        PIN_GPS_RX,
        PIN_GPS_TX
    );

    Serial.println(
        "GPS   : INITIALIZED"
    );

#endif

    // -------------------------------------------------------------------------
    // LoRaWAN
    // -------------------------------------------------------------------------

#if ENABLE_LORAWAN

    Serial.println();

    Serial.println(
        "Initializing LoRaWAN..."
    );

    /*
     * Runtime provisioning stored in NVS has
     * precedence over factory defaults.
     */
    g_lwLoadedFromNvs =
        LorawanProvisioningStore::load(
            g_lwCfg
        );

    if (g_lwLoadedFromNvs) {

        Serial.println(
            "[LORAWAN] Provisioning loaded from NVS"
        );

    } else {

        Serial.println(
            "[LORAWAN] No valid NVS provisioning; using factory defaults"
        );

        g_lwCfg =
            LorawanProvisioningStore::
                makeFactoryDefault();
    }

    /*
     * Explicit provisioning dump during development.
     *
     * NOTE:
     * this includes AppKey by design for the current
     * laboratory/debug phase.
     */
    LorawanProvisioningStore::print(
        g_lwCfg,
        Serial
    );

    const bool loraReady =
        g_lora.begin(
            g_lwCfg
        );

    Serial.printf(
        "LORA RADIO : %s\n",
        loraReady
            ? "OK"
            : "FAIL"
    );

    if (loraReady) {

        Serial.println(
            "[LORAWAN] Joining or restoring session..."
        );

        const bool joined =
            g_lora.join();

        const int status =
            g_lora.getLastError();

        Serial.printf(
            "LORA JOIN  : %s status=%s (%d)\n",
            joined
                ? "OK"
                : "FAIL",
            loraStatusToString(
                status
            ),
            status
        );
    }

#endif

    // -------------------------------------------------------------------------
    // Initial timestamps
    // -------------------------------------------------------------------------

    const uint32_t now =
        millis();

    g_lastWaterPollMs =
        now;

    g_lastEnvSampleMs =
        now;

    g_lastStatusMs =
        now;

#if ENABLE_LORAWAN

    g_lastTxMs =
        now;

#endif

    // -------------------------------------------------------------------------
    // Ready
    // -------------------------------------------------------------------------

    Serial.println();

    Serial.println(
        "System ready"
    );

    Serial.println(
        "Type 'help' for commands."
    );

    Serial.println();

    printSnapshot(
        buildSnapshot()
    );
}


// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop()
{
    const uint32_t nowMs =
        millis();

    // -------------------------------------------------------------------------
    // Serial console
    // -------------------------------------------------------------------------

    handleSerial();

    // -------------------------------------------------------------------------
    // GPS
    // -------------------------------------------------------------------------

#if ENABLE_GPS

    g_gps.update();

#endif

    // -------------------------------------------------------------------------
    // Water meter
    // -------------------------------------------------------------------------

    if (g_waterInterfaceAvailable &&
        (nowMs -
             g_lastWaterPollMs >=
         WATER_POLL_PERIOD_MS)) {

        g_lastWaterPollMs =
            nowMs;

        g_waterCounter.update(
            nowMs
        );
    }

    // -------------------------------------------------------------------------
    // Environment
    // -------------------------------------------------------------------------

    if (nowMs -
            g_lastEnvSampleMs >=
        SENSOR_SAMPLE_PERIOD_MS) {

        g_lastEnvSampleMs =
            nowMs;

        g_env.update();
    }

    // -------------------------------------------------------------------------
    // Periodic local status
    // -------------------------------------------------------------------------

    if (nowMs -
            g_lastStatusMs >=
        STATUS_LOG_PERIOD_MS) {

        g_lastStatusMs =
            nowMs;

        printSnapshot(
            buildSnapshot()
        );
    }

    // -------------------------------------------------------------------------
    // Periodic LoRaWAN uplink
    // -------------------------------------------------------------------------

#if ENABLE_LORAWAN

    if (g_lwCfg.txEnable &&
        g_lora.isJoined() &&
        (nowMs -
             g_lastTxMs >=
         g_lwCfg.uplinkPeriodMs)) {

        g_lastTxMs =
            nowMs;

        sendLoraUplink(
            buildSnapshot(),
            "periodic uplink"
        );
    }

#endif
}
