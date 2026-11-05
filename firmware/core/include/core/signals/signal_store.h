// SPDX-License-Identifier: GPL-3.0-or-later
//
// Latest-value store shared between the bus task (single writer) and the UI / LED / web
// tasks (readers). Lock-free: each slot is a seqlock.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace core::signals {

enum SampleFlags : uint8_t {
    kFlagNone = 0,
    kFlagSaturated = 1 << 0, ///< sensor/PID at the end of its range (e.g. PID 0x0B at 255 kPa)
    kFlagEstimated = 1 << 1, ///< computed, may be inaccurate (e.g. fuel on a tuned diesel)
};

struct Sample {
    double value = 0.0;
    uint32_t timestamp_ms = 0;
    bool valid = false;
    uint8_t flags = kFlagNone;
};

class SignalStore {
public:
    static constexpr size_t kCapacity = 48;

    /// Writer side (one task only).
    void publish(size_t slot, double value, uint32_t now_ms, uint8_t flags = kFlagNone);
    void invalidate(size_t slot);

    /// Reader side (any task). Returns an invalid sample for unknown slots.
    Sample read(size_t slot) const;

    /// True if the slot was updated within `max_age_ms`.
    bool fresh(size_t slot, uint32_t now_ms, uint32_t max_age_ms) const;

private:
    struct Slot {
        std::atomic<uint32_t> seq{0};
        // Fields are atomics with relaxed ordering so concurrent access is well defined;
        // the seqlock gives the reader a consistent snapshot.
        std::atomic<double> value{0.0};
        std::atomic<uint32_t> timestamp_ms{0};
        std::atomic<bool> valid{false};
        std::atomic<uint8_t> flags{0};
    };
    Slot slots_[kCapacity];
};

} // namespace core::signals
