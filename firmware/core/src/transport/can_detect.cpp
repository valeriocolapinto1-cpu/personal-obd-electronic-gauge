// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/transport/can_detect.h"

namespace core::transport {

constexpr uint32_t CanBitrateDetector::kCandidates[];

void CanBitrateDetector::start()
{
    state_ = DetectState::Listening;
    index_ = 0;
    detected_ = 0;
    saw_errors_ = false;
}

DetectState CanBitrateDetector::feed(const BusObservation &obs)
{
    if (state_ != DetectState::Listening) {
        return state_;
    }
    // Valid frames without errors: the bitrate is right. A few errors with plenty of
    // valid frames are tolerated (another node may have glitched), but errors must not
    // dominate.
    if (obs.valid_frames >= kMinValidFrames && obs.bus_errors * 4 <= obs.valid_frames) {
        detected_ = kCandidates[index_];
        state_ = DetectState::Detected;
        return state_;
    }
    if (obs.bus_errors > 0) {
        saw_errors_ = true;
    }
    if (++index_ >= kNumCandidates) {
        index_ = kNumCandidates - 1;
        state_ = saw_errors_ ? DetectState::Failed : DetectState::Silent;
    }
    return state_;
}

SilentBusAction decide_silent_bus(uint32_t hint_bitrate, bool user_allows_probe)
{
    bool known = false;
    for (uint32_t c : CanBitrateDetector::kCandidates) {
        known = known || (c == hint_bitrate);
    }
    return (known && user_allows_probe) ? SilentBusAction::ProbeAtHint : SilentBusAction::DoNotTransmit;
}

} // namespace core::transport
