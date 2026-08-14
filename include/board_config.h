#pragma once

#include <Arduino.h>

/*
 * ANDROMEDA - hardware / runtime configuration
 */

// -----------------------------------------------------------------------------
// Feature switches
// -----------------------------------------------------------------------------

// Compile-time feature switches.
// These MUST be macros because they are used with #if.
#define ENABLE_GPS       0
#define ENABLE_LORAWAN   0

// Development / simulation only.
static constexpr bool ENABLE_EXTERNAL_MOISTURE_STUB = false;

// -----------------------------------------------------------------------------
// I2C
// -----------------------------------------------------------------------------

static constexpr uint8_t PIN_I2C_SDA = 21;
static constexpr uint8_t PIN_I2C_SCL = 22;

static constexpr uint32_t I2C_FREQUENCY_HZ = 100000;
static constexpr uint32_t I2C_TIMEOUT_MS   = 50;

// Known devices
static constexpr uint8_t I2C_ADDR_SHT30   = 0x44;
static constexpr uint8_t I2C_ADDR_PCF8574 = 0x20;

// PCF8574 / water meter
static constexpr uint8_t PCF8574_WATER_INPUT_BIT = 0;

// -----------------------------------------------------------------------------
// GPS
// -----------------------------------------------------------------------------

static constexpr uint8_t PIN_GPS_RX = 26;
static constexpr uint8_t PIN_GPS_TX = 27;
static constexpr uint32_t GPS_BAUDRATE = 9600;

// -----------------------------------------------------------------------------
// SX1262 / E22-900M30S
// -----------------------------------------------------------------------------

static constexpr uint8_t PIN_LORA_NSS  = 5;
static constexpr uint8_t PIN_LORA_SCK  = 18;
static constexpr uint8_t PIN_LORA_MISO = 19;
static constexpr uint8_t PIN_LORA_MOSI = 23;

static constexpr uint8_t PIN_LORA_RST  = 14;
static constexpr uint8_t PIN_LORA_BUSY = 33;
static constexpr uint8_t PIN_LORA_DIO1 = 25;

// Not currently usable / validated.
static constexpr int8_t PIN_LORA_RXEN = -1;

// -----------------------------------------------------------------------------
// Water meter
// -----------------------------------------------------------------------------

static constexpr float WATER_METER_PULSES_PER_LITER = 87.0f;
static constexpr uint32_t WATER_METER_DEBOUNCE_MS = 20U;

// -----------------------------------------------------------------------------
// Scheduling
// -----------------------------------------------------------------------------

static constexpr uint32_t SENSOR_SAMPLE_PERIOD_MS = 60000UL;
static constexpr uint32_t WATER_POLL_PERIOD_MS    = 5UL;
static constexpr uint32_t STATUS_LOG_PERIOD_MS    = 60000UL;

// -----------------------------------------------------------------------------
// LoRa radio parameters
// -----------------------------------------------------------------------------

static constexpr float LORA_FREQUENCY_MHZ = 868.0f;
static constexpr float LORA_BANDWIDTH_KHZ = 125.0f;
static constexpr uint8_t LORA_SPREADING_FACTOR = 7;
static constexpr uint8_t LORA_CODING_RATE = 5;
static constexpr uint8_t LORA_SYNC_WORD = 0x12;
static constexpr int8_t LORA_TX_POWER_DBM = 14;

// -----------------------------------------------------------------------------
// Battery monitor
// -----------------------------------------------------------------------------

static constexpr uint8_t PIN_BATTERY_ADC = 35;
static constexpr float BATTERY_DIVIDER_RATIO = 2.0f;

static constexpr uint16_t BATTERY_LOW_MV      = 3600;
static constexpr uint16_t BATTERY_CRITICAL_MV = 3400;
