// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit conversions and derived values. Internal units are SI-like:
// temperature degC, pressure kPa (absolute unless stated), speed km/h, fuel L/h.
#pragma once

#include <cstdint>

namespace core::units {

constexpr double kKpaPerBar = 100.0;
constexpr double kKpaPerPsi = 6.894757293168361;

enum class PressureUnit : uint8_t { BarRelative, BarAbsolute, Psi, KPa };
enum class TemperatureUnit : uint8_t { Celsius, Fahrenheit };

inline double celsius_to_fahrenheit(double c)
{
    return c * 9.0 / 5.0 + 32.0;
}

/// Boost (gauge pressure) = manifold absolute pressure - barometric pressure, in kPa.
inline double boost_relative_kpa(double map_kpa, double baro_kpa)
{
    return map_kpa - baro_kpa;
}

/// Converts a manifold absolute pressure to the user's boost unit.
/// Relative units need the barometric pressure; psi is relative (gauge) pressure.
double pressure_for_display(double map_kpa_abs, double baro_kpa, PressureUnit unit);

double temperature_for_display(double celsius, TemperatureUnit unit);

/// Instantaneous consumption in L/100 km from fuel rate (L/h) and speed (km/h).
/// Returns false below `min_speed_kmh` (show L/h instead when stationary).
bool fuel_l_per_100km(double fuel_l_per_h, double speed_kmh, double &out, double min_speed_kmh = 3.0);

/// Fuel rate (L/h) of a 4-stroke engine from injected quantity per stroke.
/// mg/stroke x cylinders x (rpm / 2) strokes per minute x 60 / density(g/L) / 1000.
/// Used for the Golf TDI ("estimated", see PROJECT_PLAN 4.4).
double fuel_rate_from_injection(double mg_per_stroke, double rpm, unsigned cylinders, double fuel_density_g_per_l);

/// Diesel density at 15 degC (EN 590 range 820-845 kg/m3; mid value).
constexpr double kDieselDensityGPerL = 832.5;
/// Petrol density at 15 degC (EN 228 range 720-775 kg/m3; mid value).
constexpr double kPetrolDensityGPerL = 747.5;

} // namespace core::units
