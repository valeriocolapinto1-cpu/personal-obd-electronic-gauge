// SPDX-License-Identifier: GPL-3.0-or-later
//
// Signal model: what can be shown on a screen (RPM, coolant temperature, boost, ...).
// A signal has display metadata, alarm thresholds and an ordered list of data sources.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace core::signals {

enum class PollPriority : uint8_t { Fast, Normal, Slow };

enum class SourceType : uint8_t {
    ObdPid,       ///< standard service 0x01 PID
    Derived,      ///< computed from other signals (e.g. boost = MAP - baro)
    Manufacturer, ///< manufacturer extension (e.g. VAG measuring block), opaque reference
    Module,       ///< Qwiic module (e.g. GPS speed)
};

struct SignalSource {
    SourceType type = SourceType::ObdPid;
    uint8_t pid = 0;       ///< ObdPid
    std::string ref;       ///< Derived / Manufacturer / Module reference
    bool verified = false; ///< confirmed on a real car (CLAUDE.md: never invent)
};

/// Thresholds in the signal's internal unit. NaN-free: use the `has_` flags.
struct AlarmThresholds {
    bool has_warn_low = false, has_warn_high = false, has_crit_low = false, has_crit_high = false;
    double warn_low = 0, warn_high = 0, crit_low = 0, crit_high = 0;
    double hysteresis = 1.0;
};

struct SignalDef {
    std::string id;    ///< stable identifier, e.g. "coolant_temp"
    std::string label; ///< short display label, e.g. "COOLANT"
    std::string unit;  ///< internal unit, e.g. "degC", "kPa", "rpm"
    double min = 0;
    double max = 100;
    uint8_t decimals = 0;
    PollPriority priority = PollPriority::Normal;
    std::vector<SignalSource> sources; ///< tried in order
    AlarmThresholds alarm;
};

/// Built-in definitions (label, unit, range, default source) for the standard signals.
/// Profiles start from these and override sources, ranges and thresholds.
const SignalDef *find_builtin(const std::string &id);
const std::vector<SignalDef> &builtin_signals();

} // namespace core::signals
