#include <Arduino.h>
#include <Wire.h>
#include <vector>

#include "board_config.h"
#include "app_types.h"
#include "pulse_counter.h"
#include "env_service.h"
#include "payload_builder.h"
#include "lora_service.h"

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
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("ANDROMEDA FASE2 - baseline estesa");
    Serial.println("Booting...");

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

    bool loraOk = g_lora.begin();
    Serial.printf(
        "LoRa radio: %s | err=%d\n",
        loraOk ? "OK" : "INIT FAILED",
        g_lora.getLastError()
    );
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

    if (nowMs - g_lastLogMs >= STATUS_LOG_PERIOD_MS) {
        g_lastLogMs = nowMs;
        DeviceSnapshot snap = buildSnapshot();
        printSnapshot(snap);

        std::vector<uint8_t> payload = PayloadBuilder::buildBinary(snap);
        Serial.print("Payload HEX: ");
        Serial.println(PayloadBuilder::toHex(payload));
    }

    if (nowMs - g_lastTxMs >= PAYLOAD_TX_PERIOD_MS) {
        g_lastTxMs = nowMs;

        DeviceSnapshot snap = buildSnapshot();
        std::vector<uint8_t> payload = PayloadBuilder::buildBinary(snap);

        bool txOk = g_lora.send(payload.data(), payload.size());
        Serial.printf(
            "LoRa TX raw: %s | bytes=%u | err=%d\n",
            txOk ? "OK" : "FAIL",
            (unsigned)payload.size(),
            g_lora.getLastError()
        );
    }
}
