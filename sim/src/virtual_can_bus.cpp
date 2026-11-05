// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/virtual_can_bus.h"

#include <algorithm>

namespace sim {

void VirtualCanBus::send(INode *sender, const core::bus::CanFrame &frame, uint32_t delay_ms)
{
    Pending p{now_ms_ + delay_ms, sender, frame};
    // Keep the queue ordered by due time, FIFO for equal times.
    auto it = std::upper_bound(pending_.begin(), pending_.end(), p.due_ms,
                               [](uint32_t t, const Pending &q) { return t < q.due_ms; });
    pending_.insert(it, p);
    if (delay_ms == 0) {
        deliver_due();
    }
}

void VirtualCanBus::deliver_due()
{
    while (!pending_.empty() && pending_.front().due_ms <= now_ms_) {
        const Pending p = pending_.front();
        pending_.pop_front();
        if (trace_enabled_) {
            trace_.push_back(p.frame);
        }
        // Copy: a node may attach/send while we iterate.
        const std::vector<INode *> nodes = nodes_;
        for (INode *n : nodes) {
            if (n != p.sender) {
                n->on_frame(*this, p.frame);
            }
        }
    }
}

void VirtualCanBus::advance(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; ++i) {
        ++now_ms_;
        deliver_due();
        const std::vector<INode *> nodes = nodes_;
        for (INode *n : nodes) {
            n->on_tick(*this, now_ms_);
        }
    }
}

} // namespace sim
