// SPDX-License-Identifier: GPL-3.0-or-later
//
// VIN (ISO 3779) parsing and validation.
#pragma once

#include <cstddef>
#include <cstdint>

namespace core::obd {

struct Vin {
    char text[18] = {}; ///< 17 chars + NUL

    const char *wmi() const { return text; } ///< first 3 chars (not NUL-terminated)
    bool has_prefix(const char *prefix) const;
    /// Model-year code (position 10).
    char year_code() const { return text[9]; }
};

enum class VinStatus : uint8_t {
    Ok,
    WrongLength,
    InvalidCharacter, ///< outside A-Z/0-9 or I, O, Q
};

VinStatus parse_vin(const char *chars, size_t len, Vin &out);

/// Parses the service 09 PID 02 response data. On CAN the data begins with the item
/// count (0x01); some ECUs pad with leading NULs. Returns WrongLength if 17 valid chars
/// cannot be found.
VinStatus parse_vin_response(const uint8_t *data, size_t len, Vin &out);

/// ISO 3779 check digit (position 9). Mandatory only in North America: European VINs
/// (e.g. VW "WVWZZZ...") often have 'Z' there, so a failed check is informational.
bool vin_check_digit_valid(const Vin &vin);

/// Possible model years for the position-10 code (30-year cycle).
/// Returns how many candidates were written (0 if the code is not a valid year code).
size_t vin_model_years(char code, uint16_t out[2]);

} // namespace core::obd
