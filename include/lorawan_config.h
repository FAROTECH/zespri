#pragma once
#include <Arduino.h>

// Factory defaults used at first boot or after "lw reset".
// Runtime provisioning is loaded from NVS and overrides these values.

// TTN / TTS - OTAA - EU868
static const uint8_t LORAWAN_JOIN_EUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t LORAWAN_DEV_EUI[8] = {
    0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x07, 0x70, 0x83
};

static const uint8_t LORAWAN_APP_KEY[16] = {
    0x93, 0x1B, 0xAC, 0x92, 0x7B, 0xF8, 0x6F, 0xF7,
    0x74, 0x90, 0xBF, 0x74, 0x3F, 0xB1, 0xBD, 0xA9
};

// Primo test: LoRaWAN 1.0.x / Class A / uplink prudente
static constexpr uint32_t LORAWAN_UPLINK_PERIOD_MS = 300000UL; // 5 minuti
static constexpr uint8_t  LORAWAN_UPLINK_FPORT = 1;
static constexpr bool     LORAWAN_UPLINK_CONFIRMED = false;

// Enable/disable LoRaWAN TX (useful during development)
static constexpr bool LORAWAN_TX_ENABLE = true;