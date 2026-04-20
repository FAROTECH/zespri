#pragma once
#include <Arduino.h>

/*
 * ANDROMEDA - baseline pin map
 */

// I2C
static constexpr uint8_t PIN_I2C_SDA = 21;
static constexpr uint8_t PIN_I2C_SCL = 22;

// GPS UART
static constexpr uint8_t PIN_GPS_RX = 26;
static constexpr uint8_t PIN_GPS_TX = 27;
static constexpr uint32_t GPS_BAUDRATE = 9600;

// SX1262 / E22-900M30S
static constexpr uint8_t PIN_LORA_NSS  = 5;
static constexpr uint8_t PIN_LORA_SCK  = 18;
static constexpr uint8_t PIN_LORA_MISO = 19;
static constexpr uint8_t PIN_LORA_MOSI = 23;

static constexpr uint8_t PIN_LORA_RST  = 14;
static constexpr uint8_t PIN_LORA_BUSY = 33;
static constexpr uint8_t PIN_LORA_DIO1 = 25;

// NON affidabile al momento: dallo schema sembra passare su una linea
// che lato ESP32 non è chiaramente utilizzabile come output general purpose.
// Quindi per ora non va pilotato in firmware.
static constexpr int8_t PIN_LORA_RXEN  = -1;

// Sensori I2C noti
static constexpr uint8_t I2C_ADDR_SHT30 = 0x44;

// PCF8574 / HW-171 I/O expander
static constexpr uint8_t I2C_ADDR_PCF8574 = 0x20; // default tipico, verificare con scan
static constexpr uint8_t PCF8574_WATER_INPUT_BIT = 0; // P0

// Contatore ZENNER
static constexpr float WATER_METER_PULSES_PER_LITER = 87.0f;
static constexpr uint32_t WATER_METER_DEBOUNCE_MS = 20U;

// Scheduling
static constexpr uint32_t SENSOR_SAMPLE_PERIOD_MS   = 5000;
static constexpr uint32_t PAYLOAD_TX_PERIOD_MS      = 30000;
static constexpr uint32_t WATER_POLL_PERIOD_MS      = 5;
static constexpr uint32_t STATUS_LOG_PERIOD_MS      = 5000;

// LoRa raw baseline
// NOTA: ChirpStack richiede LoRaWAN, non LoRa raw.
// Questa baseline abilita la radio e l'invio raw per debug di link.
static constexpr float LORA_FREQUENCY_MHZ = 868.0f;
static constexpr float LORA_BANDWIDTH_KHZ = 125.0f;
static constexpr uint8_t LORA_SPREADING_FACTOR = 7;
static constexpr uint8_t LORA_CODING_RATE = 5;
static constexpr uint8_t LORA_SYNC_WORD = 0x12;
static constexpr int8_t  LORA_TX_POWER_DBM = 14;

static constexpr bool ENABLE_GPS = false;
static constexpr bool ENABLE_EXTERNAL_MOISTURE_STUB = false;

// Battery monitor
static constexpr uint8_t PIN_BATTERY_ADC = 35;     // V_CHECK
static constexpr float BATTERY_DIVIDER_RATIO = 2.0f; // R8=10k, R10=10k
static constexpr uint16_t BATTERY_LOW_MV = 3600;
static constexpr uint16_t BATTERY_CRITICAL_MV = 3400;
