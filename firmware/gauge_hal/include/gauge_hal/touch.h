// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace hal {

struct TouchPoint {
    bool pressed;
    uint16_t x;
    uint16_t y;
};

class ITouch {
public:
    virtual ~ITouch() = default;
    /// Reads the current touch state. Returns false on bus error.
    virtual bool read(TouchPoint &out) = 0;
};

} // namespace hal
