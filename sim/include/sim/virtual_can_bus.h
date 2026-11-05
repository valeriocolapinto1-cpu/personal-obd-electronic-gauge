// SPDX-License-Identifier: GPL-3.0-or-later
//
// Host-side virtual CAN bus with a simulated millisecond clock.
// Nodes (fake ECUs, foreign dongles, the device under test) exchange frames through it.
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "core/bus/can_frame.h"

namespace sim {

class VirtualCanBus;

class INode {
public:
    virtual ~INode() = default;
    /// Called for every frame on the bus sent by another node.
    virtual void on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame) = 0;
    /// Called once per simulated millisecond.
    virtual void on_tick(VirtualCanBus &, uint32_t /*now_ms*/) {}
};

class VirtualCanBus {
public:
    void attach(INode *node) { nodes_.push_back(node); }

    /// Queues a frame from `sender` (nullptr = external injector) for delivery after `delay_ms`.
    void send(INode *sender, const core::bus::CanFrame &frame, uint32_t delay_ms = 0);

    /// Advances the clock, delivering due frames and ticking nodes.
    void advance(uint32_t ms);

    uint32_t now_ms() const { return now_ms_; }
    /// Every frame delivered so far (for assertions).
    const std::vector<core::bus::CanFrame> &trace() const { return trace_; }

private:
    struct Pending {
        uint32_t due_ms;
        INode *sender;
        core::bus::CanFrame frame;
    };
    void deliver_due();

    std::vector<INode *> nodes_;
    std::deque<Pending> pending_;
    std::vector<core::bus::CanFrame> trace_;
    uint32_t now_ms_ = 0;
};

} // namespace sim
