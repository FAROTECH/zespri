#include "payload_builder.h"
#include <math.h>

void PayloadBuilder::pushU8(std::vector<uint8_t>& out, uint8_t v) {
    out.push_back(v);
}

void PayloadBuilder::pushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)(v & 0xFF));
}

void PayloadBuilder::pushU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)((v >> 24) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)(v & 0xFF));
}

void PayloadBuilder::pushI32(std::vector<uint8_t>& out, int32_t v) {
    pushU32(out, (uint32_t)v);
}

std::vector<uint8_t> PayloadBuilder::buildBinary(const DeviceSnapshot& snap) {
    std::vector<uint8_t> out;
    out.reserve(48);

    // Header
    pushU8(out, 0xA5);          // sync
    pushU8(out, 0x01);          // payload version

    // uptime
    pushU32(out, snap.uptimeMs);

    // water
    pushU32(out, snap.water.pulseCount);
    pushU32(out, (uint32_t)lroundf(snap.water.liters * 1000.0f));  // mL
    pushU8(out, snap.water.lineState ? 1 : 0);

    // env
    pushU8(out, snap.env.sht30Present ? 1 : 0);
    pushI32(out, isnan(snap.env.temperatureC) ? INT32_MIN : (int32_t)lroundf(snap.env.temperatureC * 100.0f));
    pushI32(out, isnan(snap.env.humidityRH) ? INT32_MIN : (int32_t)lroundf(snap.env.humidityRH * 100.0f));
    pushU8(out, snap.env.moisturePresent ? 1 : 0);
    pushU16(out, snap.env.moistureRaw);

    // gps
    pushU8(out, snap.gps.fix ? 1 : 0);
    pushU8(out, (uint8_t)min<uint32_t>(snap.gps.satellites, 255));
    pushI32(out, (int32_t)lround(snap.gps.latitude * 1e7));
    pushI32(out, (int32_t)lround(snap.gps.longitude * 1e7));
    pushI32(out, (int32_t)lround(snap.gps.altitudeMeters * 100.0));
    pushU16(out, (uint16_t)lround(snap.gps.hdop * 100.0));

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
