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

    var out = {
        version: version,
        sht30_present: !!(flags & (1 << 0)),
        moisture_present: !!(flags & (1 << 1)),
        water_line_high: !!(flags & (1 << 2)),
        battery_valid: !!(flags & (1 << 3)),
        battery_low: !!(flags & (1 << 4)),
        battery_critical: !!(flags & (1 << 5)),
        solar_present: !!(flags & (1 << 6)),
        charging: !!(flags & (1 << 7)),

        uptime_min: uptimeMin,
        pulse_count: pulseCount,

        temperature_c: (tempRaw === -32768) ? null : tempRaw / 100.0,
        humidity_rh: (humRaw === 0xFFFF) ? null : humRaw / 100.0,

        moisture_raw: (moistureRaw === 0xFFFF) ? null : moistureRaw,
        moisture_pct: moisturePct,
        moisture_state: moistureState,
        moisture_state_label: moistureStateLabel,

        battery_v: (batteryMv === 0xFFFF) ? null : batteryMv / 1000.0
    };

    return out;
}