// SPDX-License-Identifier: GPL-3.0-or-later
//
// Formulas: SAE J1979 / ISO 15031-5 (also published in the Wikipedia "OBD-II PIDs" table).
#include "core/obd/obd.h"

namespace core::obd {

namespace {

uint16_t u16(const uint8_t *d)
{
    return static_cast<uint16_t>((d[0] << 8) | d[1]);
}

bool dec_percent(const uint8_t *d, double &out)
{
    out = d[0] * 100.0 / 255.0;
    return true;
}

bool dec_temp(const uint8_t *d, double &out)
{
    out = d[0] - 40.0;
    return true;
}

bool dec_kpa_1(const uint8_t *d, double &out)
{
    out = d[0];
    return true;
}

bool dec_rpm(const uint8_t *d, double &out)
{
    out = u16(d) / 4.0;
    return true;
}

bool dec_speed(const uint8_t *d, double &out)
{
    out = d[0];
    return true;
}

bool dec_maf(const uint8_t *d, double &out)
{
    out = u16(d) / 100.0;
    return true;
}

bool dec_voltage(const uint8_t *d, double &out)
{
    out = u16(d) / 1000.0;
    return true;
}

bool dec_fuel_rate(const uint8_t *d, double &out)
{
    out = u16(d) / 20.0;
    return true;
}

// PID 0x87: A = support bits (bit0 sensor A, bit1 sensor B), B-C sensor A, D-E sensor B,
// 1/32 kPa per bit. The gauge uses sensor A.
bool dec_map_extended(const uint8_t *d, double &out)
{
    if ((d[0] & 0x01) == 0) {
        return false;
    }
    out = u16(&d[1]) / 32.0;
    return true;
}

// clang-format off
constexpr PidInfo kPids[] = {
    {pid::kEngineLoad,        1, "engine_load",      "%",    dec_percent},
    {pid::kCoolantTemp,       1, "coolant_temp",     "degC", dec_temp},
    {pid::kIntakeMap,         1, "intake_map",       "kPa",  dec_kpa_1},
    {pid::kEngineRpm,         2, "engine_rpm",       "rpm",  dec_rpm},
    {pid::kVehicleSpeed,      1, "vehicle_speed",    "km/h", dec_speed},
    {pid::kIntakeAirTemp,     1, "intake_air_temp",  "degC", dec_temp},
    {pid::kMafRate,           2, "maf_rate",         "g/s",  dec_maf},
    {pid::kThrottle,          1, "throttle",         "%",    dec_percent},
    {pid::kBarometric,        1, "barometric",       "kPa",  dec_kpa_1},
    {pid::kModuleVoltage,     2, "module_voltage",   "V",    dec_voltage},
    {pid::kAmbientTemp,       1, "ambient_temp",     "degC", dec_temp},
    {pid::kOilTemp,           1, "oil_temp",         "degC", dec_temp},
    {pid::kFuelRate,          2, "fuel_rate",        "L/h",  dec_fuel_rate},
    {pid::kIntakeMapExtended, 5, "intake_map_ext",   "kPa",  dec_map_extended},
};
// clang-format on

} // namespace

const PidInfo *find_pid(uint8_t p)
{
    for (const PidInfo &info : kPids) {
        if (info.pid == p) {
            return &info;
        }
    }
    return nullptr;
}

size_t build_request_01(uint8_t p, uint8_t *out, size_t cap)
{
    if (cap < 2) {
        return 0;
    }
    out[0] = kSidCurrentData;
    out[1] = p;
    return 2;
}

size_t build_request_09(uint8_t info_type, uint8_t *out, size_t cap)
{
    if (cap < 2) {
        return 0;
    }
    out[0] = kSidVehicleInfo;
    out[1] = info_type;
    return 2;
}

namespace {
ParsedResponse parse_common(uint8_t sid, uint8_t expected_id, const uint8_t *msg, size_t len)
{
    ParsedResponse r;
    if (msg == nullptr || len < 2) {
        return r;
    }
    if (msg[0] == kNegativeResponse) {
        if (len < 3) {
            return r;
        }
        r.kind = (msg[1] == sid) ? ResponseKind::Negative : ResponseKind::Mismatch;
        r.nrc = msg[2];
        return r;
    }
    if (msg[0] != sid + kPositiveOffset || msg[1] != expected_id) {
        r.kind = ResponseKind::Mismatch;
        return r;
    }
    r.kind = ResponseKind::Positive;
    r.data = msg + 2;
    r.data_len = len - 2;
    return r;
}
} // namespace

ParsedResponse parse_response_01(uint8_t expected_pid, const uint8_t *msg, size_t len)
{
    ParsedResponse r = parse_common(kSidCurrentData, expected_pid, msg, len);
    if (r.kind == ResponseKind::Positive) {
        const PidInfo *info = find_pid(expected_pid);
        const size_t need = ((expected_pid % 0x20) == 0) ? 4 : (info ? info->data_len : 1);
        if (r.data_len < need) {
            r.kind = ResponseKind::Malformed;
        }
    }
    return r;
}

ParsedResponse parse_response_09(uint8_t expected_info, const uint8_t *msg, size_t len)
{
    return parse_common(kSidVehicleInfo, expected_info, msg, len);
}

bool decode_support_bitmap(uint8_t base, const uint8_t *d, bool supported[256])
{
    const uint32_t bits = (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) |
                          (static_cast<uint32_t>(d[2]) << 8) | d[3];
    for (unsigned i = 1; i <= 32; ++i) {
        const unsigned p = base + i;
        if (p < 256) {
            supported[p] = (bits >> (32 - i)) & 1u;
        }
    }
    return (bits & 1u) != 0;
}

} // namespace core::obd
