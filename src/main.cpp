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

    Serial.printf("Total devices: %u\n", count);
    Serial.println("I2C scan end");
    Serial.println();
}


// -----------------------------------------------------------------------------
// Battery
// -----------------------------------------------------------------------------

static BatteryData readBatteryData()
{
    BatteryData data;

    /*
     * Battery acquisition is NOT yet validated.
     *
     * Do not emit fabricated telemetry.
     */
    data.valid = false;
    data.millivolts = 0;
    data.low = false;
    data.critical = false;
    data.solarPresent = false;
    data.charging = false;

    return data;
}


// -----------------------------------------------------------------------------
// Snapshot
// -----------------------------------------------------------------------------

static DeviceSnapshot buildSnapshot()
{
    DeviceSnapshot snap;

    snap.uptimeMs = millis();

    snap.water = g_waterCounter.getData();
    snap.env = g_env.getData();

#if ENABLE_GPS
    snap.gps = g_gps.getData();
#else
    snap.gps = GpsData{};
#endif

    snap.battery = readBatteryData();

    return snap;
}


// -----------------------------------------------------------------------------
// Status output
// -----------------------------------------------------------------------------

static void printSnapshot(const DeviceSnapshot& s)
{
    Serial.println("===== ANDROMEDA STATUS =====");

    Serial.printf(
        "UPTIME %lu ms\n",
        static_cast<unsigned long>(s.uptimeMs)
    );

    if (g_waterInterfaceAvailable) {

        Serial.printf(
            "WATER IF=OK pulses=%lu liters=%.3f line=%s lastPulseMs=%lu\n",
            static_cast<unsigned long>(s.water.pulseCount),
            s.water.liters,
            s.water.lineState ? "HIGH" : "LOW",
            static_cast<unsigned long>(s.water.lastPulseMs)
        );

        const uint32_t pulseDelta =
            s.water.pulseCount - g_lastLoggedPulseCount;

        const float literDelta =
            s.water.liters - g_lastLoggedLiters;

        Serial.printf(
            "WATERD pulses=%lu liters=%.3f\n",
            static_cast<unsigned long>(pulseDelta),
            literDelta
        );

    } else {

        Serial.println("WATER IF=FAIL");
    }

    // -------------------------------------------------------------------------
    // Temperature / humidity
    // -------------------------------------------------------------------------

    if (!s.env.sht30Present) {

        Serial.println("SHT    NOT PRESENT");

    } else {

        Serial.print("SHT    ");

        if (isnan(s.env.temperatureC)) {
            Serial.print("temp=INVALID");
        } else {
            Serial.printf("temp=%.2f C", s.env.temperatureC);
        }

        Serial.print(" ");

        if (isnan(s.env.humidityRH)) {
            Serial.print("hum=INVALID");
        } else {
            Serial.printf("hum=%.2f %%RH", s.env.humidityRH);
        }

        Serial.println();
    }

    // -------------------------------------------------------------------------
    // Soil moisture
    // -------------------------------------------------------------------------

    if (!s.env.moisturePresent ||
        s.env.moistureRaw == 0xFFFFU) {

        Serial.println("MOIST  INVALID");

    } else {

        Serial.printf(
            "MOIST  raw=%u pct=%d state=%s\n",
            s.env.moistureRaw,
            s.env.moisturePct == 0xFF
                ? -1
                : static_cast<int>(s.env.moisturePct),
            moistureStateToString(s.env.moistureState)
        );
    }

#if ENABLE_GPS

    Serial.printf(
        "GPS    fix=%s sats=%lu lat=%.7f lon=%.7f alt=%.2f hdop=%.2f\n",
        s.gps.fix ? "YES" : "NO",
        static_cast<unsigned long>(s.gps.satellites),
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
            s.battery.low ? "YES" : "NO",
            s.battery.critical ? "YES" : "NO",
            s.battery.solarPresent ? "YES" : "NO",
            s.battery.charging ? "YES" : "NO"
        );

    } else {

        Serial.println("BATT   INVALID / NOT IMPLEMENTED");
    }

    // -------------------------------------------------------------------------
    // Local payload verification
    // -------------------------------------------------------------------------

    const std::vector<uint8_t> payload =
        PayloadBuilder::buildBinary(s);

    Serial.print("PAYLOAD v4 HEX=");
    Serial.println(PayloadBuilder::toHex(payload));

#if ENABLE_LORAWAN
    Serial.println("LORA   ENABLED");
#else
    Serial.println("LORA   DISABLED");
#endif

    Serial.println("============================");
    Serial.println();

    g_lastLoggedPulseCount = s.water.pulseCount;
    g_lastLoggedLiters = s.water.liters;
}


// -----------------------------------------------------------------------------
// Serial command processing
// -----------------------------------------------------------------------------
//
// Non-blocking: important because the water meter is polled every 5 ms.
//

static void processCommand(const String& command)
{
    if (command == "i2c scan") {

        scanI2CBus();
        return;
    }

    if (command == "status") {

        printSnapshot(buildSnapshot());
        return;
    }

    if (command == "reboot") {

        Serial.println("Rebooting...");
        delay(100);
        ESP.restart();
        return;
    }

    if (command == "help") {

        Serial.println("Commands:");
        Serial.println("  status");
        Serial.println("  i2c scan");
        Serial.println("  reboot");

#if ENABLE_LORAWAN
        Serial.println("  LoRaWAN commands enabled");
#endif

        return;
    }

    Serial.println("Unknown command");
}


static void handleSerial()
{
    static String line;

    while (Serial.available() > 0) {

        const char c =
            static_cast<char>(Serial.read());

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {

            line.trim();

            if (!line.isEmpty()) {
                processCommand(line);
            }

            line = "";
            continue;
        }

        /*
         * Bound input length so malformed serial traffic
         * cannot grow String indefinitely.
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
    Serial.begin(115200);

    delay(1500);

    Serial.println();
    Serial.println("ANDROMEDA @ ZESPRI");
    Serial.println("Booting...");

#if ENABLE_LORAWAN
    Serial.println("BUILD : LORAWAN ENABLED");
#else
    Serial.println("BUILD : LOCAL TELEMETRY ONLY");
#endif

    // -------------------------------------------------------------------------
    // I2C
    // -------------------------------------------------------------------------

    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);

    Serial.printf(
        "I2C lines before init: SDA=%d SCL=%d\n",
        digitalRead(PIN_I2C_SDA),
        digitalRead(PIN_I2C_SCL)
    );

    Wire.begin(
        PIN_I2C_SDA,
        PIN_I2C_SCL
    );

    Wire.setClock(I2C_FREQUENCY_HZ);
    Wire.setTimeOut(I2C_TIMEOUT_MS);

    Serial.printf(
        "I2C lines after init : SDA=%d SCL=%d\n",
        digitalRead(PIN_I2C_SDA),
        digitalRead(PIN_I2C_SCL)
    );

    // -------------------------------------------------------------------------
    // Water meter
    // -------------------------------------------------------------------------

    g_waterInterfaceAvailable =
        g_waterCounter.begin(
            Wire,
            I2C_ADDR_PCF8574,
            PCF8574_WATER_INPUT_BIT
        );

    Serial.printf(
        "WATER IF : %s\n",
        g_waterInterfaceAvailable ? "OK" : "FAIL"
    );

    // -------------------------------------------------------------------------
    // Environment sensors
    // -------------------------------------------------------------------------

    const bool envAvailable =
        g_env.begin(Wire);

    /*
     * Perform an immediate real acquisition.
     * Do not wait SENSOR_SAMPLE_PERIOD_MS after boot.
     */
    g_env.update();

    const EnvData env =
        g_env.getData();

    Serial.printf(
        "SHT   : %s\n",
        env.sht30Present ? "OK" : "FAIL"
    );

    Serial.printf(
        "MOIST : %s\n",
        env.moisturePresent ? "OK" : "FAIL"
    );

    Serial.printf(
        "ENV   : %s\n",
        envAvailable ? "AVAILABLE" : "FAIL"
    );

#if ENABLE_GPS

    g_gps.begin(
        GpsSerial,
        GPS_BAUDRATE,
        PIN_GPS_RX,
        PIN_GPS_TX
    );

    Serial.println("GPS   : INITIALIZED");

#endif

#if ENABLE_LORAWAN

    /*
     * LoRaWAN initialization will be restored here.
     * For the current baseline ENABLE_LORAWAN == 0,
     * therefore no radio initialization or transmission occurs.
     */

#endif

    // Initial timestamps.
    const uint32_t now = millis();

    g_lastEnvSampleMs = now;
    g_lastStatusMs = now;

    Serial.println();
    Serial.println("System ready");
    Serial.println("Type 'help' for commands.");
    Serial.println();

    /*
     * First status immediately after startup.
     */
    printSnapshot(buildSnapshot());
}


// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop()
{
    const uint32_t nowMs = millis();

    // Never block the main acquisition loop.
    handleSerial();

#if ENABLE_GPS
    g_gps.update();
#endif

    // -------------------------------------------------------------------------
    // Water meter
    // -------------------------------------------------------------------------

    if (g_waterInterfaceAvailable &&
        (nowMs - g_lastWaterPollMs >= WATER_POLL_PERIOD_MS)) {

        g_lastWaterPollMs = nowMs;
        g_waterCounter.update(nowMs);
    }
    // -------------------------------------------------------------------------
    // Environment
    // -------------------------------------------------------------------------

    if (nowMs - g_lastEnvSampleMs >= SENSOR_SAMPLE_PERIOD_MS) {

        g_lastEnvSampleMs = nowMs;
        g_env.update();
    }

    // -------------------------------------------------------------------------
    // Local status
    // -------------------------------------------------------------------------

    if (nowMs - g_lastStatusMs >= STATUS_LOG_PERIOD_MS) {

        g_lastStatusMs = nowMs;

        const DeviceSnapshot snap =
            buildSnapshot();

        printSnapshot(snap);
    }

#if ENABLE_LORAWAN

    /*
     * LoRaWAN periodic transmission will live here.
     *
     * Disabled completely in the current baseline.
     */

#endif
}
