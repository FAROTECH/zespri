/*
 * ANDROMEDA / ZESPRI
 * The Things Stack uplink decoder
 *
 * Payload version: v4
 *
 * Calibration policy:
 * - pulse_count and moisture_raw are the primary/raw values
 *   transmitted by the device.
 * - water_liters, moisture_pct and moisture_state are derived
 *   here so field calibration can be changed without reflashing
 *   the firmware.
 */


/* --------------------------------------------------------------------------
 * Field calibration
 * -------------------------------------------------------------------------- */

var WATER_PULSES_PER_LITER = 87.0;

/*
 * Provisional moisture calibration.
 *
 * These values can be adjusted after field calibration without changing
 * the firmware.
 */
var MOISTURE_DRY_RAW = 395;
var MOISTURE_WET_RAW = 1015;

/*
 * Moisture state thresholds.
 */
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
        return {
            value: 0,
            label: "INVALID"
        };
    }

    if (pct < MOISTURE_DRY_THRESHOLD_PCT) {
        return {
            value: 1,
            label: "DRY"
        };
    }

    if (pct < MOISTURE_WET_THRESHOLD_PCT) {
        return {
            value: 2,
            label: "MOIST"
        };
    }

    return {
        value: 3,
        label: "WET"
    };
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
        return {
            error: "Invalid ANDROMEDA payload length"
        };
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
     *
     * Do not use JS bitwise operators here because they operate on signed
     * 32-bit integers. pulse_count is an unsigned 32-bit value.
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

    var temperatureC =
        (tempRaw === -32768)
            ? null
            : tempRaw / 100.0;


    /* ----------------------------------------------------------------------
     * Humidity
     * ---------------------------------------------------------------------- */

    var humRaw =
        (bytes[i++] * 256) +
        bytes[i++];

    var humidityRh =
        (humRaw === 0xFFFF)
            ? null
            : humRaw / 100.0;


    /* ----------------------------------------------------------------------
     * Moisture
     *
     * The firmware still transmits moisture_pct and moisture_state as part
     * of Payload v4. They are consumed here to preserve the payload layout,
     * but application values are recalculated from moisture_raw using the
     * calibration constants above.
     * ---------------------------------------------------------------------- */

    var moistureRawValue =
        (bytes[i++] * 256) +
        bytes[i++];

    var moistureRaw =
        (moistureRawValue === 0xFFFF)
            ? null
            : moistureRawValue;

    /*
     * Consume the firmware-derived fields.
     */
    var firmwareMoisturePct = bytes[i++];
    var firmwareMoistureState = bytes[i++];

    /*
     * Currently intentionally unused.
     * Kept in local variables because they remain part of Payload v4.
     */
    void firmwareMoisturePct;
    void firmwareMoistureState;

    var moisturePct =
        calculateMoisturePct(moistureRaw);

    var moistureState =
        calculateMoistureState(moisturePct);


    /* ----------------------------------------------------------------------
     * Battery
     * ---------------------------------------------------------------------- */

    var batteryMv =
        (bytes[i++] * 256) +
        bytes[i++];

    var batteryV =
        (batteryMv === 0xFFFF)
            ? null
            : batteryMv / 1000.0;


    /* ----------------------------------------------------------------------
     * Decoded payload
     * ---------------------------------------------------------------------- */

    return {
        version: version,

        sht30_present:
            !!(flags & (1 << 0)),

        moisture_present:
            !!(flags & (1 << 1)),

        water_line_high:
            !!(flags & (1 << 2)),

        battery_valid:
            !!(flags & (1 << 3)),

        battery_low:
            !!(flags & (1 << 4)),

        battery_critical:
            !!(flags & (1 << 5)),

        solar_present:
            !!(flags & (1 << 6)),

        charging:
            !!(flags & (1 << 7)),

        uptime_min:
            uptimeMin,

        pulse_count:
            pulseCount,

        water_liters:
            waterLiters,

        temperature_c:
            temperatureC,

        humidity_rh:
            humidityRh,

        moisture_raw:
            moistureRaw,

        moisture_pct:
            moisturePct,

        moisture_state:
            moistureState.value,

        moisture_state_label:
            moistureState.label,

        battery_v:
            batteryV
    };
}
