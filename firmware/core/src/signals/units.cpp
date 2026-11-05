// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/signals/units.h"

namespace core::units {

double pressure_for_display(double map_kpa_abs, double baro_kpa, PressureUnit unit)
{
    switch (unit) {
        case PressureUnit::BarRelative:
            return boost_relative_kpa(map_kpa_abs, baro_kpa) / kKpaPerBar;
        case PressureUnit::BarAbsolute:
            return map_kpa_abs / kKpaPerBar;
        case PressureUnit::Psi:
            return boost_relative_kpa(map_kpa_abs, baro_kpa) / kKpaPerPsi;
        case PressureUnit::KPa:
            return boost_relative_kpa(map_kpa_abs, baro_kpa);
    }
    return 0.0;
}

double temperature_for_display(double celsius, TemperatureUnit unit)
{
    return unit == TemperatureUnit::Fahrenheit ? celsius_to_fahrenheit(celsius) : celsius;
}

bool fuel_l_per_100km(double fuel_l_per_h, double speed_kmh, double &out, double min_speed_kmh)
{
    if (speed_kmh < min_speed_kmh) {
        return false;
    }
    out = fuel_l_per_h * 100.0 / speed_kmh;
    return true;
}

double fuel_rate_from_injection(double mg_per_stroke, double rpm, unsigned cylinders, double density_g_per_l)
{
    if (density_g_per_l <= 0.0) {
        return 0.0;
    }
    const double injections_per_hour = cylinders * (rpm / 2.0) * 60.0;
    const double grams_per_hour = mg_per_stroke * injections_per_hour / 1000.0;
    return grams_per_hour / density_g_per_l;
}

} // namespace core::units
