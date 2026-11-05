// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/display/boot_sequence.h"

namespace core::display {

double ease_in_out(double x)
{
    if (x <= 0) {
        return 0;
    }
    if (x >= 1) {
        return 1;
    }
    return x < 0.5 ? 4 * x * x * x : 1 - ((-2 * x + 2) * (-2 * x + 2) * (-2 * x + 2)) / 2;
}

BootFrame boot_frame(uint32_t e, const BootTiming &t)
{
    if (e < t.sweep_up_ms) {
        return {BootPhase::SweepUp, ease_in_out(static_cast<double>(e) / t.sweep_up_ms)};
    }
    e -= t.sweep_up_ms;
    if (e < t.hold_ms) {
        return {BootPhase::SweepUp, 1.0};
    }
    e -= t.hold_ms;
    if (e < t.sweep_down_ms) {
        return {BootPhase::SweepDown, 1.0 - ease_in_out(static_cast<double>(e) / t.sweep_down_ms)};
    }
    e -= t.sweep_down_ms;
    if (e < t.splash_ms) {
        return {BootPhase::Splash, 0.0};
    }
    return {BootPhase::Done, 0.0};
}

} // namespace core::display
