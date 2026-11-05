// SPDX-License-Identifier: GPL-3.0-or-later
//
// Car profiles: data (JSON) describing how to recognise a car and which signals to read.
// Schema: docs/PROFILE_SCHEMA.md.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/obd/vin.h"
#include "core/signals/signal.h"

namespace core::profiles {

enum class ProtocolHint : uint8_t {
    Auto,
    Can11Bit500k,
    Can29Bit500k,
    Can11Bit250k,
    Can29Bit250k,
    KLineKwpFast, ///< ISO 14230-4 fast init
    KLineIso9141, ///< ISO 9141-2 5-baud init
};

struct MatchRules {
    std::vector<std::string> vin_prefixes; ///< e.g. "WVWZZZ13" (longest wins)
    std::vector<std::string> wmi;          ///< e.g. "WVW"
    uint16_t year_from = 0;                ///< 0 = no limit
    uint16_t year_to = 0;
    bool verified = false; ///< matching rules confirmed on a real car
};

struct RpmConfig {
    uint16_t limiter = 6000;    ///< placeholder: the user sets the real value in the app
    uint16_t ring_start = 3000; ///< LED ring starts filling here
    uint16_t scale_max = 7000;  ///< needle full scale
};

struct Profile {
    std::string id;
    std::string name;
    std::string extension; ///< manufacturer extension to load ("vag", "hyundai", "")
    ProtocolHint protocol = ProtocolHint::Auto;
    uint8_t cylinders = 4;
    std::string fuel = "petrol";
    MatchRules match;
    RpmConfig rpm;
    std::vector<core::signals::SignalDef> signals;
};

struct LoadResult {
    bool ok = false;
    std::string error; ///< human-readable reason (shown in the web app)
};

/// Parses a profile from JSON text. Signals inherit from the built-in definitions and
/// only override what the profile states.
LoadResult load_profile(const char *json, size_t len, Profile &out);

ProtocolHint parse_protocol_hint(const std::string &s);
const char *to_string(ProtocolHint h);

/// Score of a profile for a VIN (0 = no match). Manual selection is handled by the caller.
int match_score(const Profile &p, const core::obd::Vin *vin);

/// Picks the best profile for the VIN: manual id first, then best VIN score, then the profile
/// with id "generic_obd". Returns nullptr only if `profiles` is empty.
const Profile *select_profile(const std::vector<Profile> &profiles, const core::obd::Vin *vin,
                              const std::string &manual_id);

} // namespace core::profiles
