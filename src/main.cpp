#include <Arduino.h>
#include <Wire.h>
#include <vector>

#include "board_config.h"
#include "app_types.h"
#include "pulse_counter.h"
#include "env_service.h"
#include "payload_builder.h"
#include "lora_service.h"
#include "lorawan_config.h"

#if ENABLE_GPS
#include "gps_service.h"
static HardwareSerial GpsSerial(1);
static GpsService g_gps;
#endif

static PulseCounterPcf8574 g_waterCounter;
static EnvService g_env;
static LoraService g_lora;

static bool g_pcfPresent = false;
static uint32_t g_lastWaterPollMs = 0;
static uint32_t g_lastEnvSampleMs = 0;
static uint32_t g_lastTxMs = 0;
static uint32_t g_lastLogMs = 0;

// Modalità test
static constexpr bool ZENNER_TEST_MODE = false;
static constexpr bool MOISTURE_TEST_MODE = false;

// Stato storico per log ZENNER
static uint32_t g_lastLoggedPulseCount = 0;
static float g_lastLoggedLiters = 0.0f;

// -----------------------------
// Moisture test mode
// -----------------------------
enum class MoisturePhase : uint8_t {
    AIR = 0,
    DRY = 1,
    WET = 2,
    WATER = 3
};

static MoisturePhase g_moisturePhase = MoisturePhase::AIR;
static bool g_moisturePaused = false;

// Finestra statistica per log CSV
static uint32_t g_moistureAccum = 0;
static uint16_t g_moistureSamples = 0;
static uint16_t g_moistureMin = 0xFFFF;
static uint16_t g_moistureMax = 0;
static uint32_t g_lastMoistureCsvMs = 0;

static const char* moisturePhaseToString(MoisturePhase phase) {
    switch (phase) {
        case MoisturePhase::AIR:   return "AIR";
        case MoisturePhase::DRY:   return "DRY";
        case MoisturePhase::WET:   return "WET";
        case MoisturePhase::WATER: return "WATER";
        default:                   return "UNKNOWN";
    }
}

static void resetMoistureWindow() {
    g_moistureAccum = 0;
    g_moistureSamples = 0;
    g_moistureMin = 0xFFFF;
    g_moistureMax = 0;
}

static void setMoisturePhase(MoisturePhase phase) {
    g_moisturePhase = phase;
    resetMoistureWindow();

    Serial.print("# PHASE=");
    Serial.println(moisturePhaseToString(g_moisturePhase));
}

static void printMoistureTestHelp() {
    Serial.println("# Moisture test commands:");
    Serial.println("#   a -> phase AIR");
    Serial.println("#   d -> phase DRY");
    Serial.println("#   w -> phase WET");
    Serial.println("#   x -> phase WATER");
    Serial.println("#   p -> pause logging");
    Serial.println("#   s -> resume logging");
    Serial.println("#   r -> reset current averaging window");
    Serial.println("#   h -> help");
    Serial.println("# CSV columns:");
    Serial.println("# ts_ms,phase,raw,avg,min,max,samples");
}

static void handleMoistureTestSerial() {
    while (Serial.available() > 0) {
        const char c = (char)Serial.read();

        if (c == '\n' || c == '\r') {
            continue;
        }

        switch (c) {
            case 'a':
            case 'A':
                setMoisturePhase(MoisturePhase::AIR);
                break;

            case 'd':
            case 'D':
                setMoisturePhase(MoisturePhase::DRY);
                break;

            case 'w':
            case 'W':
                setMoisturePhase(MoisturePhase::WET);
                break;

            case 'x':
            case 'X':
                setMoisturePhase(MoisturePhase::WATER);
                break;

            case 'p':
            case 'P':
                g_moisturePaused = true;
                Serial.println("# PAUSED");
                break;

            case 's':
            case 'S':
                g_moisturePaused = false;
                resetMoistureWindow();
                Serial.println("# RESUMED");
                break;

            case 'r':
            case 'R':
                resetMoistureWindow();
                Serial.println("# WINDOW RESET");
                break;

            case 'h':
            case 'H':
            case '?':
                printMoistureTestHelp();
                break;

            default:
                Serial.print("# UNKNOWN CMD=");
                Serial.println(c);
                break;
        }
    }
}

static void scanI2CBus() {
    Serial.println();
    Serial.println("I2C scan start");

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission(true);

        if (err == 0) {
            Serial.print(" - found device at 0x");
            if (addr < 16) Serial.print('0');
            Serial.println(addr, HEX);
        } else if (err == 4) {
            Serial.print(" - unknown error at 0x");
            if (addr < 16) Serial.print('0');
            Serial.println(addr, HEX);
        }
    }

    Serial.println("I2C scan end");
    Serial.println();
}

static DeviceSnapshot buildSnapshot() {
    DeviceSnapshot snap;
    snap.uptimeMs = millis();
    snap.water = g_waterCounter.getData();
    snap.env = g_env.getData();

#if ENABLE_GPS
    snap.gps = g_gps.getData();
#else
    snap.gps = GpsData{};
#endif

    return snap;
}

static void printSnapshot(const DeviceSnapshot& s) {
    Serial.println("===== ANDROMEDA STATUS =====");

    Serial.println("Uptime: " + String(s.uptimeMs) + " ms");

    Serial.println(
        "WATER  pulses=" + String(s.water.pulseCount) +
        " liters=" + String(s.water.liters, 3) +
        " line=" + String(s.water.lineState ? "HIGH" : "LOW") +
        " lastPulseMs=" + String(s.water.lastPulseMs)
    );

    const uint32_t pulseDelta = s.water.pulseCount - g_lastLoggedPulseCount;
    const float literDelta = s.water.liters - g_lastLoggedLiters;

    Serial.println(
        "WATERD pulses=" + String(pulseDelta) +
        " liters=" + String(literDelta, 3)
    );

    Serial.println(
        "SHT30  present=" + String(s.env.sht30Present ? "YES" : "NO") +
        " temp=" + String(s.env.temperatureC, 2) +
        " C hum=" + String(s.env.humidityRH, 2) + " %RH"
    );

    Serial.println(
        "MOIST  present=" + String(s.env.moisturePresent ? "YES" : "NO") +
        " raw=" + String(s.env.moistureRaw)
    );

#if ENABLE_GPS
    Serial.println(
        "GPS    fix=" + String(s.gps.fix ? "YES" : "NO") +
        " sats=" + String(s.gps.satellites) +
        " lat=" + String(s.gps.latitude, 7) +
        " lon=" + String(s.gps.longitude, 7) +
        " alt=" + String(s.gps.altitudeMeters, 2) +
        " hdop=" + String(s.gps.hdop, 2) +
        " utc=" + String(s.gps.utc[0] ? s.gps.utc : "N/A") +
        " age=" + String(s.gps.ageMs) + " ms"
    );

    if (g_gps.getLastByteMs() == 0) {
        Serial.println("GPSDBG rxBytes=" + String(g_gps.getRxBytes()) + " lastByteAgo=NEVER");
    } else {
        Serial.println(
            "GPSDBG rxBytes=" + String(g_gps.getRxBytes()) +
            " lastByteAgo=" + String(millis() - g_gps.getLastByteMs()) + " ms"
        );
    }
#endif

    Serial.println("============================");

    g_lastLoggedPulseCount = s.water.pulseCount;
    g_lastLoggedLiters = s.water.liters;
}

void setup() {
    Serial.begin(115200);
    delay(10000);

    Serial.println();
    Serial.println("ANDROMEDA @ ZESPRI");
    Serial.println("Booting...");

    if (ZENNER_TEST_MODE) {
        Serial.println("MODE: ZENNER TEST");
        Serial.println("Radio TX temporarily disabled");
    }

    if (MOISTURE_TEST_MODE) {
        Serial.println("MODE: MOISTURE TEST");
        Serial.println("Normal telemetry and radio TX temporarily disabled");
    }

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(50);

    scanI2CBus();

    g_pcfPresent = g_waterCounter.begin(Wire, I2C_ADDR_PCF8574, PCF8574_WATER_INPUT_BIT);
    Serial.printf("PCF8574 water counter: %s (addr 0x%02X)\n",
                  g_pcfPresent ? "OK" : "NOT FOUND",
                  I2C_ADDR_PCF8574);

    g_env.begin(Wire);
    Serial.println("Env service initialized");

#if ENABLE_GPS
    g_gps.begin(GpsSerial, GPS_BAUDRATE, PIN_GPS_RX, PIN_GPS_TX);
    Serial.println("GPS UART initialized");
#endif

    if (!ZENNER_TEST_MODE && !MOISTURE_TEST_MODE) {
        bool loraOk = g_lora.begin();
        Serial.printf(
            "LoRaWAN stack: %s | err=%d\n",
            loraOk ? "OK" : "INIT FAILED",
            g_lora.getLastError()
        );

        if (loraOk) {
            bool joinOk = g_lora.join();

            Serial.printf(
                "LoRaWAN join: %s\n",
                joinOk ? "OK" : "FAIL"
            );

            if (!joinOk) {
                Serial.printf(
                    "LoRaWAN join error: %d\n",
                    g_lora.getLastError()
                );
            }
        }
    }

    if (MOISTURE_TEST_MODE) {
        printMoistureTestHelp();
        Serial.println("# ts_ms,phase,raw,avg,min,max,samples");
        setMoisturePhase(MoisturePhase::AIR);
        g_moisturePaused = false;
    }

    Serial.println();
}

void loop() {
    const uint32_t nowMs = millis();

#if ENABLE_GPS
    g_gps.update();
#endif

    if (g_pcfPresent && (nowMs - g_lastWaterPollMs >= WATER_POLL_PERIOD_MS)) {
        g_lastWaterPollMs = nowMs;
        g_waterCounter.update(nowMs);
    }

    if (nowMs - g_lastEnvSampleMs >= SENSOR_SAMPLE_PERIOD_MS) {
        g_lastEnvSampleMs = nowMs;
        g_env.update();
    }

    if (MOISTURE_TEST_MODE) {
        handleMoistureTestSerial();

        const EnvData env = g_env.getData();

        if (!g_moisturePaused && env.moisturePresent) {
            const uint16_t raw = env.moistureRaw;

            g_moistureAccum += raw;
            g_moistureSamples++;

            if (raw < g_moistureMin) {
                g_moistureMin = raw;
            }

            if (raw > g_moistureMax) {
                g_moistureMax = raw;
            }
        }

        static constexpr uint32_t MOISTURE_CSV_PERIOD_MS = 1000U;
        static constexpr uint16_t MOISTURE_MIN_SAMPLES_PER_ROW = 5U;

        if (!g_moisturePaused &&
            (nowMs - g_lastMoistureCsvMs >= MOISTURE_CSV_PERIOD_MS) &&
            (g_moistureSamples >= MOISTURE_MIN_SAMPLES_PER_ROW)) {

            g_lastMoistureCsvMs = nowMs;

            const float avg = (float)g_moistureAccum / (float)g_moistureSamples;

            Serial.print(nowMs);
            Serial.print(",");
            Serial.print(moisturePhaseToString(g_moisturePhase));
            Serial.print(",");
            Serial.print(env.moistureRaw);
            Serial.print(",");
            Serial.print(avg, 2);
            Serial.print(",");
            Serial.print(g_moistureMin);
            Serial.print(",");
            Serial.print(g_moistureMax);
            Serial.print(",");
            Serial.println(g_moistureSamples);

            resetMoistureWindow();
        }

        return;
    }

    if (nowMs - g_lastLogMs >= STATUS_LOG_PERIOD_MS) {
        g_lastLogMs = nowMs;
        DeviceSnapshot snap = buildSnapshot();
        printSnapshot(snap);

        std::vector<uint8_t> payload = PayloadBuilder::buildBinary(snap);
        Serial.print("Payload HEX: ");
        Serial.println(PayloadBuilder::toHex(payload));
    }

    if (!ZENNER_TEST_MODE && !MOISTURE_TEST_MODE && (nowMs - g_lastTxMs >= LORAWAN_UPLINK_PERIOD_MS)) {
        g_lastTxMs = nowMs;

        DeviceSnapshot snap = buildSnapshot();
        std::vector<uint8_t> payload = PayloadBuilder::buildBinary(snap);

        Serial.print("[LORAWAN] real payload HEX: ");
        Serial.println(PayloadBuilder::toHex(payload));

        bool txOk = g_lora.sendUplink(
            payload.data(),
            payload.size(),
            LORAWAN_UPLINK_FPORT,
            LORAWAN_UPLINK_CONFIRMED
        );

        Serial.printf(
            "LoRaWAN uplink: %s | bytes=%u | err=%d\n",
            txOk ? "OK" : "FAIL",
            (unsigned)payload.size(),
            g_lora.getLastError()
        );
    }
}