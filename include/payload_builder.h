#pragma once
#include <Arduino.h>
#include <vector>
#include "app_types.h"

class PayloadBuilder {
public:
    static std::vector<uint8_t> buildBinary(const DeviceSnapshot& snap);
    static String toHex(const std::vector<uint8_t>& payload);

private:
    static void pushU8(std::vector<uint8_t>& out, uint8_t v);
    static void pushU16(std::vector<uint8_t>& out, uint16_t v);
    static void pushU32(std::vector<uint8_t>& out, uint32_t v);
    static void pushI32(std::vector<uint8_t>& out, int32_t v);
};
