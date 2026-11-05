// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/scheduler/poll_scheduler.h"

namespace core::scheduler {

using core::signals::PollPriority;

namespace {
bool reached(uint32_t now, uint32_t t)
{
    return static_cast<int32_t>(now - t) >= 0;
}

// ISO 14229 / ISO 14230 negative response codes
constexpr uint8_t kNrcServiceNotSupported = 0x11;
constexpr uint8_t kNrcSubFunctionNotSupported = 0x12;
constexpr uint8_t kNrcRequestOutOfRange = 0x31;
constexpr uint8_t kNrcBusyRepeatRequest = 0x21;
constexpr uint8_t kNrcConditionsNotCorrect = 0x22;
} // namespace

size_t PollScheduler::add(uint32_t key, PollPriority priority)
{
    PollItem it;
    it.key = key;
    it.priority = priority;
    items_.push_back(it);
    return items_.size() - 1;
}

void PollScheduler::set_focus(size_t index)
{
    for (size_t i = 0; i < items_.size(); ++i) {
        items_[i].focused = (i == index);
    }
}

uint32_t PollScheduler::period_of(const PollItem &it) const
{
    if (it.focused) {
        return cfg_.fast_period_ms;
    }
    switch (it.priority) {
        case PollPriority::Fast:
            return cfg_.fast_period_ms;
        case PollPriority::Normal:
            return cfg_.normal_period_ms;
        case PollPriority::Slow:
            return cfg_.slow_period_ms;
    }
    return cfg_.normal_period_ms;
}

int PollScheduler::next(uint32_t now_ms)
{
    if (in_flight_ >= 0 || items_.empty() || !reached(now_ms, next_slot_ms_)) {
        return -1;
    }
    // Most overdue item first; round-robin start keeps equal deadlines fair.
    int best = -1;
    int32_t best_lateness = -1;
    const size_t n = items_.size();
    for (size_t k = 0; k < n; ++k) {
        const size_t i = (rr_ + k) % n;
        const PollItem &it = items_[i];
        if (it.state == ItemState::Disabled || !reached(now_ms, it.next_due_ms)) {
            continue;
        }
        const int32_t lateness = static_cast<int32_t>(now_ms - it.next_due_ms);
        if (lateness > best_lateness) {
            best_lateness = lateness;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) {
        return -1;
    }
    rr_ = (static_cast<size_t>(best) + 1) % n;
    in_flight_ = best;
    const uint32_t min_gap =
        cfg_.max_requests_per_s ? (1000 + cfg_.max_requests_per_s - 1) / cfg_.max_requests_per_s : 1000;
    next_slot_ms_ = now_ms + min_gap;
    return best;
}

void PollScheduler::finish(size_t index, uint32_t)
{
    if (static_cast<int>(index) == in_flight_) {
        in_flight_ = -1;
    }
}

void PollScheduler::on_success(size_t index, uint32_t now_ms)
{
    if (index >= items_.size()) {
        return;
    }
    PollItem &it = items_[index];
    it.state = ItemState::Active;
    it.consecutive_failures = 0;
    it.backoff_ms = 0;
    it.next_due_ms = now_ms + period_of(it);
    finish(index, now_ms);
}

void PollScheduler::on_timeout(size_t index, uint32_t now_ms)
{
    if (index >= items_.size()) {
        return;
    }
    PollItem &it = items_[index];
    if (it.consecutive_failures < 255) {
        ++it.consecutive_failures;
    }
    // Exponential back-off: period, 2x, 4x ... capped.
    const uint32_t base = period_of(it);
    it.backoff_ms = it.backoff_ms == 0 ? base : it.backoff_ms * 2;
    if (it.backoff_ms > cfg_.max_backoff_ms) {
        it.backoff_ms = cfg_.max_backoff_ms;
    }
    if (it.consecutive_failures >= cfg_.timeouts_before_unavailable) {
        it.state = ItemState::Unavailable;
        it.backoff_ms = cfg_.max_backoff_ms;
    }
    it.next_due_ms = now_ms + it.backoff_ms;
    // Back off the whole bus immediately (CLAUDE.md safety rule 3).
    const uint32_t pause = now_ms + cfg_.global_backoff_ms;
    if (static_cast<int32_t>(pause - next_slot_ms_) > 0) {
        next_slot_ms_ = pause;
    }
    finish(index, now_ms);
}

void PollScheduler::on_negative(size_t index, uint8_t nrc, uint32_t now_ms)
{
    if (index >= items_.size()) {
        return;
    }
    PollItem &it = items_[index];
    switch (nrc) {
        case kNrcServiceNotSupported:
        case kNrcSubFunctionNotSupported:
        case kNrcRequestOutOfRange:
            it.state = ItemState::Disabled;
            finish(index, now_ms);
            return;
        case kNrcBusyRepeatRequest:
        case kNrcConditionsNotCorrect:
        default:
            on_timeout(index, now_ms); // same back-off path
            return;
    }
}

} // namespace core::scheduler
