// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/input/input.h"

namespace core::input {

InputEvent SwipeDetector::update(bool pressed, uint16_t x, uint16_t y, uint32_t now_ms)
{
    if (pressed) {
        if (!down_) {
            down_ = true;
            x0_ = x_ = x;
            y0_ = y_ = y;
            t0_ = now_ms;
        } else {
            x_ = x;
            y_ = y;
        }
        return InputEvent::None;
    }
    if (!down_) {
        return InputEvent::None;
    }
    down_ = false;
    const int dx = static_cast<int>(x_) - static_cast<int>(x0_);
    const int dy = static_cast<int>(y_) - static_cast<int>(y0_);
    const int adx = dx < 0 ? -dx : dx;
    const int ady = dy < 0 ? -dy : dy;
    if (adx >= min_distance_ && adx > 2 * ady) {
        return dx < 0 ? InputEvent::Next : InputEvent::Previous;
    }
    if (adx < min_distance_ / 4 && ady < min_distance_ / 4 && (now_ms - t0_) <= max_tap_ms_) {
        return InputEvent::Tap;
    }
    return InputEvent::None;
}

InputEvent ButtonDetector::update(bool raw_pressed, uint32_t now_ms)
{
    if (raw_pressed != raw_) {
        raw_ = raw_pressed;
        raw_since_ = now_ms;
    }
    if (raw_ != stable_ && (now_ms - raw_since_) >= debounce_ms_) {
        stable_ = raw_;
        if (stable_) {
            pressed_at_ = now_ms;
            long_fired_ = false;
        } else if (!long_fired_) {
            return InputEvent::Next; // released before the long-press time
        }
    }
    if (stable_ && !long_fired_ && (now_ms - pressed_at_) >= long_ms_) {
        long_fired_ = true;
        return InputEvent::LongPress;
    }
    return InputEvent::None;
}

} // namespace core::input
