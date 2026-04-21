function Decoder(bytes, port) {
    var i = 0;

    var version = bytes[i++];
    var flags = bytes[i++];

    var uptimeMin = (bytes[i++] << 8) | bytes[i++];

    var pulseCount =
        (bytes[i++] << 24) |
        (bytes[i++] << 16) |
        (bytes[i++] << 8) |
        (bytes[i++]);

    var tempRaw = (bytes[i++] << 8) | bytes[i++];
    if (tempRaw & 0x8000) {
        tempRaw -= 0x10000;
    }

    var humRaw = (bytes[i++] << 8) | bytes[i++];
    var moistureRaw = (bytes[i++] << 8) | bytes[i++];

    var moisturePct = null;
    var moistureState = null;
    var moistureStateLabel = null;

    // parsing v4
    if (version >= 4) {
        moisturePct = bytes[i++];
        moistureState = bytes[i++];

        if (moisturePct === 0xFF) {
            moisturePct = null;
        }

        switch (moistureState) {
            case 0: moistureStateLabel = "INVALID"; break;
            case 1: moistureStateLabel = "DRY"; break;
            case 2: moistureStateLabel = "MOIST"; break;
            case 3: moistureStateLabel = "WET"; break;
            default: moistureStateLabel = "UNKNOWN"; break;
        }
    }

    var batteryMv = (bytes[i++] << 8) | bytes[i++];

    var out = [];

    out.push({ field: "VERSION", value: version });
    out.push({ field: "UPTIME_MIN", value: uptimeMin });
    out.push({ field: "PULSE_COUNT", value: pulseCount });

    if (tempRaw !== -32768) {
        out.push({ field: "TEMPERATURE_C", value: tempRaw / 100.0 });
    }

    if (humRaw !== 0xFFFF) {
        out.push({ field: "HUMIDITY_RH", value: humRaw / 100.0 });
    }

    if (moistureRaw !== 0xFFFF) {
        out.push({ field: "MOISTURE_RAW", value: moistureRaw });
    }

    if (moisturePct !== null) {
        out.push({ field: "MOISTURE_PCT", value: moisturePct });
    }

    if (moistureStateLabel !== null) {
        out.push({ field: "MOISTURE_STATE", value: moistureStateLabel });
    }

    if (batteryMv !== 0xFFFF) {
        out.push({ field: "BATTERY_V", value: batteryMv / 1000.0 });
    }

    // Flags
    out.push({ field: "FLAG_SHT30_PRESENT", value: !!(flags & (1 << 0)) });
    out.push({ field: "FLAG_MOISTURE_PRESENT", value: !!(flags & (1 << 1)) });
    out.push({ field: "FLAG_WATER_LINE_STATE", value: !!(flags & (1 << 2)) });
    out.push({ field: "FLAG_BATTERY_VALID", value: !!(flags & (1 << 3)) });
    out.push({ field: "FLAG_BATTERY_LOW", value: !!(flags & (1 << 4)) });
    out.push({ field: "FLAG_BATTERY_CRITICAL", value: !!(flags & (1 << 5)) });
    out.push({ field: "FLAG_SOLAR_PRESENT", value: !!(flags & (1 << 6)) });
    out.push({ field: "FLAG_CHARGING", value: !!(flags & (1 << 7)) });

    return out;
}