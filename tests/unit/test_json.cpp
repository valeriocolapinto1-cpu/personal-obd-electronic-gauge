// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "core/util/json.h"

using core::json::Document;
using core::json::Type;

namespace {
bool parse(Document &d, const char *s)
{
    return d.parse(s, std::strlen(s));
}
} // namespace

TEST(Json, ParsesNestedDocument)
{
    Document d;
    ASSERT_TRUE(parse(d, R"( {"a": 1, "b": [true, false, null, -2.5e2, "x\"y\u00e8"], "c": {"d": "e"}} )"));
    auto r = d.root();
    ASSERT_TRUE(r.is_object());
    EXPECT_EQ(r.size(), 3u);
    EXPECT_DOUBLE_EQ(r["a"].as_number(), 1.0);
    auto b = r["b"];
    ASSERT_TRUE(b.is_array());
    EXPECT_EQ(b.size(), 5u);
    EXPECT_TRUE(b.at(0).as_bool());
    EXPECT_FALSE(b.at(1).as_bool(true));
    EXPECT_TRUE(b.at(2).is_null());
    EXPECT_DOUBLE_EQ(b.at(3).as_number(), -250.0);
    EXPECT_EQ(b.at(4).as_string(), "x\"y\xc3\xa8");
    EXPECT_EQ(r["c"]["d"].as_string(), "e");
    EXPECT_EQ(r.key_at(2), "c");
}

TEST(Json, MissingMembersAreInvalidAndSafe)
{
    Document d;
    ASSERT_TRUE(parse(d, R"({"a": 1})"));
    auto missing = d.root()["nope"];
    EXPECT_FALSE(missing.valid());
    EXPECT_EQ(missing.as_string(), "");
    EXPECT_DOUBLE_EQ(missing.as_number(7), 7.0);
    EXPECT_FALSE(missing["deeper"].valid());
    EXPECT_FALSE(d.root().at(0).valid());
}

TEST(Json, RejectsInvalidInput)
{
    const char *bad[] = {"",    "{",    "{\"a\" 1}", "[1,]",     "{\"a\":1,}", "01",     "1.", "-",   "\"abc",
                         "tru", "{} x", "\"\\q\"",   "\"a\nb\"", "[1 2]",      "{1: 2}", "1e", "nul", "\"\\u12\""};
    for (const char *s : bad) {
        Document d;
        EXPECT_FALSE(parse(d, s)) << s;
    }
}

TEST(Json, DepthLimit)
{
    std::string deep(Document::kMaxDepth + 2, '[');
    deep += std::string(Document::kMaxDepth + 2, ']');
    Document d;
    EXPECT_FALSE(d.parse(deep.c_str(), deep.size()));
    std::string ok(Document::kMaxDepth, '[');
    ok += std::string(Document::kMaxDepth, ']');
    EXPECT_TRUE(d.parse(ok.c_str(), ok.size()));
}

TEST(Json, ErrorOffset)
{
    Document d;
    EXPECT_FALSE(parse(d, "{\"a\": x}"));
    EXPECT_EQ(d.error_offset(), 6u);
}
