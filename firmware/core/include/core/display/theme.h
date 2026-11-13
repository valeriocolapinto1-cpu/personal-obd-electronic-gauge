// SPDX-License-Identifier: GPL-3.0-or-later
//
// Themes: day and night palettes, switchable from the light sensor with hysteresis.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core::display {

/// 0xRRGGBB colours.
struct Palette {
    uint32_t background = 0x000000;
    uint32_t primary = 0xFFFFFF; ///< numbers, needle hub
    uint32_t secondary = 0x808080;
    uint32_t accent = 0xE53935; ///< arc / bar fill
    uint32_t needle = 0xFF3D00;
    uint32_t warning = 0xFFB300;
    uint32_t critical = 0xFF1744;
};

struct Theme {
    std::string id;
    std::string name;
    Palette day;
    Palette night;
};

/// Built-in themes (always available, also when no theme file exists).
const std::vector<Theme> &builtin_themes();
const Theme &default_theme();
const Theme *find_theme(const std::string &id);

/// Parses a theme JSON: {"id","name","day":{...},"night":{...}} with "#RRGGBB" colours.
/// Missing night colours default to the day colours dimmed.
bool load_theme(const char *json, size_t len, Theme &out, std::string &error);

/// Parses "#RRGGBB".
bool parse_color(const std::string &s, uint32_t &out);

/// Scales a colour's brightness (0..1).
uint32_t dim(uint32_t rgb, double factor);

/// Day/night selection with hysteresis on a normalised ambient light level (0 = dark, 1 = bright).
class DayNightSwitch {
public:
    DayNightSwitch(double to_night_below = 0.15, double to_day_above = 0.25)
        : to_night_(to_night_below), to_day_(to_day_above)
    {}
    /// Returns true when night mode is active after this sample.
    bool update(double light);
    bool night() const { return night_; }

private:
    double to_night_;
    double to_day_;
    bool night_ = false;
};

} // namespace core::display
