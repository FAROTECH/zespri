/*
 * ANDROMEDA / ZESPRI
 * Datacake uplink decoder
 *
 * Payload version: v4
 *
 * Calibration policy:
 * - pulse_count and moisture_raw are the primary/raw values.
 * - water_liters, moisture_pct and moisture_state are derived here
 *   so field calibration can be changed without reflashing firmware.
 *
 * Current delivered hardware:
 * - battery telemetry is intentionally not exported to Datacake;
 * - solar/charging status is intentionally not exported to Datacake.
 *
 * Those fields remain reserved in Payload v4 and are decoded by TTS,
 * so they can be enabled on future hardware without changing the
 * LoRaWAN payload format.
 */


/* --------------------------------------------------------------------------
 * Field calibration
 * -------------------------------------------------------------------------- */

var WATER_PULSES_PER_LITER = 87.0;

var MOISTURE_DRY_RAW = 395;
var MOISTURE_WET_RAW = 1015;

var MOISTURE_DRY_THRESHOLD_PCT = 20;
var MOISTURE_WET_THRESHOLD_PCT = 60;


/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

function clamp(value, minValue, maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}


function calculateMoisturePct(raw) {
    if (raw === null) {
        return null;
    }

    if (MOISTURE_WET_RAW === MOISTURE_DRY_RAW) {
        return null;
    }

    var pct =
        (raw - MOISTURE_DRY_RAW) *
        100.0 /
        (MOISTURE_WET_RAW - MOISTURE_DRY_RAW);

    return clamp(pct, 0.0, 100.0);
}


function calculateMoistureState(pct) {
    if (pct === null) {
        return "INVALID";
    }

    if (pct < MOISTURE_DRY_THRESHOLD_PCT) {
        return "DRY";
    }

    if (pct < MOISTURE_WET_THRESHOLD_PCT) {
        return "MOIST";
    }

    return "WET";
}


/* --------------------------------------------------------------------------
 * Decoder
 * -------------------------------------------------------------------------- */

function Decoder(bytes, port) {
    var i = 0;

    /*
     * Payload v4 is 18 bytes.
     */
    if (!bytes || bytes.length < 18) {
        return [];
    }


    /* ----------------------------------------------------------------------
     * Header
     * ---------------------------------------------------------------------- */

    var version = bytes[i++];
    var flags = bytes[i++];

    var uptimeMin =
        (bytes[i++] * 256) +
        bytes[i++];


    /* ----------------------------------------------------------------------
     * Water counter
     * ---------------------------------------------------------------------- */

    var pulseCount =
        (bytes[i++] * 16777216) +
        (bytes[i++] * 65536) +
        (bytes[i++] * 256) +
        bytes[i++];

    var waterLiters =
        pulseCount / WATER_PULSES_PER_LITER;


    /* ----------------------------------------------------------------------
     * Temperature
     * ---------------------------------------------------------------------- */

    var tempRaw =
        (bytes[i++] * 256) +
        bytes[i++];

    if (tempRaw >= 0x8000) {
        tempRaw -= 0x10000;
    }


    /* ----------------------------------------------------------------------
     * Humidity
     * ---------------------------------------------------------------------- */

    var humRaw =
        (bytes[i++] * 256) +
        bytes[i++];


    /* ----------------------------------------------------------------------
     * Moisture
     *
     * Payload v4 also contains firmware-derived moisture_pct and
     * moisture_state. They are consumed to preserve the payload layout,
     * but downstream values are recalculated from moisture_raw.
     * ---------------------------------------------------------------------- */

    var moistureRawValue =
        (bytes[i++] * 256) +
        bytes[i++];

    var moistureRaw =
        (moistureRawValue === 0xFFFF)
            ? null
            : moistureRawValue;

    /*
     * Consume firmware-derived moisture fields.
     */
    i++; // firmware moisture_pct
    i++; // firmware moisture_state

    var moisturePct =
        calculateMoisturePct(moistureRaw);

    var moistureState =
        calculateMoistureState(moisturePct);


    /* ----------------------------------------------------------------------
     * Battery
     *
     * battery_mv remains part of Payload v4 but is intentionally not
     * exported to Datacake for the current delivered hardware.
     * ---------------------------------------------------------------------- */

    i += 2;


    /* ----------------------------------------------------------------------
     * Datacake output
     * ---------------------------------------------------------------------- */

    var out = [];

    out.push({
        field: "VERSION",
        value: version
    });

    out.push({
        field: "UPTIME_MIN",
        value: uptimeMin
    });

    out.push({
        field: "PULSE_COUNT",
        value: pulseCount
    });

    out.push({
        field: "WATER_LITERS",
        value: waterLiters
    });


    if (tempRaw !== -32768) {
        out.push({
            field: "TEMPERATURE_C",
            value: tempRaw / 100.0
        });
    }


    if (humRaw !== 0xFFFF) {
        out.push({
            field: "HUMIDITY_RH",
            value: humRaw / 100.0
        });
    }


    if (moistureRaw !== null) {
        out.push({
            field: "MOISTURE_RAW",
            value: moistureRaw
        });

        out.push({
            field: "MOISTURE_PCT",
            value: moisturePct
        });

        out.push({
            field: "MOISTURE_STATE",
            value: moistureState
        });
    }


    /* ----------------------------------------------------------------------
     * Sensor / interface flags relevant to the current hardware
     * ---------------------------------------------------------------------- */

    out.push({
        field: "FLAG_SHT30_PRESENT",
        value: !!(flags & (1 << 0))
    });

    out.push({
        field: "FLAG_MOISTURE_PRESENT",
        value: !!(flags & (1 << 1))
    });

    out.push({
        field: "FLAG_WATER_LINE_STATE",
        value: !!(flags & (1 << 2))
    });


    /*
     * Payload v4 flags intentionally not exported to Datacake
     * on the current delivered hardware:
     *
     * bit 3 - battery_valid
     * bit 4 - battery_low
     * bit 5 - battery_critical
     * bit 6 - solar_present
     * bit 7 - charging
     */

    return out;
}
