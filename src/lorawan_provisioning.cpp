#include "lorawan_provisioning.h"
#include "lorawan_config.h"
#include <Preferences.h>
#include <string.h>
#include <ctype.h>

static constexpr const char* NVS_NS = "lwprov";
static constexpr const char* NVS_KEY = "cfg";

struct PackedLorawanProvisioning {
    uint32_t magic;
    uint32_t version;
    uint8_t region;
    uint8_t joinEui[8];
    uint8_t devEui[8];
    uint8_t appKey[16];
    uint32_t uplinkPeriodMs;
    uint8_t uplinkFPort;
    uint8_t uplinkConfirmed;
    uint8_t txEnable;
    uint8_t valid;
    uint8_t reserved[10];
};

static constexpr uint32_t PROV_MAGIC = 0x4C575031; // "LWP1"

static void copyToRuntime(const PackedLorawanProvisioning& src, LorawanProvisioning& dst) {
    dst.valid = src.valid != 0;
    dst.version = src.version;
    dst.region = (LorawanRegion)src.region;
    memcpy(dst.joinEui, src.joinEui, sizeof(dst.joinEui));
    memcpy(dst.devEui, src.devEui, sizeof(dst.devEui));
    memcpy(dst.appKey, src.appKey, sizeof(dst.appKey));
    dst.uplinkPeriodMs = src.uplinkPeriodMs;
    dst.uplinkFPort = src.uplinkFPort;
    dst.uplinkConfirmed = src.uplinkConfirmed != 0;
    dst.txEnable = src.txEnable != 0;
}

static void copyToPacked(const LorawanProvisioning& src, PackedLorawanProvisioning& dst) {
    memset(&dst, 0, sizeof(dst));
    dst.magic = PROV_MAGIC;
    dst.version = src.version;
    dst.region = (uint8_t)src.region;
    memcpy(dst.joinEui, src.joinEui, sizeof(dst.joinEui));
    memcpy(dst.devEui, src.devEui, sizeof(dst.devEui));
    memcpy(dst.appKey, src.appKey, sizeof(dst.appKey));
    dst.uplinkPeriodMs = src.uplinkPeriodMs;
    dst.uplinkFPort = src.uplinkFPort;
    dst.uplinkConfirmed = src.uplinkConfirmed ? 1 : 0;
    dst.txEnable = src.txEnable ? 1 : 0;
    dst.valid = src.valid ? 1 : 0;
}

LorawanProvisioning LorawanProvisioningStore::makeFactoryDefault() {
    LorawanProvisioning cfg;
    cfg.valid = true;
    cfg.region = LorawanRegion::EU868;
    memcpy(cfg.joinEui, LORAWAN_JOIN_EUI, sizeof(cfg.joinEui));
    memcpy(cfg.devEui, LORAWAN_DEV_EUI, sizeof(cfg.devEui));
    memcpy(cfg.appKey, LORAWAN_APP_KEY, sizeof(cfg.appKey));
    cfg.uplinkPeriodMs = LORAWAN_UPLINK_PERIOD_MS;
    cfg.uplinkFPort = LORAWAN_UPLINK_FPORT;
    cfg.uplinkConfirmed = LORAWAN_UPLINK_CONFIRMED;
    cfg.txEnable = LORAWAN_TX_ENABLE;
    cfg.version = 1;
    return cfg;
}

bool LorawanProvisioningStore::isSane(const LorawanProvisioning& cfg) {
    if (!cfg.valid) return false;
    if (cfg.uplinkPeriodMs < 10000UL) return false;
    if (cfg.uplinkFPort == 0) return false;
    if (cfg.region != LorawanRegion::EU868) return false;
    return true;
}

bool LorawanProvisioningStore::load(LorawanProvisioning& cfg) {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) {
        return false;
    }

    PackedLorawanProvisioning raw{};
    size_t n = prefs.getBytes(NVS_KEY, &raw, sizeof(raw));
    prefs.end();

    if (n != sizeof(raw)) {
        return false;
    }

    if (raw.magic != PROV_MAGIC) {
        return false;
    }

    copyToRuntime(raw, cfg);
    return isSane(cfg);
}

bool LorawanProvisioningStore::save(const LorawanProvisioning& cfg) {
    if (!isSane(cfg)) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return false;
    }

    PackedLorawanProvisioning raw{};
    copyToPacked(cfg, raw);

    size_t n = prefs.putBytes(NVS_KEY, &raw, sizeof(raw));
    prefs.end();

    return n == sizeof(raw);
}

bool LorawanProvisioningStore::clear() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) {
        return false;
    }

    bool ok = prefs.remove(NVS_KEY);
    prefs.end();
    return ok;
}

String LorawanProvisioningStore::toHex(const uint8_t* data, size_t len) {
    String out;
    out.reserve(len * 2);
    char buf[3];
    for (size_t i = 0; i < len; i++) {
        snprintf(buf, sizeof(buf), "%02X", data[i]);
        out += buf;
    }
    return out;
}

bool LorawanProvisioningStore::parseHex(const String& hexIn, uint8_t* out, size_t outLen) {
    String hex = hexIn;
    hex.trim();

    if (hex.length() != (int)(outLen * 2)) {
        return false;
    }

    for (size_t i = 0; i < outLen; i++) {
        char c1 = hex[2 * i];
        char c2 = hex[2 * i + 1];
        if (!isxdigit((unsigned char)c1) || !isxdigit((unsigned char)c2)) {
            return false;
        }

        char tmp[3] = { c1, c2, 0 };
        out[i] = (uint8_t)strtoul(tmp, nullptr, 16);
    }

    return true;
}

void LorawanProvisioningStore::print(const LorawanProvisioning& cfg, Stream& out) {
    out.println("=== LORAWAN PROVISIONING ===");
    out.println(String("valid: ") + (cfg.valid ? "YES" : "NO"));
    out.println(String("region: ") + (cfg.region == LorawanRegion::EU868 ? "EU868" : "UNKNOWN"));
    out.println(String("joinEUI: ") + toHex(cfg.joinEui, sizeof(cfg.joinEui)));
    out.println(String("devEUI : ") + toHex(cfg.devEui, sizeof(cfg.devEui)));
    out.println(String("appKey : ") + toHex(cfg.appKey, sizeof(cfg.appKey)));
    out.println(String("period : ") + cfg.uplinkPeriodMs);
    out.println(String("fport  : ") + cfg.uplinkFPort);
    out.println(String("confirm: ") + (cfg.uplinkConfirmed ? "YES" : "NO"));
    out.println(String("tx     : ") + (cfg.txEnable ? "YES" : "NO"));
    out.println("============================");
}