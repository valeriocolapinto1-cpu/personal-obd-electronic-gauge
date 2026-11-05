// SPDX-License-Identifier: GPL-3.0-or-later
//
// Simulates another OBD dongle (Veepeak, Carista) on the same bus through a splitter:
// it polls its own PIDs on a fixed period. Responses to its requests are visible to the
// device under test and must be discarded as unsolicited.
#pragma once

#include <cstdint>
#include <vector>

#include "sim/virtual_can_bus.h"

namespace sim {

class ForeignDongle : public INode {
public:
    ForeignDongle(std::vector<uint8_t> pids, uint32_t period_ms) : pids_(std::move(pids)), period_ms_(period_ms) {}

    void on_frame(VirtualCanBus &, const core::bus::CanFrame &) override {}
    void on_tick(VirtualCanBus &bus, uint32_t now_ms) override;

    uint32_t requests_sent() const { return sent_; }

private:
    std::vector<uint8_t> pids_;
    uint32_t period_ms_;
    size_t next_ = 0;
    uint32_t sent_ = 0;
};

} // namespace sim
