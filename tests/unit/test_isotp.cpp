// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <vector>

#include "core/transport/isotp.h"

using core::bus::CanFrame;
using core::transport::ICanFrameSink;
using core::transport::IsoTpChannel;
using core::transport::IsoTpConfig;
using core::transport::IsoTpError;

namespace {

class FrameLog : public ICanFrameSink {
public:
    bool send_frame(const CanFrame &f) override
    {
        frames.push_back(f);
        return ok;
    }
    std::vector<CanFrame> frames;
    bool ok = true;
};

CanFrame frame(uint32_t id, std::vector<uint8_t> bytes)
{
    while (bytes.size() < 8) {
        bytes.push_back(0xAA);
    }
    return CanFrame::make(id, false, bytes.data(), 8);
}

std::vector<uint8_t> payload(const IsoTpChannel &ch)
{
    return std::vector<uint8_t>(ch.message(), ch.message() + ch.message_len());
}

struct Fixture : ::testing::Test {
    FrameLog log;
    IsoTpConfig cfg;
    IsoTpChannel ch{log, cfg};
};

} // namespace

TEST_F(Fixture, SingleFrameTransmitIsPaddedToEightBytes)
{
    const uint8_t req[] = {0x01, 0x0C};
    ASSERT_TRUE(ch.transmit(req, sizeof(req)));
    ASSERT_EQ(log.frames.size(), 1u);
    const CanFrame &f = log.frames[0];
    EXPECT_EQ(f.id, 0x7E0u);
    EXPECT_EQ(f.dlc, 8);
    EXPECT_EQ(f.data[0], 0x02);
    EXPECT_EQ(f.data[1], 0x01);
    EXPECT_EQ(f.data[2], 0x0C);
    for (int i = 3; i < 8; ++i) {
        EXPECT_EQ(f.data[i], 0x55);
    }
    EXPECT_FALSE(ch.tx_busy());
}

TEST_F(Fixture, FunctionalTxId)
{
    ch.set_tx_id(0x7DF);
    const uint8_t req[] = {0x01, 0x00};
    ASSERT_TRUE(ch.transmit(req, sizeof(req)));
    EXPECT_EQ(log.frames.at(0).id, 0x7DFu);
}

TEST_F(Fixture, ReceiveSingleFrame)
{
    ch.on_frame(frame(0x7E8, {0x04, 0x41, 0x0C, 0x1A, 0xF8}), 0);
    ASSERT_TRUE(ch.has_message());
    EXPECT_EQ(payload(ch), (std::vector<uint8_t>{0x41, 0x0C, 0x1A, 0xF8}));
    ch.consume();
    EXPECT_FALSE(ch.has_message());
}

TEST_F(Fixture, IgnoresOtherIdsAndInvalidSingleFrames)
{
    ch.on_frame(frame(0x7E9, {0x02, 0x41, 0x0C}), 0); // other ECU
    ch.on_frame(frame(0x7E8, {0x00, 0x41}), 0);       // length 0
    ch.on_frame(frame(0x7E8, {0x08, 0x41}), 0);       // length 8 in a SF
    EXPECT_FALSE(ch.has_message());
    CanFrame short_frame = frame(0x7E8, {0x05, 0x41, 0x0C});
    short_frame.dlc = 3; // claims 5 bytes, carries 2
    ch.on_frame(short_frame, 0);
    EXPECT_FALSE(ch.has_message());
}

TEST_F(Fixture, ReceiveMultiFrameVinAndSendFlowControl)
{
    // 49 02 01 + 17 VIN chars = 20 bytes
    const std::string vin = "WVWZZZ13ZAV000000";
    std::vector<uint8_t> msg = {0x49, 0x02, 0x01};
    msg.insert(msg.end(), vin.begin(), vin.end());
    ASSERT_EQ(msg.size(), 20u);

    ch.on_frame(frame(0x7E8, {0x10, 0x14, msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]}), 10);
    ASSERT_EQ(log.frames.size(), 1u);
    const CanFrame &fc = log.frames[0];
    EXPECT_EQ(fc.id, 0x7E0u);
    EXPECT_EQ(fc.data[0], 0x30);
    EXPECT_EQ(fc.data[1], 0x00);
    EXPECT_EQ(fc.data[2], 0x00);
    EXPECT_TRUE(ch.rx_busy());

    ch.on_frame(frame(0x7E8, {0x21, msg[6], msg[7], msg[8], msg[9], msg[10], msg[11], msg[12]}), 11);
    EXPECT_FALSE(ch.has_message());
    ch.on_frame(frame(0x7E8, {0x22, msg[13], msg[14], msg[15], msg[16], msg[17], msg[18], msg[19]}), 12);
    ASSERT_TRUE(ch.has_message());
    EXPECT_EQ(payload(ch), msg);
}

TEST_F(Fixture, WrongSequenceNumberAborts)
{
    ch.on_frame(frame(0x7E8, {0x10, 0x10, 1, 2, 3, 4, 5, 6}), 0);
    ch.on_frame(frame(0x7E8, {0x22, 7, 8, 9, 10, 11, 12, 13}), 1);
    EXPECT_FALSE(ch.has_message());
    EXPECT_FALSE(ch.rx_busy());
    EXPECT_EQ(ch.take_error(), IsoTpError::WrongSequence);
}

TEST_F(Fixture, ConsecutiveFrameTimeout)
{
    ch.on_frame(frame(0x7E8, {0x10, 0x10, 1, 2, 3, 4, 5, 6}), 0);
    ch.poll(999);
    EXPECT_TRUE(ch.rx_busy());
    ch.poll(1000);
    EXPECT_FALSE(ch.rx_busy());
    EXPECT_EQ(ch.take_error(), IsoTpError::TimeoutCr);
}

TEST_F(Fixture, UnsolicitedConsecutiveFrameIgnored)
{
    ch.on_frame(frame(0x7E8, {0x21, 1, 2, 3, 4, 5, 6, 7}), 0);
    EXPECT_FALSE(ch.has_message());
    EXPECT_EQ(ch.take_error(), IsoTpError::None);
}

TEST_F(Fixture, FirstFrameTooLargeAnswersOverflow)
{
    // 0xFFF = 4095 bytes > kMaxMessage
    ch.on_frame(frame(0x7E8, {0x1F, 0xFF, 1, 2, 3, 4, 5, 6}), 0);
    ASSERT_EQ(log.frames.size(), 1u);
    EXPECT_EQ(log.frames[0].data[0], 0x32);
    EXPECT_EQ(ch.take_error(), IsoTpError::Overflow);
}

TEST_F(Fixture, MultiFrameTransmitHonoursFlowControlBlockSizeAndStmin)
{
    std::vector<uint8_t> msg(20);
    for (size_t i = 0; i < msg.size(); ++i) {
        msg[i] = static_cast<uint8_t>(i);
    }
    ch.poll(100);
    ASSERT_TRUE(ch.transmit(msg.data(), msg.size()));
    ASSERT_EQ(log.frames.size(), 1u);
    EXPECT_EQ(log.frames[0].data[0], 0x10);
    EXPECT_EQ(log.frames[0].data[1], 20);
    EXPECT_TRUE(ch.tx_busy());

    // FC: CTS, BS=1, STmin=5 ms
    ch.on_frame(frame(0x7E8, {0x30, 0x01, 0x05}), 110);
    ASSERT_EQ(log.frames.size(), 2u);
    EXPECT_EQ(log.frames[1].data[0], 0x21);
    // Block of 1 done: waits for the next FC
    ch.poll(200);
    EXPECT_EQ(log.frames.size(), 2u);
    ch.on_frame(frame(0x7E8, {0x30, 0x00, 0x05}), 210);
    ASSERT_EQ(log.frames.size(), 3u);
    EXPECT_EQ(log.frames[2].data[0], 0x22);
    ch.poll(214);
    EXPECT_EQ(log.frames.size(), 3u); // STmin not elapsed
    ch.poll(215);
    ASSERT_EQ(log.frames.size(), 3u); // 6 + 7 + 7 = 20: done after the second CF
    EXPECT_FALSE(ch.tx_busy());
}

TEST_F(Fixture, FlowControlTimeoutAndWaitAndOverflow)
{
    std::vector<uint8_t> msg(10, 0x11);
    ch.poll(0);
    ASSERT_TRUE(ch.transmit(msg.data(), msg.size()));
    EXPECT_FALSE(ch.transmit(msg.data(), msg.size()));
    EXPECT_EQ(ch.take_error(), IsoTpError::Busy);

    ch.on_frame(frame(0x7E8, {0x31, 0, 0}), 900); // WAIT restarts N_Bs
    ch.poll(1500);
    EXPECT_TRUE(ch.tx_busy());
    ch.poll(1900);
    EXPECT_FALSE(ch.tx_busy());
    EXPECT_EQ(ch.take_error(), IsoTpError::TimeoutBs);

    ASSERT_TRUE(ch.transmit(msg.data(), msg.size()));
    ch.on_frame(frame(0x7E8, {0x32, 0, 0}), 1950);
    EXPECT_FALSE(ch.tx_busy());
    EXPECT_EQ(ch.take_error(), IsoTpError::Overflow);
}

TEST_F(Fixture, SequenceNumberWrapsAfterFifteen)
{
    // 6 + 16*7 = 118 bytes -> CF SN 1..15, 0
    const size_t len = 118;
    std::vector<uint8_t> msg(len);
    for (size_t i = 0; i < len; ++i) {
        msg[i] = static_cast<uint8_t>(i);
    }
    ch.on_frame(frame(0x7E8, {0x10, static_cast<uint8_t>(len), msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]}), 0);
    size_t pos = 6;
    uint8_t sn = 1;
    while (pos < len) {
        std::vector<uint8_t> cf = {static_cast<uint8_t>(0x20 | sn)};
        for (int i = 0; i < 7 && pos < len; ++i) {
            cf.push_back(msg[pos++]);
        }
        ch.on_frame(frame(0x7E8, cf), 1);
        sn = static_cast<uint8_t>((sn + 1) & 0x0F);
    }
    ASSERT_TRUE(ch.has_message());
    EXPECT_EQ(payload(ch), msg);
}
