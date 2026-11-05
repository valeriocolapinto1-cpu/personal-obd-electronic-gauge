// SPDX-License-Identifier: GPL-3.0-or-later
//
// Animated fake car for demos and UI development: an OBD ECU whose PIDs follow a
// repeating drive cycle (idle, pulls to the limiter, gear changes, warm-up).
// Runs on the PC and, in demo builds, on the gauge itself instead of the CAN bus.
#pragma once

#include <cstdint>

#include "sim/fake_obd_ecu.h"

namespace sim {

class DemoCar : public INode {
public:
    explicit DemoCar(uint16_t limiter_rpm = 6500);

    FakeObdEcu &ecu() { return ecu_; }

    void on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame) override { ecu_.on_frame(bus, frame); }
    void on_tick(VirtualCanBus &bus, uint32_t now_ms) override;

    /// Values of the drive cycle at a given time (exposed for tests).
    struct Snapshot {
        double rpm;
        double map_kpa;
        double coolant_c;
        double speed_kmh;
    };
    Snapshot at(uint32_t now_ms) const;

private:
    void apply(const Snapshot &s);
    FakeObdEcu ecu_;
    uint16_t limiter_;
    uint32_t last_update_ = 0;
};

} // namespace sim
