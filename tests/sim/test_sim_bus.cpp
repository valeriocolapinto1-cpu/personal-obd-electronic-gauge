// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <vector>

#include "sim/fake_obd_ecu.h"
#include "sim/foreign_dongle.h"
#include "sim/virtual_can_bus.h"

using core::bus::CanFrame;
using sim::FakeObdEcu;
using sim::PidBehaviour;
using sim::PidScript;
using sim::VirtualCanBus;

namespace {

/// Captures every frame it sees: stands in for the device under test.
class Probe : public sim::INode {
public:
    void on_frame(VirtualCanBus &, const CanFrame &f) override { rx.push_back(f); }
    std::vector<CanFrame> rx;
};

CanFrame obd_request(uint8_t pid, uint32_t id = 0x7DF)
{
    const uint8_t d[] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    return CanFrame::make(id, false, d, 8);
}

} // namespace

TEST(SimBus, FakeEcuAnswersScriptedPidAfterDelay)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Probe probe;
    bus.attach(&ecu);
    bus.attach(&probe);
    ecu.set_pid(0x0C, PidScript{{0x1A, 0xF8}, PidBehaviour::Respond, 0, 10});

    bus.send(&probe, obd_request(0x0C));
    bus.advance(9);
    EXPECT_TRUE(probe.rx.empty());
    bus.advance(1);
    ASSERT_EQ(probe.rx.size(), 1u);
    const CanFrame &r = probe.rx[0];
    EXPECT_EQ(r.id, 0x7E8u);
    EXPECT_EQ(r.data[0], 0x04);
    EXPECT_EQ(r.data[1], 0x41);
    EXPECT_EQ(r.data[2], 0x0C);
    EXPECT_EQ(r.data[3], 0x1A);
    EXPECT_EQ(r.data[4], 0xF8);
}

TEST(SimBus, SupportBitmapReflectsScript)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Probe probe;
    bus.attach(&ecu);
    bus.attach(&probe);
    ecu.set_pid(0x05, {0x7B});       // coolant
    ecu.set_pid(0x0C, {0x00, 0x00}); // rpm
    ecu.set_pid(0x33, {0x64});       // baro (next range)

    bus.send(&probe, obd_request(0x00));
    bus.advance(10);
    ASSERT_EQ(probe.rx.size(), 1u);
    // PID 0x05 -> bit 27, PID 0x0C -> bit 20, more ranges -> bit 0
    const uint32_t bits = (uint32_t(probe.rx[0].data[3]) << 24) | (uint32_t(probe.rx[0].data[4]) << 16) |
                          (uint32_t(probe.rx[0].data[5]) << 8) | probe.rx[0].data[6];
    EXPECT_EQ(bits, (1u << 27) | (1u << 20) | 1u);
}

TEST(SimBus, TimeoutAndNegativeResponse)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Probe probe;
    bus.attach(&ecu);
    bus.attach(&probe);
    ecu.set_pid(0x5C, PidScript{{}, PidBehaviour::Ignore});
    ecu.set_pid(0x5E, PidScript{{}, PidBehaviour::NegativeResponse, 0x31});

    bus.send(&probe, obd_request(0x5C));
    bus.advance(100);
    EXPECT_TRUE(probe.rx.empty());

    bus.send(&probe, obd_request(0x5E));
    bus.advance(10);
    ASSERT_EQ(probe.rx.size(), 1u);
    EXPECT_EQ(probe.rx[0].data[1], 0x7F);
    EXPECT_EQ(probe.rx[0].data[2], 0x01);
    EXPECT_EQ(probe.rx[0].data[3], 0x31);
}

TEST(SimBus, ForeignDongleGeneratesUnsolicitedTraffic)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    sim::ForeignDongle dongle({0x0D, 0x05}, 50);
    Probe probe;
    bus.attach(&ecu);
    bus.attach(&dongle);
    bus.attach(&probe);
    ecu.set_pid(0x0D, {0x32});
    ecu.set_pid(0x05, {0x5A});

    bus.advance(510); // requests at 50..500 ms, last answer at 505 ms
    EXPECT_EQ(dongle.requests_sent(), 10u);
    // The probe sees the dongle's requests and the ECU's answers to them.
    size_t answers = 0;
    for (const CanFrame &f : probe.rx) {
        if (f.id == 0x7E8) {
            ++answers;
        }
    }
    EXPECT_EQ(answers, 10u);
}
