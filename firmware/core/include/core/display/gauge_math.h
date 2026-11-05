// SPDX-License-Identifier: GPL-3.0-or-later
//
// Display-independent gauge maths: scaling, needle angles, ticks, value formatting
// in the user's units. Screens get sizes from the board; nothing here knows pixels.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "core/signals/signal.h"
#include "core/signals/units.h"

namespace core::display {

/// User unit preferences (web app, phase 8).
struct UnitPrefs {
    core::units::PressureUnit pressure = core::units::PressureUnit::BarRelative;
    core::units::TemperatureUnit temperature = core::units::TemperatureUnit::Celsius;
};

/// A value converted for display.
struct DisplayValue {
    double value = 0;
    double min = 0;
    double max = 1;
    uint8_t decimals = 0;
    std::string unit; ///< display unit label, e.g. "bar", "°C"
};

/// Converts a signal value (internal unit) and its range to the user's units.
/// `baro_kpa` is used for absolute boost display (MAP = boost + baro).
DisplayValue to_display(const core::signals::SignalDef &def, double value, const UnitPrefs &prefs,
                        double baro_kpa = 101.325);

/// Position of `value` in [min, max], clamped to [0, 1].
double fraction(double value, double min, double max);

/// Needle angle in degrees for a dial starting at `start_deg` sweeping `sweep_deg` clockwise.
double needle_angle(double value, double min, double max, double start_deg, double sweep_deg);

/// Chooses a "nice" major tick step (1, 2, 2.5, 5 x 10^n) for about `target_ticks` intervals.
double nice_step(double min, double max, int target_ticks);

/// Formats with fixed decimals; "--" for invalid values. Avoids "-0".
std::string format_value(double value, uint8_t decimals, bool valid);

} // namespace core::display
