// SPDX-License-Identifier: GPL-3.0-or-later
#include "sim/foreign_dongle.h"

namespace sim {

void ForeignDongle::on_tick(VirtualCanBus &bus, uint32_t now_ms)
{
    if (pids_.empty() || period_ms_ == 0 || (now_ms % period_ms_) != 0) {
        return;
    }
    const uint8_t req[] = {0x02, 0x01, pids_[next_], 0x55, 0x55, 0x55, 0x55, 0x55};
    next_ = (next_ + 1) % pids_.size();
    bus.send(this, core::bus::CanFrame::make(0x7DF, false, req, 8));
    ++sent_;
}

} // namespace sim
