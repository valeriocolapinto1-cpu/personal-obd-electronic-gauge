// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "core/obd/obd.h"

using namespace core::obd;

namespace {
double decode(uint8_t p, std::vector<uint8_t> data)
{
    const PidInfo *info = find_pid(p);
    EXPECT_NE(info, nullptr);
    double v = -9999;
    EXPECT_TRUE(info->decode(data.data(), v));
    return v;
}
} // namespace

TEST(ObdPid, StandardFormulas)
{
    EXPECT_DOUBLE_EQ(decode(pid::kCoolantTemp, {0x00}), -40.0);
    EXPECT_DOUBLE_EQ(decode(pid::kCoolantTemp, {0x7B}), 83.0);
    EXPECT_DOUBLE_EQ(decode(pid::kCoolantTemp, {0xFF}), 215.0);
    EXPECT_DOUBLE_EQ(decode(pid::kIntakeAirTemp, {0x46}), 30.0);
    EXPECT_DOUBLE_EQ(decode(pid::kOilTemp, {0x82}), 90.0);
    EXPECT_DOUBLE_EQ(decode(pid::kEngineRpm, {0x1A, 0xF8}), 1726.0);
    EXPECT_DOUBLE_EQ(decode(pid::kEngineRpm, {0xFF, 0xFF}), 16383.75);
    EXPECT_DOUBLE_EQ(decode(pid::kIntakeMap, {0xFF}), 255.0); // saturation point
    EXPECT_DOUBLE_EQ(decode(pid::kBarometric, {0x65}), 101.0);
    EXPECT_DOUBLE_EQ(decode(pid::kVehicleSpeed, {0x64}), 100.0);
    EXPECT_DOUBLE_EQ(decode(pid::kMafRate, {0x01, 0xF4}), 5.0);
    EXPECT_DOUBLE_EQ(decode(pid::kModuleVoltage, {0x37, 0x6E}), 14.19);
    EXPECT_DOUBLE_EQ(decode(pid::kFuelRate, {0x00, 0x64}), 5.0);
    EXPECT_DOUBLE_EQ(decode(pid::kThrottle, {0xFF}), 100.0);
    EXPECT_DOUBLE_EQ(decode(pid::kEngineLoad, {0x00}), 0.0);
}

TEST(ObdPid, ExtendedMapUsesSensorAAndSupportBit)
{
    // support A, sensor A = 0x2710 / 32 = 312.5 kPa (beyond the 255 kPa limit of PID 0x0B)
    EXPECT_DOUBLE_EQ(decode(pid::kIntakeMapExtended, {0x01, 0x27, 0x10, 0x00, 0x00}), 312.5);
    const uint8_t not_supported[] = {0x02, 0x27, 0x10, 0x00, 0x00};
    double v = 0;
    EXPECT_FALSE(find_pid(pid::kIntakeMapExtended)->decode(not_supported, v));
}

TEST(ObdPid, UnknownPid)
{
    EXPECT_EQ(find_pid(0xFE), nullptr);
}

TEST(ObdRequest, Build)
{
    uint8_t buf[4];
    ASSERT_EQ(build_request_01(0x0C, buf, sizeof(buf)), 2u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x0C);
    ASSERT_EQ(build_request_09(kInfoTypeVin, buf, sizeof(buf)), 2u);
    EXPECT_EQ(buf[0], 0x09);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(build_request_01(0x0C, buf, 1), 0u);
}

TEST(ObdResponse, ValidatesServiceAndPidEcho)
{
    const uint8_t ok[] = {0x41, 0x0C, 0x1A, 0xF8};
    ParsedResponse r = parse_response_01(0x0C, ok, sizeof(ok));
    ASSERT_EQ(r.kind, ResponseKind::Positive);
    EXPECT_EQ(r.data_len, 2u);
    EXPECT_EQ(r.data[0], 0x1A);

    // Answer to another dongle's request for speed: must be discarded.
    const uint8_t other_pid[] = {0x41, 0x0D, 0x32};
    EXPECT_EQ(parse_response_01(0x0C, other_pid, sizeof(other_pid)).kind, ResponseKind::Mismatch);
    const uint8_t other_sid[] = {0x49, 0x0C, 0x00, 0x00};
    EXPECT_EQ(parse_response_01(0x0C, other_sid, sizeof(other_sid)).kind, ResponseKind::Mismatch);

    const uint8_t truncated[] = {0x41, 0x0C, 0x1A};
    EXPECT_EQ(parse_response_01(0x0C, truncated, sizeof(truncated)).kind, ResponseKind::Malformed);

    const uint8_t neg[] = {0x7F, 0x01, 0x12};
    r = parse_response_01(0x0C, neg, sizeof(neg));
    EXPECT_EQ(r.kind, ResponseKind::Negative);
    EXPECT_EQ(r.nrc, 0x12);
    const uint8_t neg_other[] = {0x7F, 0x22, 0x31};
    EXPECT_EQ(parse_response_01(0x0C, neg_other, sizeof(neg_other)).kind, ResponseKind::Mismatch);

    EXPECT_EQ(parse_response_01(0x0C, nullptr, 0).kind, ResponseKind::Malformed);
}

TEST(ObdResponse, SupportBitmap)
{
    bool supported[256] = {};
    // 0xBE1FA813: the classic example from SAE J1979 documentation
    const uint8_t d[] = {0xBE, 0x1F, 0xA8, 0x13};
    const bool more = decode_support_bitmap(0x00, d, supported);
    EXPECT_TRUE(more);
    const std::vector<int> expected = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E,
                                       0x0F, 0x10, 0x11, 0x13, 0x15, 0x1C, 0x1F, 0x20};
    for (int p = 1; p <= 0x20; ++p) {
        const bool want = std::find(expected.begin(), expected.end(), p) != expected.end();
        EXPECT_EQ(supported[p], want) << "pid 0x" << std::hex << p;
    }
}

TEST(ObdResponse, SupportBitmapLastRange)
{
    bool supported[256] = {};
    const uint8_t d[] = {0x00, 0x00, 0x00, 0x01};
    EXPECT_TRUE(decode_support_bitmap(0xE0, d, supported)); // bit for 0x100 is out of range, no overflow
}
