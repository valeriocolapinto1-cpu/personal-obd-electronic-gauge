// SPDX-License-Identifier: GPL-3.0-or-later
//
// Input interpretation: touch swipes and the side push button (short / long press).
#pragma once

#include <cstdint>

namespace core::input {

enum class InputEvent : uint8_t {
    None,
    Next,      ///< swipe left or short button press
    Previous,  ///< swipe right
    Tap,       ///< short touch without movement (toggles style)
    LongPress, ///< long button press: menu / Wi-Fi AP
};

/// Turns a stream of touch samples into swipe/tap events.
class SwipeDetector {
public:
    /// `min_distance` in pixels (pass ~15 % of the display width).
    explicit SwipeDetector(uint16_t min_distance = 60, uint32_t max_tap_ms = 300)
        : min_distance_(min_distance), max_tap_ms_(max_tap_ms)
    {}
    InputEvent update(bool pressed, uint16_t x, uint16_t y, uint32_t now_ms);

private:
    uint16_t min_distance_;
    uint32_t max_tap_ms_;
    bool down_ = false;
    uint16_t x0_ = 0, y0_ = 0, x_ = 0, y_ = 0;
    uint32_t t0_ = 0;
};

/// Debounced push button with short/long press.
class ButtonDetector {
public:
    explicit ButtonDetector(uint32_t debounce_ms = 30, uint32_t long_ms = 800)
        : debounce_ms_(debounce_ms), long_ms_(long_ms)
    {}
    /// `raw_pressed` sampled every few ms. Long press fires once while held.
    InputEvent update(bool raw_pressed, uint32_t now_ms);

private:
    uint32_t debounce_ms_;
    uint32_t long_ms_;
    bool raw_ = false;
    bool stable_ = false;
    uint32_t raw_since_ = 0;
    uint32_t pressed_at_ = 0;
    bool long_fired_ = false;
};

} // namespace core::input
