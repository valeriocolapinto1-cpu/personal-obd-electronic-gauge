// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "core/bus/can_frame.h"

namespace hal {

enum class CanMode : uint8_t {
    ListenOnly, ///< no ACK, no error frames, no transmission (bitrate detection)
    Normal,
};

struct CanStatus {
    uint32_t bus_errors = 0; ///< cumulative since start()
    uint32_t rx_missed = 0;
    uint8_t tx_error_counter = 0;
    uint8_t rx_error_counter = 0;
    bool bus_off = false;
};

class ICanBus {
public:
    virtual ~ICanBus() = default;
    virtual bool start(uint32_t bitrate, CanMode mode) = 0;
    virtual void stop() = 0;
    virtual bool running() const = 0;
    virtual CanMode mode() const = 0;
    /// Waits up to timeout_ms for a frame.
    virtual bool receive(core::bus::CanFrame &out, uint32_t timeout_ms) = 0;
    /// Refused (false) in ListenOnly mode, whatever the caller does.
    virtual bool transmit(const core::bus::CanFrame &frame, uint32_t timeout_ms) = 0;
    virtual CanStatus status() = 0;
};

} // namespace hal
