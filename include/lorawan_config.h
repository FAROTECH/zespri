#pragma once
#include <Arduino.h>

// TTN / TTS - OTAA - EU868
static const uint8_t LORAWAN_JOIN_EUI[8] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t LORAWAN_DEV_EUI[8] = {
    0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x07, 0x70, 0x70
};

static const uint8_t LORAWAN_APP_KEY[16] = {
    0xEE, 0xBC, 0x6F, 0xC1, 0xE8, 0xBB, 0x0F, 0x3B,
    0xFA, 0x62, 0x68, 0x09, 0x6F, 0x19, 0xCA, 0x60
};

// Primo test: LoRaWAN 1.0.x / Class A / uplink prudente
static constexpr uint32_t LORAWAN_UPLINK_PERIOD_MS = 60000UL;
static constexpr uint8_t  LORAWAN_UPLINK_FPORT = 1;
static constexpr bool     LORAWAN_UPLINK_CONFIRMED = false;