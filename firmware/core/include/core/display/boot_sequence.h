// SPDX-License-Identifier: GPL-3.0-or-later
//
// Boot animation timeline: needle (and LED ring) sweep 0 -> max -> 0, then car name + logo,
// then the first screen. Pure function of elapsed time.
#pragma once

#include <cstdint>

namespace core::display {

enum class BootPhase : uint8_t { SweepUp, SweepDown, Splash, Done };

struct BootFrame {
    BootPhase phase;
    double sweep; ///< 0..1 needle/ring position during the sweep, 0 afterwards
};

struct BootTiming {
    uint32_t sweep_up_ms = 700;
    uint32_t hold_ms = 150;
    uint32_t sweep_down_ms = 700;
    uint32_t splash_ms = 1500;
};

BootFrame boot_frame(uint32_t elapsed_ms, const BootTiming &t = BootTiming());

/// Smooth start/stop (cubic ease in-out) for the sweep.
double ease_in_out(double x);

} // namespace core::display
