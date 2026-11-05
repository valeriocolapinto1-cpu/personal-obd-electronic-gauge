// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/profiles/profile.h"

#include <cmath>

#include "core/util/json.h"

namespace core::profiles {

using core::json::Value;
using core::signals::PollPriority;
using core::signals::SignalDef;
using core::signals::SignalSource;
using core::signals::SourceType;

namespace {

struct HintName {
    ProtocolHint hint;
    const char *name;
};

constexpr HintName kHints[] = {
    {ProtocolHint::Auto, "auto"},
    {ProtocolHint::Can11Bit500k, "can_11bit_500k"},
    {ProtocolHint::Can29Bit500k, "can_29bit_500k"},
    {ProtocolHint::Can11Bit250k, "can_11bit_250k"},
    {ProtocolHint::Can29Bit250k, "can_29bit_250k"},
    {ProtocolHint::KLineKwpFast, "kline_kwp_fast"},
    {ProtocolHint::KLineIso9141, "kline_iso9141"},
};

bool read_uint(const Value &v, uint32_t max, uint32_t &out)
{
    if (!v.is_number()) {
        return false;
    }
    const double d = v.as_number();
    if (d < 0 || d > max || std::floor(d) != d) {
        return false;
    }
    out = static_cast<uint32_t>(d);
    return true;
}

bool read_string_list(const Value &v, std::vector<std::string> &out, std::string &err, const char *what)
{
    if (!v.valid()) {
        return true; // optional
    }
    if (!v.is_array()) {
        err = std::string(what) + " must be an array of strings";
        return false;
    }
    for (size_t i = 0; i < v.size(); ++i) {
        if (!v.at(i).is_string() || v.at(i).as_string().empty()) {
            err = std::string(what) + " must contain non-empty strings";
            return false;
        }
        out.push_back(v.at(i).as_string());
    }
    return true;
}

bool parse_priority(const Value &v, PollPriority &out)
{
    if (!v.valid()) {
        return true;
    }
    const std::string &s = v.as_string();
    if (s == "fast") {
        out = PollPriority::Fast;
    } else if (s == "normal") {
        out = PollPriority::Normal;
    } else if (s == "slow") {
        out = PollPriority::Slow;
    } else {
        return false;
    }
    return true;
}

bool parse_source(const Value &v, SignalSource &out, std::string &err)
{
    if (!v.is_object()) {
        err = "source must be an object";
        return false;
    }
    out.verified = v["verified"].as_bool(false);
    uint32_t pid = 0;
    if (v["obd_pid"].valid()) {
        if (!read_uint(v["obd_pid"], 0xFF, pid)) {
            err = "obd_pid must be 0..255";
            return false;
        }
        out.type = SourceType::ObdPid;
        out.pid = static_cast<uint8_t>(pid);
        out.verified = true; // standard formula, availability comes from the support bitmap
        return true;
    }
    struct Kind {
        const char *key;
        SourceType type;
    };
    const Kind kinds[] = {
        {"derived", SourceType::Derived}, {"manufacturer", SourceType::Manufacturer}, {"module", SourceType::Module}};
    for (const Kind &k : kinds) {
        if (v[k.key].valid()) {
            if (!v[k.key].is_string() || v[k.key].as_string().empty()) {
                err = std::string(k.key) + " must be a non-empty string";
                return false;
            }
            out.type = k.type;
            out.ref = v[k.key].as_string();
            return true;
        }
    }
    err = "source needs one of obd_pid, derived, manufacturer, module";
    return false;
}

bool parse_threshold(const Value &v, const char *key, bool &has, double &val, std::string &err)
{
    const Value t = v[key];
    if (!t.valid()) {
        return true;
    }
    if (!t.is_number()) {
        err = std::string("alarm.") + key + " must be a number";
        return false;
    }
    has = true;
    val = t.as_number();
    return true;
}

bool parse_signal(const Value &v, SignalDef &out, std::string &err)
{
    if (!v.is_object() || !v["id"].is_string()) {
        err = "signal needs a string id";
        return false;
    }
    const std::string id = v["id"].as_string();
    const SignalDef *base = core::signals::find_builtin(id);
    if (base != nullptr) {
        out = *base;
    } else {
        out = SignalDef{};
        out.id = id;
        out.label = id;
        if (!v["unit"].is_string()) {
            err = "custom signal '" + id + "' needs a unit";
            return false;
        }
    }
    if (v["label"].is_string()) {
        out.label = v["label"].as_string();
    }
    if (v["unit"].is_string()) {
        out.unit = v["unit"].as_string();
    }
    if (v["min"].is_number()) {
        out.min = v["min"].as_number();
    }
    if (v["max"].is_number()) {
        out.max = v["max"].as_number();
    }
    if (out.min >= out.max) {
        err = "signal '" + id + "': min must be < max";
        return false;
    }
    uint32_t dec = 0;
    if (v["decimals"].valid()) {
        if (!read_uint(v["decimals"], 3, dec)) {
            err = "signal '" + id + "': decimals must be 0..3";
            return false;
        }
        out.decimals = static_cast<uint8_t>(dec);
    }
    if (!parse_priority(v["priority"], out.priority)) {
        err = "signal '" + id + "': priority must be fast/normal/slow";
        return false;
    }
    const Value sources = v["sources"];
    if (sources.valid()) {
        if (!sources.is_array() || sources.size() == 0) {
            err = "signal '" + id + "': sources must be a non-empty array";
            return false;
        }
        out.sources.clear();
        for (size_t i = 0; i < sources.size(); ++i) {
            SignalSource s;
            if (!parse_source(sources.at(i), s, err)) {
                err = "signal '" + id + "': " + err;
                return false;
            }
            out.sources.push_back(s);
        }
    }
    if (out.sources.empty()) {
        err = "signal '" + id + "' has no sources";
        return false;
    }
    const Value alarm = v["alarm"];
    if (alarm.valid()) {
        auto &a = out.alarm;
        if (!parse_threshold(alarm, "warn_low", a.has_warn_low, a.warn_low, err) ||
            !parse_threshold(alarm, "warn_high", a.has_warn_high, a.warn_high, err) ||
            !parse_threshold(alarm, "crit_low", a.has_crit_low, a.crit_low, err) ||
            !parse_threshold(alarm, "crit_high", a.has_crit_high, a.crit_high, err)) {
            return false;
        }
        if (alarm["hysteresis"].is_number()) {
            a.hysteresis = alarm["hysteresis"].as_number();
        }
    }
    return true;
}

} // namespace

ProtocolHint parse_protocol_hint(const std::string &s)
{
    for (const HintName &h : kHints) {
        if (s == h.name) {
            return h.hint;
        }
    }
    return ProtocolHint::Auto;
}

const char *to_string(ProtocolHint hint)
{
    for (const HintName &h : kHints) {
        if (h.hint == hint) {
            return h.name;
        }
    }
    return "auto";
}

LoadResult load_profile(const char *text, size_t len, Profile &out)
{
    LoadResult r;
    core::json::Document doc;
    if (!doc.parse(text, len)) {
        r.error = std::string("JSON error at offset ") + std::to_string(doc.error_offset()) + ": " + doc.error();
        return r;
    }
    const Value root = doc.root();
    if (!root.is_object()) {
        r.error = "profile must be a JSON object";
        return r;
    }
    if (root["schema"].as_number(0) != 1) {
        r.error = "unsupported schema (expected 1)";
        return r;
    }
    out = Profile{};
    if (!root["id"].is_string() || root["id"].as_string().empty() || !root["name"].is_string()) {
        r.error = "profile needs string 'id' and 'name'";
        return r;
    }
    out.id = root["id"].as_string();
    out.name = root["name"].as_string();
    out.extension = root["extension"].as_string();
    if (root["protocol"].valid()) {
        const std::string hint = root["protocol"].as_string();
        out.protocol = parse_protocol_hint(hint);
        if (out.protocol == ProtocolHint::Auto && hint != "auto") {
            r.error = "unknown protocol hint '" + hint + "'";
            return r;
        }
    }

    const Value engine = root["engine"];
    if (engine.valid()) {
        uint32_t cyl = 4;
        if (engine["cylinders"].valid() && (!read_uint(engine["cylinders"], 16, cyl) || cyl == 0)) {
            r.error = "engine.cylinders must be 1..16";
            return r;
        }
        out.cylinders = static_cast<uint8_t>(cyl);
        if (engine["fuel"].is_string()) {
            out.fuel = engine["fuel"].as_string();
        }
    }

    const Value match = root["match"];
    if (match.valid()) {
        if (!read_string_list(match["vin_prefixes"], out.match.vin_prefixes, r.error, "match.vin_prefixes") ||
            !read_string_list(match["wmi"], out.match.wmi, r.error, "match.wmi")) {
            return r;
        }
        uint32_t y = 0;
        if (match["year_from"].valid()) {
            if (!read_uint(match["year_from"], 2100, y)) {
                r.error = "match.year_from invalid";
                return r;
            }
            out.match.year_from = static_cast<uint16_t>(y);
        }
        if (match["year_to"].valid()) {
            if (!read_uint(match["year_to"], 2100, y)) {
                r.error = "match.year_to invalid";
                return r;
            }
            out.match.year_to = static_cast<uint16_t>(y);
        }
        out.match.verified = match["verified"].as_bool(false);
    }

    const Value rpm = root["rpm"];
    if (rpm.valid()) {
        uint32_t v = 0;
        if (read_uint(rpm["limiter"], 20000, v)) {
            out.rpm.limiter = static_cast<uint16_t>(v);
        }
        if (read_uint(rpm["ring_start"], 20000, v)) {
            out.rpm.ring_start = static_cast<uint16_t>(v);
        }
        if (read_uint(rpm["scale_max"], 20000, v)) {
            out.rpm.scale_max = static_cast<uint16_t>(v);
        }
        if (out.rpm.ring_start >= out.rpm.limiter || out.rpm.limiter > out.rpm.scale_max) {
            r.error = "rpm: need ring_start < limiter <= scale_max";
            return r;
        }
    }

    const Value signals = root["signals"];
    if (!signals.is_array() || signals.size() == 0) {
        r.error = "profile needs a non-empty 'signals' array";
        return r;
    }
    for (size_t i = 0; i < signals.size(); ++i) {
        SignalDef d;
        if (!parse_signal(signals.at(i), d, r.error)) {
            return r;
        }
        for (const SignalDef &existing : out.signals) {
            if (existing.id == d.id) {
                r.error = "duplicate signal '" + d.id + "'";
                return r;
            }
        }
        out.signals.push_back(std::move(d));
    }
    r.ok = true;
    return r;
}

int match_score(const Profile &p, const core::obd::Vin *vin)
{
    if (vin == nullptr) {
        return 0;
    }
    int score = 0;
    for (const std::string &prefix : p.match.vin_prefixes) {
        if (vin->has_prefix(prefix.c_str())) {
            const int s = 100 + static_cast<int>(prefix.size());
            score = s > score ? s : score;
        }
    }
    if (score == 0) {
        for (const std::string &w : p.match.wmi) {
            if (vin->has_prefix(w.c_str())) {
                score = 10;
                break;
            }
        }
    }
    if (score == 0) {
        return 0;
    }
    if (p.match.year_from != 0 || p.match.year_to != 0) {
        uint16_t years[2];
        const size_t n = core::obd::vin_model_years(vin->year_code(), years);
        bool in_range = false;
        for (size_t i = 0; i < n; ++i) {
            const bool lo = p.match.year_from == 0 || years[i] >= p.match.year_from;
            const bool hi = p.match.year_to == 0 || years[i] <= p.match.year_to;
            in_range = in_range || (lo && hi);
        }
        score = in_range ? score + 5 : score / 2;
    }
    return score;
}

const Profile *select_profile(const std::vector<Profile> &profiles, const core::obd::Vin *vin,
                              const std::string &manual_id)
{
    if (profiles.empty()) {
        return nullptr;
    }
    if (!manual_id.empty()) {
        for (const Profile &p : profiles) {
            if (p.id == manual_id) {
                return &p;
            }
        }
    }
    const Profile *best = nullptr;
    int best_score = 0;
    for (const Profile &p : profiles) {
        const int s = match_score(p, vin);
        if (s > best_score) {
            best_score = s;
            best = &p;
        }
    }
    if (best != nullptr) {
        return best;
    }
    for (const Profile &p : profiles) {
        if (p.id == "generic_obd") {
            return &p;
        }
    }
    return &profiles.front();
}

} // namespace core::profiles
