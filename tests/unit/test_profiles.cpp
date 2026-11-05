// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/profiles/profile.h"

using namespace core::profiles;
using core::obd::parse_vin;
using core::obd::Vin;

#ifndef GAUGE_PROFILES_DIR
#error "GAUGE_PROFILES_DIR must point to profiles/"
#endif

namespace {

std::string read_file(const std::string &name)
{
    std::ifstream f(std::string(GAUGE_PROFILES_DIR) + "/" + name);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

Profile load(const std::string &name)
{
    const std::string text = read_file(name);
    Profile p;
    LoadResult r = load_profile(text.c_str(), text.size(), p);
    EXPECT_TRUE(r.ok) << name << ": " << r.error;
    return p;
}

LoadResult try_load(const char *json)
{
    Profile p;
    return load_profile(json, std::strlen(json), p);
}

Vin vin(const char *s)
{
    Vin v;
    EXPECT_EQ(parse_vin(s, 17, v), core::obd::VinStatus::Ok) << s;
    return v;
}

const std::vector<std::string> kShipped = {"generic_obd.json", "vw_scirocco_14tsi_160.json", "vw_golf6_20tdi_140.json",
                                           "hyundai_i20_12_pb.json"};

} // namespace

TEST(Profiles, AllShippedProfilesLoad)
{
    for (const std::string &name : kShipped) {
        Profile p = load(name);
        EXPECT_FALSE(p.signals.empty()) << name;
        for (const auto &s : p.signals) {
            EXPECT_FALSE(s.sources.empty()) << name << " " << s.id;
        }
    }
}

TEST(Profiles, InheritsBuiltinsAndOverrides)
{
    Profile p = load("vw_golf6_20tdi_140.json");
    EXPECT_EQ(p.extension, "vag");
    EXPECT_EQ(p.protocol, ProtocolHint::Can11Bit500k);
    EXPECT_EQ(p.fuel, "diesel");
    EXPECT_EQ(p.rpm.limiter, 4500);
    const core::signals::SignalDef *rpm = nullptr;
    for (const auto &s : p.signals) {
        if (s.id == "engine_rpm") {
            rpm = &s;
        }
    }
    ASSERT_NE(rpm, nullptr);
    EXPECT_DOUBLE_EQ(rpm->max, 5500);
    EXPECT_EQ(rpm->unit, "rpm");
    EXPECT_EQ(rpm->priority, core::signals::PollPriority::Fast);
    ASSERT_EQ(rpm->sources.size(), 1u);
    EXPECT_EQ(rpm->sources[0].pid, 0x0C);
}

TEST(Profiles, ValidationErrors)
{
    EXPECT_FALSE(try_load("{").ok);
    EXPECT_FALSE(try_load(R"({"schema":2,"id":"x","name":"x","signals":[{"id":"engine_rpm"}]})").ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"name":"x","signals":[{"id":"engine_rpm"}]})").ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x","signals":[]})").ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x","signals":[{"id":"custom"}]})").ok); // no unit
    EXPECT_FALSE(
        try_load(R"({"schema":1,"id":"x","name":"x","signals":[{"id":"custom","unit":"V"}]})").ok); // no sources
    EXPECT_FALSE(
        try_load(R"({"schema":1,"id":"x","name":"x","signals":[{"id":"engine_rpm"},{"id":"engine_rpm"}]})").ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x","protocol":"can_1M",
                              "signals":[{"id":"engine_rpm"}]})")
                     .ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x","rpm":{"limiter":3000,"ring_start":4000},
                              "signals":[{"id":"engine_rpm"}]})")
                     .ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x",
                              "signals":[{"id":"engine_rpm","sources":[{"obd_pid":300}]}]})")
                     .ok);
    EXPECT_FALSE(try_load(R"({"schema":1,"id":"x","name":"x",
                              "signals":[{"id":"engine_rpm","alarm":{"warn_high":"hot"}}]})")
                     .ok);

    const LoadResult ok = try_load(R"({"schema":1,"id":"x","name":"x",
        "signals":[{"id":"egt","unit":"degC","min":0,"max":1000,"sources":[{"module":"egt0"}]}]})");
    EXPECT_TRUE(ok.ok) << ok.error;
}

TEST(Profiles, MatchingByVin)
{
    std::vector<Profile> all;
    for (const std::string &name : kShipped) {
        all.push_back(load(name));
    }
    // Placeholder VINs (not real cars). Position 10: 'A' = 2010, '8' = 2008.
    const Vin scirocco = vin("WVWZZZ13ZAV000001");
    const Vin golf = vin("WVWZZZ1KZAW000001");
    const Vin other_vw = vin("WVWZZZ3CZ8E000001");
    const Vin unknown = vin("ZFA19900000000001");

    EXPECT_EQ(select_profile(all, &scirocco, "")->id, "vw_scirocco_14tsi_160");
    EXPECT_EQ(select_profile(all, &golf, "")->id, "vw_golf6_20tdi_140");
    // Another VW: WMI match only (both VAG profiles score equally; the first one wins)
    EXPECT_NE(select_profile(all, &other_vw, "")->id, "generic_obd");
    EXPECT_EQ(select_profile(all, &unknown, "")->id, "generic_obd");
    EXPECT_EQ(select_profile(all, nullptr, "")->id, "generic_obd");
    // Manual selection always wins
    EXPECT_EQ(select_profile(all, &golf, "hyundai_i20_12_pb")->id, "hyundai_i20_12_pb");
    EXPECT_EQ(select_profile(all, &golf, "does_not_exist")->id, "vw_golf6_20tdi_140");
}

TEST(Profiles, ModelYearWindowLowersScore)
{
    Profile p = load("vw_scirocco_14tsi_160.json");
    const Vin in_range = vin("WVWZZZ13ZAV000001");  // 2010
    const Vin out_range = vin("WVWZZZ13Z1V000001"); // 2001 / 2031
    EXPECT_GT(match_score(p, &in_range), match_score(p, &out_range));
    EXPECT_GT(match_score(p, &out_range), 0);
}
