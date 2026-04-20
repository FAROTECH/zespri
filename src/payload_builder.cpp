#include "payload_builder.h"
#include <math.h>
#include <limits.h>

void PayloadBuilder::pushU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void PayloadBuilder::pushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)(v & 0xFF));
}

void PayloadBuilder::pushI16(std::vector<uint8_t>& out, int16_t v) {
    pushU16(out, (uint16_t)v);
}

void PayloadBuilder::pushU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)((v >> 24) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)(v & 0xFF));
}

std::vector<uint8_t> PayloadBuilder::buildBinary(const DeviceSnapshot& snap) {
    std::vector<uint8_t> out;
    out.reserve(16);

    pushU8(out, 0x03);

    uint8_t flags = 0;
    if (snap.env.sht30Present)     flags |= (1 << 0);
    if (snap.env.moisturePresent)  flags |= (1 << 1);
    if (snap.water.lineState)      flags |= (1 << 2);
    if (snap.battery.valid)        flags |= (1 << 3);
    if (snap.battery.low)          flags |= (1 << 4);
    if (snap.battery.critical)     flags |= (1 << 5);
    if (snap.battery.solarPresent) flags |= (1 << 6);
    if (snap.battery.charging)     flags |= (1 << 7);
    pushU8(out, flags);

    uint32_t uptimeMin32 = snap.uptimeMs / 60000UL;
    uint16_t uptimeMin = (uptimeMin32 > 0xFFFFU) ? 0xFFFFU : (uint16_t)uptimeMin32;
    pushU16(out, uptimeMin);

    pushU32(out, snap.water.pulseCount);

    int16_t tempCenti = INT16_MIN;
    if (snap.env.sht30Present && !isnan(snap.env.temperatureC)) {
        long v = lroundf(snap.env.temperatureC * 100.0f);
        if (v < INT16_MIN) v = INT16_MIN;
        if (v > INT16_MAX) v = INT16_MAX;
        tempCenti = (int16_t)v;
    }
    pushI16(out, tempCenti);

    uint16_t humCenti = 0xFFFFU;
    if (snap.env.sht30Present && !isnan(snap.env.humidityRH)) {
        long v = lroundf(snap.env.humidityRH * 100.0f);
        if (v < 0) v = 0;
        if (v > 10000) v = 10000;
        humCenti = (uint16_t)v;
    }
    pushU16(out, humCenti);

    uint16_t moistureRaw = snap.env.moisturePresent ? snap.env.moistureRaw : 0xFFFFU;
    pushU16(out, moistureRaw);

    uint16_t batteryMv = snap.battery.valid ? snap.battery.millivolts : 0xFFFFU;
    pushU16(out, batteryMv);

    return out;
}

String PayloadBuilder::toHex(const std::vector<uint8_t>& payload) {
    String hex;
    hex.reserve(payload.size() * 2);
    char buf[3];
    for (uint8_t b : payload) {
        snprintf(buf, sizeof(buf), "%02X", b);
        hex += buf;
    }
    return hex;
}