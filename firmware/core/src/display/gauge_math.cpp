// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/display/gauge_math.h"

#include <cmath>
#include <cstdio>

namespace core::display {

using core::units::PressureUnit;
using core::units::TemperatureUnit;

namespace {
bool is_pressure_boost(const core::signals::SignalDef &d)
{
    return d.id == "boost";
}

bool is_pressure_abs(const core::signals::SignalDef &d)
{
    return d.unit == "kPa" && !is_pressure_boost(d);
}
} // namespace

DisplayValue to_display(const core::signals::SignalDef &def, double value, const UnitPrefs &prefs, double baro_kpa)
{
    DisplayValue out;
    out.value = value;
    out.min = def.min;
    out.max = def.max;
    out.decimals = def.decimals;
    out.unit = def.unit;

    if (def.unit == "degC") {
        if (prefs.temperature == TemperatureUnit::Fahrenheit) {
            out.value = core::units::celsius_to_fahrenheit(value);
            out.min = core::units::celsius_to_fahrenheit(def.min);
            out.max = core::units::celsius_to_fahrenheit(def.max);
            out.unit = "\xC2\xB0"
                       "F";
        } else {
            out.unit = "\xC2\xB0"
                       "C";
        }
        return out;
    }

    if (is_pressure_boost(def)) {
        // Boost is stored relative (kPa). Convert to the chosen boost unit.
        switch (prefs.pressure) {
            case PressureUnit::BarRelative:
                out.value = value / core::units::kKpaPerBar;
                out.min = def.min / core::units::kKpaPerBar;
                out.max = def.max / core::units::kKpaPerBar;
                out.decimals = 2;
                out.unit = "bar";
                break;
            case PressureUnit::BarAbsolute:
                out.value = (value + baro_kpa) / core::units::kKpaPerBar;
                out.min = 0.0;
                out.max = (def.max + baro_kpa) / core::units::kKpaPerBar;
                out.decimals = 2;
                out.unit = "bar abs";
                break;
            case PressureUnit::Psi:
                out.value = value / core::units::kKpaPerPsi;
                out.min = def.min / core::units::kKpaPerPsi;
                out.max = def.max / core::units::kKpaPerPsi;
                out.decimals = 1;
                out.unit = "psi";
                break;
            case PressureUnit::KPa:
                out.decimals = 0;
                out.unit = "kPa";
                break;
        }
        return out;
    }

    if (is_pressure_abs(def) && prefs.pressure != PressureUnit::KPa) {
        // Absolute pressures (MAP, baro) follow bar/psi preference but stay absolute.
        if (prefs.pressure == PressureUnit::Psi) {
            out.value = value / core::units::kKpaPerPsi;
            out.min = def.min / core::units::kKpaPerPsi;
            out.max = def.max / core::units::kKpaPerPsi;
            out.decimals = 1;
            out.unit = "psi abs";
        } else {
            out.value = value / core::units::kKpaPerBar;
            out.min = def.min / core::units::kKpaPerBar;
            out.max = def.max / core::units::kKpaPerBar;
            out.decimals = 2;
            out.unit = "bar abs";
        }
    }
    return out;
}

double fraction(double value, double min, double max)
{
    if (!(max > min)) {
        return 0.0;
    }
    const double f = (value - min) / (max - min);
    return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
}

double needle_angle(double value, double min, double max, double start_deg, double sweep_deg)
{
    return start_deg + fraction(value, min, max) * sweep_deg;
}

double nice_step(double min, double max, int target_ticks)
{
    const double span = max - min;
    if (!(span > 0) || target_ticks < 1) {
        return 1.0;
    }
    const double raw = span / target_ticks;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    double nice = 10.0;
    if (norm <= 1.0) {
        nice = 1.0;
    } else if (norm <= 2.0) {
        nice = 2.0;
    } else if (norm <= 2.5) {
        nice = 2.5;
    } else if (norm <= 5.0) {
        nice = 5.0;
    }
    return nice * mag;
}

std::string format_value(double value, uint8_t decimals, bool valid)
{
    if (!valid || std::isnan(value) || std::isinf(value)) {
        return "--";
    }
    if (decimals > 3) {
        decimals = 3;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", static_cast<int>(decimals), value);
    std::string s(buf);
    // "-0", "-0.00" -> "0", "0.00"
    bool all_zero = true;
    for (char c : s) {
        if (c != '-' && c != '0' && c != '.') {
            all_zero = false;
            break;
        }
    }
    if (all_zero && !s.empty() && s[0] == '-') {
        s.erase(0, 1);
    }
    return s;
}

} // namespace core::display
