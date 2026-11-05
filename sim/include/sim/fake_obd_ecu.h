// SPDX-License-Identifier: GPL-3.0-or-later
//
// Scripted fake OBD-II ECU on an 11-bit CAN bus (ISO 15765-4).
// Phase 1: single-frame requests/responses for service 0x01. ISO-TP multi-frame
// responses (VIN) are added with the ISO-TP layer in phase 2.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "sim/virtual_can_bus.h"

namespace sim {

enum class PidBehaviour : uint8_t {
    Respond,          ///< answer with the scripted bytes
    Ignore,           ///< never answer (timeout)
    NegativeResponse, ///< 7F <sid> <nrc>
};

struct PidScript {
    std::vector<uint8_t> data; ///< bytes after "41 <pid>"
    PidBehaviour behaviour = PidBehaviour::Respond;
    uint8_t nrc = 0x12;    ///< negative response code (subFunctionNotSupported)
    uint32_t delay_ms = 5; ///< response latency
};

class FakeObdEcu : public INode {
public:
    /// `index` 0..7 selects request id 0x7E0+index and response id 0x7E8+index.
    explicit FakeObdEcu(uint8_t index = 0) : index_(index) {}

    void set_pid(uint8_t pid, PidScript script) { pids_[pid] = std::move(script); }
    void set_pid(uint8_t pid, std::vector<uint8_t> data) { pids_[pid] = PidScript{std::move(data)}; }

    uint32_t request_id() const { return 0x7E0u + index_; }
    uint32_t response_id() const { return 0x7E8u + index_; }
    uint32_t requests_seen() const { return requests_seen_; }

    void on_frame(VirtualCanBus &bus, const core::bus::CanFrame &frame) override;

private:
    /// Builds the 4-byte "supported PIDs" bitmap for PID base (0x00, 0x20, ...).
    std::vector<uint8_t> support_bitmap(uint8_t base) const;

    uint8_t index_;
    std::map<uint8_t, PidScript> pids_;
    uint32_t requests_seen_ = 0;
};

} // namespace sim
