// SPDX-License-Identifier: GPL-3.0-or-later
//
// Polling scheduler: decides which request goes out next.
//  - priorities: fast (RPM, ~20 Hz), normal, slow (temperatures, ~1 Hz);
//  - the signal on screen is promoted to fast;
//  - global rate limit (default 50 requests/s on CAN, lower on K-line);
//  - back-off on timeouts and "busy" negative responses, permanent disable on
//    "not supported" negative responses (CLAUDE.md safety rule 3).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/signals/signal.h"

namespace core::scheduler {

struct SchedulerConfig {
    uint32_t max_requests_per_s = 50;
    uint32_t fast_period_ms = 50;    ///< 20 Hz
    uint32_t normal_period_ms = 250; ///< 4 Hz
    uint32_t slow_period_ms = 1000;  ///< 1 Hz
    uint32_t max_backoff_ms = 10000;
    uint32_t global_backoff_ms = 200; ///< pause of all traffic after a timeout
    uint8_t timeouts_before_unavailable = 5;
};

enum class ItemState : uint8_t { Active, Unavailable, Disabled };

struct PollItem {
    uint32_t key = 0; ///< caller-defined (e.g. signal slot | pid << 8)
    core::signals::PollPriority priority = core::signals::PollPriority::Normal;
    ItemState state = ItemState::Active;
    uint32_t next_due_ms = 0;
    uint32_t backoff_ms = 0;
    uint8_t consecutive_failures = 0;
    bool focused = false;
};

class PollScheduler {
public:
    explicit PollScheduler(const SchedulerConfig &cfg = SchedulerConfig()) : cfg_(cfg) {}

    void set_config(const SchedulerConfig &cfg) { cfg_ = cfg; }
    const SchedulerConfig &config() const { return cfg_; }

    /// Adds an item; returns its index.
    size_t add(uint32_t key, core::signals::PollPriority priority);
    void clear() { items_.clear(); }

    /// Promotes one item to the fast rate (the signal on screen). Index out of range clears.
    void set_focus(size_t index);

    /// Returns the index of the item to request now, or -1 if nothing may be sent yet.
    /// Only one request is in flight at a time: call one of the on_* functions before the next.
    int next(uint32_t now_ms);

    void on_success(size_t index, uint32_t now_ms);
    void on_timeout(size_t index, uint32_t now_ms);
    /// Negative response. Not-supported codes disable the item, busy codes back off.
    void on_negative(size_t index, uint8_t nrc, uint32_t now_ms);

    const PollItem &item(size_t index) const { return items_[index]; }
    size_t size() const { return items_.size(); }
    bool in_flight() const { return in_flight_ >= 0; }

private:
    uint32_t period_of(const PollItem &it) const;
    void finish(size_t index, uint32_t now_ms);

    SchedulerConfig cfg_;
    std::vector<PollItem> items_;
    int in_flight_ = -1;
    uint32_t next_slot_ms_ = 0; ///< global rate limit
    size_t rr_ = 0;             ///< round-robin start for fairness
};

} // namespace core::scheduler
