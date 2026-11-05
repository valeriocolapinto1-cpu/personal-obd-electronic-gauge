// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "core/obd/vin.h"
#include "core/signals/units.h"

using namespace core::units;
using namespace core::obd;

TEST(Units, Temperature)
{
    EXPECT_DOUBLE_EQ(celsius_to_fahrenheit(0), 32.0);
    EXPECT_DOUBLE_EQ(celsius_to_fahrenheit(100), 212.0);
    EXPECT_DOUBLE_EQ(celsius_to_fahrenheit(-40), -40.0);
    EXPECT_DOUBLE_EQ(temperature_for_display(90, TemperatureUnit::Celsius), 90.0);
    EXPECT_DOUBLE_EQ(temperature_for_display(90, TemperatureUnit::Fahrenheit), 194.0);
}

TEST(Units, Boost)
{
    const double map = 251.0;
    const double baro = 101.0;
    EXPECT_DOUBLE_EQ(boost_relative_kpa(map, baro), 150.0);
    EXPECT_DOUBLE_EQ(pressure_for_display(map, baro, PressureUnit::BarRelative), 1.5);
    EXPECT_DOUBLE_EQ(pressure_for_display(map, baro, PressureUnit::BarAbsolute), 2.51);
    EXPECT_DOUBLE_EQ(pressure_for_display(map, baro, PressureUnit::KPa), 150.0);
    EXPECT_NEAR(pressure_for_display(map, baro, PressureUnit::Psi), 21.7557, 1e-3);
    // Vacuum at idle is negative boost
    EXPECT_NEAR(pressure_for_display(35.0, 101.0, PressureUnit::BarRelative), -0.66, 1e-9);
}

TEST(Units, FuelConsumption)
{
    double l100 = 0;
    EXPECT_TRUE(fuel_l_per_100km(6.0, 100.0, l100));
    EXPECT_DOUBLE_EQ(l100, 6.0);
    EXPECT_TRUE(fuel_l_per_100km(3.0, 50.0, l100));
    EXPECT_DOUBLE_EQ(l100, 6.0);
    EXPECT_FALSE(fuel_l_per_100km(0.8, 0.0, l100));
    EXPECT_FALSE(fuel_l_per_100km(0.8, 2.9, l100));
}

TEST(Units, FuelRateFromInjection)
{
    // 10 mg/stroke, 2000 rpm, 4 cyl: 4 * 1000 * 60 = 240000 injections/h -> 2400 g/h
    EXPECT_NEAR(fuel_rate_from_injection(10.0, 2000.0, 4, 800.0), 3.0, 1e-12);
    EXPECT_NEAR(fuel_rate_from_injection(10.0, 2000.0, 4, kDieselDensityGPerL), 2400.0 / 832.5, 1e-12);
    EXPECT_DOUBLE_EQ(fuel_rate_from_injection(10.0, 0.0, 4, 800.0), 0.0);
    EXPECT_DOUBLE_EQ(fuel_rate_from_injection(10.0, 2000.0, 4, 0.0), 0.0);
}

TEST(Vin, ParseAndValidate)
{
    Vin v;
    ASSERT_EQ(parse_vin("1M8GDM9AXKP042788", 17, v), VinStatus::Ok);
    EXPECT_STREQ(v.text, "1M8GDM9AXKP042788");
    EXPECT_TRUE(vin_check_digit_valid(v)); // textbook example, check digit X
    EXPECT_TRUE(v.has_prefix("1M8"));
    EXPECT_FALSE(v.has_prefix("WVW"));
    EXPECT_EQ(v.year_code(), 'K');

    EXPECT_EQ(parse_vin("1M8GDM9AXKP04278", 16, v), VinStatus::WrongLength);
    EXPECT_EQ(parse_vin("1M8GDM9AXKP04278O", 17, v), VinStatus::InvalidCharacter); // letter O
    EXPECT_EQ(parse_vin("1m8GDM9AXKP042788", 17, v), VinStatus::InvalidCharacter);
}

TEST(Vin, EuropeanPlaceholderCheckDigit)
{
    // Placeholder VIN (not a real car): 'Z' at position 9, as on many European VINs.
    Vin v;
    ASSERT_EQ(parse_vin("WVWZZZ13ZAV000001", 17, v), VinStatus::Ok);
    EXPECT_FALSE(vin_check_digit_valid(v));
    EXPECT_TRUE(v.has_prefix("WVWZZZ13"));
}

TEST(Vin, ParseServiceResponse)
{
    const char *vin = "WVWZZZ13ZAV000001";
    std::vector<uint8_t> data = {0x01};
    data.insert(data.end(), vin, vin + 17);
    Vin v;
    ASSERT_EQ(parse_vin_response(data.data(), data.size(), v), VinStatus::Ok);
    EXPECT_STREQ(v.text, vin);

    std::vector<uint8_t> padded = {0x01, 0x00, 0x00, 0x00};
    padded.insert(padded.end(), vin, vin + 17);
    ASSERT_EQ(parse_vin_response(padded.data(), padded.size(), v), VinStatus::Ok);

    EXPECT_EQ(parse_vin_response(data.data(), 10, v), VinStatus::WrongLength);
}

TEST(Vin, ModelYearCodes)
{
    uint16_t y[2];
    ASSERT_EQ(vin_model_years('A', y), 2u);
    EXPECT_EQ(y[0], 1980);
    EXPECT_EQ(y[1], 2010);
    ASSERT_EQ(vin_model_years('8', y), 2u);
    EXPECT_EQ(y[0], 2008);
    ASSERT_EQ(vin_model_years('Y', y), 2u);
    EXPECT_EQ(y[0], 2000);
    ASSERT_EQ(vin_model_years('1', y), 2u);
    EXPECT_EQ(y[0], 2001);
    EXPECT_EQ(vin_model_years('U', y), 0u); // not used for years
    EXPECT_EQ(vin_model_years('Z', y), 0u);
    EXPECT_EQ(vin_model_years('0', y), 0u);
}
