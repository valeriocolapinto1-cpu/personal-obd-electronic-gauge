// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>

#include <map>

#include "core/scheduler/poll_scheduler.h"

using namespace core::scheduler;
using core::signals::PollPriority;

namespace {
/// Runs the scheduler for `ms` answering every request instantly; returns requests per item.
std::map<size_t, int> run(PollScheduler &s, uint32_t from, uint32_t ms)
{
    std::map<size_t, int> count;
    for (uint32_t t = from; t < from + ms; ++t) {
        const int i = s.next(t);
        if (i >= 0) {
            ++count[static_cast<size_t>(i)];
            s.on_success(static_cast<size_t>(i), t);
        }
    }
    return count;
}
} // namespace

TEST(Scheduler, RatesFollowPriorities)
{
    PollScheduler s;
    const size_t rpm = s.add(1, PollPriority::Fast);
    const size_t map = s.add(2, PollPriority::Normal);
    const size_t clt = s.add(3, PollPriority::Slow);
    auto c = run(s, 0, 10000);
    EXPECT_NEAR(c[rpm], 200, 10); // 20 Hz
    EXPECT_NEAR(c[map], 40, 3);   // 4 Hz
    EXPECT_NEAR(c[clt], 10, 2);   // 1 Hz
}

TEST(Scheduler, GlobalRateLimitIsNeverExceeded)
{
    SchedulerConfig cfg;
    cfg.max_requests_per_s = 50;
    PollScheduler s(cfg);
    for (uint32_t k = 0; k < 10; ++k) {
        s.add(k, PollPriority::Fast); // 10 x 20 Hz = 200 req/s wanted
    }
    auto c = run(s, 0, 10000);
    int total = 0;
    for (auto &kv : c) {
        total += kv.second;
        EXPECT_GT(kv.second, 30) << "starvation of item " << kv.first;
    }
    EXPECT_LE(total, 500);
    EXPECT_GE(total, 480);
}

TEST(Scheduler, KLineRate)
{
    SchedulerConfig cfg;
    cfg.max_requests_per_s = 8;
    PollScheduler s(cfg);
    s.add(1, PollPriority::Fast);
    s.add(2, PollPriority::Slow);
    auto c = run(s, 0, 10000);
    EXPECT_LE(c[0] + c[1], 80);
    EXPECT_GE(c[1], 9); // slow item still served
}

TEST(Scheduler, FocusPromotesToFast)
{
    PollScheduler s;
    s.add(1, PollPriority::Slow);
    const size_t boost = s.add(2, PollPriority::Normal);
    s.set_focus(boost);
    auto c = run(s, 0, 5000);
    EXPECT_NEAR(c[boost], 100, 5);
}

TEST(Scheduler, OneRequestInFlight)
{
    PollScheduler s;
    s.add(1, PollPriority::Fast);
    s.add(2, PollPriority::Fast);
    EXPECT_EQ(s.next(0), 0);
    EXPECT_TRUE(s.in_flight());
    EXPECT_EQ(s.next(100), -1);
    s.on_success(0, 100);
    EXPECT_EQ(s.next(100), 1);
}

TEST(Scheduler, TimeoutBacksOffItemAndWholeBus)
{
    PollScheduler s;
    const size_t a = s.add(1, PollPriority::Fast);
    const size_t b = s.add(2, PollPriority::Fast);
    ASSERT_EQ(s.next(0), static_cast<int>(a));
    s.on_timeout(a, 100);
    // Global pause of 200 ms: nothing at all before t=300
    for (uint32_t t = 100; t < 300; ++t) {
        EXPECT_EQ(s.next(t), -1) << t;
    }
    EXPECT_EQ(s.next(300), static_cast<int>(b));
    s.on_success(b, 300);
    // Item a backs off exponentially and becomes unavailable after 5 timeouts
    uint32_t t = 300;
    int timeouts = 1;
    while (s.item(a).state != ItemState::Unavailable && t < 100000) {
        const int i = s.next(t);
        if (i == static_cast<int>(a)) {
            s.on_timeout(a, t);
            ++timeouts;
        } else if (i >= 0) {
            s.on_success(static_cast<size_t>(i), t);
        }
        ++t;
    }
    EXPECT_EQ(timeouts, 5);
    EXPECT_EQ(s.item(a).backoff_ms, s.config().max_backoff_ms);
    // Unavailable items are still retried slowly and recover on success
    const uint32_t due = s.item(a).next_due_ms;
    s.on_success(a, due);
    EXPECT_EQ(s.item(a).state, ItemState::Active);
}

TEST(Scheduler, NotSupportedDisablesBusyBacksOff)
{
    PollScheduler s;
    const size_t a = s.add(1, PollPriority::Fast);
    const size_t b = s.add(2, PollPriority::Fast);
    ASSERT_EQ(s.next(0), static_cast<int>(a));
    s.on_negative(a, 0x31, 10);
    EXPECT_EQ(s.item(a).state, ItemState::Disabled);
    EXPECT_EQ(s.next(30), static_cast<int>(b));
    s.on_negative(b, 0x21, 30);
    EXPECT_EQ(s.item(b).state, ItemState::Active);
    EXPECT_GT(s.item(b).next_due_ms, 30u);
    for (uint32_t t = 31; t < 2000; ++t) {
        const int i = s.next(t);
        EXPECT_NE(i, static_cast<int>(a));
        if (i >= 0) {
            s.on_success(static_cast<size_t>(i), t);
        }
    }
}
