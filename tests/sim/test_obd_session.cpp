// SPDX-License-Identifier: GPL-3.0-or-later
//
// End-to-end tests: ObdSession (device under test) against the simulated car.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/obd/obd_session.h"
#include "core/signals/signal.h"
#include "core/signals/signal_store.h"
#include "sim/fake_obd_ecu.h"
#include "sim/foreign_dongle.h"
#include "sim/virtual_can_bus.h"

using core::bus::CanFrame;
using core::obd::ObdSession;
using core::obd::SessionState;
using core::signals::SignalDef;
using core::signals::SignalStore;
using sim::FakeObdEcu;
using sim::PidBehaviour;
using sim::PidScript;
using sim::VirtualCanBus;

namespace {

const char *kVin = "WVWZZZ13ZAV000001"; // placeholder, not a real car

/// The gauge on the simulated bus.
class Tester : public sim::INode, public core::transport::ICanFrameSink {
public:
    explicit Tester(VirtualCanBus &bus, const core::obd::SessionConfig &cfg = core::obd::SessionConfig())
        : bus_(bus), session(*this, store, cfg)
    {}
    bool send_frame(const CanFrame &f) override
    {
        sent.push_back(f);
        bus_.send(this, f);
        return true;
    }
    void on_frame(VirtualCanBus &b, const CanFrame &f) override { session.on_frame(f, b.now_ms()); }
    void on_tick(VirtualCanBus &, uint32_t now) override { session.poll(now); }

    VirtualCanBus &bus_;
    SignalStore store;
    ObdSession session;
    std::vector<CanFrame> sent;
};

std::vector<SignalDef> signals(std::initializer_list<const char *> ids)
{
    std::vector<SignalDef> v;
    for (const char *id : ids) {
        const SignalDef *d = core::signals::find_builtin(id);
        EXPECT_NE(d, nullptr) << id;
        v.push_back(*d);
    }
    return v;
}

void script_petrol_car(FakeObdEcu &ecu)
{
    ecu.set_pid(0x05, {0x7B});       // coolant 83 degC
    ecu.set_pid(0x0C, {0x1A, 0xF8}); // 1726 rpm
    ecu.set_pid(0x0B, {0xC8});       // MAP 200 kPa
    ecu.set_pid(0x0F, {0x46});       // IAT 30 degC
    ecu.set_pid(0x33, {0x65});       // baro 101 kPa
    ecu.set_vin(kVin);
}

/// Runs until the session waits for signals (or gives up), then installs `defs`.
void bring_up(VirtualCanBus &bus, Tester &t, const std::vector<SignalDef> &defs)
{
    t.session.start(bus.now_ms());
    for (int i = 0; i < 3000 && t.session.state() != SessionState::WaitingForSignals; ++i) {
        bus.advance(1);
    }
    ASSERT_EQ(t.session.state(), SessionState::WaitingForSignals);
    t.session.set_signals(defs, bus.now_ms());
}

} // namespace

TEST(ObdSession, DiscoversReadsVinAndPolls)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);

    const auto defs = signals({"engine_rpm", "coolant_temp", "intake_map", "barometric", "boost", "oil_temp"});
    bring_up(bus, t, defs);
    EXPECT_FALSE(t.session.extended_ids());
    EXPECT_EQ(t.session.ecu_response_id(), 0x7E8u);
    ASSERT_TRUE(t.session.has_vin());
    EXPECT_STREQ(t.session.vin().text, kVin);
    EXPECT_TRUE(t.session.pid_supported(0x0C));
    EXPECT_FALSE(t.session.pid_supported(0x5C));
    EXPECT_EQ(t.session.slot_pid(5), 0);    // oil temp: PID 0x5C not supported, not polled
    EXPECT_EQ(t.session.slot_pid(2), 0x0B); // 0x87 not supported -> falls back to 0x0B

    bus.advance(2000);
    ASSERT_EQ(t.session.state(), SessionState::Polling);
    const uint32_t now = bus.now_ms();
    EXPECT_TRUE(t.store.fresh(0, now, 200));
    EXPECT_DOUBLE_EQ(t.store.read(0).value, 1726.0);
    EXPECT_DOUBLE_EQ(t.store.read(1).value, 83.0);
    EXPECT_DOUBLE_EQ(t.store.read(2).value, 200.0);
    EXPECT_DOUBLE_EQ(t.store.read(3).value, 101.0);
    EXPECT_DOUBLE_EQ(t.store.read(4).value, 99.0); // boost = MAP - baro
    EXPECT_FALSE(t.store.read(5).valid);
    EXPECT_EQ(t.session.gate_stats().rejected, 0u);
}

TEST(ObdSession, EveryTransmittedFrameIsAnAllowedRequestOrFlowControl)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    bring_up(bus, t, signals({"engine_rpm", "coolant_temp"}));
    bus.advance(3000);

    ASSERT_FALSE(t.sent.empty());
    for (const CanFrame &f : t.sent) {
        const uint8_t type = f.data[0] & 0xF0;
        if (type == 0x30) {
            EXPECT_EQ(f.id, 0x7E0u); // flow control to the ECU we are reading from
            continue;
        }
        ASSERT_EQ(type, 0x00) << "only single-frame requests are expected";
        const uint8_t sid = f.data[1];
        EXPECT_TRUE(sid == 0x01 || sid == 0x09) << "sid 0x" << std::hex << int(sid);
        EXPECT_TRUE(f.id == 0x7DF || f.id == 0x7E0) << std::hex << f.id;
    }
}

TEST(ObdSession, RespectsGlobalRequestRate)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    ecu.set_pid(0x0D, {0x40});
    ecu.set_pid(0x11, {0x40});
    ecu.set_pid(0x04, {0x40});
    // Many fast signals: the scheduler must cap the total rate.
    auto defs = signals({"engine_rpm", "intake_map", "vehicle_speed", "throttle", "engine_load"});
    for (auto &d : defs) {
        d.priority = core::signals::PollPriority::Fast;
    }
    bring_up(bus, t, defs);
    bus.advance(5000);
    EXPECT_LE(ecu.max_requests_per_second(), 50u);
    EXPECT_GE(ecu.max_requests_per_second(), 40u);
}

TEST(ObdSession, ForeignDongleTrafficIsIgnored)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    sim::ForeignDongle dongle({0x0D, 0x05, 0x0C}, 37);
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&dongle);
    bus.attach(&t);
    script_petrol_car(ecu);
    ecu.set_pid(0x0C, {0x2E, 0xE0}); // 3000 rpm
    ecu.set_pid(0x0D, {0x64});       // 100 km/h (only the dongle asks for it)

    bring_up(bus, t, signals({"engine_rpm", "coolant_temp"}));
    bus.advance(5000);
    EXPECT_EQ(t.session.state(), SessionState::Polling);
    EXPECT_DOUBLE_EQ(t.store.read(0).value, 3000.0);
    EXPECT_DOUBLE_EQ(t.store.read(1).value, 83.0);
    EXPECT_GT(t.session.stats().mismatched, 0u); // the dongle's answers were seen and dropped
}

TEST(ObdSession, NegativeResponseDisablesPidTimeoutsBackOff)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    ecu.set_pid(0x0F, PidScript{{}, PidBehaviour::NegativeResponse, 0x31});
    ecu.set_pid(0x05, PidScript{{}, PidBehaviour::Ignore});
    bring_up(bus, t, signals({"engine_rpm", "intake_air_temp", "coolant_temp"}));
    bus.advance(10000);
    EXPECT_EQ(t.session.state(), SessionState::Polling);
    EXPECT_TRUE(t.store.fresh(0, bus.now_ms(), 200));
    EXPECT_FALSE(t.store.read(1).valid);
    EXPECT_FALSE(t.store.read(2).valid);
    EXPECT_GE(t.session.stats().negative, 1u);
    EXPECT_GT(t.session.stats().timeouts, 0u);
    EXPECT_LT(t.session.stats().timeouts, 20u); // backed off, not hammered at 1 Hz+
}

TEST(ObdSession, BusLossInvalidatesValues)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    bring_up(bus, t, signals({"engine_rpm", "coolant_temp"}));
    bus.advance(1000);
    EXPECT_TRUE(t.store.read(0).valid);
    ecu.set_online(false);
    bus.advance(60000);
    EXPECT_EQ(t.session.state(), SessionState::Lost);
    EXPECT_FALSE(t.store.read(0).valid);
    EXPECT_FALSE(t.store.read(1).valid);
}

TEST(ObdSession, Uses29BitIdsWhen11BitIsSilent)
{
    VirtualCanBus bus;
    FakeObdEcu ecu(0x10, true);
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    bring_up(bus, t, signals({"engine_rpm"}));
    EXPECT_TRUE(t.session.extended_ids());
    EXPECT_EQ(t.session.ecu_response_id(), 0x18DAF110u);
    EXPECT_TRUE(t.session.has_vin());
    bus.advance(1000);
    EXPECT_DOUBLE_EQ(t.store.read(0).value, 1726.0);
}

TEST(ObdSession, NoEcuMeansNoResponse)
{
    VirtualCanBus bus;
    Tester t(bus);
    bus.attach(&t);
    t.session.start(0);
    bus.advance(1000);
    EXPECT_EQ(t.session.state(), SessionState::NoResponse);
    EXPECT_EQ(t.sent.size(), 2u); // one functional 01 00 on 11-bit, one on 29-bit
}

TEST(ObdSession, PrefersEngineEcuAndWorksWithoutVin)
{
    VirtualCanBus bus;
    FakeObdEcu engine(0);
    FakeObdEcu gearbox(1);
    Tester t(bus);
    bus.attach(&gearbox);
    bus.attach(&engine);
    bus.attach(&t);
    engine.set_pid(0x0C, {0x0F, 0xA0}); // 1000 rpm, no VIN
    gearbox.set_pid(0x0D, {0x10});
    bring_up(bus, t, signals({"engine_rpm"}));
    EXPECT_EQ(t.session.ecu_response_id(), 0x7E8u);
    EXPECT_FALSE(t.session.has_vin());
    bus.advance(500);
    EXPECT_DOUBLE_EQ(t.store.read(0).value, 1000.0);
}

TEST(ObdSession, MapSaturationFlag)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    ecu.set_pid(0x0B, {0xFF});
    bring_up(bus, t, signals({"intake_map", "barometric", "boost"}));
    bus.advance(2000);
    EXPECT_TRUE(t.store.read(0).flags & core::signals::kFlagSaturated);
    EXPECT_TRUE(t.store.read(2).flags & core::signals::kFlagSaturated);
    EXPECT_DOUBLE_EQ(t.store.read(2).value, 154.0);
}

TEST(ObdSession, ExtendedMapPidPreferredWhenSupported)
{
    VirtualCanBus bus;
    FakeObdEcu ecu;
    Tester t(bus);
    bus.attach(&ecu);
    bus.attach(&t);
    script_petrol_car(ecu);
    ecu.set_pid(0x87, {0x01, 0x27, 0x10, 0x00, 0x00}); // 312.5 kPa
    bring_up(bus, t, signals({"intake_map"}));
    EXPECT_EQ(t.session.slot_pid(0), 0x87);
    bus.advance(1000);
    EXPECT_DOUBLE_EQ(t.store.read(0).value, 312.5);
    EXPECT_FALSE(t.store.read(0).flags & core::signals::kFlagSaturated);
}
