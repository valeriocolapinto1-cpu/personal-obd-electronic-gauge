// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/display/theme.h"

#include "core/util/json.h"

namespace core::display {

namespace {

Theme make(const char *id, const char *name, Palette day)
{
    Theme t;
    t.id = id;
    t.name = name;
    t.day = day;
    t.night = day;
    t.night.primary = dim(day.primary, 0.6);
    t.night.secondary = dim(day.secondary, 0.5);
    t.night.accent = dim(day.accent, 0.6);
    t.night.needle = dim(day.needle, 0.7);
    t.night.warning = dim(day.warning, 0.7);
    t.night.critical = dim(day.critical, 0.8);
    return t;
}

std::vector<Theme> make_builtins()
{
    std::vector<Theme> v;
    v.push_back(
        make("oem_red", "OEM red", Palette{0x000000, 0xFFFFFF, 0x9E9E9E, 0xE53935, 0xFF3D00, 0xFFB300, 0xFF1744}));
    v.push_back(make("racing_green", "Racing green",
                     Palette{0x050A05, 0xE8F5E9, 0x66BB6A, 0x00E676, 0x76FF03, 0xFFEA00, 0xFF1744}));
    v.push_back(
        make("ice_blue", "Ice blue", Palette{0x000814, 0xE3F2FD, 0x64B5F6, 0x00B0FF, 0x40C4FF, 0xFFC400, 0xFF1744}));
    v.push_back(make("amber_classic", "Amber classic",
                     Palette{0x000000, 0xFFC107, 0xA1887F, 0xFF8F00, 0xFFA000, 0xFFEB3B, 0xFF3D00}));
    return v;
}

bool read_palette(const core::json::Value &v, Palette &p, std::string &error)
{
    if (!v.valid()) {
        return true;
    }
    if (!v.is_object()) {
        error = "palette must be an object";
        return false;
    }
    struct Field {
        const char *key;
        uint32_t *dst;
    };
    const Field fields[] = {{"background", &p.background}, {"primary", &p.primary}, {"secondary", &p.secondary},
                            {"accent", &p.accent},         {"needle", &p.needle},   {"warning", &p.warning},
                            {"critical", &p.critical}};
    for (const Field &f : fields) {
        const core::json::Value c = v[f.key];
        if (!c.valid()) {
            continue;
        }
        if (!parse_color(c.as_string(), *f.dst)) {
            error = std::string("bad colour for '") + f.key + "'";
            return false;
        }
    }
    return true;
}

} // namespace

const std::vector<Theme> &builtin_themes()
{
    static const std::vector<Theme> kThemes = make_builtins();
    return kThemes;
}

const Theme &default_theme()
{
    return builtin_themes().front();
}

const Theme *find_theme(const std::string &id)
{
    for (const Theme &t : builtin_themes()) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

bool parse_color(const std::string &s, uint32_t &out)
{
    if (s.size() != 7 || s[0] != '#') {
        return false;
    }
    uint32_t v = 0;
    for (size_t i = 1; i < 7; ++i) {
        const char c = s[i];
        uint32_t d = 0;
        if (c >= '0' && c <= '9') {
            d = static_cast<uint32_t>(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<uint32_t>(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<uint32_t>(c - 'A' + 10);
        } else {
            return false;
        }
        v = (v << 4) | d;
    }
    out = v;
    return true;
}

uint32_t dim(uint32_t rgb, double factor)
{
    if (factor < 0) {
        factor = 0;
    }
    if (factor > 1) {
        factor = 1;
    }
    auto ch = [&](int shift) {
        const double c = static_cast<double>((rgb >> shift) & 0xFF) * factor;
        return static_cast<uint32_t>(c + 0.5) << shift;
    };
    return ch(16) | ch(8) | ch(0);
}

bool load_theme(const char *json, size_t len, Theme &out, std::string &error)
{
    core::json::Document doc;
    if (!doc.parse(json, len)) {
        error = std::string("JSON error: ") + doc.error();
        return false;
    }
    const core::json::Value r = doc.root();
    if (!r["id"].is_string() || !r["name"].is_string() || !r["day"].is_object()) {
        error = "theme needs 'id', 'name' and a 'day' palette";
        return false;
    }
    Theme t = make("x", "x", Palette{});
    t.id = r["id"].as_string();
    t.name = r["name"].as_string();
    if (!read_palette(r["day"], t.day, error)) {
        return false;
    }
    // Night defaults to the dimmed day palette, then explicit night colours override.
    const Theme dimmed = make("x", "x", t.day);
    t.night = dimmed.night;
    if (!read_palette(r["night"], t.night, error)) {
        return false;
    }
    out = t;
    return true;
}

bool DayNightSwitch::update(double light)
{
    if (night_ && light > to_day_) {
        night_ = false;
    } else if (!night_ && light < to_night_) {
        night_ = true;
    }
    return night_;
}

} // namespace core::display
