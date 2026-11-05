// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include "core/transport/can_detect.h"

using namespace core::transport;

TEST(CanDetect, DetectsFirstCandidate)
{
    CanBitrateDetector d;
    d.start();
    EXPECT_EQ(d.current_bitrate(), 500000u);
    EXPECT_EQ(d.feed({120, 0}), DetectState::Detected);
    EXPECT_EQ(d.detected_bitrate(), 500000u);
}

TEST(CanDetect, ErrorsAtWrongBitrateThenDetect250k)
{
    CanBitrateDetector d;
    d.start();
    EXPECT_EQ(d.feed({0, 57}), DetectState::Listening);
    EXPECT_EQ(d.current_bitrate(), 250000u);
    EXPECT_EQ(d.feed({40, 0}), DetectState::Detected);
    EXPECT_EQ(d.detected_bitrate(), 250000u);
}

TEST(CanDetect, SilentBus)
{
    CanBitrateDetector d;
    d.start();
    EXPECT_EQ(d.feed({0, 0}), DetectState::Listening);
    EXPECT_EQ(d.feed({0, 0}), DetectState::Silent);
    EXPECT_EQ(d.feed({100, 0}), DetectState::Silent); // terminal until start()
}

TEST(CanDetect, OnlyErrorsFails)
{
    CanBitrateDetector d;
    d.start();
    d.feed({0, 10});
    EXPECT_EQ(d.feed({0, 0}), DetectState::Failed);
}

TEST(CanDetect, TooFewFramesOrTooManyErrorsAreNotADetection)
{
    CanBitrateDetector d;
    d.start();
    EXPECT_EQ(d.feed({2, 0}), DetectState::Listening); // 2 < kMinValidFrames
    d.start();
    EXPECT_EQ(d.feed({10, 5}), DetectState::Listening); // errors dominate
    d.start();
    EXPECT_EQ(d.feed({100, 3}), DetectState::Detected); // occasional error tolerated
}

TEST(CanDetect, SilentBusPolicyDefaultsToNoTransmit)
{
    EXPECT_EQ(decide_silent_bus(0, false), SilentBusAction::DoNotTransmit);
    EXPECT_EQ(decide_silent_bus(0, true), SilentBusAction::DoNotTransmit);
    EXPECT_EQ(decide_silent_bus(500000, false), SilentBusAction::DoNotTransmit);
    EXPECT_EQ(decide_silent_bus(125000, true), SilentBusAction::DoNotTransmit); // not an OBD bitrate
    EXPECT_EQ(decide_silent_bus(500000, true), SilentBusAction::ProbeAtHint);
    EXPECT_EQ(decide_silent_bus(250000, true), SilentBusAction::ProbeAtHint);
}
