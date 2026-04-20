#pragma once
#include <Arduino.h>

enum class LorawanRegion : uint8_t {
    EU868 = 0
};

struct LorawanProvisioning {
    bool valid = false;
    LorawanRegion region = LorawanRegion::EU868;

    uint8_t joinEui[8] = {0};
    uint8_t devEui[8] = {0};
    uint8_t appKey[16] = {0};

    uint32_t uplinkPeriodMs = 60000UL;
    uint8_t uplinkFPort = 1;
    bool uplinkConfirmed = false;
    bool txEnable = false;

    uint32_t version = 1;
};

class LorawanProvisioningStore {
public:
    static bool load(LorawanProvisioning& cfg);
    static bool save(const LorawanProvisioning& cfg);
    static bool clear();
    static LorawanProvisioning makeFactoryDefault();
    static void print(const LorawanProvisioning& cfg, Stream& out);

    static bool parseHex(const String& hex, uint8_t* out, size_t outLen);
    static String toHex(const uint8_t* data, size_t len);

private:
    static bool isSane(const LorawanProvisioning& cfg);
};