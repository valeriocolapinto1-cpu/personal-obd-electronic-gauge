// SPDX-License-Identifier: GPL-3.0-or-later
//
// OBD-II (SAE J1979 / ISO 15031-5) service 0x01 and 0x09 encoding and decoding.
// Only standard, publicly documented PIDs and formulas are listed here.
#pragma once

#include <cstddef>
#include <cstdint>

namespace core::obd {

constexpr uint8_t kSidCurrentData = 0x01;
constexpr uint8_t kSidVehicleInfo = 0x09;
constexpr uint8_t kNegativeResponse = 0x7F;
constexpr uint8_t kPositiveOffset = 0x40;

constexpr uint8_t kInfoTypeVin = 0x02;
constexpr uint8_t kInfoTypeCalId = 0x04;

// Standard PIDs used by the gauge (SAE J1979 names).
namespace pid {
constexpr uint8_t kSupported01To20 = 0x00;
constexpr uint8_t kEngineLoad = 0x04;
constexpr uint8_t kCoolantTemp = 0x05;
constexpr uint8_t kIntakeMap = 0x0B;
constexpr uint8_t kEngineRpm = 0x0C;
constexpr uint8_t kVehicleSpeed = 0x0D;
constexpr uint8_t kIntakeAirTemp = 0x0F;
constexpr uint8_t kMafRate = 0x10;
constexpr uint8_t kThrottle = 0x11;
constexpr uint8_t kBarometric = 0x33;
constexpr uint8_t kModuleVoltage = 0x42;
constexpr uint8_t kAmbientTemp = 0x46;
constexpr uint8_t kOilTemp = 0x5C;
constexpr uint8_t kFuelRate = 0x5E;
constexpr uint8_t kIntakeMapExtended = 0x87;
} // namespace pid

/// Description and decoder of one standard PID.
struct PidInfo {
    uint8_t pid;
    uint8_t data_len; ///< bytes after "41 <pid>"
    const char *name;
    const char *unit; ///< SI unit of the decoded value
    /// Decodes `data` (data_len bytes). Returns false if the value is not available
    /// (e.g. sensor not supported according to a support byte).
    bool (*decode)(const uint8_t *data, double &out);
};

/// Looks up a PID in the table. nullptr if the gauge does not know it.
const PidInfo *find_pid(uint8_t pid);

/// Builds a service 0x01 request for one PID. Returns the payload length (2).
size_t build_request_01(uint8_t pid, uint8_t *out, size_t cap);
/// Builds a service 0x09 request. Returns the payload length (2).
size_t build_request_09(uint8_t info_type, uint8_t *out, size_t cap);

enum class ResponseKind : uint8_t {
    Positive,
    Negative, ///< 7F <sid> <nrc>
    Mismatch, ///< answer to another service/PID (e.g. another dongle's request)
    Malformed,
};

struct ParsedResponse {
    ResponseKind kind = ResponseKind::Malformed;
    uint8_t nrc = 0;               ///< negative response code
    const uint8_t *data = nullptr; ///< bytes after the echo (service 01: after PID)
    size_t data_len = 0;
};

/// Validates a response to "01 <pid>": service echo + PID echo (CLAUDE.md coexistence rule).
ParsedResponse parse_response_01(uint8_t expected_pid, const uint8_t *msg, size_t len);

/// Validates a response to "09 <info_type>". On CAN the data starts with the item count.
ParsedResponse parse_response_09(uint8_t expected_info, const uint8_t *msg, size_t len);

/// Decodes a "supported PIDs" bitmap (response data of PID 0x00/0x20/...) into `supported`
/// (256 bools indexed by PID). Returns true if the next range is announced.
bool decode_support_bitmap(uint8_t base_pid, const uint8_t *data4, bool supported[256]);

} // namespace core::obd
