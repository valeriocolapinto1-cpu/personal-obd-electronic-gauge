// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/signals/signal_store.h"

namespace core::signals {

void SignalStore::publish(size_t slot, double value, uint32_t now_ms, uint8_t flags)
{
    if (slot >= kCapacity) {
        return;
    }
    Slot &s = slots_[slot];
    const uint32_t seq = s.seq.load(std::memory_order_relaxed);
    s.seq.store(seq + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    s.value.store(value, std::memory_order_relaxed);
    s.timestamp_ms.store(now_ms, std::memory_order_relaxed);
    s.flags.store(flags, std::memory_order_relaxed);
    s.valid.store(true, std::memory_order_relaxed);
    s.seq.store(seq + 2, std::memory_order_release);
}

void SignalStore::invalidate(size_t slot)
{
    if (slot >= kCapacity) {
        return;
    }
    Slot &s = slots_[slot];
    const uint32_t seq = s.seq.load(std::memory_order_relaxed);
    s.seq.store(seq + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    s.valid.store(false, std::memory_order_relaxed);
    s.seq.store(seq + 2, std::memory_order_release);
}

Sample SignalStore::read(size_t slot) const
{
    Sample out;
    if (slot >= kCapacity) {
        return out;
    }
    const Slot &s = slots_[slot];
    for (;;) {
        const uint32_t before = s.seq.load(std::memory_order_acquire);
        if (before & 1u) {
            continue; // write in progress
        }
        out.value = s.value.load(std::memory_order_relaxed);
        out.timestamp_ms = s.timestamp_ms.load(std::memory_order_relaxed);
        out.valid = s.valid.load(std::memory_order_relaxed);
        out.flags = s.flags.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (s.seq.load(std::memory_order_relaxed) == before) {
            return out;
        }
    }
}

bool SignalStore::fresh(size_t slot, uint32_t now_ms, uint32_t max_age_ms) const
{
    const Sample s = read(slot);
    return s.valid && (now_ms - s.timestamp_ms) <= max_age_ms;
}

} // namespace core::signals
