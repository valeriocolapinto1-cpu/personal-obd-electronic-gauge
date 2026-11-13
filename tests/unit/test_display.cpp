// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "core/display/boot_sequence.h"
#include "core/display/gauge_math.h"
#include "core/display/screens.h"
#include "core/display/theme.h"
#include "core/input/input.h"
#include "core/signals/signal.h"

using namespace core::display;
using core::input::ButtonDetector;
using core::input::InputEvent;
using core::input::SwipeDetector;
using core::units::PressureUnit;
using core::units::TemperatureUnit;

namespace {
const core::signals::SignalDef &builtin(const char *id)
{
    return *core::signals::find_builtin(id);
}
} // namespace

TEST(GaugeMath, FractionAndNeedle)
{
    EXPECT_DOUBLE_EQ(fraction(50, 0, 100), 0.5);
    EXPECT_DOUBLE_EQ(fraction(-10, 0, 100), 0.0);
    EXPECT_DOUBLE_EQ(fraction(500, 0, 100), 1.0);
    EXPECT_DOUBLE_EQ(fraction(5, 10, 10), 0.0);
    EXPECT_DOUBLE_EQ(needle_angle(3500, 0, 7000, 135, 270), 270.0);
    EXPECT_DOUBLE_EQ(needle_angle(0, 0, 7000, 135, 270), 135.0);
    EXPECT_DOUBLE_EQ(needle_angle(9000, 0, 7000, 135, 270), 405.0);
}

TEST(GaugeMath, NiceSteps)
{
    EXPECT_DOUBLE_EQ(nice_step(0, 7000, 7), 1000.0);
    EXPECT_DOUBLE_EQ(nice_step(0, 8000, 8), 1000.0);
    EXPECT_DOUBLE_EQ(nice_step(-40, 140, 9), 20.0);
    EXPECT_DOUBLE_EQ(nice_step(-1, 2, 6), 0.5);
    EXPECT_DOUBLE_EQ(nice_step(0, 2.5, 5), 0.5);
    EXPECT_DOUBLE_EQ(nice_step(0, 1, 4), 0.25);
    EXPECT_DOUBLE_EQ(nice_step(5, 5, 4), 1.0);
}

TEST(GaugeMath, FormatValue)
{
    EXPECT_EQ(format_value(1726.4, 0, true), "1726");
    EXPECT_EQ(format_value(1.234, 2, true), "1.23");
    EXPECT_EQ(format_value(-0.004, 2, true), "0.00");
    EXPECT_EQ(format_value(-0.4, 0, true), "0");
    EXPECT_EQ(format_value(-12.5, 1, true), "-12.5");
    EXPECT_EQ(format_value(1, 0, false), "--");
    EXPECT_EQ(format_value(std::nan(""), 0, true), "--");
}

TEST(GaugeMath, TemperatureUnits)
{
    UnitPrefs c;
    DisplayValue v = to_display(builtin("coolant_temp"), 90, c);
    EXPECT_DOUBLE_EQ(v.value, 90);
    EXPECT_EQ(v.unit, "\xC2\xB0"
                      "C");
    UnitPrefs f;
    f.temperature = TemperatureUnit::Fahrenheit;
    v = to_display(builtin("coolant_temp"), 90, f);
    EXPECT_DOUBLE_EQ(v.value, 194);
    EXPECT_DOUBLE_EQ(v.min, -40);
    EXPECT_DOUBLE_EQ(v.max, 284);
}

TEST(GaugeMath, BoostUnits)
{
    const auto &boost = builtin("boost");
    UnitPrefs p;
    p.pressure = PressureUnit::BarRelative;
    DisplayValue v = to_display(boost, 150, p);
    EXPECT_DOUBLE_EQ(v.value, 1.5);
    EXPECT_EQ(v.unit, "bar");
    EXPECT_EQ(v.decimals, 2);

    p.pressure = PressureUnit::BarAbsolute;
    v = to_display(boost, 150, p, 100);
    EXPECT_DOUBLE_EQ(v.value, 2.5);
    EXPECT_DOUBLE_EQ(v.min, 0.0);

    p.pressure = PressureUnit::Psi;
    v = to_display(boost, 150, p);
    EXPECT_NEAR(v.value, 21.7557, 1e-3);
    EXPECT_EQ(v.unit, "psi");

    p.pressure = PressureUnit::KPa;
    v = to_display(boost, 150, p);
    EXPECT_DOUBLE_EQ(v.value, 150);
    EXPECT_EQ(v.unit, "kPa");
}

TEST(GaugeMath, AbsolutePressureAndOthersUnchanged)
{
    UnitPrefs p;
    DisplayValue v = to_display(builtin("intake_map"), 200, p);
    EXPECT_DOUBLE_EQ(v.value, 2.0);
    EXPECT_EQ(v.unit, "bar abs");
    v = to_display(builtin("engine_rpm"), 3000, p);
    EXPECT_DOUBLE_EQ(v.value, 3000);
    EXPECT_EQ(v.unit, "rpm");
}

TEST(Screens, Navigation)
{
    ScreenList s;
    s.set_default(3, GaugeStyle::Analog);
    EXPECT_EQ(s.current().slot, 0u);
    s.next();
    s.next();
    EXPECT_EQ(s.current().slot, 2u);
    s.next();
    EXPECT_EQ(s.current().slot, 0u);
    s.prev();
    EXPECT_EQ(s.current().slot, 2u);
    EXPECT_TRUE(s.jump_to_slot(1));
    EXPECT_EQ(s.index(), 1u);
    EXPECT_FALSE(s.jump_to_slot(7));
    s.toggle_style();
    EXPECT_EQ(s.current().style, GaugeStyle::Digital);
    s.next();
    EXPECT_EQ(s.current().style, GaugeStyle::Analog);

    ScreenList empty;
    empty.next();
    empty.prev();
    empty.toggle_style();
    EXPECT_TRUE(empty.empty());
}

TEST(Theme, Colors)
{
    uint32_t c = 0;
    EXPECT_TRUE(parse_color("#FF8000", c));
    EXPECT_EQ(c, 0xFF8000u);
    EXPECT_TRUE(parse_color("#00ff7f", c));
    EXPECT_EQ(c, 0x00FF7Fu);
    EXPECT_FALSE(parse_color("FF8000", c));
    EXPECT_FALSE(parse_color("#FF80", c));
    EXPECT_FALSE(parse_color("#GG8000", c));
    EXPECT_EQ(dim(0xFF8040, 0.5), 0x804020u);
    EXPECT_EQ(dim(0xFFFFFF, 0), 0u);
    EXPECT_EQ(dim(0x123456, 2.0), 0x123456u);
}

TEST(Theme, BuiltinsAndJson)
{
    ASSERT_GE(builtin_themes().size(), 2u);
    EXPECT_NE(find_theme("oem_red"), nullptr);
    EXPECT_EQ(find_theme("nope"), nullptr);
    for (const Theme &t : builtin_themes()) {
        EXPECT_LT(t.night.primary, t.day.primary + 1) << t.id; // night never brighter
    }

    const char *json = R"({"id":"t","name":"T","day":{"primary":"#FFFFFF","accent":"#00FF00"},
                          "night":{"accent":"#004000"}})";
    Theme t;
    std::string err;
    ASSERT_TRUE(load_theme(json, std::strlen(json), t, err)) << err;
    EXPECT_EQ(t.day.accent, 0x00FF00u);
    EXPECT_EQ(t.night.accent, 0x004000u);
    EXPECT_EQ(t.night.primary, dim(0xFFFFFF, 0.6));

    const char *bad = R"({"id":"t","name":"T","day":{"primary":"white"}})";
    EXPECT_FALSE(load_theme(bad, std::strlen(bad), t, err));
    EXPECT_FALSE(load_theme("{}", 2, t, err));
}

TEST(Theme, DayNightHysteresis)
{
    DayNightSwitch sw(0.15, 0.25);
    EXPECT_FALSE(sw.update(0.5));
    EXPECT_FALSE(sw.update(0.16));
    EXPECT_TRUE(sw.update(0.14));
    EXPECT_TRUE(sw.update(0.2)); // inside the band: stays night
    EXPECT_TRUE(sw.update(0.25));
    EXPECT_FALSE(sw.update(0.26));
    EXPECT_FALSE(sw.update(0.2)); // inside the band: stays day
}

TEST(Boot, Timeline)
{
    BootTiming t;
    EXPECT_EQ(boot_frame(0, t).phase, BootPhase::SweepUp);
    EXPECT_DOUBLE_EQ(boot_frame(0, t).sweep, 0.0);
    EXPECT_DOUBLE_EQ(boot_frame(t.sweep_up_ms / 2, t).sweep, 0.5);
    EXPECT_DOUBLE_EQ(boot_frame(t.sweep_up_ms + 10, t).sweep, 1.0);
    const uint32_t down = t.sweep_up_ms + t.hold_ms;
    EXPECT_EQ(boot_frame(down + 1, t).phase, BootPhase::SweepDown);
    EXPECT_NEAR(boot_frame(down + t.sweep_down_ms / 2, t).sweep, 0.5, 1e-9);
    EXPECT_EQ(boot_frame(down + t.sweep_down_ms, t).phase, BootPhase::Splash);
    EXPECT_EQ(boot_frame(down + t.sweep_down_ms + t.splash_ms, t).phase, BootPhase::Done);
    // Monotonic up, then down
    double prev = -1;
    for (uint32_t e = 0; e <= t.sweep_up_ms; e += 10) {
        const double s = boot_frame(e, t).sweep;
        EXPECT_GE(s, prev);
        prev = s;
    }
}

TEST(Input, Swipes)
{
    SwipeDetector d(60);
    EXPECT_EQ(d.update(true, 300, 240, 0), InputEvent::None);
    EXPECT_EQ(d.update(true, 200, 245, 50), InputEvent::None);
    EXPECT_EQ(d.update(false, 0, 0, 100), InputEvent::Next); // right-to-left

    d.update(true, 100, 240, 200);
    d.update(true, 250, 230, 260);
    EXPECT_EQ(d.update(false, 0, 0, 300), InputEvent::Previous);

    d.update(true, 240, 240, 400);
    d.update(true, 243, 238, 450);
    EXPECT_EQ(d.update(false, 0, 0, 500), InputEvent::Tap);

    d.update(true, 240, 100, 600); // vertical drag: ignored
    d.update(true, 260, 300, 650);
    EXPECT_EQ(d.update(false, 0, 0, 700), InputEvent::None);

    d.update(true, 240, 240, 800); // long hold without movement: not a tap
    EXPECT_EQ(d.update(false, 0, 0, 1500), InputEvent::None);
    EXPECT_EQ(d.update(false, 0, 0, 1600), InputEvent::None); // release without press
}

TEST(Input, ButtonShortLongAndBounce)
{
    ButtonDetector b(30, 800);
    uint32_t t = 0;
    // Bounce shorter than the debounce time: nothing
    EXPECT_EQ(b.update(true, t), InputEvent::None);
    EXPECT_EQ(b.update(false, t + 10), InputEvent::None);
    t = 100;
    for (; t < 200; t += 5) {
        EXPECT_EQ(b.update(true, t), InputEvent::None);
    }
    InputEvent e = InputEvent::None;
    for (; t < 300 && e == InputEvent::None; t += 5) {
        e = b.update(false, t);
    }
    EXPECT_EQ(e, InputEvent::Next);

    // Long press fires once while held, nothing on release
    int longs = 0;
    for (t = 1000; t < 2500; t += 5) {
        if (b.update(true, t) == InputEvent::LongPress) {
            ++longs;
        }
    }
    EXPECT_EQ(longs, 1);
    for (; t < 2700; t += 5) {
        EXPECT_EQ(b.update(false, t), InputEvent::None);
    }
}
