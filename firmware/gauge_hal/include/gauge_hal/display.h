// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace hal {

enum class DisplayShape : uint8_t { Round, Rect };

/// Resolution and shape of the panel. The UI reads this instead of hard-coding sizes.
struct DisplayInfo {
    uint16_t width;
    uint16_t height;
    DisplayShape shape;
};

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual DisplayInfo info() const = 0;
    /// Backlight in percent (0 = off, 100 = full).
    virtual bool set_backlight(uint8_t percent) = 0;
};

} // namespace hal
