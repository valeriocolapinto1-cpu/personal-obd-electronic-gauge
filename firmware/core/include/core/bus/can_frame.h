// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <cstring>

namespace core::bus {

/// Classic CAN 2.0 frame (no CAN FD).
struct CanFrame {
    uint32_t id = 0;
    bool extended = false; ///< 29-bit identifier
    uint8_t dlc = 0;
    uint8_t data[8] = {};

    static CanFrame make(uint32_t id, bool extended, const uint8_t *bytes, uint8_t len)
    {
        CanFrame f;
        f.id = id;
        f.extended = extended;
        f.dlc = len > 8 ? 8 : len;
        std::memcpy(f.data, bytes, f.dlc);
        return f;
    }
};

} // namespace core::bus
